// File header
#include "Camera/CamInterface.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedMoveBlend/PedMoveBlendBase.h"
#include "Math/angmath.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/System/TaskMove.h"
#include "Task/Vehicle/TaskRideHorse.h"
#include "System/Control.h"

AI_OPTIMISATIONS()

//-------------------------------------------------------------------------
// CTaskMoveInterface
//-------------------------------------------------------------------------

void CTaskMoveInterface::SetMoveBlendRatio(const float fMoveBlendRatio, const bool bFlagAsChanged) 
{ 
	Assertf(fMoveBlendRatio >= MOVEBLENDRATIO_STILL && fMoveBlendRatio <= MOVEBLENDRATIO_SPRINT, "fMoveBlend of %.2f requested from movement task (should be in %.1f to %.1f range inclusive)", fMoveBlendRatio, MOVEBLENDRATIO_STILL, MOVEBLENDRATIO_SPRINT);
	m_fMoveBlendRatio = Clamp(fMoveBlendRatio, MOVEBLENDRATIO_STILL, MOVEBLENDRATIO_SPRINT);
	m_bNewMoveSpeed = bFlagAsChanged;
}

//-------------------------------------------------------------------------
// CTaskMove
//-------------------------------------------------------------------------

CTaskMove::CTaskMove(const float fMoveBlendRatio)
: CTaskMoveInterface(fMoveBlendRatio)
{
}

CTaskMove::~CTaskMove()
{
}

//-------------------------------------------------------------------------
// Returns true if this task is trying to move the ped
//-------------------------------------------------------------------------
bool CTaskMove::IsTaskMoving() const
{
	return (m_fMoveBlendRatio > 0.0f);
}

//-------------------------------------------------------------------------
// Based on a designated stopping distance, evaluate the current input and return the ideal move ratio stick vector
//-------------------------------------------------------------------------
Vector2 CTaskMove::ProcessStrafeInputWithRestrictions( CPed* pPed, const CEntity* pTargetEntity, float fDistanceRestriction, float& rfModifiedStickAngle, bool& rbStickAngleTweaked )
{
	Vector2 vecStickInput( 0.0f, 0.0f );
	if( pPed )
	{
		const CControl *pControl = pPed->GetControlFromPlayer();
		vecStickInput.x = pControl->GetPedWalkLeftRight().GetNorm();
		vecStickInput.y = -pControl->GetPedWalkUpDown().GetNorm();
		
		// make sure input vector doesn't exceed limits
		float fMoveRatio = vecStickInput.Mag();
		if( fMoveRatio > 1.0f )
		{
			vecStickInput.Normalize();
			fMoveRatio = 1.0f;
		}
		
		if( fMoveRatio > 0.0f )
		{
			// get the orientation of the FIRST follow ped camera, for player controls.
			float fDesiredStickAngle = rage::Atan2f( -vecStickInput.x, vecStickInput.y );
			const float fCamOrient = camInterface::GetPlayerControlCamHeading( *pPed );
			fDesiredStickAngle = fDesiredStickAngle + fCamOrient;
			fDesiredStickAngle = fwAngle::LimitRadianAngle( fDesiredStickAngle );

			// Fudge the stick angle when inside min strafing distance
			// This allows a player to still side step even with minimal angle input
			Vector3 vDiff( Vector3::ZeroType );
			if( pTargetEntity )
			{
				vDiff = VEC3V_TO_VECTOR3( pTargetEntity->GetTransform().GetPosition() - pPed->GetTransform().GetPosition() );
// 				grcDebugDraw::Line( VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() ), VEC3V_TO_VECTOR3( pTargetEntity->GetTransform().GetPosition() ), Color_green );
// 
// 				float fDistSq = vDiff.Mag2();
// 				Vector3 vDiffScaled = vDiff;
// 				vDiffScaled.Scale( 0.5f );
// 
// 				Color32 colour(Color_red);
// 				char debugText[100];
// 				sprintf(debugText, "DistSq %f\n", fDistSq);
// 				grcDebugDraw::Text( VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() ) + vDiffScaled, colour, debugText);
			}

			if( pTargetEntity && vDiff.Mag2() < rage::square( fDistanceRestriction ) )
			{
				vDiff.Normalize();
				float fForwardAngle = rage::Atan2f( -vDiff.x, vDiff.y );
				float fDifference = fwAngle::LimitRadianAngle( fDesiredStickAngle - fForwardAngle );

				// If the player is getting close to the strafe threshold, then nudge the direction a bit
				static dev_float sfStickAngleThreshold = 0.65f;
				if( Abs( fDifference ) > sfStickAngleThreshold )
				{
					float fLerpRatio = ( Abs( fDifference ) - sfStickAngleThreshold ) / ( PI - sfStickAngleThreshold );
					float fLerpAmount = Sign( fDifference ) * Lerp( fLerpRatio, HALF_PI, PI );
					rfModifiedStickAngle = fwAngle::LimitRadianAngle( fLerpAmount + fForwardAngle );
					rbStickAngleTweaked = true;
				}
				// Otherwise prevent the player from going any further forward
				else
				{
					vecStickInput.Zero();
					pPed->GetMotionData()->SetCurrentMoveBlendRatio( 0.0f, 0.0f );
					return vecStickInput;
				}
			}
			// Lets smooth out the stick angle if previously tweaked
			else if( rbStickAngleTweaked )
			{
				static dev_float sfMaxStickAngleChangeRate = 3.5f;
				const float fMaxTimeStepChange = fwTimer::GetTimeStep() * sfMaxStickAngleChangeRate;

				// Work out the requested change in current value for this timestep
				float fDelta = SubtractAngleShorter( fDesiredStickAngle, rfModifiedStickAngle );
				if( Abs( fDelta ) < 0.1f )
				{
					rfModifiedStickAngle = fDesiredStickAngle;
					rbStickAngleTweaked = false;
				}
				else
				{
					// Clamp the requested change so we don't go over the maximum change for this timestep
					fDelta = rage::Clamp(fDelta, -fMaxTimeStepChange, fMaxTimeStepChange);
					rfModifiedStickAngle += fDelta;
				}
			}
			// Otherwise do a straight copy
			else
			{
				rfModifiedStickAngle = fDesiredStickAngle;
			}

			// do not allow melee peds to get closer than the minimum impetus range
			const Vector3 vecDesiredDirection( -rage::Sinf( rfModifiedStickAngle ), rage::Cosf( rfModifiedStickAngle ), 0.0f);
			vecStickInput.x = fMoveRatio * DotProduct( vecDesiredDirection, VEC3V_TO_VECTOR3( pPed->GetTransform().GetA() ) );
			vecStickInput.y = fMoveRatio * DotProduct( vecDesiredDirection, VEC3V_TO_VECTOR3( pPed->GetTransform().GetB() ) );

			vecStickInput.x = Clamp(vecStickInput.x + pPed->GetMotionData()->GetSteerBias(), -1.0f, 1.0f);
			vecStickInput.y = Clamp(vecStickInput.y, -1.0f, 1.0f);
		}
	}

	return vecStickInput;
}

