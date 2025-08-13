// 
// audio/boataudioentity.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#include "boataudioentity.h"
#include "frontendaudioentity.h"
#include "grcore/debugdraw.h"
#include "northaudioengine.h"
#include "vehicleaudioentity.h"
#include "peds/ped.h"
#include "vehicles/boat.h"
#include "vehicles/vehicle.h"
#include "vfx/Systems/VfxWater.h"
#include "weapons/Weapon.h"
#include "control/replay/replay.h"
#include "camera/cinematic/CinematicDirector.h"
#include "audiohardware/submix.h"
#include "audiohardware/driverutil.h"
#include "audioengine/engine.h"
#include "audioengine/soundfactory.h"
#include "audiosoundtypes/simplesound.h"
#include "audioeffecttypes/waveshapereffect.h"
#include "Vehicles/Submarine.h"
#include "scriptaudioentity.h"
#include "script/script_hud.h"

#include "debugaudio.h"

AUDIO_VEHICLES_OPTIMISATIONS()

EXT_PF_COUNTER(NumCarEngines);
EXT_PF_VALUE_FLOAT(Revs);
EXT_PF_VALUE_FLOAT(RawRevs);
EXT_PF_VALUE_FLOAT(Throttle);
EXT_PF_VALUE_INT(Gear);
EXT_PF_COUNTER(NumCarEngines);
EXT_PF_COUNTER(NumRealVehicles);
EXT_PF_COUNTER(NumDisabledVehicles);
EXT_PF_COUNTER(NumDummyVehicles);

extern u32 g_MaxGranularEngines;
extern u32 g_MaxVehicleSubmixes;
extern u32 g_MaxDummyVehicles;

extern audCategory *g_IgnitionCategory;
extern audCurve g_RevsSmoothingCurve;
extern bool g_GranularEnableNPCGranularEngines;
extern bool g_ForceGranularNPCEngines;
extern bool g_GranularEnabled;
extern f32 g_RevMaxIncreaseRate;
extern f32 g_RevMaxDecreaseRate;
extern u32 g_EngineStartDuration;
extern u32 g_EngineStopDuration;
extern f32 g_EngineVolumeTrim;
extern f32 g_ExhaustVolumeTrim;
extern u32 g_SuperDummyRadiusSq;
extern f32 g_BaseRollOffScalePlayer;
extern f32 g_BaseRollOffScaleNPC;
extern f32 g_MaxRealRangeScale;
extern f32 g_MaxGranularRangeScale;
extern audCategory *g_PlayerVolumeCategories[5];
extern audCategory *g_PedVolumeCategories[5];

bool g_TreatAllBoatsAsNPCs = false;
bank_float g_FakeRevsOffsetBelow = 0.1f;
bank_float g_FakeRevsOffsetAbove = 0.1f;
bank_float g_FakeRevsMaxHoldTimeMin = 0.0f;
bank_float g_FakeRevsMaxHoldTimeMax = 1.0f;
bank_float g_FakeRevsMinHoldTimeMin = 0.0f;
bank_float g_FakeRevsMinHoldTimeMax = 0.1f;
bank_float g_FakeRevsIncreaseRateBase = 20000;
bank_float g_FakeRevsIncreaseRateScale = 2.0f;
bank_float g_FakeRevsDecreaseRateBase = 20000;
bank_float g_FakeRevsDecreaseRateScale = 2.0f;
bank_float g_FakeOutOfWaterMinTime = 0.2f;
bank_float g_FakeOutOfWaterMaxTime = 1.3f;
bank_float g_FakeInWaterMinTime = 1.5f;
bank_float g_FakeInWaterMaxTime = 6.0f;
float g_SubmersibleUnderwaterCutoff = 300.0f;

f32 g_LowerBoatDynamicImpactThreshold = -10.f;
f32 g_UpperBoatDynamicImpactThreshold = -5.f;
f32 g_BoatActivationDistSq = (90.0f * 90.0f);
u32 g_FakeWaveHitDistanceSq = 900;
f32 g_BoatOffscreenVisiblityScalar = 0.3f;
f32 g_BankSprayPanDistanceScalar = 10.0f;
f32 g_BankSprayForwardDistanceScalar = -4.0f;

#if __BANK
f32 g_VolumeTrims[5];
f32 g_WaterTurbulenceVolumeTrim = 0.0f;
f32 g_BankSprayVolumeTrim = 0.0f;
bool g_DebugDraw = false;

extern bool g_GranularDebugRenderingEnabled;
extern bool g_SimulatePlayerEnteringVehicle;
extern f32 g_OverriddenThrottle;
extern f32 g_OverriddenRevs;
extern bool g_OverrideSim;
#endif

audCurve audBoatAudioEntity::sm_BoatAirTimeToPitch;
audCurve audBoatAudioEntity::sm_BoatAirTimeToVolume;
audCurve audBoatAudioEntity::sm_BoatAirTimeToRevsMultiplier;
audCurve audBoatAudioEntity::sm_BoatAirTimeToResonance;

// ----------------------------------------------------------------
// audBoatAudioEntity constructor
// ----------------------------------------------------------------
audBoatAudioEntity::audBoatAudioEntity() : audVehicleAudioEntity()
{
	m_VehicleType = AUD_VEHICLE_BOAT;
	m_BankSpray = NULL;
	m_WaterDiveSound = NULL;

#if NON_GRANULAR_BOATS_ENABLED
	for(u32 loop = 0; loop < BOAT_ENGINE_SOUND_TYPE_MAX; loop++)
	{
		m_EngineSounds[loop] = NULL;
#if __BANK
		g_VolumeTrims[loop] = 0.0f;
#endif
	}
#endif
}

// ----------------------------------------------------------------
// audBoatAudioEntity destructor
// ----------------------------------------------------------------
audBoatAudioEntity::~audBoatAudioEntity()
{
	ShutdownPrivate();
}

// ----------------------------------------------------------------
// audBoatAudioEntity Shutdown
// ----------------------------------------------------------------
void audBoatAudioEntity::Shutdown()
{
	ShutdownPrivate();
	audVehicleAudioEntity::Shutdown();
}

// ----------------------------------------------------------------
// audBoatAudioEntity ShutdownPrivate
// ----------------------------------------------------------------
void audBoatAudioEntity::ShutdownPrivate()
{
	StopEngineSounds();
}

// ----------------------------------------------------------------
// audBoatAudioEntity InitClass
// ----------------------------------------------------------------
void audBoatAudioEntity::InitClass()
{
	sm_BoatAirTimeToPitch.Init(ATSTRINGHASH("BOAT_AIRTIME_TO_PITCH", 0xEB2A07E3));
	sm_BoatAirTimeToVolume.Init(ATSTRINGHASH("BOAT_AIRTIME_TO_VOLUME", 0x8475723E));
	sm_BoatAirTimeToRevsMultiplier.Init(ATSTRINGHASH("BOAT_AIRTIME_TO_REVS_MULTIPLIER", 0x9D096A5));
	sm_BoatAirTimeToResonance.Init(ATSTRINGHASH("BOAT_AIRTIME_TO_RESONANCE", 0x6F5CC0EE));
}

// ----------------------------------------------------------------
// audBoatAudioEntity Reset
// ----------------------------------------------------------------
void audBoatAudioEntity::Reset()
{
	audVehicleAudioEntity::Reset();
	m_BoatAudioSettings = NULL;
	m_GranularEngineSettings = NULL;	
	m_WasEngineOnLastFrame = false;	
	m_RollOffSmoother.Init(1.0f, 0.2f);
	m_RevsSmoother.Init(0.003f,0.001f,true);
	m_ThrottleSmoother.Init(0.005f,0.005f,-1.0f,1.0f);
	m_BankSpraySmoother.Init(0.0015f, 0.002f);	
	m_KeelForce = 0.f;
	m_SpeedEvo = 0.f;
	m_OutOfWaterRevsMultiplier = 1.0f;
	m_ManufacturerHash = m_ModelHash = m_CategoryHash = g_NullSoundHash;
	m_ScannerVehicleSettingsHash = 0;	
	m_WaterTurbulenceOutOfWaterTimer = 0.0f;
	m_WaterTurbulenceInWaterTimer = 0.0f;
	m_EngineOn = false;
	m_WaterSamples.Reset();
	m_FakeRevsHoldTime = 0.0f;
	m_WasHighFakeRevs = false;
	m_FakeRevsSmoother.Init(0.0001f, 0.0001f, 0.0f, 1.0f);
	m_FakeOutOfWater = false;
	m_FakeOutOfWaterTimer = 0.0f;
}

