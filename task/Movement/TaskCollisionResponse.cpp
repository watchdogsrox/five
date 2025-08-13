#include "TaskCollisionResponse.h"

// Framework headers
#include "fwanimation/animhelpers.h"
#include "fwanimation/animmanager.h"
#include "grcore/debugdraw.h"
#include "fwmaths/Angle.h"
#include "fwmaths/Vector.h"
#include "math/angmath.h"

// Game headers
#include "ai/AITarget.h"
#include "ai/ambient/ConditionalAnims.h"
#include "animation/AnimDefines.h"
#include "Control/Route.h"
#include "Vehicles/Automobile.h"
#include "Event/Events.h"
#include "Event/EventGroup.h"
#include "Event/EventDamage.h"
#include "Game/ModelIndices.h"
#include "ik/IkRequest.h"
#include "ModelInfo/VehicleModelInfo.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Physics/GtaArchetype.h"
#include "physics/WorldProbe/worldprobe.h"
#include "streaming/SituationalClipSetStreamer.h"
#include "Task/Animation/TaskAnims.h"
#include "Task/Default/AmbientAnimationManager.h"
#include "Task/Default/TaskWander.h"
#include "Task/General/TaskBasic.h"
#include "task/Physics/TaskNMJumpRollFromRoadVehicle.h"
#include "task/Response/TaskAgitated.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/VehicleHullAI.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Task/Movement/Jumping/TaskFallGetUp.h"
#include "Task/Movement/Jumping/TaskJump.h"
#include "Task/movement/TaskGetUp.h"
#include "Task/Movement/TaskGoTo.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Scenario/Info/ScenarioInfo.h"
#include "Task/Scenario/Info/ScenarioInfoManager.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Task/Scenario/Types/TaskCowerScenario.h"
#include "Task/General/TaskSecondary.h"
#include "Task/System/TaskTypes.h"

AI_OPTIMISATIONS()

/////////////////////
// CTaskCower
/////////////////////
int CClonedCowerInfo::GetScriptedTaskType() const
{
	return SCRIPT_TASK_COWER;
}

CTaskFSMClone* CClonedCowerInfo::CreateCloneFSMTask()
{
	return rage_new CTaskCower(GetDuration());
}

CTaskCower::CTaskCower(const int iDuration, const int iScenarioIndex) 
: m_iDuration(iDuration),
  m_iScenarioIndex(iScenarioIndex),
  m_cloneLeaveCowerAnims(false),
  m_forceStateChangeOnMigrate(false)
{
#if __ASSERT
	// add this to catch when we set a duration that's out of range for network
	if(m_iDuration > 0)
		Assertf(static_cast<unsigned>(m_iDuration) < (1u << CClonedCowerInfo::SIZEOF_DURATION), "CTaskCower :: Duration out of range for network, Duration:%d", iDuration);
#endif
	SetInternalTaskType(CTaskTypes::TASK_COWER);
}

CTaskCower::FSM_Return CTaskCower::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Running)
			FSM_OnEnter
				StateRunning_OnEnter();
			FSM_OnUpdate
				return StateRunning_OnUpdate();
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

bool CTaskCower::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	if (pEvent)
	{
		SetFlinchIgnoreEvent((CEvent*)pEvent);
	}

	return CTask::ShouldAbort(iPriority, pEvent);
}

void CTaskCower::StateRunning_OnEnter()
{
	// Set the time.
	if (m_iDuration >= 0)
	{
		m_Timer.Set(fwTimer::GetTimeInMilliseconds(), m_iDuration);
	}

	// Grab the scenario depending on what the ped has currently set for its cower animations.
	CPed* pPed = GetPed();
	m_iScenarioIndex = (pPed && pPed->IsNetworkClone() && !IsRunningLocally()) ? m_iScenarioIndex : SCENARIOINFOMGR.GetScenarioIndex(GetPed()->GetPedIntelligence()->GetFleeBehaviour().GetStationaryReactHash());
	CScenarioInfo* pInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(m_iScenarioIndex);

	// Validate the scenario info after the hash lookup.
	if (!Verifyf(pInfo, "Invalid scenario info.  Reaction hash %s .  ScenarioIndex %d .  Network clone?  %d", 
		GetPed()->GetPedIntelligence()->GetFleeBehaviour().GetStationaryReactHash().GetCStr(), m_iScenarioIndex, pPed->IsNetworkClone()))
	{
		return;
	}

	s32 iChosenPriority = 0;
	s32 iChosenConditionalAnimation = 0;
	CScenarioCondition::sScenarioConditionData conditionData;
	conditionData.pPed = pPed;
	conditionData.eAmbientEventType = AET_In_Place;
	
	// Validate that a conditional animations group could be chosen.
	if (!Verifyf(CAmbientAnimationManager::ChooseConditionalAnimations(conditionData, iChosenPriority, &pInfo->GetConditionalAnimsGroup(), iChosenConditionalAnimation, CConditionalAnims::AT_PANIC_BASE), "Unable to choose a cowering anim!  Scenario:  %s  Network clone:  %d", pInfo->GetName(), pPed->IsNetworkClone()))
	{
		iChosenConditionalAnimation = 0;
	}

	// Validate the the chosen conditional animation is a valid index for the group.
	if (!Verifyf(pInfo->GetConditionalAnimsGroup().GetNumAnims() > iChosenConditionalAnimation && iChosenConditionalAnimation >= 0, 
		"Invalid number of conditional anims.  Group count:  %u .  Chosen:  %d  Scenario info:  %s  Network clone:  %d.", pInfo->GetConditionalAnimsGroup().GetNumAnims(), iChosenConditionalAnimation, pInfo->GetName(), pPed->IsNetworkClone()))
	{
		return;
	}

	const CConditionalAnims* pAnims = pInfo->GetConditionalAnimsGroup().GetAnims(iChosenConditionalAnimation);

	// Create an invalid target.
	CAITarget target;

	// Create the task.
	CTaskCowerScenario* pTask = rage_new CTaskCowerScenario(m_iScenarioIndex, NULL, pAnims, target);

	// Tell the cower task to cower forever, when the ped exits is under this task's control.
	if (m_iDuration == -1)
	{
		pTask->SetCowerForever(true);
	}

	// HACK HACK HACK
	// Because of a discrepancy of how the anims were made compared to the seated cowers, standing cowers must always go through the undirected
	// exit or the blend won't work out.  At this stage it was easier to force the undirected exits to always play instead of changing the animations.
	pTask->ForceUndirectedExit(true);

	SetNewTask(pTask);
}
CTaskCower::FSM_Return CTaskCower::StateRunning_OnUpdate()
{
	// Cache off the subtask.
	CTask* pSubTask = GetSubTask();
	CPed* pPed = GetPed();

	// Don't respond to getting bumped.
	pPed->SetPedResetFlag(CPED_RESET_FLAG_DontActivateRagdollFromPlayerPedImpactReset, true);

	// Turn on kinematic physics to prevent getting pushed around easily since we have now disabled the bump response.
	pPed->SetPedResetFlag(CPED_RESET_FLAG_UseKinematicPhysics, true);

	// If the subtask finishes then finish.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !pSubTask)
	{
		SetState(State_Finish);
	}

	// Is the timer being used if so has the timer finished?
	if(m_Timer.IsSet() && m_Timer.IsOutOfTime())
	{
		// Tell the subtask to abort.
		if (pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_COWER_SCENARIO)
		{
			CTaskCowerScenario* pTaskScenario = static_cast<CTaskCowerScenario*>(pSubTask);
			pTaskScenario->TriggerNormalExit();
		}
	}
	// Otherwise, continue using the scenario.
	return FSM_Continue;
}

CTaskCower::FSM_Return CTaskCower::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	if(iState != State_Finish)
	{
		if(iEvent == OnUpdate)
		{
			// if the network state is telling us to finish...
			s32 iStateFromNetwork = GetStateFromNetwork();
			if(iStateFromNetwork != GetState() && iStateFromNetwork == State_Finish)
			{
				SetState(iStateFromNetwork);
			}
		}
	}

	return UpdateFSM(iState, iEvent);
}

CTaskInfo* CTaskCower::CreateQueriableState() const
{
	//default to our time, this catches the -1 case for infinite time
	int newDuration = m_iDuration;
	//if the timer is set, then only give them how much time is remaining to run
	if (m_Timer.IsSet())
	{
		newDuration = m_Timer.GetTimeLeft();
		//check if the time happened to expire this round
		if (newDuration <= 0) {
			newDuration = 0;
		}
	}
	return rage_new CClonedCowerInfo(GetState(), m_iScenarioIndex, newDuration);
}

void CTaskCower::ReadQueriableState(CClonedFSMTaskInfo *pTaskInfo)
{
	Assert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_COWER);

	CClonedCowerInfo *pCowerInfo = static_cast<CClonedCowerInfo*>(pTaskInfo);
	m_iDuration = pCowerInfo->GetDuration();
	m_iScenarioIndex = pCowerInfo->GetScenarioIndex();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

CTaskFSMClone* CTaskCower::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	CTaskCower* pTask = rage_new CTaskCower(m_iDuration,m_iScenarioIndex);
	pTask->m_forceStateChangeOnMigrate = true;
	m_cloneLeaveCowerAnims = true;
	return pTask;
}

CTaskFSMClone* CTaskCower::CreateTaskForLocalPed(CPed* pPed)
{
	return CreateTaskForClonePed(pPed);
}

void CTaskCower::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

// Cowering shouldn't flinch because of the event that triggered cowering to begin with, otherwise the ped would continually flinch as
// that event persists during cowering.
void CTaskCower::SetFlinchIgnoreEvent(const CEvent* pEvent)
{
	if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COWER_SCENARIO)
	{
		CTaskCowerScenario* pCowerTask = static_cast<CTaskCowerScenario*>(GetSubTask());
		pCowerTask->SetFlinchIgnoreEvent(static_cast<const CEvent*>(pEvent));
	}
}

//--------------------------------------------------------------
// CLASS: CTaskComplexEvasiveStep
// Evasive step as result of a potential get run over
//--------------------------------------------------------------

CTaskComplexEvasiveStep::Tunables CTaskComplexEvasiveStep::sm_Tunables;
IMPLEMENT_MOVEMENT_TASK_TUNABLES(CTaskComplexEvasiveStep, 0x077d1a9b);

const float CTaskComplexEvasiveStep::ms_fTurnSpeedMultiplier = 6.0f;
const float CTaskComplexEvasiveStep::ms_fTurnTolerance = ( DtoR * 8.0f);

////////////////////////////////////////////////////////////////////////////////

CTaskComplexEvasiveStep::CTaskComplexEvasiveStep(CEntity * pTargetEntity, fwFlags8 uConfigFlags) :
BANK_ONLY(m_vMoveDir(Vector3(0.0f,0.0f,0.0f)),)
m_vDivePosition(Vector3(0.0f,0.0f,0.0f)),
m_pTargetEntity(pTargetEntity),
m_fBlendOutDeltaOverride(0.0f),
m_uConfigFlags(uConfigFlags),
m_bUseDivePosition(false)
{
	SetInternalTaskType(CTaskTypes::TASK_COMPLEX_EVASIVE_STEP);

	//Assert that the parameters are valid.
	taskAssertf(!m_uConfigFlags.IsFlagSet(CF_Dive) || !m_uConfigFlags.IsFlagSet(CF_Casual), "There are no casual dives.");
}

