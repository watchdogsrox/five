// Filename   :	TaskNMDraggingToSafety.cpp
// Description:	An NM behavior for a ped being dragged to safety


// --- Include Files ------------------------------------------------------------

// Rage headers
#include "pharticulated\articulatedcollider.h"
#include "phbound/boundcomposite.h"
#include "physics/constraintdistance.h"
#include "physics/constraintfixed.h"

// Game headers
#include "Peds/ped.h"
#include "physics/physics.h"
#include "Task/Physics/TaskNMDraggingToSafety.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CTaskNMDraggingToSafety
////////////////////////////////////////////////////////////////////////////////

CTaskNMDraggingToSafety::Tunables CTaskNMDraggingToSafety::sm_Tunables;
IMPLEMENT_PHYSICS_TASK_TUNABLES(CTaskNMDraggingToSafety, 0xac5ae193);

CTaskNMDraggingToSafety::CTaskNMDraggingToSafety(CPed* pDragged, CPed* pDragger)
: CTaskNMBehaviour(1000, 50000)
, m_pDragged(pDragged)
, m_pDragger(pDragger)
, m_HandleL()
, m_HandleR()
{
	SetInternalTaskType(CTaskTypes::TASK_NM_DRAGGING_TO_SAFETY);
}

CTaskNMDraggingToSafety::~CTaskNMDraggingToSafety()
{
}

void CTaskNMDraggingToSafety::CleanUp()
{
	//Reset the constraints.
	ResetConstraint(m_HandleL);
	ResetConstraint(m_HandleR);
}

CTask::FSM_Return CTaskNMDraggingToSafety::ProcessPreFSM()
{
	//Ensure the dragged ped is valid.
	if(!GetDragged())
	{
		return FSM_Quit;
	}
	
	//Ensure the dragger ped is valid.
	if(!GetDragger())
	{
		return FSM_Quit;
	}
	
	return CTaskNMBehaviour::ProcessPreFSM();
}

bool CTaskNMDraggingToSafety::ProcessPhysics(float fTimeStep, int UNUSED_PARAM(nTimeSlice))
{
	if (sm_Tunables.m_Forces.m_Enabled)
	{
		phBoundComposite *boundDragger = GetDragger()->GetRagdollInst()->GetCacheEntry()->GetBound();
		Matrix34 leftHandMat, rightHandMat;
		leftHandMat.Dot(RCC_MATRIX34(boundDragger->GetCurrentMatrix(RAGDOLL_HAND_LEFT)), RCC_MATRIX34(GetDragger()->GetRagdollInst()->GetMatrix()));
		rightHandMat.Dot(RCC_MATRIX34(boundDragger->GetCurrentMatrix(RAGDOLL_HAND_RIGHT)), RCC_MATRIX34(GetDragger()->GetRagdollInst()->GetMatrix()));
		phBoundComposite *boundDragged = GetDragged()->GetRagdollInst()->GetCacheEntry()->GetBound();
		Matrix34 leftClavicle, rightClavicle;
		leftClavicle.Dot(RCC_MATRIX34(boundDragged->GetCurrentMatrix(RAGDOLL_CLAVICLE_LEFT)), RCC_MATRIX34(GetDragged()->GetRagdollInst()->GetMatrix()));
		rightClavicle.Dot(RCC_MATRIX34(boundDragged->GetCurrentMatrix(RAGDOLL_CLAVICLE_RIGHT)), RCC_MATRIX34(GetDragged()->GetRagdollInst()->GetMatrix()));

		// offset the hand positions
		leftHandMat.d = leftHandMat.d + 
			leftHandMat.a * sm_Tunables.m_Forces.m_LeftHandOffset.x + 
			leftHandMat.b * sm_Tunables.m_Forces.m_LeftHandOffset.y + 
			leftHandMat.c * sm_Tunables.m_Forces.m_LeftHandOffset.z;
		rightHandMat.d = rightHandMat.d + 
			rightHandMat.a * sm_Tunables.m_Forces.m_RightHandOffset.x + 
			rightHandMat.b * sm_Tunables.m_Forces.m_RightHandOffset.y + 
			rightHandMat.c * sm_Tunables.m_Forces.m_RightHandOffset.z;

		static float fImpulseMult = 0.0f;
		Vector3 leftImpulse = (leftHandMat.d-leftClavicle.d) * fImpulseMult * fTimeStep;
		Vector3 rightImpulse = (rightHandMat.d-rightClavicle.d) * fImpulseMult * fTimeStep;
		// Vector3 upImpulse = Vector3(0.0f,0.0f,1.0f) * fImpulseMult * fTimeStep;
		GetDragged()->ApplyImpulse(leftImpulse, VEC3_ZERO, RAGDOLL_CLAVICLE_LEFT, true);
		GetDragged()->ApplyImpulse(rightImpulse, VEC3_ZERO, RAGDOLL_CLAVICLE_RIGHT, true);
	}

	return true;
}

