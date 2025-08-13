#include "vehiclereflectionsaudioentity.h"
#include "audiosynth/synthcore.h"
#include "debug/DebugScene.h"
#include "grcore/debugdraw.h"
#include "scene/entity.h"
#include "Peds/PedFactory.h"
#include "audio/vehicleaudioentity.h"
#include "audio/ambience/audambientzone.h"
#include "audio/northaudioengine.h"
#include "audio/scriptaudioentity.h"
#include "vehicles/vehicle.h"
#include "Peds/ped.h"
#include "audioeffecttypes/biquadfiltereffect.h"
#include "audioeffecttypes/variabledelayeffect.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "fwscene/stores/staticboundsstore.h"
#include "camera/CamInterface.h"
#include "camera/cinematic/CinematicDirector.h"

AUDIO_VEHICLES_OPTIMISATIONS()

audVehicleReflectionsEntity g_ReflectionsAudioEntity;
extern f32 g_EngineVolumeTrim;
extern f32 g_ExhaustVolumeTrim;
extern CPlayerSwitchInterface g_PlayerSwitch;
extern f32 g_SpeakerReverbDamping;

f32 g_StereoEffectEngineVolumeTrim = 0.0f;
f32 g_StereoEffectExhaustVolumeTrim = 0.0f;
f32 g_WetReverbForReflections = 0.15f;
f32 g_ReflectionsStepRateSmoothing = 1.0f;
f32 g_MaxProbeDist = 25.0f;
f32 g_MinProbeDist = 0.0f;
f32 g_VehicleReflectionsActivationDist = 25.0f;
u32 g_ReflectionsAttackTime = 300u;
f32 g_RayCastForwardOffset = 5.0f;
f32 g_LeftOscillatorSpeed = 1.23f;
f32 g_RightOscillatorSpeed = 2.14f;
f32 g_LeftOscillatorDist = 0.0f;
f32 g_RightOscillatorDist = 0.4f;
f32 g_ProximityMinDistChangeRate = 1.0f;
f32 g_ProximityMaxProbeDistance = 3.0f;
u32 g_ProximityWhooshMinRepeatTime = 500;
f32 g_ProximityRayCastForwardOffset = 1.5f;
f32 g_StuntTunnelRayCastForwardOffset = 1.5f;
u32 g_StuntTunnelMinWhooshRepeatTimeMs = 1000;
u32 g_StuntBoostIntensityIncreaseDelay = 100;
u32 g_RechargeIntensityIncreaseDelay = 100;
f32 g_StuntTunnelMinWhooshExitDistance = 50.f;
f32 g_StuntTunnelRayCastLength = 50.f;
f32 g_StuntTunnelProbeHitTimeout = 0.2f;
s32 g_StereoLeftPan = 270;
s32 g_StereoRightPan = 90;
f32 g_StereoLeftAttenuation = -6.0f;
f32 g_StereoRightAttenuation = -6.0f;
f32 g_StereoDelayLFOHzLeft = 0.0f;
f32 g_StereoDelayLFOHzRight = 0.0f;
f32 g_StereoDelayLFOMagLeft = 0.0f;
f32 g_StereoDelayLFOMagRight = 0.0f;
f32 g_StereoDelayLFOOffsetLeft = 0.05f;
f32 g_StereoDelayLFOOffsetRight = 0.0f;
bool g_StereoDelayLFOEnabled = true;
bool g_BonnetCamStereoEffectEnabled = true;
bool g_InteriorReflectionsEnabled = true;
bool g_VehicleReflectionsEnabled = false;
bool g_ForceStuntTunnelProbesEnabled = false;
bool g_PointSourceVehicleReflections = false;
bool g_PanInteriorReflectionsWithHeadAngle = false;

audCurve audVehicleReflectionsEntity::sm_ReflectionsVehicleSpeedToVolCurve;
audCurve audVehicleReflectionsEntity::sm_ReflectionsVehicleRevRatioToVolCurve;
audCurve audVehicleReflectionsEntity::sm_ReflectionsVehicleMassToVolCurve;
audCurve audVehicleReflectionsEntity::sm_StaticReflectionsRollOffCurve;
audSoundSet audVehicleReflectionsEntity::sm_CameraSwitchSoundSet;

PF_PAGE(audVehicleReflectionsEntityPage, "audVehicleReflectionsEntity Values");
PF_GROUP(audVehicleReflectionsValues);
PF_LINK(audVehicleReflectionsEntityPage, audVehicleReflectionsValues);
PF_VALUE_FLOAT(LeftProbeDist, audVehicleReflectionsValues);
PF_VALUE_FLOAT(LeftProbeDistChangeRate, audVehicleReflectionsValues);
PF_VALUE_FLOAT(RightProbeDist, audVehicleReflectionsValues);
PF_VALUE_FLOAT(RightProbeDistChangeRate, audVehicleReflectionsValues);

#if __BANK
char g_TestReflectionsName[128]={0};
bool g_TestReflections = false;
extern f32 g_EngineVolumeTrim;
extern f32 g_ExhaustVolumeTrim;
bool g_DebugPrintInteriorTunnels = false;
bool g_DebugPrintInteriorsAll = false;
bool g_ReflectionsDebugDraw = false;
bool g_ReflectionsLocationPickerEnabled = false;
extern CEntity * g_pFocusEntity;
bool g_ForceSwitchProximityWhooshes = false;
#endif

// ----------------------------------------------------------------
// audVehicleReflectionsEntity constructor
// ----------------------------------------------------------------
audVehicleReflectionsEntity::audVehicleReflectionsEntity()
{	
	m_ActiveVehicle = NULL;
	m_ReflectionsTypeLastFrame = AUD_REFLECTIONS_TYPE_MAX;
	m_ProximityWhooshesEnabled = false;
	m_OscillationTimer = 0.0f;
	m_LastStuntTunnelWhooshPosition.Zero();
	m_LastStuntTunnelWhooshWasEnter = false;
}

// ----------------------------------------------------------------
// audVehicleReflectionsEntity destructor
// ----------------------------------------------------------------
audVehicleReflectionsEntity::~audVehicleReflectionsEntity()
{
}

// ----------------------------------------------------------------
// audVehicleReflectionsEntity InitClass
// ----------------------------------------------------------------
void audVehicleReflectionsEntity::InitClass()
{
	// If this fails, we have filter modes in the audio definitions that aren't in the synth, or vice-versa 
	CompileTimeAssert((int)NUM_FILTERMODE == (int)synthBiquadFilter::kNumBiquadModes);
	sm_ReflectionsVehicleSpeedToVolCurve.Init(ATSTRINGHASH("VEHICLE_SPEED_TO_REFLECTIONS_VOL", 0xF5FC74E5));
	sm_ReflectionsVehicleRevRatioToVolCurve.Init(ATSTRINGHASH("VEHICLE_REV_RATIO_TO_REFLECTIONS_VOL", 0xFDF12A3B));
	sm_ReflectionsVehicleMassToVolCurve.Init(ATSTRINGHASH("VEHICLE_MASS_TO_REFLECTIONS_VOL", 0x7FED0285));
	sm_StaticReflectionsRollOffCurve.Init(ATSTRINGHASH("DEFAULT_ROLLOFF", 0x3BD35057));
	sm_CameraSwitchSoundSet.Init(ATSTRINGHASH("CameraSwitchSounds", 0x124A34BF));
}

// ----------------------------------------------------------------
// audVehicleReflectionsEntity Init
// ----------------------------------------------------------------
void audVehicleReflectionsEntity::Init()
{
	naAudioEntity::Init();
	m_HasPlayedInteriorEnter = false;
	m_HasPlayedInteriorExit = false;
	m_IsFocusVehicleInStuntTunnel = false;
	m_LastStuntTunnelWhooshSound = 0u;
	m_LastBonnetCamUpdateTime = 0u;

	RegisterStuntTunnelMaterial("STUNT_RAMP_SURFACE");

	m_ReflectionsSourceSubmix = g_AudioEngine.GetEnvironment().GetReflectionsSourceSubmix();

	for(u32 i = 0; i < AUD_NUM_REFLECTIONS_OUTPUT_SUBMIXES; i++)
	{
		g_AudioEngine.GetEnvironment().FreeReflectionsOutputSubmix(i);
	}

	for(u32 i = 0; i < INTERIOR_REFLECTION_MAX; i++)
	{
		m_InteriorReflections[i].m_ReflectionsEntity = NULL;
		m_InteriorReflections[i].m_ProbeDist = g_MaxProbeDist;
		m_InteriorReflections[i].m_ProbeDesc.SetResultsStructure(&m_InteriorReflections[i].m_ProbeResults);
		m_InteriorReflections[i].m_ReflectionsVoice = NULL;
		m_InteriorReflections[i].m_ProbeHitTimer = 0.0f;
		m_InteriorReflections[i].m_OscillatorDist = i == INTERIOR_REFLECTION_LEFT? g_LeftOscillatorDist : g_RightOscillatorDist;
		m_InteriorReflections[i].m_OscillatorSpeed = i == INTERIOR_REFLECTION_LEFT? g_LeftOscillatorSpeed : g_RightOscillatorSpeed;
		m_InteriorReflections[i].m_FirstUpdate = true;
		m_InteriorReflections[i].m_ProbeHit = false;
		m_InteriorReflections[i].m_ReflectionsSubmixIndex = -1; 
		m_InteriorReflections[i].m_ReflectionsSubmix = NULL;
		m_InteriorReflections[i].m_PrevTunnelDirection.Zero();
		m_InteriorReflections[i].m_ProbeDistSmoother.Init(1.0f, 1.0f);
	}

	for(u32 i = 0; i < kMaxVehicleReflections; i++)
	{
		m_VehicleReflections[i].m_ReflectionsEntity = NULL;
		m_VehicleReflections[i].m_ReflectionsVoice = NULL;
		m_VehicleReflections[i].m_FirstUpdate = true;
		m_VehicleReflections[i].m_ReflectionsSubmixIndex = -1; 
		m_VehicleReflections[i].m_ReflectionsSubmix = NULL;
		m_VehicleReflections[i].m_DistSmoother.Init(1.0f, 1.0f);
	}

	for(u32 i = 0; i < 2; i++)
	{
		m_ProximityWhooshes[i].m_ProbeDesc.SetResultsStructure(&m_ProximityWhooshes[i].m_ProbeResults);
		m_ProximityWhooshes[i].m_ProbeDist = 0.0f;
		m_ProximityWhooshes[i].m_SmoothedProbeDist = 0.0f;
		m_ProximityWhooshes[i].m_LastWhooshTime = 0;
	}

	for(u32 i = 0; i < kMaxStuntTunnelProbes; i++)
	{
		m_StuntTunnelProbes[i].m_ProbeDesc.SetResultsStructure(&m_StuntTunnelProbes[i].m_ProbeResults);
		m_StuntTunnelProbes[i].m_ProbeHit = false;
		m_StuntTunnelProbes[i].m_ProbeLastHitTimer = 10.0f;
	}

	// Game will start off with all engine/exhaust submixes connected to the reflections, so unhook them to begin with
	for(s32 loop = 0; loop < EFFECT_ROUTE_VEHICLE_ENGINE_MAX - EFFECT_ROUTE_VEHICLE_ENGINE_MIN + 1; loop++)
	{
		audMixerSubmix* thisSubmix = g_AudioEngine.GetEnvironment().GetVehicleEngineSubmix(loop);
		thisSubmix->DisconnectOutput(m_ReflectionsSourceSubmix->GetSubmixId(), false);
	}

	for(s32 loop = 0; loop < EFFECT_ROUTE_VEHICLE_EXHAUST_MAX - EFFECT_ROUTE_VEHICLE_EXHAUST_MIN + 1; loop++)
	{
		audMixerSubmix* thisSubmix = g_AudioEngine.GetEnvironment().GetVehicleExhaustSubmix(loop);
		thisSubmix->DisconnectOutput(m_ReflectionsSourceSubmix->GetSubmixId(), false);
	}
}

// ----------------------------------------------------------------
// audVehicleReflectionsEntity Shutdown
// ----------------------------------------------------------------
void audVehicleReflectionsEntity::Shutdown()
{
	DetachFromAll();
	m_ReflectionsPoints.clear();
	audEntity::Shutdown();
}

// ----------------------------------------------------------------
// audVehicleReflectionsEntity SetZoneReflectionPosition
// ----------------------------------------------------------------
void audVehicleReflectionsEntity::AddAmbientZoneReflectionPosition(const Vector3& pos, audAmbientZone* ambientZone)
{
	for(u32 loop = 0; loop < m_ReflectionsPoints.GetCount(); loop++)
	{
		if(m_ReflectionsPoints[loop].type == audReflectionPoint::kReflectionPointType_AmbientZone && 
		   m_ReflectionsPoints[loop].parentZone == ambientZone)
		{
			m_ReflectionsPoints[loop].position = pos;
			return;
		}
	}

	audReflectionPoint newReflectionPoint;
	newReflectionPoint.position = pos;
	newReflectionPoint.type = audReflectionPoint::kReflectionPointType_AmbientZone;
	newReflectionPoint.parentZone = ambientZone;
	m_ReflectionsPoints.Push(newReflectionPoint);
}

// ----------------------------------------------------------------
// audVehicleReflectionsEntity AddDebugReflectionPosition
// ----------------------------------------------------------------
void audVehicleReflectionsEntity::AddDebugReflectionPosition(const Vector3& pos)
{
	for(u32 loop = 0; loop < m_ReflectionsPoints.GetCount(); loop++)
	{
		if(m_ReflectionsPoints[loop].type == audReflectionPoint::kReflectionPointType_Debug)
		{
			m_ReflectionsPoints[loop].position = pos;
			return;
		}
	}

	audReflectionPoint newReflectionPoint;
	newReflectionPoint.position = pos;
	newReflectionPoint.type = audReflectionPoint::kReflectionPointType_Debug;
	m_ReflectionsPoints.Push(newReflectionPoint);
}

// ----------------------------------------------------------------
// audVehicleReflectionsEntity RemoveAmbientZoneReflectionPosition
// ----------------------------------------------------------------
void audVehicleReflectionsEntity::RemoveAmbientZoneReflectionPosition(audAmbientZone* ambientZone)
{
	for(u32 loop = 0; loop < m_ReflectionsPoints.GetCount(); loop++)
	{
		if(m_ReflectionsPoints[loop].parentZone == ambientZone)
		{
			m_ReflectionsPoints.Delete(loop);
			loop--;
		}
	}
}