////////////////////////////////////////////////////////////////////////////////

CTaskComplexEvasiveStep::CTaskComplexEvasiveStep(const Vector3 & vDivePosition) :
BANK_ONLY(m_vMoveDir(Vector3(0.0f,0.0f,0.0f)),)
m_vDivePosition(vDivePosition),
m_pTargetEntity(NULL),
m_fBlendOutDeltaOverride(0.0f),
m_uConfigFlags(0),
m_bUseDivePosition(true)
{
	SetInternalTaskType(CTaskTypes::TASK_COMPLEX_EVASIVE_STEP);

	m_uConfigFlags.SetFlag(CF_Dive);
}

////////////////////////////////////////////////////////////////////////////////

CTaskComplexEvasiveStep::~CTaskComplexEvasiveStep()
{
}

////////////////////////////////////////////////////////////////////////////////

float CTaskComplexEvasiveStep::GetBlendOutDelta() const
{
	//Check if the override is set.
	if(m_fBlendOutDeltaOverride != 0.0f)
	{
		return m_fBlendOutDeltaOverride;
	}

	return sm_Tunables.m_BlendOutDelta;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskComplexEvasiveStep::SetBlendOutDeltaOverride(float fBlendOutDelta)
{
	//Set the blend out delta override.
	m_fBlendOutDeltaOverride = fBlendOutDelta;

	//Note that the blend out delta override changed.
	OnBlendOutDeltaOverrideChanged();
}

void CTaskComplexEvasiveStep::CleanUp()
{
	GetPed()->SetActivateRagdollOnCollision(false);
	GetPed()->ClearActivateRagdollOnCollisionEvent();
}

////////////////////////////////////////////////////////////////////////////////

aiTask* CTaskComplexEvasiveStep::Copy() const
{
	CTaskComplexEvasiveStep* pTask = m_bUseDivePosition ?
		rage_new CTaskComplexEvasiveStep(m_vDivePosition) :
		rage_new CTaskComplexEvasiveStep(m_pTargetEntity, m_uConfigFlags);

	pTask->SetBlendOutDeltaOverride(m_fBlendOutDeltaOverride);
	
	return pTask;
}

////////////////////////////////////////////////////////////////////////////////

extern f32 g_HealthLostForLowPain;

aiTask* CTaskComplexEvasiveStep::CreatePlayEvasiveStepSubtask(CPed* pPed)
{
	bool bRelativelyRight = true;
	Vector3 vMoveDir(0.0f,0.0f,0.0f);

#if __BANK
	vMoveDir = m_vMoveDir;	// this is only used when the direction is set whilst testing from RAG
#endif

	if(m_bUseDivePosition)
	{
		vMoveDir = m_vDivePosition - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		vMoveDir.z = 0.0f;
		vMoveDir.NormalizeSafe();

		// B is to the right
		if(DotProduct(vMoveDir, VEC3V_TO_VECTOR3(pPed->GetTransform().GetB())) > 0.0f)
		{
			bRelativelyRight = false;
		}
	}

	// Pick the clip based on the direction to the target
	else if(m_pTargetEntity)
	{
		//////////////////////////////////////////////////////////////////////////
		// @RSNWE-JW
		// Should be a vector from the target to the ped.
		vMoveDir = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition() - m_pTargetEntity->GetTransform().GetPosition());
		vMoveDir.Normalize();
		Vector3 vMoveSpeed = VEC3V_TO_VECTOR3(m_pTargetEntity->GetTransform().GetB());
		if( m_pTargetEntity->GetIsPhysical() )
		{
			Vector3 vPhysicalMoveSpeed = ((CPhysical*)m_pTargetEntity.Get())->GetVelocity();
			if( vPhysicalMoveSpeed.Mag2() > 0.5f )
			{
				vMoveSpeed = vPhysicalMoveSpeed;
			}
		}
		Vector3 vRight;
		vRight.Cross(vMoveDir, ZAXIS);
		if( vRight.Dot(vMoveSpeed) > 0.0f )
		{
			bRelativelyRight = false;
		}
	}

	// We don't have a valid direction to dive
	if (vMoveDir.IsZero())
	{
		return NULL;
	}

	float fDirToThreat = rage::Atan2f(-vMoveDir.x, vMoveDir.y);
	fDirToThreat += PI;
	fDirToThreat -= pPed->GetTransform().GetHeading();
	fDirToThreat = fwAngle::LimitRadianAngleSafe(fDirToThreat);

	// transform float heading into an integer offset
	float fTemp = fDirToThreat + QUARTER_PI;
	if(fTemp < 0.0f) fTemp += TWO_PI;
	int nHitDirn = int(fTemp / HALF_PI);

	Direction nOrientation	= D_None;
	Direction nDirection	= D_None;
	switch(nHitDirn)
	{
	case CEntityBoundAI::FRONT:
		nOrientation	= D_Front;
		nDirection		= bRelativelyRight ? D_Left : D_Right;
		break;
	case CEntityBoundAI::REAR:
		nOrientation	= D_Back;
		nDirection		= bRelativelyRight ? D_Right : D_Left;
		break;
	case CEntityBoundAI::LEFT:
		nOrientation	= D_Left;
		nDirection		= bRelativelyRight ? D_Back : D_Front;
		break;
	case CEntityBoundAI::RIGHT:
		nOrientation	= D_Right;
		nDirection		= bRelativelyRight ? D_Front : D_Back;
		break;
	}

	//Note: Hacks galore
	{
		//Hacks for non-dives
		if(!m_uConfigFlags.IsFlagSet(CF_Dive))
		{
			if(m_pTargetEntity)
			{
				//Disabling side-steps when avoiding vehicles due to it looking like crap
				//when the ped can't make it out of the way.  If we are going to re-enable
				//these, we probably need some kind of bounding box projection to make sure
				//the ped (thinks) they won't get hit due to the projected position.
				if(m_pTargetEntity->GetIsTypeVehicle())
				{
					switch(nOrientation)
					{
						case D_Back:
							nDirection = D_Front;
							break;
						case D_Front:
							nDirection = D_Back;
							break;
						case D_Left:
							nDirection = D_Right;
							break;
						case D_Right:
							nDirection = D_Left;
							break;
						default:
							break;
					}
				}
				else if(m_pTargetEntity->GetIsTypePed())
				{
					//When avoiding peds, randomly use the opposite direction (so we get run over).
					static dev_float s_fChancesToUseOppositeDirection = 0.5f;
					if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < s_fChancesToUseOppositeDirection)
					{
						//Check the orientation.
						switch(nOrientation)
						{
							case D_Back:
								nDirection = D_Front;
								break;
							case D_Front:
								nDirection = D_Back;
								break;
							case D_Left:
								nDirection = D_Right;
								break;
							case D_Right:
								nDirection = D_Left;
								break;
							default:
								break;
						}
					}
					else
					{
						//Always use casual anims for side-steps vs. peds (apparently the urgent ones are not liked).
						m_uConfigFlags.SetFlag(CF_Casual);
					}
				}
			}
		}
		//Hacks for dives
		else
		{

		}
	}
	
	//Get the reaction clip.
	fwMvClipSetId clipSetId;
	fwMvClipId clipId;
	bool bFoundClip = GetClipForReaction(nOrientation, nDirection, m_uConfigFlags.IsFlagSet(CF_Dive), m_uConfigFlags.IsFlagSet(CF_Casual), clipSetId, clipId);
	taskAssertf(bFoundClip, "No reaction clip found.");
	if(!bFoundClip)
	{
		return NULL;
	}

	//Say some audio.
	bool bUseDodgeAudio = m_uConfigFlags.IsFlagSet(CF_Dive);
	if(!bUseDodgeAudio)
	{
		bUseDodgeAudio = (m_pTargetEntity && m_pTargetEntity->GetIsTypeVehicle());
	}
	if(bUseDodgeAudio)
	{
		if(m_pTargetEntity && m_pTargetEntity->GetIsTypeVehicle() &&
			(static_cast<CVehicle*>(m_pTargetEntity.Get()))->GetVehicleType() == VEHICLE_TYPE_BICYCLE)
		{
			GetPed()->NewSay("DODGE_NON_VEHICLE");
		}
		else
		{
			GetPed()->NewSay("DODGE");
		}
	}
	else
	{
		//Check if we are still, or aggressive.
		static dev_float s_fMaxSpeed = 0.5f;
		bool bIsStill = (GetPed()->GetVelocity().Mag2() < square(s_fMaxSpeed));
		if(bIsStill || CTaskAgitated::IsAggressive(*GetPed()))
		{
			GetPed()->NewSay("GENERIC_SHOCKED_MED");
		}
		else
		{
			GetPed()->NewSay("BUMP");
		}
	}

	//Assert that the clip set is streamed in.
	taskAssert(fwClipSetManager::IsStreamedIn_DEPRECATED(clipSetId));

	// HACK - shorten the clips
	crClip* pClip = fwClipSetManager::GetClip(clipSetId, clipId);
	float duration = -1.0f;
	if (pClip && m_pTargetEntity && !m_uConfigFlags.IsFlagSet(CF_Dive))
	{
		TUNE_GROUP_FLOAT(EVASIVE_STEP, s_shortenClipPhase, 0.3f, 0.0f, 1.0f, 0.001f);
		duration = pClip->GetDuration() - Abs(SLOW_BLEND_DURATION) - pClip->ConvertPhaseToTime(s_shortenClipPhase); //HACK - get the clips shortened
	}

	//Create the task.
	CTaskRunClip* pTask = rage_new CTaskRunClip(clipSetId, clipId, NORMAL_BLEND_IN_DELTA, GetBlendOutDelta(), (s32)(duration*1000.0f), CTaskRunClip::ATF_IGNORE_MOVER_BLEND_ROTATION);

	//Set the rate.
	float fRate = fwRandom::GetRandomNumberInRange(0.9f, 1.1f); 
	pTask->SetRate(fRate);

	//Check if we are diving.
	if(m_uConfigFlags.IsFlagSet(CF_Dive))
	{
		// add the delay blend out flag so we have a frame to start the getup anim
		pTask->SetClipFlag(APF_SKIP_NEXT_UPDATE_BLEND);
		//Set up an NM event.
		CTask* pTaskNM = rage_new CTaskNMJumpRollFromRoadVehicle(1000, 10000, false);
		CEventSwitch2NM event(10000, pTaskNM);
		GetPed()->SetActivateRagdollOnCollisionEvent(event);
	}

	return pTask;
}


aiTask* CTaskComplexEvasiveStep::CreateGetUpSubTask(CPed* UNUSED_PARAM(pPed))
{
	return rage_new CTaskGetUp();
}

////////////////////////////////////////////////////////////////////////////////