// ----------------------------------------------------------------
// Init anything boat related
// ----------------------------------------------------------------
bool audBoatAudioEntity::InitVehicleSpecific()
{
	m_BoatAudioSettings = GetBoatAudioSettings();

	if(m_BoatAudioSettings)
	{
#if NON_GRANULAR_BOATS_ENABLED
		m_EngineCurves[BOAT_ENGINE_SOUND_TYPE_ENGINE_1].Init(m_BoatAudioSettings->Engine1Vol);
		m_EngineCurves[BOAT_ENGINE_SOUND_TYPE_ENGINE_2].Init(m_BoatAudioSettings->Engine2Vol);
		m_EngineCurves[BOAT_ENGINE_SOUND_TYPE_LOW_RESO].Init(m_BoatAudioSettings->LowResoLoopVol);
		m_EngineCurves[BOAT_ENGINE_SOUND_TYPE_RESO].Init(m_BoatAudioSettings->ResoLoopVol);
		m_EnginePitchCurves[BOAT_ENGINE_SOUND_TYPE_ENGINE_1].Init(m_BoatAudioSettings->Engine1Pitch);
		m_EnginePitchCurves[BOAT_ENGINE_SOUND_TYPE_ENGINE_2].Init(m_BoatAudioSettings->Engine2Pitch);
		m_EnginePitchCurves[BOAT_ENGINE_SOUND_TYPE_LOW_RESO].Init(m_BoatAudioSettings->LowResoPitch);
		m_EnginePitchCurves[BOAT_ENGINE_SOUND_TYPE_RESO].Init(m_BoatAudioSettings->ResoPitch);
#endif

		m_WaterTurbulenceVolumeCurve.Init(m_BoatAudioSettings->WaterTurbulanceVol);
		m_VehicleSpeedToHullSlapVol.Init(m_BoatAudioSettings->IdleHullSlapSpeedToVol);
		m_WaterTurbulencePitchCurve.Init(m_BoatAudioSettings->WaterTurbulancePitch);

		if(AUD_GET_TRISTATE_VALUE(m_BoatAudioSettings->Flags, FLAG_ID_BOATAUDIOSETTINGS_ISSUBMARINE) == AUD_TRISTATE_TRUE)
		{
			InitSubmersible(m_BoatAudioSettings);
		}

		const u32 modelNameHash = GetVehicleModelNameHash();

		if (modelNameHash == ATSTRINGHASH("Kosatka", 0x4FAF0D70))
		{
			m_HasMissileLockWarningSystem = true;
		}
		
		m_GranularEngineSettings = audNorthAudioEngine::GetObject<GranularEngineAudioSettings>(m_BoatAudioSettings->GranularEngine);

#if !NON_GRANULAR_BOATS_ENABLED
		if (modelNameHash != ATSTRINGHASH("Kosatka", 0x4FAF0D70))
		{
			audAssertf(m_GranularEngineSettings, "Non granular boats have been disabled, but boat %s does not have a granular engine!", GetVehicleModelName());
		}
#endif

#if NA_RADIO_ENABLED
		m_AmbientRadioDisabled = (AUD_GET_TRISTATE_VALUE(m_BoatAudioSettings->Flags, FLAG_ID_BOATAUDIOSETTINGS_DISABLEAMBIENTRADIO) == AUD_TRISTATE_TRUE);
		m_RadioType = (RadioType)m_BoatAudioSettings->RadioType;
		m_RadioGenre = (RadioGenre)m_BoatAudioSettings->RadioGenre;
#endif

		m_VehicleEngine.Init(this);				
		SetEnvironmentGroupSettings(true, audNorthAudioEngine::GetOcclusionManager()->GetMaxOcclusionFullPathDepth());
		SetupVolumeCones();
		return true;
	}
	
	return false;
}

// ----------------------------------------------------------------
// Check if this boat is using a granular engine
// ----------------------------------------------------------------
bool audBoatAudioEntity::IsUsingGranularEngine()
{
	return m_VehicleEngine.IsGranularEngineActive();
}

// ----------------------------------------------------------------
// Get the idle hull slap volume
// ----------------------------------------------------------------
f32 audBoatAudioEntity::GetIdleHullSlapVolumeLinear(f32 speed) const
{
	return m_VehicleSpeedToHullSlapVol.CalculateValue(speed);
}

// ----------------------------------------------------------------
// Get the boat horn sound
// ----------------------------------------------------------------
u32 audBoatAudioEntity::GetVehicleHornSoundHash(bool UNUSED_PARAM(ignoreMods))
{
	if(m_BoatAudioSettings)
	{
		return m_BoatAudioSettings->HornLoop;
	}
	else 
	{
		m_BoatAudioSettings = GetBoatAudioSettings();
		if(m_BoatAudioSettings)
		{
			return m_BoatAudioSettings->HornLoop;
		}
	}
	return 0;
}

// ----------------------------------------------------------------
// Get the granular engine settings
// ----------------------------------------------------------------
GranularEngineAudioSettings* audBoatAudioEntity::GetGranularEngineAudioSettings() const
{
	return m_GranularEngineSettings;
}

// ----------------------------------------------------------------
// Get the switch time scalar
// ----------------------------------------------------------------
f32 audBoatAudioEntity::GetEngineSwitchFadeTimeScalar() const
{
	if(!m_IsPlayerVehicle)
	{
		// If we're low on vehicles, we can be a bit more lazy with our fade time - the lack of vehicles makes the sharper 
		// cuts in/out more obvious, so this improves the sound of the transitions
		if(m_Vehicle->GetOwnedBy() != ENTITY_OWNEDBY_CUTSCENE && 
			m_Vehicle->GetOwnedBy() != ENTITY_OWNEDBY_SCRIPT)
		{
			if(sm_ActivationRangeScale == g_MaxRealRangeScale &&
				(!m_VehicleEngine.IsGranularEngineActive() || sm_GranularActivationRangeScale == g_MaxGranularRangeScale))
			{
				return 2.0f;
			}
		}
	}

	return 1.0f;
}

// ----------------------------------------------------------------
// Populate boat variables
// ----------------------------------------------------------------
void audBoatAudioEntity::PopulateBoatAudioVariables(audVehicleVariables* state)
{
	naAssertf(m_Vehicle, "m_vehicle must be valid, about to access a null ptr...");

	state->gear = m_Vehicle->m_Transmission.GetGear();
	state->gasPedal = (m_Vehicle->m_nVehicleFlags.bEngineOn || m_Vehicle->m_nVehicleFlags.bEngineStarting) ? Abs(m_Vehicle->m_Transmission.GetThrottle()) : 0.0f;

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		state->rawRevs = m_ReplayRevs;
	}
	else
#endif
	{
		if(m_Vehicle->GetDriver() && (!m_Vehicle->GetDriver()->IsPlayer() || g_TreatAllBoatsAsNPCs) && !IsSubmersible() && m_Vehicle->m_nVehicleFlags.bEngineOn)
		{
			// AI controlled boat - fake the revs based on fwd speed
			if(!m_Vehicle->GetIsJetSki() || state->fwdSpeed < 2.5f || m_Vehicle->m_nVehicleFlags.bIsRacing)
			{
				state->rawRevs = Clamp(state->fwdSpeed * 0.1f, 0.2f, 0.85f);
				m_FakeOutOfWaterTimer = 0.0f;
				m_FakeOutOfWater = false;
			}
			else
			{
				f32 fakeRevs = Clamp(state->fwdSpeed * 0.14f, 0.2f, 0.7f);
				f32 baseFakeRevs = Max(fakeRevs - audEngineUtil::GetRandomNumberInRange(g_FakeRevsOffsetBelow * 0.5f, g_FakeRevsOffsetBelow), 0.2f);
				f32 maxFakeRevs = Min(fakeRevs + audEngineUtil::GetRandomNumberInRange(g_FakeRevsOffsetAbove * 0.5f, g_FakeRevsOffsetAbove), 0.8f);

				f32 fakeRevsTarget = m_WasHighFakeRevs? maxFakeRevs : baseFakeRevs;
				m_FakeRevsSmoother.SetBounds(baseFakeRevs, maxFakeRevs);
				state->rawRevs = m_FakeRevsSmoother.CalculateValue(fakeRevsTarget, state->timeInMs);

				if(m_WasHighFakeRevs)
				{
					state->gasPedal = 1.0f;
				}
				else
				{
					state->gasPedal = 0.0f;
				}

				if(state->rawRevs == fakeRevsTarget)
				{
					m_FakeRevsHoldTime -= fwTimer::GetTimeStep();

					if(m_FakeRevsHoldTime < 0.0f)
					{
						if(m_WasHighFakeRevs)
						{
							m_WasHighFakeRevs = false;
							m_FakeRevsHoldTime = audEngineUtil::GetRandomNumberInRange(g_FakeRevsMinHoldTimeMin, g_FakeRevsMinHoldTimeMax);
						}
						else
						{
							m_WasHighFakeRevs = true;
							m_FakeRevsHoldTime = audEngineUtil::GetRandomNumberInRange(g_FakeRevsMaxHoldTimeMin, g_FakeRevsMaxHoldTimeMax);
						}

						m_FakeRevsSmoother.SetRates((1.0f/g_FakeRevsIncreaseRateBase) * audEngineUtil::GetRandomNumberInRange(1.0f, g_FakeRevsIncreaseRateScale), (1.0f/g_FakeRevsDecreaseRateBase) * audEngineUtil::GetRandomNumberInRange(1.0f, g_FakeRevsDecreaseRateScale));
					}
				}

				if(state->fwdSpeed > 5.0f && m_DistanceFromListener2LastFrame > g_FakeWaveHitDistanceSq)
				{
					m_FakeOutOfWaterTimer -= fwTimer::GetTimeStep();

					if(m_FakeOutOfWaterTimer < 0.0f)
					{
						m_FakeOutOfWater = !m_FakeOutOfWater;

						if(m_FakeOutOfWater)
						{
							m_FakeOutOfWaterTimer = audEngineUtil::GetRandomNumberInRange(g_FakeOutOfWaterMinTime, g_FakeOutOfWaterMaxTime);
						}
						else
						{
							m_FakeOutOfWaterTimer = audEngineUtil::GetRandomNumberInRange(g_FakeInWaterMinTime, g_FakeInWaterMaxTime);
						}
					}
				}
				else
				{
					m_FakeOutOfWaterTimer = 0.0f;
					m_FakeOutOfWater = false;
				}
			}
		}
		else
		{
			state->rawRevs = F32_VerifyFinite(m_Vehicle->m_Transmission.GetRevRatio());
			m_FakeOutOfWaterTimer = 0.0f;
			m_FakeOutOfWater = false;
		}

		if(m_Vehicle->GetDriver())
		{
			f32 filterFactor = 0.2f;
			f32 desiredRevsMultiplier = 1.0f;

			if(m_OutOfWaterTimer > 0.0f)
			{
				desiredRevsMultiplier = sm_BoatAirTimeToRevsMultiplier.CalculateValue(m_OutOfWaterTimer);
				desiredRevsMultiplier *= Min(state->fwdSpeed * 0.5f, 1.0f);
			}

			m_OutOfWaterRevsMultiplier = (desiredRevsMultiplier * filterFactor) + (m_OutOfWaterRevsMultiplier * (1.0f - filterFactor));
			state->rawRevs *= m_OutOfWaterRevsMultiplier;
		}
		
		REPLAY_ONLY(m_ReplayRevs = state->rawRevs;)
	}

	state->granularEnginePitchOffset = static_cast<s32>(sm_BoatAirTimeToPitch.CalculateValue(m_OutOfWaterTimer));

	state->engineConeAtten = m_GranularEngineSettings ? m_EngineVolumeCone.ComputeAttenuation(m_Vehicle->GetMatrix()) : 0.f;
	state->exhaustConeAtten = m_GranularEngineSettings ? m_ExhaustVolumeCone.ComputeAttenuation(m_Vehicle->GetMatrix()) : 0.f;

	f32 entityVariableThrottle = 0.0f;
	f32 entityVariableRevs = 0.0f;
	f32 fakeEngineFactor = 0.0f;

	if(HasEntityVariableBlock())
	{
		entityVariableThrottle = GetEntityVariableValue(ATSTRINGHASH("fakethrottle", 0xEB27990));
		entityVariableRevs = GetEntityVariableValue(ATSTRINGHASH("fakerevs", 0xCEB98BEB));

		// Reduce this each frame so that that the behaviour gets canceled even if a sound forgets to do so
		fakeEngineFactor = GetEntityVariableValue(ATSTRINGHASH("usefakeengine", 0x91DF7F97));
		SetEntityVariable(ATSTRINGHASH("usefakeengine", 0x91DF7F97), Clamp(fakeEngineFactor - 0.1f, 0.0f, 1.0f));
	}

	if(fakeEngineFactor > 0.1f)
	{
		state->gasPedal = entityVariableThrottle;
		state->rawRevs = entityVariableRevs;
	}

	// Reversing flag (not the same as being in reverse gear) can be used to determine whether the vehicle is 
	// intentionally moving backwards, as opposed to going backwards because it did a big drift/spin
	if(state->gear == 0 && state->fwdSpeed < -0.1f)
	{
		m_IsReversing = true;
	}
	else if(state->fwdSpeed >= -0.1f)
	{
		m_IsReversing = false;
	}

	if(m_IsReversing)
	{
		state->rawRevs = Clamp(state->rawRevs, 0.0f, 0.8f);
	}

	