// ----------------------------------------------------------------
// audVehicleReflectionsEntity DetachFromAll
// ----------------------------------------------------------------
void audVehicleReflectionsEntity::DetachFromAll()
{
	for(s32 loop = 0; loop < m_AttachedEngineSubmixes.GetCount(); loop++)
	{
		audMixerSubmix* thisSubmix = g_AudioEngine.GetEnvironment().GetVehicleEngineSubmix(m_AttachedEngineSubmixes[loop]);
		thisSubmix->DisconnectOutput(m_ReflectionsSourceSubmix->GetSubmixId(), false);
		m_AttachedEngineSubmixes.Delete(loop);
		loop--;
	}

	for(s32 loop = 0; loop < m_AttachedExhaustSubmixes.GetCount(); loop++)
	{
		audMixerSubmix* thisSubmix = g_AudioEngine.GetEnvironment().GetVehicleExhaustSubmix(m_AttachedExhaustSubmixes[loop]);
		thisSubmix->DisconnectOutput(m_ReflectionsSourceSubmix->GetSubmixId(), false);
		m_AttachedExhaustSubmixes.Delete(loop);
		loop--;
	}

	for(u32 i = 0; i < INTERIOR_REFLECTION_MAX; i++)
	{
		if(m_InteriorReflections[i].m_ReflectionsSubmixIndex >= 0)
		{
			g_AudioEngine.GetEnvironment().FreeReflectionsOutputSubmix(m_InteriorReflections[i].m_ReflectionsSubmixIndex);
		}
		
		StopAndForgetSounds(m_InteriorReflections[i].m_ReflectionsVoice);
	}
}

// ----------------------------------------------------------------
// audVehicleReflectionsEntity DetachFromVehicle
// ----------------------------------------------------------------
void audVehicleReflectionsEntity::DetachFromVehicle(audVehicleAudioEntity* vehAudioEntity)
{
	if(vehAudioEntity->HasValidEngineEffectRoute() && vehAudioEntity->HasValidExhaustEffectRoute())
	{
		s32 engineEffectRoute = vehAudioEntity->GetEngineEffectSubmixIndex();
		s32 exhaustEffectRoute = vehAudioEntity->GetExhaustEffectSubmixIndex();

		for(s32 loop = 0; loop < m_AttachedEngineSubmixes.GetCount(); loop++)
		{
			if(m_AttachedEngineSubmixes[loop] == engineEffectRoute)
			{
				audMixerSubmix* thisSubmix = g_AudioEngine.GetEnvironment().GetVehicleEngineSubmix(m_AttachedEngineSubmixes[loop]);
				thisSubmix->DisconnectOutput(m_ReflectionsSourceSubmix->GetSubmixId(), false);
				m_AttachedEngineSubmixes.Delete(loop);
				loop--;
			}
		}

		for(s32 loop = 0; loop < m_AttachedExhaustSubmixes.GetCount(); loop++)
		{
			if(m_AttachedExhaustSubmixes[loop] == exhaustEffectRoute)
			{
				audMixerSubmix* thisSubmix = g_AudioEngine.GetEnvironment().GetVehicleExhaustSubmix(m_AttachedExhaustSubmixes[loop]);
				thisSubmix->DisconnectOutput(m_ReflectionsSourceSubmix->GetSubmixId(), false);
				m_AttachedExhaustSubmixes.Delete(loop);
				loop--;
			}
		}
	}

	if(vehAudioEntity == m_ActiveVehicle)
	{
		m_ActiveVehicle = NULL;

		for(u32 i = 0; i < INTERIOR_REFLECTION_MAX; i++)
		{
			if(m_InteriorReflections[i].m_ReflectionsSubmixIndex >= 0)
			{
				g_AudioEngine.GetEnvironment().FreeReflectionsOutputSubmix(m_InteriorReflections[i].m_ReflectionsSubmixIndex);
			}

			StopAndForgetSounds(m_InteriorReflections[i].m_ReflectionsVoice);
		}
	}
}

// ----------------------------------------------------------------
// audVehicleReflectionsEntity AttachToVehicle
// ----------------------------------------------------------------
void audVehicleReflectionsEntity::AttachToVehicle(audVehicleAudioEntity* vehAudioEntity)
{
	if(vehAudioEntity->HasValidEngineEffectRoute() && vehAudioEntity->HasValidExhaustEffectRoute() && vehAudioEntity != m_ActiveVehicle)
	{
		s32 engineEffectRoute = vehAudioEntity->GetEngineEffectSubmixIndex();
		s32 exhaustEffectRoute = vehAudioEntity->GetExhaustEffectSubmixIndex();

		bool engineEffectRouteValid = false;
		bool exhaustEffectRouteValid = false;

		for(s32 loop = 0; loop < m_AttachedEngineSubmixes.GetCount(); loop++)
		{
			if(m_AttachedEngineSubmixes[loop] == engineEffectRoute)
			{
				engineEffectRouteValid = true;
			}
			else
			{
				audMixerSubmix* thisSubmix = g_AudioEngine.GetEnvironment().GetVehicleEngineSubmix(m_AttachedEngineSubmixes[loop]);
				thisSubmix->DisconnectOutput(m_ReflectionsSourceSubmix->GetSubmixId(), false);
				m_AttachedEngineSubmixes.Delete(loop);
				loop--;
			}
		}

		for(s32 loop = 0; loop < m_AttachedExhaustSubmixes.GetCount(); loop++)
		{
			if(m_AttachedExhaustSubmixes[loop] == exhaustEffectRoute)
			{
				exhaustEffectRouteValid = true;
			}
			else
			{
				audMixerSubmix* thisSubmix = g_AudioEngine.GetEnvironment().GetVehicleExhaustSubmix(m_AttachedExhaustSubmixes[loop]);
				thisSubmix->DisconnectOutput(m_ReflectionsSourceSubmix->GetSubmixId(), false);
				m_AttachedExhaustSubmixes.Delete(loop);
				loop--;
			}
		}

		ALIGNAS(16) f32 outputVolumes[g_MaxOutputChannels] ;
		sysMemSet(&outputVolumes[0], 0, sizeof(outputVolumes));
		static const f32 clippingVolumeFix = 0.707946f;
		outputVolumes[0] = clippingVolumeFix;

		if(!engineEffectRouteValid)
		{
			audMixerSubmix* engineSubmix = vehAudioEntity->GetEngineEffectSubmix();
			engineSubmix->AddOutput(m_ReflectionsSourceSubmix->GetSubmixId(), true, false);
			engineSubmix->SetOutputVolumes(4, outputVolumes);
			m_AttachedEngineSubmixes.Push(engineEffectRoute);
		}

		if(!exhaustEffectRouteValid)
		{
			audMixerSubmix* exhaustSubmix = vehAudioEntity->GetExhaustEffectSubmix();
			exhaustSubmix->AddOutput(m_ReflectionsSourceSubmix->GetSubmixId(), true, false);
			exhaustSubmix->SetOutputVolumes(4, outputVolumes);
			m_AttachedExhaustSubmixes.Push(exhaustEffectRoute);
		}

		m_ActiveVehicle = vehAudioEntity;
	}
}

// ----------------------------------------------------------------
// ReflectionsPoints comparison
// ----------------------------------------------------------------
int CbCompareReflectionPoints(const audVehicleReflectionsEntity::audReflectionPoint* pA, const audVehicleReflectionsEntity::audReflectionPoint* pB)
{
	Vector3 sourcePos = g_ReflectionsAudioEntity.GetCurrentSourcePosition();
	f32 pADist2 = pA->position.Dist2(sourcePos);
	f32 pBDist2 = pB->position.Dist2(sourcePos);

	// Currently just sort on distance, should probably do it on estimated volume too
	return pADist2 < pBDist2 ? -1 : (pADist2 == pBDist2 ? 0 : 1);
}

// ----------------------------------------------------------------
// audVehicleReflectionsEntity Update
// ----------------------------------------------------------------
void audVehicleReflectionsEntity::Update()
{
	eReflectionsType reflectionsType = AUD_REFLECTIONS_TYPE_MAX;

	m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::StepRateSmoothing, g_ReflectionsStepRateSmoothing);
	m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::TimeStep, fwTimer::GetTimeStep());
	bool stuntTunnelStateUpdated = false;

	if(g_BonnetCamStereoEffectEnabled && audNorthAudioEngine::IsRenderingFirstPersonVehicleCam())
	{
		UpdateBonnetCamStereoEffect();
		reflectionsType = AUD_REFLECTIONS_TYPE_STEREO_EFFECT;
		m_LastBonnetCamUpdateTime = fwTimer::GetTimeInMilliseconds();
	}
	else if(g_InteriorReflectionsEnabled)
	{
		if(m_ReflectionsTypeLastFrame == AUD_REFLECTIONS_TYPE_STEREO_EFFECT)
		{
			m_InteriorReflections[0].m_ProbeHitTimer = 1.f;
			m_InteriorReflections[1].m_ProbeHitTimer = 1.f;
		}

		if(BANK_ONLY(!g_TestReflections && )SUPERCONDUCTOR.GetVehicleConductor().JumpConductorActive() && NetworkInterface::IsGameInProgress() BANK_ONLY(|| g_ForceStuntTunnelProbesEnabled))
		{			
			UpdateStuntTunnelReflections();
			stuntTunnelStateUpdated = true;
		}
		else
		{
			UpdateInteriorReflections();
		}
		
		reflectionsType = AUD_REFLECTIONS_TYPE_INTERIOR;
	}

	if(m_ReflectionsSourceSubmix && m_ReflectionsTypeLastFrame != reflectionsType)
	{
		for(u32 i = 0; i < g_MaxOutputChannels; i++)
		{
			m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::ResetDelay + i, 1);
		}
	}

	if(m_ProximityWhooshesEnabled BANK_ONLY(|| g_ForceSwitchProximityWhooshes))
	{
		UpdateProximityWhooshes(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix(), NULL);
	}

	if(g_VehicleReflectionsEnabled)
	{
		UpdateVehicleReflections();
	}

	// Stop any DSP voices that we're not using any more
	for(u32 loop = 0; loop < AUD_NUM_REFLECTIONS_OUTPUT_SUBMIXES; loop++)
	{
		bool bypass = g_AudioEngine.GetEnvironment().IsReflectionsOutputSubmixFree(loop);
		m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::BypassChannel + loop, bypass);
	}

	// Make sure we clear this flag if we didn't update the stunt tunnel probes (eg. they have been disabled)
	if(!stuntTunnelStateUpdated)
	{
		m_IsFocusVehicleInStuntTunnel = false;
	}

#if __BANK
	UpdateDebug();
#endif

	m_ReflectionsTypeLastFrame = reflectionsType;
}

// ----------------------------------------------------------------
// audVehicleReflectionsEntity AreInteriorReflectionsActive
// ----------------------------------------------------------------
bool audVehicleReflectionsEntity::AreInteriorReflectionsActive() const
{
	for(u32 i = 0; i < m_InteriorReflections.GetMaxCount(); i++)
	{
		if(m_InteriorReflections[i].m_ReflectionsVoice != NULL)
		{
			return true;
		}
	}

	return false;
}

// ----------------------------------------------------------------
// audVehicleReflectionsEntity StopAllReflectionsVoices
// ----------------------------------------------------------------
void audVehicleReflectionsEntity::StopAllReflectionsVoices()
{
	for(u32 i = 0; i < INTERIOR_REFLECTION_MAX; i++)
	{		
		m_InteriorReflections[i].m_ProbeDistSmoother.Reset();
		StopAndForgetSounds(m_InteriorReflections[i].m_ReflectionsVoice);
	}
}

// ----------------------------------------------------------------
// audVehicleReflectionsEntity RegisterStuntTunnelMaterial
// ----------------------------------------------------------------
void audVehicleReflectionsEntity::RegisterStuntTunnelMaterial(const char* materialName)
{
	phMaterialMgr::Id stuntMaterialID = PGTAMATERIALMGR->FindMaterialId(materialName);
	if(stuntMaterialID != phMaterialMgr::MATERIAL_NOT_FOUND)
	{
		m_StuntTunnelMaterialIDs.PushAndGrow(stuntMaterialID);
	}
}	