Vector3 CTaskComplexEvasiveStep::ComputeEvasiveStepMoveDir( const CPed& ped, CVehicle *pVehicle )
{
	Vector3 vecMoveDirn;
	//int iVehicleHitSide = CPedGeometryAnalyser::ComputeEntityHitSide(ped,*pVehicle);

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
	CEntityBoundAI bound(*pVehicle,vPedPosition.z, ped.GetCapsuleInfo()->GetHalfWidth());
	const int iVehicleHitSide=bound.ComputeHitSideByPosition(vPedPosition);

	switch(iVehicleHitSide)
	{
	case CEntityBoundAI::LEFT:
		vecMoveDirn = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetA());
		vecMoveDirn *= -1;
		break;
	case CEntityBoundAI::REAR:
		vecMoveDirn = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB());
		vecMoveDirn *= -1;
		break;
	case CEntityBoundAI::RIGHT:
		vecMoveDirn = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetA());
		break;
	case CEntityBoundAI::FRONT:
		vecMoveDirn = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB());
		break;
	default:
		Assert(false);
		break;
	}

	return vecMoveDirn;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskComplexEvasiveStep::ProcessPreFSM()
{
	if (!GetPed()->IsNetworkClone())
	{
		// Check the shock flag as we want certain shocking events to be able to alter this task's behaviour (car crash).
		GetPed()->GetPedIntelligence()->SetCheckShockFlag(true);

		//Update the heading.
		UpdateHeading();

		//Set whether we can switch to nm on collisions.
		bool bCanSwitchToNm = CanSwitchToNm();
		GetPed()->SetActivateRagdollOnCollision(bCanSwitchToNm);
		if(bCanSwitchToNm)
		{
			GetPed()->SetRagdollOnCollisionAllowedSlope(-2.0f);
		}

		if(ShouldInterrupt())
		{
			return FSM_Quit;
		}

		//Prevent scooting up steep slopes.
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_EnableSteepSlopePrevention, true);
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ExpandPedCapsuleFromSkeleton, true);
	}

	//Look at the target entity.
	if(m_pTargetEntity)
	{
		CIkRequestBodyLook oIkRequest(m_pTargetEntity, BONETAG_HEAD);
		GetPed()->GetIkManager().Request(oIkRequest);
		GetPed()->GetPedIntelligence()->Evaded(m_pTargetEntity);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskComplexEvasiveStep::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.
	FSM_Begin
		FSM_State(State_PlayEvasiveStepClip)
			FSM_OnEnter
				return StatePlayEvasiveStepClip_OnEnter(pPed);
			FSM_OnUpdate
				return StatePlayEvasiveStepClip_OnUpdate(pPed);
			FSM_OnExit
				StatePlayEvasiveStepClip_OnExit(pPed);

		FSM_State(State_GetUp)
				FSM_OnEnter
					return StateGetUp_OnEnter(pPed);
				FSM_OnUpdate
					return StateGetUp_OnUpdate(pPed);

		FSM_State(State_Quit)
				FSM_OnUpdate
					return FSM_Quit;
	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

void CTaskComplexEvasiveStep::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Quit);
}

////////////////////////////////////////////////////////////////////////////////

CTaskInfo* CTaskComplexEvasiveStep::CreateQueriableState() const
{
	return rage_new CClonedComplexEvasiveStepInfo( GetState(), m_pTargetEntity, m_uConfigFlags, m_bUseDivePosition, m_vDivePosition );
}

////////////////////////////////////////////////////////////////////////////////

void CTaskComplexEvasiveStep::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_EVASIVE_STEP);
	CClonedComplexEvasiveStepInfo* pEvasiveStepInfo = static_cast<CClonedComplexEvasiveStepInfo*>(pTaskInfo);

	m_pTargetEntity	   = pEvasiveStepInfo->GetTarget();
	m_uConfigFlags	   = pEvasiveStepInfo->GetConfigFlags();
	m_bUseDivePosition = pEvasiveStepInfo->GetUseDivePosition();
	m_vDivePosition    = pEvasiveStepInfo->GetDivePosition();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);	
}

////////////////////////////////////////////////////////////////////////////////

