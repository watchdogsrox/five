// Filename   :	TaskRageRagdoll.cpp
// Description:	Handle the rage ragdoll task, then transfer to TaskBlendFromNM
//
// --- Include Files ------------------------------------------------------------

// Rage headers
#include "pharticulated/articulatedcollider.h"
#include "phbound/boundcomposite.h"
#include "fwsys/timer.h"

// Game headers
#include "Task/Movement/TaskAnimatedFallback.h"
#include "Task/Physics/TaskNMPrototype.h"
#include "Task/Physics/TaskRageRagdoll.h"
#include "Task/Physics/TaskBlendFromNM.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "Peds/PedIntelligence.h"
#include "performance/clearinghouse.h"

AI_OPTIMISATIONS()

// Tunable parameters. ///////////////////////////////////////////////////
CTaskRageRagdoll::Tunables CTaskRageRagdoll::sm_Tunables;
IMPLEMENT_PHYSICS_TASK_TUNABLES(CTaskRageRagdoll, 0x17564231);
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskFSMClone *CClonedRageRagdollInfo::CreateCloneFSMTask()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	return rage_new CTaskRageRagdoll();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskRageRagdoll::CTaskRageRagdoll(bool bRunningPrototype, CEntity *pHitEntity)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
: m_bRunningPrototype(bRunningPrototype) 
, m_pHitEntity(pHitEntity) 
{
	Init();
	SetInternalTaskType(CTaskTypes::TASK_RAGE_RAGDOLL);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskRageRagdoll::Init()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	m_driveTimer = 30;
	m_StartedCorrectingPoses = false;
	m_WritheFinished = false;
	m_UpwardCarImpactApplied = false;

	m_suggestedClipId = CLIP_ID_INVALID;
	m_suggestedClipSetId = CLIP_SET_ID_INVALID;
	m_suggestedClipPhase = 0.0f;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskRageRagdoll::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed *pPed = GetPed(); 

	if(iPriority==ABORT_PRIORITY_IMMEDIATE || pPed->IsNetworkClone())
	{
		return CTask::ShouldAbort(iPriority, pEvent);
	}

	eEventType etype = EVENT_NONE;
	if(pEvent)
		etype = static_cast<eEventType>((static_cast<const CEvent*>(pEvent))->GetEventType());

	if (etype == EVENT_DEATH || etype == EVENT_DAMAGE)
	{
		return CTask::ShouldAbort(iPriority, pEvent);
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskRageRagdoll::DoAbort(const AbortPriority iPriority, const aiEvent* pEvent)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CTask::DoAbort(iPriority, pEvent);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskRageRagdoll::ProcessPreFSM()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// Force disable leg ik.

	pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_OFF);
	pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_FADE_OFF);

	return FSM_Continue;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskRageRagdoll::UpdateFSM(const s32 iState, const FSM_Event iEvent)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin

		FSM_State(State_Ragdoll)
			FSM_OnEnter
				Ragdoll_OnEnter(pPed);
			FSM_OnUpdate
				return Ragdoll_OnUpdate(pPed);

		FSM_State(State_AnimatedFallback)
			FSM_OnEnter
				AnimatedFallback_OnEnter(pPed);
			FSM_OnUpdate
				return AnimatedFallback_OnUpdate(pPed);

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskRageRagdoll::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	return UpdateFSM(iState, iEvent);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskRageRagdoll::ControlPassingAllowed(CPed* pPed, const netPlayer& player, eMigrationType migrationType)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// If we're in the animated fall back state but haven't yet created a subtask then don't allow control passing
	// Certain animated fall back sub tasks (such as CTaskFallOver) don't allow control passing until they've entered a certain state
	// If those sub tasks haven't yet been created then we have to assume that we can't yet pass control!
	if (GetState() == State_AnimatedFallback && !GetSubTask())
	{
		return false;
	}

	return CTaskFSMClone::ControlPassingAllowed(pPed, player, migrationType);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskRageRagdoll::Ragdoll_OnEnter(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CTaskDyingDead* pDeathTask = static_cast<CTaskDyingDead*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DYING_DEAD));
	if (pDeathTask)
	{
		if (pDeathTask->HasCorpseRelaxed() || pPed->GetDeathState() != DeathState_Dead)
		{
			m_driveTimer = -1;
		}
		else
		{
			pDeathTask->SetHasCorpseRelaxed();
		}
	}

	// Active the Ped is not already
	if (pPed->GetRagdollState() < RAGDOLL_STATE_PHYS_ACTIVATE)
	{
		// Block NM activation
		pPed->GetRagdollInst()->SetBlockNMActivation(true);

		// Network peds are allowed to run NM tasks without being able to actually switch to ragdoll since they need to synchronize properly
		// with clones/owners that could be allowed to ragdoll. We need to check here that we're actually allowed to ragdoll before switching
		CTaskNMControl* pTaskNMControl = static_cast<CTaskNMControl*>(FindParentTaskOfType(CTaskTypes::TASK_NM_CONTROL));
		if (pTaskNMControl && NetworkInterface::IsGameInProgress())
		{
			pTaskNMControl->SwitchClonePedToRagdoll(pPed);
		}
		else
		{
			// Not sure if we even need this switch to ragdoll at this point since we should have already switched at some earlier point and this
			// could introduce issues where peds switch to ragdoll without calling CTaskNMBehaviour::CanUseRagdoll
			pPed->SwitchToRagdoll(*this);
		}

		// Re-allow NM activation
		pPed->GetRagdollInst()->SetBlockNMActivation(false);
	}

	if(pPed->GetUsingRagdoll())
	{
		// Set the stiffness.  
		if (!pPed->GetRagdollInst()->GetBulletLoosenessActive())
		{
			if (pPed->GetCollider() && ((phArticulatedCollider*)pPed->GetCollider())->GetBody())
			{
				phArticulatedBody *body = ((phArticulatedCollider*)pPed->GetCollider())->GetBody();
				if (body)
				{
					static float stiffnessHuman = 0.5f; 
					static float stiffnessAnimal = 0.25f; 
					static float stiffnessWater = 0.1f;
					body->SetStiffness(pPed->GetIsInWater() ? stiffnessWater : pPed->GetRagdollInst()->GetType()->GetARTAssetID() >= 0 ? stiffnessHuman : stiffnessAnimal);
				}
			}
		}

		// Ensure that the NM agent is removed
		pPed->GetRagdollInst()->SwitchFromNMToRageRagdoll(!NetworkInterface::IsGameInProgress());

		// Ensure that we are in the correct pool
		if (CTaskNMBehaviour::GetRagdollPool(CTaskNMBehaviour::kRagdollPoolRageRagdoll).GetFreeSlots() > 0)
		{
			CTaskNMBehaviour::AddToRagdollPool(CTaskNMBehaviour::kRagdollPoolRageRagdoll, *pPed);
		}

		// Allow extra penetration on rage ragdolls to reduce the chance of needing pushes
		phCollider *collider = const_cast<phCollider *>(pPed->GetCollider());
		if (collider)
		{
			collider->SetExtraAllowedPenetration(fragInstNM::GetExtraAllowedRagdollPenetration());
		}
	}

	// Use default friction
	pPed->SetCorpseRagdollFriction(-1.0f);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskRageRagdoll::Ragdoll_OnUpdate(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (pPed && pPed->GetUsingRagdoll() && pPed->GetCollider())
	{
		switch(pPed->GetRagdollInst()->GetCurrentPhysicsLOD())
		{
		case fragInst::RAGDOLL_LOD_HIGH:
			{
				perfClearingHouse::Increment(perfClearingHouse::HIGH_LOD_RAGDOLLS);
				break;
			}
		case fragInst::RAGDOLL_LOD_MEDIUM:
			{
				perfClearingHouse::Increment(perfClearingHouse::MEDIUM_LOD_RAGDOLLS);
				break;
			}
		case fragInst::RAGDOLL_LOD_LOW:
			{
				perfClearingHouse::Increment(perfClearingHouse::LOW_LOD_RAGDOLLS);
				break;
			}
		default: 
			{
				break;
			}
		}
		
		if (pPed->GetRagdollInst()->GetType()->GetARTAssetID() < 0)
		{
			// Writhe
			ProcessWrithe(pPed);
		}

		// In MP games we ramp the joint stiffness/damping down over time.  This change wasn't implemented in time to
		// be properly tested for SP but is likely something that would be beneficial in all game modes for future projects...
		if (NetworkInterface::IsGameInProgress())
		{
			ProcessJointRelaxation(pPed);
		}

		ProcessHitByVehicle(pPed);
		
		CorrectUnstablePoses(pPed);

		if (m_driveTimer == 0 && !NetworkInterface::IsGameInProgress())
		{
			((phArticulatedCollider*)pPed->GetCollider())->GetBody()->SetDriveState(phJoint::DRIVE_STATE_FREE);
		}
		else if (m_driveTimer >= 0 && pPed->GetRagdollInst()->GetCurrentPhysicsLOD() == fragInst::RAGDOLL_LOD_HIGH &&
			pPed->GetRagdollInst()->GetType()->GetARTAssetID() >= 0)
		{
			if (pPed->IsProne())
			{
				CorrectAwkwardPoses(pPed);
			}

			if (m_StartedCorrectingPoses)
			{
				m_driveTimer--;
			}
		}

		// Wait for a few seconds then exit
		static const float defaultMaxTime = 3.5f;
		float maxRunTime = defaultMaxTime;
#if __BANK	
		if (m_bRunningPrototype)
		{
			maxRunTime = CTaskNMPrototype::GetRunForever() ? FLT_MAX : static_cast<float>(CTaskNMPrototype::GetDesiredSimulationTime()) / 1000.0f;
		}
#endif
		if (GetTimeInState() > maxRunTime && !pPed->IsNetworkClone()) //don't time out for network clones, the task needs to keep running until told to stop via an update
		{
			// Return control to NM Control
			SetState(State_Finish);
		}
		else
		{
			// Let the ped know that a valid task is in control of the ragdoll.
			pPed->TickRagdollStateFromTask(*this);
		}
	}
	else if (pPed->GetMovePed().GetState() == CMovePed::kStateStaticFrame)
	{
		nmTaskDebugf(this, "Quitting task due to ped being in an animated static frame state");
		return FSM_Quit;
	}
	else
	{
		// We're no longer in ragdoll(we can be forced back into animated at any moment by script, etc)
		// Go to fallback state.
		nmTaskDebugf(this, "Switching to animated fallback");
		SetState(State_AnimatedFallback);
	}

	return FSM_Continue;
}

