#include "math/angmath.h"

#include "TaskMoveFollowLeader.h"

#include "PedGroup/Formation.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedMoveBlend/PedMoveBlendInWater.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Movement/TaskSeekEntity.h"


AI_OPTIMISATIONS()

#define MOVE_IN_FORMATION_RADIUS 0.35f		// equal to what PED_RADIUS was before it became data driven

//****************************************************************************
//	CTaskMoveBeInFormation
//	Task to keep a ped in their position in a formation.  It can be used as
//	the top-level general movement task for any task which requires the ped
//	to maintain their formation whilst doing something else.
//****************************************************************************

// If the difference between the ped's pos or goto task's pos is greater than this
// then the current move task is updated, or a new move task is started.
const float CTaskMoveBeInFormation::ms_fXYDiffSqrToSetNewTarget = 1.0f; // NB_UseAdaptiveSetTargetEpsilon is set, so this should be ok. //(2.0f * 2.0f) * 1.5f;	//(2.0f * 2.0f) * 2.0f;
const float CTaskMoveBeInFormation::ms_fXYDiffSqrToSetNewTargetWhenWalkingAlongsideLeader = 1.0f;
const float CTaskMoveBeInFormation::ms_fZDiffToSetNewTarget = 3.0f;
const float CTaskMoveBeInFormation::ms_fXYDiffSqrToSwitchToNavMeshRoute = (2.0f * 2.0f) * 2.0f;
const u32 CTaskMoveBeInFormation::ms_iTestWalkAlongProbeFreq = 2000;
const bool CTaskMoveBeInFormation::ms_bStopPlayerGroupMembersWalkingOverEdges = true;
const float CTaskMoveBeInFormation::ms_fLeaveInteriorSeekEntityTargetRadiusXY = 1.2f;

// On GTA5 we are no longer able to strafe when unarmed.. Therefore we've turned off strafing
// in this task altogether.  This could possibly be reimplemented via the reposition-move, or
// enabled at a later date if unarmed strafes are made available again.
const bool CTaskMoveBeInFormation::ms_bHasUnarmedStrafingAnims = false;

CTaskMoveBeInFormation::CTaskMoveBeInFormation(CPedGroup * pGroup) :
	CTaskMove(MOVEBLENDRATIO_STILL),
	m_pPedGroup(pGroup),
	m_bFollowingGroup(true),
	m_pNonGroupLeader(NULL),
	m_bUseLargerSearchExtents(false)
{
	Assert(m_pPedGroup);
	Init();
	SetInternalTaskType(CTaskTypes::TASK_MOVE_BE_IN_FORMATION);
}

CTaskMoveBeInFormation::CTaskMoveBeInFormation(CPed * pNonGroupLeader) :
	CTaskMove(MOVEBLENDRATIO_STILL),
	m_pPedGroup(NULL),
	m_bFollowingGroup(false),
	m_pNonGroupLeader(pNonGroupLeader),
	m_bUseLargerSearchExtents(false)
{
	Assert(m_pNonGroupLeader);

	Init();
	SetInternalTaskType(CTaskTypes::TASK_MOVE_BE_IN_FORMATION);
}

void
CTaskMoveBeInFormation::Init()
{
	m_fDesiredMoveBlendRatio = MOVEBLENDRATIO_RUN;
	m_fFormationMoveBlendRatio = MOVEBLENDRATIO_RUN;
	m_fLeadersLastMoveBlendRatio = MOVEBLENDRATIO_STILL;
	m_vLastTargetPos = Vector3(10000.0f, 10000.0f, 10000.0f);
	m_vLastWalkAlongSideLeaderTarget = Vector3(10000.0f, 10000.0f, 10000.0f);
	m_bSetStrafing = false;
	m_bLeaderHasJustStoppedMoving = false;
	m_bIsTryingToWalkAlongsideLeader = false;
	m_bLosFromLeaderToTargetIsClear = false;
	m_bLosToFindGroundIsClear = false;
	m_bNonGroupWalkAlongsideLeader = false;
	m_bUseLargerSearchExtents = false;
	m_hLeaderToTargetProbe.Reset();
	m_hTargetToGroundProbe.Reset();
	m_fNonGroupSpacing = MOVE_IN_FORMATION_RADIUS * 10.0f;
	m_fStartCrowdingDistFromPlayer = 0.0f;
	m_vStartCrowdingLocation = Vector3(0.0f,0.0f,0.0f);
	m_fMaxSlopeNavigable = 0.0f;
}
CTaskMoveBeInFormation::~CTaskMoveBeInFormation()
{
	CancelProbes();
}

#if !__FINAL
void
CTaskMoveBeInFormation::Debug() const
{
#if DEBUG_DRAW
	if(m_bIsTryingToWalkAlongsideLeader)
	{
		Color32 iCol = IsPositionClearToWalkAlongsideLeader() ? Color_green2 : Color_red2;
		grcDebugDraw::Line(m_vLastWalkAlongSideLeaderTarget - Vector3(0.25f,0.0f,0.0f), m_vLastWalkAlongSideLeaderTarget + Vector3(0.25f,0.0f,0.0f), iCol);
		grcDebugDraw::Line(m_vLastWalkAlongSideLeaderTarget - Vector3(0.0f,0.25f,0.0f), m_vLastWalkAlongSideLeaderTarget + Vector3(0.0f,0.25f,0.0f), iCol);
	}

	grcDebugDraw::Sphere(m_vDesiredPosition, m_fDesiredTargetRadius, Color_burlywood, false);

	if (m_pPedGroup && m_pPedGroup->GetFormation())
	{
		m_pPedGroup->GetFormation()->Debug(m_pPedGroup);
	}
#endif

	if(GetSubTask())
	{
		GetSubTask()->Debug();
	}
}
#endif

Vector3
CTaskMoveBeInFormation::GetTarget() const
{
	return m_vDesiredPosition;
}

float
CTaskMoveBeInFormation::GetTargetRadius() const
{
	return m_fDesiredTargetRadius;
}

bool
CTaskMoveBeInFormation::NeedsToLeaveInterior(CPed * pPed)
{
	CPed * pLeader = GetLeader();
	if (!taskVerifyf(pLeader, "This ped should have a leader!"))
	{
		return false;
	}

	const bool bLeaderInInterior = pLeader->GetPortalTracker()->IsInsideInterior();
	const bool bPedInInterior = pPed->GetPortalTracker()->IsInsideInterior();
	if(bPedInInterior == bLeaderInInterior)
		return false;

	if(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition() - pLeader->GetTransform().GetPosition()).XYMag2() < ms_fLeaveInteriorSeekEntityTargetRadiusXY*ms_fLeaveInteriorSeekEntityTargetRadiusXY)
		return false;
	
	return true;
}

bool
CTaskMoveBeInFormation::GetCrowdRoundLeaderWhenStationary(CPed * pPed)
{
	// Don't do this when in water, as if peds make no effort to pathfind to their
	// leader then they might drown.  Also it looks a bit like synchronised-swimming.
	CPed * pLeader = GetLeader();
	if(pPed->GetPedResetFlag( CPED_RESET_FLAG_IsDrowning ) || (pLeader && pLeader->GetPedResetFlag( CPED_RESET_FLAG_IsDrowning )))
	{
		return false;
	}
	// Don't crowd around if we are in an interior
	if(pPed->GetPortalTracker()->IsInsideInterior())
		return false;

	// Not when the leader is up a ladder
	if(pLeader && pLeader->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_GO_TO_AND_CLIMB_LADDER, true))
		return false;

	// Not when the leader is more-or-less directly above the ped
	const float fDiffZ = (pLeader->GetTransform().GetPosition() - pPed->GetTransform().GetPosition()).GetZf();
	if(fDiffZ > 2.0f)
		return false;

	if(m_bFollowingGroup)
	{
		Assert(m_pPedGroup);

		if(pLeader && pLeader->GetPortalTracker()->IsInsideInterior())
			return false;

		const CPedFormation * pFormation = m_pPedGroup->GetFormation();
		return pFormation->GetCrowdRoundLeaderWhenStationary();
	}

	if(m_pNonGroupLeader && m_pNonGroupLeader->GetPortalTracker()->IsInsideInterior())
		return false;

	return true;
}

bool
CTaskMoveBeInFormation::GetWalkAlongsideLeaderWhenClose(CPed * pPed)
{
	CPed * pLeader = GetLeader();
	if(pPed->GetPedResetFlag( CPED_RESET_FLAG_IsDrowning ) || (pLeader && pLeader->GetPedResetFlag( CPED_RESET_FLAG_IsDrowning )))
	{
		return false;
	}

	if(! pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_WalkAlongsideLeaderWhenClose) )
	{
		return false;
	}

	// Not when the leader is up a ladder
	if(pLeader && pLeader->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_GO_TO_AND_CLIMB_LADDER, true))
	{
		return false;
	}

	// When following a group this depends upon the formation setting
	if(m_bFollowingGroup)
	{
		Assert(m_pPedGroup);
		const CPedFormation * pFormation = m_pPedGroup->GetFormation();
		return pFormation->GetWalkAlongsideLeaderWhenClose();
	}
	// Otherwise can turn this on/off for non-group following
	return m_bNonGroupWalkAlongsideLeader;
}

bool
CTaskMoveBeInFormation::GetAddLeadersVelocityOntoGotoTarget()
{
	if(m_bFollowingGroup)
	{
		Assert(m_pPedGroup);
		return m_pPedGroup->GetFormation()->GetAddLeadersVelocityOntoGotoTarget();
	}

	return true;
}

CPed *
CTaskMoveBeInFormation::GetLeader()
{
	if(m_bFollowingGroup)
	{
		Assert(m_pPedGroup);
		CPed * pLeader = m_pPedGroup->GetGroupMembership()->GetLeader();
		return pLeader;
	}

	return m_pNonGroupLeader;
}

