//Game heades.
#include "fwanimation/animmanager.h"
#include "AI/Ambient/ConditionalAnimManager.h"
#include "PedGroup/Formation.h"
#include "PedGroup/PedGroup.h"
#include "Peds/ped.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Physics/GtaArchetype.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Task/Animation/TaskAnims.h"
#include "Task/Combat/CombatManager.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Movement/TaskCollisionResponse.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Movement/TaskGoTo.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/General/TaskSecondary.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "Task/Movement/TaskMoveFollowLeader.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Task/Weapons/WeaponController.h"
#include "Vehicles/Vehicle.h"
#include "vehicleAi/task/TaskVehicleGoTo.h"
#include "vehicleAi/task/TaskVehicleGoToAutomobile.h"
#include "weapons/Weapon.h"

AI_OPTIMISATIONS()

void CEntitySeekPosCalculatorXYOffsetFixed::ComputeEntitySeekPos(const CPed& UNUSED_PARAM(ped), const CEntity& entity, Vector3& vSeekPos) const
{
	vSeekPos = VEC3V_TO_VECTOR3(entity.GetTransform().GetPosition()) + m_vOffset;
}

void CEntitySeekPosCalculatorXYOffsetRotated::ComputeEntitySeekPos(const CPed& UNUSED_PARAM(ped), const CEntity& entity, Vector3& vSeekPos) const
{
	vSeekPos = VEC3V_TO_VECTOR3(entity.GetTransform().Transform(RCC_VEC3V(m_vOffset)));
}

void CEntitySeekPosCalculatorRadiusAngleOffset::ComputeEntitySeekPos(const CPed& UNUSED_PARAM(ped), const CEntity& entity, Vector3& vSeekPos) const
{
	//Compute the target heading.
	const Vector3 vForward=VEC3V_TO_VECTOR3(entity.GetTransform().GetB());
    float fEntityHeading=fwAngle::GetRadianAngleBetweenPoints(vForward.x,vForward.y,0.0f,0.0f);
    fEntityHeading=fwAngle::LimitRadianAngle(fEntityHeading);
	float fTargetHeading=fEntityHeading+m_fAngle;
    fTargetHeading=fwAngle::LimitRadianAngle(fTargetHeading);
    
    //Compute the direction from the target.
	Vector3 vDir;
	vDir.x=-rage::Sinf(fTargetHeading);
    vDir.y=rage::Cosf(fTargetHeading);
    vDir.z=0;
    vDir*=m_fRadius;
    
    //Compute the target point.
    vSeekPos = VEC3V_TO_VECTOR3(entity.GetTransform().GetPosition())+vDir;
}

//////////////////////////
// CClonedSeekEntityInfos
//////////////////////////

CTask* CClonedSeekEntityStandardInfo::CreateLocalTask(fwEntity *UNUSED_PARAM(pEntity))
{
	netObject* pNetObj = static_cast<CNetObjEntity*>(NetworkInterface::GetObjectManager().GetNetworkObject(m_entityId));

	if (AssertVerify(pNetObj && pNetObj->GetEntity()))
	{
		 TTaskMoveSeekEntityStandard* pSeekTask = rage_new TTaskMoveSeekEntityStandard(pNetObj->GetEntity(), m_iSeekTime, m_iPeriod, m_fTargetRadius, m_fSlowDownDistance, m_fFollowNodeThresholdHeightChange, m_bFaceEntityWhenDone);
		 pSeekTask->SetUseLargerSearchExtents(m_bUseLargerSearchExtents);	
		 pSeekTask->SetUseAdaptiveUpdateFreq(m_bUseAdaptiveUpdateFreq);
		 pSeekTask->SetPreferPavements(m_bPreferPavements);
		 pSeekTask->SetMoveBlendRatio(m_fMoveBlendRatio);
			
         CTask *pTask = rage_new CTaskComplexControlMovement( pSeekTask );

         return pTask;
	}

	return NULL;
}

CTask* CClonedSeekEntityLastNavMeshIntersectionInfo::CreateLocalTask(fwEntity *UNUSED_PARAM(pEntity))
{
	netObject* pNetObj = static_cast<CNetObjEntity*>(NetworkInterface::GetObjectManager().GetNetworkObject(m_entityId));

	if (AssertVerify(pNetObj && pNetObj->GetEntity()))
	{
		TTaskMoveSeekEntityLastNavMeshIntersection* pSeekTask = rage_new TTaskMoveSeekEntityLastNavMeshIntersection(pNetObj->GetEntity(), m_iSeekTime, m_iPeriod, m_fTargetRadius, m_fSlowDownDistance, m_fFollowNodeThresholdHeightChange, m_bFaceEntityWhenDone);
		pSeekTask->SetUseLargerSearchExtents(m_bUseLargerSearchExtents);	
		pSeekTask->SetUseAdaptiveUpdateFreq(m_bUseAdaptiveUpdateFreq);
		pSeekTask->SetPreferPavements(m_bPreferPavements);
		pSeekTask->SetMoveBlendRatio(m_fMoveBlendRatio);

		CTask *pTask = rage_new CTaskComplexControlMovement( pSeekTask );

		return pTask;
	}

	return NULL;
}

CTask* CClonedSeekEntityOffsetRotateInfo::CreateLocalTask(fwEntity *UNUSED_PARAM(pEntity))
{
	netObject* pNetObj = static_cast<CNetObjEntity*>(NetworkInterface::GetObjectManager().GetNetworkObject(m_entityId));

	if (AssertVerify(pNetObj && pNetObj->GetEntity()))
	{
		TTaskMoveSeekEntityXYOffsetRotated *pSeekTask = rage_new TTaskMoveSeekEntityXYOffsetRotated(pNetObj->GetEntity(), m_iSeekTime, m_iPeriod, m_fTargetRadius, m_fSlowDownDistance, m_fFollowNodeThresholdHeightChange, m_bFaceEntityWhenDone);
		pSeekTask->SetUseLargerSearchExtents(m_bUseLargerSearchExtents);	
		pSeekTask->SetUseAdaptiveUpdateFreq(m_bUseAdaptiveUpdateFreq);
		pSeekTask->SetPreferPavements(m_bPreferPavements);
		pSeekTask->SetMoveBlendRatio(m_fMoveBlendRatio);

        CTask *pTask = rage_new CTaskComplexControlMovement( pSeekTask );

        return pTask;
	}

	return NULL;
}