CTaskComplexEvasiveStep::FSM_Return CTaskComplexEvasiveStep::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	return UpdateFSM(iState, iEvent);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskComplexEvasiveStep::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if (iPriority==ABORT_PRIORITY_IMMEDIATE)
	{
		return true;
	}
	else if (pEvent)
	{
		CEvent* pCEvent = (CEvent*)pEvent;
		if(pCEvent->GetEventType() == EVENT_SCRIPT_COMMAND)
		{
			CEventScriptCommand* command = static_cast<CEventScriptCommand*>(pCEvent);
			if(command)
			{
				if(command->GetTask()->GetTaskType() == CTaskTypes::TASK_SYNCHRONIZED_SCENE)
				{
					return true;
				}
			}
		}
		
		if (pCEvent->RequiresAbortForRagdoll() || pCEvent->RequiresAbortForMelee(pPed) || ((pCEvent->GetEventType()==EVENT_DAMAGE) && CanAbortForDamageAnim(pEvent)) ||
			(!m_uConfigFlags.IsFlagSet(CF_Dive) && (pCEvent->GetEventType() == EVENT_GUN_AIMED_AT)))
		{
			return true;
		}
		return false;
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskComplexEvasiveStep::StatePlayEvasiveStepClip_OnEnter(CPed* pPed)
{
	aiTask* pSubtask = CreatePlayEvasiveStepSubtask(pPed);
	if    (pSubtask)
	{
		SetNewTask(pSubtask);
		return FSM_Continue;
	}

	return FSM_Quit;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskComplexEvasiveStep::StatePlayEvasiveStepClip_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if(taskVerifyf(GetSubTask(), "GetSubTask is NULL"))
	{
		//If the subtask has ended or doesn't exit go to the CreateNextSubTask state.
		if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
		{
			if(m_uConfigFlags.IsFlagSet(CF_Dive))
			{
				SetState(State_GetUp);
				return FSM_Continue;
			}

			return FSM_Quit;

		}
		
		return FSM_Continue;
	}

	return FSM_Quit;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskComplexEvasiveStep::StatePlayEvasiveStepClip_OnExit(CPed* /*pPed*/)
{
	if(!m_uConfigFlags.IsFlagSet(CF_Dive))
	{
		//Force the motion state.
		GetPed()->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle);
	}

	//Say some audio.  It isn't safe to create sounds from outside the NorthAudio thread while it's running, so
	//use SayWhenSafe() as this can be triggered from the render thread which runs parallel to the NorthAudio thread.
	if(m_pTargetEntity && m_pTargetEntity->GetIsTypeVehicle() && GetPed()->GetSpeechAudioEntity())
	{
		GetPed()->GetSpeechAudioEntity()->SayWhenSafe("GENERIC_SHOCKED_MED");
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskComplexEvasiveStep::StateGetUp_OnEnter(CPed* pPed)
{
	if(pPed->IsNetworkClone())
	{
		return FSM_Continue;
	}

	aiTask* pSubtask = CreateGetUpSubTask(pPed);
	if    (pSubtask)
	{
		SetNewTask(pSubtask);
		return FSM_Continue;
	}

	return FSM_Quit;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskComplexEvasiveStep::StateGetUp_OnUpdate(CPed *pPed)
{
	if(pPed->IsNetworkClone() && !GetSubTask())
	{
		if(GetStateFromNetwork() == State_Quit)
		{
			SetState(State_Quit);
		}
		else
		{
			CreateSubTaskFromClone(pPed, CTaskTypes::TASK_GET_UP);
		}

		return FSM_Continue;
	}

	if(taskVerifyf(GetSubTask(), "GetSubTask is NULL"))
	{
		//If the subtask has ended or doesn't exit go to the CreateNextSubTask state.
		if (!GetIsFlagSet(aiTaskFlags::SubTaskFinished))
		{
			return FSM_Continue;
		}
	}

	return FSM_Quit;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskComplexEvasiveStep::CanInterrupt() const
{
	//Check if the config flag is set.
	if(m_uConfigFlags.IsFlagSet(CF_Interrupt))
	{
		return true;
	}

	//Check if we are running threat response.
	if(GetPed()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_THREAT_RESPONSE))
	{
		return true;
	}

	//Check if we are running combat.
	if(GetPed()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
	{
		return true;
	}

	//Check if we are running flee.
	if(GetPed()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SMART_FLEE))
	{
		return true;
	}

	//Check if we are running hurry away.
	if(GetPed()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SHOCKING_EVENT_HURRYAWAY))
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskComplexEvasiveStep::CanSwitchToNm() const
{
	//Ensure we are diving.
	if(!m_uConfigFlags.IsFlagSet(CF_Dive))
	{
		return false;
	}

	//Ensure the ped is on-screen.
	if(!GetPed()->GetIsVisibleInSomeViewportThisFrame())
	{
		return false;
	}

	//Ensure the ped is not using low lod physics.
	if(GetPed()->IsUsingLowLodPhysics())
	{
		return false;
	}

	//Ensure the sub-task is valid.
	const CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
		return false;
	}

	//Ensure the sub-task is the right type.
	if(pSubTask->GetTaskType() != CTaskTypes::TASK_RUN_CLIP)
	{
		return false;
	}

	//Grab the task.
	const CTaskRunClip* pTask = static_cast<const CTaskRunClip *>(pSubTask);

	//Ensure the clip helper is valid.
	const CClipHelper* pClipHelper = pTask->GetClipHelper();
	if(!pClipHelper)
	{
		return false;
	}

	return pClipHelper->IsEvent(CClipEventTags::CanSwitchToNm);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskComplexEvasiveStep::CanAbortForDamageAnim(const aiEvent* pEvent) const
{
	//Ensure we are diving.
	if(m_uConfigFlags.IsFlagSet(CF_Dive))
	{
		// Still about if we're running TaskFallGetup
		if (pEvent->GetEventType() == EVENT_DAMAGE)
		{
			if (((CEventDamage*)pEvent)->GetPhysicalResponseTaskType() == CTaskTypes::TASK_FALL_AND_GET_UP)
			{
				return true;
			}
		}

		return false;
	}
	
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskComplexEvasiveStep::GetClipForReaction(Direction nOrientation, Direction nDirection, bool bDive, bool bCasual, fwMvClipSetId& clipSetId, fwMvClipId& clipId) const
{
	//Check if we are diving.
	if(bDive)
	{
		//Set the clip set id.
		clipSetId = CLIP_SET_MOVE_AVOIDANCE;

		//Check the orientation.
		switch(nOrientation)
		{
			case D_Back:
			{
				//Check the direction.
				if(nDirection == D_Left)
				{
					clipId = CLIP_REACT_BACK_DIVE_LEFT;
					return true;
				}
				else if(nDirection == D_Right)
				{
					clipId = CLIP_REACT_BACK_DIVE_RIGHT;
					return true;
				}
				else
				{
					return false;
				}
			}
			case D_Front:
			{
				//Check the direction.
				if(nDirection == D_Left)
				{
					clipId = CLIP_REACT_FRONT_DIVE_LEFT;
					return true;
				}
				else if(nDirection == D_Right)
				{
					clipId = CLIP_REACT_FRONT_DIVE_RIGHT;
					return true;
				}
				else
				{
					return false;
				}
			}
			case D_Left:
			{
				//Check the direction.
				if(nDirection == D_Back)
				{
					clipId = CLIP_REACT_LEFT_SIDE_DIVE_BACK;
					return true;
				}
				else if(nDirection == D_Front)
				{
					clipId = CLIP_REACT_LEFT_SIDE_DIVE_FRONT;
					return true;
				}
				else
				{
					return false;
				}
			}
			case D_Right:
			{
				//Check the direction.
				if(nDirection == D_Back)
				{
					clipId = CLIP_REACT_RIGHT_SIDE_DIVE_BACK;
					return true;
				}
				else if(nDirection == D_Front)
				{
					clipId = CLIP_REACT_RIGHT_SIDE_DIVE_FRONT;
					return true;
				}
				else
				{
					return false;
				}
			}
			default:
			{
				return false;
			}
		}
	}
	else
	{
		//Set the clip set id.
		clipSetId = CSituationalClipSetStreamer::GetInstance().GetClipSetForAvoids(bCasual);
		taskAssert(clipSetId != CLIP_SET_ID_INVALID);

		//Check the orientation.
		switch(nOrientation)
		{
			case D_Back:
			{
				//Check the direction.
				if(nDirection == D_Front)
				{
					clipId = CLIP_AVOID_FROM_BACK_TO_FRONT;
					return true;
				}
				else if(nDirection == D_Left)
				{
					clipId = CLIP_AVOID_FROM_BACK_TO_LEFT;
					return true;
				}
				else if(nDirection == D_Right)
				{
					clipId = CLIP_AVOID_FROM_BACK_TO_RIGHT;
					return true;
				}
				else
				{
					return false;
				}
			}
			case D_Front:
			{
				//Check the direction.
				if(nDirection == D_Back)
				{
					clipId = CLIP_AVOID_FROM_FRONT_TO_BACK;
					return true;
				}
				else if(nDirection == D_Left)
				{
					clipId = CLIP_AVOID_FROM_FRONT_TO_LEFT;
					return true;
				}
				else if(nDirection == D_Right)
				{
					clipId = CLIP_AVOID_FROM_FRONT_TO_RIGHT;
					return true;
				}
				else
				{
					return false;
				}
			}
			case D_Left:
			{
				//Check the direction.
				if(nDirection == D_Back)
				{
					clipId = CLIP_AVOID_FROM_LEFT_TO_BACK;
					return true;
				}
				else if(nDirection == D_Front)
				{
					clipId = CLIP_AVOID_FROM_LEFT_TO_FRONT;
					return true;
				}
				else if(nDirection == D_Right)
				{
					clipId = CLIP_AVOID_FROM_LEFT_TO_RIGHT;
					return true;
				}
				else
				{
					return false;
				}
			}
			case D_Right:
			{
				//Check the direction.
				if(nDirection == D_Back)
				{
					clipId = CLIP_AVOID_FROM_RIGHT_TO_BACK;
					return true;
				}
				else if(nDirection == D_Front)
				{
					clipId = CLIP_AVOID_FROM_RIGHT_TO_FRONT;
					return true;
				}
				else if(nDirection == D_Left)
				{
					clipId = CLIP_AVOID_FROM_RIGHT_TO_LEFT;
					return true;
				}
				else
				{
					return false;
				}
			}
			default:
			{
				return false;
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskComplexEvasiveStep::OnBlendOutDeltaOverrideChanged()
{
	//Ensure the sub-task is valid.
	CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
		return;
	}

	//Ensure the sub-task is the right type.
	if(pSubTask->GetTaskType() != CTaskTypes::TASK_RUN_CLIP)
	{
		return;
	}

	//Grab the task.
	CTaskRunClip* pTask = static_cast<CTaskRunClip *>(pSubTask);

	//Set the blend out delta.
	pTask->SetBlendOutDelta(GetBlendOutDelta());
}

bool CTaskComplexEvasiveStep::ShouldInterrupt() const
{
	//Ensure we can interrupt.
	if(!CanInterrupt())
	{
		return false;
	}

	//Ensure the sub-task is valid.
	const CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
		return false;
	}

	//Ensure the sub-task is the right type.
	if(pSubTask->GetTaskType() != CTaskTypes::TASK_RUN_CLIP)
	{
		return false;
	}

	//Grab the task.
	const CTaskRunClip* pTask = static_cast<const CTaskRunClip *>(pSubTask);

	//Ensure the clip helper is valid.
	const CClipHelper* pClipHelper = pTask->GetClipHelper();
	if(!pClipHelper)
	{
		return false;
	}

	return pClipHelper->IsEvent(CClipEventTags::Interruptible);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskComplexEvasiveStep::UpdateHeading()
{
	//Ensure we are not diving.
	if(m_uConfigFlags.IsFlagSet(CF_Dive))
	{
		return;
	}

	//Ensure the target entity is valid.
	if(!m_pTargetEntity)
	{
		return;
	}

	//Ensure the sub-task is valid.
	CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
		return;
	}

	//Ensure the sub-task is the right type.
	if(pSubTask->GetTaskType() != CTaskTypes::TASK_RUN_CLIP)
	{
		return;
	}

	//Grab the task.
	CTaskRunClip* pTask = static_cast<CTaskRunClip *>(pSubTask);

	//Ensure the clip helper is valid.
	const CClipHelper* pClipHelper = pTask->GetClipHelper();
	if(!pClipHelper)
	{
		return;
	}

	//Ensure the clip is valid.
	const crClip* pClip = pClipHelper->GetClip();
	if(!pClip)
	{
		return;
	}

	//Grab the phase.
	float fPhase = pClipHelper->GetPhase();

	//Grab the translations.
	const Vector3 vCurrentTranslation	= fwAnimHelpers::GetMoverTrackTranslation(*pClip, fPhase);
	const Vector3 vEndTranslation		= fwAnimHelpers::GetMoverTrackTranslation(*pClip, 1.0f);

	//Ensure the remaining translation is small.
	float fDistSq = vCurrentTranslation.XZDist2(vEndTranslation);
	static float s_fMaxDist = 0.2f;
	float fMaxDistSq = square(s_fMaxDist);
	if(fDistSq > fMaxDistSq)
	{
		return;
	}

	//Calculate the remaining heading change.
	const Quaternion qCurrent	= fwAnimHelpers::GetMoverTrackRotation(*pClip, fPhase);
	const Quaternion qEnd		= fwAnimHelpers::GetMoverTrackRotation(*pClip, 1.0f);

	Vector3 vCurrent;
	qCurrent.ToEulers(vCurrent, "xyz");
	Vector3 vEnd;
	qEnd.ToEulers(vEnd, "xyz");

	vCurrent.z	= fwAngle::LimitRadianAngle(vCurrent.z);
	vEnd.z		= fwAngle::LimitRadianAngle(vEnd.z);

	const float fRemainingHeadingChange = fwAngle::LimitRadianAngle(vEnd.z - vCurrent.z);

	//Calculate the required heading change.
	const float fTargetHeading = fwAngle::GetRadianAngleBetweenPoints(m_pTargetEntity->GetTransform().GetPosition().GetXf(),
		m_pTargetEntity->GetTransform().GetPosition().GetYf(), GetPed()->GetTransform().GetPosition().GetXf(),
		GetPed()->GetTransform().GetPosition().GetYf());
	const float fRequiredHeadingChange = SubtractAngleShorter(fwAngle::LimitRadianAngle(fTargetHeading), GetPed()->GetCurrentHeading());

	//Calculate the maximum amount of heading change for this frame.
	static float s_fHeadingChangeRate = 2.0f;
	float fMaxHeadingChange = s_fHeadingChangeRate * GetTimeStep();

	//Calculate the heading change for this frame.
	float fHeadingChange = fwAngle::LimitRadianAngle(fRequiredHeadingChange - fRemainingHeadingChange);
	fHeadingChange = Clamp(fHeadingChange, -fMaxHeadingChange, fMaxHeadingChange);

	//Apply the heading change.
	float fHeading = GetPed()->GetCurrentHeading();
	fHeading += fHeadingChange;
	GetPed()->SetHeading(fwAngle::LimitRadianAngleSafe(fHeading));
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskComplexEvasiveStep::GetStaticStateName( s32 iState )
{
	static const char* sStateNames[] = 
	{
		"State_PlayEvasiveStepClip",
		"State_GetUp",
		"State_Quit"
	};

	CompileTimeAssert(sizeof(sStateNames) / sizeof(const char*) == (State_Quit + 1));

	return sStateNames[iState];
}

#endif

//-------------------------------------------------------------------------
//		EVASIVE STEP TASK INFO
//-------------------------------------------------------------------------

CClonedComplexEvasiveStepInfo::CClonedComplexEvasiveStepInfo(s32 const iState, CEntity* pTarget, u8 const uConfigFlags, bool const bUseDivePosition, const Vector3& vDivePosition) 
	: m_pTarget(pTarget)
	, m_uConfigFlags(uConfigFlags)
	, m_bUseDivePosition(bUseDivePosition)
	, m_vDivePosition(vDivePosition)
{
	SetStatusFromMainTaskState(iState);	
}

////////////////////////////////////////////////////////////////////////////////

CClonedComplexEvasiveStepInfo::CClonedComplexEvasiveStepInfo()
{}

////////////////////////////////////////////////////////////////////////////////

CClonedComplexEvasiveStepInfo::~CClonedComplexEvasiveStepInfo()
{}

////////////////////////////////////////////////////////////////////////////////

CTaskFSMClone *CClonedComplexEvasiveStepInfo::CreateCloneFSMTask()
{
	return m_bUseDivePosition ? rage_new CTaskComplexEvasiveStep( m_vDivePosition ) : rage_new CTaskComplexEvasiveStep( m_pTarget.GetEntity(), m_uConfigFlags );
}

////////////////////////////////////////////////////////////////////////////////

CTask* CClonedComplexEvasiveStepInfo::CreateLocalTask(fwEntity* UNUSED_PARAM( pEntity ) )
{
	return m_bUseDivePosition ? rage_new CTaskComplexEvasiveStep( m_vDivePosition ) : rage_new CTaskComplexEvasiveStep( m_pTarget.GetEntity(), m_uConfigFlags );
}


/////////////////////
//WALK ROUND FIRE
/////////////////////

const float CTaskWalkRoundFire::ms_fFireRadius=3.0f;


CTaskWalkRoundFire::CTaskWalkRoundFire(const float fMoveBlendRatio, const Vector3& vFirePos, const float fFireRadius, const Vector3& vTarget)
:m_fMoveBlendRatio(fMoveBlendRatio),
m_vFirePos(vFirePos),
m_fFireRadius(fFireRadius),
m_vTarget(vTarget)
{
	SetInternalTaskType(CTaskTypes::TASK_WALK_ROUND_FIRE);
}
CTaskWalkRoundFire::~CTaskWalkRoundFire()
{
}

#if !__FINAL
void CTaskWalkRoundFire::Debug() const
{
	if(GetSubTask())
		GetSubTask()->Debug();
}
#endif

CTask::FSM_Return CTaskWalkRoundFire::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.
	if( pPed->GetIsInVehicle() )
		return FSM_Quit;

	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnEnter
				return StateInitial_OnEnter(pPed);
			FSM_OnUpdate
				return StateInitial_OnUpdate(pPed);
		FSM_State(State_Moving)
			FSM_OnEnter
				return StateMoving_OnEnter(pPed);
			FSM_OnUpdate
				return StateMoving_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return CTaskWalkRoundFire::StateInitial_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	return FSM_Continue;
}
CTask::FSM_Return CTaskWalkRoundFire::StateInitial_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	SetState(State_Moving);
	return FSM_Continue;
}
CTask::FSM_Return CTaskWalkRoundFire::StateMoving_OnEnter(CPed * pPed)
{
	m_vStartPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 vDetourTarget;
	ComputeDetourTarget(*pPed, vDetourTarget);

	CTaskMoveGoToPoint * pMoveTask = rage_new CTaskMoveGoToPoint(m_fMoveBlendRatio, vDetourTarget);
	SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask));
	return FSM_Continue;
}
CTask::FSM_Return CTaskWalkRoundFire::StateMoving_OnUpdate(CPed * pPed)
{
	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		CTaskComplexControlMovement * pCtrlMove = (CTaskComplexControlMovement*)GetSubTask();
		CTask * pMoveTask = pCtrlMove->GetRunningMovementTask(pPed);
		if(pMoveTask && pMoveTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT)
		{
			Vector3 vDetourTarget;
			ComputeDetourTarget(*pPed, vDetourTarget);

			CTaskMoveGoToPoint * pGoToTask = (CTaskMoveGoToPoint*)pMoveTask;
			pGoToTask->SetTarget(pPed, VECTOR3_TO_VEC3V(vDetourTarget));
		}
	}
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

void CTaskWalkRoundFire::ComputeDetourTarget(const CPed& ped, Vector3& vDetourTarget)
{
	spdSphere sphere;
	sphere.Set( RCC_VEC3V(m_vFirePos), ScalarV(m_fFireRadius + ped.GetCapsuleInfo()->GetHalfWidth()) );

	Vector3 vStart = m_vStartPos;
	m_vStartPos.z = m_vFirePos.z;
	Vector3 vTarget = m_vTarget;
	vTarget.z = m_vFirePos.z;
	Vector3 vPedPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
	vPedPos.z = m_vFirePos.z;

	Vector3 vNewTarget;
	CPedGrouteCalculator::ComputeRouteRoundSphere(vPedPos, sphere, vStart, vTarget, vNewTarget, vDetourTarget);
}	


CTaskWalkRoundVehicleDoor::CTaskWalkRoundVehicleDoor(const Vector3 & vTarget, CVehicle * pParentVehicle, const s32 iDoorIndex, const float fMoveBlendRatio, const u32 iFlags) : CTaskMove(fMoveBlendRatio)
{
	m_pParentVehicle = pParentVehicle;
	m_iDoorIndex = iDoorIndex;
	m_vTarget = vTarget;
	m_iFlags = iFlags;
	SetInternalTaskType(CTaskTypes::TASK_MOVE_WALK_ROUND_VEHICLE_DOOR);
}
CTaskWalkRoundVehicleDoor::~CTaskWalkRoundVehicleDoor()
{

}
#if !__FINAL
void CTaskWalkRoundVehicleDoor::Debug() const
{
	if(GetSubTask())
		GetSubTask()->Debug();
}
#endif

bool CTaskWalkRoundVehicleDoor::HasTarget() const
{
	const CTask * pSubTask = GetSubTask();
	if(!pSubTask)
		return false;
	Assert(pSubTask->IsMoveTask());
	return ((const CTaskMove*)pSubTask)->HasTarget();
}
Vector3 CTaskWalkRoundVehicleDoor::GetTarget() const
{
	const CTask * pSubTask = GetSubTask();
	if(!pSubTask)
		return m_vTarget;
	Assert(pSubTask->IsMoveTask());
	return ((const CTaskMove*)pSubTask)->GetTarget();
}
float CTaskWalkRoundVehicleDoor::GetTargetRadius(void) const
{
	const CTask * pSubTask = GetSubTask();
	if(!pSubTask)
		return 0.0f;
	Assert(pSubTask->IsMoveTask());
	return ((const CTaskMove*)pSubTask)->GetTargetRadius();
}
void CTaskWalkRoundVehicleDoor::SetTarget( const CPed* UNUSED_PARAM(pPed), Vec3V_In vTarget, const bool UNUSED_PARAM(bForce))
{
	m_vTarget = VEC3V_TO_VECTOR3(vTarget);
	SetFlag(aiTaskFlags::RestartCurrentState);
}
bool CTaskWalkRoundVehicleDoor::ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent)
{
	return CTask::ShouldAbort(iPriority, pEvent);
}
CTask::FSM_Return CTaskWalkRoundVehicleDoor::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed();
	if( pPed->GetIsInVehicle() )
		return FSM_Quit;

	FSM_Begin
		FSM_State(State_Moving)
			FSM_OnEnter
				return StateMoving_OnEnter(pPed);
			FSM_OnUpdate
				return StateMoving_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return CTaskWalkRoundVehicleDoor::StateMoving_OnEnter(CPed* pPed)
{
	if(!m_pParentVehicle)
		return FSM_Quit;
	CCarDoor * pDoor = m_pParentVehicle->GetDoor(m_iDoorIndex);
	if(!pDoor || !pDoor->GetIsIntact(m_pParentVehicle) || pDoor->GetIsClosed())
		return FSM_Quit;

	Vector3 vRoutePts[4];
	s32 iNumPts = CalculateRoute(pPed, vRoutePts);
	if(!iNumPts)
		return FSM_Quit;

	CPointRoute route;
	for(s32 p=0; p<iNumPts; p++)
		route.Add(vRoutePts[p]);
	route.Add(m_vTarget);

	CTaskMoveFollowPointRoute * pFollowRoute = rage_new CTaskMoveFollowPointRoute(
		m_fMoveBlendRatio,
		route);
	
	SetNewTask(pFollowRoute);

	return FSM_Continue;
}

// NAME : CalculateRoute
// PURPOSE : Calculates a route from the ped's side of the door to the target's side, whilst
// being aware of the winding order dictated by which side of the vehicle this door is attached,
// and ensuring not to traverse the side of the door which is attached to the vehicle.
// pOutRoutePoints : an array of 4 Vector3's, which will receive the route
// Returns the number of points in the route (maximum of 4); returns 0 if task should quit

void CTaskWalkRoundVehicleDoor::CreateDoorBoundAI(CPed * pPed, const Matrix34 & matVehicle, CEntityBoundAI & outBoundAi, Matrix34 & outMatDoor)
{
	CCarDoor * pDoor = m_pParentVehicle->GetDoor(m_iDoorIndex);

	pDoor->GetLocalMatrix(m_pParentVehicle, outMatDoor);
	outMatDoor.Dot(matVehicle);

	Vector3 vDoorMin, vDoorMax;
	pDoor->GetLocalBoundingBox(m_pParentVehicle, vDoorMin, vDoorMax);

	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	outBoundAi = CEntityBoundAI(vDoorMin, vDoorMax, outMatDoor, vPedPos.z, pPed->GetCapsuleInfo()->GetHalfWidth(), false);
}

s32 CTaskWalkRoundVehicleDoor::CalculateRoute(CPed * pPed, Vector3 * pOutRoutePoints)
{
	const Matrix34 matVehicle = MAT34V_TO_MATRIX34(m_pParentVehicle->GetMatrix());
	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Matrix34 matDoor;
	CEntityBoundAI boundai;
	CreateDoorBoundAI(pPed, matVehicle, boundai, matDoor);


	s32 iPedSide = boundai.ComputeHitSideByPosition(vPedPos);
	s32 iTargetSide = boundai.ComputeHitSideByPosition(m_vTarget);
	s32 iWinding = matVehicle.a.Dot( matVehicle.d - matDoor.d ) > 0.0f ? -1 : 1;

	if(iPedSide == iTargetSide)	// nothing to do
		return 0;

	Vector3 vCorners[4];
	boundai.GetCorners(vCorners);

	if(iWinding==-1)
		iPedSide--;
	if(iPedSide < 0)
		iPedSide += 4;

	s32 iNumPts=0;
	while(iPedSide != iTargetSide)
	{
		pOutRoutePoints[iNumPts++] = vCorners[iPedSide];
		iPedSide += iWinding;
		if(iPedSide < 0)
			iPedSide += 4;
		if(iPedSide >= 4)
			iPedSide -= 4;
	}

	if(iWinding==-1)
		pOutRoutePoints[iNumPts++] = vCorners[iPedSide];
	
	Assert(iNumPts > 0 && iNumPts <= 4);
	return iNumPts;
}

CTask::FSM_Return CTaskWalkRoundVehicleDoor::StateMoving_OnUpdate(CPed* pPed)
{
	if(!m_pParentVehicle)
		return FSM_Quit;
	CCarDoor * pDoor = m_pParentVehicle->GetDoor(m_iDoorIndex);
	if(!pDoor || !pDoor->GetIsIntact(m_pParentVehicle) || pDoor->GetIsClosed())
		return FSM_Quit;

	const Matrix34 matVehicle = MAT34V_TO_MATRIX34(m_pParentVehicle->GetMatrix());
	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Matrix34 matDoor;
	CEntityBoundAI boundai;
	CreateDoorBoundAI(pPed, matVehicle, boundai, matDoor);

	s32 iPedSide = boundai.ComputeHitSideByPosition(vPedPos);
	s32 iTargetSide = boundai.ComputeHitSideByPosition(m_vTarget);

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || iPedSide==iTargetSide)
	{
		return FSM_Quit;
	}

	if( (m_iFlags & Flag_TestHasClearedDoor)!=0 )
	{
		if( boundai.TestLineOfSight(VECTOR3_TO_VEC3V(vPedPos), VECTOR3_TO_VEC3V(m_vTarget)) )
		{
			return FSM_Quit;
		}
	}

	return FSM_Continue;
}


const int CTaskWalkRoundEntity::m_iTestClearedEntityFrequency = 15;
const int CTaskWalkRoundEntity::ms_iRecalcRouteFrequency = 250;
const float CTaskWalkRoundEntity::ms_fDefaultMaxDistWillTravelToWalkRndPed = 2.5f;
const float CTaskWalkRoundEntity::ms_fDefaultWaypointRadius = 0.25f;	// CTaskMoveGoToPoint::ms_fTargetRadius

CTaskWalkRoundEntity::CTaskWalkRoundEntity(const float fMoveBlendRatio, const Vector3 & vTargetPos, CEntity * pEntity, bool bStrafeAroundEntity) :
	CTaskMove(fMoveBlendRatio),
	m_vTargetPos(vTargetPos),
	m_pEntity(pEntity),
	m_pPointRoute(NULL),
	m_bNewTarget(false),
	m_bStrafeAroundEntity(bStrafeAroundEntity),
	m_bFirstTimeRun(true),
	m_bCheckForCarDoors(false),
	m_iHitDoorId(VEH_INVALID_ID),
	m_fWaypointRadius(ms_fDefaultWaypointRadius),
	m_bTestHasClearedEntity(false)
{
	Init();
	SetInternalTaskType(CTaskTypes::TASK_WALK_ROUND_ENTITY);
}

void CTaskWalkRoundEntity::Init()
{
	m_fMaxDistWillTravelToWalkRndPedSqr = fwRandom::GetRandomNumberInRange(
		ms_fDefaultMaxDistWillTravelToWalkRndPed - 0.5f,
		ms_fDefaultMaxDistWillTravelToWalkRndPed + 0.5f
		);
	m_fMaxDistWillTravelToWalkRndPedSqr *= m_fMaxDistWillTravelToWalkRndPedSqr;

	m_iRecalcRouteFrequency = ms_iRecalcRouteFrequency;
	m_bLineTestAgainstBuildings = false;
	m_bLineTestAgainstVehicles = false;
	m_bLineTestAgainstObjects = false;

	m_iLastPedSide = -1;
	m_iLastTargetSide = -1;

	m_bQuitNextFrame = false;

	Assert(m_pEntity);
	Assert(m_fMoveBlendRatio >= MBR_WALK_BOUNDARY && m_fMoveBlendRatio <= MOVEBLENDRATIO_SPRINT);
}

CTaskWalkRoundEntity::~CTaskWalkRoundEntity(void)
{
	if(m_pPointRoute)
	{
		delete m_pPointRoute;
		m_pPointRoute = NULL;
	}
}

#if !__FINAL
void
CTaskWalkRoundEntity::Debug() const
{
	if(GetSubTask())
	{
		GetSubTask()->Debug();
	}
}
#endif

void
CTaskWalkRoundEntity::SetTarget(const CPed * UNUSED_PARAM(pPed), Vec3V_In vTarget, const bool UNUSED_PARAM(bForce))
{
	static const float fDeltaEpsSqr = 0.25f*0.25f;
	const Vector3 vDiff = VEC3V_TO_VECTOR3(vTarget) - m_vTargetPos;
	if(vDiff.Mag2() < fDeltaEpsSqr)
		return;

	m_vTargetPos = VEC3V_TO_VECTOR3(vTarget);
	m_bNewTarget = true;
}

CTask::FSM_Return CTaskWalkRoundEntity::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.
	// This won't work in a vehicle
	if( pPed->GetIsInVehicle() )
	{
		return FSM_Quit;
	}

FSM_Begin
	FSM_State(State_Initial)
		FSM_OnEnter
			return Initial_OnEnter(pPed);
		FSM_OnUpdate
			return Initial_OnUpdate(pPed);
	FSM_State(State_WalkingRoundEntity)
		FSM_OnUpdate
			return WalkingRoundEntity_OnUpdate(pPed);
FSM_End
}

CTask::FSM_Return CTaskWalkRoundEntity::Initial_OnEnter(CPed * pPed)
{
	if(m_pEntity == NULL || m_pEntity.Get() == NULL) //entity doesn't exist so just quit.
	{
		return FSM_Quit;
	}

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	if(m_bFirstTimeRun)
	{
		m_bFirstTimeRun = false;
		m_vPedsStartingPos = vPedPosition;
	}

	static dev_s32 iRecalcFreqMissionDoor = 25;	// 10x faster than normal
	static dev_s32 iClearanceMissionDoor = m_iTestClearedEntityFrequency;

	if( pPed->PopTypeIsMission() && m_pEntity->GetIsTypeObject() && ((CObject*)m_pEntity.Get())->IsADoor() )
	{
		m_RecalcRouteTimer.Set(fwTimer::GetTimeInMilliseconds(), iRecalcFreqMissionDoor);
		m_TestClearedEntityTimer.Set(fwTimer::GetTimeInMilliseconds(), iClearanceMissionDoor);
	}
	else
	{
		m_RecalcRouteTimer.Set(fwTimer::GetTimeInMilliseconds(), m_iRecalcRouteFrequency);
		m_TestClearedEntityTimer.Set(fwTimer::GetTimeInMilliseconds(), m_iTestClearedEntityFrequency);
	}

	m_vEntityInitialPos = VEC3V_TO_VECTOR3(m_pEntity->GetTransform().GetPosition());
	m_vEntityInitialRightVec = VEC3V_TO_VECTOR3(m_pEntity->GetTransform().GetA());
	m_vEntityInitialUpVec = VEC3V_TO_VECTOR3(m_pEntity->GetTransform().GetC());

	CEntityBoundAI entityBound(*m_pEntity, vPedPosition.z, pPed->GetCapsuleInfo()->GetHalfWidth());
	m_iLastPedSide = entityBound.ComputeHitSideByPosition(vPedPosition);
	m_iLastTargetSide = entityBound.ComputeHitSideByPosition(m_vTargetPos);

	if(!m_pPointRoute)
		m_pPointRoute = rage_new CPointRoute;

	// This are the world-probe test flags - peds, buildings, peds, etc.
	// NOTE that if these flags are set to check for stuff, it'll use world
	// line-of-sight tests every m_iRecalcRouteFrequency! (so best not to)
	u32 iFlags = 0;

	if(m_bLineTestAgainstBuildings) iFlags |= ArchetypeFlags::GTA_ALL_MAP_TYPES;
	if(m_bLineTestAgainstVehicles) iFlags |= ArchetypeFlags::GTA_VEHICLE_TYPE;
	if(m_bLineTestAgainstObjects) iFlags |= ArchetypeFlags::GTA_OBJECT_TYPE;

	CPointRoute * pTempRoute = rage_new CPointRoute();

	// A return value of "RouteRndEntityRet_StraightToTarget" will indicate no point-route is needed
	const CPedGrouteCalculator::ERouteRndEntityRet eRouteRet = CPedGrouteCalculator::ComputeRouteRoundEntityBoundingBox(
		*pPed,
		vPedPosition,
		*m_pEntity,
		m_vTargetPos,
		m_vTargetPos,
		*pTempRoute,
		iFlags,
		0,
		GetCheckForCarDoors(),
		m_iHitDoorId
		);

	// If we were already following the route, and this result is no better - then stick with the original route
	if(eRouteRet == CPedGrouteCalculator::RouteRndEntityRet_BlockedBothWays && !m_bFirstTimeRun && m_pPointRoute && m_pPointRoute->GetSize() > 0)
	{
		delete pTempRoute;
		//Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_FOLLOW_POINT_ROUTE);
		return FSM_Continue;
	}

	aiTask * pNewTask = NULL;

	// The route was able to go directly to the target with a single waypoint
	if(eRouteRet == CPedGrouteCalculator::RouteRndEntityRet_StraightToTarget)
	{
		delete pTempRoute;
		if(m_pPointRoute)
			m_pPointRoute->Clear();

		if(m_bFirstTimeRun)
		{
			pNewTask = CreateSubTask(CTaskTypes::TASK_MOVE_GO_TO_POINT, pPed);
		}
		else
		{
			return FSM_Quit;
		}
	}
	else
	{
		if(m_pPointRoute)
			delete m_pPointRoute;
		m_pPointRoute = pTempRoute;
		m_pPointRoute->Set(m_pPointRoute->GetSize()-1, m_vTargetPos);

		pNewTask = CreateSubTask(CTaskTypes::TASK_MOVE_FOLLOW_POINT_ROUTE, pPed);
	}
	Assert(pNewTask);
	SetNewTask(pNewTask);

	return FSM_Continue;
}

CTask::FSM_Return CTaskWalkRoundEntity::Initial_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
	SetState(State_WalkingRoundEntity);
	return FSM_Continue;
}

