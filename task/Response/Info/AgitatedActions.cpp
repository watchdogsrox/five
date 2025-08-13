// File header
#include "Task/Response/Info/AgitatedActions.h"

// Game headers
#include "ai/AITarget.h"
#include "ai/ambient/ConditionalAnimManager.h"
#include "event/EventShocking.h"
#include "event/Events.h"
#include "Peds/PedIntelligence.h"
#include "task/Combat/TaskNewCombat.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "task/Default/TaskAmbient.h"
#include "task/General/TaskBasic.h"
#include "task/General/Phone/TaskCallPolice.h"
#include "Task/Movement/TaskIdle.h"
#include "task/Movement/TaskSeekEntity.h"
#include "Task/Response/Info/AgitatedConditions.h"
#include "Task/Response/TaskAgitated.h"
#include "Task/Response/TaskConfront.h"
#include "Task/Response/TaskFlee.h"
#include "Task/Response/TaskReactAndFlee.h"
#include "task/Response/TaskShockingEvents.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "task/Vehicle/TaskCar.h"
#include "task/Vehicle/TaskEnterVehicle.h"
#include "task/Vehicle/TaskExitVehicle.h"
#include "task/Vehicle/TaskVehicleWeapon.h"
#include "vehicleAi/task/TaskVehiclePark.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

INSTANTIATE_RTTI_CLASS(CAgitatedAction,0x2C2EDCAC);
INSTANTIATE_RTTI_CLASS(CAgitatedActionConditional,0xADF7242F);
INSTANTIATE_RTTI_CLASS(CAgitatedActionMulti,0x180EA47C);
INSTANTIATE_RTTI_CLASS(CAgitatedActionChangeResponse,0x4A5D9577);
INSTANTIATE_RTTI_CLASS(CAgitatedActionTask,0x2C57776E);
INSTANTIATE_RTTI_CLASS(CAgitatedActionAnger,0xED16DA84);
INSTANTIATE_RTTI_CLASS(CAgitatedActionApplyAgitation,0x5CFA11D3);
INSTANTIATE_RTTI_CLASS(CAgitatedActionCallPolice,0x7BD838B7);
INSTANTIATE_RTTI_CLASS(CAgitatedActionClearHash,0x130F65BA);
INSTANTIATE_RTTI_CLASS(CAgitatedActionConfront,0xD7EC972A);
INSTANTIATE_RTTI_CLASS(CAgitatedActionEnterVehicle,0x29A5E47F);
INSTANTIATE_RTTI_CLASS(CAgitatedActionExitVehicle,0x72BD4707);
INSTANTIATE_RTTI_CLASS(CAgitatedActionFace,0xD3C11997);
INSTANTIATE_RTTI_CLASS(CAgitatedActionFear,0x209d2488);
INSTANTIATE_RTTI_CLASS(CAgitatedActionFight,0x7B726E6D);
INSTANTIATE_RTTI_CLASS(CAgitatedActionFlee,0xE320A0C9);
INSTANTIATE_RTTI_CLASS(CAgitatedActionFlipOff,0x454433CF);
INSTANTIATE_RTTI_CLASS(CAgitatedActionFollow,0xC0FE7CA);
INSTANTIATE_RTTI_CLASS(CAgitatedActionHurryAway,0x2d43e253);
INSTANTIATE_RTTI_CLASS(CAgitatedActionIgnoreForcedAudioFailures,0xCBD46799);
INSTANTIATE_RTTI_CLASS(CAgitatedActionMakeAggressiveDriver,0x7F5CEC5B);
INSTANTIATE_RTTI_CLASS(CAgitatedActionReportCrime,0x32AB4312);
INSTANTIATE_RTTI_CLASS(CAgitatedActionReset,0x74CBB203);
INSTANTIATE_RTTI_CLASS(CAgitatedActionSay,0x8790C5F2);
INSTANTIATE_RTTI_CLASS(CAgitatedActionSayAgitator,0x97C56F92);
INSTANTIATE_RTTI_CLASS(CAgitatedActionSetFleeAmbientClips,0x2C58DA56);
INSTANTIATE_RTTI_CLASS(CAgitatedActionSetFleeMoveBlendRatio,0x487BD872);
INSTANTIATE_RTTI_CLASS(CAgitatedActionSetHash,0xD05A8590);
INSTANTIATE_RTTI_CLASS(CAgitatedActionStopVehicle,0xC88FE69F);
INSTANTIATE_RTTI_CLASS(CAgitatedActionTurnOnSiren,0xF7962DD6);
INSTANTIATE_RTTI_CLASS(CAgitatedScenarioExit,0xBFDDBB15);
INSTANTIATE_RTTI_CLASS(CAgitatedScenarioNormalCowardExit,0x750416C1);
INSTANTIATE_RTTI_CLASS(CAgitatedScenarioFastCowardExit,0x2E796418);
INSTANTIATE_RTTI_CLASS(CAgitatedScenarioFleeExit,0xA49D9C98);
INSTANTIATE_RTTI_CLASS(CAgitatedSetWaterSurvivalTime, 0x2B4D5B01);

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionConditional
////////////////////////////////////////////////////////////////////////////////