CTask* CClonedSeekEntityOffsetFixedInfo::CreateLocalTask(fwEntity *UNUSED_PARAM(pEntity))
{
	netObject* pNetObj = static_cast<CNetObjEntity*>(NetworkInterface::GetObjectManager().GetNetworkObject(m_entityId));

	if (AssertVerify(pNetObj && pNetObj->GetEntity()))
	{
		TTaskMoveSeekEntityXYOffsetFixed *pSeekTask = rage_new TTaskMoveSeekEntityXYOffsetFixed(pNetObj->GetEntity(), m_iSeekTime, m_iPeriod, m_fTargetRadius, m_fSlowDownDistance, m_fFollowNodeThresholdHeightChange, m_bFaceEntityWhenDone);
		pSeekTask->SetUseLargerSearchExtents(m_bUseLargerSearchExtents);	
		pSeekTask->SetUseAdaptiveUpdateFreq(m_bUseAdaptiveUpdateFreq);
		pSeekTask->SetPreferPavements(m_bPreferPavements);
		pSeekTask->SetMoveBlendRatio(m_fMoveBlendRatio);

        CTask *pTask = rage_new CTaskComplexControlMovement( pSeekTask );

        return pTask;
	}

	return NULL;
}

CTask* CClonedSeekEntityRadiusAngleInfo::CreateLocalTask(fwEntity *UNUSED_PARAM(pEntity))
{
	netObject* pNetObj = static_cast<CNetObjEntity*>(NetworkInterface::GetObjectManager().GetNetworkObject(m_entityId));

	if (AssertVerify(pNetObj && pNetObj->GetEntity()))
	{
		TTaskMoveSeekEntityRadiusAngleOffset *pSeekTask = rage_new TTaskMoveSeekEntityRadiusAngleOffset(pNetObj->GetEntity(), m_iSeekTime, m_iPeriod, m_fTargetRadius, m_fSlowDownDistance, m_fFollowNodeThresholdHeightChange, m_bFaceEntityWhenDone);
		pSeekTask->SetUseLargerSearchExtents(m_bUseLargerSearchExtents);	
		pSeekTask->SetUseAdaptiveUpdateFreq(m_bUseAdaptiveUpdateFreq);
		pSeekTask->SetPreferPavements(m_bPreferPavements);
		pSeekTask->SetMoveBlendRatio(m_fMoveBlendRatio);

        CTask *pTask = rage_new CTaskComplexControlMovement( pSeekTask );

        return pTask;
	}

	return NULL;
}

//////////////////////////
//FOLLOW LEADER IN FORMATION
//////////////////////////

const float CTaskFollowLeaderInFormation::ms_fMaxDistanceToLeader		= -1;
const float CTaskFollowLeaderInFormation::ms_fXYDiffSqrToSetNewTarget	= 1.0f;
const float CTaskFollowLeaderInFormation::ms_fZDiffToSetNewTarget		= 4.0f;
const float CTaskFollowLeaderInFormation::ms_fMinSecondsBetweenCoverAttempts = 3.0f;
const s32 CTaskFollowLeaderInFormation::ms_iPossibleLookAtFreq		= 8000;
const s32 CTaskFollowLeaderInFormation::ms_iLookAtChance				= 75;

CTaskFollowLeaderInFormation::CTaskFollowLeaderInFormation(CPedGroup* pPedGroup, CPed* pLeader, const float fMaxDistanceToLeader, const s32 iPreferredSeat)
	: m_pPedGroup(pPedGroup),
	m_pLeader(pLeader),
	m_fMaxDistanceToLeader(fMaxDistanceToLeader),
	m_bUseAdditionalAimTask(false), 
	m_iPreferredSeat(iPreferredSeat),
	m_fSecondsSinceAttemptedSeekCover(ms_fMinSecondsBetweenCoverAttempts),
	m_fTimeLeaderNotMoving(0.0f),
	m_vLeaderPositionOnCoverStart(Vector3::ZeroType)
{
	m_LookAtTimer.Set(fwTimer::GetTimeInMilliseconds(), fwRandom::GetRandomNumberInRange(1000, ms_iPossibleLookAtFreq));

	SetInternalTaskType(CTaskTypes::TASK_FOLLOW_LEADER_IN_FORMATION);
}

CTaskFollowLeaderInFormation::~CTaskFollowLeaderInFormation()
{
}

bool CTaskFollowLeaderInFormation::IsValidForMotionTask(CTaskMotionBase& task) const
{
	return (task.IsOnFoot() || task.IsInWater());
}