CTask::FSM_Return CTaskWalkRoundEntity::WalkingRoundEntity_OnUpdate(CPed * pPed)
{
	if(!m_pEntity || m_bQuitNextFrame || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}

	if(m_bStrafeAroundEntity)
	{
		pPed->SetIsStrafing(true);

		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		const Vector3 vEntityPosition = VEC3V_TO_VECTOR3(m_pEntity->GetTransform().GetPosition());
		Vector3 vToEntity = vEntityPosition - vPedPosition;
		Vector3 vToTarget = m_vTargetPos - vPedPosition;

		vToEntity.Normalize();
		vToTarget.Normalize();

		// If entity is directly ahead then face entity whilst strafing around it
		if(DotProduct(vToEntity, vToTarget) > 0.707f)
		{
			pPed->SetDesiredHeading(vEntityPosition);
		}
		// Otherwise turn to face the target as we strafe round the remained of the entity
		else
		{
			pPed->SetDesiredHeading(m_vTargetPos);
		}
	}

	// If we are walking round another ped, and have traveled too far - then quit.
	if(m_pEntity->GetIsTypePed())
	{
		float fDistTravlledSqrXY = (VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_vPedsStartingPos).XYMag2();
		if(fDistTravlledSqrXY > m_fMaxDistWillTravelToWalkRndPedSqr)
		{
			if(!GetSubTask() || GetSubTask()->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
			{
				return FSM_Quit;
			}
		}
	}

	// If we have been walking into something for a long time, then force an event to interrupt us
	if(pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() >= CStaticMovementScanner::ms_iStaticCounterStuckLimit)
	{
		CEventStaticCountReachedMax staticEvent(GetTaskType());
		pPed->GetPedIntelligence()->AddEvent(staticEvent);
		return FSM_Continue;
	}

	// Optionally test whether we have now cleared the entity
	// Lightweight test against only this object, so performed at a higher
	// frequency than the recalc route
	if( m_bTestHasClearedEntity && m_TestClearedEntityTimer.IsOutOfTime() )
	{
		m_TestClearedEntityTimer.Set(fwTimer::GetTimeInMilliseconds(), m_iTestClearedEntityFrequency);

		// Optionally test whether we have cleared the entity
		// If so then we can quit the task
		if( TestHasClearedEntity(pPed) )
		{
			return FSM_Quit;
		}
	}

	// If the recalc-route timer has expired, then check to see if the entity has moved.
	// If it has then we restart the task causing a new route to be calculated.

	bool bForceRecalcRoute = false;
	if(m_RecalcRouteTimer.IsOutOfTime())
	{
		m_RecalcRouteTimer.Set(fwTimer::GetTimeInMilliseconds(), m_iRecalcRouteFrequency);

		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		CEntityBoundAI entityBound(*m_pEntity, vPedPosition.z, pPed->GetCapsuleInfo()->GetHalfWidth());
		const int iPedSide = entityBound.ComputeHitSideByPosition(vPedPosition);
		const int iTargetSide = entityBound.ComputeHitSideByPosition(m_vTargetPos);

		if( m_iLastPedSide != iPedSide || m_iLastTargetSide != iTargetSide || HasEntityMovedOrReorientated() )
		{
			bForceRecalcRoute = true;
		}

		m_iLastPedSide = iPedSide;
		m_iLastTargetSide = iTargetSide;
	}

	if((m_bNewTarget || bForceRecalcRoute) && (!GetSubTask() || GetSubTask()->MakeAbortable( ABORT_PRIORITY_URGENT, NULL)))
	{
		SetState(State_Initial);
	}

	return FSM_Continue;
}


bool CTaskWalkRoundEntity::TestHasClearedEntity(CPed * pPed)
{
	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	u32 iLosFlags = 0;
	if(m_pEntity->GetIsTypeObject() || m_pEntity->GetIsTypeDummyObject())
		iLosFlags |= ArchetypeFlags::GTA_OBJECT_TYPE;
	if(m_pEntity->GetIsTypeVehicle())
		iLosFlags |= (ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_BOX_VEHICLE_TYPE);

	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	WorldProbe::CShapeTestHitPoint result;
	WorldProbe::CShapeTestResults capsuleResult(result);

	capsuleDesc.SetResultsStructure(&capsuleResult);
	capsuleDesc.SetCapsule(vPedPos, m_vTargetPos, pPed->GetCapsuleInfo()->GetHalfWidth());
	capsuleDesc.SetIncludeFlags(iLosFlags);
	capsuleDesc.SetDoInitialSphereCheck(true);
	capsuleDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
	capsuleDesc.SetIncludeEntity(m_pEntity);
	capsuleDesc.SetIsDirected(true);
	
	if(WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
		return false;

	return true;
}

bool CTaskWalkRoundEntity::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	// Don't abort for a potential walk-rnd event if we are already walking around the same vehicle
	if(pEvent && ((CEvent*)pEvent)->GetEventType()==EVENT_POTENTIAL_WALK_INTO_VEHICLE
		&& ((CEvent*)pEvent)->GetSourceEntity()==m_pEntity)
	{
		return false;
	}
	// Likewise for object collisions
	if(pEvent && ((CEvent*)pEvent)->GetEventType()==EVENT_POTENTIAL_WALK_INTO_OBJECT
		&& ((CEvent*)pEvent)->GetSourceEntity()==m_pEntity)
	{
		return false;
	}
	// Likewise for vehicle collisions
	if(pEvent && ((CEvent*)pEvent)->GetEventType()==EVENT_VEHICLE_COLLISION)
	{
		CEventVehicleCollision * pVehCol = ((CEventVehicleCollision*)pEvent);
		if(pVehCol->GetVehicle()==m_pEntity.Get() && pVehCol->GetVehicleComponent()==m_iHitDoorId)
		{
			return false;
		}
	}

	// Base class
	return CTaskMove::ShouldAbort(iPriority, pEvent);
}

aiTask *
CTaskWalkRoundEntity::CreateSubTask(const int iSubTaskType, CPed * pPed) const
{
	switch(iSubTaskType)
	{
		case CTaskTypes::TASK_MOVE_GO_TO_POINT:
		{
			// Compute a goto target a little way ahead of the ped, on the way to the actual target
			// This will make this task persist for a little while before returning - during which
			// time we can control the strafe of the ped to look like the ped is turning out the way
			Vector3 vTarget;

			const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			Vector3 vToTarget = m_vTargetPos - vPedPosition;
			if(vToTarget.Mag2() > 1.0f)
			{
				vToTarget.Normalize();
				vTarget = vPedPosition + (vToTarget * 1.0f);
			}
			else
			{
				vTarget = m_vTargetPos;
			}
			CTaskMoveGoToPoint * pTaskGoTo = rage_new CTaskMoveGoToPoint(m_fMoveBlendRatio, vTarget);
			return pTaskGoTo;
		}
		case CTaskTypes::TASK_MOVE_FOLLOW_POINT_ROUTE:
		{
			Assert(m_pPointRoute);
			// For walking round peds use a larger radius
			static const float fRadiusForPed = 2.0f;
			const float fTargetRadius = m_pEntity->GetIsTypePed() ? fRadiusForPed : CTaskMoveFollowPointRoute::ms_fTargetRadius;

			CTaskMoveFollowPointRoute * pTaskRoute = rage_new CTaskMoveFollowPointRoute(
				m_fMoveBlendRatio,
				*m_pPointRoute,
				CTaskMoveFollowPointRoute::TICKET_SINGLE,
				fTargetRadius,
				CTaskMoveFollowPointRoute::ms_fSlowDownDistance,
				false,				// must overshoot
				true,				// use blending
				false,				// don't stand still at the end of the point-route
				0,					// starting progress
				m_fWaypointRadius	// target radius for internal waypoints (not the final target)
			);
			return pTaskRoute;
		}
		case CTaskTypes::TASK_FINISHED:
		{
			return NULL;
		}
	}
	Assert(0);
	return NULL;
}

bool
CTaskWalkRoundEntity::HasEntityMovedOrReorientated()
{
	Assert(m_pEntity);
	if(!m_pEntity)
		return false;

	static const float sfPosDeltaSqr = 0.25f*0.25f;
	static const float sfAngleMinDot = 0.9f;							// approx 25 degs

	static const float sfMissionPedDoor_PosDeltaSqr = 0.01f*0.01f;
	static const float sfMissionPedDoor_AngleMinDot = 0.9998f;		// approx 1 degs

	const bool bMissionPedOpeningDoor = GetPed()->PopTypeIsMission() && m_pEntity->GetIsTypeObject() && ((CObject*)m_pEntity.Get())->IsADoor();

	const float fPosDeltaSqr = bMissionPedOpeningDoor ? sfMissionPedDoor_PosDeltaSqr : sfPosDeltaSqr;
	const float fAngleMinDot = bMissionPedOpeningDoor ? sfMissionPedDoor_AngleMinDot : sfAngleMinDot;

	const Vector3 vDiff = VEC3V_TO_VECTOR3(m_pEntity->GetTransform().GetPosition()) - m_vEntityInitialPos;
	if(vDiff.Mag2() > fPosDeltaSqr)
		return true;

	if(DotProduct(VEC3V_TO_VECTOR3(m_pEntity->GetTransform().GetA()), m_vEntityInitialRightVec) < fAngleMinDot)
		return true;
	if(DotProduct(VEC3V_TO_VECTOR3(m_pEntity->GetTransform().GetC()), m_vEntityInitialUpVec) < fAngleMinDot)
		return true;

	return false;
}


CTaskWalkRoundCarWhileWandering::CTaskWalkRoundCarWhileWandering(const float fMoveBlendRatio, const Vector3 & vTargetPos, CVehicle * pVehicle) :
	CTaskMove(fMoveBlendRatio),
	m_vInitialWanderTarget(vTargetPos),
	m_vTargetPos(vTargetPos),
	m_pVehicle(pVehicle),
	m_vInitialToVehicle(VEC3_ZERO),
	m_bStartedOnPavement(false)
{
	SetInternalTaskType(CTaskTypes::TASK_MOVE_WANDER_AROUND_VEHICLE);
}

CTaskWalkRoundCarWhileWandering::~CTaskWalkRoundCarWhileWandering()
{
}

#if !__FINAL
void CTaskWalkRoundCarWhileWandering::Debug() const
{
#if DEBUG_DRAW
	grcDebugDraw::Cross(VECTOR3_TO_VEC3V(m_vTargetPos), 0.5f, Color_blue2);
#endif

	if(GetSubTask())
		GetSubTask()->Debug();
}
#endif 

void CTaskWalkRoundCarWhileWandering::CalculateTargetPos(CPed * pPed)
{
	static dev_float fTargetExtend = 0.0f;
	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const float fHalfWidth = pPed->GetCapsuleInfo()->GetHalfWidth();
	CEntityBoundAI bnd(*m_pVehicle, vPedPos.z, fHalfWidth);

	Vector3 vToTarget = m_vTargetPos - vPedPos;
	vToTarget.NormalizeSafe( VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward()), SMALL_FLOAT );

	Vector3 vCrossing1, vCrossing2;
	if(bnd.ComputeCrossingPoints(m_vTargetPos, vToTarget, vCrossing1, vCrossing2))
	{
		Assert((vCrossing1-m_vTargetPos).Mag2() >= (vCrossing2-m_vTargetPos).Mag2());
		m_vTargetPos = vCrossing2 + (vToTarget * (fTargetExtend + fHalfWidth));
	}
	else
	{
		m_vTargetPos = m_vInitialWanderTarget;
	}

#if __ASSERT
	int iTargetSide = bnd.ComputeHitSideByPosition(m_vTargetPos);
	int iPedSide = bnd.ComputeHitSideByPosition(vPedPos);
	Assert(iTargetSide==iPedSide);
#endif

	m_vInitialToVehicle = VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetPosition()) - vPedPos;
	m_vInitialToVehicle.z = 0.0f;
	m_vInitialToVehicle.Normalize();
}