CTaskInfo* CTaskNMDraggingToSafety::CreateQueriableState() const 
{ 
	// this task is not networked yet
	Assert(!NetworkInterface::IsGameInProgress()); 
	return rage_new CTaskInfo(); 
}

void CTaskNMDraggingToSafety::StartBehaviour(CPed* pPed)
{
	//Create the constraints.
	CreateConstraint(m_HandleL, RAGDOLL_CLAVICLE_LEFT,  RAGDOLL_HAND_LEFT);
	CreateConstraint(m_HandleR, RAGDOLL_CLAVICLE_RIGHT, RAGDOLL_HAND_RIGHT);

	//Turn off joint driving.
	((phArticulatedCollider*)pPed->GetCollider())->GetBody()->SetDriveState(phJoint::DRIVE_STATE_FREE);
	
	//Set a minimum stiffness on the neck/head to increase stability.
	((phArticulatedCollider*)pPed->GetCollider())->GetBody()->GetJoint(RAGDOLL_HEAD_JOINT).SetMinStiffness(sm_Tunables.m_Stiffness.m_HeadAndNeck);
	((phArticulatedCollider*)pPed->GetCollider())->GetBody()->GetJoint(RAGDOLL_NECK_JOINT).SetMinStiffness(sm_Tunables.m_Stiffness.m_HeadAndNeck);

	//Set a minimum stiffness on the ankles to increase stability.
	((phArticulatedCollider*)pPed->GetCollider())->GetBody()->GetJoint(RAGDOLL_ANKLE_LEFT_JOINT).SetMinStiffness(sm_Tunables.m_Stiffness.m_AnkleAndWrist);
	((phArticulatedCollider*)pPed->GetCollider())->GetBody()->GetJoint(RAGDOLL_ANKLE_RIGHT_JOINT).SetMinStiffness(sm_Tunables.m_Stiffness.m_AnkleAndWrist);
	((phArticulatedCollider*)pPed->GetCollider())->GetBody()->GetJoint(RAGDOLL_WRIST_LEFT_JOINT).SetMinStiffness(sm_Tunables.m_Stiffness.m_AnkleAndWrist);
	((phArticulatedCollider*)pPed->GetCollider())->GetBody()->GetJoint(RAGDOLL_WRIST_RIGHT_JOINT).SetMinStiffness(sm_Tunables.m_Stiffness.m_AnkleAndWrist);
	
	//Create the NM message.
	ART::MessageParams msg;

	//Begin NM.
	msg.addBool(NMSTR_PARAM(NM_START), true);

	//Hold the current pose (as opposed to moving to t-pose).
	msg.addBool(NMSTR_PARAM(NM_RELAX_HOLD_POSE), true);

	//Set the relaxation parameter.
	msg.addFloat(NMSTR_PARAM(NM_RELAX_RELAXATION), rage::Max(10.0f, 100.0f - sm_Tunables.m_Stiffness.m_Relaxation));

	//Post the NM message.
	pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_RELAX_MSG), &msg);
}

void CTaskNMDraggingToSafety::ControlBehaviour(CPed* UNUSED_PARAM(pPed))
{
	//Grab the dragger ped.
	CPed* pDragger = GetDragger();
	
	//Update the ragdoll bounds.
	pDragger->UpdateRagdollBoundsFromAnimatedSkel();
	
	//Control the dragger arm ik.
	ControlDraggerArmIk();
}