CTask::FSM_Return CTaskFollowLeaderInFormation::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin

		FSM_State(State_Initial)
			FSM_OnEnter
				return StateInitial_OnEnter(pPed);
			FSM_OnUpdate
				return StateInitial_OnUpdate(pPed);

		FSM_State(State_EnteringVehicle)
			FSM_OnEnter
				return StateEnteringVehicle_OnEnter(pPed);
			FSM_OnUpdate
				return StateEnteringVehicle_OnUpdate(pPed);

		FSM_State(State_InVehicle)
			FSM_OnEnter
				return StateInVehicle_OnEnter(pPed);
			FSM_OnUpdate
				return StateInVehicle_OnUpdate(pPed);

		FSM_State(State_ExitingVehicle)
			FSM_OnEnter
				return StateExitingVehicle_OnEnter(pPed);
			FSM_OnUpdate
				return StateExitingVehicle_OnUpdate(pPed);

		FSM_State(State_SeekingLeader)
			FSM_OnEnter
				return StateSeekingLeader_OnEnter(pPed);
			FSM_OnUpdate
				return StateSeekingLeader_OnUpdate(pPed);

		FSM_State(State_StandingByLeader)
			FSM_OnEnter
				return StateStandingByLeader_OnEnter(pPed);
			FSM_OnUpdate
				return StateStandingByLeader_OnUpdate(pPed);

		FSM_State(State_InFormation)
			FSM_OnEnter
				return StateInFormation_OnEnter(pPed);
			FSM_OnUpdate
				return StateInFormation_OnUpdate(pPed);

		FSM_State(State_SeekCover)
			FSM_OnEnter
				return StateSeekCover_OnEnter(pPed);
			FSM_OnUpdate
				return StateSeekCover_OnUpdate(pPed);
			FSM_OnExit
				return StateSeekCover_OnExit();

	FSM_End
}