// ----------------------------------------------------------------
// audVehicleReflectionsEntity UpdateStuntTunnelReflections
// ----------------------------------------------------------------
void audVehicleReflectionsEntity::UpdateStuntTunnelReflections()
{
	for(u32 i = 0; i < kMaxStuntTunnelProbes; i++)
	{
		m_StuntTunnelProbes[i].m_ProbeLastHitTimer += fwTimer::GetTimeStep();
	}

	if(m_ActiveVehicle)
	{
		// Actually sounds better without this - should we maybe just always not oscillate when not using dynamic reflections?
		// m_OscillationTimer += fwTimer::GetTimeStep();

		Mat34V_In matrix = m_ActiveVehicle->GetVehicle()->GetMatrix();
		Vector3 sourcePos = VEC3V_TO_VECTOR3(matrix.d());	
		Vector3 right = VEC3V_TO_VECTOR3(matrix.a());
		Vector3 forward = VEC3V_TO_VECTOR3(matrix.b());
		Vector3 up = VEC3V_TO_VECTOR3(matrix.c());	

		for(u32 i = 0; i < kMaxStuntTunnelProbes; i++)
		{
			if(!m_StuntTunnelProbes[i].m_ProbeResults.GetWaitingOnResults())
			{
				if(m_StuntTunnelProbes[i].m_ProbeResults.GetResultsReady())
				{
#if __BANK
					if(g_ReflectionsDebugDraw)
					{
						grcDebugDraw::Line(m_StuntTunnelProbes[i].m_ProbeDesc.GetStart(), m_StuntTunnelProbes[i].m_ProbeDesc.GetEnd(), Color32(1.f,1.f,1.f));
					}
#endif

					u32 resultIndex = 0;
					bool hitTunnel = false;				

					if(m_StuntTunnelProbes[i].m_ProbeResults.GetNumHits() > 0)
					{
						do 
						{
							const WorldProbe::CShapeTestHitPoint& probeResult = m_StuntTunnelProbes[i].m_ProbeResults[resultIndex];
							const phInst* physInst = probeResult.GetHitInst();

							if(physInst)
							{			
								if(!IsStuntTunnelMaterial(probeResult.GetHitMaterialId()))
								{
									resultIndex++;
								}
								else
								{
									m_StuntTunnelProbes[i].m_ProbeLastHitTimer = 0.0f;
									hitTunnel = true;								
									break;
								}
							}
							else
							{
								resultIndex++;
							}
						} while (resultIndex < m_StuntTunnelProbes[i].m_ProbeResults.GetNumHits());


						if(resultIndex < m_StuntTunnelProbes[i].m_ProbeResults.GetNumHits())
						{						
							m_StuntTunnelProbes[i].m_ProbeHit = true;
							m_StuntTunnelProbes[i].m_ProbeHitPos = m_StuntTunnelProbes[i].m_ProbeResults[resultIndex].GetHitPosition();

#if __BANK
							if(g_ReflectionsDebugDraw)
							{
								grcDebugDraw::Sphere(m_StuntTunnelProbes[i].m_ProbeHitPos, 0.3f, Color32(1.0f, 1.0f, 0.0f, 0.6f), true, 5);
							}
#endif
						}
						else
						{						
							m_StuntTunnelProbes[i].m_ProbeHit = false;
						}
					}
					else
					{
						m_StuntTunnelProbes[i].m_ProbeHit = false;
					}

					m_StuntTunnelProbes[i].m_ProbeResults.Reset();
				}

				Vector3 source = sourcePos + (forward * g_StuntTunnelRayCastForwardOffset);
				Vector3 target = source;

				switch(i)
				{
				case 0:
					target += right * g_StuntTunnelRayCastLength;
					break;
				case 1:
					target -= right * g_StuntTunnelRayCastLength;
					break;
				case 2:
					target += up * g_StuntTunnelRayCastLength;
					break;
				case 3:
					target -= up * g_StuntTunnelRayCastLength;
					break;
				}

				m_StuntTunnelProbes[i].m_ProbeDesc.SetStartAndEnd(source, target);
				m_StuntTunnelProbes[i].m_ProbeDesc.SetContext(WorldProbe::LOS_Audio);
				m_StuntTunnelProbes[i].m_ProbeDesc.SetIncludeFlags(ArchetypeFlags::GTA_OBJECT_TYPE);
				WorldProbe::GetShapeTestManager()->SubmitTest(m_StuntTunnelProbes[i].m_ProbeDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
			}
		}
	}

	u32 numProbesHit = 0;

	for(u32 i = 0; i < kMaxStuntTunnelProbes; i++)
	{
		if(m_StuntTunnelProbes[i].m_ProbeLastHitTimer < g_StuntTunnelProbeHitTimeout)
		{
			numProbesHit++;
		}
	}

	// All 4 probes have to hit the tunnel to consider it an enter, but 
	bool isInTunnel = m_IsFocusVehicleInStuntTunnel? numProbesHit > 2 : numProbesHit == 4;
	ReflectionsSettings* reflectionsSettings = audNorthAudioEngine::GetObject<ReflectionsSettings>(ATSTRINGHASH("TUNNEL_STATIC_STUNT", 0x70C19F88));

	if(m_ActiveVehicle && m_IsFocusVehicleInStuntTunnel != isInTunnel)
	{
		if(reflectionsSettings)
		{
			const u32 now = fwTimer::GetTimeInMilliseconds();

			if(now - m_LastStuntTunnelWhooshSound > g_StuntTunnelMinWhooshRepeatTimeMs)
			{
				audSoundInitParams initParams;
				initParams.Position = VEC3V_TO_VECTOR3(m_ActiveVehicle->GetVehicle()->GetMatrix().d());

				if(now - m_LastStuntTunnelWhooshSound > 5000 || m_LastStuntTunnelWhooshWasEnter == isInTunnel || m_LastStuntTunnelWhooshPosition.Dist2(initParams.Position) > (g_StuntTunnelMinWhooshExitDistance * g_StuntTunnelMinWhooshExitDistance))
				{
					BANK_ONLY(audDisplayf("Triggering stunt tunnel whoosh at %.02fm from previous whoosh", m_LastStuntTunnelWhooshPosition.Dist(initParams.Position)));
					CreateAndPlaySound(isInTunnel ? reflectionsSettings->EnterSound : reflectionsSettings->ExitSound, &initParams);			
					m_LastStuntTunnelWhooshSound = fwTimer::GetTimeInMilliseconds();
					m_LastStuntTunnelWhooshPosition = initParams.Position;
					m_LastStuntTunnelWhooshWasEnter = isInTunnel;
				}
			}
		}		
	}

	m_IsFocusVehicleInStuntTunnel = isInTunnel;
	
	// If we've got reflections attached to the focus vehicle, trigger the reflections submix voice
	if(reflectionsSettings)
	{
		if(m_ActiveVehicle)
		{
			for(u32 i = 0; i < INTERIOR_REFLECTION_MAX; i++)
			{
				if(isInTunnel)
				{
					f32 delaySeconds = i == 0? reflectionsSettings->MinDelay : reflectionsSettings->MaxDelay;
					f32 dist = delaySeconds * g_AudioEngine.GetConfig().GetSpeedOfSound();
					m_InteriorReflections[i].m_ReflectionsSettings = reflectionsSettings;
					m_InteriorReflections[i].m_ProbeHit = true;
					m_InteriorReflections[i].m_DistanceToFilterInputCurve.Init(reflectionsSettings->DistanceToFilterInput);
					m_InteriorReflections[i].m_ProbeHitPos = VEC3V_TO_VECTOR3(m_ActiveVehicle->GetVehicle()->GetMatrix().GetCol3());
					m_InteriorReflections[i].m_ProbeDist = m_InteriorReflections[i].m_ProbeDistSmoother.CalculateValue(dist);
					m_InteriorReflections[i].m_ProbeDistSmoother.SetRates(m_InteriorReflections[i].m_ReflectionsSettings->Smoothing, m_InteriorReflections[i].m_ReflectionsSettings->Smoothing);
					m_InteriorReflections[i].m_TunnelRight = VEC3V_TO_VECTOR3(m_ActiveVehicle->GetVehicle()->GetMatrix().a());
					m_InteriorReflections[i].m_ReflectionsSettings = reflectionsSettings;

					UpdateInteriorReflectionsVoice(i);
				}
				else
				{
					StopInteriorReflectionsVoice(i);
				}
			}
		}
		else
		{
			for(u32 i = 0; i < INTERIOR_REFLECTION_MAX; i++)
			{
				StopInteriorReflectionsVoice(i);
			}
		}
	}	
}

// ----------------------------------------------------------------
// audVehicleReflectionsEntity AddVehicleReflection
// ----------------------------------------------------------------
bool audVehicleReflectionsEntity::AddVehicleReflection(audVehicleAudioEntity* vehicle)
{
	if(g_VehicleReflectionsEnabled)
	{
		s32 firstFreeIndex = -1;

		for(u32 loop = 0; loop < kMaxVehicleReflections; loop++)
		{
			if(!m_VehicleReflections[loop].m_ReflectionsEntity)
			{
				if(firstFreeIndex == -1)
				{
					firstFreeIndex = loop;
				}
			}
			else if(m_VehicleReflections[loop].m_ReflectionsEntity == vehicle)
			{
				return true;
			}
		}

		if(firstFreeIndex >= 0)
		{
			s32 reflectionsSubmixIndex = g_AudioEngine.GetEnvironment().AssignReflectionsOutputSubmix();

			if(reflectionsSubmixIndex >= 0)
			{
				m_VehicleReflections[firstFreeIndex].m_ReflectionsSubmixIndex = reflectionsSubmixIndex; 
				m_VehicleReflections[firstFreeIndex].m_ReflectionsSubmix = g_AudioEngine.GetEnvironment().GetReflectionsOutputSubmix(reflectionsSubmixIndex);
				m_VehicleReflections[firstFreeIndex].m_ReflectionsEntity = vehicle;
				m_VehicleReflections[firstFreeIndex].m_ReflectionsSettings = audNorthAudioEngine::GetObject<ReflectionsSettings>(ATSTRINGHASH("VEHICLE_DEFAULT", 0xC8ED64B0));
				m_VehicleReflections[firstFreeIndex].m_DistanceToFilterInputCurve.Init(m_VehicleReflections[firstFreeIndex].m_ReflectionsSettings->DistanceToFilterInput);
				return true;
			}
		}
	}
	
	return false;
}

// ----------------------------------------------------------------
// audVehicleReflectionsEntity RemoveVehicleReflections
// ----------------------------------------------------------------
void audVehicleReflectionsEntity::RemoveVehicleReflections(audVehicleAudioEntity* vehicle)
{
	for(u32 loop = 0; loop < kMaxVehicleReflections; loop++)
	{
		if(m_VehicleReflections[loop].m_ReflectionsEntity == vehicle)
		{
			if(m_VehicleReflections[loop].m_ReflectionsSubmixIndex >= 0)
			{
				g_AudioEngine.GetEnvironment().FreeReflectionsOutputSubmix(m_VehicleReflections[loop].m_ReflectionsSubmixIndex);
				m_VehicleReflections[loop].m_ReflectionsSubmixIndex = -1; 
				m_VehicleReflections[loop].m_ReflectionsSubmix = NULL;
			}
			
			m_VehicleReflections[loop].m_ReflectionsEntity = NULL;
			StopAndForgetSounds(m_VehicleReflections[loop].m_ReflectionsVoice);
		}
	}
}

// ----------------------------------------------------------------
// audVehicleReflectionsEntity UpdateVehicleReflections
// ----------------------------------------------------------------
void audVehicleReflectionsEntity::UpdateVehicleReflections()
{
	CPed *localPlayer = CPedFactory::GetFactory()->GetLocalPlayer();

	if(m_ActiveVehicle && localPlayer)
	{
		Vector3 forward = VEC3V_TO_VECTOR3(m_ActiveVehicle->GetVehicle()->GetMatrix().b());
		f32 fwdSpeed = DotProduct(m_ActiveVehicle->GetVehicle()->GetVelocity(), forward);

		for(u32 loop = 0; loop < kMaxVehicleReflections; loop++)
		{
			if(m_VehicleReflections[loop].m_ReflectionsEntity)
			{
				if(!m_VehicleReflections[loop].m_ReflectionsVoice)
				{
					audSoundInitParams initParams;
					initParams.AttackTime = g_ReflectionsAttackTime;
					initParams.SourceEffectSubmixId = (s16) m_VehicleReflections[loop].m_ReflectionsSubmix->GetSubmixId();
					CreateAndPlaySound_Persistent(m_VehicleReflections[loop].m_ReflectionsSettings->SubmixVoice, &m_VehicleReflections[loop].m_ReflectionsVoice, &initParams);
					m_VehicleReflections[loop].m_FirstUpdate = true;
					m_VehicleReflections[loop].m_DistSmoother.Reset();
					m_VehicleReflections[loop].m_DistSmoother.SetRates(m_VehicleReflections[loop].m_ReflectionsSettings->Smoothing, m_VehicleReflections[loop].m_ReflectionsSettings->Smoothing);
				}

				if(m_VehicleReflections[loop].m_ReflectionsVoice)
				{
					// Using the player ped as the listener position, so that we don't get reflections pitching up and down if the camera spins around
					Vector3 closestPoint;
					Vector3 forward = VEC3V_TO_VECTOR3(m_VehicleReflections[loop].m_ReflectionsEntity->GetVehicle()->GetMatrix().b());
					Vector3 vehiclePosition = VEC3V_TO_VECTOR3(m_VehicleReflections[loop].m_ReflectionsEntity->GetPosition());
					Vector3 sourcePosition = VEC3V_TO_VECTOR3(localPlayer->GetTransform().GetPosition());

					if(g_PointSourceVehicleReflections)
					{
						closestPoint = vehiclePosition;
					}
					else
					{
						f32 vehicleLength = m_VehicleReflections[loop].m_ReflectionsEntity->GetVehicle()->GetBoundSphere().GetRadiusf();
						fwGeom::fwLine positioningLine = fwGeom::fwLine(vehiclePosition - (forward * vehicleLength), vehiclePosition + (forward * vehicleLength));
						positioningLine.FindClosestPointOnLine(sourcePosition, closestPoint);

#if __BANK
						if(g_ReflectionsDebugDraw)
						{
							grcDebugDraw::Line(positioningLine.m_start, positioningLine.m_end, Color32(1.f,1.f,1.f));
						}
#endif
					}
					
					f32 distance = m_VehicleReflections[loop].m_DistSmoother.CalculateValue(closestPoint.Dist(sourcePosition));
					f32 speedOfSound = g_AudioEngine.GetConfig().GetSpeedOfSound();
					f32 delaySeconds = ((distance/speedOfSound) * m_VehicleReflections[loop].m_ReflectionsSettings->DelayTimeScalar) + m_VehicleReflections[loop].m_ReflectionsSettings->DelayTimeAddition;
					delaySeconds = Clamp(delaySeconds, m_VehicleReflections[loop].m_ReflectionsSettings->MinDelay, m_VehicleReflections[loop].m_ReflectionsSettings->MaxDelay);
					u32 delaySamples = (u32)(kMixerNativeSampleRate * delaySeconds);
					m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::DelayInSamples + loop, delaySamples);

					const bool filterEnabled = AUD_GET_TRISTATE_VALUE(m_VehicleReflections[loop].m_ReflectionsSettings->Flags, FLAG_ID_REFLECTIONSSETTINGS_FILTERENABLED) == AUD_TRISTATE_TRUE;
					const f32 distanceFactor = Clamp((f32)m_VehicleReflections[loop].m_DistanceToFilterInputCurve.CalculateValue(distance), 0.0f, 1.0f);
					const f32 filterFrequency = m_VehicleReflections[loop].m_ReflectionsSettings->FilterFrequencyMin + ((m_VehicleReflections[loop].m_ReflectionsSettings->FilterFrequencyMax - m_VehicleReflections[loop].m_ReflectionsSettings->FilterFrequencyMin) * distanceFactor);
					const f32 filterBandwidth = m_VehicleReflections[loop].m_ReflectionsSettings->FilterBandwidthMin + ((m_VehicleReflections[loop].m_ReflectionsSettings->FilterBandwidthMax - m_VehicleReflections[loop].m_ReflectionsSettings->FilterBandwidthMin) * distanceFactor);
					const f32 filterResonance = m_VehicleReflections[loop].m_ReflectionsSettings->FilterResonanceMin + ((m_VehicleReflections[loop].m_ReflectionsSettings->FilterResonanceMax - m_VehicleReflections[loop].m_ReflectionsSettings->FilterResonanceMin) * distanceFactor);

					m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::FilterEnabled + m_VehicleReflections[loop].m_ReflectionsSubmixIndex, filterEnabled);
					m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::FilterMode + m_VehicleReflections[loop].m_ReflectionsSubmixIndex, m_VehicleReflections[loop].m_ReflectionsSettings->FilterMode);
					m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::FilterFrequency + m_VehicleReflections[loop].m_ReflectionsSubmixIndex, filterFrequency);
					m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::FilterBandwidth + m_VehicleReflections[loop].m_ReflectionsSubmixIndex, filterBandwidth);
					m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::FilterResonance + m_VehicleReflections[loop].m_ReflectionsSubmixIndex, filterResonance);

					bool invertPhase = AUD_GET_TRISTATE_VALUE(m_VehicleReflections[loop].m_ReflectionsSettings->Flags, (FLAG_ID_REFLECTIONSSETTINGS_INVERTPHASECHANNEL0 + m_VehicleReflections[loop].m_ReflectionsSubmixIndex)) == AUD_TRISTATE_TRUE;
					m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::InvertPhase + m_VehicleReflections[loop].m_ReflectionsSubmixIndex, invertPhase);

					if(m_VehicleReflections[loop].m_FirstUpdate)
					{
						m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::ResetDelay + loop, 1);
						m_VehicleReflections[loop].m_FirstUpdate = false;
					}

					f32 requestedVolume = sm_ReflectionsVehicleSpeedToVolCurve.CalculateValue(fwdSpeed);

					if(m_VehicleReflections[loop].m_ReflectionsEntity->GetVehicle()->pHandling)
					{
						f32 mass = m_VehicleReflections[loop].m_ReflectionsEntity->GetVehicle()->pHandling->m_fMass;
						requestedVolume *= sm_ReflectionsVehicleMassToVolCurve.CalculateValue(mass);
					}

					bool isFirstPersonVehicleCamActive = audNorthAudioEngine::IsRenderingFirstPersonVehicleCam();
					f32 engineExhaustVolume = Max(m_ActiveVehicle->GetDSPSettings()->enginePostSubmixAttenuation - g_EngineVolumeTrim - (isFirstPersonVehicleCamActive? g_StereoEffectEngineVolumeTrim : 0.0f), m_ActiveVehicle->GetDSPSettings()->exhaustPostSubmixAttenuation - g_ExhaustVolumeTrim - (isFirstPersonVehicleCamActive? g_StereoEffectExhaustVolumeTrim : 0.0f));
					m_VehicleReflections[loop].m_ReflectionsVoice->SetRequestedPosition(closestPoint);
					m_VehicleReflections[loop].m_ReflectionsVoice->SetRequestedDopplerFactor(0.0f);
					m_VehicleReflections[loop].m_ReflectionsVoice->SetRequestedVolumeCurveScale(m_VehicleReflections[loop].m_ReflectionsSettings->RollOffScale);					
					m_VehicleReflections[loop].m_ReflectionsVoice->SetRequestedPostSubmixVolumeAttenuation(audDriverUtil::ComputeDbVolumeFromLinear(requestedVolume) + engineExhaustVolume + (m_VehicleReflections[loop].m_ReflectionsSettings->PostSubmixVolumeAttenuation * 0.01f));

