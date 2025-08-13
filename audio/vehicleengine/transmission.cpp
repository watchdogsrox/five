#include "transmission.h"
#include "audio/audio_channel.h"
#include "audio/caraudioentity.h"
#include "audioengine/engine.h"
#include "audioengine/soundset.h"
#include "audioengine/widgets.h"
#include "audiosoundtypes/sound.h"
#include "vehicles/vehicle.h"

AUDIO_VEHICLES_OPTIMISATIONS()

audCurve audVehicleTransmission::sm_GearTransPitchCurve;

audSoundSet audVehicleTransmission::sm_ExtrasSoundSet;
audSoundSet audVehicleTransmission::sm_DamageSoundSet;

f32 g_BonnetCamGTOffset = 3.0f;
extern audCategory *g_EngineCategory;
extern f32 g_audVehicleDamageFactoGearBoxCrunch;
extern f32 g_audVehicleGearBoxCrunchProb;
extern f32 g_GearboxVolumeTrim;

f32 g_TransWobbleFreq = 4.0f;
f32 g_TransWobbleFreqVariance = 3.0f;
f32 g_TransWobbleMag = 0.2f;
f32 g_TransWobbleMagVariance = 0.05f;
f32 g_TransWobbleLength = 2.5f;
f32 g_TransWobbleLengthVariance = 1.0f;
BANK_ONLY(bool g_ForceTransWobble = false;)

// -------------------------------------------------------------------------------
// audVehicleTransmission constructor
// -------------------------------------------------------------------------------
audVehicleTransmission::audVehicleTransmission() :
  m_Parent(NULL)
, m_VehicleEngineAudioSettings(NULL)
, m_GearTransLoop(NULL)
, m_LastGearChangeTimeMs(0u)
, m_EngineLastGear(0u)
, m_LastGearChangeGear(0u)
{
}

// -------------------------------------------------------------------------------
// audVehicleTransmission InitClass
// -------------------------------------------------------------------------------
void audVehicleTransmission::InitClass()
{
	VerifyExists(sm_GearTransPitchCurve.Init(ATSTRINGHASH("GEAR_TRANS_PITCH_CURVE", 0x79E79183)),"invalid gear trans pitch curve");
	VerifyExists(sm_ExtrasSoundSet.Init(ATSTRINGHASH("VehicleExtras", 0xF68247A0)), "Failed to find VehicleExtras sound set");
	VerifyExists(sm_DamageSoundSet.Init(ATSTRINGHASH("VehicleDamage", 0x25E4D820)), "Failed to find VehicleDamage sound set");
}

// -------------------------------------------------------------------------------
// audVehicleTransmission Init
// -------------------------------------------------------------------------------
void audVehicleTransmission::Init(audCarAudioEntity* parent)
{
	naAssertf(parent, "Initialising vehicle transmission without a valid vehicle");
	m_Parent = parent;

	if( m_Parent )
	{
		m_VehicleEngineAudioSettings = m_Parent->GetVehicleEngineAudioSettings();
	}

	m_LastGearForWobble = 0;
	m_LastGearChangeGear = 0;
}

