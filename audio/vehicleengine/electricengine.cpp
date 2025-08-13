#include "electricengine.h"
#include "audio/northaudioengine.h"
#include "audio/caraudioentity.h"

AUDIO_VEHICLES_OPTIMISATIONS()

f32 g_BoostLoopSpinUp = 1.0f / 1000.f;
f32 g_BoostLoopSpinDown = 3.0f / 1000.f;

f32 g_ElectricEngineBoostVolScale = 1.0f;
f32 g_ElectricEngineSpeedVolScale = 1.0f;
f32 g_ElectricEngineRevsOffVolScale = 1.0f;
f32 g_ElectricEngineSpeedLoopSpeedBias = 0.75f;
f32 g_ElectricEngineSpeedLoopSpeedApplyScalar = 2.f;

audCurve audVehicleElectricEngine::sm_BoostToVolCurve;
audCurve audVehicleElectricEngine::sm_RevsToBoostCurve;

extern audCategory* g_EngineLoudCategory;
extern audCategory* g_EngineCategory;

// -------------------------------------------------------------------------------
// audVehicleElectricEngine constructor
// -------------------------------------------------------------------------------
audVehicleElectricEngine::audVehicleElectricEngine() :
    m_Parent(NULL)
  , m_ElectricEngineAudioSettings(NULL)
  , m_SpeedLoop(NULL)
  , m_BoostLoop(NULL)
  , m_RevsOffLoop(NULL)
  , m_VolumeScale(1.0f)
{
}

// -------------------------------------------------------------------------------
// audVehicleElectricEngine InitClass
// -------------------------------------------------------------------------------
void audVehicleElectricEngine::InitClass()
{
	VerifyExists(sm_RevsToBoostCurve.Init(ATSTRINGHASH("ELECTRIC_ENGINE_REVS_TO_BOOST", 0x17B36908)), "Invalid electric revs to boost curve");
	VerifyExists(sm_BoostToVolCurve.Init(ATSTRINGHASH("ELECTRIC_ENGINE_BOOST_TO_VOL", 0xA284299F)), "Invalid electric boost to vol curve");
}

// -------------------------------------------------------------------------------
// audVehicleElectricEngine Init
// -------------------------------------------------------------------------------
void audVehicleElectricEngine::Init(audCarAudioEntity* parent)
{
	naAssertf(parent, "Initialising vehicle transmission without a valid vehicle");

	m_Parent = parent;
	m_ElectricEngineAudioSettings = audNorthAudioEngine::GetObject<ElectricEngineAudioSettings>(parent->GetCarAudioSettings()->ElectricEngine);
	m_RevsOffLoopVolSmoother.Init(0.05f,0.0005f,true);
	m_SpeedLoopSpeedVolSmoother.Init(0.0005f,0.0005f);
	m_SpeedLoopThrottleVolSmoother.Init(0.0005f,0.0005f);

	if(m_ElectricEngineAudioSettings)
	{
		m_BoostLoopApplySmoother.Init(g_BoostLoopSpinUp*(m_ElectricEngineAudioSettings->BoostLoop_SpinupSpeed*0.01f), g_BoostLoopSpinDown*(m_ElectricEngineAudioSettings->BoostLoop_SpinupSpeed*0.01f));
	}
}

// -------------------------------------------------------------------------------
// audVehicleElectricEngine update
// -------------------------------------------------------------------------------
void audVehicleElectricEngine::Update(audVehicleVariables* state)
{
	if(!m_ElectricEngineAudioSettings)
	{
		return;
	}

	UpdateSpeedLoop(state);
	UpdateBoostLoop(state);
	UpdateRevsOffLoop(state);

#if __BANK
	// Update to catch an RAVE changes
	m_BoostLoopApplySmoother.SetRates(g_BoostLoopSpinUp*(m_ElectricEngineAudioSettings->BoostLoop_SpinupSpeed*0.01f), g_BoostLoopSpinDown*(m_ElectricEngineAudioSettings->BoostLoop_SpinupSpeed*0.01f));
#endif
}