bool CTaskNMDraggingToSafety::FinishConditions(CPed* pPed)
{
	static float fallingSpeed = -4.0f;
	if (pPed->GetVelocity().z < fallingSpeed)
	{
		m_bHasFailed = true;
		m_nSuggestedNextTask = CTaskTypes::TASK_NM_HIGH_FALL;
	}

	return false;
}

bool CTaskNMDraggingToSafety::CalculateBonePositionForPed(const CPed& rPed, eAnimBoneTag nBoneTag, Vector3& vOffset, Vector3& vPosition) const
{
	//Get the bone matrix.
	Matrix34 mBone;
	if(!rPed.GetBoneMatrix(mBone, nBoneTag))
	{
		return false;
	}
	
	//Calculate the position.
	vPosition = vOffset;
	mBone.Transform(vPosition);
	
	return true;
}

bool CTaskNMDraggingToSafety::CalculateLeftHandPositionForDraggerArmIk(Vector3& vPosition) const
{
	return CalculateBonePositionForPed(*GetDragged(), BONETAG_L_CLAVICLE, sm_Tunables.m_DraggerArmIk.m_LeftBoneOffset, vPosition);
}

bool CTaskNMDraggingToSafety::CalculateRightHandPositionForDraggerArmIk(Vector3& vPosition) const
{
	return CalculateBonePositionForPed(*GetDragged(), BONETAG_R_CLAVICLE, sm_Tunables.m_DraggerArmIk.m_RightBoneOffset, vPosition);
}

void CTaskNMDraggingToSafety::ControlDraggerArmIk()
{
	//Ensure the dragger arm ik is enabled.
	if(!sm_Tunables.m_DraggerArmIk.m_Enabled)
	{
		return;
	}
	
	//Grab the dragger.
	CPed* pDragger = GetDragger();
	
	//Calculate the left hand position.
	Vector3 vLeftHandPosition;
	if(CalculateLeftHandPositionForDraggerArmIk(vLeftHandPosition))
	{
		//Set the left hand position.
		pDragger->GetIkManager().SetAbsoluteArmIKTarget(crIKSolverArms::LEFT_ARM, vLeftHandPosition, AIK_TARGET_WRT_POINTHELPER);
	}

	//Calculate the right hand position.
	Vector3 vRightHandPosition;
	if(CalculateRightHandPositionForDraggerArmIk(vRightHandPosition))
	{
		//Set the right hand position.
		pDragger->GetIkManager().SetAbsoluteArmIKTarget(crIKSolverArms::RIGHT_ARM, vRightHandPosition, AIK_TARGET_WRT_POINTHELPER);
	}
}

void CTaskNMDraggingToSafety::CreateConstraint(phConstraintHandle& rHandle, const u16 uDraggedComponent, const u16 uDraggerComponent)
{
	//Build up the constraint parameters.
	phConstraintDistance::Params constraint;
	constraint.instanceA = GetDragged()->GetRagdollInst();
	constraint.instanceB = GetDragger()->GetRagdollInst();
	constraint.componentA = uDraggedComponent;
	constraint.componentB = uDraggerComponent;
	constraint.constructUsingLocalPositions = true;
	constraint.maxDistance = sm_Tunables.m_Constraints.m_MaxDistance;
	
	//Insert the constraint.
	Assert(!rHandle.IsValid());
	rHandle = PHCONSTRAINT->Insert(constraint);
	
	//Ensure the handle is valid.
	if(!rHandle.IsValid())
	{
		return;
	}
	
	//Ensure the constraint is valid.
	phConstraintBase* pConstraint = PHCONSTRAINT->GetTemporaryPointer(rHandle);
	if(!pConstraint)
	{
		return;
	}
	
	//Don't allow the constraint to activate the dragger.
	pConstraint->SetAllowForceActivation(false);
}

void CTaskNMDraggingToSafety::ResetConstraint(phConstraintHandle& rHandle)
{
	//Ensure the handle is valid.
	if(!rHandle.IsValid())
	{
		return;
	}
	
	//Remove the handle.
	CPhysics::GetSimulator()->GetConstraintMgr()->Remove(rHandle);
	
	//Reset the handle.
	rHandle.Reset();
}