bool CTaskWalkRoundCarWhileWandering::IsPedPastCar(CPed * pPed)
{
	if(!m_pVehicle)
		return true;

	if( m_bStartedOnPavement && pPed->GetNavMeshTracker().GetIsValid() && !pPed->GetNavMeshTracker().GetNavPolyData().m_bPavement )
		return false;

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	Vector3 vToVehicle = VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetPosition()) - vPedPosition;
	vToVehicle.z = 0.0f;
	vToVehicle.Normalize();

	static dev_float fDotEps = -Cosf( DtoR * 67.5f);

	if( DotProduct(m_vInitialToVehicle, vToVehicle) < fDotEps )
		return true;
	
	static dev_bool bCheckHitSides = false;
	if(bCheckHitSides)
	{
		CEntityBoundAI bnd(*m_pVehicle, vPedPosition.z, pPed->GetCapsuleInfo()->GetHalfWidth());
		int iTargetSide = bnd.ComputeHitSideByPosition(m_vTargetPos);
		int iPedSide = bnd.ComputeHitSideByPosition(vPedPosition);
		return (iTargetSide==iPedSide);
	}

	return false;
}

CTask::FSM_Return CTaskWalkRoundCarWhileWandering::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed();
	if(pPed->GetIsInVehicle())
		return FSM_Quit;

	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnEnter
				return Initial_OnEnter(pPed);
			FSM_OnUpdate
				return Initial_OnUpdate(pPed);
		FSM_State(State_FollowingNavMesh)
			FSM_OnEnter
				return FollowingNavMesh_OnEnter(pPed);
			FSM_OnUpdate
				return FollowingNavMesh_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return CTaskWalkRoundCarWhileWandering::Initial_OnEnter(CPed* pPed)
{
	m_bStartedOnPavement = pPed->GetNavMeshTracker().GetIsValid() && pPed->GetNavMeshTracker().GetNavPolyData().m_bPavement;

	m_vInitialToVehicle = VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	m_vInitialToVehicle.z = 0.0f;
	m_vInitialToVehicle.Normalize();

	return FSM_Continue;
}
CTask::FSM_Return CTaskWalkRoundCarWhileWandering::Initial_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	SetState(State_FollowingNavMesh);

	return FSM_Continue;
}