float
CTaskMoveBeInFormation::GetMinDistToAdjustSpeed()
{
	if(m_bFollowingGroup)
	{
		Assert(m_pPedGroup);
		return m_pPedGroup->GetFormation()->GetAdjustSpeedMinDist();
	}

	//JR was 3, now that is the default tuning in the ped nav capabilities.
	return GetPed()->GetPedIntelligence()->GetNavCapabilities().GetNonGroupMinDistToAdjustSpeed();
}
float
CTaskMoveBeInFormation::GetMaxDistToAdjustSpeed()
{
	if(m_bFollowingGroup)
	{
		Assert(m_pPedGroup);
		return m_pPedGroup->GetFormation()->GetAdjustSpeedMaxDist();
	}

	//JR was 16, now that is the default tuning in the ped nav capabilities.
	return GetPed()->GetPedIntelligence()->GetNavCapabilities().GetNonGroupMaxDistToAdjustSpeed();
}

float
CTaskMoveBeInFormation::GetFormationSpacing()
{
	if(m_bFollowingGroup)
	{
		Assert(m_pPedGroup);
		return m_pPedGroup->GetFormation()->GetSpacing();
	}

	return m_fNonGroupSpacing;
}

CTask::FSM_Return 
CTaskMoveBeInFormation::ProcessPreFSM()
{
	const CEntity* pLeader = GetLeader();
	if (!pLeader)
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveBeInFormation::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Initial);
			FSM_OnUpdate
				return Initial_OnUpdate(pPed);
		FSM_State(State_AttachedToSomething);
			FSM_OnEnter
				return AttachedToSomething_OnEnter(pPed);
			FSM_OnUpdate
				return AttachedToSomething_OnUpdate(pPed);
		FSM_State(State_FollowingLeader);
			FSM_OnEnter
				return FollowingLeader_OnEnter(pPed);
			FSM_OnUpdate
				return FollowingLeader_OnUpdate(pPed);
		FSM_State(State_Surfacing);
			FSM_OnEnter
				return Surfacing_OnEnter(pPed);
			FSM_OnUpdate
				return Surfacing_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return