#if __BANK
	if(g_OverrideSim && ((m_Vehicle == g_AudioDebugEntity) || (!g_AudioDebugEntity && m_IsPlayerVehicle)))
	{
		state->gasPedal = g_OverriddenThrottle;
		state->rawRevs = g_OverriddenRevs;
	}
#endif

	if(m_IsPlayerVehicle)
	{
		m_ThrottleSmoother.SetRates(0.005f,0.005f);
	}
	else
	{
		m_ThrottleSmoother.SetRates(0.0025f,0.0025f);
	}

	if(m_VehicleEngine.GetState() == audVehicleEngine::AUD_ENGINE_STARTING)
	{
		// GTA 5 DLC fix - fake startup revs don't sound great on certain boats. Should be made
		// into a gameobject PTS/fakeThrottle sound (as with cars) on future projects.
		bool skipStartupRevs = false;
		const u32 modelNameHash = GetVehicleModelNameHash();

		if(modelNameHash == ATSTRINGHASH("TORO", 0x3FD5AA2F) || modelNameHash == ATSTRINGHASH("TORO2",0x362CAC6D))
		{
			skipStartupRevs = true;
		}

		if(!skipStartupRevs)
		{
			// if the player starts acelerating then interrupt starting behaviour
			if((m_Vehicle->GetThrottle()>0.1f || state->fwdSpeed<-0.1f) && m_IsPlayerVehicle)
			{
				// force the throttle to be 0 so it'll smooth up
				m_ThrottleSmoother.Reset();
				m_ThrottleSmoother.CalculateValue(0.0f, state->timeInMs);
			}
			else
			{
				// seriously smooth revs if we're starting the engine
				m_RevsSmoother.SetRates(1.5f / 1000.f,0.45f / 1000.f);

				u32 engineOnDuration = fwTimer::GetTimeInMilliseconds() - m_EngineStartTime;
				if(engineOnDuration < 5)
				{
					state->rawRevs = 0.0f;
				}
				else if(engineOnDuration > (u32)(g_EngineStartDuration/3.0f))
				{
					state->rawRevs = CTransmission::ms_fIdleRevs;
				}
				else
				{
					state->rawRevs = 0.75f;
				}
			}
		}		
	}
	else if(m_VehicleEngine.GetState() == audVehicleEngine::AUD_ENGINE_STOPPING)
	{
		// seriously smooth revs if we're stopping the engine
		m_RevsSmoother.SetRates(0.3f / 1000.f, 0.3f / 1000.f);
		state->rawRevs = 0.0f;
	}
	else
	{
		f32 revsSmoothScalar = g_RevsSmoothingCurve.CalculateValue(state->rawRevs);

		// Prevent divide by zero
		if(revsSmoothScalar < 1.0f)
		{
			revsSmoothScalar = 1.0f;
		}

		if(state->gear == 0 && IsReversing())
		{
			m_RevsSmoother.SetRates(0.0002f, g_RevMaxDecreaseRate / (1000.f * revsSmoothScalar));
		}
		else
		{
			m_RevsSmoother.SetRates(g_RevMaxIncreaseRate / 1000.f, g_RevMaxDecreaseRate / (1000.f * revsSmoothScalar));
		}
	}

	state->clutchRatio = m_Vehicle->m_Transmission.GetClutchRatio();

	// If the engine is off, revert to clutch pressed
	if(!m_Vehicle->IsEngineOn())
	{
		state->clutchRatio = 0.1f;
	}

	const f32 engineHealth = ComputeEffectiveEngineHealth();
	state->engineDamageFactor = 1.0f - engineHealth / CTransmission::GetEngineHealthMax();
	state->throttle = m_ThrottleSmoother.CalculateValue(state->gasPedal, state->timeInMs);
	state->throttleInput = m_Vehicle->GetThrottle();
	state->onRevLimiter = false;
	state->revs = F32_VerifyFinite(m_RevsSmoother.CalculateValue(state->rawRevs, state->timeInMs));

	if(m_VehicleEngine.GetState() != audVehicleEngine::AUD_ENGINE_STARTING && m_VehicleEngine.GetState() != audVehicleEngine::AUD_ENGINE_STOPPING)
	{
		// clamp revs to idle minimum
		state->revs = Max(CTransmission::ms_fIdleRevs,state->revs);
	}

	if(m_VehicleEngine.IsGranularEngineActive())
	{
		state->carVolOffset = (m_GranularEngineSettings->MasterVolume/100.f);
	}
	else
	{
		state->carVolOffset = 0.0f;
	}

	f32 enginePostSubmixVol = state->carVolOffset;
	f32 exhaustPostSubmixVol = state->carVolOffset;

	// In practice rev ratio always lies between 0.2 and 1.0, so scale up to a 0-1 range
	f32 revRatio = ((state->revs - 0.2f) * 1.25f);

	// Similarly, clutch ratio actually goes from 0.1 to 1.0, so scale up to a 0-1 range
	f32 clutchRatio = ((state->clutchRatio - 0.1f) * 1.11f);

	if(m_VehicleEngine.IsGranularEngineActive())
	{
		enginePostSubmixVol += (m_GranularEngineSettings->EngineVolume_PostSubmix * 0.01f);
		exhaustPostSubmixVol += (m_GranularEngineSettings->ExhaustVolume_PostSubmix * 0.01f);

		enginePostSubmixVol += revRatio * (m_GranularEngineSettings->EngineRevsVolume_PostSubmix * 0.01f);
		enginePostSubmixVol += state->throttle * (m_GranularEngineSettings->EngineThrottleVolume_PostSubmix * 0.01f);
		enginePostSubmixVol += (m_GranularEngineSettings->EngineClutchAttenuation_PostSubmix * 0.01f) * (1.0f - clutchRatio);

		exhaustPostSubmixVol += revRatio * (m_GranularEngineSettings->ExhaustRevsVolume_PostSubmix * 0.01f );
		exhaustPostSubmixVol += state->throttle * (m_GranularEngineSettings->ExhaustThrottleVolume_PostSubmix * 0.01f);
		exhaustPostSubmixVol += (m_GranularEngineSettings->ExhaustClutchAttenuation_PostSubmix * 0.01f) * (1.0f - clutchRatio);

		f32 engineIdleBoostLin = Lerp(m_VehicleEngine.GetGranularEngine()->GetIdleVolumeScale(), 1.0f, audDriverUtil::ComputeLinearVolumeFromDb((m_GranularEngineSettings->EngineIdleVolume_PostSubmix/100.0f)));
		f32 exhaustIdleBoostLin = Lerp(m_VehicleEngine.GetGranularEngine()->GetIdleVolumeScale(), 1.0f, audDriverUtil::ComputeLinearVolumeFromDb((m_GranularEngineSettings->ExhaustIdleVolume_PostSubmix/100.0f)));		

		enginePostSubmixVol += audDriverUtil::ComputeDbVolumeFromLinear(engineIdleBoostLin);
		exhaustPostSubmixVol += audDriverUtil::ComputeDbVolumeFromLinear(exhaustIdleBoostLin);
	} 
	else
	{
		enginePostSubmixVol = m_BoatAudioSettings->EngineVolPostSubmix;
		exhaustPostSubmixVol = m_BoatAudioSettings->ExhaustVolPostSubmix;
	}

	if(m_IsPlayerVehicle)
	{
		const f32 airtimeVolumeBoost = sm_BoatAirTimeToVolume.CalculateValue(m_OutOfWaterTimer);
		enginePostSubmixVol += airtimeVolumeBoost;
		exhaustPostSubmixVol += airtimeVolumeBoost;
	}
	
	state->granularEnginePitchOffset = static_cast<s32>(sm_BoatAirTimeToPitch.CalculateValue(m_OutOfWaterTimer));

	state->enginePostSubmixVol = enginePostSubmixVol + state->engineConeAtten;
	state->exhaustPostSubmixVol = exhaustPostSubmixVol + state->exhaustConeAtten;