CTaskMove * CTaskMove::GetControlMovementBackup(const CPed * pPed)
{
	CTaskComplexControlMovement * pCtrlMove = (CTaskComplexControlMovement*) pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT);
	if(pCtrlMove && pCtrlMove->GetRunningMovementTask()==this)
	{
		CTask * pBackup = pCtrlMove->GetBackupCopyOfMovementSubtask();
		if(pBackup && pBackup->GetTaskType()==GetTaskType())
		{
			Assert(pBackup->IsMoveTask());
			return (CTaskMove*)pBackup;
		}
	}

	return NULL;
}

//----------------------------------------------------------------------------------
// CLASS : CTaskMoveBase
// PURPOSE : Base class from which lowest level locomotion tasks should be derived

CTaskMoveBase::CTaskMoveBase(const float fMoveBlend)
: CTaskMove(fMoveBlend),
m_fTheta(0.0f), m_fPitch(0.0f), m_bDisableHeadingUpdate(false)
{
	Assertf(fMoveBlend >= MOVEBLENDRATIO_STILL && fMoveBlend <= MOVEBLENDRATIO_SPRINT, "fMoveBlend of %.2f used to construct movement task (should be in %.1f to %.1f range inclusive)", fMoveBlend, MOVEBLENDRATIO_STILL, MOVEBLENDRATIO_SPRINT);
	m_fInitialMoveBlendRatio = Clamp(fMoveBlend, MOVEBLENDRATIO_STILL, MOVEBLENDRATIO_SPRINT);
	
}

CTaskMoveBase::~CTaskMoveBase()
{
}

float CTaskMoveBase::ComputeMoveBlend(CPed& ped, const float fTargetSpeed)
{
	int i;
	CMoveBlendRatioSpeeds moveSpeeds;
	ped.GetCurrentMotionTask()->GetMoveSpeeds(moveSpeeds);
	const float * speeds = moveSpeeds.GetSpeedsArray();

	if(fTargetSpeed>=speeds[3])
	{
		//Faster than the sprint.  
		//Ped can't go any faster sprint.
		return 3.0f;
	}
	else
	{
		for(i=0;i<3;i++)
		{
			if(fTargetSpeed < speeds[i+1])
			{
				return (i + (fTargetSpeed-speeds[i])/(speeds[i+1]-speeds[i]));
			}
		}
	}

	Assert(false);
	return 1.0f;
}

void
CTaskMoveBase::SetSpeed(const float UNUSED_PARAM(fSpeedInMetresPerSecond))
{
}