// -------------------------------------------------------------------------------
// audVehicleTransmission update
// -------------------------------------------------------------------------------
void audVehicleTransmission::Update(audVehicleVariables* state)
{
	if( !m_VehicleEngineAudioSettings )
	{
		return;
	}

	u32 gearTransLoop = m_VehicleEngineAudioSettings->GearTransLoop;

	if(m_Parent->IsTransmissionUpgraded() && m_VehicleEngineAudioSettings->UpgradedGearTransLoop != g_NullSoundHash)
	{
		gearTransLoop = m_VehicleEngineAudioSettings->UpgradedGearTransLoop;
	}

	// gear transmission sound
	if(gearTransLoop != g_NullSoundHash)
	{
		u32 timeInMs = fwTimer::GetTimeInMilliseconds();
		u32 timeSinceGearChange = timeInMs - m_LastGearChangeTimeMs;
		bool triggerUpShiftWobble = false;
		bool triggerDownShiftWobble = false;

		if(m_Parent->IsPlayerVehicle())
		{
			if(BANK_ONLY(g_ForceTransWobble ||) 
				(timeSinceGearChange < 1000 && 
				state->gear != m_LastGearForWobble))
			{
				if(state->gear > 1 BANK_ONLY(|| g_ForceTransWobble))
				{
#if __BANK
					if(g_ForceTransWobble)
					{
						triggerUpShiftWobble = true;
						g_ForceTransWobble = false;
					}
					else 
#endif
					if(state->gear >= m_LastGearForWobble && state->throttle == 1.0f)
					{
						triggerUpShiftWobble = true;
					}
					else if(state->clutchRatio == 1.0f)
					{
						triggerDownShiftWobble = true;
					}
				}
			}

			if(m_GearTransLoop && m_GearTransLoop->GetPlayState() == AUD_SOUND_PLAYING)
			{
				if(triggerUpShiftWobble)
				{
					//@@: location AUDVEHICLETRANSMISSION_UPDATE_ON_TRIGGER_SHIFT_UP_WOBBLE
					m_GearTransLoop->FindAndSetVariableValue(ATSTRINGHASH("W_Length", 0xC23BF9F9), g_TransWobbleLength + audEngineUtil::GetRandomNumberInRange(0.0f, g_TransWobbleLengthVariance));
					m_GearTransLoop->FindAndSetVariableValue(ATSTRINGHASH("W_Mag", 0x30FBBB42), g_TransWobbleMag + audEngineUtil::GetRandomNumberInRange(0.0f, g_TransWobbleMagVariance));
					m_GearTransLoop->FindAndSetVariableValue(ATSTRINGHASH("W_Frequency", 0x4DAC2C57), g_TransWobbleFreq + audEngineUtil::GetRandomNumberInRange(0.0f, g_TransWobbleFreqVariance));
					m_GearTransLoop->FindAndSetVariableValue(ATSTRINGHASH("W_Trigger", 0x5C31BE9C), 1);
					m_LastGearForWobble = state->gear;
				}
				else if(triggerDownShiftWobble)
				{
					//@@: location AUDVEHICLETRANSMISSION_UPDATE_ON_TRIGGER_SHIFT_DOWN_WOBBLE
					// For down shifts do a slower, longer wobble
					m_GearTransLoop->FindAndSetVariableValue(ATSTRINGHASH("W_Length", 0xC23BF9F9), (g_TransWobbleLength * 1.5f) + audEngineUtil::GetRandomNumberInRange(0.0f, g_TransWobbleLengthVariance * 1.5f));
					m_GearTransLoop->FindAndSetVariableValue(ATSTRINGHASH("W_Mag", 0x30FBBB42), (g_TransWobbleMag * 0.5f) + audEngineUtil::GetRandomNumberInRange(0.0f, g_TransWobbleMagVariance * 0.5f));
					m_GearTransLoop->FindAndSetVariableValue(ATSTRINGHASH("W_Frequency", 0x4DAC2C57), (g_TransWobbleFreq * 0.3f) + audEngineUtil::GetRandomNumberInRange(0.0f, g_TransWobbleFreqVariance * 0.3f));
					m_GearTransLoop->FindAndSetVariableValue(ATSTRINGHASH("W_Trigger", 0x5C31BE9C), 1);
					m_LastGearForWobble = state->gear;
				}
				else
				{
					m_GearTransLoop->FindAndSetVariableValue(ATSTRINGHASH("W_Trigger", 0x5C31BE9C), 0);
				}
			}
		}

		// no clutch -> no gear trans loop
		f32 camModeOffset = 0.0f;

		if(state->isInFirstPersonCam || state->isInHoodMountedCam)
		{
			camModeOffset = g_BonnetCamGTOffset;
		}

		bool reversing = state->fwdSpeed < 0.f;
		f32 upgradedTransmissionVolumeBoost = m_Parent->GetTransmissionUpgradeLevel() * (m_Parent->GetVehicleEngineAudioSettings()->UpgradedTransmissionVolumeBoost/100.0f);
		
		// Clutch ratio actually goes from 0.1 to 1.0, so scale up to a 0-1 range
		f32 clutchRatio = ((state->clutchRatio - 0.1f) * 1.1f);

		// -3dB quieter in reverse, to compensate for much quieter exhaust
		f32 linVol = audDriverUtil::ComputeLinearVolumeFromDb(g_GearboxVolumeTrim) * 
					 clutchRatio * 
					 audDriverUtil::ComputeLinearVolumeFromDb((reversing?-3.f:0.f) + upgradedTransmissionVolumeBoost + (m_VehicleEngineAudioSettings->MasterTransmissionVolume/100.0f) + camModeOffset + ((m_VehicleEngineAudioSettings->GTThrottleVol*0.01f) * state->throttle));

		const f32 pitchScale = sm_GearTransPitchCurve.CalculateValue(state->fwdSpeedRatio);
		const f32 pitchRatio = audDriverUtil::ConvertPitchToRatio(m_VehicleEngineAudioSettings->GearTransMinPitch + (s32)((m_VehicleEngineAudioSettings->GearTransMaxPitch - m_VehicleEngineAudioSettings->GearTransMinPitch) * pitchScale));

		// Only start a new transmission loop if this is the player vehicle
		if(m_GearTransLoop || m_Parent->IsPlayerVehicle())
		{
			m_Parent->UpdateLoopWithLinearVolumeAndPitchRatio(&m_GearTransLoop, gearTransLoop, linVol, pitchRatio, m_Parent->GetEnvironmentGroup(), g_EngineCategory, true, true);
		}

		if(m_GearTransLoop)
		{
			f32 revRatio = ((state->revs - 0.2f) * 1.25f);
			m_GearTransLoop->FindAndSetVariableValue(ATSTRINGHASH("RPM", 0x5B924509), revRatio);
			m_GearTransLoop->FindAndSetVariableValue(ATSTRINGHASH("Gear", 0xBE74155D), (f32)state->gear);
			m_GearTransLoop->FindAndSetVariableValue(ATSTRINGHASH("Throttle", 0xEA0151DC), (f32)state->throttle);
		}
	}
	else if(m_GearTransLoop)
	{
		m_GearTransLoop->StopAndForget();
	}

	UpdateGearChanges(state);
}