CTask::FSM_Return CTaskFollowLeaderInFormation::StateInitial_OnEnter(CPed * pPed)
{
	// JB: still necessary?
	AbortMoveTask(pPed);

	return FSM_Continue;
}
CTask::FSM_Return CTaskFollowLeaderInFormation::StateInitial_OnUpdate(CPed * pPed)
{
	Assert(m_pLeader);

	// If in a vehicle & its the leader's then stay put - otherwise leave the car
	if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle())
	{
		if((m_pLeader->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && m_pLeader->GetMyVehicle()==pPed->GetMyVehicle()) || LeaderOnSameBoat(pPed))
		{
			SetState(State_InVehicle);
		}
		else
		{
			SetState(State_ExitingVehicle);
		}
	}
	else
	{
		SetState(State_InFormation);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskFollowLeaderInFormation::StateEnteringVehicle_OnEnter(CPed * pPed)
{
	Assert(m_pLeader->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && m_pLeader->GetMyVehicle());

	VehicleEnterExitFlags enterFlags = VehicleEnterExitFlags();
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WarpIntoLeadersVehicle))
	{
		enterFlags.BitSet().Set(CVehicleEnterExitFlags::Warp);
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_WarpIntoLeadersVehicle, false);
	}

	if(m_iPreferredSeat > 0)
	{
		SetNewTask(rage_new CTaskEnterVehicle(m_pLeader->GetMyVehicle(), SR_Prefer, m_iPreferredSeat, enterFlags));
	}
	else
	{
		SetNewTask(rage_new CTaskEnterVehicle(m_pLeader->GetMyVehicle(), SR_Any, m_iPreferredSeat, enterFlags));
	}
	return FSM_Continue;
}
CTask::FSM_Return CTaskFollowLeaderInFormation::StateEnteringVehicle_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	Assert(m_pLeader);

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Initial);
	}
	// If the leader has left their vehicle whilst we are attempting to enter it, then restart
	else if(!(m_pLeader->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && m_pLeader->GetMyVehicle()))
	{
		SetState(State_Initial);
	}
	return FSM_Continue;
}
CTask::FSM_Return CTaskFollowLeaderInFormation::StateInVehicle_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	return FSM_Continue;
}
CTask::FSM_Return CTaskFollowLeaderInFormation::StateInVehicle_OnUpdate(CPed * pPed)
{
	if ((!m_pLeader->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) || !m_pLeader->GetMyVehicle()) && !LeaderOnSameBoat(pPed))
	{
		SetState(State_ExitingVehicle);
	}

	return FSM_Continue;
}
CTask::FSM_Return CTaskFollowLeaderInFormation::StateExitingVehicle_OnEnter(CPed * pPed)
{
	VehicleEnterExitFlags vehicleFlags;
	SetNewTask( rage_new CTaskExitVehicle(pPed->GetMyVehicle(), vehicleFlags) );
	return FSM_Continue;
}
CTask::FSM_Return CTaskFollowLeaderInFormation::StateExitingVehicle_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Avoid infinite recursion when impossible to leave a vehicle
		if(GetTimeInState() > 0.0f)
			SetState(State_Initial);
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskFollowLeaderInFormation::StateSeekingLeader_OnEnter(CPed* pPed)
{
	TTaskMoveSeekEntityStandard * pSeekEntity = rage_new TTaskMoveSeekEntityStandard(
		m_pLeader,
		TTaskMoveSeekEntityStandard::ms_iMaxSeekTime,
		TTaskMoveSeekEntityStandard::ms_iPeriod,
		ComputeSeekTargetRadius(pPed)
	);
	pSeekEntity->SetUseLargerSearchExtents(m_bUseLargerSearchExtents);
	SetNewTask( rage_new CTaskComplexControlMovement(pSeekEntity) );

	return FSM_Continue;
}
CTask::FSM_Return CTaskFollowLeaderInFormation::StateSeekingLeader_OnUpdate(CPed * pPed)
{
	Assert(m_pLeader);

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Initial);
	}
	else
	{
		CheckEnterVehicle(pPed);
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskFollowLeaderInFormation::StateStandingByLeader_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	SetNewTask( rage_new CTaskDoNothing(500) );
	return FSM_Continue;
}
CTask::FSM_Return CTaskFollowLeaderInFormation::StateStandingByLeader_OnUpdate(CPed * pPed)
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_SeekingLeader);
	}
	else
	{
		CheckEnterVehicle(pPed);
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskFollowLeaderInFormation::StateInFormation_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	CTaskMoveBeInFormation * pMoveInFormation = m_pPedGroup ? rage_new CTaskMoveBeInFormation(m_pPedGroup) : rage_new CTaskMoveBeInFormation(m_pLeader);
	CTask* pAdditionalTask;
	if(m_bUseAdditionalAimTask)
	{
		pAdditionalTask = rage_new CTaskAimSweep(fwRandom::GetRandomTrueFalse() ? CTaskAimSweep::Aim_Left : CTaskAimSweep::Aim_Right, false);
	}
	else
	{
		pAdditionalTask = rage_new CTaskAmbientClips(CTaskAmbientClips::Flag_PlayIdleClips | CTaskAmbientClips::Flag_PlayBaseClip, CONDITIONALANIMSMGR.GetConditionalAnimsGroup("BUDDY_AI_IDLES"));
	}

	SetNewTask( rage_new CTaskComplexControlMovement(pMoveInFormation, pAdditionalTask, CTaskComplexControlMovement::TerminateOnMovementTask) );

	m_fTimeLeaderNotMoving = 0.0f;
	m_vLeaderPositionOnCoverStart.Zero();
		 
	return FSM_Continue;
}
CTask::FSM_Return CTaskFollowLeaderInFormation::StateInFormation_OnUpdate(CPed * pPed)
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_SeekingLeader);
	}
	else
	{
		if (CheckEnterVehicle(pPed))
		{
			return FSM_Continue;
		}
		else if (ShouldSeekCover(pPed) != eSCR_None)
		{
			SetState(State_SeekCover);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskFollowLeaderInFormation::StateSeekCover_OnEnter(CPed* pPed)
{
	// Find an enemy attacking the formation leader.
	CEntity* pHostileEntity = NULL;
	if (m_pLeader->IsPlayer())
	{
		CCombatTaskManager* pCombatTaskManager = CCombatManager::GetCombatTaskManager();
		if ( pCombatTaskManager )
		{
			pHostileEntity = pCombatTaskManager->GetPedInCombatWithPlayer(*m_pLeader);
		}
	}
	else
	{ 
		pHostileEntity = m_pLeader->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget();
	}

	s32 iSearchType = CCover::CS_PREFER_PROVIDE_COVER;

	const Vector3 vLeaderPosition = VEC3V_TO_VECTOR3(m_pLeader->GetTransform().GetPosition());

	m_vLeaderPositionOnCoverStart=vLeaderPosition;

	CTaskCover* pCoverTask = NULL;
	if (pHostileEntity)
	{
		// If we have a hostile attacker, take cover from them.
		pCoverTask = rage_new CTaskCover(CAITarget(pHostileEntity));
	}
	else
	{
		// Otherwise find cover near the leader.
		if (m_pLeader)
		{
			CCoverPoint* pLeaderCoverPoint = m_pLeader->GetCoverPoint();
			
			if(ShouldSeekCover(pPed) == eSCR_LeaderStopped)
			{
				Vec3V vDir = pLeaderCoverPoint ? pLeaderCoverPoint->GetCoverDirectionVector() : m_pLeader->GetTransform().GetForward();
				Vector3 vFakeThreatPosition = vLeaderPosition + (VEC3V_TO_VECTOR3(vDir) * 15.f);
				pCoverTask = rage_new CTaskCover(CAITarget(vFakeThreatPosition), CTaskCover::CF_DisableAimingAndPeeking);
				iSearchType = CCover::CS_ANY;
			}
			else if (pLeaderCoverPoint)
			{
				Vector3 vFakeThreatPosition = vLeaderPosition + (VEC3V_TO_VECTOR3(pLeaderCoverPoint->GetCoverDirectionVector()) * 15.f); 
				pCoverTask = rage_new CTaskCover(CAITarget(vFakeThreatPosition));
			}
		}
	}

	if (pCoverTask)
	{
		pCoverTask->SetSearchPosition(vLeaderPosition);
		pCoverTask->SetSearchFlags(CTaskCover::CF_SeekCover | CTaskCover::CF_DontMoveIfNoCoverFound | CTaskCover::CF_CoverSearchByPos);
		pCoverTask->SetMoveBlendRatio(MOVEBLENDRATIO_SPRINT);
		pCoverTask->SetSearchType(iSearchType);

		SetNewTask( pCoverTask ); 
	}
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskFollowLeaderInFormation::StateSeekCover_OnUpdate(CPed* pPed)
{	
	// Quit if the leader entered a vehicle
	CheckEnterVehicle(pPed);

	if ((ShouldSeekCover(pPed) != eSCR_None) && GetSubTask())
	{
		return FSM_Continue; 
	}
	else
	{
		SetState(State_SeekingLeader);
		return FSM_Continue;
	}
}

CTask::FSM_Return CTaskFollowLeaderInFormation::StateSeekCover_OnExit()
{
	m_fSecondsSinceAttemptedSeekCover = 0.f;
	return FSM_Continue;
}

CTask::FSM_Return CTaskFollowLeaderInFormation::ProcessPreFSM()
{
	m_fSecondsSinceAttemptedSeekCover += fwTimer::GetTimeStep();

	if(m_pLeader && (m_pLeader->GetVelocity().Mag2() < rage::square(0.1f)))
	{
		m_fTimeLeaderNotMoving += fwTimer::GetTimeStep();
	}
	else
	{
		m_fTimeLeaderNotMoving = 0.0f;
	}

	CPed *pPed = GetPed(); //Get the ped ptr.

	if (!m_pLeader)
	{
		AbortMoveTask(pPed);
		return FSM_Quit;
	}

	const float fDistanceToLeaderSqr = DistSquared(m_pLeader->GetTransform().GetPosition(), pPed->GetTransform().GetPosition()).Getf();
	if (m_fMaxDistanceToLeader > 0.0f && fDistanceToLeaderSqr > square(m_fMaxDistanceToLeader))
	{
		AbortMoveTask(pPed);
		return FSM_Quit;
	}

	ProcessLookAt(pPed);
	return FSM_Continue;
}

void CTaskFollowLeaderInFormation::AbortMoveTask(CPed * pPed)
{
	// Abort any movement tasks which are currently running, a CMoveSimpleDoNothing will be created.
	CTask * pMoveTask = pPed->GetPedIntelligence()->GetActiveMovementTask();
	if(pMoveTask)
		pMoveTask->MakeAbortable( ABORT_PRIORITY_URGENT, NULL);
}
void CTaskFollowLeaderInFormation::ProcessLookAt(CPed * pPed)
{
	if(m_LookAtTimer.IsOutOfTime() && pPed->GetIkManager().GetLookAtEntity()==NULL)
	{
		int iNextTimeVal = -1;
		if(fwRandom::GetRandomNumberInRange(0, 100) < ms_iLookAtChance)
		{
			CPed * pPedToLookAt = m_pLeader;
			if (CPedGroup * pGroup = pPed->GetPedsGroup())
			{
				// Randomly look at either the leader or another group member.
				// The leader is more likely chose of whom to look at.
				if(fwRandom::GetRandomNumberInRange(0, 100) < 80)
				{
					pPedToLookAt = m_pLeader;
				}
				else
				{
					const int iNumMembers = pGroup->GetGroupMembership()->CountMembers()-1;
					pPedToLookAt = pGroup->GetGroupMembership()->GetMember(fwRandom::GetRandomNumberInRange(0, iNumMembers));
					if(pPedToLookAt==pPed)
						pPedToLookAt = m_pLeader;
				}
			}

			if(pPedToLookAt)
			{
				const int iMaxLookAtTime = (pPedToLookAt==m_pLeader) ? 8000 : 3000;
				const int iLookAtTime = fwRandom::GetRandomNumberInRange(iMaxLookAtTime/2, iMaxLookAtTime);
				const s32 iBlendInTime = fwRandom::GetRandomNumberInRange(500,1250);
				const s32 iBlendOutTime = Min(fwRandom::GetRandomNumberInRange(500,800), iBlendInTime);
				pPed->GetIkManager().LookAt(0, pPedToLookAt, iLookAtTime, BONETAG_HEAD, NULL, 0, iBlendInTime, iBlendOutTime);
				iNextTimeVal = iLookAtTime;
			}
		}
		if(iNextTimeVal==-1)
			iNextTimeVal = fwRandom::GetRandomNumberInRange(1000, ms_iPossibleLookAtFreq);
		m_LookAtTimer.Set(fwTimer::GetTimeInMilliseconds(), iNextTimeVal);
	}
}

bool CTaskFollowLeaderInFormation::CheckEnterVehicle(CPed * pPed)
{
	// if leader is in a vehicle and is nearby we can try to enter the vehicle
	// Dont do this if we have ped group, events handle entering/exiting vehicles
	if (m_pPedGroup == NULL && m_pLeader->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && m_pLeader->GetMyVehicle())
	{
		const Vector3 vToLeader = VEC3V_TO_VECTOR3(m_pLeader->GetTransform().GetPosition() - pPed->GetTransform().GetPosition());
		if (vToLeader.Mag2() < 25.0f)
		{
			// there is space for us as a passenger
			if(m_pLeader->GetMyVehicle()->GetSeatManager()->CountFreeSeats(false, true, pPed))
			{
				AbortMoveTask(pPed);
				SetState(State_EnteringVehicle);
				return true;
			}
		}
	}
	return false;
}

bool CTaskFollowLeaderInFormation::IsLeaderInRangedCombat(const CPed* pLeaderPed)
{
	const bool bMelee = false;
	return IsLeaderInCombatType(pLeaderPed, bMelee);
}

bool CTaskFollowLeaderInFormation::IsLeaderInMeleeCombat(const CPed* pLeaderPed)
{
	const bool bMelee = true;
	return IsLeaderInCombatType(pLeaderPed, bMelee);
}

bool CTaskFollowLeaderInFormation::IsLeaderInCombatType(const CPed* pLeaderPed, bool bMelee)
{
	if (pLeaderPed)
	{
		CEntity* pHostileTarget = NULL;
		if (pLeaderPed->IsPlayer())
		{
			CCombatTaskManager* pCombatTaskManager = CCombatManager::GetCombatTaskManager();
			if ( pCombatTaskManager )
			{
				pHostileTarget = pCombatTaskManager->GetPedInCombatWithPlayer(*pLeaderPed);
			}
		}
		else
		{
			pHostileTarget = pLeaderPed->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget();
		}

		if ( pHostileTarget && pHostileTarget->GetIsTypePed() )
		{
			CPed* pHostilePed = static_cast<CPed*>(pHostileTarget);
			if (pHostilePed->GetWeaponManager() && pHostilePed->GetWeaponManager()->GetEquippedWeaponInfo())
			{
				return (bMelee == pHostilePed->GetWeaponManager()->GetEquippedWeaponInfo()->GetIsMelee());
			}
		}
	}
	return false;
}


bool CTaskFollowLeaderInFormation::IsLeaderInCover() const
{
	if (m_pLeader)
	{
		const bool bAssertIfNotFound = false;
		return m_pLeader->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_IN_COVER, PED_TASK_PRIORITY_MAX, bAssertIfNotFound) != NULL;
	}

	return false;
}

bool CTaskFollowLeaderInFormation::IsLeaderWanted() const
{
	if (m_pLeader)
	{
		if (m_pLeader->GetPlayerInfo())
		{
			return m_pLeader->GetPlayerInfo()->GetWanted().GetWantedLevel() > WANTED_CLEAN;
		}
	}
	return false;
}


CTaskFollowLeaderInFormation::eSeekCoverReason CTaskFollowLeaderInFormation::ShouldSeekCover(const CPed* pPed) const
{
	if (m_fSecondsSinceAttemptedSeekCover < ms_fMinSecondsBetweenCoverAttempts)
	{
		return eSCR_None;
	}

	if (!pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanUseCover))
	{
		return eSCR_None;
	}

	// If leader is in ranged combat and leader is stopped
	if( m_pLeader && IsLeaderInRangedCombat(m_pLeader) && CanSeekCoverIfLeaderHasStopped(pPed) )
	{
		return eSCR_LeaderStopped;
	}
		
	// If a ped is blocking non-temporary events, we need to handle cover and such here.
	//  Otherwise the event responses will do everything we need. [3/5/2013 mdawe]
	if (IsLeaderInRangedCombat(m_pLeader) && pPed->GetBlockingOfNonTemporaryEvents())
	{
		// Make sure we still follow the leader if they move far enough away
		if (NetworkInterface::IsGameInProgress())
		{
			if (m_pLeader)
			{
				const Vec3V vLeaderPos = m_pLeader->GetTransform().GetPosition();
				const Vec3V vFollowerPos = pPed->GetTransform().GetPosition();
				TUNE_GROUP_FLOAT(COVER_TUNE, ms_fMaxDistanceToLeaderToBreakCover, 10.0f, 0.0f, 20.0f, 0.01f);
				const ScalarV scMaxDistFromLeaderSq = ScalarVFromF32(square(ms_fMaxDistanceToLeaderToBreakCover));
				const ScalarV scDistFromLeaderSq = DistSquared(vLeaderPos, vFollowerPos);
				if (IsLessThanAll(scDistFromLeaderSq, scMaxDistFromLeaderSq))
				{
					return eSCR_LeaderInCombat;
				}
			}
		}
		else
		{
			return eSCR_LeaderInCombat;
		}
	}

	// Make sure you appear to hide from the cops if the player is.
	if (IsLeaderInCover() && IsLeaderWanted())
	{
		return eSCR_LeaderWanted;
	}

	return eSCR_None;
}

bool CTaskFollowLeaderInFormation::CanSeekCoverIfLeaderHasStopped(const CPed* pPed) const
{
	//! Seek cover if leader hasn't moved and ped allows it.
	if( pPed->GetPedResetFlag(CPED_RESET_FLAG_IfLeaderStopsSeekCover) )
	{
		TUNE_GROUP_FLOAT(COVER_TUNE, ms_fMaxDistanceToLeaderToSeekCoverIfStopped, 16.0f, 0.0f, 20.0f, 0.01f);
		TUNE_GROUP_FLOAT(COVER_TUNE, ms_fMaxDistanceToLeaderStopPositionToBreakCover, 5.0f, 0.0f, 20.0f, 0.01f);

		const Vec3V vLeaderPos = m_pLeader->GetTransform().GetPosition();
		const Vec3V vFollowerPos = pPed->GetTransform().GetPosition();
		const ScalarV scMaxDistFromLeaderSq = ScalarVFromF32(square(ms_fMaxDistanceToLeaderToSeekCoverIfStopped));
		const ScalarV scDistFromLeaderSq = DistSquared(vLeaderPos, vFollowerPos);
		if (IsLessThanAll(scDistFromLeaderSq, scMaxDistFromLeaderSq))
		{
			//! If we are already running the cover task, then leader must move at least x metres from position they instigated cover
			//! before breaking out.
			if(GetState()==State_SeekCover)
			{
				const Vec3V vFollowerStartCoverPos = VECTOR3_TO_VEC3V(m_vLeaderPositionOnCoverStart);
				const ScalarV scMaxDistCoverStartSq = ScalarVFromF32(square(ms_fMaxDistanceToLeaderStopPositionToBreakCover));
				const ScalarV scDistFromCoverStartSq = DistSquared(vLeaderPos, vFollowerStartCoverPos);
				if (IsLessThanAll(scDistFromCoverStartSq, scMaxDistCoverStartSq))
				{
					return true;
				}
			}
			else
			{
				TUNE_GROUP_FLOAT(COVER_TUNE, ms_fTimeToEnterCoverIfLeaderHasStopped, 2.0f, 0.0f, 20.0f, 0.01f);
				if(m_fTimeLeaderNotMoving > ms_fTimeToEnterCoverIfLeaderHasStopped)
				{
					return true;
				}
			}
		}
	}

	return false;
}

float CTaskFollowLeaderInFormation::ComputeSeekTargetRadius(const CPed* pPed) const
{
	// Default to reasonable value
	float fTargetRadius = 4.0f;

	// If set to seek cover on leader stop and leader is under fire
	if( pPed->GetPedResetFlag(CPED_RESET_FLAG_IfLeaderStopsSeekCover) && m_pLeader && IsLeaderInRangedCombat(m_pLeader) )
	{
		// Use custom value when moving to seek leader
		TUNE_GROUP_FLOAT(COVER_TUNE, ms_fMaxLeaderStopsSeekTargetRadius, 8.0f, 0.0f, 20.0f, 0.01f);
		fTargetRadius = ms_fMaxLeaderStopsSeekTargetRadius;
	}
	
	return fTargetRadius;
}

bool CTaskFollowLeaderInFormation::LeaderOnSameBoat(const CPed* pPed, const float fMaxDistanceAway) const
{
	// B*2234577: Stay in vehicle if it's a boat and the leader is still standing on the same boat within reasonable distance.
	CVehicle *pPedVehicle = pPed->GetVehiclePedInside();
	if (pPedVehicle && pPedVehicle->InheritsFromBoat() && m_pLeader->GetGroundPhysical() && m_pLeader->GetGroundPhysical()->GetIsTypeVehicle())
	{
		CVehicle *pLeaderVehicle = ((CVehicle*)m_pLeader->GetGroundPhysical());
		float fDistanceBetweenPeds = (VEC3V_TO_VECTOR3(m_pLeader->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())).Mag();
		if (pPedVehicle == pLeaderVehicle && fDistanceBetweenPeds <= fMaxDistanceAway)
		{
			return true;
		}
	}
	return false;
}

//////////////////////////
//FOLLOW LEADER ANY MEANS
//////////////////////////

CTaskFollowLeaderAnyMeans::CTaskFollowLeaderAnyMeans(CPedGroup* pPedGroup, CPed* pLeader)
	: m_pPedGroup(pPedGroup),
	m_pLeader(pLeader)
{
	SetInternalTaskType(CTaskTypes::TASK_FOLLOW_LEADER_ANY_MEANS);
}

CTaskFollowLeaderAnyMeans::~CTaskFollowLeaderAnyMeans()
{
}

bool CTaskFollowLeaderAnyMeans::ShouldAbort( const AbortPriority iPriority, const aiEvent * pEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	bool bLeaderRacingAway = false;
	CPedGroup* pPedGroup = pPed->GetPedsGroup();

	// Group following peds don't acknowledge events that will take them into combat unless the ped is effectively in combat
	if( pEvent && pPedGroup )
	{
		CPed* pLeader = pPedGroup->GetGroupMembership()->GetLeader();
		if( pLeader && pPed != pLeader )
		{	
			if( pPed->GetPedIntelligence()->GetEventResponseTask() &&
				pPed->GetPedIntelligence()->GetEventResponseTask()->GetTaskType() == CTaskTypes::TASK_THREAT_RESPONSE )
			{
				if( pLeader->IsAPlayerPed() )
				{
					bLeaderRacingAway = pPed->GetPedIntelligence()->GetDefensiveArea()->IsActive() &&
						!pPed->GetPedIntelligence()->GetDefensiveArea()->IsPointInDefensiveArea(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
				}
				else
				{
					bLeaderRacingAway = !pLeader->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT);
				}

				bool bIgnoreDueToWater = false;
				if( pPed->GetIsInWater() )
				{
					bIgnoreDueToWater = true; 
				}

				// If this is a ped in a group shooting who has strayed too far from the player, abort so the ped can catch up.
				if( iPriority != CTask::ABORT_PRIORITY_IMMEDIATE &&
					(bIgnoreDueToWater || bLeaderRacingAway) && (!pEvent || ((CEvent*)pEvent)->GetEventType() != EVENT_SCRIPT_COMMAND))
				{
					// IF there's no physical response task, ignore the event completely
					if( pPed->GetPedIntelligence()->GetPhysicalEventResponseTask() == NULL )
					{
						static_cast<CEvent*>(const_cast<aiEvent*>(pEvent))->Tick();
						return false;
					}
					// If there is a physical response task just remove the behavioural response
					else 
					{
						pPed->GetPedIntelligence()->RemoveEventResponseTask();
					}
				}
			}
		}
	}
	return true;
}

bool CTaskFollowLeaderAnyMeans::IsValidForMotionTask(CTaskMotionBase& task) const
{
	return (task.IsOnFoot() || task.IsInWater());
}

CTask::FSM_Return CTaskFollowLeaderAnyMeans::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin

		FSM_State(State_Initial)
			FSM_OnEnter
				return StateInitial_OnEnter(pPed);
			FSM_OnUpdate
				return StateInitial_OnUpdate(pPed);

		FSM_State(State_FollowingInFormation)
			FSM_OnEnter
				return StateFollowingInFormation_OnEnter(pPed);
			FSM_OnUpdate
				return StateFollowingInFormation_OnUpdate(pPed);

	FSM_End
}


