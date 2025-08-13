#include "vehicleenginecomponent.h"
#include "audioengine/engine.h"
#include "audioengine/category.h"
#include "audiosoundtypes/sound.h"
#include "vehicles/vehicle.h"
#include "audio/caraudioentity.h"
#include "profile/group.h"

AUDIO_VEHICLES_OPTIMISATIONS()

extern audCategory* g_PlayerVolumeCategories[5];
extern audCategory *g_EngineCategory;
extern audCategory *g_EngineLoudCategory;
extern audCurve g_EngineLowVolCurve;
extern audCurve g_EngineHighVolCurve;
extern audCurve g_ExhaustLowVolCurve;
extern audCurve g_ExhaustHighVolCurve;
extern audCurve g_EngineExhaustPitchCurve;
extern audCurve g_IdleVolCurve;
extern audCurve g_IdlePitchCurve; 
extern u32 g_audVehicleLoopReleaseTime;

#if __BANK
extern CEntity * g_pFocusEntity;
extern bool g_VehicleAudioAllowEntityFocus;
#endif

u32 g_DefaultFadeInTime = 100;

EXT_PF_GROUP(VehicleAudioTimings);
PF_TIMER(VehicleEngineComponentUpdate, VehicleAudioTimings);

// -------------------------------------------------------------------------------
// audVehicleEngineComponent constructor
// -------------------------------------------------------------------------------
audVehicleEngineComponent::audVehicleEngineComponent() :
  m_Parent(NULL)
, m_VehicleEngineAudioSettings(NULL)
, m_EngineSoundType(ENGINE_SOUND_TYPE_MAX)
, m_LoopLow(NULL)
, m_LoopHigh(NULL)
, m_LoopIdle(NULL)
, m_VolumeScale(1.0f)
, m_LowLoopVolume(0.0f)
, m_HighLoopVolume(0.0f)
, m_IdleLoopVolume(0.0f)
, m_FadeInRequired(g_DefaultFadeInTime)
, m_HasPrepared(false)
, m_HasPlayed(false)
{
}

// -------------------------------------------------------------------------------
// audVehicleEngineComponent Init
// -------------------------------------------------------------------------------
void audVehicleEngineComponent::Init(audCarAudioEntity* parent, EngineSoundType soundType)
{
	naAssertf(parent, "Initialising vehicle transmission without a valid vehicle");
	m_Parent = parent;
	m_EngineSoundType = soundType;

	if( m_Parent )
	{
		m_VehicleEngineAudioSettings = m_Parent->GetVehicleEngineAudioSettings();
	}

	m_lowVolSmoother.Init(0.01f);
}

// -------------------------------------------------------------------------------
// audVehicleEngineComponent Prepare
// -------------------------------------------------------------------------------
bool audVehicleEngineComponent::Prepare()
{
	if( !m_HasPrepared )
	{
		if(m_LoopLow && m_LoopHigh && m_LoopIdle)
		{
			audWaveSlot* waveSlot = m_Parent->GetWaveSlot();

			if(waveSlot && !waveSlot->GetIsLoading())
			{
				if( m_LoopLow->Prepare(m_Parent->GetWaveSlot()) == AUD_PREPARED &&
					m_LoopHigh->Prepare(m_Parent->GetWaveSlot()) == AUD_PREPARED &&
					m_LoopIdle->Prepare(m_Parent->GetWaveSlot()) == AUD_PREPARED )
				{
					m_HasPrepared = true;
				}
			}
		}
	}

	return m_HasPrepared;
}

// -------------------------------------------------------------------------------
// audVehicleEngineComponent Play
// -------------------------------------------------------------------------------
void audVehicleEngineComponent::Play()
{
	if(m_LoopLow->GetPlayState() != AUD_SOUND_PLAYING)
	{
		m_LoopLow->SetClientVariable(m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE? (u32)AUD_VEHICLE_SOUND_ENGINE : (u32)AUD_VEHICLE_SOUND_EXHAUST);
		m_LoopLow->SetReleaseTime(g_audVehicleLoopReleaseTime);
		m_LoopLow->Play();
	}

	if(m_LoopHigh->GetPlayState() != AUD_SOUND_PLAYING)
	{
		m_LoopHigh->SetClientVariable(m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE? (u32)AUD_VEHICLE_SOUND_ENGINE : (u32)AUD_VEHICLE_SOUND_EXHAUST);
		m_LoopHigh->SetReleaseTime(g_audVehicleLoopReleaseTime);
		m_LoopHigh->Play();
	}

	if(m_LoopIdle->GetPlayState() != AUD_SOUND_PLAYING)
	{
		m_LoopIdle->SetClientVariable(m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE? (u32)AUD_VEHICLE_SOUND_ENGINE : (u32)AUD_VEHICLE_SOUND_EXHAUST);
		m_LoopIdle->SetReleaseTime(g_audVehicleLoopReleaseTime);
		m_LoopIdle->Play();
	}
	
	m_HasPlayed = true;
}