#if __BANK
					if(g_ReflectionsDebugDraw)
					{
						grcDebugDraw::Sphere(m_VehicleReflections[loop].m_ReflectionsVoice->GetRequestedPosition(), 1.5f, Color32(1.0f, 0.0f, 0.0f, 0.6f));

						char tempString[64];
						f32 yCoord = 0.2f + (0.1f * loop);

						sprintf(tempString, "Reflection %d: %d/%d samples", loop, delaySamples, DELAY_MAX_SAMPLES_UNALIGNED);
						grcDebugDraw::Text(Vector2(0.05f, yCoord + 0.05f), Color32(255,255,255), tempString);

						sprintf(tempString, "            %0.2f/%0.2f seconds", delaySeconds, DELAY_TIME_MAX/1000.0f);
						grcDebugDraw::Text(Vector2(0.07f, yCoord + 0.07f), Color32(255,255,255), tempString);

						sprintf(tempString, "            Dist: %0.2f m", distance);
						grcDebugDraw::Text(Vector2(0.07f, yCoord + 0.09f), Color32(255,255,255), tempString);
					}
#endif
				}
			}
		}
	}
}

// ----------------------------------------------------------------
// audVehicleReflectionsEntity UpdateProximityWhooshes
// ----------------------------------------------------------------
void audVehicleReflectionsEntity::UpdateProximityWhooshes(Mat34V_In matrix, CEntity* entity)
{
	Vector3 sourcePos = VEC3V_TO_VECTOR3(matrix.d());
	Vector3 forward = VEC3V_TO_VECTOR3(matrix.b());
	Vector3 right = VEC3V_TO_VECTOR3(matrix.a());

	for(u32 i = 0; i < 2; i++)
	{
		if(!m_ProximityWhooshes[i].m_ProbeResults.GetWaitingOnResults())
		{
			if(m_ProximityWhooshes[i].m_ProbeResults.GetResultsReady())
			{
#if __BANK
				if(g_ReflectionsDebugDraw)
				{
					grcDebugDraw::Line(m_ProximityWhooshes[i].m_ProbeDesc.GetStart(), m_ProximityWhooshes[i].m_ProbeDesc.GetEnd(), Color32(1.f,1.f,1.f));
				}
#endif

				u32 resultIndex = 0;

				if(m_ProximityWhooshes[i].m_ProbeResults.GetNumHits() > 0)
				{
					do 
					{
						phInst* physInst = m_ProximityWhooshes[i].m_ProbeResults[resultIndex].GetHitInst();

						if(physInst)
						{
							CEntity* intersectionEntity = CPhysics::GetEntityFromInst(physInst);

							if(intersectionEntity && (intersectionEntity == entity || intersectionEntity->GetIsTypePed()))
							{
								resultIndex++;
							}
							else
							{
								break;
							}
						}
						else
						{
							resultIndex++;
						}
					} while (resultIndex < m_ProximityWhooshes[i].m_ProbeResults.GetNumHits());

					
					if(resultIndex < m_ProximityWhooshes[i].m_ProbeResults.GetNumHits())
					{
						m_ProximityWhooshes[i].m_ProbeHit = true;
						m_ProximityWhooshes[i].m_ProbeHitPos = m_ProximityWhooshes[i].m_ProbeResults[resultIndex].GetHitPosition();
						m_ProximityWhooshes[i].m_ProbeDist = m_ProximityWhooshes[i].m_ProbeDesc.GetStart().Dist(m_ProximityWhooshes[i].m_ProbeResults[resultIndex].GetHitPosition());

#if __BANK
						if(g_ReflectionsDebugDraw)
						{
							grcDebugDraw::Sphere(m_ProximityWhooshes[i].m_ProbeHitPos, 0.3f, Color32(1.0f, 1.0f, 0.0f, 0.6f), true, 5);
						}
#endif
					}
					else
					{
						m_ProximityWhooshes[i].m_ProbeHit = false;
						m_ProximityWhooshes[i].m_ProbeDist = g_ProximityMaxProbeDistance;
					}
				}
				else
				{
					m_ProximityWhooshes[i].m_ProbeHit = false;
					m_ProximityWhooshes[i].m_ProbeDist = g_ProximityMaxProbeDistance;
				}

				const f32 filterFactor = 0.1f;
				f32 prevSmoothedDist = m_ProximityWhooshes[i].m_SmoothedProbeDist;
				m_ProximityWhooshes[i].m_SmoothedProbeDist = (m_ProximityWhooshes[i].m_SmoothedProbeDist * (1.0f - filterFactor)) + (m_ProximityWhooshes[i].m_ProbeDist * filterFactor);
				f32 distChangeRate = (m_ProximityWhooshes[i].m_SmoothedProbeDist - prevSmoothedDist) * fwTimer::GetInvTimeStep();

				if(distChangeRate < -g_ProximityMinDistChangeRate && 
					fwTimer::GetTimeInMilliseconds() - m_ProximityWhooshes[i].m_LastWhooshTime > g_ProximityWhooshMinRepeatTime)
				{
					audSoundInitParams initParams;
					initParams.Position = m_ProximityWhooshes[i].m_ProbeHitPos;

#if __BANK
					if(g_ReflectionsDebugDraw)
					{
						grcDebugDraw::Sphere(initParams.Position, 0.4f, Color32(1.0f, 0.0f, 0.0f, 0.8f), true, 15);
					}
#endif

					CreateAndPlaySound(sm_CameraSwitchSoundSet.Find(ATSTRINGHASH("CameraSwitchWhoosh", 0xD9593FB5)), &initParams);
					m_ProximityWhooshes[i].m_LastWhooshTime = fwTimer::GetTimeInMilliseconds();
				}

				if(i == 0)
				{
					PF_SET(LeftProbeDist, m_ProximityWhooshes[i].m_SmoothedProbeDist);
					PF_SET(LeftProbeDistChangeRate, distChangeRate);
				}
				else
				{
					PF_SET(RightProbeDist, m_ProximityWhooshes[i].m_SmoothedProbeDist);
					PF_SET(RightProbeDistChangeRate, distChangeRate);
				}

				m_ProximityWhooshes[i].m_ProbeResults.Reset();
			}

			f32 entityWidth = 0.0f;

			if(entity)
			{
				entityWidth = entity->GetBaseModelInfo()->GetBoundingBox().GetExtent().GetYf()/2.0f;
			}

			Vector3 source = sourcePos + (forward * g_ProximityRayCastForwardOffset) + ((i == 0? -right : right) * entityWidth);
			Vector3 target = source + ((i == 0? -right : right) * g_ProximityMaxProbeDistance);
			m_ProximityWhooshes[i].m_ProbeDesc.SetStartAndEnd(source, target);
			m_ProximityWhooshes[i].m_ProbeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
			m_ProximityWhooshes[i].m_ProbeDesc.SetContext(WorldProbe::LOS_Audio);
			WorldProbe::GetShapeTestManager()->SubmitTest(m_ProximityWhooshes[i].m_ProbeDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
		}
	}
}

// ----------------------------------------------------------------
// audVehicleReflectionsEntity UpdateBonnetCamStereoEffect
// ----------------------------------------------------------------
void audVehicleReflectionsEntity::UpdateBonnetCamStereoEffect()
{
	CPed *localPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
	
	if(m_ActiveVehicle && localPlayer)
	{				
		for(u32 i = 0; i < INTERIOR_REFLECTION_MAX; i++)
		{
			// Assign a submix
			if(!m_InteriorReflections[i].m_ReflectionsSubmix)
			{
				s32 reflectionsSubmixIndex = g_AudioEngine.GetEnvironment().AssignReflectionsOutputSubmix();

				if(reflectionsSubmixIndex >= 0)
				{
					m_InteriorReflections[i].m_ReflectionsSubmixIndex = reflectionsSubmixIndex; 
					m_InteriorReflections[i].m_ReflectionsSubmix = g_AudioEngine.GetEnvironment().GetReflectionsOutputSubmix(reflectionsSubmixIndex);
				}
			}

			if(!m_InteriorReflections[i].m_ReflectionsSettings)
			{
				m_InteriorReflections[i].m_ReflectionsSettings = audNorthAudioEngine::GetObject<ReflectionsSettings>(ATSTRINGHASH("INTERNAL_VIEW_REFLECTIONS", 0x6CA03B12));
			}

			// Create a voice
			if(m_InteriorReflections[i].m_ReflectionsSubmix && 
			   m_InteriorReflections[i].m_ReflectionsSettings && 
			   !m_InteriorReflections[i].m_ReflectionsVoice)
			{
				audSoundInitParams initParams;
				initParams.AttackTime = g_ReflectionsAttackTime;
				initParams.SourceEffectSubmixId = (s16) m_InteriorReflections[i].m_ReflectionsSubmix->GetSubmixId();
				CreateAndPlaySound_Persistent(ATSTRINGHASH("DEFAULT_REFLECTIONS_SUBMIX_CONTROL", 0xCE8F80C), &m_InteriorReflections[i].m_ReflectionsVoice, &initParams);
				m_InteriorReflections[i].m_FirstUpdate = true;
			}

			// Set voice filter and delay parameters
			if(m_InteriorReflections[i].m_ReflectionsVoice)
			{				
				f32 delaySeconds = 0.0f;

				if(g_StereoDelayLFOEnabled)
				{
					f32 lfoHz = i == 0? g_StereoDelayLFOHzLeft : g_StereoDelayLFOHzRight;
					f32 lfoDelay = i == 0? g_StereoDelayLFOMagLeft : g_StereoDelayLFOMagRight;
					f32 lfoOffset = i == 0? g_StereoDelayLFOOffsetLeft : g_StereoDelayLFOOffsetRight;
					delaySeconds = lfoOffset + (lfoDelay * 0.5f) + (lfoDelay * 0.5f * Sinf((fwTimer::GetTimeInMilliseconds() * 0.001f * lfoHz)));
				}
				else
				{
					delaySeconds = i == 0? m_InteriorReflections[i].m_ReflectionsSettings->MinDelay : m_InteriorReflections[i].m_ReflectionsSettings->MaxDelay;
				}

				u32 delaySamples = (u32)(kMixerNativeSampleRate * delaySeconds);

				m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::DelayInSamples + i, delaySamples);

				if(m_InteriorReflections[i].m_FirstUpdate)
				{
					m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::ResetDelay + i, 1);
					m_InteriorReflections[i].m_FirstUpdate = false;
				}

				f32 headAngle = 0.0f;

				if(g_PanInteriorReflectionsWithHeadAngle)
				{
					Vec3V vehicleForward = m_ActiveVehicle->GetVehicle()->GetTransform().GetForward();
					vehicleForward.SetZ(ScalarV(V_ZERO));
					vehicleForward = Normalize(vehicleForward);

					Vec3V listenerForward = g_AudioEngine.GetEnvironment().GetPanningListenerMatrix().GetCol1();
					listenerForward.SetZ(ScalarV(V_ZERO));
					listenerForward = Normalize(listenerForward);

					Vec3V forwardCross = Cross(vehicleForward, listenerForward);
					f32 forwardDot = Dot(vehicleForward, listenerForward).Getf();
					headAngle = AcosfSafe(forwardDot) * RtoD;

					if(Dot(Vec3V(m_ActiveVehicle->GetVehicle()->GetTransform().GetUp()), forwardCross).Getf() < 0.0f)
					{
						headAngle *= -1.0f;
					}
				}
								
				bool isFirstPersonVehicleCamActive = audNorthAudioEngine::IsRenderingFirstPersonVehicleCam();
				s32 finalPan = i == 0? g_StereoLeftPan : g_StereoRightPan;
				finalPan += (s32)headAngle;

				if(finalPan < 0)
				{
					finalPan += 360;
				}
				else if(finalPan > 360)
				{
					finalPan -= 360;
				}

				f32 engineExhaustVolume = Max(m_ActiveVehicle->GetDSPSettings()->enginePostSubmixAttenuation - g_EngineVolumeTrim - (isFirstPersonVehicleCamActive? g_StereoEffectEngineVolumeTrim : 0.0f), m_ActiveVehicle->GetDSPSettings()->exhaustPostSubmixAttenuation - g_ExhaustVolumeTrim - (isFirstPersonVehicleCamActive? g_StereoEffectExhaustVolumeTrim : 0.0f));
				m_InteriorReflections[i].m_ReflectionsVoice->SetRequestedDopplerFactor(0.0f);
				m_InteriorReflections[i].m_ReflectionsVoice->SetRequestedPan(finalPan);
				m_InteriorReflections[i].m_ReflectionsVoice->SetRequestedPostSubmixVolumeAttenuation(engineExhaustVolume + (i == 0? g_StereoLeftAttenuation : g_StereoRightAttenuation) + (m_InteriorReflections[i].m_ReflectionsSettings->PostSubmixVolumeAttenuation * 0.01f));

				bool filterEnabled = AUD_GET_TRISTATE_VALUE(m_InteriorReflections[i].m_ReflectionsSettings->Flags, FLAG_ID_REFLECTIONSSETTINGS_FILTERENABLED) == AUD_TRISTATE_TRUE;
				const f32 filterFrequency = i == 0? m_InteriorReflections[i].m_ReflectionsSettings->FilterFrequencyMin : m_InteriorReflections[i].m_ReflectionsSettings->FilterFrequencyMax;
				const f32 filterBandwidth = i == 0? m_InteriorReflections[i].m_ReflectionsSettings->FilterBandwidthMin : m_InteriorReflections[i].m_ReflectionsSettings->FilterBandwidthMax;
				const f32 filterResonance = i == 0? m_InteriorReflections[i].m_ReflectionsSettings->FilterResonanceMin : m_InteriorReflections[i].m_ReflectionsSettings->FilterResonanceMax;

				m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::FilterEnabled + m_InteriorReflections[i].m_ReflectionsSubmixIndex, filterEnabled);
				m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::FilterMode + m_InteriorReflections[i].m_ReflectionsSubmixIndex, m_InteriorReflections[i].m_ReflectionsSettings->FilterMode);
				m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::FilterFrequency + m_InteriorReflections[i].m_ReflectionsSubmixIndex, filterFrequency);
				m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::FilterBandwidth + m_InteriorReflections[i].m_ReflectionsSubmixIndex, filterBandwidth);
				m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::FilterResonance + m_InteriorReflections[i].m_ReflectionsSubmixIndex, filterResonance);

				bool invertPhase = AUD_GET_TRISTATE_VALUE(m_InteriorReflections[i].m_ReflectionsSettings->Flags, (FLAG_ID_REFLECTIONSSETTINGS_INVERTPHASECHANNEL0 + m_InteriorReflections[i].m_ReflectionsSubmixIndex)) == AUD_TRISTATE_TRUE;
				m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::InvertPhase + m_InteriorReflections[i].m_ReflectionsSubmixIndex, invertPhase);