#if __ASSERT
	audAssertf(state->enginePostSubmixVol <= 24.0f, "Vehicle %s has an invalid engine post submix volume (%f)", GetVehicleModelName(), state->enginePostSubmixVol);
	audAssertf(state->exhaustPostSubmixVol <= 24.0f, "Vehicle %s has an invalid exhaust post submix volume (%f)", GetVehicleModelName(), state->exhaustPostSubmixVol);
#endif

	state->enginePostSubmixVol += g_EngineVolumeTrim;
	state->exhaustPostSubmixVol += g_ExhaustVolumeTrim;

	f32 rollOff = m_IsPlayerVehicle? m_BoatAudioSettings->MaxRollOffScalePlayer : m_BoatAudioSettings->MaxRollOffScaleNPC;
	f32 desiredRollOff = m_IsPlayerVehicle? g_BaseRollOffScalePlayer : g_BaseRollOffScaleNPC + (rollOff * Clamp(revRatio * revRatio * revRatio, 0.0f, 1.0f));
	state->rollOffScale = m_RollOffSmoother.CalculateValue(desiredRollOff);

#if __BANK
	if(m_Vehicle->GetStatus() == STATUS_PLAYER)
	{
		PF_SET(Revs, state->revs);
		PF_SET(RawRevs, state->rawRevs);
		PF_SET(Throttle, state->throttle);
		PF_SET(Gear, state->gear);
	}
#endif
}

// ----------------------------------------------------------------
// Engine Started
// ----------------------------------------------------------------
void audBoatAudioEntity::EngineStarted()
{
	m_EngineStartTime = fwTimer::GetTimeInMilliseconds();
	m_RevsSmoother.CalculateValue(CTransmission::ms_fIdleRevs,m_EngineStartTime);// init revs to idle
	m_EngineEffectWetSmoother.Reset();
	m_EngineEffectWetSmoother.CalculateValue(0.0f, fwTimer::GetTimeStep());
}

// ----------------------------------------------------------------
// Player entered/exited the vehicles
// ----------------------------------------------------------------
void audBoatAudioEntity::OnFocusVehicleChanged()
{
	if(g_GranularEnabled && 
		m_VehicleEngine.HasBeenInitialised() &&
		m_VehicleEngine.GetState() != audVehicleEngine::AUD_ENGINE_STOPPING)
	{
		m_VehicleEngine.QualitySettingChanged();
	}
}