CTask::FSM_Return CTaskFollowLeaderAnyMeans::StateInitial_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	SetNewTask(rage_new CTaskPause(1));
	return FSM_Continue;
}
CTask::FSM_Return CTaskFollowLeaderAnyMeans::StateInitial_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_FollowingInFormation);
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskFollowLeaderAnyMeans::StateFollowingInFormation_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	SetNewTask(rage_new CTaskFollowLeaderInFormation(m_pPedGroup, m_pLeader));
	return FSM_Continue;
}
CTask::FSM_Return CTaskFollowLeaderAnyMeans::StateFollowingInFormation_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskFollowLeaderAnyMeans::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	CPedGroup* pPedsGroup = pPed->GetPedsGroup();
	if(m_pLeader.Get() == NULL || (pPedsGroup && pPedsGroup->GetGroupMembership()->GetLeader() != m_pLeader))
	{
		return FSM_Quit;
	}

	//*********************************************************************************
	// Cops following other cops in formation are culled far away, if they have orders

	if( pPed->GetPedType() == PEDTYPE_COP && m_pLeader->GetPedType() == PEDTYPE_COP )
	{
		weaponAssert(pPed->GetWeaponManager());
		weaponAssert(m_pLeader->GetWeaponManager());
		bool bIsLeaderArmed = m_pLeader->GetWeaponManager()->GetEquippedWeapon() ? m_pLeader->GetWeaponManager()->GetEquippedWeapon()->GetWeaponInfo()->GetIsArmed() : false;
		bool bIsPedUnarmed = pPed->GetWeaponManager()->GetEquippedWeapon() ? pPed->GetWeaponManager()->GetEquippedWeapon()->GetWeaponInfo()->GetIsUnarmed() : false;
		if( bIsLeaderArmed && bIsPedUnarmed )
		{
			pPed->GetWeaponManager()->EquipBestWeapon();
		}
	}

	return FSM_Continue;
}


