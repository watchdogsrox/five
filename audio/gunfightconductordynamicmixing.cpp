#include "audioengine/engine.h"
#include "audioengine/engineutil.h"
#include "debugaudio.h"
#include "grcore/debugdraw.h"

#include "gunfightconductor.h"
#include "gunfightconductordynamicmixing.h"
#include "northaudioengine.h"
#include "Peds/ped.h"

AUDIO_DYNAMICMIXING_OPTIMISATIONS()

f32 audGunFightConductorDynamicMixing::sm_NonTargetsLowVolOffset = 0.f;
f32 audGunFightConductorDynamicMixing::sm_NonTargetsMedVolOffset = -1.5f;
f32 audGunFightConductorDynamicMixing::sm_NonTargetsHighVolOffset = -3.f; 
f32 audGunFightConductorDynamicMixing::sm_TargetsLowVolOffset = -2.f;
f32 audGunFightConductorDynamicMixing::sm_TargetsMedVolOffset = 0.f;
f32 audGunFightConductorDynamicMixing::sm_TargetsHighVolOffset = 0.f;

//-------------------------------------------------------------------------------------------------------------------
//														INIT
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorDynamicMixing::Init()
{
	m_GunfightScene = NULL;
	m_BulletImpactsScene = NULL;
	m_GunfightApplySmoother.Init(0.001f,true);

	SetState(ConductorState_Idle);
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorDynamicMixing::Reset()
{
	if(m_GunfightScene)
	{
		StopGunfightScene();
	}
	DeemphasizeBulletImpacts();
	for(s32 pedIndex = 0; pedIndex < SUPERCONDUCTOR.GetGunFightConductor().GetNumberOfPedsInCombat(); pedIndex++)
	{
		CPed *pPed = SUPERCONDUCTOR.GetGunFightConductor().GetPedInCombat(pedIndex);
		if(pPed)
		{
			DYNAMICMIXER.RemoveEntityFromMixGroup((CEntity *)pPed);
		}
	}
	SetState(ConductorState_Idle);
}
//-------------------------------------------------------------------------------------------------------------------
//													DECISION MAKING
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorDynamicMixing::ProcessUpdate()
{
	Update();
	BANK_ONLY(GunfightConductorDMDebugInfo();)
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audGunFightConductorDynamicMixing::UpdateState(const s32 state, const s32, const fwFsm::Event event)
{
	fwFsmBegin
		fwFsmState(ConductorState_Idle)
			fwFsmOnUpdate
			return Idle();	
		fwFsmState(ConductorState_EmphasizeLowIntensity)
			fwFsmOnUpdate
			return ProcessLowIntensityDM();	
		fwFsmState(ConductorState_EmphasizeMediumIntensity)
			fwFsmOnUpdate
			return ProcessMediumIntensityDM();	
		fwFsmState(ConductorState_EmphasizeHighIntensity)
			fwFsmOnUpdate
			return ProcessHighIntensityDM();	
	fwFsmEnd
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audGunFightConductorDynamicMixing::Idle()
{
	if(m_GunfightScene)
	{
		StopGunfightScene();
	}
	return fwFsm::Continue;
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audGunFightConductorDynamicMixing::ProcessLowIntensityDM()
{
	if(!m_GunfightScene)
	{
		StartGunfightScene();
	}
	if(m_GunfightScene)
	{
		m_GunfightScene->SetVariableValue(ATSTRINGHASH("apply", 0xE865CDE8),m_GunfightApplySmoother.CalculateValue(0.f,audNorthAudioEngine::GetCurrentTimeInMs()));
	}
	UpdateGunfightMixing(sm_NonTargetsLowVolOffset,sm_NonTargetsLowVolOffset);
	return fwFsm::Continue;
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audGunFightConductorDynamicMixing::ProcessMediumIntensityDM()
{
	if(!m_GunfightScene)
	{
		StartGunfightScene();
	}
	if(m_GunfightScene)
	{
		m_GunfightScene->SetVariableValue(ATSTRINGHASH("apply", 0xE865CDE8),m_GunfightApplySmoother.CalculateValue(1.f,audNorthAudioEngine::GetCurrentTimeInMs()));
	}
	UpdateGunfightMixing(sm_NonTargetsMedVolOffset,sm_TargetsMedVolOffset);
	return fwFsm::Continue;
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audGunFightConductorDynamicMixing::ProcessHighIntensityDM()
{
	if(!m_GunfightScene)
	{
		StartGunfightScene();
	}
	if(m_GunfightScene)
	{
		m_GunfightScene->SetVariableValue(ATSTRINGHASH("apply", 0xE865CDE8),m_GunfightApplySmoother.CalculateValue(1.f,audNorthAudioEngine::GetCurrentTimeInMs()));
	}
	UpdateGunfightMixing(sm_NonTargetsHighVolOffset,sm_TargetsHighVolOffset);
	return fwFsm::Continue;
}
//-------------------------------------------------------------------------------------------------------------------
//														HELPERS	
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorDynamicMixing::EmphasizeBulletImpacts()
{
	if (!m_BulletImpactsScene)
	{
		// Start the scene if we haven't do it already.
		MixerScene * sceneSettings  = DYNAMICMIXMGR.GetObject<MixerScene>(ATSTRINGHASH("EMPHASIZE_BULLET_IMPACTS_SCENE", 0x748299FE)); 
		if(sceneSettings)
		{
			DYNAMICMIXER.StartScene(sceneSettings, &m_BulletImpactsScene, NULL);
		}
		naAssertf(m_BulletImpactsScene,"Failed when trying to initialize the low intensity scene.");
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorDynamicMixing::DeemphasizeBulletImpacts()
{
	if (m_BulletImpactsScene)
	{	
		m_BulletImpactsScene->Stop();
		m_BulletImpactsScene = NULL;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorDynamicMixing::StartGunfightScene()
{
	// Start the scene if we haven't do it already.
	MixerScene * sceneSettings  = DYNAMICMIXMGR.GetObject<MixerScene>(ATSTRINGHASH("GUNFIGHT_CONDUCTOR_SCENE", 0x6ABD4DD3)); 
	if(sceneSettings)
	{
		DYNAMICMIXER.StartScene(sceneSettings, &m_GunfightScene, NULL);
	}
	naAssertf(m_GunfightScene,"Failed when trying to initialize the low intensity scene.");
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorDynamicMixing::StopGunfightScene()
{
	for(s32 pedIndex = 0; pedIndex < SUPERCONDUCTOR.GetGunFightConductor().GetNumberOfPedsInCombat(); pedIndex++)
	{
		CPed *pPed = SUPERCONDUCTOR.GetGunFightConductor().GetPedInCombat(pedIndex);
		if(naVerifyf(pPed,"Null ped ref. "))
		{
			pPed->GetPedAudioEntity()->SetGunfightVolumeOffset(0.f); 
			pPed->GetPedAudioEntity()->SetWasInCombat(false);
			pPed->GetPedAudioEntity()->SetWasTarget(false);
		}
	}
	m_GunfightScene->Stop();
	m_GunfightScene = NULL;
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorDynamicMixing::UpdateGunfightMixing(f32 volumeOffset, f32 targetVolOffset)
{
	for(s32 pedIndex = 0; pedIndex < SUPERCONDUCTOR.GetGunFightConductor().GetNumberOfPedsInCombat(); pedIndex++)
	{
		CPed *pPed = SUPERCONDUCTOR.GetGunFightConductor().GetPedInCombat(pedIndex);
		if(pPed && !SUPERCONDUCTOR.GetGunFightConductor().IsTarget(pPed))
		{
			if(pPed->GetPedAudioEntity()->GetWasTarget() || !pPed->GetPedAudioEntity()->GetWasInCombat())
			{
				pPed->GetPedAudioEntity()->SetGunfightVolumeOffset(volumeOffset); 
			}
		}else if(pPed && !pPed->GetPedAudioEntity()->GetWasTarget() )
		{
			pPed->GetPedAudioEntity()->SetGunfightVolumeOffset(targetVolOffset); 
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
#if __BANK
const char* audGunFightConductorDynamicMixing::GetStateName(s32 iState) const
{
	taskAssert(iState >= ConductorState_Idle && iState <= ConductorState_EmphasizeHighIntensity);
	switch (iState)
	{
	case ConductorState_Idle:				return "ConductorState_Idle";
	case ConductorState_EmphasizeLowIntensity:		return "ConductorState_EmphasizeLowIntensity";		
	case ConductorState_EmphasizeMediumIntensity:	return "ConductorState_EmphasizeMediumIntensity";	
	case ConductorState_EmphasizeHighIntensity:		return "ConductorState_EmphasizeHighIntensity";		
	default: taskAssert(0); 	
	}
	return "ConductorState_Invalid";
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorDynamicMixing::GunfightConductorDMDebugInfo() 
{
	if(audGunFightConductor::GetShowInfo())
	{
		char text[64];
		formatf(text,"GunfightConductor DynamicMixing state -> %s", GetStateName(GetState()));
		grcDebugDraw::AddDebugOutput(text);
		formatf(text,"Time in state -> %f", GetTimeInState());
		grcDebugDraw::AddDebugOutput(text);
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorDynamicMixing::AddWidgets(bkBank &bank){
	bank.PushGroup("GunFight Conductor Dynamic Mixing",false);
	bank.AddSlider("All peds low intensity vol (db) offset", &sm_NonTargetsLowVolOffset, -100.0f, 24.f, 1.f);
	bank.AddSlider("Non-targets medium intensity vol (db) offset", &sm_NonTargetsMedVolOffset, -100.0f, 24.f, 1.f);
	bank.AddSlider("Non-targets high intensity vol (db) offset", &sm_NonTargetsHighVolOffset, -100.0f, 24.f, 1.f);
	//bank.AddSlider("targets low intensity vol (db) offset", &sm_TargetsLowVolOffset, -100.0f, 24.f, 1.f);
	bank.AddSlider("targets medium intensity vol (db) offset", &sm_TargetsMedVolOffset, -100.0f, 24.f, 1.f);
	bank.AddSlider("targets high intensity vol (db) offset", &sm_TargetsHighVolOffset, -100.0f, 24.f, 1.f);
	bank.PopGroup();
}
#endif