// -------------------------------------------------------------------------------
// audBoatAudioEntity::AcquireWaveSlot
// -------------------------------------------------------------------------------
bool audBoatAudioEntity::AcquireWaveSlot()
{
	// We want to load sfx slot on Kosatka even if not the focus vehicle, see url:bugstar:6767607
	if(IsSubmersible() && (m_IsFocusVehicle || GetVehicleModelNameHash() == ATSTRINGHASH("KOSATKA", 0x4FAF0D70)) && m_BoatAudioSettings->SFXBankSound != 0u && m_BoatAudioSettings->SFXBankSound != g_NullSoundHash)	
	{
		m_RequiresSFXBank = true;
		RequestSFXWaveSlot(m_BoatAudioSettings->SFXBankSound);

		if(!m_SFXWaveSlot)
		{
			return false;
		}
	}
	else
	{
		m_RequiresSFXBank = false;
	}

	if(m_VehicleEngine.IsGranularEngineActive())
	{
		if(m_VehicleEngine.IsLowQualityGranular())
		{
			RequestWaveSlot(&sm_StandardWaveSlotManager, m_GranularEngineSettings->NPCEngineAccel);
		}
		else
		{
			RequestWaveSlot(&sm_HighQualityWaveSlotManager, m_GranularEngineSettings->EngineAccel);
		}
	}
	else
	{
		RequestWaveSlot(&sm_StandardWaveSlotManager, m_BoatAudioSettings->Engine1Loop);
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

// -------------------------------------------------------------------------------
// Convert the boat from dummy
// -------------------------------------------------------------------------------
void audBoatAudioEntity::ConvertFromDummy()
{
	if(m_VehicleEngine.IsGranularEngineActive())
	{
		m_VehicleEngine.ConvertFromDummy();
	}
}

// ----------------------------------------------------------------
// Convert to super dummy
// ----------------------------------------------------------------
void audBoatAudioEntity::ConvertToSuperDummy()
{
	m_VehicleEngine.ConvertToDummy();
	StopEngineSounds(true);
}

// ----------------------------------------------------------------
// Convert to disabled
// ----------------------------------------------------------------
void audBoatAudioEntity::ConvertToDisabled()
{
	ConvertToSuperDummy();
	StopAndForgetSounds(m_BankSpray, m_SubTurningSweetener, m_SubExtrasSound, m_SubmersibleCreakSound);
}

// ----------------------------------------------------------------
// Get the desired boat LOD
// ----------------------------------------------------------------
audVehicleLOD audBoatAudioEntity::GetDesiredLOD(f32 fwdSpeedRatio, u32 originalDistFromListenerSq, bool visibleBySniper)
{
	// No driver, no revs, disabled? Probably just sat stationary so keep it disabled
	// Kosatka is an exception, see url:bugstar:6767607
	if(!m_Vehicle->GetDriver() && (!m_Vehicle->m_nVehicleFlags.bEngineOn && m_VehicleEngine.GetState() == audVehicleEngine::AUD_ENGINE_OFF) && !m_IsFocusVehicle && GetVehicleModelNameHash() != ATSTRINGHASH("KOSATKA", 0x4FAF0D70))
	{
		return AUD_VEHICLE_LOD_DISABLED;
	}
	else if(m_IsFocusVehicle)
	{
		return AUD_VEHICLE_LOD_REAL;
	}
	else
	{
		f32 desiredVisibilityScalar = 1.0f;

		spdSphere boundSphere;
		m_Vehicle->GetBoundSphere(boundSphere);
		if(!camInterface::IsSphereVisibleInGameViewport(boundSphere.GetV4()))
		{
			// We're not maxing out the submixes, allow vehicles to stay audible even after going off screen
			if(sm_ActivationRangeScale == g_MaxRealRangeScale &&
				(!m_VehicleEngine.IsGranularEngineActive() || sm_GranularActivationRangeScale == g_MaxGranularRangeScale))
			{
				desiredVisibilityScalar = Max(g_BoatOffscreenVisiblityScalar, m_VisibilitySmoother.GetLastValue());
			}
			else
			{
				desiredVisibilityScalar = g_BoatOffscreenVisiblityScalar;
			}
		}

		f32 visibilityScalar = m_VisibilitySmoother.CalculateValue(desiredVisibilityScalar, fwTimer::GetTimeStep());

		Vec3V vehiclePosition = m_Vehicle->GetTransform().GetPosition();
		Vec3V listenerPosition = g_AudioEngine.GetEnvironment().GetVolumeListenerPosition(audNorthAudioEngine::GetMicrophones().IsTinyRacersMicrophoneActive() ? 1 : 0);
		f32 vehicleActivationRangeScale = 1.f;

		if (GetVehicleModelNameHash() == ATSTRINGHASH("Kosatka", 0x4FAF0D70))
		{			
			vehiclePosition = ComputeClosestPositionOnVehicleYAxis();
			vehicleActivationRangeScale = 2.f;
		}

		Vector3 distToListener = VEC3V_TO_VECTOR3(vehiclePosition - listenerPosition);

		// Artificially increase height offset to give preference to vehicles on the same plane as the listener
		distToListener.SetZ(distToListener.GetZ() * 2.0f);
		f32 mag2 = visibleBySniper? originalDistFromListenerSq : distToListener.Mag2();
		f32 speedRatioMult = Clamp(fwdSpeedRatio/0.5f, 0.0f, 1.0f);
		f32 granularActivationRangeScale = m_VehicleEngine.IsGranularEngineActive() ? sm_GranularActivationRangeScale : 1.0f;

		// Active vehicles primarily take speed into account, but also a bit of distance - we still want to hear the idle on very close stationary ones
		if(mag2 < (g_BoatActivationDistSq * sm_ActivationRangeScale * vehicleActivationRangeScale * granularActivationRangeScale * visibilityScalar * (0.6f + (0.4f * speedRatioMult))))
		{
			return AUD_VEHICLE_LOD_REAL;
		}
		else
		{
			return AUD_VEHICLE_LOD_DISABLED;
		}
	}
}

// ----------------------------------------------------------------
// Set up boat volume cones
// ----------------------------------------------------------------
void audBoatAudioEntity::SetupVolumeCones()
{
	Vec3V engineConeDir = Vec3V(0.0f, 1.0f, 0.0f);
	Vec3V exhaustConeDir = Vec3V(0.0f, -1.0f, 0.0f);

	bool useExhaustCone = true;
	const Vec3V exPos = m_Vehicle->TransformIntoWorldSpace(m_ExhaustOffsetPos);
	const Vec3V vVehiclePosition = m_Vehicle->GetTransform().GetPosition();
	Vec3V ex2centre = vVehiclePosition - exPos;
	ScalarV dp = Dot(ex2centre, m_Vehicle->GetTransform().GetB());

	if(IsTrue(IsLessThanOrEqual(dp, ScalarV(V_ZERO))))
	{
		useExhaustCone = false;
	}

	Vec3V temp = vVehiclePosition - m_Vehicle->TransformIntoWorldSpace(m_EngineOffsetPos);
	dp = Dot(temp, m_Vehicle->GetTransform().GetB());

	if(IsTrue(IsGreaterThan(dp, ScalarV(V_ZERO))))
	{
		// rear
		engineConeDir = exhaustConeDir;
	}

	if(m_GranularEngineSettings)
	{
		m_EngineVolumeCone.Init(engineConeDir, m_GranularEngineSettings->EngineMaxConeAttenuation/100.f, 60.f,165.f);
		m_ExhaustVolumeCone.Init(exhaustConeDir, (useExhaustCone?m_GranularEngineSettings->ExhaustMaxConeAttenuation/100.f:0.f), 45.f, 160.f);
	}
}

// ----------------------------------------------------------------
// Is this boat a submersible
// ----------------------------------------------------------------
bool audBoatAudioEntity::IsSubmersible() const
{
	if(m_BoatAudioSettings)
	{
		return AUD_GET_TRISTATE_VALUE(m_BoatAudioSettings->Flags, FLAG_ID_BOATAUDIOSETTINGS_ISSUBMARINE) == AUD_TRISTATE_TRUE;
	}	
	else
	{
		return false;
	}	
}

// ----------------------------------------------------------------
// Update anything boat related
// ----------------------------------------------------------------
void audBoatAudioEntity::UpdateVehicleSpecific(audVehicleVariables& vehicleVariables, audVehicleDSPSettings& dspSettings)
{
	f32 boatSprayAngle = 0.0f;
	const u32 timeInMs = fwTimer::GetTimeInMilliseconds();
	
	if(m_Vehicle->GetAudioEnvironmentGroup())
	{
		SetEnvironmentGroupSettings(true, audNorthAudioEngine::GetOcclusionManager()->GetMaxOcclusionFullPathDepth());
		if(m_Vehicle->PopTypeIsMission())
		{
			m_Vehicle->GetAudioEnvironmentGroup()->GetEnvironmentGameMetric().SetRolloffFactor(3.f);
		}
		else
		{
			m_Vehicle->GetAudioEnvironmentGroup()->GetEnvironmentGameMetric().SetRolloffFactor(1.f);
		}
	}

	if(m_BoatAudioSettings)
	{
		if(!IsDisabled())
		{
			bool isInWater = CalculateIsInWater();

			if(m_FakeOutOfWater)
			{
				isInWater = false;
			}

			if(IsReal())
			{
				PopulateBoatAudioVariables(&vehicleVariables);

				if(m_VehicleEngine.IsGranularEngineActive())
				{
					UpdateEngineGranular(vehicleVariables, timeInMs);
				}
#if NON_GRANULAR_BOATS_ENABLED
				else if(m_EngineWaveSlot)
				{
					UpdateEngine(vehicleVariables, timeInMs);
				}
#endif
				
				dspSettings.rolloffScale = vehicleVariables.rollOffScale;
				dspSettings.enginePostSubmixAttenuation = vehicleVariables.enginePostSubmixVol;
				dspSettings.exhaustPostSubmixAttenuation = vehicleVariables.exhaustPostSubmixVol;
				dspSettings.AddDSPParameter(atHashString("RPM", 0x5B924509), vehicleVariables.revs);
				dspSettings.AddDSPParameter(atHashString("Throttle", 0xEA0151DC), vehicleVariables.throttle);
				dspSettings.AddDSPParameter(atHashString("ThrottleInput", 0x918028C4), vehicleVariables.throttleInput);
				AddCommonDSPParameters(dspSettings);

				if(sm_BoatAirTimeToResonance.IsValid())
				{
					dspSettings.AddDSPParameter(atHashString("AirtimeResonance", 0x6CECA0B1), sm_BoatAirTimeToResonance.CalculateValue(m_OutOfWaterTimer));
				}				
			}

			if(IsReal() || IsDummy())
			{
				UpdateNonEngine(vehicleVariables, timeInMs);
			}

			f32 speedFactor = 0.0f;
			f32 inWaterFactor = 0.0f;

			if(IsSubmersible())
			{
				UpdateSubmersible(vehicleVariables, m_BoatAudioSettings);
			}
			
			speedFactor = CalculateWaterSpeedFactor();

			if(m_Vehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR)
			{
				inWaterFactor = !m_Vehicle->GetIsInWater()?0.f:1.f;
			}
			else
			{
				inWaterFactor = (!((CBoat*)m_Vehicle)->m_BoatHandling.IsInWater()?0.f:1.f);				
			}

			inWaterFactor = m_Vehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR ? 1.0f : (!((CBoat*)m_Vehicle)->m_BoatHandling.IsInWater()?0.f:1.f);

			if(m_IsFocusVehicle)
			{
				const f32 bankVol = m_BankSpraySmoother.CalculateValue(inWaterFactor, fwTimer::GetTimeInMilliseconds());

				if(bankVol > g_SilenceVolumeLin)
				{
					if(!m_BankSpray)
					{
						audSoundInitParams initParams;
						initParams.UpdateEntity = true;
						initParams.EnvironmentGroup = m_Vehicle->GetAudioEnvironmentGroup();
						CreateSound_PersistentReference(m_BoatAudioSettings->BankSpraySound, &m_BankSpray, &initParams);

						if(m_BankSpray)
						{
							m_BankSpray->PrepareAndPlay();
						}
					}

					if(m_BankSpray)
					{
						m_BankSpray->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(bankVol) BANK_ONLY(+ g_BankSprayVolumeTrim));
						Vector3 boatRight = VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetA());						
						Vector3 boatForward = VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetB());						
						boatSprayAngle = DotProduct(boatRight, g_UnitUp);
						m_BankSpray->FindAndSetVariableValue(ATSTRINGHASH("angle", 0xF3805B5), Abs(boatSprayAngle));
						boatRight.SetZ(0.0f);
						boatForward.SetZ(0.0f);

						// Pan the sound left and right as the angle changes
						m_BankSpray->SetRequestedPosition(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()) + (boatForward * g_BankSprayForwardDistanceScalar * ((vehicleVariables.isInFirstPersonCam || vehicleVariables.isInHoodMountedCam) ? 0.0f : 1.0f)) +  (boatRight * g_BankSprayPanDistanceScalar * Clamp(boatSprayAngle * 2, -1.0f, 1.0f)));
					}
				}
				else if(m_BankSpray)
				{
					m_BankSpray->StopAndForget();
				}
			}		
			else if(m_BankSpray)
			{
				m_BankSpray->StopAndForget();
			}					

#if __BANK
			if(m_IsPlayerVehicle)
			{
				if(g_SimulatePlayerEnteringVehicle)
				{
					OnFocusVehicleChanged();
					g_SimulatePlayerEnteringVehicle = false;
				}

				if(g_DebugDraw)
				{
					char tempString[128];
					sprintf(tempString, "In Water Factor: %.02f", inWaterFactor);
					grcDebugDraw::Text(Vector2(0.1f, 0.11f), Color32(0,0,255), tempString );

					sprintf(tempString, "Speed Factor: %.02f", speedFactor);
					grcDebugDraw::Text(Vector2(0.1f, 0.13f), Color32(0,0,255), tempString );

					sprintf(tempString, "Spray Angle: %.02f", Abs(boatSprayAngle));
					grcDebugDraw::Text(Vector2(0.1f, 0.15f), Color32(0,0,255), tempString );

					sprintf(tempString, "Out of Water Timer: %.02f", m_OutOfWaterTimer);
					grcDebugDraw::Text(Vector2(0.1f, 0.17f), Color32(0,0,255), tempString );

					sprintf(tempString, "In Water Timer: %.02f", m_InWaterTimer);
					grcDebugDraw::Text(Vector2(0.1f, 0.19f), Color32(0,0,255), tempString );

					sprintf(tempString, "Raw Revs: %.02f", vehicleVariables.rawRevs);
					grcDebugDraw::Text(Vector2(0.1f, 0.23f), Color32(0,0,255), tempString );

					sprintf(tempString, "Smoothed Revs: %.02f", vehicleVariables.revs);
					grcDebugDraw::Text(Vector2(0.1f, 0.25f), Color32(0,0,255), tempString );

					sprintf(tempString, "Velocity Sq: %.02f", m_CachedVehicleVelocity.Mag2());
					grcDebugDraw::Text(Vector2(0.1f, 0.27f), Color32(0,0,255), tempString );

					if(m_BankSpray)
					{
						grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_BankSpray->GetRequestedPosition()), 1.f, Color32(255, 0, 0, (int)(255 * audDriverUtil::ComputeLinearVolumeFromDb(m_BankSpray->GetRequestedVolume()))));
					}

					if(IsSubmersible())
					{
						sprintf(tempString, "Angular Velocity: %.02f", m_SubAngularVelocityMag);
						grcDebugDraw::Text(Vector2(0.1f, 0.29f), Color32(0,0,255), tempString );

						sprintf(tempString, "In Water: %s", m_Vehicle->GetIsInWater() ? "true" : "false");
						grcDebugDraw::Text(Vector2(0.1f, 0.31f), Color32(0, 0, 255), tempString);
					}
				}
			}
#endif
		}
		else
		{			
			m_WaterTurbulenceOutOfWaterTimer = 0.0f;
			m_WaterTurbulenceInWaterTimer = 0.0f;
		}
	}

	m_WasEngineOnLastFrame = m_Vehicle->m_nVehicleFlags.bEngineOn;
}

// ----------------------------------------------------------------
// audBoatAudioEntity CalculateIsInWater
// ----------------------------------------------------------------
bool audBoatAudioEntity::CalculateIsInWater() const
{
#if GTA_REPLAY // For replay we set this from CPacketBoatUpdate
	if(CReplayMgr::IsEditModeActive())
	{	
		return m_ReplayIsInWater;
	}
#endif
	if(IsSubmersible() || m_Vehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINE)
	{
		return m_Vehicle->GetIsInWater();
	}
	else
	{
		if(m_Vehicle->GetVehicleType() != VEHICLE_TYPE_SUBMARINECAR)
		{
			CBoat* boat = (CBoat*)m_Vehicle;
			return m_WaterSamples.IsSet(CVfxWater::GetEntryWaterSampleIndex(boat));
		}
		else
		{
			return false;
		}		
	}
}