#if __BANK
				if(g_ReflectionsDebugDraw)
				{
					char tempString[64];					
					f32 yCoord = 0.05f;

					if(i == INTERIOR_REFLECTION_LEFT)
					{
						f32 xCoord = 0.05f;
						sprintf(tempString, "Left Delay:");
						grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
						xCoord = 0.02f;
						yCoord += 0.02f;						

						sprintf(tempString, "%d/%d samples", delaySamples, DELAY_MAX_SAMPLES_UNALIGNED);
						grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
						yCoord += 0.02f;

						sprintf(tempString, "%f", delaySeconds);
						grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);						
						yCoord += 0.02f;

						sprintf(tempString, "Pan %d", finalPan);
						grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
						yCoord += 0.02f;

						if(filterEnabled)
						{
							sprintf(tempString, "Filter Frequency: %0.2f", filterFrequency);
							grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);						
							yCoord += 0.02f;

							sprintf(tempString, "Filter Bandwidth: %0.2f", filterBandwidth);
							grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);						
							yCoord += 0.02f;

							sprintf(tempString, "Filter Resonance: %0.2f", filterResonance);
							grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);						
							yCoord += 0.02f;
						}
						else
						{
							sprintf(tempString, "Filter Disabled");
							grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);						
							yCoord += 0.02f;
						}
					}
					else
					{
						f32 xCoord = 0.75f;
						sprintf(tempString, "Right Delay:");
						grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
						xCoord += 0.02f;
						yCoord += 0.02f;

						sprintf(tempString, "%d/%d samples", delaySamples, DELAY_MAX_SAMPLES_UNALIGNED);
						grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
						yCoord += 0.02f;

						sprintf(tempString, "%f seconds", delaySeconds);
						grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
						yCoord += 0.02f;

						sprintf(tempString, "Pan %d", finalPan);
						grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
						yCoord += 0.02f;

						if(filterEnabled)
						{
							sprintf(tempString, "Filter Frequency: %0.2f", filterFrequency);
							grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);						
							yCoord += 0.02f;

							sprintf(tempString, "Filter Bandwidth: %0.2f", filterBandwidth);
							grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);						
							yCoord += 0.02f;

							sprintf(tempString, "Filter Resonance: %0.2f", filterResonance);
							grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);						
							yCoord += 0.02f;
						}
						else
						{
							sprintf(tempString, "Filter Disabled");
							grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);						
							yCoord += 0.02f;
						}
					}
				}
#endif
			}
		}
	}
}