////////////////////////
//SEEK ENTITY AIMING
////////////////////////

CTaskSeekEntityAiming::CTaskSeekEntityAiming(const CEntity* pEntity, const float fSeekRadius, const float fAimRadius)
	: m_pTargetEntity(pEntity),
	m_fSeekRadius(fSeekRadius),
	m_fAimRadius(fAimRadius),
	m_bUseLargerSearchExtents(false)
{
	Assert(m_fAimRadius>=m_fSeekRadius);
	SetInternalTaskType(CTaskTypes::TASK_SEEK_ENTITY_AIMING);
}

CTaskSeekEntityAiming::~CTaskSeekEntityAiming()
{
}

CTask::FSM_Return CTaskSeekEntityAiming::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnEnter
				return StateInitial_OnEnter(pPed);
			FSM_OnUpdate
				return StateInitial_OnUpdate(pPed);
		FSM_State(State_Seeking)
			FSM_OnEnter
				return StateSeeking_OnEnter(pPed);
			FSM_OnUpdate
				return StateSeeking_OnUpdate(pPed);
		FSM_State(State_StandingStill)
			FSM_OnEnter
				return StateStandingStill_OnEnter(pPed);
			FSM_OnUpdate
				return StateStandingStill_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return CTaskSeekEntityAiming::StateInitial_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	return FSM_Continue;
}
CTask::FSM_Return CTaskSeekEntityAiming::StateInitial_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	if(!m_pTargetEntity)
		return FSM_Quit;

	SetState(State_Seeking);
	return FSM_Continue;
}
CTask::FSM_Return CTaskSeekEntityAiming::StateSeeking_OnEnter(CPed * pPed)
{
	if(!m_pTargetEntity)
		return FSM_Quit;

	Vector3 vDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition() - m_pTargetEntity->GetTransform().GetPosition());
	vDiff.z = 0;
	const float f2 = vDiff.Mag2();

	TTaskMoveSeekEntityStandard * pMoveTask = rage_new TTaskMoveSeekEntityStandard(
		m_pTargetEntity,
		TTaskMoveSeekEntityStandard::ms_iMaxSeekTime,
		TTaskMoveSeekEntityStandard::ms_iPeriod,
		m_fSeekRadius
	);

	pMoveTask->SetUseLargerSearchExtents(m_bUseLargerSearchExtents);

	CTask * pSubTask = NULL;
	if(f2 < rage::square(m_fAimRadius))
	{
		pSubTask = rage_new CTaskGun(CWeaponController::WCT_Aim, CTaskTypes::TASK_AIM_GUN_ON_FOOT, CWeaponTarget(m_pTargetEntity), 60.0f);
	}
	else
	{
		pSubTask = rage_new CTaskDoNothing(-1);
	}

	CTaskComplexControlMovement * pTask = rage_new CTaskComplexControlMovement( pMoveTask, pSubTask, CTaskComplexControlMovement::TerminateOnMovementTask );
	SetNewTask(pTask);

	return FSM_Continue;
}
CTask::FSM_Return CTaskSeekEntityAiming::StateSeeking_OnUpdate(CPed * pPed)
{
	if(!m_pTargetEntity)
		return FSM_Quit;

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_StandingStill);
	}
	else
	{
		Assert(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT);

		if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
		{
			Vector3 vDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition() - m_pTargetEntity->GetTransform().GetPosition());
			vDiff.z=0;
			const float f2=vDiff.Mag2();

			CTaskComplexControlMovement* pControlTask = ((CTaskComplexControlMovement*)GetSubTask());
			if( f2 < rage::square(m_fAimRadius) )
			{
				// If the current subtask is aiming, switch it to nothing
				if( pControlTask->GetSubTask() && pControlTask->GetSubTask()->GetTaskType() == CTaskTypes::TASK_DO_NOTHING )
				{
					CTaskGun* pTaskGun = rage_new CTaskGun( CWeaponController::WCT_Aim, CTaskTypes::TASK_AIM_GUN_ON_FOOT, CWeaponTarget(m_pTargetEntity), 60.0f );
					pControlTask->SetNewMainSubtask( pTaskGun );
				}
			}
		}
	}
	return FSM_Continue;
}
CTask::FSM_Return CTaskSeekEntityAiming::StateStandingStill_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	if(!m_pTargetEntity)
		return FSM_Quit;

	CTaskGun* pFireSubtask = rage_new CTaskGun(CWeaponController::WCT_Aim, CTaskTypes::TASK_AIM_GUN_ON_FOOT, CWeaponTarget(m_pTargetEntity), 60.0f);
	SetNewTask( rage_new CTaskComplexControlMovement( rage_new CTaskMoveStandStill(2000), pFireSubtask , CTaskComplexControlMovement::TerminateOnMovementTask ) );

	return FSM_Continue;
}
CTask::FSM_Return CTaskSeekEntityAiming::StateStandingStill_OnUpdate(CPed * pPed)
{
	if(!m_pTargetEntity)
		return FSM_Quit;

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Seeking);
	}
	else
	{
		Assert(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT);

		if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
		{
			Vector3 vDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition() - m_pTargetEntity->GetTransform().GetPosition());
			vDiff.z=0;
			const float f2=vDiff.Mag2();

			CTaskComplexControlMovement* pControlTask = ((CTaskComplexControlMovement*)GetSubTask());
			if( f2 < rage::square(m_fAimRadius) )
			{
				// If the current subtask is aiming, switch it to nothing
				if( pControlTask->GetSubTask() && pControlTask->GetSubTask()->GetTaskType() == CTaskTypes::TASK_DO_NOTHING )
				{
					CTaskGun* pTaskGun = rage_new CTaskGun( CWeaponController::WCT_Aim, CTaskTypes::TASK_AIM_GUN_ON_FOOT, CWeaponTarget(m_pTargetEntity), 60.0f );
					pControlTask->SetNewMainSubtask( pTaskGun );
				}
			}
		}
	}
	return FSM_Continue;
}

CTaskInfo *CTaskSeekEntityAiming::CreateQueriableState() const
{
	return rage_new CClonedSeekEntityAimingInfo(m_pTargetEntity.Get(), m_fSeekRadius, m_fAimRadius);
}

const float CClonedSeekEntityAimingInfo::MAX_SEEK_RADIUS = 5.0f;
const float CClonedSeekEntityAimingInfo::MAX_AIM_RADIUS = 50.0f;