// ----------------------------------------------------------------
// audBoatAudioEntity GetVehicleRainSound
// ----------------------------------------------------------------
u32 audBoatAudioEntity::GetVehicleRainSound(bool interiorView) const
{
	if(m_BoatAudioSettings)
	{
		if(interiorView)
		{
			return m_BoatAudioSettings->VehicleRainSoundInterior;
		}
		else
		{
			return m_BoatAudioSettings->VehicleRainSound;
		}		
	}

	return audVehicleAudioEntity::GetVehicleRainSound(interiorView);
}

// ----------------------------------------------------------------
// audBoatAudioEntity TriggerDoorOpenSound
// ----------------------------------------------------------------
void audBoatAudioEntity::TriggerDoorOpenSound(eHierarchyId doorIndex)
{
	if(IsDisabled())
	{
		return;
	}

	if(m_BoatAudioSettings)
	{
		u32 hash = m_BoatAudioSettings->DoorOpen;
		TriggerDoorSound(hash, doorIndex);
	}
}

// ----------------------------------------------------------------
// audBoatAudioEntity TriggerDoorCloseSound
// ----------------------------------------------------------------
void audBoatAudioEntity::TriggerDoorCloseSound(eHierarchyId doorIndex, const bool UNUSED_PARAM(isBroken))
{
	if(IsDisabled())
	{
		return;
	}

	if(m_BoatAudioSettings)
	{
		u32 hash = m_BoatAudioSettings->DoorClose;
		TriggerDoorSound(hash, doorIndex);
	}
}

// ----------------------------------------------------------------
// audBoatAudioEntity TriggerDoorFullyOpenSound
// ----------------------------------------------------------------
void audBoatAudioEntity::TriggerDoorFullyOpenSound(eHierarchyId doorIndex)
{
	if(IsDisabled())
	{
		return;
	}

	if(m_BoatAudioSettings)
	{
		u32 hash = m_BoatAudioSettings->DoorLimit;
		TriggerDoorSound(hash, doorIndex);
	}
}

// ----------------------------------------------------------------
// audBoatAudioEntity TriggerDoorStartCloseSound
// ----------------------------------------------------------------
void audBoatAudioEntity::TriggerDoorStartCloseSound(eHierarchyId doorIndex)
{
	if(IsDisabled())
	{
		return;
	}

	if(m_BoatAudioSettings)
	{
		u32 hash = m_BoatAudioSettings->DoorStartClose;
		TriggerDoorSound(hash, doorIndex);
	}
}

// ----------------------------------------------------------------
// audBoatAudioEntity GetEngineIgnitionSound
// ----------------------------------------------------------------
u32 audBoatAudioEntity::GetEngineIgnitionLoopSound() const			
{
	if(m_BoatAudioSettings)
	{
		return m_BoatAudioSettings->IgnitionLoop;
	}

	return audVehicleAudioEntity::GetEngineIgnitionLoopSound();
}

// ----------------------------------------------------------------
// audBoatAudioEntity GetWaveHitBigAirTime
// ----------------------------------------------------------------
f32 audBoatAudioEntity::GetWaveHitBigAirTime() const
{
	if(m_BoatAudioSettings)
	{
		return m_BoatAudioSettings->BigAirMinTime;
	}

	return audVehicleAudioEntity::GetWaveHitBigAirTime();
}

// ----------------------------------------------------------------
// audBoatAudioEntity GetTurretSoundSet
// ----------------------------------------------------------------
u32 audBoatAudioEntity::GetTurretSoundSet() const
{	
	if (GetVehicleModelNameHash() == ATSTRINGHASH("PATROLBOAT", 0xEF813606))
	{
		return ATSTRINGHASH("PATROLBOAT_TURRET_SOUNDS", 0x92EBBEC5);
	}

	return audVehicleAudioEntity::GetTurretSoundSet();
}

// ----------------------------------------------------------------
// audBoatAudioEntity GetEngineShutdownSound
// ----------------------------------------------------------------
u32 audBoatAudioEntity::GetEngineShutdownSound() const
{
	if(m_BoatAudioSettings)
	{
		if(IsSubmersible() && !m_WasInWaterLastFrame)
		{
			// GTAV fix - subs are using a bubble-y underwater sound here, which makes no sense on dry land
			return g_NullSoundHash;
		}
		else
		{
			return m_BoatAudioSettings->ShutdownOneShot;
		}		
	}

	return audVehicleAudioEntity::GetEngineShutdownSound();
}

// ----------------------------------------------------------------
// audBoatAudioEntity GetEngineShutdownSound
// ----------------------------------------------------------------
u32 audBoatAudioEntity::GetEngineStartupSound() const
{
	if(m_BoatAudioSettings)
	{		
		if(IsSubmersible() && !m_WasInWaterLastFrame)
		{
			// GTAV fix - subs are using a bubble-y underwater sound here, which makes no sense on dry land
			return g_NullSoundHash;
		}
		else
		{
			return m_BoatAudioSettings->EngineStartUp;
		}		
	}

	return audVehicleAudioEntity::GetEngineStartupSound();
}

// ----------------------------------------------------------------
// audBoatAudioEntity GetIdleHullSlapSound
// ----------------------------------------------------------------
u32 audBoatAudioEntity::GetIdleHullSlapSound() const
{
	if(m_BoatAudioSettings)
	{
		return m_BoatAudioSettings->IdleHullSlapLoop;
	}

	return audVehicleAudioEntity::GetIdleHullSlapSound();
}

// ----------------------------------------------------------------
// audBoatAudioEntity GetLeftWaterSound
// ----------------------------------------------------------------
u32 audBoatAudioEntity::GetLeftWaterSound() const
{
	if(m_BoatAudioSettings)
	{
		return m_BoatAudioSettings->LeftWaterSound;
	}

	return audVehicleAudioEntity::GetLeftWaterSound();
}

// ----------------------------------------------------------------
// audBoatAudioEntity GetWaveHitSound
// ----------------------------------------------------------------
u32 audBoatAudioEntity::GetWaveHitSound() const
{
	if(m_BoatAudioSettings)
	{
		return m_BoatAudioSettings->WaveHitSound;
	}

	return audVehicleAudioEntity::GetWaveHitSound();
}


// ----------------------------------------------------------------
// audBoatAudioEntity GetWaveHitBigSound
// ----------------------------------------------------------------
u32 audBoatAudioEntity::GetWaveHitBigSound() const
{
	if(m_BoatAudioSettings)
	{
		return m_BoatAudioSettings->WaveHitBigAirSound;
	}

	return audVehicleAudioEntity::GetWaveHitBigSound();
}

// ----------------------------------------------------------------
// Compute the ignition hold time
// ----------------------------------------------------------------
f32 audBoatAudioEntity::ComputeIgnitionHoldTime(bool isStartingThisTime)
{
	// Maintaining behaviour of old code - non-granular boats just have a one shot ignition rather than a proper hold time
	if(IsUsingGranularEngine())
	{
		bool isStartingFirstTime = (isStartingThisTime && m_Vehicle->m_failedEngineStartAttempts == 0);
		const f32 maxHold = (isStartingFirstTime?sm_StartingIgnitionMaxHold:sm_IgnitionMaxHold);
		const f32 minHold = (isStartingFirstTime?sm_StartingIgnitionMinHold:sm_IgnitionMinHold);
		f32 timeFactor = 1.f;

		if(!isStartingFirstTime && (m_Vehicle->GetStatus() == STATUS_WRECKED || m_Vehicle->m_Transmission.GetEngineHealth() <= ENGINE_DAMAGE_ONFIRE))
		{
			// its not going to start so try for longer
			timeFactor = 2.5f;
		}

		m_IgnitionHoldTime = audEngineUtil::GetRandomNumberInRange(minHold * timeFactor, maxHold * timeFactor);
	}
	else
	{
		m_IgnitionHoldTime = 0.0f;
	}
	
	return m_IgnitionHoldTime;
}