// ----------------------------------------------------------------
// audVehicleReflectionsEntity UpdateInteriorReflections
// ----------------------------------------------------------------
void audVehicleReflectionsEntity::UpdateInteriorReflections()
{
	CPed *localPlayer = CPedFactory::GetFactory()->GetLocalPlayer();

	if(m_ActiveVehicle && localPlayer)
	{
		if ((fwTimer::GetSystemFrameCount() & 31) == 23)
		{
			UpdateLoadedInteriors();
		}

		m_OscillationTimer += fwTimer::GetTimeStep();
		m_CurrentSourcePos = VEC3V_TO_VECTOR3(m_ActiveVehicle->GetVehicle()->TransformIntoWorldSpace(m_ActiveVehicle->GetEngineOffsetPos()));
		
		Vector3 forward = VEC3V_TO_VECTOR3(m_ActiveVehicle->GetVehicle()->GetMatrix().b());
		forward.SetZ(0.0f);
		u32 numProbesHit = 0;

		for(u32 i = 0; i < INTERIOR_REFLECTION_MAX; i++)
		{
			if(!m_InteriorReflections[i].m_ProbeResults.GetWaitingOnResults())
			{
				if(m_InteriorReflections[i].m_ProbeResults.GetResultsReady())
				{
#if __BANK
					if(g_ReflectionsDebugDraw)
					{
						grcDebugDraw::Line(m_InteriorReflections[i].m_ProbeDesc.GetStart(), m_InteriorReflections[i].m_ProbeDesc.GetEnd(), Color32(1.f,1.f,1.f));
					}
#endif

					if(m_InteriorReflections[i].m_ProbeResults.GetNumHits() > 0)
					{
						s32 furthestHit = m_InteriorReflections[i].m_ProbeResults.GetNumHits() - 1;
						ReflectionsSettings* reflectionsSettings = NULL;
						CEntity* intersectionEntity = NULL;

						while(furthestHit >= 0 && !reflectionsSettings)
						{
							// Have found a room in the material, lets try and get the interior entity
							phInst* physInst = m_InteriorReflections[i].m_ProbeResults[furthestHit].GetHitInst();
							intersectionEntity = NULL;

							if(physInst)
							{
								intersectionEntity = CPhysics::GetEntityFromInst(physInst);

								if(intersectionEntity)
								{
									CBaseModelInfo *modelInfo = intersectionEntity->GetBaseModelInfo();

									if(modelInfo && modelInfo->GetModelType() == MI_TYPE_MLO)
									{
										if(intersectionEntity != m_InteriorReflections[i].m_ReflectionsEntity)
										{
											InteriorSettings* interiorSettings = audNorthAudioEngine::GetObject<InteriorSettings>(modelInfo->GetHashKey());

											if(interiorSettings)
											{
												reflectionsSettings = audNorthAudioEngine::GetObject<ReflectionsSettings>(interiorSettings->InteriorReflections);
											}
										}
										else
										{
											reflectionsSettings = m_InteriorReflections[i].m_ReflectionsSettings;
										}
									}
									else if (intersectionEntity->GetOwnedBy() == ENTITY_OWNEDBY_STATICBOUNDS)
									{
										strLocalIndex boundsStoreIndex = strLocalIndex(intersectionEntity->GetIplIndex());
										Assert(g_StaticBoundsStore.IsValidSlot(boundsStoreIndex));

										InteriorProxyIndex proxyId = -1;
										strLocalIndex depSlot;
										g_StaticBoundsStore.GetDummyBoundData(boundsStoreIndex, proxyId, depSlot);

										if(proxyId >= 0)
										{
											CInteriorProxy* pInteriorProxy = CInteriorProxy::GetPool()->GetSlot(proxyId);

											if(pInteriorProxy)
											{
												if(intersectionEntity != m_InteriorReflections[i].m_ReflectionsEntity)
												{
													InteriorSettings* interiorSettings = audNorthAudioEngine::GetObject<InteriorSettings>(pInteriorProxy->GetNameHash());

													if(interiorSettings)
													{
														reflectionsSettings = audNorthAudioEngine::GetObject<ReflectionsSettings>(interiorSettings->InteriorReflections);
													}
												}
												else
												{
													reflectionsSettings = m_InteriorReflections[i].m_ReflectionsSettings;
												}
											}
										}
									}
								}
							}	

							if(!reflectionsSettings)
							{
								furthestHit--;
							}
						}

						if(reflectionsSettings)
						{
							m_InteriorReflections[i].m_ProbeHit = true;
							m_InteriorReflections[i].m_ReflectionsSettings = reflectionsSettings;
							m_InteriorReflections[i].m_DistanceToFilterInputCurve.Init(reflectionsSettings->DistanceToFilterInput);
							m_InteriorReflections[i].m_ProbeHitPos = m_InteriorReflections[i].m_ProbeResults[furthestHit].GetHitPosition();
							m_InteriorReflections[i].m_ProbeDist = m_InteriorReflections[i].m_ProbeDesc.GetStart().Dist(m_InteriorReflections[i].m_ProbeResults[furthestHit].GetHitPosition());
							m_InteriorReflections[i].m_ProbeDist = m_InteriorReflections[i].m_ProbeDistSmoother.CalculateValue(Clamp(m_InteriorReflections[i].m_ProbeDist, g_MinProbeDist, g_MaxProbeDist));
							m_InteriorReflections[i].m_ProbeDistSmoother.SetRates(m_InteriorReflections[i].m_ReflectionsSettings->Smoothing, m_InteriorReflections[i].m_ReflectionsSettings->Smoothing);
						}
						else
						{
							m_InteriorReflections[i].m_ProbeHit = false;
						}

						m_InteriorReflections[i].m_ReflectionsEntity = intersectionEntity;
						m_InteriorReflections[i].m_ProbeResults.Reset();
					}
					else
					{
						m_InteriorReflections[i].m_ProbeHit = false;
					}
				}

				CInteriorProxy* closestInterior = IsPointInOrNearDynamicReflectionsInterior(m_CurrentSourcePos);
				if(closestInterior && closestInterior->GetInteriorInst())
				{
					Vector3 tunnelDir;
					CalculateTunnelDirection(closestInterior->GetInteriorInst(), m_CurrentSourcePos, tunnelDir);

					if(tunnelDir.Dot(m_InteriorReflections[i].m_PrevTunnelDirection) < 0.0f)
					{	
						tunnelDir *= -1.0f;
					}

					m_InteriorReflections[i].m_PrevTunnelDirection = tunnelDir;
					Vector3 tunnelRight = tunnelDir;
					tunnelRight.RotateZ(HALF_PI);
					f32 forwardDot = forward.Dot(tunnelDir);
					Vector3 source = m_CurrentSourcePos + (tunnelDir * g_RayCastForwardOffset * forwardDot);
					Vector3 target = source + ((i == INTERIOR_REFLECTION_LEFT? -tunnelRight : tunnelRight) * g_MaxProbeDist);
					m_InteriorReflections[i].m_ProbeDesc.SetStartAndEnd(source, target);
					m_InteriorReflections[i].m_ProbeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
					m_InteriorReflections[i].m_ProbeDesc.SetContext(WorldProbe::LOS_Audio);
					m_InteriorReflections[i].m_TunnelRight = tunnelRight;
					WorldProbe::GetShapeTestManager()->SubmitTest(m_InteriorReflections[i].m_ProbeDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
				}
				else
				{
					m_InteriorReflections[i].m_ProbeHit = false;
				}
			}

#if __BANK
			if(g_TestReflections)
			{
				ReflectionsSettings* reflectionsSettings = audNorthAudioEngine::GetObject<ReflectionsSettings>(g_TestReflectionsName);

				if(reflectionsSettings && AUD_GET_TRISTATE_VALUE(reflectionsSettings->Flags, FLAG_ID_REFLECTIONSSETTINGS_STATICREFLECTIONS) == AUD_TRISTATE_TRUE)
				{
					// Static reflections have two delay lines at fixed offsets from one another
					f32 delaySeconds = i == 0? reflectionsSettings->MinDelay : reflectionsSettings->MaxDelay;
					f32 dist = delaySeconds * g_AudioEngine.GetConfig().GetSpeedOfSound();

					m_InteriorReflections[i].m_ProbeHit = true;
					m_InteriorReflections[i].m_ReflectionsSettings = reflectionsSettings;
					m_InteriorReflections[i].m_DistanceToFilterInputCurve.Init(reflectionsSettings->DistanceToFilterInput);
					m_InteriorReflections[i].m_ProbeHitPos = VEC3V_TO_VECTOR3(localPlayer->GetTransform().GetPosition());
					m_InteriorReflections[i].m_ProbeDist = m_InteriorReflections[i].m_ProbeDistSmoother.CalculateValue(dist);
					m_InteriorReflections[i].m_ProbeDistSmoother.SetRates(m_InteriorReflections[i].m_ReflectionsSettings->Smoothing, m_InteriorReflections[i].m_ReflectionsSettings->Smoothing);
					m_InteriorReflections[i].m_TunnelRight = VEC3V_TO_VECTOR3(m_ActiveVehicle->GetVehicle()->GetMatrix().a());
				}
			}
#endif

			// No dynamic reflections? Check if we've got some static ones linked to this interior
			if(!m_InteriorReflections[i].m_ProbeHit)
			{
				CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();

				if(pIntInst)
				{
					if(pIntInst->GetArchetype())
					{
						u32 modelName = pIntInst->GetArchetype()->GetModelNameHash();
						InteriorSettings* interiorSettings = audNorthAudioEngine::GetObject<InteriorSettings>(modelName);

						if(interiorSettings)
						{
							ReflectionsSettings* reflectionsSettings = audNorthAudioEngine::GetObject<ReflectionsSettings>(interiorSettings->InteriorReflections);

							if(reflectionsSettings && AUD_GET_TRISTATE_VALUE(reflectionsSettings->Flags, FLAG_ID_REFLECTIONSSETTINGS_STATICREFLECTIONS) == AUD_TRISTATE_TRUE)
							{
								// Static reflections have two delay lines at fixed offsets from one another
								f32 delaySeconds = i == 0? reflectionsSettings->MinDelay : reflectionsSettings->MaxDelay;
								f32 dist = delaySeconds * g_AudioEngine.GetConfig().GetSpeedOfSound();
								
								m_InteriorReflections[i].m_ProbeHit = true;
								m_InteriorReflections[i].m_ReflectionsSettings = reflectionsSettings;
								m_InteriorReflections[i].m_DistanceToFilterInputCurve.Init(reflectionsSettings->DistanceToFilterInput);
								m_InteriorReflections[i].m_ProbeHitPos = VEC3V_TO_VECTOR3(localPlayer->GetTransform().GetPosition());
								m_InteriorReflections[i].m_ProbeDist = m_InteriorReflections[i].m_ProbeDistSmoother.CalculateValue(dist);
								m_InteriorReflections[i].m_ProbeDistSmoother.SetRates(m_InteriorReflections[i].m_ReflectionsSettings->Smoothing, m_InteriorReflections[i].m_ReflectionsSettings->Smoothing);
								m_InteriorReflections[i].m_TunnelRight = VEC3V_TO_VECTOR3(m_ActiveVehicle->GetVehicle()->GetMatrix().a());
							}
						}
					}
				}
			}

			// No dynamic or static reflections? Check if the ambient zones are requesting any
			if(!m_InteriorReflections[i].m_ProbeHit)
			{
				for(u32 loop = 0; loop < g_AmbientAudioEntity.GetNumActiveZones(); loop++)
				{
					const audAmbientZone* ambientZone = g_AmbientAudioEntity.GetActiveZone(loop);

					bool isZoneActive = false;

					if(AUD_GET_TRISTATE_VALUE(ambientZone->GetZoneData()->Flags, FLAG_ID_AMBIENTZONE_HASTUNNELREFLECTIONS) == AUD_TRISTATE_TRUE ||
					   AUD_GET_TRISTATE_VALUE(ambientZone->GetZoneData()->Flags, FLAG_ID_AMBIENTZONE_HASREVERBLINKEDREFLECTIONS) == AUD_TRISTATE_TRUE)
					{
						// BuiltUp factor unused in reflections zones, so used to designate cylinder radius
						const ScalarV cylinderRadius = ScalarV(ambientZone->GetZoneData()->BuiltUpFactor * 100.0f);
						const Vec3V localPlayerPos = localPlayer->GetTransform().GetPosition();

						if(ambientZone->GetZoneData()->Shape == kAmbientZoneCuboidLineEmitter || ambientZone->GetZoneData()->Shape == kAmbientZoneSphereLineEmitter)
						{
							const Vec3V closestPoint = ambientZone->ComputeClosestPointOnPositioningZoneBoundary(localPlayerPos);
							const Vec3V lineStart = RCC_VEC3V(ambientZone->GetPositioningLine()->m_start);
							const Vec3V lineEnd = RCC_VEC3V(ambientZone->GetPositioningLine()->m_end);
							const ScalarV distSquared = DistSquared(lineStart, lineEnd);

							if(IsTrue(IsLessThan(DistSquared(localPlayerPos, lineStart), distSquared)) &&
								IsTrue(IsLessThan(DistSquared(localPlayerPos, lineEnd), distSquared)) &&
								IsTrue(IsLessThan(DistSquared(localPlayerPos, closestPoint), (cylinderRadius * cylinderRadius))))
							{
								isZoneActive = true;
							}
						}
						else if(ambientZone->IsPointInPositioningZone(localPlayerPos))
						{
							isZoneActive = true;
						}
					}

					if(isZoneActive)
					{
						if(AUD_GET_TRISTATE_VALUE(ambientZone->GetZoneData()->Flags, FLAG_ID_AMBIENTZONE_HASTUNNELREFLECTIONS) == AUD_TRISTATE_TRUE)
						{	
							// Just hard coding this with a flag for now to keep ambient zone GO size down - if we have enough of these then we may need to make it configurable
							ReflectionsSettings* reflectionsSettings = audNorthAudioEngine::GetObject<ReflectionsSettings>(ATSTRINGHASH("TUNNEL_STATIC_SMALL_ENCLOSED", 0xAB0B5B6A));

							if(reflectionsSettings)
							{
								f32 delaySeconds = i == 0? reflectionsSettings->MinDelay : reflectionsSettings->MaxDelay;
								f32 dist = delaySeconds * g_AudioEngine.GetConfig().GetSpeedOfSound();
								m_InteriorReflections[i].m_ProbeHit = true;
								m_InteriorReflections[i].m_ReflectionsSettings = reflectionsSettings;
								m_InteriorReflections[i].m_DistanceToFilterInputCurve.Init(reflectionsSettings->DistanceToFilterInput);
								m_InteriorReflections[i].m_ProbeHitPos = VEC3V_TO_VECTOR3(localPlayer->GetTransform().GetPosition());
								m_InteriorReflections[i].m_ProbeDist = m_InteriorReflections[i].m_ProbeDistSmoother.CalculateValue(dist);
								m_InteriorReflections[i].m_ProbeDistSmoother.SetRates(m_InteriorReflections[i].m_ReflectionsSettings->Smoothing, m_InteriorReflections[i].m_ReflectionsSettings->Smoothing);
								m_InteriorReflections[i].m_TunnelRight = VEC3V_TO_VECTOR3(m_ActiveVehicle->GetVehicle()->GetMatrix().a());
							}

							break;
						}
						else if(AUD_GET_TRISTATE_VALUE(ambientZone->GetZoneData()->Flags, FLAG_ID_AMBIENTZONE_HASREVERBLINKEDREFLECTIONS) == AUD_TRISTATE_TRUE)
						{
							f32 speakerReverbWetAverage = 0.0f;
							f32 invSpeakerDamping = 1.0f/g_SpeakerReverbDamping;

							for(u32 loop = 0; loop < 4; loop++)
							{
								speakerReverbWetAverage += Max(audNorthAudioEngine::GetGtaEnvironment()->GetSpeakerReverbWet(loop, 0), audNorthAudioEngine::GetGtaEnvironment()->GetSpeakerReverbWet(loop, 1)) * invSpeakerDamping;
							}

							speakerReverbWetAverage /= 4.0f;

							if(speakerReverbWetAverage > g_WetReverbForReflections)
							{
								// Just hard coding this with a flag for now to keep ambient zone GO size down - if we have enough of these then we may need to make it configurable
								ReflectionsSettings* reflectionsSettings = audNorthAudioEngine::GetObject<ReflectionsSettings>(ATSTRINGHASH("TUNNEL_STATIC_CAR_PARK", 0x43CFB20A));

								if(reflectionsSettings)
								{
									f32 delaySeconds = i == 0? reflectionsSettings->MinDelay : reflectionsSettings->MaxDelay;
									f32 dist = delaySeconds * g_AudioEngine.GetConfig().GetSpeedOfSound();
									m_InteriorReflections[i].m_ProbeHit = true;
									m_InteriorReflections[i].m_ReflectionsSettings = reflectionsSettings;
									m_InteriorReflections[i].m_DistanceToFilterInputCurve.Init(reflectionsSettings->DistanceToFilterInput);
									m_InteriorReflections[i].m_ProbeHitPos = VEC3V_TO_VECTOR3(localPlayer->GetTransform().GetPosition());
									m_InteriorReflections[i].m_ProbeDist = m_InteriorReflections[i].m_ProbeDistSmoother.CalculateValue(dist);
									m_InteriorReflections[i].m_ProbeDistSmoother.SetRates(m_InteriorReflections[i].m_ReflectionsSettings->Smoothing, m_InteriorReflections[i].m_ReflectionsSettings->Smoothing);
									m_InteriorReflections[i].m_TunnelRight = VEC3V_TO_VECTOR3(m_ActiveVehicle->GetVehicle()->GetMatrix().a());
								}

								break;
							}
						}
					}
				}
			}

			if(m_InteriorReflections[i].m_ProbeHit)
			{
				m_InteriorReflections[i].m_ProbeHitTimer = 0.0f;
				numProbesHit++;
				UpdateInteriorReflectionsVoice(i);
			}
			else
			{
				m_InteriorReflections[i].m_ProbeHitTimer += fwTimer::GetTimeStep();

				if(m_InteriorReflections[i].m_ProbeHitTimer > 1.0f)
				{
					StopInteriorReflectionsVoice(i);					
				}
			}
		}

		if(numProbesHit == INTERIOR_REFLECTION_MAX && !m_HasPlayedInteriorEnter)
		{
			if(fwTimer::GetTimeInMilliseconds() - m_LastBonnetCamUpdateTime > 500)
			{
				audSoundInitParams initParams;
				initParams.Position = (m_InteriorReflections[INTERIOR_REFLECTION_LEFT].m_ProbeHitPos + m_InteriorReflections[INTERIOR_REFLECTION_RIGHT].m_ProbeHitPos)/2.0f;
				CreateAndPlaySound(m_InteriorReflections[INTERIOR_REFLECTION_LEFT].m_ReflectionsSettings->EnterSound, &initParams);

#if __BANK
				if(g_ReflectionsDebugDraw)
				{
					grcDebugDraw::Sphere(initParams.Position, 0.3f, Color32(1.0f, 1.0f, 0.0f, 0.6f), true, 30);
				}
#endif
			}
			
			m_HasPlayedInteriorEnter = true;
			m_HasPlayedInteriorExit = false;
		}
		else if(numProbesHit == 0 && m_HasPlayedInteriorEnter && !m_HasPlayedInteriorExit)
		{
			if(m_InteriorReflections[INTERIOR_REFLECTION_LEFT].m_ReflectionsSettings)
			{
				if(fwTimer::GetTimeInMilliseconds() - m_LastBonnetCamUpdateTime > 500)
				{
					audSoundInitParams initParams;
					initParams.Position = (m_InteriorReflections[INTERIOR_REFLECTION_LEFT].m_ProbeHitPos + m_InteriorReflections[INTERIOR_REFLECTION_RIGHT].m_ProbeHitPos)/2.0f;
					CreateAndPlaySound(m_InteriorReflections[INTERIOR_REFLECTION_LEFT].m_ReflectionsSettings->ExitSound, &initParams);

#if __BANK
					if(g_ReflectionsDebugDraw)
					{
						grcDebugDraw::Sphere(initParams.Position, 0.3f, Color32(1.0f, 1.0f, 0.0f, 0.6f), true, 30);
					}
#endif
				}

				m_HasPlayedInteriorEnter = false;
				m_HasPlayedInteriorExit = true;
				g_ScriptAudioEntity.PlayMobileGetSignal(0.6f);
			}
		}
	}
	else
	{
		for(u32 i = 0; i < INTERIOR_REFLECTION_MAX; i++)
		{
			m_InteriorReflections[i].m_ProbeHitTimer += fwTimer::GetTimeStep();

			if(m_InteriorReflections[i].m_ProbeHitTimer > 1.0f)
			{
				StopInteriorReflectionsVoice(i);				
			}
		}
	}

#if __BANK
	if(g_ReflectionsDebugDraw)
	{
		char tempString[64];
		formatf(tempString, "Active Interiors: %d", m_CurrentlyLoadedInteriors.GetCount());
		grcDebugDraw::Text(Vector2(0.4f, 0.09f), Color32(255,255,255), tempString);

		formatf(tempString, "Dot Product: %02f", m_DotTest);
		grcDebugDraw::Text(Vector2(0.4f, 0.11f), Color32(255,255,255), tempString);

		for(u32 loop = 0; loop < kMaxVehicleReflections; loop++)
		{
			formatf(tempString, "Submix %d: %s", loop, g_AudioEngine.GetEnvironment().IsReflectionsOutputSubmixFree(loop)? "Free" : "In Use");
			grcDebugDraw::Text(Vector2(0.4f, 0.13f + (0.02f * loop)), Color32(255,255,255), tempString);
		}
	}
#endif
}

// ----------------------------------------------------------------
// Update interior reflections
// ----------------------------------------------------------------
void audVehicleReflectionsEntity::UpdateInteriorReflectionsVoice(u32 i)
{
	if(!m_InteriorReflections[i].m_ReflectionsSubmix)
	{
		s32 reflectionsSubmixIndex = g_AudioEngine.GetEnvironment().AssignReflectionsOutputSubmix();

		if(reflectionsSubmixIndex >= 0)
		{
			m_InteriorReflections[i].m_ReflectionsSubmixIndex = reflectionsSubmixIndex; 
			m_InteriorReflections[i].m_ReflectionsSubmix = g_AudioEngine.GetEnvironment().GetReflectionsOutputSubmix(reflectionsSubmixIndex);
		}
	}

	if(m_InteriorReflections[i].m_ReflectionsSubmix && !m_InteriorReflections[i].m_ReflectionsVoice)
	{
		audSoundInitParams initParams;
		initParams.AttackTime = g_ReflectionsAttackTime;
		initParams.SourceEffectSubmixId = (s16) m_InteriorReflections[i].m_ReflectionsSubmix->GetSubmixId();
		CreateAndPlaySound_Persistent(m_InteriorReflections[i].m_ReflectionsSettings->SubmixVoice, &m_InteriorReflections[i].m_ReflectionsVoice, &initParams);
		m_InteriorReflections[i].m_FirstUpdate = true;
	}

	if(m_InteriorReflections[i].m_ReflectionsVoice)
	{
		// Static emitter distances are faked - just play the sound from the listener position
		if(AUD_GET_TRISTATE_VALUE(m_InteriorReflections[i].m_ReflectionsSettings->Flags, FLAG_ID_REFLECTIONSSETTINGS_STATICREFLECTIONS) == AUD_TRISTATE_TRUE)
		{
			Vector3 listenerPos = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix().d());
			m_InteriorReflections[i].m_ReflectionsVoice->SetRequestedPosition(listenerPos);
		}
		else
		{
			Vector3 right = m_InteriorReflections[i].m_TunnelRight;
			m_InteriorReflections[i].m_ReflectionsVoice->SetRequestedPosition(m_CurrentSourcePos + ((i == INTERIOR_REFLECTION_LEFT? -right : right) * m_InteriorReflections[i].m_ProbeDist));
		}
	}

	if(m_InteriorReflections[i].m_ReflectionsVoice)
	{
		// Using the player ped as the listener position, so that we don't get reflections pitching up and down if the camera spins around
		CPed *localPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
		Vector3 listenerPos = VEC3V_TO_VECTOR3(localPlayer->GetTransform().GetPosition());
		Vector3 position = VEC3V_TO_VECTOR3(m_InteriorReflections[i].m_ReflectionsVoice->GetRequestedPosition());
		Vector3 sourcePos = GetCurrentSourcePosition();
		f32 dist = 0.0f;
		f32 volumeAttenuation = 0.0f;

		// Static reflections are using a fake distance with the actual sounds positioned on the vehicle, so just apply a bit of volume attenuation depending on how far away they are supposed to sound
		if(AUD_GET_TRISTATE_VALUE(m_InteriorReflections[i].m_ReflectionsSettings->Flags, FLAG_ID_REFLECTIONSSETTINGS_STATICREFLECTIONS) == AUD_TRISTATE_TRUE)
		{
			dist = m_InteriorReflections[i].m_ProbeDist;
			volumeAttenuation = sm_StaticReflectionsRollOffCurve.CalculateValue(dist - listenerPos.Dist(position));
		}
		else
		{
			dist = sourcePos.Dist(position) + listenerPos.Dist(position);
		}

		dist += sin(m_OscillationTimer * m_InteriorReflections[i].m_OscillatorSpeed) * m_InteriorReflections[i].m_OscillatorDist;
		f32 delaySeconds = ((dist/g_AudioEngine.GetConfig().GetSpeedOfSound()) * m_InteriorReflections[i].m_ReflectionsSettings->DelayTimeScalar) + m_InteriorReflections[i].m_ReflectionsSettings->DelayTimeAddition;

		if(AUD_GET_TRISTATE_VALUE(m_InteriorReflections[i].m_ReflectionsSettings->Flags, FLAG_ID_REFLECTIONSSETTINGS_STATICREFLECTIONS) != AUD_TRISTATE_TRUE)
		{
			delaySeconds = Clamp(delaySeconds, m_InteriorReflections[i].m_ReflectionsSettings->MinDelay, m_InteriorReflections[i].m_ReflectionsSettings->MaxDelay);
		}

		u32 delaySamples = (u32)(kMixerNativeSampleRate * delaySeconds);

		m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::DelayInSamples + i, delaySamples);

		if(m_InteriorReflections[i].m_FirstUpdate)
		{
			m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::ResetDelay + i, 1);
			m_InteriorReflections[i].m_FirstUpdate = false;
		}

		bool isFirstPersonVehicleCamActive = audNorthAudioEngine::IsRenderingFirstPersonVehicleCam();
		f32 engineExhaustVolume = Max(m_ActiveVehicle->GetDSPSettings()->enginePostSubmixAttenuation - g_EngineVolumeTrim - (isFirstPersonVehicleCamActive? g_StereoEffectEngineVolumeTrim : 0.0f), m_ActiveVehicle->GetDSPSettings()->exhaustPostSubmixAttenuation - g_ExhaustVolumeTrim - (isFirstPersonVehicleCamActive? g_StereoEffectExhaustVolumeTrim : 0.0f));

		// GTAV specific fix
		if(m_ActiveVehicle->IsMonsterTruck())
		{
			// Monster trucks are super loud, need to reign this in a bit
			engineExhaustVolume -= 7.0f;
		}
		else
		{
			const u32 modelNameHash = m_ActiveVehicle->GetVehicleModelNameHash();

			if(modelNameHash == ATSTRINGHASH("INSURGENT", 0x9114EADA) ||
				modelNameHash == ATSTRINGHASH("INSURGENT2", 0x7B7E56F0) ||
				modelNameHash == ATSTRINGHASH("INSURGENT3", 0x8D4B7A8A))
			{
				engineExhaustVolume -= 10.0f;
			}					
		}
		// GTAV specific fix

		m_InteriorReflections[i].m_ReflectionsVoice->SetRequestedDopplerFactor(0.0f);
		m_InteriorReflections[i].m_ReflectionsVoice->SetRequestedPan(-1);
		m_InteriorReflections[i].m_ReflectionsVoice->SetRequestedVolumeCurveScale(m_InteriorReflections[i].m_ReflectionsSettings->RollOffScale);
		m_InteriorReflections[i].m_ReflectionsVoice->SetRequestedPostSubmixVolumeAttenuation(engineExhaustVolume + volumeAttenuation + (m_InteriorReflections[i].m_ReflectionsSettings->PostSubmixVolumeAttenuation * 0.01f));

		bool filterEnabled = AUD_GET_TRISTATE_VALUE(m_InteriorReflections[i].m_ReflectionsSettings->Flags, FLAG_ID_REFLECTIONSSETTINGS_FILTERENABLED) == AUD_TRISTATE_TRUE;
		const f32 distanceFactor = Clamp((f32)m_InteriorReflections[i].m_DistanceToFilterInputCurve.CalculateValue(dist), 0.0f, 1.0f);
		const f32 filterFrequency = m_InteriorReflections[i].m_ReflectionsSettings->FilterFrequencyMin + ((m_InteriorReflections[i].m_ReflectionsSettings->FilterFrequencyMax - m_InteriorReflections[i].m_ReflectionsSettings->FilterFrequencyMin) * distanceFactor);
		const f32 filterBandwidth = m_InteriorReflections[i].m_ReflectionsSettings->FilterBandwidthMin + ((m_InteriorReflections[i].m_ReflectionsSettings->FilterBandwidthMax - m_InteriorReflections[i].m_ReflectionsSettings->FilterBandwidthMin) * distanceFactor);
		const f32 filterResonance = m_InteriorReflections[i].m_ReflectionsSettings->FilterResonanceMin + ((m_InteriorReflections[i].m_ReflectionsSettings->FilterResonanceMax - m_InteriorReflections[i].m_ReflectionsSettings->FilterResonanceMin) * distanceFactor);

		m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::FilterEnabled + m_InteriorReflections[i].m_ReflectionsSubmixIndex, filterEnabled);
		m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::FilterMode + m_InteriorReflections[i].m_ReflectionsSubmixIndex, m_InteriorReflections[i].m_ReflectionsSettings->FilterMode);
		m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::FilterFrequency + m_InteriorReflections[i].m_ReflectionsSubmixIndex, filterFrequency);
		m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::FilterBandwidth + m_InteriorReflections[i].m_ReflectionsSubmixIndex, filterBandwidth);
		m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::FilterResonance + m_InteriorReflections[i].m_ReflectionsSubmixIndex, filterResonance);

		bool invertPhase = AUD_GET_TRISTATE_VALUE(m_InteriorReflections[i].m_ReflectionsSettings->Flags, (FLAG_ID_REFLECTIONSSETTINGS_INVERTPHASECHANNEL0 + m_InteriorReflections[i].m_ReflectionsSubmixIndex)) == AUD_TRISTATE_TRUE;
		m_ReflectionsSourceSubmix->SetEffectParam(0, audVariableDelayEffect::InvertPhase + m_InteriorReflections[i].m_ReflectionsSubmixIndex, invertPhase);