// -------------------------------------------------------------------------------
// audVehicleEngineComponent Stop
// -------------------------------------------------------------------------------
void audVehicleEngineComponent::Stop(s32 releaseTime)
{
	if(m_LoopLow)
	{
		m_LoopLow->SetReleaseTime(releaseTime);
	}

	if(m_LoopHigh)
	{
		m_LoopHigh->SetReleaseTime(releaseTime);
	}

	if(m_LoopIdle)
	{
		m_LoopIdle->SetReleaseTime(releaseTime);
	}

	m_Parent->StopAndForgetSounds(m_LoopLow,m_LoopHigh,m_LoopIdle);
	m_HasPlayed = false;
	m_HasPrepared = false;
	m_FadeInRequired = g_DefaultFadeInTime;
}

// -------------------------------------------------------------------------------
// audVehicleEngineComponent Start
// -------------------------------------------------------------------------------
void audVehicleEngineComponent::Start()
{
	naAssertf(m_VehicleEngineAudioSettings, "Car audio settings must be valid, about to access a null ptr...");
	audAssert(m_Parent->GetEngineEffectRoute() >= 0 && m_Parent->GetExhaustEffectRoute() >= 0);

	if(!m_Parent->GetDynamicWaveSlot() || !m_Parent->GetDynamicWaveSlot()->IsLoaded())
	{
		return;
	}

	// pick a random start offset to reduce the chance of multiple vehicles of the same type spawned on the same
	// frame by script sounding weird when idling next to each other, eg the taxi rank.
	u32 startOffset = audEngineUtil::GetRandomNumberInRange(0,99);

	audSoundInitParams initParams;

	initParams.Category = g_EngineLoudCategory;
	initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
	initParams.UpdateEntity = true;
	initParams.StartOffset = startOffset;
	initParams.IsStartOffsetPercentage = true; 
	initParams.AttackTime = m_FadeInRequired;

	if( m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE )
	{
		Assign(initParams.EffectRoute, m_Parent->GetEngineEffectRoute());

		if(!m_LoopLow)
		{
			m_Parent->CreateSound_PersistentReference(m_VehicleEngineAudioSettings->LowEngineLoop, &m_LoopLow, &initParams);
		}
		
		if(!m_LoopHigh)
		{
			m_Parent->CreateSound_PersistentReference(m_VehicleEngineAudioSettings->HighEngineLoop, &m_LoopHigh, &initParams);
		}
		
		if(!m_LoopIdle)
		{
			m_Parent->CreateSound_PersistentReference(m_VehicleEngineAudioSettings->IdleEngineSimpleLoop, &m_LoopIdle, &initParams);
		}
	}
	else
	{
		Assign(initParams.EffectRoute, m_Parent->GetExhaustEffectRoute());

		if(!m_LoopLow)
		{
			m_Parent->CreateSound_PersistentReference(m_VehicleEngineAudioSettings->LowExhaustLoop, &m_LoopLow, &initParams);
		}
		
		if(!m_LoopHigh)
		{
			m_Parent->CreateSound_PersistentReference(m_VehicleEngineAudioSettings->HighExhaustLoop, &m_LoopHigh, &initParams);
		}
		
		if(!m_LoopIdle)
		{
			m_Parent->CreateSound_PersistentReference(m_VehicleEngineAudioSettings->IdleExhaustSimpleLoop, &m_LoopIdle, &initParams);
		}
	}

	if(m_LoopLow && m_LoopHigh && m_LoopIdle)
	{
		if( m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE )
		{
			m_LoopLow->SetClientVariable((u32)AUD_VEHICLE_SOUND_ENGINE);
			m_LoopHigh->SetClientVariable((u32)AUD_VEHICLE_SOUND_ENGINE);
			m_LoopIdle->SetClientVariable((u32)AUD_VEHICLE_SOUND_ENGINE);
		}
		else
		{
			m_LoopLow->SetClientVariable((u32)AUD_VEHICLE_SOUND_EXHAUST);
			m_LoopHigh->SetClientVariable((u32)AUD_VEHICLE_SOUND_EXHAUST);
			m_LoopIdle->SetClientVariable((u32)AUD_VEHICLE_SOUND_EXHAUST);
		}

		m_LoopLow->SetReleaseTime(g_audVehicleLoopReleaseTime);
		m_LoopHigh->SetReleaseTime(g_audVehicleLoopReleaseTime);
		m_LoopIdle->SetReleaseTime(g_audVehicleLoopReleaseTime);

		m_LoopLow->SetRequestedFrequencySmoothingRate(m_Parent->GetFrequencySmoothingRate());
		m_LoopHigh->SetRequestedFrequencySmoothingRate(m_Parent->GetFrequencySmoothingRate());
		m_LoopIdle->SetRequestedFrequencySmoothingRate(m_Parent->GetFrequencySmoothingRate());
	}
}

