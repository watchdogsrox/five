#include "audioengine/engine.h"
#include "audioengine/engineutil.h"
#include "debugaudio.h"
#include "grcore/debugdraw.h"

#include "audio/ambience/ambientaudioentity.h"
#include "ai/EntityScanner.h"
#include "vehicleconductorfakescenes.h"
#include "northaudioengine.h"
#include "scene/world/GameWorld.h"
#include "Peds/ped.h"

AUDIO_OPTIMISATIONS()

f32 audVehicleConductorFakeScenes::sm_DistanceFakedSirens = 300.f;
f32 audVehicleConductorFakeScenes::sm_FakeSirenDiscWidth = 300.;
f32 audVehicleConductorFakeScenes::sm_MinUpCountryDistThresholdForFakedSirens = 75.f;
f32 audVehicleConductorFakeScenes::sm_MaxUpCountryDistThresholdForFakedSirens = 150.f;
f32 audVehicleConductorFakeScenes::sm_MinUpCityDistThresholdForFakedSirens = 100.f;
f32 audVehicleConductorFakeScenes::sm_MaxUpCityDistThresholdForFakedSirens = 400.f;
f32 audVehicleConductorFakeScenes::sm_TimeToMoveDistantSiren = 200.f;

BANK_ONLY(bool audVehicleConductorFakeScenes::sm_ShowDistantSirenInfo = false;)
//-------------------------------------------------------------------------------------------------------------------
//														INIT
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorFakeScenes::Init()
{
	naAudioEntity::Init();
	audEntity::SetName("audVehicleConductorFakeScenes");

	m_DistanceSiren = NULL;
	m_FakeDistSirenLinVol = 1.f;
	m_FakeDistSirenVolSmoother.Init(0.001f,true);
	m_OWOToFakeSirnVol.Init(ATSTRINGHASH("OWO_TO_FAKE_SIREN_VOL", 0x12AD333E));

	m_DistantSirenPosApply = 0.f; 
	m_DistantSirenPos = Vec3V(V_ZERO);
	SetState(State_Idle);
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorFakeScenes::Reset()
{
	if(m_DistanceSiren)
	{
		m_DistanceSiren->StopAndForget();
		m_DistanceSiren = NULL;
	}
	SetState(State_Idle);
}
//-------------------------------------------------------------------------------------------------------------------
//									GUNFIGHT CONDUCTOR FAKED SCENES DECISION MAKING
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorFakeScenes::ProcessUpdate()
{
	// update fake sirens position 
	if(m_DistanceSiren)
	{
		CPed* player = CGameWorld::FindLocalPlayer();
		if(naVerifyf(player,"Error getting player ref."))
		{
			const Vec3V sirenPosition = player->GetTransform().GetPosition() + Vec3V(0.f,0.f,sm_DistanceFakedSirens);
			m_DistanceSiren->SetRequestedPosition(sirenPosition);
			f32 listenerHeight = g_AudioEngine.GetEnvironment().GetVolumeListenerPosition().GetZf();
			if(!g_AmbientAudioEntity.IsPlayerInTheCity())
			{
				m_FakeDistSirenLinVol = m_FakeDistSirenVolSmoother.CalculateValue(1.f - ClampRange(listenerHeight, sm_MinUpCountryDistThresholdForFakedSirens,sm_MaxUpCountryDistThresholdForFakedSirens),g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
			}
			else
			{
				m_FakeDistSirenLinVol = m_FakeDistSirenVolSmoother.CalculateValue(1.f - ClampRange(listenerHeight, sm_MinUpCityDistThresholdForFakedSirens,sm_MaxUpCityDistThresholdForFakedSirens),g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
			}
			// Occlusion
			//naDisplayf("OWOAP : %f",audNorthAudioEngine::GetOcclusionManager()->GetOutsideWorldOcclusionAfterPortals());
			m_FakeDistSirenLinVol *= m_OWOToFakeSirnVol.CalculateValue(audNorthAudioEngine::GetOcclusionManager()->GetOutsideWorldOcclusionAfterPortals());
			//naDisplayf("VOL : %f %f",m_FakeDistSirenLinVol, audDriverUtil::ComputeDbVolumeFromLinear(m_FakeDistSirenLinVol));
			m_DistanceSiren->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(m_FakeDistSirenLinVol));
			Vec3V desirePos = m_DistantSirenPos;
			GetFakeSirenPosition(desirePos);
			// means there is a change on direction
			if(IsFalse(Dot(m_DistantSirenPos,desirePos) == ScalarV(1.f)))
			{
				//Update apply
				m_DistantSirenPosApply += (1.f/sm_TimeToMoveDistantSiren) * audNorthAudioEngine::GetTimeStep();
				m_DistantSirenPosApply = Min(m_DistantSirenPosApply,1.f);
				f32 newX = Lerp (m_DistantSirenPosApply,m_DistantSirenPos.GetXf(),desirePos.GetXf());
				f32 newY = Lerp (m_DistantSirenPosApply,m_DistantSirenPos.GetYf(),desirePos.GetYf());
				m_DistantSirenPos = Vec3V(newX,newY,m_DistantSirenPos.GetZf());
				m_DistanceSiren->SetRequestedPosition(m_DistantSirenPos);
			}
		}
	}
	Update();
	BANK_ONLY(VehicleConductorFSDebugInfo();)
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audVehicleConductorFakeScenes::UpdateState(const s32 state, const s32, const fwFsm::Event event)
{
	fwFsmBegin
		fwFsmState(State_Idle)
		fwFsmOnUpdate
		return Idle();
	fwFsmEnd
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audVehicleConductorFakeScenes::Idle()
{
	return fwFsm::Continue;
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorFakeScenes::PlayFakeDistanceSirens()
{
	if(!m_DistanceSiren)
	{
		m_FakeDistSirenLinVol = 1.f;
		audSoundInitParams initParams;
		GetFakeSirenPosition(m_DistantSirenPos);
		initParams.Position = VEC3V_TO_VECTOR3(m_DistantSirenPos);
		f32 listenerHeight = g_AudioEngine.GetEnvironment().GetVolumeListenerPosition().GetZf();
		if(!g_AmbientAudioEntity.IsPlayerInTheCity())
		{
			m_FakeDistSirenLinVol = m_FakeDistSirenVolSmoother.CalculateValue(1.f - ClampRange(listenerHeight, sm_MinUpCountryDistThresholdForFakedSirens,sm_MaxUpCountryDistThresholdForFakedSirens),g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
		}
		else
		{
			m_FakeDistSirenLinVol = m_FakeDistSirenVolSmoother.CalculateValue(1.f - ClampRange(listenerHeight, sm_MinUpCityDistThresholdForFakedSirens,sm_MaxUpCityDistThresholdForFakedSirens),g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
		}
		m_FakeDistSirenLinVol *= m_OWOToFakeSirnVol.CalculateValue(audNorthAudioEngine::GetOcclusionManager()->GetOutsideWorldOcclusionAfterPortals());
		initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(m_FakeDistSirenLinVol);
		CreateAndPlaySound_Persistent(ATSTRINGHASH("SIRENS_CONDUCTOR_VARIABLE", 0x7966038A), &m_DistanceSiren, &initParams);
		REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(ATSTRINGHASH("SIRENS_CONDUCTOR_VARIABLE", 0x7966038A), &initParams, m_DistanceSiren, NULL,eAmbientSoundEntity);)
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorFakeScenes::GetFakeSirenPosition(Vec3V_InOut sirenPosition)
{
	CPed* player = CGameWorld::FindLocalPlayer();
	if(naVerifyf(player,"Error getting player ref."))
	{
		Vector3 avgDirection = Vector3(0.f,0.f,0.f); 
		SUPERCONDUCTOR.GetVehicleConductor().GetAvgDirForFakeSiren(avgDirection);
		sirenPosition = Add(player->GetTransform().GetPosition(),Vec3V(0.f,0.f,sm_DistanceFakedSirens));
		sirenPosition = Add(sirenPosition,VECTOR3_TO_VEC3V(sm_FakeSirenDiscWidth * avgDirection));
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorFakeScenes::StopFakeDistanceSirens()
{
	if(m_DistanceSiren)
	{
		m_DistanceSiren->StopAndForget();
		m_DistanceSiren = NULL;
	}
}
//-------------------------------------------------------------------------------------------------------------------
//														HELPERS	
//-------------------------------------------------------------------------------------------------------------------
#if __BANK  
const char* audVehicleConductorFakeScenes::GetStateName(s32 iState) const
{
	taskAssert(iState >= State_Idle && iState <= State_Idle);
	switch (iState)
	{
	case State_Idle:				return "State_Idle";
	default: taskAssert(0); 	
	}
	return "State_Invalid";
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorFakeScenes::VehicleConductorFSDebugInfo() 
{
	if(audVehicleConductor::GetShowInfo())
	{
		char text[64];
		formatf(text,sizeof(text),"VehicleConductor fake scenes state -> %s", GetStateName(GetState()));
		grcDebugDraw::AddDebugOutput(text);
		formatf(text,sizeof(text),"Time in state -> %f", GetTimeInState());
		grcDebugDraw::AddDebugOutput(text);
	}
	if(sm_ShowDistantSirenInfo && m_DistanceSiren)
	{
		CPed *pPlayer = CGameWorld::FindLocalPlayer();
		if (pPlayer)
		{
			Vector3 position = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()) + Vector3(0.f,0.f,1.3f);
			Vector3 direction = Vector3(m_DistantSirenPos.GetXf(),m_DistantSirenPos.GetYf(),position.z) - position;
			direction.Normalize();
			Vector3 soundPosition = position + direction * 2.f ;
			// Draw disc 
			grcDebugDraw::Circle(position,2.f ,Color_red,Vector3(1.f,0.f,0.f),Vector3(0.f,1.f,0.f));
			grcDebugDraw::Sphere(soundPosition,0.2f,Color_green);
			grcDebugDraw::Line(position,soundPosition,Color_blue);
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorFakeScenes::AddWidgets(bkBank &bank){
	bank.PushGroup("Vehicle Conductor Fake Scenes",false);
	bank.AddToggle("Show distant siren calculation info ", &sm_ShowDistantSirenInfo);
		bank.AddSlider("Vertical distance to play fake sirens",&sm_DistanceFakedSirens,0.f,1000.f,10.f);
		bank.AddSlider("Fake siren disc width",&sm_FakeSirenDiscWidth,0.f,1000.f,10.f);
		bank.AddSlider("Time to move distant siren position (seconds)",&sm_TimeToMoveDistantSiren,0.f,1000.f,10.f);
		bank.AddSlider("sm_MinUpCountryDistThresholdForFakedSirens",&sm_MinUpCountryDistThresholdForFakedSirens,0.f,10000.f,10.f);
		bank.AddSlider("sm_MaxUpCountryDistThresholdForFakedSirens",&sm_MaxUpCountryDistThresholdForFakedSirens,0.f,10000.f,10.f);
		bank.AddSlider("sm_MinUpCityDistThresholdForFakedSirens",&sm_MinUpCityDistThresholdForFakedSirens,0.f,10000.f,10.f);
		bank.AddSlider("sm_MaxUpCityDistThresholdForFakedSirens",&sm_MaxUpCityDistThresholdForFakedSirens,0.f,10000.f,10.f);
	bank.PopGroup();
}
#endif