// ----------------------------------------------------------------
// boat min openness
// ----------------------------------------------------------------
f32 audBoatAudioEntity::GetMinOpenness() const 
{
	if(m_BoatAudioSettings)
	{
		return m_BoatAudioSettings->Openness;
	}
	return 1.f;
}
// ----------------------------------------------------------------
// audBoatAudioEntity UpdateNonEngine
// ----------------------------------------------------------------
void audBoatAudioEntity::UpdateNonEngine(audVehicleVariables& vehicleVariables, const u32 UNUSED_PARAM(timeInMs))
{
	if(m_Vehicle->m_nVehicleFlags.bEngineOn)
	{
		Vec3V pos;
		const s32 engineBoneId = m_Vehicle->GetVehicleModelInfo()->GetBoneIndex(VEH_ENGINE);
		f32 waterDepth = m_EngineWaterDepth;		

		// Using separate water timers for turbulence sounds - the default timers only monitor the big bow VFX 
		// at the front of the vehicle, so can report that the boat is not in water even when water VFX is playing visually.
		bool isAnyWaterVFXActive = IsAnyWaterVFXActive();

		if(isAnyWaterVFXActive)
		{
			m_WaterTurbulenceInWaterTimer += fwTimer::GetTimeStep();
			m_WaterTurbulenceOutOfWaterTimer = 0.f;
		}
		else
		{
			m_WaterTurbulenceOutOfWaterTimer += fwTimer::GetTimeStep();
			m_WaterTurbulenceInWaterTimer = 0.f;
		}

		f32 volLin = m_WaterTurbulenceVolumeCurve.CalculateValue((vehicleVariables.revs - 0.2f) * 1.25f);
		volLin *= isAnyWaterVFXActive? Clamp(m_WaterTurbulenceInWaterTimer * 3.0f, 0.0f, 1.0f) : 1.0f - Clamp(m_WaterTurbulenceOutOfWaterTimer * 3.0f, 0.0f, 1.0f);

		// Submersibles use the centre of the sub as the turbulence sound position
		if(volLin > g_SilenceVolumeLin)
		{
			if(IsSubmersible())
			{
				if (GetVehicleModelNameHash() == ATSTRINGHASH("Kosatka", 0x4FAF0D70))
				{
					pos = ComputeClosestPositionOnVehicleYAxis();
				}
				else
				{
					f32 waterZ = 0.f;
					Vector3 vRiverHitPos, vRiverHitNormal;
					Vector3 buoyancyProbePos = VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition());
					bool inRiver = m_Vehicle->m_Buoyancy.GetCachedRiverBoundProbeResult(vRiverHitPos, vRiverHitNormal) && (vRiverHitPos.z + REJECTIONABOVEWATER) > buoyancyProbePos.z;
					waterZ = inRiver ? vRiverHitPos.z : Water::GetWaterLevel(buoyancyProbePos, &waterZ, false, POOL_DEPTH, REJECTIONABOVEWATER, NULL);
					waterDepth = buoyancyProbePos.z - waterZ;
					pos = RCC_VEC3V(buoyancyProbePos);
				}
			}
			else if(engineBoneId !=-1 )
			{
				pos = m_Vehicle->TransformIntoWorldSpace(m_ExhaustOffsetPos);
			}
			else
			{
				pos = m_Vehicle->GetTransform().GetPosition();
			}

			// For submersibles, fade out the turbulence as we get below water
			if(IsSubmersible())
			{
				volLin *= 1.0f - Min(abs(Min((waterDepth + 1.0f), 0.0f))/2.0f, 1.0f);
			}
		
			if(!m_WaterTurbulenceSound)
			{
				audSoundInitParams initParams;
				initParams.AttackTime = m_WasEngineOnLastFrame? sm_LodSwitchFadeTime : 300;
				initParams.EnvironmentGroup = m_Vehicle->GetAudioEnvironmentGroup();
				CreateAndPlaySound_Persistent(m_BoatAudioSettings->WaterTurbulance, &m_WaterTurbulenceSound, &initParams);
			}

			if(m_WaterTurbulenceSound)
			{
				f32 vol = audDriverUtil::ComputeDbVolumeFromLinear(volLin);
				f32 pitch = m_WaterTurbulencePitchCurve.CalculateValue(vehicleVariables.revs);

				m_WaterTurbulenceSound->SetRequestedPitch(static_cast<s32>(pitch));
				m_WaterTurbulenceSound->SetRequestedVolume(vol BANK_ONLY(+ g_WaterTurbulenceVolumeTrim));
				m_WaterTurbulenceSound->SetRequestedPosition(pos);
			}
		}
		else if(m_WaterTurbulenceSound)
		{
			m_WaterTurbulenceSound->StopAndForget();
		}
	}
	else if(m_WaterTurbulenceSound)
	{
		m_WaterTurbulenceSound->SetReleaseTime(800);
		m_WaterTurbulenceSound->StopAndForget();
	}
}

#if NON_GRANULAR_BOATS_ENABLED
// ----------------------------------------------------------------
// audBoatAudioEntity UpdateEngine
// ----------------------------------------------------------------
void audBoatAudioEntity::UpdateEngine(audVehicleVariables& vehicleVariables, const u32 UNUSED_PARAM(timeInMs))
{
	naAssertf(m_Vehicle, "In UpdateEngine audBoatAudioentity as no vehicle!");
	naAssertf(m_BoatAudioSettings, "In UpdateEngine audBoatAudioEntity has no boat audio settings");
	
	if(m_EngineWaveSlot)
	{
		if(m_WasEngineOnLastFrame && !m_Vehicle->m_nVehicleFlags.bEngineOn)
		{
			audSoundInitParams ignitionInitParams;
			ignitionInitParams.EnvironmentGroup = m_Vehicle->GetAudioEnvironmentGroup();
			ignitionInitParams.Tracker = m_Vehicle->GetPlaceableTracker();
			ignitionInitParams.UpdateEntity = true;
			CreateAndPlaySound(m_BoatAudioSettings->ShutdownOneShot,&ignitionInitParams);
		}
	}

	if(!m_Vehicle->m_nVehicleFlags.bEngineOn)
	{
		StopEngineSounds();
	}
	else if(!m_EngineOn)
	{
		StartEngineSounds();
	}

	if(m_Vehicle->m_nVehicleFlags.bEngineOn)
	{
		for(u32 i = 0 ; i < BOAT_ENGINE_SOUND_TYPE_MAX; i++)
		{
			if(m_EngineSounds[i])
			{
				f32 volLin = m_EngineCurves[i].CalculateValue(vehicleVariables.revs);
				f32 vol = audDriverUtil::ComputeDbVolumeFromLinear(volLin);
				f32 pitch = m_EnginePitchCurves[i].CalculateValue(vehicleVariables.revs);

				if(IsSubmersible() && 
				   (i == BOAT_ENGINE_SOUND_TYPE_ENGINE_1 || i == BOAT_ENGINE_SOUND_TYPE_ENGINE_2))
				{
					pitch += m_SubTurningEnginePitchModifier.CalculateValue(m_SubAngularVelocityMag);
				}

				m_EngineSounds[i]->SetRequestedPitch(static_cast<s32>(pitch));
				m_EngineSounds[i]->SetRequestedVolume(vol BANK_ONLY(+ g_VolumeTrims[i]));

				if(i == 0)
				{
					m_EngineSounds[i]->SetRequestedPostSubmixVolumeAttenuation(m_BoatAudioSettings->ExhaustVolPostSubmix);
				}
				else if(i == 1)
				{
					m_EngineSounds[i]->SetRequestedPostSubmixVolumeAttenuation(m_BoatAudioSettings->EngineVolPostSubmix);
				}
			}
		}
	}
}
#endif

// ----------------------------------------------------------------
// audBoatAudioEntity UpdateEngineGranular
// ----------------------------------------------------------------
void audBoatAudioEntity::UpdateEngineGranular(audVehicleVariables& vehicleVariables, const u32 UNUSED_PARAM(timeInMs))
{
	naAssertf(m_Vehicle, "In UpdateEngine audBoatAudioentity as no vehicle!");
	naAssertf(m_BoatAudioSettings, "In UpdateEngine audBoatAudioEntity has no boat audio settings");

	if(IsSubmersible())
	{
		m_VehicleEngine.SetGranularPitch(static_cast<s32>(m_SubTurningEnginePitchModifier.CalculateValue(m_SubAngularVelocityMag)));
	}
	else
	{
		m_VehicleEngine.SetGranularPitch(vehicleVariables.granularEnginePitchOffset);
	}

	m_VehicleEngine.Update(&vehicleVariables);
}

// -------------------------------------------------------------------------------
// Get the engine submix synth def
// -------------------------------------------------------------------------------
u32 audBoatAudioEntity::GetEngineSubmixSynth() const
{
	if(ShouldUseDSPEffects() && m_BoatAudioSettings)
	{
		return m_BoatAudioSettings->EngineSynthDef;
	}
	else
	{
		return audVehicleAudioEntity::GetEngineSubmixSynth();
	}
}

// -------------------------------------------------------------------------------
// Get the engine submix synth preset
// -------------------------------------------------------------------------------
u32 audBoatAudioEntity::GetEngineSubmixSynthPreset() const
{
	if(ShouldUseDSPEffects() && m_BoatAudioSettings)
	{
		return m_BoatAudioSettings->EngineSynthPreset;
	}
	else
	{
		return audVehicleAudioEntity::GetEngineSubmixSynthPreset();
	}
}

// -------------------------------------------------------------------------------
// Get the exhaust submix synth def
// -------------------------------------------------------------------------------
u32 audBoatAudioEntity::GetExhaustSubmixSynth() const
{
	if(ShouldUseDSPEffects() && m_BoatAudioSettings)
	{
		return m_BoatAudioSettings->ExhaustSynthDef;
	}
	else
	{
		return audVehicleAudioEntity::GetExhaustSubmixSynth();
	}
}

// -------------------------------------------------------------------------------
// Get the exhaust submix synth preset
// -------------------------------------------------------------------------------
u32 audBoatAudioEntity::GetExhaustSubmixSynthPreset() const
{
	if(ShouldUseDSPEffects() && m_BoatAudioSettings)
	{
		return m_BoatAudioSettings->ExhaustSynthPreset;
	}
	else
	{
		return audVehicleAudioEntity::GetExhaustSubmixSynthPreset();
	}
}

// -------------------------------------------------------------------------------
// Get the engine submix voice
// -------------------------------------------------------------------------------
u32 audBoatAudioEntity::GetEngineSubmixVoice() const			
{
	if(m_BoatAudioSettings)
	{
		return m_BoatAudioSettings->EngineSubmixVoice;
	}
	else
	{
		return audVehicleAudioEntity::GetEngineSubmixVoice();
	}
}

// -------------------------------------------------------------------------------
// Get the exhaust submix voice
// -------------------------------------------------------------------------------
u32 audBoatAudioEntity::GetExhaustSubmixVoice() const			
{
	if(m_BoatAudioSettings)
	{
		return m_BoatAudioSettings->ExhaustSubmixVoice;
	}
	else
	{
		return audVehicleAudioEntity::GetExhaustSubmixVoice();
	}
}