// -------------------------------------------------------------------------------
// audVehicleTransmission stop
// -------------------------------------------------------------------------------
void audVehicleTransmission::UpdateGearChanges(audVehicleVariables *state)
{
	CVehicle* vehicle = m_Parent->GetVehicle();

	if(vehicle)
	{
		u32 timeInMs = fwTimer::GetTimeInMilliseconds();
		f32 throttle = state->throttle;

		if(m_EngineLastGear != state->gear && 
		   vehicle->m_Transmission.GetNumGears() > 1 &&
		   vehicle->m_nVehicleFlags.bEngineOn)
		{
			if(m_LastGearChangeTimeMs + 500 < timeInMs)
			{
				m_GearChangePending = true;
				m_LastGearChangeTimeMs = timeInMs; 
			}
		}

		if(m_GearChangePending)
		{
			if(m_ThrottleLastFrame < throttle || throttle == 0.0f)
			{
				audMetadataRef soundRef = g_NullSoundRef;

				if(vehicle->GetVehicleType() == VEHICLE_TYPE_BIKE)
				{
					soundRef = sm_ExtrasSoundSet.Find(ATSTRINGHASH("GearChange_Bike", 0xE54B8139));
				}
				else
				{	
					if(m_LastGearChangeGear < state->gear && state->gear > 1 && state->engineDamageFactor > g_audVehicleDamageFactoGearBoxCrunch && audEngineUtil::ResolveProbability(g_audVehicleGearBoxCrunchProb * state->engineDamageFactor))
					{				
						soundRef = sm_DamageSoundSet.Find(ATSTRINGHASH("GearGrind_Car", 0xB34A814F));
					}
					else
					{
						if(m_Parent->IsPlayerVehicle() && state->isInFirstPersonCam)
						{
							if(m_Parent->IsTransmissionUpgraded() && m_Parent->GetVehicleEngineAudioSettings()->UpgradedGearChangeInt != g_NullSoundHash)
							{
								soundRef = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectMetadataRefFromHash(m_Parent->GetVehicleEngineAudioSettings()->UpgradedGearChangeInt);
							}
							else
							{
								// GTA V DLC specific fix - disable gear change sounds on non-upgraded WINDSOR. Should specify gear change sounds
								// in metadata for future projects
								if(m_Parent->GetVehicleModelNameHash() != ATSTRINGHASH("WINDSOR", 0x5E4327C8))
								{
									soundRef = sm_ExtrasSoundSet.Find(ATSTRINGHASH("GearChange_Internal", 0x24895154));
								}								
							}
						}
						else
						{
							if(m_Parent->IsTransmissionUpgraded() && m_Parent->GetVehicleEngineAudioSettings()->UpgradedGearChangeExt != g_NullSoundHash)
							{
								soundRef = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectMetadataRefFromHash(m_Parent->GetVehicleEngineAudioSettings()->UpgradedGearChangeExt);
							}
							else
							{
								// GTA V DLC specific fix - disable gear change sounds on non-upgraded WINDSOR. Should specify gear change sounds
								// in metadata for future projects
								if(m_Parent->GetVehicleModelNameHash() != ATSTRINGHASH("WINDSOR", 0x5E4327C8))
								{
									soundRef = sm_ExtrasSoundSet.Find(ATSTRINGHASH("GearChange_External", 0x39C8089));
								}								
							}
						}
					}
				}

				if(soundRef != g_NullSoundRef)
				{
					audSoundInitParams initParams;
					initParams.Tracker = vehicle->GetPlaceableTracker();
					initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
					initParams.Volume = g_GearboxVolumeTrim + (m_VehicleEngineAudioSettings->MasterTransmissionVolume/100.0f);
					m_Parent->CreateAndPlaySound(soundRef, &initParams);
				}
				
				m_GearChangePending = false;
				m_LastGearChangeGear = state->gear;
			}
			else if(timeInMs - m_LastGearChangeTimeMs > 400)
			{
				m_GearChangePending = false;
				m_LastGearChangeGear = state->gear;
			}
		}

		m_ThrottleLastFrame = throttle;
	}

	m_EngineLastGear = state->gear;
}

// -------------------------------------------------------------------------------
// audVehicleTransmission stop
// -------------------------------------------------------------------------------
void audVehicleTransmission::Stop()
{
	m_Parent->StopAndForgetSounds(m_GearTransLoop);
}