#if __BANK
		if(g_ReflectionsDebugDraw)
		{
			char tempString[64];

			if(i == INTERIOR_REFLECTION_LEFT)
			{
				sprintf(tempString, "Left Delay: %d/%d samples", delaySamples, DELAY_MAX_SAMPLES_UNALIGNED);
				grcDebugDraw::Text(Vector2(0.05f, 0.05f), Color32(255,255,255), tempString);

				sprintf(tempString, "            %0.2f/%0.2f seconds", delaySeconds, DELAY_TIME_MAX/1000.0f);
				grcDebugDraw::Text(Vector2(0.07f, 0.07f), Color32(255,255,255), tempString);

				sprintf(tempString, "            Dist: %0.2f m", dist);
				grcDebugDraw::Text(Vector2(0.07f, 0.09f), Color32(255,255,255), tempString);

				const char* reflectionsName = audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_InteriorReflections[i].m_ReflectionsSettings->NameTableOffset);
				sprintf(tempString, "            Gameobject: %s", reflectionsName);
				grcDebugDraw::Text(Vector2(0.07f, 0.11f), Color32(255,255,255), tempString);

				f32 fwdSpeed = DotProduct(m_ActiveVehicle->GetVehicle()->GetVelocity(), VEC3V_TO_VECTOR3(m_ActiveVehicle->GetVehicle()->GetTransform().GetB()));
				sprintf(tempString, "            Speed: %0.2f m/s", fwdSpeed);
				grcDebugDraw::Text(Vector2(0.4f, 0.05f), Color32(255,255,255), tempString);
			}
			else
			{
				sprintf(tempString, "Right Delay: %d/%d samples", delaySamples, DELAY_MAX_SAMPLES_UNALIGNED);
				grcDebugDraw::Text(Vector2(0.75f, 0.05f), Color32(255,255,255), tempString);

				sprintf(tempString, "            %0.2f/%0.2f seconds", delaySeconds, DELAY_TIME_MAX/1000.0f);
				grcDebugDraw::Text(Vector2(0.75f, 0.07f), Color32(255,255,255), tempString);

				sprintf(tempString, "            Dist: %0.2f m", dist);
				grcDebugDraw::Text(Vector2(0.75f, 0.09f), Color32(255,255,255), tempString);

				const char* reflectionsName = audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_InteriorReflections[i].m_ReflectionsSettings->NameTableOffset);
				sprintf(tempString, "            Gameobject: %s", reflectionsName);
				grcDebugDraw::Text(Vector2(0.75f, 0.11f), Color32(255,255,255), tempString);
			}
		}
#endif
	}
}

// ----------------------------------------------------------------
// Stop the given interior reflections voice
// ----------------------------------------------------------------
void audVehicleReflectionsEntity::StopInteriorReflectionsVoice(u32 i)
{
	if(m_InteriorReflections[i].m_ReflectionsSubmixIndex >= 0)
	{
		g_AudioEngine.GetEnvironment().FreeReflectionsOutputSubmix(m_InteriorReflections[i].m_ReflectionsSubmixIndex);
		m_InteriorReflections[i].m_ReflectionsSubmixIndex = -1; 
		m_InteriorReflections[i].m_ReflectionsSubmix = NULL;
	}

	m_InteriorReflections[i].m_ProbeDistSmoother.Reset();
	StopAndForgetSounds(m_InteriorReflections[i].m_ReflectionsVoice);
}

// ----------------------------------------------------------------
// Update which interiors are loaded
// ----------------------------------------------------------------
void audVehicleReflectionsEntity::UpdateLoadedInteriors()
{
	m_CurrentlyLoadedInteriors.clear();
	CInteriorProxy* intProxy = NULL;
	CInteriorProxy::Pool* pool = CInteriorProxy::GetPool();

	if(pool)
	{
		s32 poolSize = pool->GetSize();
		while(poolSize--)
		{
			intProxy = pool->GetSlot(poolSize);
			if(intProxy && intProxy->GetInteriorInst())
			{
				m_CurrentlyLoadedInteriors.PushAndGrow(intProxy);
			}
		}
	}
}

// ----------------------------------------------------------------
// Check if a point is in or close to an interior
// ----------------------------------------------------------------
CInteriorProxy* audVehicleReflectionsEntity::IsPointInOrNearDynamicReflectionsInterior(const Vector3& position)
{
	f32 closestInteriorDistanceSq = FLT_MAX;
	CInteriorProxy* closestInterior = NULL;

	for(s32 loop = 0; loop < m_CurrentlyLoadedInteriors.GetCount(); loop++)
	{
		if(m_CurrentlyLoadedInteriors[loop]->GetInteriorInst() && m_CurrentlyLoadedInteriors[loop]->GetInteriorInst()->GetInteriorSceneGraphNode() && m_CurrentlyLoadedInteriors[loop]->GetInteriorInst()->GetNumPortals() == 2)
		{
			spdAABB boundingBox;
			m_CurrentlyLoadedInteriors[loop]->GetBoundBox(boundingBox);
			spdSphere sphere = spdSphere(Vec3V(position), ScalarV(V_TEN));

			if(boundingBox.IntersectsSphere(sphere))
			{
				f32 distSq = VEC3V_TO_VECTOR3(boundingBox.GetCenter()).Dist2(position);
				if(distSq < closestInteriorDistanceSq)
				{
					CInteriorProxy* thisInterior = m_CurrentlyLoadedInteriors[loop];
					InteriorSettings* interiorSettings = audNorthAudioEngine::GetObject<InteriorSettings>(thisInterior->GetNameHash());

					if(interiorSettings)
					{
						ReflectionsSettings* reflectionsSettings = audNorthAudioEngine::GetObject<ReflectionsSettings>(interiorSettings->InteriorReflections);

						if(reflectionsSettings && AUD_GET_TRISTATE_VALUE(reflectionsSettings->Flags, FLAG_ID_REFLECTIONSSETTINGS_STATICREFLECTIONS) != AUD_TRISTATE_TRUE)
						{
							closestInterior = m_CurrentlyLoadedInteriors[loop];
							closestInteriorDistanceSq = distSq;	
						}
					}
				}
			}
		}
	}

	return closestInterior;
}