#if NON_GRANULAR_BOATS_ENABLED
// ----------------------------------------------------------------
// Start engine sounds
// ----------------------------------------------------------------
void audBoatAudioEntity::StartEngineSounds()
{
	naAssertf(m_BoatAudioSettings, "No boat audio setting in StartEngineSounds");
	naAssertf(m_EngineWaveSlot, "Invalid engine slot index in StartengineSounds");
	audSoundInitParams initParams;

	if(!m_Vehicle->m_nVehicleFlags.bSkipEngineStartup && !m_WasEngineOnLastFrame)
	{
		audSoundInitParams ignitionInitParams;
		ignitionInitParams.EnvironmentGroup = m_Vehicle->GetAudioEnvironmentGroup();
		ignitionInitParams.Tracker = m_Vehicle->GetPlaceableTracker();
		ignitionInitParams.UpdateEntity = true;
		CreateAndPlaySound(m_BoatAudioSettings->IgnitionOneShot,&ignitionInitParams);
		initParams.Predelay = 700;
	}
	else
	{
		initParams.AttackTime = m_WasEngineOnLastFrame? sm_LodSwitchFadeTime : 300;
	}
	
	initParams.UpdateEntity = true;
	initParams.u32ClientVar = AUD_VEHICLE_SOUND_ENGINE;
	initParams.EnvironmentGroup = m_Vehicle->GetAudioEnvironmentGroup();
	initParams.Category = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("VEHICLES_BOATS_ENGINES", 0x6CB697B1));

	Assign(initParams.EffectRoute, GetExhaustEffectRoute());

	if(!m_EngineSounds[BOAT_ENGINE_SOUND_TYPE_ENGINE_1])
	{
		CreateSound_PersistentReference(m_BoatAudioSettings->Engine1Loop, &m_EngineSounds[BOAT_ENGINE_SOUND_TYPE_ENGINE_1], &initParams);
	}
	
	Assign(initParams.EffectRoute, GetEngineEffectRoute());

	if(!m_EngineSounds[BOAT_ENGINE_SOUND_TYPE_ENGINE_2])
	{
		CreateSound_PersistentReference(m_BoatAudioSettings->Engine2Loop, &m_EngineSounds[BOAT_ENGINE_SOUND_TYPE_ENGINE_2], &initParams);
	}
	
	initParams.EffectRoute = 0;

	if(!m_EngineSounds[BOAT_ENGINE_SOUND_TYPE_LOW_RESO])
	{
		CreateSound_PersistentReference(m_BoatAudioSettings->LowResoLoop, &m_EngineSounds[BOAT_ENGINE_SOUND_TYPE_LOW_RESO], &initParams);
	}
	
	if(!m_EngineSounds[BOAT_ENGINE_SOUND_TYPE_RESO])
	{
		CreateSound_PersistentReference(m_BoatAudioSettings->ResoLoop, &m_EngineSounds[BOAT_ENGINE_SOUND_TYPE_RESO], &initParams);
	}

	for(u32 i = 0 ; i < BOAT_ENGINE_SOUND_TYPE_MAX; i++)
	{
		if(m_EngineSounds[i])
		{
			m_EngineSounds[i]->PrepareAndPlay(m_EngineWaveSlot->waveSlot,false,15000);
		}
	}

	m_EngineOn = true;
}
#endif 

// ----------------------------------------------------------------
// Stop engine sounds
// ----------------------------------------------------------------
void audBoatAudioEntity::StopEngineSounds(bool convertedToDummy)
{
#if NON_GRANULAR_BOATS_ENABLED
	for(u32 i = 0 ; i < BOAT_ENGINE_SOUND_TYPE_MAX; i++)
	{
		if(m_EngineSounds[i])
		{
			m_EngineSounds[i]->SetReleaseTime(convertedToDummy ? sm_LodSwitchFadeTime : 800);
			m_EngineSounds[i]->StopAndForget();
		}
	}
#else
	UNUSED_VAR(convertedToDummy);
#endif

	m_EngineOn = false;
}

// ----------------------------------------------------------------
// Get boat audio settings
// ----------------------------------------------------------------
BoatAudioSettings *audBoatAudioEntity::GetBoatAudioSettings()
{
	BoatAudioSettings *settings = NULL;

	if(g_AudioEngine.IsAudioEnabled())
	{
		if(m_ForcedGameObject != 0u)
		{
			settings = audNorthAudioEngine::GetObject<BoatAudioSettings>(m_ForcedGameObject);
			m_ForcedGameObject = 0u;
		}

		if(!settings)
		{
			if(m_Vehicle->GetVehicleModelInfo()->GetAudioNameHash() != 0)
			{
				settings = audNorthAudioEngine::GetObject<BoatAudioSettings>(m_Vehicle->GetVehicleModelInfo()->GetAudioNameHash());
			}

			if(!settings)
			{
				settings = audNorthAudioEngine::GetObject<BoatAudioSettings>(GetVehicleModelNameHash());
			}
		}

		if(!audVerifyf(settings, "Couldn't find boat audio settings for %s", GetVehicleModelName()))
		{
			// fall back to default for now
			settings = audNorthAudioEngine::GetObject<BoatAudioSettings>(ATSTRINGHASH("JETMAX", 0x33581161));
		}
		else
		{
			m_ManufacturerHash = settings->ScannerMake;
			m_ModelHash = settings->ScannerModel;
			m_CategoryHash = settings->ScannerCategory;
			m_ScannerVehicleSettingsHash = settings->ScannerVehicleSettings;
		}
	}

	return settings;
}

// ----------------------------------------------------------------
// Get a sound from the object data
// ----------------------------------------------------------------
u32 audBoatAudioEntity::GetSoundFromObjectData(const u32 fieldHash) const
{ 
	if(m_BoatAudioSettings) 
	{ 
		u32 *ret =(u32 *)(m_BoatAudioSettings->GetFieldPtr(fieldHash)); 
		return (ret) ? *ret : 0;
	} 
	return 0;
}


VehicleCollisionSettings* audBoatAudioEntity::GetVehicleCollisionSettings() const
{
	if(m_BoatAudioSettings)
	{
		return audNorthAudioEngine::GetObject<VehicleCollisionSettings>(m_BoatAudioSettings->VehicleCollisions);
	}

	return audVehicleAudioEntity::GetVehicleCollisionSettings();
}

#if __BANK
// ----------------------------------------------------------------
// Add widgets to RAG tool
// ----------------------------------------------------------------
void audBoatAudioEntity::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Boats", false);
		bank.AddToggle("Enable Granular Debug Rendering", &g_GranularDebugRenderingEnabled);
		bank.AddToggle("Debug Draw", &g_DebugDraw);
		bank.AddSlider("Engine 1 Volume Trim", &g_VolumeTrims[0], -100.0f, 100.f, 0.1f);
		bank.AddSlider("Engine 2 Volume Trim", &g_VolumeTrims[1], -100.0f, 100.f, 0.1f);
		bank.AddSlider("Low Reso Trim", &g_VolumeTrims[2], -100.0f, 100.f, 0.1f);
		bank.AddSlider("Reso Volume Trim", &g_VolumeTrims[3], -100.0f, 100.f, 0.1f);
		bank.AddSlider("Water Turbulance Volume Trim", &g_WaterTurbulenceVolumeTrim, -100.0f, 100.f, 0.1f);
		bank.AddSlider("Bank Spray Volume Trim", &g_BankSprayVolumeTrim, -100.0f, 100.f, 0.1f);
		bank.AddSlider("Bank Spray Panning Distance Scalar", &g_BankSprayPanDistanceScalar, 0.0f, 10.f, 0.1f);
		bank.AddSlider("Bank Spray Forward Distance Scalar", &g_BankSprayForwardDistanceScalar, -50.0f, 50.f, 0.1f);		
		bank.AddSlider("Submersible Underwater Cutoff", &g_SubmersibleUnderwaterCutoff, 0.0f, 24000.f, 1.0f);

		bank.PushGroup("NPC Fake Revs");
			bank.AddToggle("Treat All Boats as NPCs", &g_TreatAllBoatsAsNPCs);
			bank.AddSlider("Fake Revs Offset Above", &g_FakeRevsOffsetAbove, 0.0f, 1.0f, 0.001f);
			bank.AddSlider("Fake Revs Offset Below", &g_FakeRevsOffsetBelow, 0.0f, 1.0f, 0.001f);
			bank.AddSlider("Fake Revs Min Hold Time Min", &g_FakeRevsMinHoldTimeMin, 0.0f, 10.0f, 0.01f);
			bank.AddSlider("Fake Revs Min Hold Time Max", &g_FakeRevsMinHoldTimeMax, 0.0f, 10.0f, 0.01f);
			bank.AddSlider("Fake Revs Max Hold Time Min", &g_FakeRevsMaxHoldTimeMin, 0.0f, 10.0f, 0.01f);
			bank.AddSlider("Fake Revs Max Hold Time Max", &g_FakeRevsMaxHoldTimeMax, 0.0f, 10.0f, 0.01f);
			bank.AddSlider("Fake Revs Increase Rate Base", &g_FakeRevsIncreaseRateBase, 0.0f, 200000.0f, 1000.0f);
			bank.AddSlider("Fake Revs Increase Rate Scale", &g_FakeRevsIncreaseRateScale, 0.0f, 100.f, 0.1f);
			bank.AddSlider("Fake Revs Decrease Rate Base", &g_FakeRevsDecreaseRateBase, 0.0f, 200000.0f, 1000.0f);
			bank.AddSlider("Fake Revs Decrease Rate Scale", &g_FakeRevsDecreaseRateScale, 0.0f, 100.f, 0.1f);
			bank.AddSlider("Fake Out of Water Min Time", &g_FakeOutOfWaterMinTime, 0.0f, 100.0f, 0.1f);
			bank.AddSlider("Fake Out of Water Max Time", &g_FakeOutOfWaterMaxTime, 0.0f, 100.0f, 0.1f);
			bank.AddSlider("Fake In Water Min Time", &g_FakeInWaterMinTime, 0.0f, 100.0f, 0.1f);
			bank.AddSlider("Fake In Water Max Time", &g_FakeInWaterMaxTime, 0.0f, 100.0f, 0.1f);
		bank.PopGroup();
	bank.PopGroup();
}
#endif