CTask::FSM_Return CTaskWalkRoundCarWhileWandering::FollowingNavMesh_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	if(!m_pVehicle)
		return FSM_Quit;

	static dev_bool bKeepToPavements = false;
	static dev_float fMultiplier = 4.5f;
	const float fTargetRadius = CTaskMoveGoToPoint::ms_fTargetRadius * fMultiplier;

	CTaskMoveFollowNavMesh * pNavTask = rage_new CTaskMoveFollowNavMesh( m_fMoveBlendRatio, m_vTargetPos, fTargetRadius );
	pNavTask->SetKeepMovingWhilstWaitingForPath(true);
	pNavTask->SetDontStandStillAtEnd(true);
	pNavTask->SetKeepToPavements(bKeepToPavements);
	pNavTask->SetScanForObstructionsFrequency(1000);	// default ms_iScanForObstructionsFreq is 4000ms

	pNavTask->SetDontUseClimbs(true);
	pNavTask->SetDontUseLadders(true);
	pNavTask->SetDontUseDrops(true);

	SetNewTask(pNavTask);
	
	// Ensure vehicle is not allowed to go to deactivate
	if(m_pVehicle->IsInPathServer())
	{
		CPathServerGta::UpdateDynamicObject(m_pVehicle, false, true);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskWalkRoundCarWhileWandering::FollowingNavMesh_OnUpdate(CPed* pPed)
{
	// Ensure vehicle is not allowed to go to deactivate
	if(!m_pVehicle)
		return FSM_Quit;

	if(m_pVehicle->IsInPathServer())
	{
		CPathServerGta::UpdateDynamicObject(m_pVehicle, false, true);
	}

	static bool bDoPastCarTest = true;
	if( bDoPastCarTest && IsPedPastCar(pPed) )
		return FSM_Quit;

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
	{
		return FSM_Quit;
	}

	// If the navmesh task was unable to find a path around the vehicle, give up
	Assert(GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH);
	CTaskMoveFollowNavMesh * pNavTask = (CTaskMoveFollowNavMesh*)GetSubTask();

	if(pNavTask->IsUnableToFindRoute())
	{
		// If ped is running the wander task, and is on pavement - then flag that we're blocked by this vehicle
		// Ped will then stop wandering until the vehicle moves.
		if( pPed->GetNavMeshTracker().GetIsValid() && pPed->GetNavMeshTracker().GetNavPolyData().m_bPavement )
		{
			CTaskWander * pWanderTask = (CTaskWander*)pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_WANDER);
			if(pWanderTask)
			{
				pWanderTask->SetBlockedByVehicle(m_pVehicle);
			}
		}

		return FSM_Quit;
	}

	return FSM_Continue;
}