// -------------------------------------------------------------------------------
// audVehicleEngineComponent Update
// -------------------------------------------------------------------------------
void audVehicleEngineComponent::Update(audVehicleVariables* state)
{
	PF_FUNC(VehicleEngineComponentUpdate);

	if(!AreSoundsValid())
	{
		return;
	}

	m_IdleLoopVolume = audDriverUtil::ComputeDbVolumeFromLinear(g_IdleVolCurve.CalculateValue(state->revs) * m_VolumeScale);
	s32 idlePitchCents = (s32)(g_IdlePitchCurve.CalculateRescaledValue((f32)m_VehicleEngineAudioSettings->IdleMinPitch, (f32)m_VehicleEngineAudioSettings->IdleMaxPitch, 0.0f,1.0f,state->revs));
	const f32 idleFreqRatio = audDriverUtil::ConvertPitchToRatio(idlePitchCents + m_Parent->GetEnginePitchOffset());

	f32 minRatio = audDriverUtil::ConvertPitchToRatio(m_VehicleEngineAudioSettings->MinPitch);
	f32 maxRatio = audDriverUtil::ConvertPitchToRatio(m_VehicleEngineAudioSettings->MaxPitch);
	f32 pitchRange = maxRatio - minRatio;

	if(m_Parent->GetVehicle()->m_Transmission.IsEngineUnderLoad() && !m_Parent->GetVehicle()->GetHandBrake())
	{
		pitchRange *= (1.0f - (0.5f * m_Parent->GetVehicleEngine()->GetUnderLoadRatio()));
	}

	state->pitchRatio = (minRatio + (g_EngineExhaustPitchCurve.CalculateValue(state->revs) * (pitchRange))) * audDriverUtil::ConvertPitchToRatio(m_Parent->GetEnginePitchOffset());
	const s32 pitch = audDriverUtil::ConvertRatioToPitch(state->pitchRatio);

	if( m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE )
	{
		m_LowLoopVolume = state->engineThrottleAtten + audDriverUtil::ComputeDbVolumeFromLinear(g_EngineLowVolCurve.CalculateValue(state->revs) * m_VolumeScale);
		m_LowLoopVolume = m_lowVolSmoother.CalculateValue(m_LowLoopVolume, state->timeInMs);
		m_HighLoopVolume = state->engineThrottleAtten + audDriverUtil::ComputeDbVolumeFromLinear(g_EngineHighVolCurve.CalculateValue(state->revs) * m_VolumeScale);
	}
	else
	{
		m_LowLoopVolume = state->exhaustThrottleAtten + audDriverUtil::ComputeDbVolumeFromLinear(g_ExhaustLowVolCurve.CalculateValue(state->revs) * m_VolumeScale);
		m_HighLoopVolume = state->exhaustThrottleAtten + audDriverUtil::ComputeDbVolumeFromLinear(g_ExhaustHighVolCurve.CalculateValue(state->revs) * m_VolumeScale);
	}

	m_LoopLow->SetRequestedVolume(m_LowLoopVolume);
	m_LoopLow->SetRequestedPitch(pitch);
	m_LoopLow->SetRequestedFrequencySmoothingRate(m_Parent->GetFrequencySmoothingRate());

	m_LoopHigh->SetRequestedVolume(m_HighLoopVolume);
	m_LoopHigh->SetRequestedPitch(pitch);
	m_LoopHigh->SetRequestedFrequencySmoothingRate(m_Parent->GetFrequencySmoothingRate());

	m_LoopIdle->SetRequestedVolume(m_IdleLoopVolume);
	m_LoopIdle->SetRequestedFrequencyRatio(idleFreqRatio);
	m_LoopIdle->SetRequestedFrequencySmoothingRate(m_Parent->GetFrequencySmoothingRate());

#if __BANK
	if(g_VehicleAudioAllowEntityFocus)
	{
		if(m_Parent->GetVehicle() != g_pFocusEntity)
		{
			m_LoopLow->SetRequestedVolume(-100);
			m_LoopHigh->SetRequestedVolume(-100);
			m_LoopIdle->SetRequestedVolume(-100);
		}
	}
#endif
}
