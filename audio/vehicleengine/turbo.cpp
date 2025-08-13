#include "turbo.h"
#include "audio/caraudioentity.h"
#include "audioengine/engine.h"
#include "audioengine/widgets.h"
#include "audiosoundtypes/sound.h"
#include "vehicles/vehicle.h"
#include "Peds/ped.h"

AUDIO_VEHICLES_OPTIMISATIONS()

audCurve audVehicleTurbo::sm_RevsToTurboBoostCurve;
audCurve audVehicleTurbo::sm_TurboBoostToVolCurve; 
audCurve audVehicleTurbo::sm_TurboBoostToPitchCurve;

f32 g_TurboSpinUp = 1.0f / 1000.f;
f32 g_TurboSpinDown = 3.0f / 1000.f;
f32 g_DumpValvePressureThreshold = 0.3f;

extern audCategory *g_EngineCategory;
extern f32 g_TurboVolumeTrim;

// -------------------------------------------------------------------------------
// audVehicleTransmission constructor
// -------------------------------------------------------------------------------
audVehicleTurbo::audVehicleTurbo() :
  m_Parent(NULL)
, m_VehicleEngineAudioSettings(NULL)
, m_TurboWhineLoop(NULL)
, m_DumpValveSound(NULL)
, m_LastDumpValveTime(0u)
, m_LastDumpValveExhaustPopTime(0u)
, m_PrevGear(0)
, m_HasDumpValve(false)
{
}

// -------------------------------------------------------------------------------
// audVehicleTransmission InitClass
// -------------------------------------------------------------------------------
void audVehicleTurbo::InitClass()
{
	VerifyExists(sm_TurboBoostToPitchCurve.Init(ATSTRINGHASH("TURBO_CURVES_BOOST_TO_PITCH", 0xCA53458F)), "invalid turbo boost to pitch curve");
	VerifyExists(sm_TurboBoostToVolCurve.Init(ATSTRINGHASH("TURBO_CURVES_BOOST_TO_VOL", 0x8D5A153)), "invalid turbo boost to vol curve");
	VerifyExists(sm_RevsToTurboBoostCurve.Init(ATSTRINGHASH("TURBO_CURVES_REVS_TO_BOOST", 0xA8986BDC)), "invalid Revs to boost curve");
}

// -------------------------------------------------------------------------------
// audVehicleTransmission Init
// -------------------------------------------------------------------------------
void audVehicleTurbo::Init(audCarAudioEntity* parent)
{
	naAssertf(parent, "Initialising vehicle transmission without a valid vehicle");
	m_Parent = parent;

	if( m_Parent )
	{
		m_VehicleEngineAudioSettings = m_Parent->GetVehicleEngineAudioSettings();

		if( m_VehicleEngineAudioSettings )
		{
			if( m_Parent->GetVehicle() )
			{
				m_HasDumpValve = ENTITY_SEED_PROB(m_Parent->GetVehicle()->GetRandomSeed(), m_VehicleEngineAudioSettings->DumpValveProb*0.01f);
			}

			m_TurboSmoother.Init(g_TurboSpinUp * (m_VehicleEngineAudioSettings->TurboSpinupSpeed*0.01f), g_TurboSpinDown * (m_VehicleEngineAudioSettings->TurboSpinupSpeed*0.01f));
		}
	}
}