//-------------------------------------------------------------------------------------------------------------

const int CTaskMoveWalkRoundVehicle::m_iTestClearedEntityFrequency = 200;

CTaskMoveWalkRoundVehicle::CTaskMoveWalkRoundVehicle(const Vector3 & vTarget, CVehicle * pVehicle, const float fMoveBlendRatio, const u32 iFlags)
	: CTaskMove(fMoveBlendRatio),
	m_vInitialCarPos(vTarget),
	m_vTarget(vTarget),
	m_pVehicle(pVehicle),
	m_iFlags(iFlags),
	m_pPointRoute(NULL),
	m_iLastTaskUpdateTimeMs(0),
	m_bCanUpdateTaskThisFrame(false)
{
	SetInternalTaskType(CTaskTypes::TASK_MOVE_WALK_ROUND_VEHICLE);	
}
CTaskMoveWalkRoundVehicle::~CTaskMoveWalkRoundVehicle()
{
	if(m_pPointRoute)
		delete m_pPointRoute;
}

#if !__FINAL
void CTaskMoveWalkRoundVehicle::Debug() const
{
	if(GetSubTask())
		GetSubTask()->Debug();
}
#endif //!__FINAL

bool CTaskMoveWalkRoundVehicle::HasTarget() const
{
	return true;
}
Vector3 CTaskMoveWalkRoundVehicle::GetTarget() const
{
	if(GetSubTask())
		return ((CTaskMove*)GetSubTask())->GetTarget();
	return m_vTarget;
}
float CTaskMoveWalkRoundVehicle::GetTargetRadius() const
{
	return CTaskMoveGoToPoint::ms_fTargetRadius;
}
void CTaskMoveWalkRoundVehicle::SetTarget(const CPed* UNUSED_PARAM(pPed), Vec3V_In UNUSED_PARAM(vTarget), const bool UNUSED_PARAM(bForce))
{

}
bool CTaskMoveWalkRoundVehicle::ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent)
{
	return CTask::ShouldAbort(iPriority, pEvent);
}


CTask::FSM_Return CTaskMoveWalkRoundVehicle::ProcessPreFSM()
{
	// The vehicle pointer is used by HasVehicleMoved() and maybe other functions
	// without checking for NULL. Hopefully we can just quit the task if the vehicle
	// got destroyed.
	if(!m_pVehicle)
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}


CTask::FSM_Return CTaskMoveWalkRoundVehicle::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed * pPed = GetPed();

	if(pPed->GetIsInVehicle())
		return FSM_Quit;

	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnEnter
				return StateInitial_OnEnter(pPed);
			FSM_OnUpdate
				return StateInitial_OnUpdate(pPed);
		FSM_State(State_FollowingPointRoute)
			FSM_OnEnter
				return StateFollowingPointRoute_OnEnter(pPed);
			FSM_OnUpdate
				return StateFollowingPointRoute_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return CTaskMoveWalkRoundVehicle::StateInitial_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	m_vInitialCarPos = VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetPosition());
	m_vInitialCarRightAxis = VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetRight());
	m_vInitialCarFwdAxis = VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetForward());

	m_TestClearedEntityTimer.Set(fwTimer::GetTimeInMilliseconds(), m_iTestClearedEntityFrequency);

	return FSM_Continue;
}
CTask::FSM_Return CTaskMoveWalkRoundVehicle::StateInitial_OnUpdate(CPed* pPed)
{
	if(!CalculateRouteRoundVehicle(pPed))
	{
		return FSM_Quit;
	}

	SetState(State_FollowingPointRoute);

	return FSM_Continue;
}
CTask::FSM_Return CTaskMoveWalkRoundVehicle::StateFollowingPointRoute_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	CTaskMoveFollowPointRoute * pRouteTask = rage_new CTaskMoveFollowPointRoute(
		m_fMoveBlendRatio,
		*m_pPointRoute,
		CTaskMoveFollowPointRoute::TICKET_SINGLE,
		CTaskMoveFollowPointRoute::ms_fTargetRadius,
		0.0f, //CTaskMoveFollowPointRoute::ms_fSlowDownDistance,
		false,
		true,
		false);

	SetNewTask(pRouteTask);

	return FSM_Continue;
}
CTask::FSM_Return CTaskMoveWalkRoundVehicle::StateFollowingPointRoute_OnUpdate(CPed * pPed)
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
	{
		return FSM_Quit;
	}

	if(HasVehicleMoved())
	{
		SetState(State_Initial);
	}

	// Optionally test whether we have now cleared the entity
	// Lightweight test against only this object, so performed at a higher
	// frequency than the recalc route
	if( ((m_iFlags & Flag_TestHasClearedEntity)!=0) && m_TestClearedEntityTimer.IsOutOfTime() )
	{
		m_TestClearedEntityTimer.Set(fwTimer::GetTimeInMilliseconds(), m_iTestClearedEntityFrequency);

		// Optionally test whether we have cleared the entity
		// If so then we can quit the task
		if( TestHasClearedEntity(pPed) )
		{
			return FSM_Quit;
		}
	}

	return FSM_Continue;
}

bool CTaskMoveWalkRoundVehicle::HasVehicleMoved() const
{
	const float fMoveEps = 0.25f*0.25f;
	const float fOrientEps = Cosf(( DtoR * 5.0f));

	// If the vehicle has moved/reorientated, then restart
	if((m_vInitialCarPos - VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetPosition())).XYMag2() > fMoveEps)
		return true;
	if(DotProduct(m_vInitialCarRightAxis, VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetRight())) < fOrientEps)
		return true;
	if(DotProduct(m_vInitialCarFwdAxis, VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetForward())) < fOrientEps)
		return true;

	return false;
}

bool CTaskMoveWalkRoundVehicle::CalculateRouteRoundVehicle(CPed* pPed)
{
	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const Vector2 vPedPos2d(vPedPos.x, vPedPos.y);
	Vector2 vTargetPos2d(m_vTarget.x, m_vTarget.y);

	CVehicleHullAI vehicleHull(m_pVehicle);
	const float fHalfWidth = pPed->GetCapsuleInfo()->GetHalfWidth();
	vehicleHull.Init(fHalfWidth);

	// If the target position is inside the hull, then push it outside in the direction
	// from the ped to the target (if this vector is zero length, we can quit)
	if(vehicleHull.LiesInside(vTargetPos2d))
	{
		Vector2 vMoveVec = vTargetPos2d - vPedPos2d;
		if(vMoveVec.Mag2() < SMALL_FLOAT)
			return false;
		vMoveVec.Normalize();
		vehicleHull.MoveOutside(vTargetPos2d, vMoveVec);
	}

	if(!m_pPointRoute)
		m_pPointRoute = rage_new CPointRoute();
	else
		m_pPointRoute->Clear();

	const u32 iLosTestFlags = ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE;

	if(vehicleHull.CalcShortestRouteToTarget(pPed, vPedPos2d, vTargetPos2d, m_pPointRoute, vPedPos.z, iLosTestFlags, fHalfWidth))
	{
		// remove the final (target) point
		if(m_pPointRoute->GetSize() > 1)
			m_pPointRoute->Remove( m_pPointRoute->GetSize()-1 );

		// Remove route sections which will be completed instantly in the first frame
		// This avoids the movement anims getting locked into the wrong directional walkstart
		while(m_pPointRoute->GetSize() > 0 && (m_pPointRoute->Get(0)- vPedPos).XYMag2() < CTaskMoveFollowPointRoute::ms_fTargetRadius*CTaskMoveFollowPointRoute::ms_fTargetRadius)
		{
			m_pPointRoute->Remove(0);
		}

		return (m_pPointRoute->GetSize() > 0);
	}

	return false;
}

bool CTaskMoveWalkRoundVehicle::TestHasClearedEntity(CPed * pPed)
{
	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	u32 iLosFlags = (ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_BOX_VEHICLE_TYPE);

	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	WorldProbe::CShapeTestHitPoint result;
	WorldProbe::CShapeTestResults capsuleResult(result);

	capsuleDesc.SetResultsStructure(&capsuleResult);
	capsuleDesc.SetCapsule(vPedPos, m_vTarget, pPed->GetCapsuleInfo()->GetHalfWidth());
	capsuleDesc.SetIncludeFlags(iLosFlags);
	capsuleDesc.SetDoInitialSphereCheck(true);
	capsuleDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
	capsuleDesc.SetIncludeEntity(m_pVehicle);
	capsuleDesc.SetIsDirected(true);

	if(WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
		return false;

	return true;
}


