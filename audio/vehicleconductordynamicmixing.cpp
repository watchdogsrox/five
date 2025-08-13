#include "audioengine/engine.h"
#include "audioengine/engineutil.h"
#include "debugaudio.h"
#include "grcore/debugdraw.h"

#include "vehicleconductor.h"
#include "vehicleconductordynamicmixing.h"
#include "northaudioengine.h"
#include "Peds/ped.h"
#include "vehicles/vehicle.h"

AUDIO_DYNAMICMIXING_OPTIMISATIONS()

//-------------------------------------------------------------------------------------------------------------------
//														INIT
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorDynamicMixing::Init()
{
	m_LowIntensityScene = NULL;
	m_MediumIntensityScene = NULL;
	m_HighIntensityScene = NULL;
	m_GoodLandingScene = NULL;
	m_BadLandingScene = NULL;
	m_StuntJumpScene = NULL;
	

	SetState(ConductorState_Idle);
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorDynamicMixing::Reset()
{
	if(m_LowIntensityScene)
	{
		StopLowIntensityScene();
	}
	if(m_MediumIntensityScene)
	{
		StopMediumIntensityScene();
	}
	if(m_HighIntensityScene)
	{
		StopHighIntensityScene();
	}
	StopGoodLandingScene();
	StopBadLandingScene();
	StopStuntJumpScene();

	SetState(ConductorState_Idle);
}
//-------------------------------------------------------------------------------------------------------------------
//													DECISION MAKING
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorDynamicMixing::ProcessUpdate()
{
	Update();
	BANK_ONLY(VehicleConductorDMDebugInfo(););
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audVehicleConductorDynamicMixing::UpdateState(const s32 state, const s32, const fwFsm::Event event)
{
	fwFsmBegin
		fwFsmState(ConductorState_Idle)
			fwFsmOnUpdate
			return Idle();	
		fwFsmState(ConductorState_EmphasizeLowIntensity)
			fwFsmOnUpdate
			return ProcessLowIntensity();	
		fwFsmState(ConductorState_EmphasizeMediumIntensity)
			fwFsmOnUpdate
			return ProcessMediumIntensity();	
		fwFsmState(ConductorState_EmphasizeHighIntensity)
			fwFsmOnUpdate
			return ProcessHighIntensity();	
	fwFsmEnd
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audVehicleConductorDynamicMixing::Idle()
{
	return fwFsm::Continue;
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audVehicleConductorDynamicMixing::ProcessLowIntensity()
{
	if(m_MediumIntensityScene)
	{
		StopMediumIntensityScene();
	}
	if(m_HighIntensityScene)
	{
		StopHighIntensityScene();
	}
	if(!m_LowIntensityScene)
	{
		StartLowIntensityScene();
	}
	UpdateLowIntensityMixGroup();
	return fwFsm::Continue;
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audVehicleConductorDynamicMixing::ProcessMediumIntensity()
{
	if(m_LowIntensityScene)
	{
		StopLowIntensityScene();
	}
	if(m_HighIntensityScene)
	{
		StopHighIntensityScene();
	}
	if(!m_MediumIntensityScene)
	{
		StartMediumIntensityScene();
	}
	UpdateMediumIntensityMixGroup();
	return fwFsm::Continue;
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audVehicleConductorDynamicMixing::ProcessHighIntensity()
{
	if(m_LowIntensityScene)
	{
		StopLowIntensityScene();
	}
	if(m_MediumIntensityScene)
	{
		StopMediumIntensityScene();
	}
	if(!m_HighIntensityScene)
	{
		StartHighIntensityScene();
	}
	UpdateHighIntensityMixGroup();
	return fwFsm::Continue;
}
//-------------------------------------------------------------------------------------------------------------------
//														HELPERS	
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorDynamicMixing::ClearDynamicMixJobForVehicle(CVehicle *pVeh)
{
	if(naVerifyf(pVeh,"Null veh ref. when cleaning DM job."))
	{
		DYNAMICMIXER.RemoveEntityFromMixGroup((CEntity *)pVeh, 2.f);
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorDynamicMixing::StartLowIntensityScene()
{
	// Start the scene if we haven't do it already.
	MixerScene * sceneSettings  = DYNAMICMIXMGR.GetObject<MixerScene>(ATSTRINGHASH("VEH_LOW_INTENSITY_SCENE", 0xF635521A)); 
	if(sceneSettings)
	{
		DYNAMICMIXER.StartScene(sceneSettings, &m_LowIntensityScene, NULL);
	}
	naAssertf(m_LowIntensityScene,"Failed when trying to initialize the low intensity scene.");
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorDynamicMixing::StopLowIntensityScene()
{
	m_LowIntensityScene->Stop();
	m_LowIntensityScene = NULL;
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorDynamicMixing::UpdateLowIntensityMixGroup()
{
	////Deemphasize gunfire for all peds in combat, do it every frame because the main conductor is calculating the peds every frame. 
	//static const audStringHash mixGroup("CHASING_VEHICLES");
	//for(s32 vehIndex = 0; vehIndex < SUPERCONDUCTOR.GetVehicleConductor().GetNumberOfChasingVeh(); vehIndex++)
	//{
	//	CVehicle *pVeh = SUPERCONDUCTOR.GetVehicleConductor().GetChasingVehicle(vehIndex);
	//	if(naVerifyf(pVeh,"Null veh ref. when adding to its mix group.") && !pVeh->GetVehicleAudioEntity()->GetWasChasing())
	//	{
	//		DYNAMICMIXER.AddEntityToMixGroup((CEntity *)pVeh,mixGroup);
	//	}
	//}
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorDynamicMixing::StartMediumIntensityScene()
{
	// Start the scene if we haven't do it already.
	MixerScene * sceneSettings  = DYNAMICMIXMGR.GetObject<MixerScene>(ATSTRINGHASH("VEH_MEDIUM_INTENSITY_SCENE", 0xAF5ED1E0)); 
	if(sceneSettings)
	{
		DYNAMICMIXER.StartScene(sceneSettings, &m_MediumIntensityScene, NULL);
	}
	naAssertf(m_MediumIntensityScene,"Failed when trying to initialize the low intensity scene.");
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorDynamicMixing::StopMediumIntensityScene()
{
	m_MediumIntensityScene->Stop();
	m_MediumIntensityScene = NULL;
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorDynamicMixing::UpdateMediumIntensityMixGroup()
{ 
	////Deemphasize gunfire for all peds in combat, do it every frame because the main conductor is calculating the peds every frame. 
	//static const audStringHash mixGroup("CHASING_VEHICLES");
	//for(s32 vehIndex = 0; vehIndex < SUPERCONDUCTOR.GetVehicleConductor().GetNumberOfChasingVeh(); vehIndex++)
	//{
	//	CVehicle *pVeh = SUPERCONDUCTOR.GetVehicleConductor().GetChasingVehicle(vehIndex);
	//	if(naVerifyf(pVeh,"Null veh ref. when adding to its mix group.") && !SUPERCONDUCTOR.GetVehicleConductor().IsTarget(pVeh))
	//	{
	//		if(pVeh->GetVehicleAudioEntity()->GetWasTarget() || !pVeh->GetVehicleAudioEntity()->GetWasChasing())
	//		{
	//			DYNAMICMIXER.AddEntityToMixGroupInTime((CEntity *)pVeh,mixGroup,0.7f);
	//		}
	//	}else if( !pVeh->GetVehicleAudioEntity()->GetWasTarget() )
	//	{
	//		DYNAMICMIXER.RemoveEntityFromMixGroup((CEntity *)pVeh);
	//	}
	//}
} 
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorDynamicMixing::StartHighIntensityScene()
{
	// Start the scene if we haven't do it already.
	MixerScene * sceneSettings  = DYNAMICMIXMGR.GetObject<MixerScene>(ATSTRINGHASH("VEH_HIGH_INTENSITY_SCENE", 0xFB33B647)); 
	if(sceneSettings)
	{
		DYNAMICMIXER.StartScene(sceneSettings, &m_HighIntensityScene, NULL);
	}
	naAssertf(m_HighIntensityScene,"Failed when trying to initialize the low intensity scene.");
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorDynamicMixing::StopHighIntensityScene()
{
	m_HighIntensityScene->Stop();
	m_HighIntensityScene = NULL;
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorDynamicMixing::UpdateHighIntensityMixGroup()
{
	////Deemphasize gunfire for all peds in combat, do it every frame because the main conductor is calculating the peds every frame. 
	//static const audStringHash nonTargetsMixGroup("CHASING_VEHICLES");
	//static const audStringHash targetsMixGroup("VEH_TARGETS");
	//for(s32 vehIndex = 0; vehIndex < SUPERCONDUCTOR.GetVehicleConductor().GetNumberOfChasingVeh(); vehIndex++)
	//{
	//	CVehicle *pVeh = SUPERCONDUCTOR.GetVehicleConductor().GetChasingVehicle(vehIndex);
	//	if(naVerifyf(pVeh,"Null veh ref. when adding to its mix group.")/* && !SUPERCONDUCTOR.GetVehicleConductor().IsTarget(pVeh)*/)
	//	{
	//		if(/*pVeh->GetVehicleAudioEntity()->GetWasTarget() || */!pVeh->GetVehicleAudioEntity()->GetWasChasing())
	//		{
	//			//DYNAMICMIXER.RemoveEntityFromMixGroup((CEntity *)pVeh);
	//			DYNAMICMIXER.AddEntityToMixGroupInTime((CEntity *)pVeh,nonTargetsMixGroup, 2.f);
	//		}
	//	}/*else if( !pVeh->GetVehicleAudioEntity()->GetWasTarget() )
	//	{
	//		DYNAMICMIXER.AddEntityToMixGroup((CEntity *)pVeh,targetsMixGroup);
	//	}*/
	//}
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorDynamicMixing::StartGoodLandingScene()
{
	if(!m_GoodLandingScene)
	{
		// Start the scene if we haven't do it already.
		MixerScene * sceneSettings  = NULL;
		if(NetworkInterface::IsGameInProgress())
		{
			sceneSettings  = DYNAMICMIXMGR.GetObject<MixerScene>(ATSTRINGHASH("MP_STUNT_JUMP_GOOD_LANDING_SCENE", 0xA1EEC4DB)); 
		}
		else
		{
			sceneSettings  = DYNAMICMIXMGR.GetObject<MixerScene>(ATSTRINGHASH("STUNT_JUMP_GOOD_LANDING_SCENE", 0xF2AE1AA0)); 
		}
		if(sceneSettings)
		{
			DYNAMICMIXER.StartScene(sceneSettings, &m_GoodLandingScene, NULL);
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorDynamicMixing::StartBadLandingScene()
{
	if(!m_BadLandingScene)
	{
		// Start the scene if we haven't do it already.
		MixerScene * sceneSettings  = NULL;
		if(NetworkInterface::IsGameInProgress())
		{
			sceneSettings  = DYNAMICMIXMGR.GetObject<MixerScene>(ATSTRINGHASH("MP_STUNT_JUMP_BAD_LANDING_SCENE", 0x920D00E8)); 
		}
		else
		{
			sceneSettings  = DYNAMICMIXMGR.GetObject<MixerScene>(ATSTRINGHASH("STUNT_JUMP_BAD_LANDING_SCENE", 0xBFBB33FA)); 
		}
		if(sceneSettings)
		{
			DYNAMICMIXER.StartScene(sceneSettings, &m_BadLandingScene, NULL);
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorDynamicMixing::StartStuntJumpScene()
{
	if(!m_StuntJumpScene)
	{
		// Start the scene if we haven't do it already.
		MixerScene * sceneSettings  = NULL;
		if(NetworkInterface::IsGameInProgress())
		{
			sceneSettings  = DYNAMICMIXMGR.GetObject<MixerScene>(ATSTRINGHASH("MP_STUNT_JUMP_SCENE", 0x8B63B032)); 
		}
		if(sceneSettings)
		{
			DYNAMICMIXER.StartScene(sceneSettings, &m_StuntJumpScene, NULL);
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorDynamicMixing::StopGoodLandingScene()
{
	if(m_GoodLandingScene)
	{
		m_GoodLandingScene->Stop();
		m_GoodLandingScene = NULL;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorDynamicMixing::StopBadLandingScene()
{
	if(m_BadLandingScene)
	{
		m_BadLandingScene->Stop();
		m_BadLandingScene = NULL;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorDynamicMixing::StopStuntJumpScene()
{
	if(m_StuntJumpScene)
	{
		m_StuntJumpScene->Stop();
		m_StuntJumpScene = NULL;
	}
}
//-------------------------------------------------------------------------------------------------------------------
#if __BANK
const char* audVehicleConductorDynamicMixing::GetStateName(s32 iState) const
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
void audVehicleConductorDynamicMixing::VehicleConductorDMDebugInfo() 
{
	if(audVehicleConductor::GetShowInfo())
	{
		char text[64];
		formatf(text,sizeof(text),"VehilceConductor DynamicMixing state -> %s", GetStateName(GetState()));
		grcDebugDraw::AddDebugOutput(text);
		formatf(text,sizeof(text),"Time in state -> %f", GetTimeInState());
		grcDebugDraw::AddDebugOutput(text);
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductorDynamicMixing::AddWidgets(bkBank& bank) 
{
	bank.PushGroup("Vehicle Conductor Dynamic Mixing",false);
	bank.PopGroup();
}
#endif