CAgitatedActionConditional::CAgitatedActionConditional()
: m_Condition(NULL)
, m_Action(NULL)
{
}

CAgitatedActionConditional::~CAgitatedActionConditional()
{
	delete m_Condition;
	delete m_Action;
}

void CAgitatedActionConditional::Execute(const CAgitatedActionContext& rContext) const
{
	//Check if the condition is valid.
	if(m_Condition)
	{
		//Ensure the condition is valid.
		CAgitatedConditionContext context;
		context.m_Task = rContext.m_Task;
		if(!m_Condition->Check(context))
		{
			return;
		}
	}

	//Execute the action.
	m_Action->Execute(rContext);
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionMulti
////////////////////////////////////////////////////////////////////////////////

CAgitatedActionMulti::CAgitatedActionMulti()
: CAgitatedAction()
, m_Actions()
{
}

CAgitatedActionMulti::~CAgitatedActionMulti()
{
	//Free the actions.
	for(s32 i = 0; i < m_Actions.GetCount(); i++)
	{
		delete m_Actions[i];
	}
}

void CAgitatedActionMulti::Execute(const CAgitatedActionContext& rContext) const
{
	//Execute the actions.
	for(s32 i = 0; i < m_Actions.GetCount(); i++)
	{
		m_Actions[i]->Execute(rContext);
	}
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionChangeResponse
////////////////////////////////////////////////////////////////////////////////

CAgitatedActionChangeResponse::CAgitatedActionChangeResponse()
: CAgitatedAction()
, m_Response()
, m_State()
{

}

CAgitatedActionChangeResponse::~CAgitatedActionChangeResponse()
{

}

void CAgitatedActionChangeResponse::Execute(const CAgitatedActionContext& rContext) const
{
	rContext.m_Task->ChangeResponse(m_Response, m_State);
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionTask
////////////////////////////////////////////////////////////////////////////////

CAgitatedActionTask::CAgitatedActionTask()
: CAgitatedAction()
{

}

CAgitatedActionTask::~CAgitatedActionTask()
{

}

void CAgitatedActionTask::Execute(const CAgitatedActionContext& rContext) const
{
	//Create the task.
	CTask* pTask = CreateTask(rContext);
	if(!aiVerifyf(pTask, "Task is invalid."))
	{
		return;
	}
	
	//Change the task.
	rContext.m_Task->ChangeTask(pTask);
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionAnger
////////////////////////////////////////////////////////////////////////////////

void CAgitatedActionAnger::Execute(const CAgitatedActionContext& rContext) const
{
	//Check if the set is valid.
	if(m_Set >= 0.0f)
	{
		//Set the anger.
		rContext.m_Task->SetAnger(m_Set);
	}

	//Check if the add is valid.
	if(m_Add >= 0.0f)
	{
		//Add the anger.
		rContext.m_Task->AddAnger(m_Add);
	}

	//Check if the min is valid.
	if(m_Min >= 0.0f)
	{
		//Set the min anger.
		rContext.m_Task->SetMinAnger(m_Min);
	}

	//Check if the max is valid.
	if(m_Max >= 0.0f)
	{
		//Set the max anger.
		rContext.m_Task->SetMaxAnger(m_Max);
	}
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionApplyAgitation
////////////////////////////////////////////////////////////////////////////////

void CAgitatedActionApplyAgitation::Execute(const CAgitatedActionContext& rContext) const
{
	rContext.m_Task->ApplyAgitation(m_Type);
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionCallPolice
////////////////////////////////////////////////////////////////////////////////

CTask* CAgitatedActionCallPolice::CreateTask(const CAgitatedActionContext& rContext) const
{
	//Create the task.
	CTaskCallPolice* pTask = rage_new CTaskCallPolice(rContext.m_Task->GetAgitator());

	return pTask;
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionClearHash
////////////////////////////////////////////////////////////////////////////////

void CAgitatedActionClearHash::Execute(const CAgitatedActionContext& rContext) const
{
	rContext.m_Task->ClearHash(m_Value);
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionConfront
////////////////////////////////////////////////////////////////////////////////

CAgitatedActionConfront::CAgitatedActionConfront()
: CAgitatedActionTask()
, m_AmbientClips()
{

}

CAgitatedActionConfront::~CAgitatedActionConfront()
{

}

CTask* CAgitatedActionConfront::CreateTask(const CAgitatedActionContext& rContext) const
{
	//Create the task.
	CTaskConfront* pTask = rage_new CTaskConfront(rContext.m_Task->GetAgitator());
	
	//Set the ambient clips.
	pTask->SetAmbientClips(m_AmbientClips);
	
	return pTask;
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionEnterVehicle
////////////////////////////////////////////////////////////////////////////////

CAgitatedActionEnterVehicle::CAgitatedActionEnterVehicle()
: CAgitatedActionTask()
{

}

CAgitatedActionEnterVehicle::~CAgitatedActionEnterVehicle()
{

}

CTask* CAgitatedActionEnterVehicle::CreateTask(const CAgitatedActionContext& rContext) const
{
	//Grab the ped.
	const CPed* pPed = rContext.m_Task->GetPed();

	//Ensure the vehicle is valid.
	CVehicle* pVehicle = pPed->GetMyVehicle();
	if(!aiVerifyf(pVehicle, "The vehicle is invalid."))
	{
		return NULL;
	}

	//Create the task.
	CTask* pTask = rage_new CTaskEnterVehicle(pVehicle, SR_Specific, Seat_driver);

	return pTask;
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionExitVehicle
////////////////////////////////////////////////////////////////////////////////

CAgitatedActionExitVehicle::CAgitatedActionExitVehicle()
: CAgitatedActionTask()
{

}

CAgitatedActionExitVehicle::~CAgitatedActionExitVehicle()
{

}

CTask* CAgitatedActionExitVehicle::CreateTask(const CAgitatedActionContext& rContext) const
{
	//Grab the ped.
	const CPed* pPed = rContext.m_Task->GetPed();
	
	//Ensure the vehicle is valid.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!aiVerifyf(pVehicle, "The vehicle is invalid."))
	{
		return NULL;
	}
	
	//Create the task.
	VehicleEnterExitFlags flags;
	CTask* pTask = rage_new CTaskExitVehicle(pVehicle, flags);

	return pTask;
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionFace
////////////////////////////////////////////////////////////////////////////////

CAgitatedActionFace::CAgitatedActionFace()
: CAgitatedActionTask()
, m_AmbientClips()
{

}

CAgitatedActionFace::~CAgitatedActionFace()
{

}

CTask* CAgitatedActionFace::CreateTask(const CAgitatedActionContext& rContext) const
{
	//Create the move task.
	CTaskMoveFaceTarget* pMoveTask = rage_new CTaskMoveFaceTarget(CAITarget(rContext.m_Task->GetAgitator()));
	pMoveTask->SetLimitUpdateBasedOnDistance(true);

	//Create the sub-task.
	CTask* pSubTask = NULL;
	if(m_AmbientClips.IsNotNull())
	{
		pSubTask = rage_new CTaskAmbientClips(CTaskAmbientClips::Flag_PlayBaseClip|CTaskAmbientClips::Flag_PlayIdleClips|CTaskAmbientClips::Flag_ForceFootClipSetRestart,
			CONDITIONALANIMSMGR.GetConditionalAnimsGroup(m_AmbientClips));
	}

	//Create the task.
	CTask* pTask = rage_new CTaskComplexControlMovement(pMoveTask, pSubTask);

	return pTask;
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionFear
////////////////////////////////////////////////////////////////////////////////

void CAgitatedActionFear::Execute(const CAgitatedActionContext& rContext) const
{
	//Check if the set is valid.
	if(m_Set >= 0.0f)
	{
		//Set the anger.
		rContext.m_Task->SetFear(m_Set);
	}

	//Check if the add is valid.
	if(m_Add >= 0.0f)
	{
		//Add the anger.
		rContext.m_Task->AddFear(m_Add);
	}

	//Check if the min is valid.
	if(m_Min >= 0.0f)
	{
		//Set the min anger.
		rContext.m_Task->SetMinFear(m_Min);
	}

	//Check if the max is valid.
	if(m_Max >= 0.0f)
	{
		//Set the max anger.
		rContext.m_Task->SetMaxFear(m_Max);
	}
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionFight
////////////////////////////////////////////////////////////////////////////////

CAgitatedActionFight::CAgitatedActionFight()
: CAgitatedActionTask()
{

}

CAgitatedActionFight::~CAgitatedActionFight()
{

}

CTask* CAgitatedActionFight::CreateTask(const CAgitatedActionContext& rContext) const
{
	//Grab the ped.
	CPed* pPed = rContext.m_Task->GetPed();

	//Grab the agitator.
	const CPed* pAgitator = rContext.m_Task->GetAgitator();

	//Create the task.
	CTaskThreatResponse* pTask = rage_new CTaskThreatResponse(pAgitator);
	
	//Always fight.
	pTask->SetThreatResponseOverride(CTaskThreatResponse::TR_Fight);

	//Check if the ped is a cop.
	if(pPed->IsRegularCop() && pAgitator->IsAPlayerPed() && !NetworkInterface::IsGameInProgress())
	{
		CVehicle* pTargetVehicle = pAgitator->GetVehiclePedInside();
		if(!pTargetVehicle || (!pTargetVehicle->GetLayoutInfo()->GetDisableJackingAndBusting() && !pTargetVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DISABLE_BUSTING)))
		{
			//Arrest the agitator.
			pTask->GetConfigFlagsForCombat().SetFlag(CTaskCombat::ComF_ArrestTarget);
		}
	}
	
	return pTask;
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionFlee
////////////////////////////////////////////////////////////////////////////////

CAgitatedActionFlee::CAgitatedActionFlee()
: CAgitatedActionTask()
, m_FleeStopDistance(0.0f)
, m_MoveBlendRatio(0.0f)
, m_ForcePreferPavements(false)
, m_NeverLeavePavements(false)
, m_KeepMovingWhilstWaitingForFleePath(false)
, m_AmbientClips()
, m_Flags(0)
{

}

CAgitatedActionFlee::~CAgitatedActionFlee()
{

}

CTask* CAgitatedActionFlee::CreateTask(const CAgitatedActionContext& rContext) const
{
	CTaskAgitated* pAgitatedTask = rContext.m_Task;
	CPed* pAgitator = pAgitatedTask->GetAgitator();

	u16 uFlags = 0;

	//Don't scream.  The agitated system handles the speech.
	uFlags |= CTaskSmartFlee::CF_DontScream;

	//Use the last vehicle we were inside, if it's closer.
	uFlags |= CTaskSmartFlee::CF_EnterLastUsedVehicleImmediatelyIfCloserThanTarget;

	//Check if we are in a vehicle.
	if(rContext.m_Task->GetPed()->GetIsInVehicle())
	{
		//Allow us to reverse/accelerate.
		uFlags |= CTaskSmartFlee::CF_ReverseInVehicle;
		uFlags |= CTaskSmartFlee::CF_AccelerateInVehicle;

		//Don't exit the vehicle.
		uFlags |= CTaskSmartFlee::CF_DisableExitVehicle;
	}

	//Never enter water.
	uFlags |= CTaskSmartFlee::CF_NeverEnterWater;

	if (pAgitatedTask->IsHostile())
	{
		//If the agitator was recently hostile, use threat response instead of smart flee so we get the reaction animations.
		CTaskThreatResponse* pTaskThreatResponse = rage_new CTaskThreatResponse(pAgitatedTask->GetAgitator());
		
		//Maintain the desired mbr.
		pTaskThreatResponse->SetMoveBlendRatioForFlee(m_MoveBlendRatio);
		
		//Maintain the relevant flee config flags.
		pTaskThreatResponse->SetConfigFlagsForFlee(uFlags);

		//Force flee so we don't accidentally choose combat.
		pTaskThreatResponse->SetThreatResponseOverride(CTaskThreatResponse::TR_Flee);

		//The rest of the configuration options below for SmartFlee don't apply to a hostile flee, so return the task.
		return pTaskThreatResponse;
	}
	else
	{
		//Create the task.
		CTaskSmartFlee* pTask = rage_new CTaskSmartFlee(CAITarget(pAgitator), m_FleeStopDistance);
		pTask->SetConfigFlags(uFlags);

		//Set the move/blend ratio.
		pTask->SetMoveBlendRatio(m_MoveBlendRatio);

		//Set the force prefer pavements.
		pTask->SetForcePreferPavements(m_ForcePreferPavements);

		//Set the never leave pavements.
		pTask->SetNeverLeavePavements(m_NeverLeavePavements);

		//Keep moving while waiting for the path... but only if we're moving.
		if(m_KeepMovingWhilstWaitingForFleePath)
		{
			//Check if we have exceeded the minimum speed.
			float fSpeedSq = rContext.m_Task->GetPed()->GetVelocity().XYMag2();
			static dev_float s_fMinSpeed = 0.5f;
			float fMinSpeedSq = square(s_fMinSpeed);
			if(fSpeedSq >= fMinSpeedSq)
			{
				pTask->SetKeepMovingWhilstWaitingForFleePath(true);
			}
		}

		pTask->SetConsiderRunStartForPathLookAhead(true);

		//Set the ambient clips.
		pTask->SetAmbientClips(m_AmbientClips);

		//Check if we can load the ambient clips info.
		AmbientClipsInfo info;
		if(LoadAmbientClipsInfo(*rContext.m_Task->GetPed(), info))
		{
			//Check if we should claim the prop.
			bool bClaimProp = false;
			if(info.m_bDestroyPropInsteadOfDrop && m_Flags.IsFlagSet(ClaimPropInsteadOfDestroy))
			{
				bClaimProp = true;
			}
			else if(!info.m_bDestroyPropInsteadOfDrop && m_Flags.IsFlagSet(ClaimPropInsteadOfDrop))
			{
				bClaimProp = true;
			}

			//Set whether we should not claim the prop.
			pTask->SetDontClaimProp(!bClaimProp);

			//Check if we are claiming the prop.
			if(bClaimProp)
			{
				//Keep the current ambient clips.
				//If we don't do this, the ambient clips will generally not match the props.
				pTask->SetAmbientClips(info.m_hAmbientClips);
			}
		}

		return pTask;
	}
}

bool CAgitatedActionFlee::LoadAmbientClipsInfo(const CPed& rPed, AmbientClipsInfo& rInfo) const
{
	//Find the ambient clips.
	CTaskAmbientClips* pTask = static_cast<CTaskAmbientClips *>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));
	if(!pTask)
	{
		return false;
	}
	
	//Set whether the prop will be destroyed.
	rInfo.m_bDestroyPropInsteadOfDrop = pTask->ShouldDestroyProp();
	
	//Set the ambient clips.
	const CConditionalAnimsGroup* pConditionalAnimsGroup = pTask->GetConditionalAnimsGroup();
	rInfo.m_hAmbientClips = pConditionalAnimsGroup ? atHashWithStringNotFinal(pConditionalAnimsGroup->GetHash()) : atHashWithStringNotFinal::Null();
	
	return true;
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionFlipOff
////////////////////////////////////////////////////////////////////////////////

CAgitatedActionFlipOff::CAgitatedActionFlipOff()
: CAgitatedAction()
, m_Time(0.0f)
{
}

CAgitatedActionFlipOff::~CAgitatedActionFlipOff()
{
}

void CAgitatedActionFlipOff::Execute(const CAgitatedActionContext& rContext) const
{
	//Grab the ped.
	CPed* pPed = rContext.m_Task->GetPed();
	
	//Find the in-vehicle task.
	CTaskInVehicleBasic* pTask = static_cast<CTaskInVehicleBasic *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_VEHICLE_BASIC));
	if(!pTask)
	{
		return;
	}
	
	//Ensure the weapon manager is valid.
	CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	if(!pWeaponManager)
	{
		return;
	}
	
	//Equip the unarmed weapon.
	pWeaponManager->EquipWeapon(pPed->GetDefaultUnarmedWeaponHash(), -1, true);
	
	//Request a drive-by.
	CAITarget target(rContext.m_Task->GetAgitator());
	pTask->RequestDriveBy(m_Time, CTaskVehicleGun::Mode_Aim, target);
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionFollow
////////////////////////////////////////////////////////////////////////////////

CAgitatedActionFollow::CAgitatedActionFollow()
: CAgitatedActionTask()
{

}

CAgitatedActionFollow::~CAgitatedActionFollow()
{

}

CTask* CAgitatedActionFollow::CreateTask(const CAgitatedActionContext& rContext) const
{
	//Create the task.
	CTask* pTask = rage_new CTaskFollowLeaderInFormation(NULL, rContext.m_Task->GetAgitator());

	return pTask;
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionHurryAway
////////////////////////////////////////////////////////////////////////////////

CTask* CAgitatedActionHurryAway::CreateTask(const CAgitatedActionContext& rContext) const
{
	//Create the shocking event.
	CPed* pAgitator = rContext.m_Task->GetAgitator();
	taskAssert(pAgitator);
	CEventShockingSeenConfrontation event(*pAgitator, rContext.m_Task->GetPed());

	//Create the task.
	CTaskShockingEventHurryAway* pTask = rage_new CTaskShockingEventHurryAway(&event);
	pTask->SetShouldPlayShockingSpeech(false);
	pTask->SetGoDirectlyToWaitForPath(true);
	pTask->SetForcePavementNotRequired(true);

	return pTask;
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionIgnoreForcedAudioFailures
////////////////////////////////////////////////////////////////////////////////

CAgitatedActionIgnoreForcedAudioFailures::CAgitatedActionIgnoreForcedAudioFailures()
: CAgitatedAction()
{
}

CAgitatedActionIgnoreForcedAudioFailures::~CAgitatedActionIgnoreForcedAudioFailures()
{
}

void CAgitatedActionIgnoreForcedAudioFailures::Execute(const CAgitatedActionContext& rContext) const
{
	rContext.m_Task->GetConfigFlags().SetFlag(CTaskAgitated::CF_IgnoreForcedAudioFailures);
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionSetFleeAmbientClips
////////////////////////////////////////////////////////////////////////////////

CAgitatedActionSetFleeAmbientClips::CAgitatedActionSetFleeAmbientClips()
: CAgitatedAction()
, m_AmbientClips()
{

}

CAgitatedActionSetFleeAmbientClips::~CAgitatedActionSetFleeAmbientClips()
{

}

void CAgitatedActionSetFleeAmbientClips::Execute(const CAgitatedActionContext& rContext) const
{
	//Find the flee task.
	CTaskSmartFlee* pTask = static_cast<CTaskSmartFlee *>(rContext.m_Task->GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SMART_FLEE));
	if(!aiVerifyf(pTask, "Flee task is invalid."))
	{
		return;
	}

	//Set the ambient clips.
	pTask->SetAmbientClips(m_AmbientClips);
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionMakeAggressiveDriver
////////////////////////////////////////////////////////////////////////////////

CAgitatedActionMakeAggressiveDriver::CAgitatedActionMakeAggressiveDriver()
: CAgitatedAction()
{

}

CAgitatedActionMakeAggressiveDriver::~CAgitatedActionMakeAggressiveDriver()
{

}

void CAgitatedActionMakeAggressiveDriver::Execute(const CAgitatedActionContext& rContext) const
{
	//Grab the ped.
	CPed* pPed = rContext.m_Task->GetPed();

	pPed->GetPedIntelligence()->SetDriverAggressivenessOverride(1.0f);
	pPed->GetPedIntelligence()->SetDriverAbilityOverride(0.0f);
	
	CVehicle* pMyVehicle = pPed->GetMyVehicle();
	if (pMyVehicle)
	{
		pMyVehicle->m_nVehicleFlags.bMadDriver = true;
	}
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionReportCrime
////////////////////////////////////////////////////////////////////////////////

void CAgitatedActionReportCrime::Execute(const CAgitatedActionContext& rContext) const
{
	rContext.m_Task->ReportCrime();
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionReset
////////////////////////////////////////////////////////////////////////////////

CAgitatedActionReset::CAgitatedActionReset()
: CAgitatedAction()
{

}

CAgitatedActionReset::~CAgitatedActionReset()
{

}

void CAgitatedActionReset::Execute(const CAgitatedActionContext& rContext) const
{
	//Reset the action.
	rContext.m_Task->ResetAction();
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionSay
////////////////////////////////////////////////////////////////////////////////

void CAgitatedActionSay::Execute(const CAgitatedActionContext& rContext) const
{
	//Say the audio.
	rContext.m_Task->Say(m_Say);
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionSayAgitator
////////////////////////////////////////////////////////////////////////////////

void CAgitatedActionSayAgitator::Execute(const CAgitatedActionContext& rContext) const
{
	//Ensure the agitator is valid.
	CPed* pAgitator = rContext.m_Task->GetAgitator();
	if(!pAgitator)
	{
		return;
	}

	//Say the audio to the ped.
	CTalkHelper::Talk(*pAgitator, *rContext.m_Task->GetPed(), m_Audio);
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionSetFleeMoveBlendRatio
////////////////////////////////////////////////////////////////////////////////

CAgitatedActionSetFleeMoveBlendRatio::CAgitatedActionSetFleeMoveBlendRatio()
: CAgitatedAction()
, m_MoveBlendRatio(0.0f)
{

}

CAgitatedActionSetFleeMoveBlendRatio::~CAgitatedActionSetFleeMoveBlendRatio()
{

}

void CAgitatedActionSetFleeMoveBlendRatio::Execute(const CAgitatedActionContext& rContext) const
{
	//Find the flee task.
	CTaskSmartFlee* pTaskSmartFlee = static_cast<CTaskSmartFlee *>(rContext.m_Task->GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SMART_FLEE));
	if(pTaskSmartFlee)
	{
		//Set the move/blend ratio.
		pTaskSmartFlee->SetMoveBlendRatio(m_MoveBlendRatio);
		return;
	}
	
	// The flee task could be a react and flee task.
	CTaskReactAndFlee* pTaskReactAndFlee = static_cast<CTaskReactAndFlee *>(rContext.m_Task->GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_REACT_AND_FLEE));
	if(pTaskReactAndFlee)
	{
		//Set the move/blend ratio.
		pTaskReactAndFlee->SetMoveBlendRatio(m_MoveBlendRatio);
	}
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionSetHash
////////////////////////////////////////////////////////////////////////////////

void CAgitatedActionSetHash::Execute(const CAgitatedActionContext& rContext) const
{
	rContext.m_Task->SetHash(m_Value);
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionStopVehicle
////////////////////////////////////////////////////////////////////////////////

CAgitatedActionStopVehicle::CAgitatedActionStopVehicle()
: CAgitatedActionTask()
{

}

CAgitatedActionStopVehicle::~CAgitatedActionStopVehicle()
{

}

CTask* CAgitatedActionStopVehicle::CreateTask(const CAgitatedActionContext& rContext) const
{
	//Grab the ped.
	const CPed* pPed = rContext.m_Task->GetPed();
	
	//Ensure the ped is driving.
	if(!aiVerifyf(pPed->GetIsDrivingVehicle(), "The ped is not driving a vehicle."))
	{
		return NULL;
	}
	
	//Ensure the vehicle is valid.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!aiVerifyf(pVehicle, "The vehicle is invalid."))
	{
		return NULL;
	}
	
	//Create the vehicle task.
	CTask* pVehicleTask = rage_new CTaskVehicleStop(0);
	
	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(pVehicle, pVehicleTask);

	return pTask;
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionTurnOnSiren
////////////////////////////////////////////////////////////////////////////////

void CAgitatedActionTurnOnSiren::Execute(const CAgitatedActionContext& rContext) const
{
	rContext.m_Task->TurnOnSiren(m_Time);
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionalAction
////////////////////////////////////////////////////////////////////////////////

CAgitatedConditionalAction::CAgitatedConditionalAction()
: m_Condition(NULL)
, m_Action(NULL)
{

}

CAgitatedConditionalAction::~CAgitatedConditionalAction()
{
	delete m_Condition;
	delete m_Action;
}

///////////////////////////////////////////////////////////////////////////////
// CAgitatedScenarioExit
///////////////////////////////////////////////////////////////////////////////

CAgitatedScenarioExit::CAgitatedScenarioExit()
: CAgitatedAction()
{

}

CAgitatedScenarioExit::~CAgitatedScenarioExit()
{

}

void CAgitatedScenarioExit::Execute(const CAgitatedActionContext& rContext) const
{

	const CPed* pPed = rContext.m_Task->GetPed();
	CTaskUseScenario* pTask = static_cast<CTaskUseScenario*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
	if(aiVerifyf(pTask, "TaskUseScenario is invalid."))
	{
		pTask->TriggerAggroThenExit(rContext.m_Task->GetAgitator());
	}
}

///////////////////////////////////////////////////////////////////////////////
// CAgitatedScenarioNormalCowardExit
///////////////////////////////////////////////////////////////////////////////

CAgitatedScenarioNormalCowardExit::CAgitatedScenarioNormalCowardExit()
: CAgitatedAction()
{

}

CAgitatedScenarioNormalCowardExit::~CAgitatedScenarioNormalCowardExit()
{

}

void CAgitatedScenarioNormalCowardExit::Execute(const CAgitatedActionContext& rContext) const
{

	const CPed* pPed = rContext.m_Task->GetPed();
	CTaskUseScenario* pTask = static_cast<CTaskUseScenario*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
	if(aiVerifyf(pTask, "TaskUseScenario is invalid."))
	{
		pTask->TriggerExitForCowardDisputes(false, rContext.m_Task->GetAgitator());
	}
}

///////////////////////////////////////////////////////////////////////////////
// CAgitatedScenarioFastCowardExit
///////////////////////////////////////////////////////////////////////////////

void CAgitatedScenarioFastCowardExit::Execute(const CAgitatedActionContext& rContext) const
{
	const CPed* pPed = rContext.m_Task->GetPed();
	CTaskUseScenario* pTask = static_cast<CTaskUseScenario*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
	if(aiVerifyf(pTask, "TaskUseScenario is invalid."))
	{
		pTask->TriggerExitForCowardDisputes(true, rContext.m_Task->GetAgitator());
	}
}

///////////////////////////////////////////////////////////////////////////////
// CAgitatedScenarioFleeExit
///////////////////////////////////////////////////////////////////////////////

void CAgitatedScenarioFleeExit::Execute(const CAgitatedActionContext& rContext) const
{
	const CPed* pPed = rContext.m_Task->GetPed();
	CTaskUseScenario* pTask = static_cast<CTaskUseScenario*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
	if(aiVerifyf(pTask, "TaskUseScenario is invalid."))
	{
		CTaskAgitated* pAgitatedTask = rContext.m_Task;
		if (Verifyf(pAgitatedTask, "NULL task agitated when triggering flee exit."))
		{
			CPed* pAgitator = pAgitatedTask->GetAgitator();
			if (Verifyf(pAgitator, "NULL agitator when triggering flee exit."))
			{
				Vec3V vPos = pAgitator->GetTransform().GetPosition();
				pTask->TriggerFleeExitForCowardDisputes(vPos);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// CAgitatedSetWaterSurvivalTime
///////////////////////////////////////////////////////////////////////////////

void CAgitatedSetWaterSurvivalTime::Execute(const CAgitatedActionContext& rContext) const
{
	CPed* pPed = rContext.m_Task->GetPed();
	pPed->SetMaxTimeInWater(m_Time);
}