// ----------------------------------------------------------------
// Calculate the forward direction through a tunnel
// ----------------------------------------------------------------
void audVehicleReflectionsEntity::CalculateTunnelDirection(CInteriorInst* interiorInst, const Vector3& position, Vector3& direction)
{
	if(audVerifyf(interiorInst->GetNumPortals() == 2, "Tunnel %s has > 2 exits", interiorInst->GetModelName()))
	{
		Vec3V position2D = RCC_VEC3V(position);
		position2D.SetZ(ScalarV(V_ZERO));

		const Vec3V portal0p0 = interiorInst->GetPortal(0).GetCornerV(0);
		const Vec3V portal0p1 = interiorInst->GetPortal(0).GetCornerV(1);
		const Vec3V portal0p2 = interiorInst->GetPortal(0).GetCornerV(2);
		const Vec3V portal0p3 = interiorInst->GetPortal(0).GetCornerV(3);
		Vec3V portal0Centre = (portal0p0 + portal0p1 + portal0p2 + portal0p3) * ScalarV(V_QUARTER);
		Vec3V portal0Normal = Normalize(Cross(portal0p1 - portal0p0, portal0p2 - portal0p0) + Cross(portal0p2 - portal0p0, portal0p3 - portal0p0));

		const Vec3V portal1p0 = interiorInst->GetPortal(1).GetCornerV(0);
		const Vec3V portal1p1 = interiorInst->GetPortal(1).GetCornerV(1);
		const Vec3V portal1p2 = interiorInst->GetPortal(1).GetCornerV(2);
		const Vec3V portal1p3 = interiorInst->GetPortal(1).GetCornerV(3);
		Vec3V portal1Centre = (portal1p0 + portal1p1 + portal1p2 + portal1p3) * ScalarV(V_QUARTER);
		Vec3V portal1Normal = Normalize(Cross(portal1p1 - portal1p0, portal1p2 - portal1p0) + Cross(portal1p2 - portal1p0, portal1p3 - portal1p0));

		if(Dot(portal0Normal, portal1Centre - portal0Centre).Getf() < 0.0f)
		{
			portal0Normal *= ScalarV(V_NEGONE);
		}

		if(Dot(portal1Normal, portal1Centre - portal0Centre).Getf() < 0.0f)
		{
			portal1Normal *= ScalarV(V_NEGONE);
		}

#if __BANK
		if(g_ReflectionsDebugDraw)
		{
			grcDebugDraw::Line(portal0Centre, portal0Centre + (portal0Normal * ScalarV(20.0f)), Color32(1.f,0.f,0.f), 5);
			grcDebugDraw::Line(portal1Centre, portal1Centre + (portal1Normal * ScalarV(20.0f)), Color32(0.f,1.f,0.f), 5);
		}
#endif

		portal0Centre.SetZ(ScalarV(V_ZERO));
		portal1Centre.SetZ(ScalarV(V_ZERO));

		ScalarV distBetweenPortals = Dist(portal0Centre, portal1Centre);
		ScalarV distToPortal0 = Dist(position2D, portal0Centre);
		ScalarV fractionBetween = Clamp(distToPortal0/distBetweenPortals, ScalarV(V_ZERO), ScalarV(V_ONE));
		Vec3V tunnelDirection = AddScaled(portal0Normal, portal1Normal - portal0Normal, fractionBetween);
		tunnelDirection.SetZ(ScalarV(V_ZERO));

		direction = VEC3V_TO_VECTOR3(Normalize(tunnelDirection));
	}
}

#if __BANK
// ----------------------------------------------------------------
// audVehicleReflectionsEntity UpdateDebug
// ----------------------------------------------------------------
void audVehicleReflectionsEntity::UpdateDebug()
{
	if(g_ReflectionsDebugDraw)
	{
		CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();

		if(pIntInst && m_ActiveVehicle)
		{
			Vector3 tunnelDir;
			CalculateTunnelDirection(pIntInst, m_CurrentSourcePos, tunnelDir);
			Vector3 forward = VEC3V_TO_VECTOR3(m_ActiveVehicle->GetVehicle()->GetMatrix().b());
			forward.SetZ(0.0f);	
			f32 forwardDot = forward.Dot(tunnelDir);
			tunnelDir *= 20.0f;
			grcDebugDraw::Line(m_CurrentSourcePos, m_CurrentSourcePos + (tunnelDir * forwardDot), Color32(1.f,1.f,1.f));
		}
	}

	for(u32 i = 0; i < INTERIOR_REFLECTION_MAX; i++)
	{
		if(g_ReflectionsDebugDraw)
		{
			if(m_InteriorReflections[i].m_ReflectionsVoice)
			{
				grcDebugDraw::Sphere(m_InteriorReflections[i].m_ReflectionsVoice->GetRequestedPosition(), 0.3f, Color32(1.0f, 0.0f, 0.0f, 0.6f));
			}
		}
	}

	if(g_ReflectionsLocationPickerEnabled)
	{
		if(CDebugScene::GetMouseLeftPressed())
		{
			CDebugScene::GetWorldPositionUnderMouse(m_DebugWorldPos);
			AddDebugReflectionPosition(m_DebugWorldPos);
		}
	}

	if(g_DebugPrintInteriorTunnels || g_DebugPrintInteriorsAll)
	{
		CInteriorProxy* intProxy = NULL;
		CInteriorProxy::Pool* pool = CInteriorProxy::GetPool();

		if(pool)
		{
			s32 poolSize = pool->GetSize();
			while(poolSize--)
			{
				intProxy = pool->GetSlot(poolSize);

				if(intProxy && (g_DebugPrintInteriorsAll || atString(intProxy->GetModelName()).IndexOf("tun") >= 0))
				{
					spdAABB boundingBox;
					intProxy->GetBoundBox(boundingBox);
					audDisplayf("%s: %s %f %f %f", g_DebugPrintInteriorsAll? "Interior" : "Tunnel", intProxy->GetModelName(), boundingBox.GetCenter().GetXf(), boundingBox.GetCenter().GetYf(), boundingBox.GetCenter().GetZf());
				}
			}
		}

		g_DebugPrintInteriorTunnels = false;
		g_DebugPrintInteriorsAll = false;
	}
}

// ----------------------------------------------------------------
// audVehicleReflectionsEntity AddWidgets
// ----------------------------------------------------------------
void audVehicleReflectionsEntity::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Vehicle Reflections",false);
		bank.PushGroup("Debug Controls",false);
			bank.PushGroup("Bonnet Cam Stereo Effect",false);
				bank.AddToggle("Bonnet Cam Stereo Effect Enabled", &g_BonnetCamStereoEffectEnabled);
				bank.AddToggle("Debug Draw", &g_ReflectionsDebugDraw);
				bank.AddSlider("Left Pan", &g_StereoLeftPan, 0, 360, 1);
				bank.AddSlider("Right Pan", &g_StereoRightPan, 0, 360, 1);
				bank.AddSlider("Left Attenuation", &g_StereoLeftAttenuation, -100.0f, 0.0f, 0.1f);
				bank.AddSlider("Right Attenuation", &g_StereoRightAttenuation, -100.0f, 0.0f, 0.01f);
				bank.AddToggle("Delay LFO Enabled", &g_StereoDelayLFOEnabled);
				bank.AddSlider("Delay LFO Hz (Left Channel)", &g_StereoDelayLFOHzLeft, 0.0f, 5000.0f, 0.0001f);
				bank.AddSlider("Delay LFO Hz (Right Channel)", &g_StereoDelayLFOHzRight, 0.0f, 5000.0f, 0.0001f);
				bank.AddSlider("Delay LFO Amplitude (Left Channel)", &g_StereoDelayLFOMagLeft, 0.0f, 2.0f, 0.0001f);	
				bank.AddSlider("Delay LFO Amplitude (Right Channel)", &g_StereoDelayLFOMagRight, 0.0f, 2.0f, 0.0001f);
				bank.AddSlider("Delay LFO Offset (Left Channel)", &g_StereoDelayLFOOffsetLeft, 0.0f, 2.0f, 0.0001f);	
				bank.AddSlider("Delay LFO Offset (Right Channel)", &g_StereoDelayLFOOffsetRight, 0.0f, 2.0f, 0.0001f);
				bank.AddSlider("Stereo Engine Volume Trim", &g_StereoEffectEngineVolumeTrim, -100.0f, 100.f, 0.1f);
				bank.AddSlider("Stereo Exhaust Volume Trim", &g_StereoEffectExhaustVolumeTrim, -100.0f, 100.f, 0.1f);
				bank.AddToggle("Pan Interior Reflections With Head Angle", &g_PanInteriorReflectionsWithHeadAngle);
			bank.PopGroup();
			bank.AddText("Test Static Reflections Name", g_TestReflectionsName, sizeof(g_TestReflectionsName));
			bank.AddToggle("Test Static Reflections", &g_TestReflections);
			bank.AddToggle("Debug Draw", &g_ReflectionsDebugDraw);
			bank.AddToggle("Debug Print Tunnel Locations", &g_DebugPrintInteriorTunnels);
			bank.AddToggle("Debug Print All Locations", &g_DebugPrintInteriorsAll);
			bank.AddSlider("Engine Volume Trim", &g_EngineVolumeTrim, -100.0f, 100.f, 0.1f);
			bank.AddSlider("Exhaust Volume Trim", &g_ExhaustVolumeTrim, -100.0f, 100.f, 0.1f);
			bank.AddToggle("Interior Reflections Enabled", &g_InteriorReflectionsEnabled);
			bank.AddToggle("Vehicle Reflections Enabled", &g_VehicleReflectionsEnabled);
			bank.AddToggle("Force Stunt Tunnel Probes", &g_ForceStuntTunnelProbesEnabled);
			bank.AddToggle("Point Source Vehicle Reflections", &g_PointSourceVehicleReflections);			
			bank.AddToggle("Location Picker Enabled ", &g_ReflectionsLocationPickerEnabled);
			bank.AddSlider("Max Probe Distance", &g_MaxProbeDist, 0.0f, 100.f, 1.0f);
			bank.AddSlider("Min Probe Distance", &g_MinProbeDist, 0.0f, 100.f, 1.0f);
			bank.AddSlider("Vehicle Reflections Activation Distance", &g_VehicleReflectionsActivationDist, 0.0f, 100.f, 1.0f);
			bank.AddSlider("Attack Time", &g_ReflectionsAttackTime, 0, 1000, 10);			
			bank.AddSlider("Ray Cast Forward Offset", &g_RayCastForwardOffset, 0.0f, 100.0F, 0.1f);			
			bank.AddSlider("Step Rate Smoothing", &g_ReflectionsStepRateSmoothing, 0.0001f, 10.f, 0.01f);
			bank.AddSlider("Left Oscillator Speed", &g_LeftOscillatorSpeed, 0.01f, 10.f, 0.1f);
			bank.AddSlider("Right Oscillator Speed", &g_RightOscillatorSpeed, 0.01f, 10.f, 0.1f);
			bank.AddSlider("Left Oscillator Distance", &g_LeftOscillatorDist, 0.01f, 10.f, 0.1f);
			bank.AddSlider("Right Oscillator Distance", &g_RightOscillatorDist, 0.01f, 10.f, 0.1f);
			bank.AddButton("Detach Reflections", datCallback(MFA(audVehicleReflectionsEntity::DetachFromAll), (datBase*)this), "");
			bank.AddToggle("Force Character Switch Proximity Whooshes", &g_ForceSwitchProximityWhooshes);
			bank.AddSlider("Proximity Required Dist Change Rate", &g_ProximityMinDistChangeRate, 1.0f, 500.0f, 1.0f);
			bank.AddSlider("Proximity Max Probe Dist", &g_ProximityMaxProbeDistance, 0.0f, 100.f, 0.1f);
			bank.AddSlider("Proximity Min Repeat Time", &g_ProximityWhooshMinRepeatTime, 0, 5000, 10);
			bank.AddSlider("Proximity Ray Cast Forward Offset", &g_ProximityRayCastForwardOffset, 0.0f, 100.0F, 0.1f);
			bank.PushGroup("Stunt Tunnels",false);
				bank.AddSlider("Stunt Tunnel Ray Cast Length", &g_StuntTunnelRayCastLength, 0.0f, 100.0F, 0.1f);
				bank.AddSlider("Stunt Tunnel Probe Hit Timeout", &g_StuntTunnelProbeHitTimeout, 0.0f, 10.0F, 0.1f);
				bank.AddSlider("Stunt Tunnel Ray Cast Forward Offset", &g_StuntTunnelRayCastForwardOffset, 0.0f, 100.0F, 0.1f);
				bank.AddSlider("Stunt Tunnel Min Whoosh Repeat Time", &g_StuntTunnelMinWhooshRepeatTimeMs, 0, 50000, 1000);
				bank.AddSlider("Stunt Boost Intensity Min Increase Time", &g_StuntBoostIntensityIncreaseDelay, 0, 1000, 10);
				bank.AddSlider("Recharge Intensity Min Increase Time", &g_RechargeIntensityIncreaseDelay, 0, 1000, 10);				
				bank.AddSlider("Stunt Tunnel Whoosh Exit Min Dist", &g_StuntTunnelMinWhooshExitDistance, 0.f, 1000.f, 1.f);					
			bank.PopGroup();
		bank.PopGroup();
	bank.PopGroup();
}
#endif