bool CTaskMoveBase::UpdateRideControlSubTasks(CTaskMove& moveTask, float ENABLE_HORSE_ONLY(mbr))
{
	//aiTask* pSubTask = moveTask.GetSubTask();
	if(ShouldRunRideControlSubTasks(*moveTask.GetPed()))
	{
#if ENABLE_HORSE
		// We should use CTaskMoveMountedSpurControl as a subtask, start it if it's not already running.
		aiTask* pSubTask = moveTask.GetSubTask();
		if(!pSubTask)
		{
			// Set the subtask, specifying the move blend ratio we want to achieve by spurring,
			// braking, etc.
			CTaskMoveMountedSpurControl* pNewTask = rage_new CTaskMoveMountedSpurControl;
			pNewTask->SetInputTargetMoveBlendRatio(mbr);
			moveTask.SetNewTask(pNewTask);

			return true;	// Caller shouldn't set the move blend ratio directly.
		}
		else if(pSubTask->GetTaskType() == CTaskTypes::TASK_MOVE_MOUNTED_SPUR_CONTROL)
		{
			// CTaskMoveMountedSpurControl is running already, just update the target move blend ratio in case it changed.
			static_cast<CTaskMoveMountedSpurControl*>(pSubTask)->SetInputTargetMoveBlendRatio(mbr);

			return true;	// Caller shouldn't set the move blend ratio directly.
		}
		// Note: I'm not sure if there are valid cases of other subtasks running here. /FF
#else
		taskAssert(0);
#endif
	}
	else
	{
#if ENABLE_HORSE
		aiTask* pSubTask = moveTask.GetSubTask();
		// Nobody is riding, if for some reason we were using CTaskMoveMountedSpurControl as a subtask, stop it.
		if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_MOVE_MOUNTED_SPUR_CONTROL)
		{
			if(!pSubTask->MakeAbortable(CTask::ABORT_PRIORITY_URGENT, NULL))
			{
				// Failed to abort, so the task is probably still running and we
				// shouldn't set the move blend ratio directly.
				return true;
			}
		}
#endif
	}

	return false;
}

bool CTaskMoveBase::ShouldRunRideControlSubTasks(CPed& ped)
{
	// The current condition we use here is to check if the ped has a "driver",
	// meaning that it's a mounted animal. We may want to extend this for example
	// to also include horses without riders, to get the same type of acceleration,
	// which is how it was done in RDR2 (but with an option to override).
	return ped.GetSeatManager() && ped.GetSeatManager()->GetDriver();
}

CTask::FSM_Return CTaskMoveBase::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Running)
		FSM_OnUpdate
		return StateRunning_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return CTaskMoveBase::StateRunning_OnUpdate(CPed* pPed)
{
	if(ProcessMove(pPed))
	{
		return FSM_Quit;
	}
	else
	{
		MoveAtSpeedInDirection(pPed);

		return FSM_Continue;
	}
}


void CTaskMoveBase::MoveAtSpeedInDirection(CPed* pPed)
{
	//Test if ped needs to do a building or vehicle scan before moving anywhere.
	ScanWorld(pPed);

	// Set the move blend ratio, if we're not running the subtask for doing it.
	if(!UpdateRideControlSubTasks(*this, m_fMoveBlendRatio))
	{
		// If not strafing, then we want to apply movement forwards only
		if(!pPed->IsStrafing())
		{
			pPed->GetMotionData()->SetDesiredMoveBlendRatio(m_fMoveBlendRatio);
		}
		// Otherwise we want to apply a 3d moveratio in the direction of the target
		else
		{
			//Vector3 vToTarget = GetTarget() - pPed->GetPosition();
			const Vector3 & vCurrentTarget = GetCurrentTarget();
			Vector3 vToTarget = vCurrentTarget - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

			if(vToTarget.Mag2() > 0.0f)
			{
				Matrix34 mat;
				mat.Identity();
				mat.RotateZ(-pPed->GetCurrentHeading());

				mat.Transform3x3(vToTarget);

				vToTarget.z = 0.0f;
				vToTarget.Normalize();
				vToTarget *= m_fMoveBlendRatio;

				pPed->GetMotionData()->SetDesiredMoveBlendRatio(vToTarget.y, vToTarget.x);
			}
			else
			{
				pPed->GetMotionData()->SetDesiredMoveBlendRatio(0.0f, 0.0f);
			}
		}
	}

	//Set the ped's heading.
	SetPedHeadingAndPitch(pPed);
}


void CTaskMoveBase::SetPedHeadingAndPitch(CPed* pPed)
{
	// If not strafing, then we want to face our target
	if(!pPed->IsStrafing())
	{
		if(!m_bDisableHeadingUpdate && !pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableMoveTaskHeadingAdjustments))
			pPed->SetDesiredHeading(m_fTheta);

		if(pPed->GetCurrentMotionTask()->IsInWater())
		{
			pPed->SetDesiredPitch(m_fPitch);
		}
	}
}

void CTaskMoveBase::ScanWorld(CPed* pPed)
{
	if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_HasJustLeftCar ))
	{
		//This is the first time the ped has tried to go to a point since the ped got out of a car.
		//Force the ped to do a scan of potential vehicle collision events so that the ped automatically knows
		//to walk around the car.
		pPed->GetPedIntelligence()->GetEventScanner()->ScanForEventsNow(*pPed,CEventScanner::VEHICLE_POTENTIAL_COLLISION_SCAN);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_HasJustLeftCar, false );
	}
}