void CTaskRageRagdoll::ProcessJointRelaxation(CPed* pPed)
{
	phArticulatedBody* pBody = static_cast<phArticulatedCollider*>(pPed->GetCollider())->GetBody();

	if (pBody != NULL)
	{
		for (int i = 0; i < pBody->GetNumJoints(); i++)
		{
			if (pBody->GetJoint(i).GetDriveState() != phJoint::DRIVE_STATE_FREE)
			{
				switch (pBody->GetTypeOfJoint(i))
				{
				case phJoint::JNT_1DOF:
					{
						float fCurrentMuscleAngleStrength = pBody->GetJoint1Dof(i).GetMuscleAngleStrength();
						float fCurrentMuscleSpeedStrength = pBody->GetJoint1Dof(i).GetMuscleSpeedStrength();
						bool bReachedAngleGoal = Approach(fCurrentMuscleAngleStrength, 0.0f, sm_Tunables.m_fMuscleAngleStrengthRampDownRate, fwTimer::GetTimeStep());
						bool bReachedSpeedGoal = Approach(fCurrentMuscleSpeedStrength, 0.0f, sm_Tunables.m_fMuscleSpeedStrengthRampDownRate, fwTimer::GetTimeStep());

						if (bReachedAngleGoal && bReachedSpeedGoal)
						{
							pBody->GetJoint1Dof(i).SetMuscleTargetAngle(0.0f);
							pBody->GetJoint1Dof(i).SetMuscleTargetSpeed(0.0f);
							pBody->GetJoint(i).SetDriveState(phJoint::DRIVE_STATE_FREE);
						}
						else
						{
							pBody->GetJoint1Dof(i).SetMuscleAngleStrength(fCurrentMuscleAngleStrength);
							pBody->GetJoint1Dof(i).SetMuscleSpeedStrength(fCurrentMuscleSpeedStrength);
						}
						break;
					}
				case phJoint::JNT_3DOF:
					{
						Vector3 vCurrentMuscleAngleStrength = pBody->GetJoint3Dof(i).GetMuscleAngleStrength();
						Vector3 vCurrentMuscleSpeedStrength = pBody->GetJoint3Dof(i).GetMuscleSpeedStrength();
						bool bReachedAngleGoal = vCurrentMuscleAngleStrength.ApproachStraight(VEC3_ZERO, sm_Tunables.m_fMuscleAngleStrengthRampDownRate, fwTimer::GetTimeStep());
						bool bReachedSpeedGoal = vCurrentMuscleSpeedStrength.ApproachStraight(VEC3_ZERO, sm_Tunables.m_fMuscleSpeedStrengthRampDownRate, fwTimer::GetTimeStep());

						if (bReachedAngleGoal && bReachedSpeedGoal)
						{
							pBody->GetJoint3Dof(i).SetMuscleTargetAngle(VEC3_ZERO);
							pBody->GetJoint3Dof(i).SetMuscleTargetSpeed(VEC3_ZERO);
							pBody->GetJoint(i).SetDriveState(phJoint::DRIVE_STATE_FREE);
						}
						else
						{
							pBody->GetJoint3Dof(i).SetMuscleAngleStrength(vCurrentMuscleAngleStrength);
							pBody->GetJoint3Dof(i).SetMuscleSpeedStrength(vCurrentMuscleSpeedStrength);
						}
						break;
					}
				default:
					{
						break;
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskRageRagdoll::CorrectUnstablePoses(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (pPed->GetRagdollInst()->GetType()->GetARTAssetID() >= 0 && pPed->GetRagdollInst()->GetCurrentPhysicsLOD() == fragInst::RAGDOLL_LOD_HIGH && !pPed->GetIsInWater())
	{
		phArticulatedBody *body = pPed->GetRagdollInst()->GetArticulatedBody();

		// If the pelvis is resting on both feet, push the pelvis to the side
		phBoundComposite *bound = pPed->GetRagdollInst()->GetCacheEntry()->GetBound();
		Matrix34 mTemp;
		mTemp.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(RAGDOLL_BUTTOCKS)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); 
		Vector3 vPelvisPos = mTemp.d;
		mTemp.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(RAGDOLL_FOOT_RIGHT)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); 
		Vector3 vRightFootPos = mTemp.d;
		mTemp.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(RAGDOLL_FOOT_LEFT)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); 
		Vector3 vLeftFootPos = mTemp.d;

		float fDistRFootPelvisSq = vPelvisPos.Dist(vRightFootPos);
		float fDistLFootPelvisSq = vPelvisPos.Dist(vLeftFootPos);
		float fDistRFootLFootSq = vLeftFootPos.Dist(vRightFootPos);
		float fPelvisHeightAboveLFoot = vPelvisPos.z - vLeftFootPos.z;
		float fPelvisHeightAboveRFoot = vPelvisPos.z - vRightFootPos.z;

		static float sfPelvisAboveFeetThresholdMin = 0.15f;
		static float sfPelvisAboveFeetThresholdMax = 0.4f;
		static float sfCloseBoundThreshold = 0.39f;
		static float sfForceMag = 100.0f;
		if (fDistRFootPelvisSq <= sfCloseBoundThreshold && fDistLFootPelvisSq <= sfCloseBoundThreshold && 
			fDistRFootLFootSq <= sfCloseBoundThreshold &&
			fPelvisHeightAboveLFoot >= sfPelvisAboveFeetThresholdMin && fPelvisHeightAboveRFoot >= sfPelvisAboveFeetThresholdMin &&
			fPelvisHeightAboveLFoot <= sfPelvisAboveFeetThresholdMax && fPelvisHeightAboveRFoot <= sfPelvisAboveFeetThresholdMax
			&& body)
		{
			ScalarV vTimeStep = ScalarV(fwTimer::GetTimeStep() / CPhysics::GetNumTimeSlices());
			fragInstNMGta *pInst = pPed->GetRagdollInst();
			Vector3 vForce = vRightFootPos - vLeftFootPos;
			vForce.z = 0.0f;
			vForce.NormalizeFast();

			for (int iLink = 0; iLink < body->GetNumBodyParts(); iLink++)
			{
				if (pInst->GetBodyPartGroup(iLink) == fragInstNMGta::kSpine)
				{
					body->ApplyForce(iLink, vForce*sfForceMag * body->GetMass(iLink).Getf(), body->GetLink(iLink).GetPosition(), vTimeStep.GetIntrin128Ref());
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskRageRagdoll::CorrectAwkwardPoses(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (pPed->GetRagdollInst()->GetType()->GetARTAssetID() >= 0 && pPed->GetRagdollInst()->GetCurrentPhysicsLOD() == fragInst::RAGDOLL_LOD_HIGH && !pPed->GetIsInWater())
	{
		m_StartedCorrectingPoses = true;

		// Only apply corrective forces if the ped is moving relatively slowly 
		static float sfSlowVel = 1.5f;
		Vec3V relativeVelocity = Subtract(pPed->GetCollider()->GetVelocity(), pPed->GetCollider()->GetReferenceFrameVelocity());
		float fCOMvel = Mag(relativeVelocity).Getf();
		if (fCOMvel <= sfSlowVel)
		{
			// Reduce friction to avoid getting stuck on the ground while trying to avoid awkward poses
			static float sfFriction = 0.4f;
			pPed->SetCorpseRagdollFriction(sfFriction);

			// Keep the ped awake while correcting awkward poses
			pPed->GetCollider()->GetSleep()->Reset();

			// Get orientation data for the Ped
			phArticulatedBody *body = ((phArticulatedCollider*)pPed->GetCollider())->GetBody();
			phBoundComposite *bound = pPed->GetRagdollInst()->GetCacheEntry()->GetBound();
			Matrix34 pelvisMat;
			pelvisMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(0)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); // The pelvis
			Vec3V dirFacing = -(RCC_MAT34V(pelvisMat).GetCol2());  
			Vec3V dirSide = RCC_MAT34V(pelvisMat).GetCol1();  // The left

			// If the torso is facing up or down, straighten the legs
			static float damping = 1.0f;
			static float torsoUpDotLim = 0.8f;
			static float kneeStrength = 7.0f;
			static float hipStrength = 7.0f;
			static float straightKneeAngle = 0.0f;
			static float straightHipAngle1 = -1.0f;
			float torsoUpDot = Dot(dirFacing, Vec3V(V_Z_AXIS_WZERO)).Getf();
			if (Abs(torsoUpDot) > torsoUpDotLim)
			{
				body->GetJoint(RAGDOLL_KNEE_LEFT_JOINT).SetDriveState(phJoint::DRIVE_STATE_ANGLE_AND_SPEED);
				body->GetJoint1Dof(RAGDOLL_KNEE_LEFT_JOINT).SetMuscleAngleStrength(kneeStrength);
				body->GetJoint1Dof(RAGDOLL_KNEE_LEFT_JOINT).SetMuscleSpeedStrength(damping);
				body->GetJoint1Dof(RAGDOLL_KNEE_LEFT_JOINT).SetMuscleTargetAngle(straightKneeAngle);

				body->GetJoint(RAGDOLL_KNEE_RIGHT_JOINT).SetDriveState(phJoint::DRIVE_STATE_ANGLE_AND_SPEED);
				body->GetJoint1Dof(RAGDOLL_KNEE_RIGHT_JOINT).SetMuscleAngleStrength(kneeStrength);
				body->GetJoint1Dof(RAGDOLL_KNEE_RIGHT_JOINT).SetMuscleSpeedStrength(damping);
				body->GetJoint1Dof(RAGDOLL_KNEE_RIGHT_JOINT).SetMuscleTargetAngle(straightKneeAngle);

				body->GetJoint(RAGDOLL_HIP_LEFT_JOINT).SetDriveState(phJoint::DRIVE_STATE_ANGLE_AND_SPEED);
				body->GetJoint3Dof(RAGDOLL_HIP_LEFT_JOINT).SetMuscleAngleStrength(hipStrength);
				body->GetJoint3Dof(RAGDOLL_HIP_LEFT_JOINT).SetMuscleSpeedStrength(damping);
				body->GetJoint3Dof(RAGDOLL_HIP_LEFT_JOINT).SetLean1TargetAngle(straightHipAngle1);

				body->GetJoint(RAGDOLL_HIP_RIGHT_JOINT).SetDriveState(phJoint::DRIVE_STATE_ANGLE_AND_SPEED);
				body->GetJoint3Dof(RAGDOLL_HIP_RIGHT_JOINT).SetMuscleAngleStrength(hipStrength);
				body->GetJoint3Dof(RAGDOLL_HIP_RIGHT_JOINT).SetMuscleSpeedStrength(damping);
				body->GetJoint3Dof(RAGDOLL_HIP_RIGHT_JOINT).SetLean1TargetAngle(straightHipAngle1);

				// If torso is facing up... ensure ankles point upwards and legs aren't rotated - otherwise they can rest on an edge of the ankle bound box and lift
				// the leg off the ground slightly - making it look like the leg is floating
				if (torsoUpDot > torsoUpDotLim)
				{
					static float ankleStrength = 15.0f;
					static float uprightAnkleAngle1 = 0.2f;
					static float uprightAnkleAngle2 = 0.0f;
					body->GetJoint(RAGDOLL_ANKLE_LEFT_JOINT).SetDriveState(phJoint::DRIVE_STATE_ANGLE_AND_SPEED);
					body->GetJoint3Dof(RAGDOLL_ANKLE_LEFT_JOINT).SetMuscleAngleStrength(ankleStrength);
					body->GetJoint3Dof(RAGDOLL_ANKLE_LEFT_JOINT).SetMuscleSpeedStrength(damping);
					body->GetJoint3Dof(RAGDOLL_ANKLE_LEFT_JOINT).SetLean1TargetAngle(uprightAnkleAngle1);
					body->GetJoint3Dof(RAGDOLL_ANKLE_LEFT_JOINT).SetLean2TargetAngle(uprightAnkleAngle2);

					body->GetJoint(RAGDOLL_ANKLE_RIGHT_JOINT).SetDriveState(phJoint::DRIVE_STATE_ANGLE_AND_SPEED);
					body->GetJoint3Dof(RAGDOLL_ANKLE_RIGHT_JOINT).SetMuscleAngleStrength(ankleStrength);
					body->GetJoint3Dof(RAGDOLL_ANKLE_RIGHT_JOINT).SetMuscleSpeedStrength(damping);
					body->GetJoint3Dof(RAGDOLL_ANKLE_RIGHT_JOINT).SetLean1TargetAngle(uprightAnkleAngle1);
					body->GetJoint3Dof(RAGDOLL_ANKLE_RIGHT_JOINT).SetLean2TargetAngle(uprightAnkleAngle2);

					static float straightHipLeftTwistAngle = 0.0f;
					static float straightHipRightTwistAngle = 0.0f;
					body->GetJoint3Dof(RAGDOLL_HIP_LEFT_JOINT).SetTwistTargetSpeed(ankleStrength);
					body->GetJoint3Dof(RAGDOLL_HIP_LEFT_JOINT).SetTwistTargetAngle(straightHipLeftTwistAngle);
					body->GetJoint3Dof(RAGDOLL_HIP_RIGHT_JOINT).SetTwistTargetSpeed(ankleStrength);
					body->GetJoint3Dof(RAGDOLL_HIP_RIGHT_JOINT).SetTwistTargetAngle(straightHipRightTwistAngle);
				}
			}	
			else if (!NetworkInterface::IsGameInProgress())
			{
				body->GetJoint(RAGDOLL_KNEE_LEFT_JOINT).SetDriveState(phJoint::DRIVE_STATE_FREE);
				body->GetJoint(RAGDOLL_KNEE_RIGHT_JOINT).SetDriveState(phJoint::DRIVE_STATE_FREE);
			}

			// If the forearms are facing up or down, straighten the elbows
			if (!NetworkInterface::IsGameInProgress())
			{
				body->GetJoint(RAGDOLL_ELBOW_RIGHT_JOINT).SetDriveState(phJoint::DRIVE_STATE_FREE);
				body->GetJoint(RAGDOLL_ELBOW_LEFT_JOINT).SetDriveState(phJoint::DRIVE_STATE_FREE);
			}
			static float elbowStrength = 10.0f;
			Matrix34 forearmMat;
			forearmMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(RAGDOLL_LOWER_ARM_RIGHT)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); 
			Vec3V dirPointing = -(RCC_MAT34V(forearmMat).GetCol1());  
			static float forearmUpDotLim = 0.7f;
			float forearmUpDot = Abs(Dot(dirPointing, Vec3V(V_Z_AXIS_WZERO)).Getf());
			if (forearmUpDot > forearmUpDotLim)
			{
				body->GetJoint(RAGDOLL_ELBOW_RIGHT_JOINT).SetDriveState(phJoint::DRIVE_STATE_ANGLE_AND_SPEED);
				body->GetJoint1Dof(RAGDOLL_ELBOW_RIGHT_JOINT).SetMuscleSpeedStrength(damping);
				body->GetJoint1Dof(RAGDOLL_ELBOW_RIGHT_JOINT).SetMuscleAngleStrength(elbowStrength);
				body->GetJoint1Dof(RAGDOLL_ELBOW_RIGHT_JOINT).SetMuscleTargetAngle(0.0f);
			}
			forearmMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(RAGDOLL_LOWER_ARM_LEFT)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); 
			dirPointing = -(RCC_MAT34V(forearmMat).GetCol1());  
			forearmUpDot = Abs(Dot(dirPointing, Vec3V(V_Z_AXIS_WZERO)).Getf());
			if (forearmUpDot > forearmUpDotLim)
			{
				body->GetJoint(RAGDOLL_ELBOW_LEFT_JOINT).SetDriveState(phJoint::DRIVE_STATE_ANGLE_AND_SPEED);
				body->GetJoint1Dof(RAGDOLL_ELBOW_LEFT_JOINT).SetMuscleSpeedStrength(damping);
				body->GetJoint1Dof(RAGDOLL_ELBOW_LEFT_JOINT).SetMuscleAngleStrength(elbowStrength);
				body->GetJoint1Dof(RAGDOLL_ELBOW_LEFT_JOINT).SetMuscleTargetAngle(0.0f);
			}

			// If the torso is above the pelvis, push the chest down and to one side
			Matrix34 mat;
			mat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(RAGDOLL_BUTTOCKS)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix()));
			Vector3 dirLeft = mat.b;  
			dirLeft.z = 0.0f;
			dirLeft.NormalizeSafe(dirLeft);
			float pelvisHeight = mat.d.z;
			mat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(RAGDOLL_SPINE3)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix()));
			float chestHeight = mat.d.z;
			static float distMin = 0.1f;
			static float downForce = -100.0f;
			static float sideForceMag = -100.0f;
			float pelvisToChest = chestHeight - pelvisHeight;
			Vector3 vSideForce = RCC_VECTOR3(dirSide) * sideForceMag;
			if (pelvisToChest > distMin)
			{
				pPed->ApplyForce(Vector3(0.0f,0.0f,downForce), mat.d - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), RAGDOLL_SPINE3);
				pPed->ApplyForce(vSideForce, mat.d - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), RAGDOLL_SPINE3);
			}

			// If the pelvis is above the torso, straighten the spine and thighs
			if (pelvisHeight-chestHeight > distMin)
			{
				static float strength = 7.0f;
				static float damping = 1.0f;
				body->GetJoint(RAGDOLL_HIP_LEFT_JOINT).SetDriveState(phJoint::DRIVE_STATE_ANGLE_AND_SPEED);
				body->GetJoint3Dof(RAGDOLL_HIP_LEFT_JOINT).SetMuscleAngleStrength(strength);
				body->GetJoint3Dof(RAGDOLL_HIP_LEFT_JOINT).SetMuscleSpeedStrength(damping);
				body->GetJoint3Dof(RAGDOLL_HIP_LEFT_JOINT).SetLean1TargetAngle(straightHipAngle1);
				body->GetJoint(RAGDOLL_HIP_RIGHT_JOINT).SetDriveState(phJoint::DRIVE_STATE_ANGLE_AND_SPEED);
				body->GetJoint3Dof(RAGDOLL_HIP_RIGHT_JOINT).SetMuscleAngleStrength(strength);
				body->GetJoint3Dof(RAGDOLL_HIP_RIGHT_JOINT).SetMuscleSpeedStrength(damping);
				body->GetJoint3Dof(RAGDOLL_HIP_RIGHT_JOINT).SetLean1TargetAngle(straightHipAngle1);
				body->GetJoint(RAGDOLL_SPINE0_JOINT).SetDriveState(phJoint::DRIVE_STATE_ANGLE_AND_SPEED);
				body->GetJoint3Dof(RAGDOLL_SPINE0_JOINT).SetMuscleAngleStrength(strength);
				body->GetJoint3Dof(RAGDOLL_SPINE0_JOINT).SetMuscleSpeedStrength(damping);
				body->GetJoint(RAGDOLL_SPINE1_JOINT).SetDriveState(phJoint::DRIVE_STATE_ANGLE_AND_SPEED);
				body->GetJoint3Dof(RAGDOLL_SPINE1_JOINT).SetMuscleAngleStrength(strength);
				body->GetJoint3Dof(RAGDOLL_SPINE1_JOINT).SetMuscleSpeedStrength(damping);
				body->GetJoint(RAGDOLL_SPINE2_JOINT).SetDriveState(phJoint::DRIVE_STATE_ANGLE_AND_SPEED);
				body->GetJoint3Dof(RAGDOLL_SPINE2_JOINT).SetMuscleAngleStrength(strength);
				body->GetJoint3Dof(RAGDOLL_SPINE2_JOINT).SetMuscleSpeedStrength(damping);
			}
			else if (!NetworkInterface::IsGameInProgress())
			{
				body->GetJoint(RAGDOLL_SPINE0_JOINT).SetDriveState(phJoint::DRIVE_STATE_FREE);
				body->GetJoint(RAGDOLL_SPINE1_JOINT).SetDriveState(phJoint::DRIVE_STATE_FREE);
				body->GetJoint(RAGDOLL_SPINE2_JOINT).SetDriveState(phJoint::DRIVE_STATE_FREE);
			}

			// If the corpse is on it's side, add some torque to make the torso level with the ground
			if (Abs(torsoUpDot) < 0.8f)
			{
				// Is the lower elbow in front or in back of the torso? (used to detect which direction rotation would be blocked by the underneath arm)
				float torsoSideDot = Dot(dirSide, Vec3V(V_Z_AXIS_WZERO)).Getf();
				float torqueSigh = 1.0f;
				if (torsoSideDot > 0.0f)
				{
					forearmMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(RAGDOLL_LOWER_ARM_RIGHT)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); 
				}
				else
				{
					torqueSigh = -1.0f;
					forearmMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(RAGDOLL_LOWER_ARM_LEFT)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); 
				}

				Vector3 vChestToArm = forearmMat.d - mat.d;  // mat is assumed to be spine3
				if (vChestToArm.Dot(RCC_VECTOR3(dirFacing)) < 0.0f)
				{
					torqueSigh *= -1.0f;
				}

				static float mult = 60.0f;
				Vector3 torque = mat.a * torqueSigh * mult;
				ScalarV vTimeStep = ScalarV(fwTimer::GetTimeStep() / CPhysics::GetNumTimeSlices());
				pPed->GetRagdollInst()->GetArticulatedBody()->ApplyTorque(RAGDOLL_SPINE3, RCC_VEC3V(torque).GetIntrin128ConstRef(), vTimeStep.GetIntrin128ConstRef());
			}

			// If the corpse is on it's side with an arm in the air, try to push the arm towards the front of the body
			if (Abs(torsoUpDot) < 0.8f)
			{
				float torsoSideDot = Dot(dirSide, Vec3V(V_Z_AXIS_WZERO)).Getf();
				int component = RAGDOLL_LOWER_ARM_LEFT;
				if (torsoSideDot < 0.0f)
				{
					component = RAGDOLL_LOWER_ARM_RIGHT;
				}
				forearmMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(component)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); 
				Vector3 vChestToArm = forearmMat.d - mat.d;  // mat is assumed to be spine3
				if (vChestToArm.z > 0.0f)
				{
					static float mult = 10.0f;
					pPed->ApplyForce(RCC_VECTOR3(dirFacing) * mult, forearmMat.d - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), component);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskRageRagdoll::ProcessWrithe(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (m_WritheFinished)
	{
		return;
	}

	// Determine whether writhe should be finished processing
	static float defaultStiffness = 0.25f;
	u32 elapsed = fwTimer::GetTimeInMilliseconds() - pPed->GetRagdollInst()->GetActivationStartTime();
	static u32 timeToMinStiffness = 4000;
	phArticulatedBody *body = ((phArticulatedCollider*)pPed->GetCollider())->GetBody();
	if ((pPed->IsDead() && (fwTimer::GetTimeInMilliseconds() - pPed->GetDeathTime()) > timeToMinStiffness) || 
		(!pPed->IsDead() && elapsed > timeToMinStiffness))
	{
		if (!NetworkInterface::IsGameInProgress())
		{
			((phArticulatedCollider*)pPed->GetCollider())->GetBody()->SetDriveState(phJoint::DRIVE_STATE_FREE);
			((phArticulatedCollider*)pPed->GetCollider())->GetBody()->SetEffectorsToZeroPose();
		}
		body->SetStiffness(defaultStiffness);
		m_WritheFinished = true;
		return;
	}

	// Keep the ped awake while writhing
	pPed->GetCollider()->GetSleep()->Reset();

	// Process proportional gain ramping
	if (GetTimeInState() > sm_Tunables.m_SpineStrengthTuning.m_fInitialDelay)
	{
		pPed->GetRagdollInst()->ProcessTwoStageRamp(fragInstNMGta::kProportionalGain, fragInstNMGta::kSpine, sm_Tunables.m_SpineStrengthTuning.m_fStartStrength, sm_Tunables.m_SpineStrengthTuning.m_fMidStrength,
			sm_Tunables.m_SpineStrengthTuning.m_fEndStrength, sm_Tunables.m_SpineStrengthTuning.m_fDurationStage1, sm_Tunables.m_SpineStrengthTuning.m_fDurationStage2,	GetTimeInState());
	}
	if (GetTimeInState() > sm_Tunables.m_NeckStrengthTuning.m_fInitialDelay)
	{
		pPed->GetRagdollInst()->ProcessTwoStageRamp(fragInstNMGta::kProportionalGain, fragInstNMGta::kNeck, sm_Tunables.m_NeckStrengthTuning.m_fStartStrength, sm_Tunables.m_NeckStrengthTuning.m_fMidStrength,
			sm_Tunables.m_NeckStrengthTuning.m_fEndStrength, sm_Tunables.m_NeckStrengthTuning.m_fDurationStage1, sm_Tunables.m_NeckStrengthTuning.m_fDurationStage2, GetTimeInState());
	}
	if (GetTimeInState() > sm_Tunables.m_LimbStrengthTuning.m_fInitialDelay)
	{
		pPed->GetRagdollInst()->ProcessTwoStageRamp(fragInstNMGta::kProportionalGain, fragInstNMGta::kLimb, sm_Tunables.m_LimbStrengthTuning.m_fStartStrength, sm_Tunables.m_LimbStrengthTuning.m_fMidStrength,
			sm_Tunables.m_LimbStrengthTuning.m_fEndStrength, sm_Tunables.m_LimbStrengthTuning.m_fDurationStage1, sm_Tunables.m_LimbStrengthTuning.m_fDurationStage2, GetTimeInState());
	}

	// Rotate all joints between -1.0 and 1.0
	body->SetDriveState(phJoint::DRIVE_STATE_ANGLE_AND_SPEED);
	static float rate1Dof = 10.0f;
	static float rate3Dof = 5.0f;
	static float mag1Dof = 0.5f;
	static float mag3Dof = 0.25f;
	static float stagger = 0.75f;
	// static float massRelevance = 0.3f;
	float currentMotorDirection;
	static float sinMultFactor = 0.01f;
	float sinMult = sinMultFactor * elapsed * fwTimer::GetTimeStep();
	for (int iJoint = 0; iJoint < body->GetNumJoints(); iJoint++)
	{
		currentMotorDirection = iJoint % 2 == 0 ? 1.0f : -1.0f;
		switch (body->GetJoint(iJoint).GetJointType())
		{
		case phJoint::JNT_1DOF:
			{
				phJoint1Dof& joint1Dof = *static_cast<phJoint1Dof*>(&body->GetJoint(iJoint));
				joint1Dof.SetMuscleTargetAngle((currentMotorDirection * mag1Dof * sin((rate1Dof * sinMult) + (iJoint * stagger))));
				break;
			}
		case phJoint::JNT_3DOF:
			{
				phJoint3Dof& joint3Dof = *static_cast<phJoint3Dof*>(&body->GetJoint(iJoint));
				Vec3V targetAngles = Vec3V(
					ScalarVFromF32(currentMotorDirection * mag3Dof * sin((rate3Dof * sinMult) + (iJoint * stagger))),
					ScalarVFromF32(currentMotorDirection * mag3Dof * sin((rate3Dof * sinMult) + (iJoint * stagger))),
					ScalarV(V_ZERO));
				joint3Dof.SetMuscleTargetAngle(RCC_VECTOR3(targetAngles));
				break;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskRageRagdoll::ProcessHitByVehicle(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	static float sfMinMass = 80.0f;
	if (m_pHitEntity && m_pHitEntity->GetIsTypeVehicle() && pPed->GetMass() >= sfMinMass)
	{
		CVehicle *pVehicle = static_cast<CVehicle *>(m_pHitEntity.Get());
		if (m_ImpactedHitEntity && pVehicle->GetCollider())
		{
			if (!pVehicle->InheritsFromTrain())
			{
				const ScalarV vCarImpactThreshold(-0.3f);
				const Vec3V vUpAxis(V_UP_AXIS_WZERO);
				const Vec3V vZero(V_ZERO);

				Vec3V vCarVel = pVehicle->GetCollider()->GetLocalVelocity(pPed->GetTransform().GetPosition().GetIntrin128());
				Vec3V vCarVelocityDir = Normalize(vCarVel);
				ScalarV vCarSpeed = Dot(vCarVel, vCarVelocityDir);
				ScalarV vDot = Dot(vUpAxis, m_vHitEntityImpactNormal);
				if (IsGreaterThanAll(vCarSpeed, ScalarV(V_FIVE)) && IsGreaterThanAll(vDot, vCarImpactThreshold))
				{
					const ScalarV vVerticalImpactThreshold(0.6f);

					phArticulatedBody *body = pPed->GetRagdollInst()->GetArticulatedBody();
					fragInstNMGta *fragInst = pPed->GetRagdollInst();

					vDot = Dot(vCarVelocityDir, m_vHitEntityImpactNormal);

					// Front hit ?
					if (!m_UpwardCarImpactApplied && IsGreaterThanAll(vDot, vVerticalImpactThreshold))
					{
						static float upMult = 0.75f;
						Vec3V vImpulse(Scale(vUpAxis, ScalarV(upMult)));

						for (int iLink = 0; iLink < body->GetNumBodyParts(); iLink++)
						{
							if (fragInst->GetBodyPartGroup(iLink) == fragInstNMGta::kSpine)
							{
								body->ApplyImpulse(iLink, Scale(vImpulse, body->GetMass(iLink)), vZero);
							}
						}
						m_UpwardCarImpactApplied = true;
					}
					else 
					{
						ScalarV vTimeStep(fwTimer::GetTimeStep() / CPhysics::GetNumTimeSlices());

						// On top of the car ?
						vDot = Dot(vUpAxis, m_vHitEntityImpactNormal);

						if (IsGreaterThanAll(vDot, vVerticalImpactThreshold))
						{
							// Moving slower than the car in the car's direction of motion?
							Vec3V vRelVel(Subtract(pPed->GetCollider()->GetVelocity(), vCarVel));
							vDot = Dot(vCarVelocityDir, vRelVel);

							static float f1 = -2.0f;
							if (IsLessThanAll(vDot, ScalarVFromF32(f1)))
							{
								// Roll torque
								static float sfMaxSpeed = 8.0f;
								static float sfMinRollMult = 0.0f;
								static float sfMaxRollMult = 3.0f;
								static float sfNeckRollMult = 1.5f;
								float speedRatio = ClampRange(vCarSpeed.Getf(), 0.0f, sfMaxSpeed);
								float rollMult = Lerp(speedRatio, sfMinRollMult, sfMaxRollMult);
								Vec3V vRollAxis = Cross(vCarVelocityDir, vUpAxis);
								vRollAxis = Scale(vRollAxis, ScalarVFromF32(rollMult));
								vRollAxis = Scale(vRollAxis, ScalarVFromF32(pPed->GetMass()));
								Vec3V vNeckRoll = Scale(vRollAxis, ScalarVFromF32(sfNeckRollMult));
								fragInstNMGta::eBodyPartGroup group;
								for (int iLink = 0; iLink < body->GetNumBodyParts(); iLink++)
								{
									group = fragInst->GetBodyPartGroup(iLink);
									if (group == fragInstNMGta::kSpine)
									{
										body->ApplyTorque(iLink, vRollAxis.GetIntrin128Ref(), vTimeStep.GetIntrin128Ref());
									}
									else if (group == fragInstNMGta::kNeck)
									{
										body->ApplyTorque(iLink, vNeckRoll.GetIntrin128Ref(), vTimeStep.GetIntrin128Ref());
									}
								}

								// Align body to roll axis
								Matrix34 spine3Mat, pelvisMat;
								pPed->GetBoneMatrix(spine3Mat, BONETAG_SPINE3);
								pPed->GetBoneMatrix(pelvisMat, BONETAG_PELVIS);
								Vector3 vTemp = spine3Mat.d - pelvisMat.d;
								Vec3V vAnimalUpDir = RC_VEC3V(vTemp);
								vAnimalUpDir = Normalize(vAnimalUpDir);
								vRollAxis = Cross(vCarVelocityDir, vUpAxis);
								vDot = Dot(vAnimalUpDir, vRollAxis);
								if (IsLessThanAll(vDot, ScalarV(V_ZERO)))
								{
									vRollAxis = -vRollAxis;
								}
								static float alignMult = 2.0f;
								Vec3V vAlignTorque(Cross(vAnimalUpDir, vRollAxis));
								ScalarV vAlignMag(Mag(vAlignTorque));
								vAlignTorque = Scale(vAlignTorque, ScalarVFromF32(alignMult));
								vAlignTorque = Scale(vAlignTorque, vAlignMag);
								vAlignTorque = Scale(vAlignTorque, ScalarVFromF32(pPed->GetMass()));
								for (int iLink = 0; iLink < body->GetNumBodyParts(); iLink++)
								{
									if (fragInst->GetBodyPartGroup(iLink) == fragInstNMGta::kSpine)
									{
										body->ApplyTorque(iLink, vAlignTorque.GetIntrin128Ref(), vTimeStep.GetIntrin128Ref());
									}
								}
							}
						}
					}
				}
			}

			m_ImpactedHitEntity = false;
		}
	}
}

CTaskFSMClone* CTaskRageRagdoll::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	CTaskRageRagdoll* pNewTask = static_cast<CTaskRageRagdoll*>(Copy());

	if (pNewTask && pNewTask->GetState() == State_AnimatedFallback)
	{
		const CTaskFallOver* pTaskFallOver = static_cast<CTaskFallOver*>(FindSubTaskOfType(CTaskTypes::TASK_FALL_OVER));
		if (pTaskFallOver != NULL)
		{
			pNewTask->m_suggestedClipId = pTaskFallOver->GetClipId();
			pNewTask->m_suggestedClipSetId = pTaskFallOver->GetClipSetId();
			pNewTask->m_suggestedClipPhase = pTaskFallOver->GetClipPhase();
		}
	}

	return pNewTask;
}

CTaskFSMClone* CTaskRageRagdoll::CreateTaskForLocalPed(CPed* pPed)
{
	return CreateTaskForClonePed(pPed);
}

void CTaskRageRagdoll::AnimatedFallback_OnEnter(CPed* pPed)
{
	fwMvClipSetId clipSetId = CLIP_SET_ID_INVALID;
	fwMvClipId clipId = CLIP_ID_INVALID;

	Vector3 vecHeadPos(0.0f,0.0f,0.0f);
	pPed->GetBonePosition(vecHeadPos, BONETAG_HEAD);
	// If the ped is already prone then use an on-ground damage reaction
	if (vecHeadPos.z < pPed->GetTransform().GetPosition().GetZf())
	{
		const EstimatedPose pose = pPed->EstimatePose();
		if (pose == POSE_ON_BACK)
		{
			clipId = CLIP_DAM_FLOOR_BACK;
		}
		else
		{
			clipId = CLIP_DAM_FLOOR_FRONT;
		}

		Matrix34 rootMatrix;
		if (pPed->GetBoneMatrix(rootMatrix, BONETAG_ROOT))
		{
			float fAngle = AngleZ(pPed->GetTransform().GetForward(), RCC_VEC3V(rootMatrix.c)).Getf();
			if (fAngle < -QUARTER_PI)
			{
				clipId = CLIP_DAM_FLOOR_LEFT;
			}
			else if (fAngle > QUARTER_PI)
			{
				clipId = CLIP_DAM_FLOOR_RIGHT;
			}
		}

		clipSetId = pPed->GetPedModelInfo()->GetFullBodyDamageClipSet();

		SetNewTask(rage_new CTaskAnimatedFallback(clipSetId, clipId, 0.0f, 0.0f));
	}
	else
	{
		float fStartPhase = 0.0f;
		// If this task is starting as the result of a migration then use the suggested clip and start phase
		CTaskNMControl* pControlTask = static_cast<CTaskNMControl*>(FindParentTaskOfType(CTaskTypes::TASK_NM_CONTROL));
		if (pControlTask != NULL && (pControlTask->GetFlags() & CTaskNMControl::ALREADY_RUNNING) != 0 && 
			m_suggestedClipSetId != CLIP_SET_ID_INVALID && m_suggestedClipId != CLIP_SET_ID_INVALID)
		{
			clipSetId = m_suggestedClipSetId;
			clipId = m_suggestedClipId;
			fStartPhase = m_suggestedClipPhase;
		}
		else
		{
			CTaskFallOver::eFallDirection dir = CTaskFallOver::kDirFront;
			CTaskFallOver::eFallContext context = CTaskFallOver::kContextShotPistol;
			CTaskFallOver::PickFallAnimation(pPed, context, dir, clipSetId, clipId);
		}

		CTaskFallOver* pNewTask = rage_new CTaskFallOver(clipSetId, clipId, 1.0f, fStartPhase);
		pNewTask->SetRunningLocally(true);

		SetNewTask(pNewTask);
	}
}

CTask::FSM_Return CTaskRageRagdoll::AnimatedFallback_OnUpdate(CPed* pPed)
{
	nmDebugf3("LOCAL: Controlling animated fallback task %s (0x%p) Ped ragdolling : %s\n", GetSubTask() ? GetSubTask()->GetTaskName() : "None", this, pPed->GetUsingRagdoll() ? "true" : "false");

	// disable ped capsule control so the blender can set the velocity directly on the ped
	if (pPed->IsNetworkClone())
	{
		NetworkInterface::UseAnimatedRagdollFallbackBlending(*pPed);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePedCapsuleControl, true);
	}

	if (pPed->GetUsingRagdoll())
	{
		SetState(State_Ragdoll);
	}
	else if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