CTaskMoveBeInFormation::Initial_OnUpdate(CPed* pPed)
{
	if(!UpdatePedPositioning(pPed))
	{
		ASSERT_ONLY( if(m_bFollowingGroup) printf("Ped no longer in group in CTaskMoveBeInFormation::CreateFirstSubTask.  Will return NULL."); )
		return FSM_Quit;
	}
	if(pPed->GetIsAttached())
	{
		// Stop group peds from trying to move if they're attached to something
		SetState(State_AttachedToSomething);
	}
	else
	{
		SetState(State_FollowingLeader);
	}

	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveBeInFormation::AttachedToSomething_OnEnter(CPed * pPed)
{
	SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed) );
	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveBeInFormation::AttachedToSomething_OnUpdate(CPed * pPed)
{
	if(pPed->GetIsAttached())
	{
		if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
		{
			// Stop group peds from trying to move if they're attached to something
			SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed) );
		}
	}
	else
	{
		SetState(State_FollowingLeader);
	}

	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveBeInFormation::FollowingLeader_OnEnter(CPed* pPed)
{
	// If part of a group then randomize navigation points slightly to reduce clumping problems
	if(m_bFollowingGroup)
		pPed->SetPedResetFlag( CPED_RESET_FLAG_RandomisePointsDuringNavigation, true );

	SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, pPed) );

	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveBeInFormation::FollowingLeader_OnUpdate(CPed* pPed)
{
	Assert(GetSubTask());

	CPed * pLeader = GetLeader();
	if(pLeader)
	{
		if(pLeader->GetIsSwimming() && pPed->GetIsSwimming())
		{
			static dev_float fEps = 0.5f;

			CTaskMotionPed * pLeaderMotionTask = (CTaskMotionPed*) pLeader->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_PED);
			CTaskMotionPed * pPedMotionTask = (CTaskMotionPed*) pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_PED);

			// I wanted to use the CInWaterEventScanner's submerged timer, but this doesn't seem to update correctly,
			// at least not in the shallow swimming-pool situation of url:bugstar:1542217
			if(pLeaderMotionTask && pPedMotionTask &&
				pLeaderMotionTask->GetState()!=CTaskMotionPed::State_Diving &&
				 pPedMotionTask->GetState()==CTaskMotionPed::State_Diving)
			{
				Vec3V vStartPos = pPed->GetTransform().GetPosition();
				Vec3V vEndPos = pPed->GetTransform().GetPosition() + Vec3V(0.f,0.f, 150.f);
				Vec3V vOutPos;

				if (CVfxHelper::TestLineAgainstWater(vStartPos, vEndPos, vOutPos))
				{
					if (vOutPos.GetZf() > vStartPos.GetZf() + fEps)
					{
						SetState(State_Surfacing);
						return FSM_Continue;
					}
				}
			}
		}
	}

	// If part of a group then randomize navigation points slightly to reduce clumping problems
	if(m_bFollowingGroup)
		pPed->SetPedResetFlag( CPED_RESET_FLAG_RandomisePointsDuringNavigation, true );

	//*************************************************************************************
	//	Subtask has just quit.  This is equivalent to CreateNextSubtask in the old system

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || (GetSubTask() && GetSubTask()->GetIsFlagSet(aiTaskFlags::IsAborted)) )
	{
		// Respect some ped's wishes to not face their leader exactly.
		float fHeadingTolerance = pPed->GetPedIntelligence()->GetNavCapabilities().GetMoveFollowLeaderHeadingTolerance();
		bool bDontFaceLeader = false;
		if (fHeadingTolerance > SMALL_FLOAT)
		{
			bDontFaceLeader = fabs(fwAngle::LimitRadianAngle(SubtractAngleShorter(pPed->GetDesiredHeading(), m_fDesiredHeading))) < fHeadingTolerance * DtoR; 
		}
		CTaskTypes::eTaskType nextTaskType = pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING) || bDontFaceLeader ? 
			CTaskTypes::TASK_MOVE_STAND_STILL : CTaskTypes::TASK_MOVE_ACHIEVE_HEADING;
		CTask * pNewSubTask = NULL;

		switch(GetSubTask()->GetTaskType())
		{
			case CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH:
			{
				if(NeedsToLeaveInterior(pPed))
				{
					pNewSubTask = CreateSubTask(CTaskTypes::TASK_MOVE_SEEK_ENTITY_STANDARD, pPed);
				}
				else
				{
					pNewSubTask = CreateSubTask(nextTaskType, pPed);
				}
				break;
			}
			case CTaskTypes::TASK_MOVE_SEEK_ENTITY_STANDARD:
			{
				pNewSubTask = CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed);
				break;
			}
			case CTaskTypes::TASK_MOVE_GO_TO_POINT:
			{
				if(NeedsToLeaveInterior(pPed))
				{
					pNewSubTask = CreateSubTask(CTaskTypes::TASK_MOVE_SEEK_ENTITY_STANDARD, pPed);
				}
				else
				{
					pNewSubTask = CreateSubTask(nextTaskType, pPed);
				}
				break;
			}
			case CTaskTypes::TASK_MOVE_ACHIEVE_HEADING:
			case CTaskTypes::TASK_MOVE_STAND_STILL:
			case CTaskTypes::TASK_MOVE_CROWD_AROUND_LOCATION:
			{
				if(NeedsToLeaveInterior(pPed))
				{
					pNewSubTask = CreateSubTask(CTaskTypes::TASK_MOVE_SEEK_ENTITY_STANDARD, pPed);
					break;
				}
				else
				{
					if(GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_STAND_STILL && pPed->GetIsAttached())
					{
						// Stop group peds from trying to move if they're attached to something
						pNewSubTask = CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed);
						break;
					}
					else
					{
						// Don't crowd round the leader when on a boat, or we'll probably fall overboard
						bool bOnBoat = pPed->GetGroundPhysical() && pPed->GetGroundPhysical()->GetIsTypeVehicle() && ((CVehicle*)pPed->GetGroundPhysical())->GetVehicleType()==VEHICLE_TYPE_BOAT;

						if(GetCrowdRoundLeaderWhenStationary(pPed) && !bOnBoat)
						{
							CPed * pLeader = GetLeader();
							if (taskVerifyf(pLeader, "This Ped should have a leader!"))
							{
								Vector2 vCurrMbr;
								pLeader->GetMotionData()->GetCurrentMoveBlendRatio(vCurrMbr);
								if(vCurrMbr.Mag()==0.0f)
								{
									pNewSubTask = CreateSubTask(CTaskTypes::TASK_MOVE_CROWD_AROUND_LOCATION, pPed);
									break;
								}
								else
								{
									pNewSubTask = CreateSubTask(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, pPed);
									break;
								}
							}
						}
						else
						{
							if(NeedsToLeaveInterior(pPed))
							{
								pNewSubTask = CreateSubTask(CTaskTypes::TASK_MOVE_SEEK_ENTITY_STANDARD, pPed);
								break;
							}
							else
							{
								// Check to see if we're too far from our desired location.
								// If we are too far and we're also too close to the leader, move to your desired position.
								if (!IsLooseFormation() && !IsInPosition() && IsTooCloseToLeader())
								{
									pNewSubTask = CreateSubTask(CTaskTypes::TASK_MOVE_GO_TO_POINT, pPed);
								}
								else
								{
									// Turn to your desired heading if you're facing too far away.
									float fHeadingEps = Max(DtoR * 4.0f, pPed->GetPedIntelligence()->GetNavCapabilities().GetMoveFollowLeaderHeadingTolerance() * DtoR);
									float fHeadingDelta = SubtractAngleShorter(pPed->GetDesiredHeading(), m_fDesiredHeading);
									fHeadingDelta = fwAngle::LimitRadianAngle(fHeadingDelta);
									if(Abs(fHeadingDelta) > fHeadingEps)
									{
										pNewSubTask = CreateSubTask(nextTaskType, pPed);
										break;
									}
									else
									{
										pNewSubTask = CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed);
										break;
									}
								}
							}
						}
					}
				}
			}
		}
		if(pNewSubTask)
		{
			SetNewTask(pNewSubTask);
		}
	}

	//***************************************************************************
	//	Code previously in ControlSubtask()

	// Stop group peds from trying to move if they're attached to something
	if(pPed->GetIsAttached())
	{
		SetState(State_AttachedToSomething);
		return FSM_Continue;
	}

	m_bLeaderHasJustStoppedMoving = false;

	// Check that the ped is still a member of the ped-group.  This should be handled by the controlling task.
	// If no longer a member of his group this task will quit.
	if(m_bFollowingGroup)
	{
		Assert(m_pPedGroup);
		CPedGroupMembership * pMembership = m_pPedGroup->GetGroupMembership();
		if(!pMembership || !pMembership->IsMember(pPed))
		{
#if __DEV
			printf("CTaskMoveBeInFormation is quitting because ped is no longer in their pedgroup.\n");
#endif
			if(MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
				return FSM_Quit;
			return FSM_Continue;
		}
	}

	if(GetSubTask())
	{
		if(GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_CROWD_AROUND_LOCATION)
		{
			CPed * pLeader = GetLeader();
			Assert(pLeader);
			if(pLeader)
				((CTaskMoveCrowdAroundLocation*)GetSubTask())->SetCrowdLocation(VEC3V_TO_VECTOR3(pLeader->GetTransform().GetPosition()));
		}

		else if(GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_ACHIEVE_HEADING)
		{
			((CTaskMoveAchieveHeading*)GetSubTask())->SetHeading(m_fDesiredHeading);
		}
	}

	float fLeaderMoveBlendRatio = MOVEBLENDRATIO_STILL;
	if(pLeader)
	{
		fLeaderMoveBlendRatio = pLeader->GetMotionData()->GetCurrentMbrY();
	}
	
	m_bLeaderHasJustStoppedMoving = (CPedMotionData::GetIsStill(fLeaderMoveBlendRatio) && !CPedMotionData::GetIsStill(m_fLeadersLastMoveBlendRatio));
	m_fLeadersLastMoveBlendRatio = fLeaderMoveBlendRatio;

	// Get the position that this ped should be within the formation
	if(!UpdatePedPositioning(pPed))
	{
		if(MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
			return FSM_Quit;
	}

	Vector3 vTargetPos;
	GetTargetPosAdjustedForLeaderMovement(vTargetPos);

	// If we are idling/turning, then we may want to start a new movement task before the idle/turn completes.
	// Note that if a task is returned, then the current subtask will have already be made abortable.
	CTask * pNewSubTask = MaybeInterruptIdleTaskToStartMoving(pPed, vTargetPos);
	if(pNewSubTask && pNewSubTask != GetSubTask())
	{
		SetNewTask(pNewSubTask);
		return FSM_Continue;
	}

	// Maybe update the target in the movement subtask
	// Note that if a task is returned, then the current subtask will have already be made abortable.
	pNewSubTask = MaybeUpdateMovementSubTask(pPed, vTargetPos);
	if(pNewSubTask && pNewSubTask != GetSubTask())
	{
		SetNewTask(pNewSubTask);
		return FSM_Continue;
	}

	SetStrafingState(pPed, vTargetPos);
	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveBeInFormation::Surfacing_OnEnter(CPed * pPed)
{
	static dev_float fEps = 0.5f;

	Vec3V vStartPos = pPed->GetTransform().GetPosition();
	Vec3V vEndPos = pPed->GetTransform().GetPosition() + Vec3V(0.f,0.f, 150.f);
	Vec3V vOutPos;

	if (CVfxHelper::TestLineAgainstWater(vStartPos, vEndPos, vOutPos))
	{
		if (vOutPos.GetZf() > vStartPos.GetZf() + fEps)
		{
			vOutPos.SetZf( vOutPos.GetZf() + 2.0f );
			CTaskMoveGoToPoint* pTaskMove = rage_new CTaskMoveGoToPoint( m_fMoveBlendRatio, VEC3V_TO_VECTOR3(vOutPos), 1.0f, false);
			pTaskMove->SetDontExpandHeightDeltaUnderwater(true);
			pTaskMove->SetTargetHeightDelta(0.5f);

			SetNewTask(pTaskMove);
			return FSM_Continue;
		}
	}

	SetNewTask( rage_new CTaskMoveStandStill(1000) );
	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveBeInFormation::Surfacing_OnUpdate(CPed * pPed)
{
	CTaskMotionPed * pPedMotionTask = (CTaskMotionPed*) pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_PED);

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask() || (pPedMotionTask && pPedMotionTask->GetState()!=CTaskMotionPed::State_Diving))
	{
		SetState(State_FollowingLeader);
	}

	return FSM_Continue;
}

//
float CTaskMoveBeInFormation::GetTargetRadiusForNavMeshTask(CPed* pPed)
{
	float fRadius = Max(m_fDesiredTargetRadius - 1.0f, 0.0f);

	if (m_pPedGroup && m_pPedGroup->GetFormation() && m_pPedGroup->GetFormation()->GetFormationType() == CPedFormationTypes::FORMATION_LOOSE)
	{
		float fRadiusOverride = pPed->GetPedIntelligence()->GetNavCapabilities().GetLooseFormationTargetRadiusOverride();
		if (fRadiusOverride > 0.0f)
		{
			fRadius = fRadiusOverride;
		}
	}
	return fRadius;
}

CTask * CTaskMoveBeInFormation::CreateSubTask(const int iSubTaskType, CPed* pPed)
{
	Vector3 vTargetPos;
	GetTargetPosAdjustedForLeaderMovement(vTargetPos);

	// If part of a group then randomize navigation points slightly to reduce clumping problems
	if(m_bFollowingGroup)
		pPed->SetPedResetFlag( CPED_RESET_FLAG_RandomisePointsDuringNavigation, true );

	switch(iSubTaskType)
	{
		case CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH:
		{
			CPed * pLeader = GetLeader();
			Assert(pLeader);

			bool bStandStillAtEnd = false;

			if (!pPed->WillPedEnterWater() && (pLeader->GetIsInWater() || pLeader->GetIsSwimming()))
			{
				bStandStillAtEnd = true;
			}

			float fMoveRatio = GetMoveBlendRatioToUse(pPed, vTargetPos, pPed->GetMotionData()->GetCurrentMoveBlendRatio().y, false);
			if(fMoveRatio > MOVEBLENDRATIO_STILL)
			{
				float fRadius = GetTargetRadiusForNavMeshTask(pPed);

				const float fSlowdownDistance = 1.5f;
				CTaskMoveFollowNavMesh * pNavTask = rage_new CTaskMoveFollowNavMesh(
					fMoveRatio,
					m_vDesiredPosition,
					fRadius,
					fSlowdownDistance
					);
				pNavTask->SetDontStandStillAtEnd(!bStandStillAtEnd);
				pNavTask->SetNeverEnterWater(!pPed->WillPedEnterWater());
				pNavTask->SetMaxSlopeNavigable(m_fMaxSlopeNavigable);
				pNavTask->SetUseLargerSearchExtents(m_bUseLargerSearchExtents);
				pNavTask->SetStopExactly(true);

				if (pPed->GetPedIntelligence()->GetNavCapabilities().AlwaysKeepMovingWhilstFollowingLeaderPath())
				{
					pNavTask->SetKeepMovingWhilstWaitingForPath(true);
				}

				// This handles the case where a ped is standing in shallow water.
				// Since he's already on a water poly, he doesn't mind continuing in water.
				
				if(!pPed->WillPedEnterWater() && !pPed->GetPedResetFlag( CPED_RESET_FLAG_IsDrowning ) && pLeader && pLeader->GetPedResetFlag( CPED_RESET_FLAG_IsDrowning ))
					pNavTask->SetNeverStartInWater(true);

				return pNavTask;
			}
			else
			{
				return rage_new CTaskMoveStandStill(2000);
			}
		}
		case CTaskTypes::TASK_MOVE_SEEK_ENTITY_STANDARD:
		{
			CPed * pLeader = GetLeader();
			Assert(pLeader);	// This task only created if there is a leader ped available

			float fSeekDist = NeedsToLeaveInterior(pPed) ? Min(m_fDesiredTargetRadius, ms_fLeaveInteriorSeekEntityTargetRadiusXY) : m_fDesiredTargetRadius;

			TTaskMoveSeekEntityStandard * pSeekTask = rage_new TTaskMoveSeekEntityStandard(
				pLeader,
				TTaskMoveSeekEntityStandard::ms_iMaxSeekTime,
				TTaskMoveSeekEntityStandard::ms_iPeriod,
				fSeekDist,
				6.0f
				);

			pSeekTask->SetNeverEnterWater(!pPed->WillPedEnterWater());
			pSeekTask->SetMaxSlopeNavigable(m_fMaxSlopeNavigable);

			// Try to suppress runstops which will often cause following peds to barge into the player, if
			// the navmesh subtask is triggered close to the target.
			pSeekTask->SetSlowToWalkInsteadOfUsingRunStops(true);

			// Customize the behaviour based on the ped's navigation capabilities.
			const CPedNavCapabilityInfo& navInfo = pPed->GetPedIntelligence()->GetNavCapabilities();

			const float fWalkSpeedRange = navInfo.GetFollowLeaderMoveSeekEntityWalkSpeedRange();
			const float fRunSpeedRange = navInfo.GetFollowLeaderMoveSeekEntityRunSpeedRange();
			pSeekTask->SetUseSpeedRanges(true, fWalkSpeedRange, fRunSpeedRange);

			if (navInfo.CanSprintInFollowLeaderSeekEntity())
			{
				pSeekTask->SetMoveBlendRatio(MOVEBLENDRATIO_SPRINT);
			}

			// This handles the case where a ped is standing in shallow water.
			// Since he's already on a water poly, he doesn't mind continuing in water.
			if(!pPed->WillPedEnterWater() && !pPed->GetPedResetFlag( CPED_RESET_FLAG_IsDrowning ) && pLeader && pLeader->GetPedResetFlag( CPED_RESET_FLAG_IsDrowning ))
				pSeekTask->SetNeverStartInWater(true);

			return pSeekTask;
		}
		case CTaskTypes::TASK_MOVE_GO_TO_POINT:
		{
			float fMoveRatio = GetMoveBlendRatioToUse(pPed, vTargetPos, pPed->GetMotionData()->GetCurrentMoveBlendRatio().y, false);
			if(fMoveRatio > MOVEBLENDRATIO_STILL)
			{
				const CPedFormation * pFormation = m_pPedGroup ? m_pPedGroup->GetFormation() : NULL;
				float fTargetRadius = m_fDesiredTargetRadius;
				if( pFormation && pFormation->GetFormationType() == CPedFormationTypes::FORMATION_LOOSE )
				{
					fTargetRadius = 0.25f;
				}

				CPed* pLeader = GetLeader();
				Assert(pLeader);

				if( pLeader && pLeader->GetVelocity().Mag2() > 0.01f )
				{
					fTargetRadius = 0.1f;
				}
				CTaskMoveGoToPoint * pGotoTask = rage_new CTaskMoveGoToPoint(
					fMoveRatio,
					vTargetPos,
					fTargetRadius
					);
				return pGotoTask;
			}
			else
			{
				return rage_new CTaskMoveStandStill(2000);
			}
		}
		case CTaskTypes::TASK_MOVE_ACHIEVE_HEADING:
		{
			CTaskMoveAchieveHeading * pHeadingTask = rage_new CTaskMoveAchieveHeading(m_fDesiredHeading);
			return pHeadingTask;
		}
		case CTaskTypes::TASK_MOVE_STAND_STILL:
		{
			CTaskMoveStandStill * pStillTask = rage_new CTaskMoveStandStill(500);
			return pStillTask;
		}
		case CTaskTypes::TASK_MOVE_CROWD_AROUND_LOCATION:
		{
			CPed * pLeader = GetLeader();
			Assert(pLeader);

			const Vector3 vLeaderPosition = VEC3V_TO_VECTOR3(pLeader->GetTransform().GetPosition());
			//Vector3 vDiff = vLeaderPosition - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			float fCrowdDist = GetFormationSpacing();
			float fCrowdSpacing = MOVE_IN_FORMATION_RADIUS * fwRandom::GetRandomNumberInRange(3.0f, 6.0f);

			//m_fStartCrowdingDistFromPlayer = vDiff.Mag() + 0.1f;
			//m_fStartCrowdingDistFromPlayer = Min(m_fStartCrowdingDistFromPlayer, fCrowdDist);
			m_fStartCrowdingDistFromPlayer = fCrowdDist + 1.0f;
			m_vStartCrowdingLocation = vLeaderPosition;

			CTaskMoveCrowdAroundLocation * pCrowdTask = rage_new CTaskMoveCrowdAroundLocation(vLeaderPosition, fCrowdDist, fCrowdSpacing);
			pCrowdTask->SetRemainingWithinCrowdRadiusIsAcceptable(true);

			if(ms_bStopPlayerGroupMembersWalkingOverEdges && m_bFollowingGroup && pLeader->IsPlayer())
			{
				pCrowdTask->SetUseLineTestsToStopPedWalkingOverEdge(true);
			}
			return pCrowdTask;
		}
		case CTaskTypes::TASK_MOVE_TRACKING_ENTITY:
		{
			CPed * pLeader = GetLeader();
			Assert(pLeader);

			Vector3 vOffset(1.0f, 0.0f, 0.0f);
			Vector2 vCurrMbr;
			pLeader->GetMotionData()->GetCurrentMoveBlendRatio(vCurrMbr);

			CTaskMoveTrackingEntity * pTrackTask = rage_new CTaskMoveTrackingEntity(
				vCurrMbr.Mag(),
				pLeader,
				vOffset,
				true
				);
			return pTrackTask;
		}
		case CTaskTypes::TASK_FINISHED:
		{
			return NULL;
		}
	}
	Assert(0);
	return NULL;
}

void
CTaskMoveBeInFormation::GetTargetPosAdjustedForLeaderMovement(Vector3 & vPos)
{
	vPos = m_vDesiredPosition;

	// This might not be suitable for some formation types
	if(!GetAddLeadersVelocityOntoGotoTarget())
		return;

	CPed * pLeader = GetLeader();
	if(pLeader)
	{
		// A ped's movespeed will be correct(ish) regardless of whether it is local or a network-clone
		if(!pLeader->GetMotionData()->GetIsStill())
			vPos += pLeader->GetVelocity();
	}
}

float
CTaskMoveBeInFormation::GetMoveBlendRatioToUse(CPed * pPed, const Vector3 & vTargetPos, const float fCurrentMBR, const bool bTaskAlreadyMoving)
{
	const CPedNavCapabilityInfo& navInfo = pPed->GetPedIntelligence()->GetNavCapabilities();

	const float fRunDistance = navInfo.GetDefaultMoveFollowLeaderRunDistance();
	const float fSprintDistance = navInfo.GetDefaultMoveFollowLeaderSprintDistance();
	const float fRunDistanceSqr = fRunDistance*fRunDistance;
	const float fSprintDistanceSqr = fSprintDistance*fSprintDistance;

	CPed * pLeader = GetLeader();

	Vector3 vTargetToUse;
	if(GetAddLeadersVelocityOntoGotoTarget() && pLeader)
	{
		vTargetToUse = VEC3V_TO_VECTOR3(pLeader->GetTransform().GetPosition());
	}
	else
	{
		vTargetToUse = vTargetPos;
	}

	const Vector3 vDiff = vTargetToUse - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const float fDistanceToTarget = vDiff.Mag();

	//-------------------------------------------------------------------------------------
	// If we are not yet moving, then avoid triggering a runstart close to out target ped
	// as this can result in a long runstart/runstop sequence which causes the buddy ped
	// to run right into the leader (and looks pretty silly)

	if(!bTaskAlreadyMoving && fDistanceToTarget < fRunDistance)
	{
		//---------------------------------------------------
		// Test whether the leader is running away from us

		bool bLeaderIsMovingAwayFromUs = false;

		Vector3 vLeaderPos = VEC3V_TO_VECTOR3(pLeader->GetTransform().GetPosition());
		Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		Vector3 vToLeader = vLeaderPos - vPedPos;
		if(vToLeader.NormalizeSafeRet())
		{
			Vector3 vMoveDir;
			Assert(pLeader->GetCurrentMotionTask());
			pLeader->GetCurrentMotionTask()->GetMoveVector(vMoveDir);

			if(vMoveDir.Mag2() > 0.1f)
			{
				vMoveDir.Normalize();
				bLeaderIsMovingAwayFromUs = DotProduct(vToLeader, vMoveDir) > 0.707f;
			}
		}

		if(!bLeaderIsMovingAwayFromUs)
		{
			return MOVEBLENDRATIO_WALK;
		}
	}

	static const float fWalkAlongsideMinDistToAdjustSpeed = 1.25f;
	static const float fWalkAlongsideMaxDistToAdjustSpeed = 4.0f;

	float fMinDistToAdjustSpeed, fMaxDistToAdjustSpeed;
	if(m_bIsTryingToWalkAlongsideLeader)
	{
		fMinDistToAdjustSpeed = fWalkAlongsideMinDistToAdjustSpeed;
		fMaxDistToAdjustSpeed = fWalkAlongsideMaxDistToAdjustSpeed;
	}
	else
	{
		fMinDistToAdjustSpeed = GetMinDistToAdjustSpeed();
		fMaxDistToAdjustSpeed = GetMaxDistToAdjustSpeed();
	}

	if(pLeader)
	{
		// Attempt to match the leader's move speed
		const float fLeaderSpeedSqr = pLeader->GetVelocity().XYMag2();
		if(fLeaderSpeedSqr > 0.001f)
		{
			const float fLeaderSpeed = rage::Sqrtf(fLeaderSpeedSqr);

			// Calculate what MBR we need to match the leader's speed
			CMoveBlendRatioSpeeds moveSpeeds;
			Assert(pPed->GetCurrentMotionTask());
			pPed->GetCurrentMotionTask()->GetMoveSpeeds(moveSpeeds);
			const float * fMoveSpeeds = moveSpeeds.GetSpeedsArray();

			float fDesiredMBR = CTaskMotionBase::GetMoveBlendRatioForDesiredSpeed(fMoveSpeeds, fLeaderSpeed);

			float fMbrAccel = 1.0f;
			if(m_bFollowingGroup && m_pPedGroup && m_pPedGroup->GetFormation())
			{
				fMbrAccel = m_pPedGroup->GetFormation()->GetMBRAccelForDistance(fDistanceToTarget, pPed->GetPedIntelligence()->GetNavCapabilities().UseAdaptiveMbrAccelInFollowLeader());
			}
			else if (pPed->GetPedIntelligence()->GetNavCapabilities().UseAdaptiveMbrAccelInFollowLeader())
			{
				fMbrAccel = CPedFormation_Loose::GetAdaptiveMBRAccel(fDistanceToTarget, fMinDistToAdjustSpeed, fMaxDistToAdjustSpeed);
			}
			float fMinMbr = pPed->GetPedIntelligence()->GetNavCapabilities().GetDesiredMbrLowerBoundInMoveFollowLeader();

			static const float fMinRatio = 0.5f;

			fDesiredMBR = Clamp(fDesiredMBR, fMinRatio, MOVEBLENDRATIO_SPRINT);

			// If task is not yet moving, then return exactly the MBR we desire
			// to ensure that we will trigger the correct walk/run/sprint start anim
			if(!bTaskAlreadyMoving)
				return fDesiredMBR;

			// Otherwise we accel/decel MBR

			if(fDistanceToTarget < fMinDistToAdjustSpeed)
			{
				fDesiredMBR = fMinMbr;
			}
			else if(fDistanceToTarget > fMaxDistToAdjustSpeed)
			{
				fDesiredMBR = MOVEBLENDRATIO_SPRINT;
			}

			// Approach fRatio from current to desired
			const float fDeltaMBR = fDesiredMBR - fCurrentMBR;
			float fRatio = fCurrentMBR + (fDeltaMBR * fMbrAccel * GetTimeStep());

			// Prevent overshoot
			if(fCurrentMBR < fDesiredMBR)
				fRatio = Min(fRatio, fDesiredMBR);
			else if(fCurrentMBR > fDesiredMBR)
				fRatio = Max(fRatio, fDesiredMBR);

			fRatio = Clamp(fRatio, fMinRatio, MOVEBLENDRATIO_SPRINT);
			return fRatio;
		}
	}

	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
	{
		CTaskMoveFollowNavMesh * pRouteTask = (CTaskMoveFollowNavMesh*)GetSubTask();
		if(pRouteTask->IsFollowingNavMeshRoute() && pRouteTask->GetLastRoutePointIsTarget())
		{
			const float fRouteDistRemaining = pRouteTask->GetDistanceLeftOnCurrentRouteSection(pPed);

			if(fRouteDistRemaining*fRouteDistRemaining > fSprintDistanceSqr)
			{
				return MOVEBLENDRATIO_SPRINT;
			}
			else if(fRouteDistRemaining*fRouteDistRemaining < fRunDistanceSqr)
			{
				return MOVEBLENDRATIO_WALK;
			}
			else
			{
				const float fRatio = Range(fRouteDistRemaining, fRunDistance, fSprintDistance);
				return MOVEBLENDRATIO_WALK + ((MOVEBLENDRATIO_SPRINT-MOVEBLENDRATIO_RUN)*fRatio);
			}
		}
	}

	// If we're a significant distance away from our target, then sprint or run
	if(vDiff.Mag2() > fSprintDistanceSqr)
	{
		return MOVEBLENDRATIO_SPRINT;
	}
	else if(vDiff.Mag2() < fRunDistanceSqr)
	{
		return MOVEBLENDRATIO_WALK;
	}
	else
	{
		const float fRatio = Range(vDiff.Mag(), fRunDistance, fSprintDistance);
		return MOVEBLENDRATIO_WALK + ((MOVEBLENDRATIO_SPRINT-MOVEBLENDRATIO_RUN)*fRatio);
	}
}

void CTaskMoveBeInFormation::SetStrafingState(CPed * pPed, const Vector3 & vTargetPos)
{
	// We cannot strafe without a weapon anymore; for simplicity's sake we'll just disable strafing
	// in this task altogether until this is possible.
	if(!ms_bHasUnarmedStrafingAnims)
		return;

	// If the ped has been told to strafe by a main task, then don't mess with the strafing-state.
	// Same applies if this ped is walking alongside the leader ped.
	if(pPed->IsStrafing() || m_bIsTryingToWalkAlongsideLeader)
		return;

	// Don't update strafing while handcuffed	
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
		return;

	// Maybe switch to strafing movement
	bool bWasStrafing = m_bSetStrafing;
	bool bSetStrafing = false;

	// The section below handles switching between normal/strafing movement.  The ped will strafe
	// if their target is nearby and is behind them.  We prevent switching strafing on/off more than
	// once per second - as this causes nasty anim glitches.
	if(!m_SwitchStrafingTimer.IsSet() || m_SwitchStrafingTimer.IsOutOfTime())
	{
		switch(GetSubTask()->GetTaskType())
		{
			case CTaskTypes::TASK_MOVE_GO_TO_POINT:
			case CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH:
			{
				Vector3 vDiff = vTargetPos - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				float fDistSqrXY = vDiff.XYMag2();

				// If we are close to out target, and our target is behind us - then strafe
				if(fDistSqrXY <= 9.0f && Abs(vDiff.z) < 3.0f)
				{
					vDiff.z = 0.0f;
					vDiff.Normalize();
					if(DotProduct(vDiff, VEC3V_TO_VECTOR3(pPed->GetTransform().GetB())) < 0.0f)
						bSetStrafing = true;
				}
			}
		}

		if(bSetStrafing!=bWasStrafing)
		{
			m_SwitchStrafingTimer.Set(fwTimer::GetTimeInMilliseconds(), 1000);
		}

		m_bSetStrafing = bSetStrafing;
	}

	pPed->SetIsStrafing(m_bSetStrafing);

	if(m_bSetStrafing)
		pPed->SetDesiredHeading(m_fDesiredHeading);
}

void
CTaskMoveBeInFormation::IssueWalkAlongsideLeaderProbes(const Vector3 & vLeaderPos, const Vector3 & vTargetPos)
{
	CancelProbes();

	const u32 iTestTypes = ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE;
	WorldProbe::CShapeTestProbeDesc probeData;
	probeData.SetResultsStructure(&m_hLeaderToTargetProbe);
	probeData.SetStartAndEnd(vLeaderPos,vTargetPos);
	probeData.SetContext(WorldProbe::EMovementAI);
	probeData.SetIncludeFlags(iTestTypes);
	probeData.SetIsDirected(true);
	WorldProbe::GetShapeTestManager()->SubmitTest(probeData, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);

	if(m_hLeaderToTargetProbe.GetResultsStatus() != WorldProbe::SUBMISSION_FAILED)
	{
		Vector3 vTestAbove = vTargetPos;
		Vector3 vTestBelow = vTargetPos;
		vTestAbove.z += 2.0f;
		vTestBelow.z -= 2.0f;

		// The idea here is to hit the ground, hopefully at a similar height to the leader's Z position
		WorldProbe::CShapeTestProbeDesc probeData;
		probeData.SetResultsStructure(&m_hTargetToGroundProbe);
		probeData.SetStartAndEnd(vTestAbove,vTestBelow);
		probeData.SetContext(WorldProbe::EMovementAI);
		probeData.SetIncludeFlags(iTestTypes);
		probeData.SetIsDirected(true);
		WorldProbe::GetShapeTestManager()->SubmitTest(probeData, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);

		// If the 2nd test failed, then cancel the first as well - we will disable the walking-alongside behaviour
		if(m_hTargetToGroundProbe.GetResultsStatus() == WorldProbe::SUBMISSION_FAILED)
		{
			m_hLeaderToTargetProbe.Reset();
		}
	}

	m_vLastWalkAlongSideLeaderTarget = vTargetPos;
}

bool
CTaskMoveBeInFormation::RetrieveWalkAlongsideLeaderProbes(const Vector3 & UNUSED_PARAM(vLeaderPos), const Vector3 & vTargetPos)
{
	if(m_hLeaderToTargetProbe.GetResultsWaitingOrReady() && m_hTargetToGroundProbe.GetResultsWaitingOrReady())
	{
		m_bLosFromLeaderToTargetIsClear = m_hLeaderToTargetProbe.GetNumHits() == 0u;
		m_bLosToFindGroundIsClear = m_hTargetToGroundProbe.GetNumHits() == 0u;

		// We are hoping to have an intersection (ie. no LOS) close to the desired target pos.
		// If we don't have it, then set the m_bLosToFindGroundIsClear flag to invalidate this position.
		if(!m_bLosToFindGroundIsClear)
		{
			Vector3 const& vIntersection = m_hTargetToGroundProbe[0].GetHitPosition();
			if(!rage::IsNearZero(vIntersection.z - (vTargetPos.z-1.0f), 1.0f))
				m_bLosToFindGroundIsClear = true;
		}

		m_hLeaderToTargetProbe.Reset();
		m_hTargetToGroundProbe.Reset();

		return true;
	}

	return false;
}

void
CTaskMoveBeInFormation::CancelProbes()
{
	m_hLeaderToTargetProbe.Reset();
	m_hTargetToGroundProbe.Reset();
}

bool
CTaskMoveBeInFormation::IsPositionClearToWalkAlongsideLeader() const
{
	return (m_bLosFromLeaderToTargetIsClear && !m_bLosToFindGroundIsClear);
}

bool
CTaskMoveBeInFormation::UpdatePedPositioning(CPed * pPed)
{
	s32 iMemberIndex = 0;

	static dev_float fMaxDistApart = 5.0f;
	float fMaxWalkAlongsideDistApart = fMaxDistApart;

	if(m_bFollowingGroup)
	{
		CPedGroupMembership * pMembership = m_pPedGroup->GetGroupMembership();
		if(!pMembership)
			return false;

		iMemberIndex = pMembership->GetMemberId(pPed);
		if(iMemberIndex==-1)
			return false;

		if(!m_pPedGroup->GetGroupMembership()->GetMember(iMemberIndex))
			return false;

		static dev_float fExtra = 1.0f;
		fMaxWalkAlongsideDistApart = m_pPedGroup->GetFormation()->GetSpacing() + fExtra;
	}

	// We need to take note of this for elsewhere in the task
	m_bIsTryingToWalkAlongsideLeader = false;
	bool bWaitingForProbeResults = false;

	if( GetWalkAlongsideLeaderWhenClose(pPed) && iMemberIndex==0 && (!m_bFollowingGroup || !pPed->IsStrafing()))
	{
		const CPed * pLeader = GetLeader();
		Assert(pLeader);

		const Vector3 vLeaderPosition = VEC3V_TO_VECTOR3(pLeader->GetTransform().GetPosition());

		// bOnlyWalkAlongsidePlayer - toggles this behaviour for player-only or all group leaders.
		// Important since the walk-alongside has a certain overhead wrt async probes, etc.
		Vector2 vCurrMbr;
		pLeader->GetMotionData()->GetCurrentMoveBlendRatio(vCurrMbr);

		static const bool bOnlyWalkAlongsidePlayer = false;
		const float fMoveBlendRatio = pLeader->IsStrafing() ? 0.0f : vCurrMbr.Mag();

		if((!bOnlyWalkAlongsidePlayer || pLeader->IsPlayer()) && fMoveBlendRatio > 0.0f && fMoveBlendRatio < 1.5f)
		{
			Vector3 vToLeader = vLeaderPosition - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			const float fDist = NormalizeAndMag(vToLeader);
			const float fHeadingDelta = SubtractAngleShorter(pLeader->GetCurrentHeading(), pPed->GetCurrentHeading());
			
			static dev_float fMaxDelta = ( DtoR * 60.0f);

			if(fDist < fMaxDistApart && Abs(fHeadingDelta) < fMaxDelta)
			{
				static dev_float fDistanceBehindLeader = 1.0f;
				Vector3 vWalkAlongsidePos = vLeaderPosition - (vToLeader * fDistanceBehindLeader);

				// How far to the left/right of the leader that this ped will try to walk
				static const float fDistanceToSide = 1.3f;

				const Vector3 vRight = VEC3V_TO_VECTOR3(pLeader->GetTransform().GetA());
				if(DotProduct(vToLeader, vRight) < 0.0f)
				{
					vWalkAlongsidePos += vRight*fDistanceToSide;
				}
				else
				{
					vWalkAlongsidePos -= vRight*fDistanceToSide;
				}

				// See whether this position is known to be clear
				const float fDistSqr = (vWalkAlongsidePos - m_vLastWalkAlongSideLeaderTarget).Mag2();

				// Position delta exceeded, so initiate some linetests.
				// We will continue using the previous results until the new ones arrive.
				if(fDistSqr > 2.0f*2.0f)
				{
					IssueWalkAlongsideLeaderProbes(vLeaderPosition, vWalkAlongsidePos);
					bWaitingForProbeResults = true;
				}
				else
				{
					bWaitingForProbeResults = !RetrieveWalkAlongsideLeaderProbes(vLeaderPosition, vWalkAlongsidePos);
				}

				if(IsPositionClearToWalkAlongsideLeader())
				{
					m_bIsTryingToWalkAlongsideLeader = true;

					m_vDesiredPosition = vWalkAlongsidePos;
					m_fDesiredHeading = pLeader->GetDesiredHeading();
					m_fDesiredTargetRadius = 1.0f;

					// The target radius isn't always set.  So look for a simple move task
					// ourself & set the target radius explicitly.
					CTask * pTask = pPed->GetPedIntelligence()->GetActiveSimplestMovementTask();
					if(pTask && pTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT)
					{
						((CTaskMoveGoToPoint*)pTask)->SetTargetRadius(m_fDesiredTargetRadius);
					}

					return true;
				}
			}
		}
	}

	if(!bWaitingForProbeResults)
	{
		CancelProbes();
	}

	if(m_bFollowingGroup)
	{
		const CPedFormation * pFormation = m_pPedGroup->GetFormation();
		Assert(pFormation);

		const CPedPositioning & pedPositioning = pFormation->GetPedPositioning(iMemberIndex);
		m_vDesiredPosition = pedPositioning.GetPosition();
		m_fDesiredHeading = pedPositioning.GetHeading();

		// For FORMATION_LOOSE we keep a distance from each member of the formation 
		// For all others we will use the per-member target radius value
		if( pFormation->GetFormationType() == CPedFormationTypes::FORMATION_LOOSE )
		{
			m_fDesiredTargetRadius = static_cast<const CPedFormation_Loose*>(pFormation)->ms_fPerMemberSpacing; 
		}
		else
		{
			m_fDesiredTargetRadius = pedPositioning.GetTargetRadius();
		}
	}
	else
	{
		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		const CPed * pLeader = GetLeader();

		// Bail out if no leader as this will crash below.
		if (!pLeader)
		{
			return false;
		}

		const Vector3 vLeaderPos = VEC3V_TO_VECTOR3(pLeader->GetTransform().GetPosition());
		m_vDesiredPosition = vLeaderPos;
		m_fDesiredHeading = fwAngle::GetRadianAngleBetweenPoints(
			vLeaderPos.x, vLeaderPos.y,
			vPedPosition.x, vPedPosition.y
		);
		m_fDesiredTargetRadius = GetFormationSpacing();
	}

	// If this ped is in water & will take damage, and the leader is not, then reduce target range to help ensure they get out
	
	const CPed * pLeader = GetLeader();

	if(pPed->GetPedResetFlag( CPED_RESET_FLAG_IsDrowning ) && !pPed->WillPedEnterWater() &&
		pLeader && !pLeader->GetPedResetFlag( CPED_RESET_FLAG_IsDrowning ) )
	{
		m_fDesiredTargetRadius = 1.0f;
	}

	return true;
}

float CTaskMoveBeInFormation::GetFormationTargetRadius(CPed * pPed)
{
	if(m_bFollowingGroup)
	{
		const CPedFormation * pFormation = m_pPedGroup->GetFormation();
		Assert(pFormation);
		if(pFormation)
		{
			CPedGroupMembership * pMembership = m_pPedGroup->GetGroupMembership();
			if(pMembership)
			{
				const int iMember = pMembership->GetMemberId(pPed);
				if(iMember != -1)
				{
					const CPedPositioning & pedPositioning = pFormation->GetPedPositioning(iMember);
					return pedPositioning.GetTargetRadius();
				}
			}
		}
	}

	return GetFormationSpacing();
}

bool CTaskMoveBeInFormation::ArePedsMovingTowardsEachOther(const CPed * pPed, const CPed * pLeader, const float fTestDistance) const
{
	if(!pLeader)
		return false;

	if (pPed->GetPedIntelligence()->GetNavCapabilities().ShouldIgnoreMovingTowardsLeaderTargetRadiusExpansion())
	{
		return false;
	}

	static dev_float fDotVelThreshold = 0.707f;	// 0.0f
	static dev_float fDotVelToLeaderThreshold = 0.707f;
	static dev_float fMinVelSqr = 0.1f;

	Vector3 vPedVel = pPed->GetVelocity();
	Vector3 vLeaderVel = pLeader->GetVelocity();
	const bool bPedMoving = (vPedVel.Mag2() > fMinVelSqr);
	const bool bLeaderMoving = (vLeaderVel.Mag2() > fMinVelSqr);

	if( bPedMoving || bLeaderMoving )
	{
		Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		Vector3 vLeaderPos = VEC3V_TO_VECTOR3(pLeader->GetTransform().GetPosition());
		if(IsClose(vPedPos.z, vLeaderPos.z, 4.0f))
		{
			Vector3 vPedToLeader = vLeaderPos - vPedPos;
			float fDistFromLeader = vPedToLeader.Mag();
			if(fDistFromLeader < fTestDistance)
			{
				vPedToLeader.Normalize();

				Vector3 vRelVel = vLeaderVel - vPedVel;
				vPedVel.Normalize();
				if(vRelVel.Mag2() > SMALL_FLOAT)
					vRelVel.Normalize();

				if( (bPedMoving && DotProduct(vRelVel, vPedVel) < -fDotVelThreshold) ||
					(bLeaderMoving && DotProduct(vRelVel, vLeaderVel) > fDotVelThreshold) )
				{
					if(DotProduct(vRelVel, vPedToLeader) < -fDotVelToLeaderThreshold)
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

CTask *
CTaskMoveBeInFormation::MaybeUpdateMovementSubTask(CPed * pPed, const Vector3 & vTargetPos)
{
	CTask * pSubTask = GetSubTask();
	Assert(pSubTask && pSubTask->IsMoveTask());

	if(!pSubTask || !pSubTask->GetIsFlagSet(aiTaskFlags::HasBegun) || pSubTask->GetIsFlagSet(aiTaskFlags::HasFinished) || pSubTask->GetIsFlagSet(aiTaskFlags::IsAborted))
	{
		return pSubTask;
	}
	
	const CPed * pLeader = GetLeader();

	// For each possible subtask, check their breakout conditions against the peds' positions
	// and maybe replace them with an alternative subtask.
	switch(pSubTask->GetTaskType())
	{
		// Update the navmesh route's target
		case CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH:
		{
			CTaskMoveFollowNavMesh * pNavTask = (CTaskMoveFollowNavMesh*)pSubTask;

			// Firstly, check to see if the navmesh task was unable to find a route.
			// In this case we should switch to a seek-entity task to seek the leader.
			// Once that finishes, we will do a navmesh task again.
			eNavMeshRouteResult ret = pNavTask->GetNavMeshRouteResult();
			if(ret == NAVMESHROUTE_ROUTE_NOT_FOUND && pNavTask->GetNavMeshRouteMethod() >= CTaskMoveFollowNavMesh::eRouteMethod_LastMethodWhichAvoidsObjects)
			{
				if(pLeader && pNavTask->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
				{
					return CreateSubTask(CTaskTypes::TASK_MOVE_SEEK_ENTITY_STANDARD, pPed);
				}
			}

			Assert(pNavTask->HasTarget());
			Vector3 vDiff = pNavTask->GetTarget() - m_vDesiredPosition;
			float fDistSqrXY = vDiff.XYMag2();
			float fXYDistToSetNewTarget = m_bIsTryingToWalkAlongsideLeader ? ms_fXYDiffSqrToSetNewTargetWhenWalkingAlongsideLeader : ms_fXYDiffSqrToSetNewTarget;
			
			float fSetTargetDistOverride = pPed->GetPedIntelligence()->GetNavCapabilities().GetFollowLeaderResetTargetDistanceSquaredOverride();

			if (fSetTargetDistOverride > 0.0f)
			{
				fXYDistToSetNewTarget = fSetTargetDistOverride;
			}
			
			const bool bTaskIsMoving = (pNavTask->GetState()==CTaskNavBase::NavBaseState_FollowingPath || pNavTask->GetState()==CTaskNavBase::NavBaseState_MovingWhilstWaitingForPath);


			// Set new navmesh target if leader has moved significantly OR if they have just
			// stopped moving, which should help prevent peds running into their leader.
			if(m_bLeaderHasJustStoppedMoving || fDistSqrXY >= fXYDistToSetNewTarget || Abs(vDiff.z) > ms_fZDiffToSetNewTarget)
			{
				pNavTask->SetTarget(pPed, VECTOR3_TO_VEC3V(m_vDesiredPosition));
			}

			// Set moveblend ratio based up the distance left along the nav path
			const float fMBR = GetMoveBlendRatioToUse( pPed, m_vDesiredPosition, pNavTask->GetMoveBlendRatio(), bTaskIsMoving );

			pNavTask->SetMoveBlendRatio(fMBR);

			// Ensure that the lowest level locomotion task also gets updated
			// TODO JB: This is supposed to automatically propogate, why has it broken?
			CTask * pTaskSimpleMove = pPed->GetPedIntelligence()->GetActiveSimplestMovementTask();
			if(pTaskSimpleMove && pTaskSimpleMove->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT)
			{
				((CTaskMoveGoToPoint*)pTaskSimpleMove)->SetMoveBlendRatio(fMBR);
			}

			if (!pPed->GetPedIntelligence()->GetNavCapabilities().AlwaysKeepMovingWhilstFollowingLeaderPath())
			{
				if(bTaskIsMoving)
					pNavTask->SetKeepMovingWhilstWaitingForPath(true);
				else
					pNavTask->SetKeepMovingWhilstWaitingForPath(false);
			}

			float fRadius = GetTargetRadiusForNavMeshTask(pPed);

			const bool bIsLeaderStill = pLeader ? pLeader->GetMotionData()->GetIsStill() : true;
			if(bIsLeaderStill)
			{
				// Stop early if not following a group, OR following in the loose formation.
				// TODO JB: Move that decision in a flag in the CFormation class
				bool bMayStopEarly = !m_bFollowingGroup;
				const CPedFormation * pFormation = m_pPedGroup ? m_pPedGroup->GetFormation() : NULL;
				if(m_bFollowingGroup)
				{
					bMayStopEarly = (pFormation && pFormation->GetFormationType() == CPedFormationTypes::FORMATION_LOOSE);
				}

				if (pPed->GetPedIntelligence()->GetNavCapabilities().IsFlagSet(CPedNavCapabilityInfo::FLAG_NEVER_STOP_NAVMESH_TASK_EARLY_IN_FOLLOW_LEADER))
				{
					bMayStopEarly = false;
				}
				if(bMayStopEarly && pNavTask->GetLastRoutePointIsTarget() && pNavTask->GetDistanceLeftOnCurrentRouteSection(pPed) < m_fDesiredTargetRadius)
				{
					if(pNavTask->MakeAbortable(ABORT_PRIORITY_URGENT, NULL))
					{
						if (pFormation && pFormation->GetFormationType() == CPedFormationTypes::FORMATION_LOOSE)
						{
							// GoToPoint will not consider geometry or anything
							return CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed);
						}
						else
						{
							return CreateSubTask(CTaskTypes::TASK_MOVE_SEEK_ENTITY_STANDARD, pPed);
						}
					}
				}
			}
			else
			{
				static dev_float fTestDistance = 10.0f;

				if(ArePedsMovingTowardsEachOther(pPed, pLeader, fTestDistance))
				{
					fRadius = Max(fRadius, fTestDistance);
				}
			}

			pNavTask->SetTargetRadius(fRadius);

			break;
		}
		// Update the goto point's target, or create a navmesh route if we are now too far away
		case CTaskTypes::TASK_MOVE_GO_TO_POINT:
		{
			CTaskMoveGoToPoint * pGotoTask = (CTaskMoveGoToPoint*)GetSubTask();

			// If our leader is still..
			if(pLeader && CPedMotionData::GetIsStill(m_fLeadersLastMoveBlendRatio))
			{
				// If our goto target is very close to the leader's position (probably identical)
				const Vector3 vTargetToLeader = VEC3V_TO_VECTOR3(pLeader->GetTransform().GetPosition()) - pGotoTask->GetTarget();
				const float fDistXY2 = vTargetToLeader.Mag2();
				if(fDistXY2 < 0.2f && pGotoTask->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
				{
					if(NeedsToLeaveInterior(pPed))
					{
						return CreateSubTask(CTaskTypes::TASK_MOVE_SEEK_ENTITY_STANDARD, pPed);
					}
					else
					{
						CTaskTypes::eTaskType nextTaskType = pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING) ? 
							CTaskTypes::TASK_MOVE_STAND_STILL : CTaskTypes::TASK_MOVE_ACHIEVE_HEADING;
						return CreateSubTask(nextTaskType, pPed);
					}
				}
			}

			Vector3 vDiff = vTargetPos - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			float fDistSqrXY = vDiff.XYMag2();
			if(fDistSqrXY >= ms_fXYDiffSqrToSwitchToNavMeshRoute || Abs(vDiff.z) > ms_fZDiffToSetNewTarget)
			{
				if(pGotoTask->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
				{
					return CreateSubTask(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, pPed);
				}
			}

			pGotoTask->SetTarget(pPed, VECTOR3_TO_VEC3V(vTargetPos));
			pGotoTask->SetMoveBlendRatio(GetMoveBlendRatioToUse(pPed, vTargetPos, MOVEBLENDRATIO_STILL, true));

			break;
		}
		case CTaskTypes::TASK_MOVE_ACHIEVE_HEADING:
		{
			// If leader is crowding us, move to our desired location
			if (!IsLooseFormation() && !IsInPosition() && IsTooCloseToLeader())
			{
				if (GetSubTask()->MakeAbortable(ABORT_PRIORITY_URGENT, NULL))
				{
					return CreateSubTask(CTaskTypes::TASK_MOVE_GO_TO_POINT, pPed);
				}
			}
			break;
		}
		default:
		{
			break;
		}
	}

	return pSubTask;
}


CTask *
CTaskMoveBeInFormation::MaybeInterruptIdleTaskToStartMoving(CPed * pPed, const Vector3 & vTargetPos)
{
	// We'll only interrupt the achieve-heading or stand-still subtasks
	if(GetSubTask()->GetTaskType() != CTaskTypes::TASK_MOVE_ACHIEVE_HEADING &&
		GetSubTask()->GetTaskType() != CTaskTypes::TASK_MOVE_STAND_STILL &&
		GetSubTask()->GetTaskType() != CTaskTypes::TASK_MOVE_CROWD_AROUND_LOCATION)
	{
		return NULL;
	}

	CPed * pLeader = GetLeader();

	if(GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOVE_CROWD_AROUND_LOCATION &&
		((CTaskMoveCrowdAroundLocation*)GetSubTask())->GetRemainingWithinCrowdRadiusIsAcceptable() &&
		((!GetSubTask()->GetSubTask()) || GetSubTask()->GetSubTask()->GetTaskType()!=CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH))
	{
		if(pLeader)
		{
			if(NeedsToLeaveInterior(pPed))
			{
				if(GetSubTask()->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
					return CreateSubTask(CTaskTypes::TASK_MOVE_SEEK_ENTITY_STANDARD, pPed);
			}
			else
			{
				// We will break out of CTaskMoveCrownRoundLocation if
				//	i) we are outside the m_fStartCrowdingDistFromPlayer distance
				//	ii) we are outside the m_fDesiredTargetRadius and the leader is moving away from us

				bool bBreakOutDueToLeaderMovement = false;

				Vector3 vLeaderPos = VEC3V_TO_VECTOR3(pLeader->GetTransform().GetPosition());
				Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				Vector3 vToLeader = vLeaderPos - vPedPos;
				if(vToLeader.XYMag2() > m_fDesiredTargetRadius*m_fDesiredTargetRadius)
				{
					if(vToLeader.NormalizeSafeRet())
					{
						Vector3 vMoveDir;
						pLeader->GetCurrentMotionTask()->GetMoveVector(vMoveDir);

						if(vMoveDir.Mag2() > 0.1f)
						{
							vMoveDir.Normalize();
							bBreakOutDueToLeaderMovement = DotProduct(vToLeader, vMoveDir) > 0.707f;
						}
					}
				}

				if(bBreakOutDueToLeaderMovement)
				{
					if(GetSubTask()->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
						return CreateSubTask(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, pPed);
				}

				const Vector3 vCrowdPos = m_vStartCrowdingLocation;
				const Vector3 vDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - vCrowdPos;
				const float fDistSqrXY = vDiff.Mag2();

				if(fDistSqrXY > m_fStartCrowdingDistFromPlayer*m_fStartCrowdingDistFromPlayer)
				{
					if(GetSubTask()->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
						return CreateSubTask(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, pPed);
				}
			}
		}
		return NULL;
	}

	static const float fSmallDistanceChangeSqrForGoToPoint = 0.25f*0.25f;

	Vector3 vDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - vTargetPos;
	float fDistSqrXY = vDiff.XYMag2();

	Vector3 vDiffFromLastTargetPos = vTargetPos - m_vLastTargetPos;
	float fDiffFromLastSqrXY = vDiffFromLastTargetPos.XYMag2();

	static const float fSmallChange = 0.03125f*0.03125f;
	bool bTargetHasChanged = fDiffFromLastSqrXY > fSmallChange;

	if(!bTargetHasChanged)
		return NULL;

	m_vLastTargetPos = vTargetPos;

	// If we're outside a certain distance away, start up a navmesh task
	float fMinDistToMove = m_fDesiredTargetRadius;
	if ( m_pPedGroup && m_pPedGroup->GetFormation() && m_pPedGroup->GetFormation()->GetFormationType() == CPedFormationTypes::FORMATION_LOOSE )
	{
		fMinDistToMove = m_pPedGroup->GetFormation()->GetSpacing();
	}

	if(fDistSqrXY >= ms_fXYDiffSqrToSetNewTarget &&
		(fDistSqrXY > (rage::square(fMinDistToMove)) || Abs(vDiff.z) > ms_fZDiffToSetNewTarget))
	{
		static dev_float fTestDistance = 10.0f;

		if( !ArePedsMovingTowardsEachOther(pPed, pLeader, fTestDistance) )
		{
			if(GetSubTask()->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
			{
				return CreateSubTask(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, pPed);
			}
		}
	}
	// Else if we're outside of the target radius & outside of a small distance, then just to a goto point
	else if(fDistSqrXY > fSmallDistanceChangeSqrForGoToPoint &&
		fDistSqrXY > (rage::square(fMinDistToMove)) &&
		Abs(vDiff.z) < ms_fZDiffToSetNewTarget)
	{
		if(GetSubTask()->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
			return CreateSubTask(CTaskTypes::TASK_MOVE_GO_TO_POINT, pPed);
	}

	return NULL;
}

// PURPOSE: Use most of the same logic for determining if we should be moving (but reversed)
//			to determine if we are in position
bool CTaskMoveBeInFormation::IsInPosition()
{
	CPed* pPed = GetPed();
	if(!pPed)
	{
		return false;
	}

	// we can't be in position unless we are standing still or trying to achieve heading
	if(GetSubTask()->GetTaskType() != CTaskTypes::TASK_MOVE_ACHIEVE_HEADING &&
	   GetSubTask()->GetTaskType() != CTaskTypes::TASK_MOVE_STAND_STILL)
	{
		return false;
	}

	Vector3 vTargetPos;
	GetTargetPosAdjustedForLeaderMovement(vTargetPos);

	static const float fSmallDistanceChangeSqrForGoToPoint = 0.25f * 0.25f;

	Vector3 vDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - vTargetPos;
	float fDistSqrXY = vDiff.XYMag2();

	float fMinDistToMove = m_fDesiredTargetRadius;
	if ( m_pPedGroup && m_pPedGroup->GetFormation() && m_pPedGroup->GetFormation()->GetFormationType() == CPedFormationTypes::FORMATION_LOOSE )
	{
		fMinDistToMove = CPedFormation::ms_fMinSpacing;
	}

	// If we're outside a certain distance away then we aren't in position
	if(fDistSqrXY >= ms_fXYDiffSqrToSetNewTarget &&
		(fDistSqrXY > (rage::square(fMinDistToMove)) || Abs(vDiff.z) > ms_fZDiffToSetNewTarget))
	{
		return false;
	}
	// Else if we're outside of the target radius & outside of a small distance, then again we're not in position
	else if(fDistSqrXY > fSmallDistanceChangeSqrForGoToPoint &&
		fDistSqrXY > (rage::square(fMinDistToMove)) &&
		Abs(vDiff.z) < ms_fZDiffToSetNewTarget)
	{
		return false;
	}

	return true;
}

bool CTaskMoveBeInFormation::IsTooCloseToLeader()
{
	CPed* pPed = GetPed();
	if(!pPed)
	{
		return false;
	}

	dev_float fTooCloseDist = CPedFormation::ms_fMinSpacing;
	CPed * pLeader = GetLeader();
	ScalarV vDist2ToLeader = DistSquared(pLeader->GetTransform().GetPosition(), pPed->GetTransform().GetPosition());
	ScalarV vTooCloseDist2 = ScalarVFromF32(rage::square(fTooCloseDist));

	return IsLessThanAll(vDist2ToLeader, vTooCloseDist2) != 0;
}

bool CTaskMoveBeInFormation::IsLooseFormation()
{
	if(!m_bFollowingGroup)
		return true;

	Assert(m_pPedGroup);
	const CPedFormation * pFormation = m_pPedGroup->GetFormation();
	return (pFormation && pFormation->GetFormationType() == CPedFormationTypes::FORMATION_LOOSE);
}









//**************************************************************
// CTaskMoveTrackingEntity
//

CTaskMoveTrackingEntity::CTaskMoveTrackingEntity(const float fMoveBlend, CEntity * pEntity, const Vector3 & vOffset, bool bOffsetRotatesWithEntity)
	: CTaskMoveBase(fMoveBlend),
	m_pEntity(pEntity),
	m_vOffset(vOffset),
	m_fBlend(0.0f),
	m_fWalkDelay(0.0f),
	m_bOffsetRotatesWithEntity(bOffsetRotatesWithEntity),
	m_bAborting(false)
{
	Assert(m_pEntity);
	Assert(m_pEntity->GetType() == ENTITY_TYPE_PED || m_pEntity->GetType() == ENTITY_TYPE_VEHICLE || m_pEntity->GetType() == ENTITY_TYPE_OBJECT);

	CalcTarget();

	SetInternalTaskType(CTaskTypes::TASK_MOVE_TRACKING_ENTITY);
}


CTaskMoveTrackingEntity::~CTaskMoveTrackingEntity()
{
}

#if !__FINAL
void CTaskMoveTrackingEntity::Debug() const
{
#if DEBUG_DRAW
    const Vector3 v1 = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
    const Vector3& v2 = m_vCalculatedTarget;
    grcDebugDraw::Line(v1,v2,Color32(0x00,0xff,0x00,0xff));
#endif
}
#endif

// This function must generate an appropriate target position
Vector3
CTaskMoveTrackingEntity::GetTarget() const
{
	if(!m_pEntity)
	{
		return m_vCalculatedTarget;
	}

	return m_vCalculatedTarget;
}

void
CTaskMoveTrackingEntity::CalcTarget(void)
{
	if(m_bOffsetRotatesWithEntity)
	{
		m_vOffset = m_vCalculatedTarget = m_pEntity->TransformIntoWorldSpace(m_vOffset);
	}
	else
	{
		m_vCalculatedTarget = VEC3V_TO_VECTOR3(m_pEntity->GetTransform().GetPosition()) + m_vOffset;
	}
}

bool
CTaskMoveTrackingEntity::ProcessMove(CPed* pPed)
{
	if(!m_pEntity)
	{
		return true;
	}

	if(m_bAborting)
	{
	    return true;
    }

	// Calculate the target we are heading for - an offset from the tracked entity
	CalcTarget();

	if(pPed->GetCurrentMotionTask()->IsInMotionState(CPedMotionStates::MotionState_Idle) && m_fWalkDelay == 0.0f)
	{
		m_fWalkDelay = 1.0f;
	}

	Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 vPedToTarget = m_vCalculatedTarget - vPedPosition;
	vPedToTarget.z = 0;
	float fDistToTarget = vPedToTarget.Mag();
	vPedToTarget.Normalize();

	// Work out how far the entity has moved since the last time
	// From this, calculate the entity speed in metres-per-second
	// For entities derived from CPhysical this is easy, we can
	// just get the m_moveSpeed and multiply it by FRAMES_PER_SECOND
	Vector3 vTargetVel(0.0f,0.0f,0.0f);

	// Check the target type is supported
	Assert( m_pEntity->GetType() == ENTITY_TYPE_PED || m_pEntity->GetType() == ENTITY_TYPE_VEHICLE || m_pEntity->GetType() == ENTITY_TYPE_OBJECT );

	float fEntitySpeed = ((CPhysical*)m_pEntity.Get())->GetVelocity().Mag();

	/*
	if(m_pEntity->GetIsDynamic())
	{
		CDynamicEntity * pDynEnt = (CDynamicEntity*)m_pEntity;
		phInst * pPhys = pDynEnt->GetPhysInst();
		if(pPhys)
			pPhys->GetExternallyControlledVelocity(vTargetVel);
		fEntitySpeed = vTargetVel.Mag();
	}
	*/



	// These constants are used for speeding-up & slowing-down the ped to get adjacent to the entity
	const float fDistToSlowDownFast		= -0.5f;
	const float fDistToSlowDown			= -0.25f;
	const float fDistToSpeedUp			= 1.0f;
	const float fDistToSpeedUpFast		= 2.0f;

	const float fSpeedInc				= 0.02f;//0.002f;
	const float fSpeedIncFast			= 0.05f;//0.005f;
	const float fSpeedDec				= fSpeedInc;
	const float fSpeedDecFast			= fSpeedIncFast;

	const float fMinMoveBlend = 0.3f;
	const float fMaxMoveBlend = 3.0f;

	Vector3 vecForward(VEC3V_TO_VECTOR3(m_pEntity->GetTransform().GetB()));
	float plane_d = -DotProduct(m_vCalculatedTarget, vecForward);
	float fDistInFront = DotProduct(vPedPosition, vecForward) + plane_d;

	float fDist = -fDistInFront;

	if(fDist < fDistToSlowDownFast)
	{
		// slow down walk-speed a lot to prevent ped from getting ahead of target
		m_fBlend = rage::Max(fMinMoveBlend, m_fMoveBlendRatio - fSpeedDecFast);
	}
	else if(fDist < fDistToSlowDown)
	{
		// slow down walk-speed a little to prevent ped from getting ahead of target
		m_fBlend = rage::Max(fMinMoveBlend, m_fMoveBlendRatio - fSpeedDec);
	}
	else if(fDist > fDistToSpeedUpFast)
	{
		// speed up walk-speed a lot so ped can keep up with target
		m_fBlend = rage::Min(fMaxMoveBlend, m_fMoveBlendRatio + fSpeedIncFast);
	}
	else if(fDist > fDistToSpeedUp)
	{
		// speed up walk-speed a little so ped can keep up with target
		m_fBlend = rage::Min(fMaxMoveBlend, m_fMoveBlendRatio + fSpeedInc);
	}
	else
	{
		// we are alongside the target entity, so try to match speed precisely
		float fTargetBlend = CTaskMoveBase::ComputeMoveBlend(*pPed, fEntitySpeed);
		// not too slow
		fTargetBlend = rage::Max(fMinMoveBlend, fTargetBlend);

		if(fTargetBlend > m_fMoveBlendRatio)
		{
			m_fBlend += fSpeedIncFast * 4.0f;
			if(m_fBlend > fTargetBlend) m_fBlend = fTargetBlend;
		}
		else if(fTargetBlend < m_fMoveBlendRatio)
		{
			m_fBlend -= fSpeedDecFast * 4.0f;
			if(m_fBlend < fTargetBlend) m_fBlend = fTargetBlend;
		}
	}

	// If we're not in-front of the leader OR are over 2m away from where we're supposed to be - then
	// orientate towards the target.  This logic is to allow gang guys to overshoot their target without
	// immediately spinning around to point back at it (looks awful).
	if(fDistInFront < -1.0f || fDistToTarget > 2.0f)
	{
		// Calculate the desired heading to get to the target
		Vector3 vHeadTo = m_vCalculatedTarget;
		m_vCalculatedTarget += vTargetVel * 2.0f;	// 2mps ahead
		vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());	//	Just in case the ped's position has changed since the last time vPedPosition was set
		m_fTheta = fwAngle::GetRadianAngleBetweenPoints(vHeadTo.x, vHeadTo.y, vPedPosition.x, vPedPosition.y);
		m_fTheta = fwAngle::LimitRadianAngle(m_fTheta);

		//Calculate the desired pitch angle for the ped.
		Vector3 vDiff = vHeadTo - vPedPosition;
		m_fPitch = Sign(vDiff.z) * acosf(fDistToTarget / vDiff.Mag());
		m_fPitch = fwAngle::LimitRadianAngle(m_fPitch);
	}
	// Otherwise, if we're real close to the target & keeping up well with the tracked entity,
	// set our desired heading to be the same as the entity's heading (if a ped!)
	else
	{

	}

	// If walk delay is non-zero, then it means that we have just started moving from a standstill -
	// and the moveBlend must remain at 1.0f until the delay expires to enable the ped to blend out
	// of the Idle anim and into the Walk anim.
	if(m_fWalkDelay > 0.0f)
	{
		m_fWalkDelay -= fwTimer::GetTimeStep();
		m_fBlend = 1.0f;
	}

	m_fMoveBlendRatio = m_fBlend;

	return false;
}

void CTaskMoveTrackingEntity::DoAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(CTask::ABORT_PRIORITY_IMMEDIATE==iPriority)
	{
		//If aborting immediately then get rid of any walking anims.
		pPed->GetMotionData()->SetDesiredMoveBlendRatio(MOVEBLENDRATIO_STILL);
	}

	m_bAborting = true;

	// Base class
	CTaskMove::DoAbort(iPriority, pEvent);
}