// -------------------------------------------------------------------------------
// audVehicleElectricEngine UpdateSpeedLoop
// -------------------------------------------------------------------------------
void audVehicleElectricEngine::UpdateSpeedLoop(audVehicleVariables* state)
{
	bool linkVolumeToSpeed = false;

	if(m_Parent->GetVehicle()->GetModelNameHash() == ATSTRINGHASH("Omnisegt", 0xE1E2E6D7))
	{
		linkVolumeToSpeed = true;
	}

	f32 linVol = 0.f;
	f32 throttle = state->throttle;

	if(linkVolumeToSpeed)
	{
		f32 totalVolume = m_ElectricEngineAudioSettings->SpeedLoop_ThrottleVol * 0.01f;
		f32 speedVolume = totalVolume * g_ElectricEngineSpeedLoopSpeedBias;
		f32 throttleVolume = totalVolume * (1.f - g_ElectricEngineSpeedLoopSpeedBias);

		f32 dbVol = m_ElectricEngineAudioSettings->MasterVolume/100.0f;
		f32 throttleVolLinear = audDriverUtil::ComputeLinearVolumeFromDb(throttleVolume);
		f32 throttleVolLinearScaled = 1.f + ((1.f - throttleVolLinear) * m_SpeedLoopSpeedVolSmoother.CalculateValue(throttle, state->timeInMs));
		dbVol += audDriverUtil::ComputeDbVolumeFromLinear(throttleVolLinearScaled);
		linVol = audDriverUtil::ComputeLinearVolumeFromDb(dbVol) * (audDriverUtil::ComputeLinearVolumeFromDb(speedVolume) * Min(Max(m_Parent->GetBurnoutRatio(), state->fwdSpeedRatio * g_ElectricEngineSpeedLoopSpeedApplyScalar), 1.f));

		throttle = state->throttleInput;
		m_SpeedLoopThrottleVolSmoother.SetRates(0.0005f,0.001f);
	}

	linVol = Max(linVol, audDriverUtil::ComputeLinearVolumeFromDb((m_ElectricEngineAudioSettings->MasterVolume/100.0f) + (m_ElectricEngineAudioSettings->SpeedLoop_ThrottleVol*0.01f)) * m_SpeedLoopThrottleVolSmoother.CalculateValue(throttle, state->timeInMs));

	const f32 pitchRatio = audDriverUtil::ConvertPitchToRatio(m_ElectricEngineAudioSettings->SpeedLoop_MinPitch + (s32)((m_ElectricEngineAudioSettings->SpeedLoop_MaxPitch - m_ElectricEngineAudioSettings->SpeedLoop_MinPitch) * state->fwdSpeedRatio));
	m_Parent->UpdateLoopWithLinearVolumeAndPitchRatio(&m_SpeedLoop, m_ElectricEngineAudioSettings->SpeedLoop, linVol * g_ElectricEngineSpeedVolScale * m_VolumeScale, pitchRatio, m_Parent->GetEnvironmentGroup(), g_EngineCategory, true, true);

	if(m_SpeedLoop && m_SpeedLoop->GetPlayState() == AUD_SOUND_PLAYING)
	{
		m_SpeedLoop->FindAndSetVariableValue(ATSTRINGHASH("RPM", 0x5B924509), state->revs);
		m_SpeedLoop->FindAndSetVariableValue(ATSTRINGHASH("Gear", 0xBE74155D), (f32)state->gear);
	}
}

// -------------------------------------------------------------------------------
// audVehicleElectricEngine UpdateBoostLoop
// -------------------------------------------------------------------------------
void audVehicleElectricEngine::UpdateBoostLoop(audVehicleVariables* state)
{
	const f32 requestedBoost = state->gear > 0 && state->wheelSpeedAbs > 1.0f? sm_RevsToBoostCurve.CalculateValue(state->revs) * state->throttle : 0.0f;
	const f32 boost = m_BoostLoopApplySmoother.CalculateValue(requestedBoost, state->timeInMs);
	const f32 linVol = sm_BoostToVolCurve.CalculateValue(boost) * audDriverUtil::ComputeLinearVolumeFromDb(m_ElectricEngineAudioSettings->MasterVolume/100.0f) * audDriverUtil::ComputeLinearVolumeFromDb(m_ElectricEngineAudioSettings->BoostLoop_Vol/100.0f);
	const f32 pitchRatio = audDriverUtil::ConvertPitchToRatio(m_ElectricEngineAudioSettings->BoostLoop_MinPitch + (s32)((m_ElectricEngineAudioSettings->BoostLoop_MaxPitch - m_ElectricEngineAudioSettings->BoostLoop_MinPitch) * boost));
	m_Parent->UpdateLoopWithLinearVolumeAndPitchRatio(&m_BoostLoop, m_ElectricEngineAudioSettings->BoostLoop, linVol * g_ElectricEngineBoostVolScale * m_VolumeScale, pitchRatio, m_Parent->GetEnvironmentGroup(), g_EngineCategory, true, true);
}

// -------------------------------------------------------------------------------
// audVehicleElectricEngine stop
// -------------------------------------------------------------------------------
void audVehicleElectricEngine::UpdateRevsOffLoop(audVehicleVariables* state)
{
	bool revsOffActive = false;

	if(state->revs < m_Parent->GetVehicleEngine()->GetEngineLastRevs() && 
	   state->throttle <= 0.1f && 
	   state->fwdSpeedAbs > 1.0f &&
	   state->numWheelsTouchingFactor == 1.0f &&
	   m_Parent->GetVehicleEngine()->GetState() == audVehicleEngine::AUD_ENGINE_ON)
	{
		revsOffActive = true;
	}

	f32 revsOffApply = m_RevsOffLoopVolSmoother.CalculateValue(revsOffActive? 1.0f : 0.0f, state->timeInMs);
	f32 revsOffVolLin = revsOffApply * audDriverUtil::ComputeLinearVolumeFromDb(m_ElectricEngineAudioSettings->MasterVolume/100.0f) * audDriverUtil::ComputeLinearVolumeFromDb(m_ElectricEngineAudioSettings->RevsOffLoop_Vol/100.0f);
	const f32 pitchRatio = audDriverUtil::ConvertPitchToRatio(m_ElectricEngineAudioSettings->RevsOffLoop_MinPitch + (s32)((m_ElectricEngineAudioSettings->RevsOffLoop_MaxPitch - m_ElectricEngineAudioSettings->RevsOffLoop_MinPitch) * state->fwdSpeedRatio));
	m_Parent->UpdateLoopWithLinearVolumeAndPitchRatio(&m_RevsOffLoop, m_ElectricEngineAudioSettings->RevsOffLoop, revsOffVolLin * g_ElectricEngineRevsOffVolScale * m_VolumeScale, pitchRatio, m_Parent->GetEnvironmentGroup(), g_EngineLoudCategory, false, true, state->environmentalLoudness);
}

// -------------------------------------------------------------------------------
// audVehicleElectricEngine stop
// -------------------------------------------------------------------------------
void audVehicleElectricEngine::Stop()
{
	m_Parent->StopAndForgetSounds(m_SpeedLoop, m_BoostLoop, m_RevsOffLoop);
}