// -------------------------------------------------------------------------------
// audVehicleTurbo update
// -------------------------------------------------------------------------------
void audVehicleTurbo::Update(audVehicleVariables *state)
{
	if( !m_VehicleEngineAudioSettings )
	{
		return;
	}

	u32 turboWhineSound = m_VehicleEngineAudioSettings->TurboWhine;
	if(m_Parent->IsTurboUpgraded() && m_VehicleEngineAudioSettings->UpgradedTurboWhine != g_NullSoundHash)
	{
		turboWhineSound = m_VehicleEngineAudioSettings->UpgradedTurboWhine;
	}

	// disable boost in reverse, scale by throttle
	const f32 revsBoost = state->gear!=0 ? static_cast<float>(sm_RevsToTurboBoostCurve.CalculateValue(state->revs)) : 0.f;
	const f32 requestedBoost = revsBoost * state->throttle;
	const f32 upgradeLevel = m_Parent->GetTurboUpgradeLevel();

	if(m_Parent->IsPlayerVehicle())
	{
		const f32 requestedInputBoost = revsBoost * state->throttleInput;

		// Cars can optionally turbo dump on gear changes regardless of pressure. For future projects this should be moved
		// into the vehicle gameobject
		const bool turboDumpOnUpgradedGearChange = (m_Parent->GetVehicleModelNameHash() == ATSTRINGHASH("SULTANRS", 0xEE6024BC)) || (m_Parent->GetVehicleModelNameHash() == ATSTRINGHASH("OMNIS", 0xD1AD4937));
		const bool turboDumpOnAnyGearChange = (m_Parent->GetVehicleModelNameHash() == ATSTRINGHASH("OMNIS", 0xD1AD4937) || m_Parent->GetVehicleModelNameHash() == ATSTRINGHASH("ELEGY", 0xBBA2261) || m_Parent->GetVehicleModelNameHash() == ATSTRINGHASH("JESTER3", 0xF330CB6A));
		const bool triggerDumpValveExhaustPops = (m_Parent->GetVehicleModelNameHash() == ATSTRINGHASH("SANCTUS", 0x58E316C7));

		if((requestedBoost + g_DumpValvePressureThreshold < m_TurboSmoother.GetLastValue()) || 
			(turboDumpOnUpgradedGearChange && state->gear > 1 && requestedInputBoost + g_DumpValvePressureThreshold < m_TurboSmoother.GetLastValue()) ||
			(turboDumpOnUpgradedGearChange && state->gear > 1 && m_PrevGear < state->gear && state->throttleInput > 0.9f && (turboDumpOnAnyGearChange || m_Parent->IsTurboUpgraded() || m_Parent->IsEngineUpgraded())))
		{
			u32 dumpValveSound = m_VehicleEngineAudioSettings->DumpValve;

			if(m_Parent->IsTurboUpgraded() && m_VehicleEngineAudioSettings->UpgradedDumpValve != g_NullSoundHash)
			{
				dumpValveSound = m_VehicleEngineAudioSettings->UpgradedDumpValve;
			}

			if((m_HasDumpValve || triggerDumpValveExhaustPops || m_Parent->IsTurboUpgraded()) && state->timeInMs > m_LastDumpValveTime + 750)
			{					
				m_LastDumpValveTime = state->timeInMs;
						
				audSoundInitParams initParams;
				initParams.SetVariableValue(ATSTRINGHASH("boostpressure", 0xCF28C7B3), m_TurboSmoother.GetLastValue());
				initParams.UpdateEntity = true;
				initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
				initParams.u32ClientVar = AUD_VEHICLE_SOUND_ENGINE;

				// V B*3065309 - Adding support for gear change exhaust pops, hijacking dump valve code as that basically does what we want anyway
				if(triggerDumpValveExhaustPops)
				{
					audSound** exhaustPopSoundPtr = m_Parent->GetVehicleEngine()->GetExhaustPopSoundPtr();

					if(!(*exhaustPopSoundPtr))
					{
						u32 exhaustPopHash = m_VehicleEngineAudioSettings->ExhaustPops;

						if(m_Parent->GetAudioVehicleType() == AUD_VEHICLE_CAR)
						{
							if(((audCarAudioEntity*)m_Parent)->IsExhaustUpgraded())
							{
								if(m_VehicleEngineAudioSettings->UpgradedExhaustPops != g_NullSoundHash)
								{
									exhaustPopHash = m_VehicleEngineAudioSettings->UpgradedExhaustPops;
								}
							}
						}

						m_Parent->CreateAndPlaySound_Persistent(exhaustPopHash, exhaustPopSoundPtr, &initParams);

						if(*exhaustPopSoundPtr)
						{
#if GTA_REPLAY
							if(!CReplayMgr::IsEditModeActive())
#endif
							{
								m_Parent->GetVehicle()->m_nVehicleFlags.bAudioBackfired = true;
								REPLAY_ONLY(CReplayMgr::ReplayRecordSound(exhaustPopHash, &initParams, m_Parent->GetVehicle()));
							}

							m_LastDumpValveExhaustPopTime = fwTimer::GetTimeInMilliseconds();
						}
					}					
				}						

				if(turboWhineSound != g_NullSoundHash && dumpValveSound != g_NullSoundHash && !m_DumpValveSound)
				{
					initParams.Volume = state->carVolOffset + state->engineConeAtten + g_TurboVolumeTrim + (m_VehicleEngineAudioSettings->MasterTurboVolume/100.f) + audDriverUtil::ComputeDbVolumeFromLinear(m_TurboSmoother.GetLastValue());

					// Dump valve can get hugely annoying, so only trigger the full dump sound at max fwd speed
					s32 startOffset = (s32) Max(0.0f, 25.0f - (25.0f * (state->fwdSpeedRatio * 1.12f)));
					initParams.StartOffset = startOffset;
					initParams.IsStartOffsetPercentage = true; 					
					initParams.SetVariableValue(ATSTRINGHASH("UpgradeLevel", 0x53C887C2), upgradeLevel);
					m_Parent->CreateAndPlaySound_Persistent(dumpValveSound, &m_DumpValveSound, &initParams);
				}						

				if(m_DumpValveSound)
				{
					m_DumpValveSound->SetRequestedFrequencySmoothingRate(m_Parent->GetFrequencySmoothingRate());					
				}						

				// boost pressure needs to be...dumped
				m_TurboSmoother.Reset();
			}
		}
	}

	f32 turboBoost = m_TurboSmoother.CalculateValue(requestedBoost, state->timeInMs);
	const f32 linVol = sm_TurboBoostToVolCurve.CalculateValue(turboBoost);
	const f32 pitchScale = sm_TurboBoostToPitchCurve.CalculateValue(turboBoost);
	const f32 pitchRatio = audDriverUtil::ConvertPitchToRatio(m_VehicleEngineAudioSettings->TurboMinPitch + (s32)((m_VehicleEngineAudioSettings->TurboMaxPitch - m_VehicleEngineAudioSettings->TurboMinPitch) * pitchScale));
	const bool turboWhineValid = turboWhineSound != g_NullSoundHash && m_Parent->IsPlayerVehicle() && linVol > g_SilenceVolumeLin;

	if(turboWhineValid)
	{
		if(!m_TurboWhineLoop)
		{
			audSoundInitParams initParams;
			initParams.Category = g_EngineCategory;
			initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
			m_Parent->CreateSound_PersistentReference(turboWhineSound, &m_TurboWhineLoop, &initParams);
			if(m_TurboWhineLoop)
			{
				m_TurboWhineLoop->SetUpdateEntity(true);
				m_TurboWhineLoop->SetClientVariable((u32)AUD_VEHICLE_SOUND_ENGINE);
				m_TurboWhineLoop->PrepareAndPlay();
			}
		}	

		if(m_TurboWhineLoop)
		{			
			m_TurboWhineLoop->SetRequestedVolume(g_TurboVolumeTrim + audDriverUtil::ComputeDbVolumeFromLinear(linVol) + (m_VehicleEngineAudioSettings->MasterTurboVolume/100.0f) + (m_Parent->GetTurboUpgradeLevel() * (m_VehicleEngineAudioSettings->UpgradedTurboVolumeBoost/100.0f)));
			m_TurboWhineLoop->SetRequestedPitch(audDriverUtil::ConvertRatioToPitch(pitchRatio));
			m_TurboWhineLoop->FindAndSetVariableValue(ATSTRINGHASH("UpgradeLevel", 0x53C887C2), upgradeLevel);
		}
	}
	else if(m_TurboWhineLoop)
	{
		m_TurboWhineLoop->StopAndForget();
	}
	
	f32 spinUpSpeed = g_TurboSpinUp;
	f32 spinDownSpeed = g_TurboSpinDown;

	// If we're in a network game and the driver isn't the local player, smooth the turbo spinup/down 
	// speeds more heavily to stop it sounding step-y
	if(NetworkInterface::IsGameInProgress())
	{			
		CPed* driverPed = m_Parent->GetVehicle()->GetDriver();

		if(driverPed && !driverPed->IsLocalPlayer())
		{
			spinUpSpeed *= 0.5f;
			spinDownSpeed = spinUpSpeed;
		}
	}		

	spinUpSpeed *= (m_VehicleEngineAudioSettings->TurboSpinupSpeed * 0.01f);
	spinDownSpeed *= (m_VehicleEngineAudioSettings->TurboSpinupSpeed * 0.01f);
	m_TurboSmoother.SetRates(spinUpSpeed, spinDownSpeed);	
	m_PrevGear = state->gear;
}

// -------------------------------------------------------------------------------
// audVehicleTransmission stop
// -------------------------------------------------------------------------------
void audVehicleTurbo::Stop()
{
	m_Parent->StopAndForgetSounds(m_TurboWhineLoop);
}
