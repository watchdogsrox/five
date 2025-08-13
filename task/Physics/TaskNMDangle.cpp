// Filename   :	TaskNMDangle.cpp
// Description:	Natural Motion dangle class (FSM version)


// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "crskeleton\Skeleton.h"
#include "fragment\Cache.h"
#include "fragment\Instance.h"
#include "fragment\Type.h"
#include "fragment\TypeChild.h"
#include "fragmentnm\messageparams.h"
#include "pharticulated/articulatedcollider.h"
#include "physics/shapetest.h"
#include "Physics/constrainthinge.h"
#include "physics/simulator.h"
#include "phbound/boundcomposite.h"

// Framework headers
#include "grcore/debugdraw.h"

// Game headers
#include "Peds\Ped.h"
#include "Task/Physics/NmDebug.h"
#include "Task/Physics/TaskNMDangle.h"
#include "Physics/physics.h"
#include "physics/RagdollConstraints.h"

AI_OPTIMISATIONS()

CTaskNMDangle::CTaskNMDangle(const Vector3 &currentHookPos)
:	CTaskNMBehaviour(30000, 30000),
m_bDoGrab(false),
m_FirstPhysicsFrame(false),
m_fFrequency(1.0f)
{
	m_RotConstraintHandle.Reset();
	m_CurrentHookPos = currentHookPos;
	m_LastHookPos = m_CurrentHookPos;
	m_MinAngles.Zero();
	m_MaxAngles.Zero();
	SetInternalTaskType(CTaskTypes::TASK_NM_DANGLE);
}

CTaskNMDangle::~CTaskNMDangle()
{
	if (m_RotConstraintHandle.IsValid())
	{
		PHCONSTRAINT->Remove(m_RotConstraintHandle);
		m_RotConstraintHandle.Reset();
	}
}

CTaskInfo* CTaskNMDangle::CreateQueriableState() const 
{ 
	 // this task is not networked yet
	Assert(!NetworkInterface::IsGameInProgress()); 
	return rage_new CTaskInfo(); 
}

void CTaskNMDangle::SetGrabParams(CPed* pPed, bool bDoGrab, float fFrequency)
{
	m_bDoGrab = bDoGrab;
	m_fFrequency = fFrequency;

	ART::MessageParams msg;
	msg.addBool(NMSTR_PARAM(NM_DANGLE_DO_GRAB), m_bDoGrab);
	msg.addFloat(NMSTR_PARAM(NM_DANGLE_GRAB_FREQUENCY), m_fFrequency);
	pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_DANGLE_MSG), &msg);
}

void CTaskNMDangle::StartBehaviour(CPed* pPed)
{
	ART::MessageParams msg;
	msg.addBool(NMSTR_PARAM(NM_START), true);
	msg.addBool(NMSTR_PARAM(NM_DANGLE_DO_GRAB), m_bDoGrab);
	msg.addFloat(NMSTR_PARAM(NM_DANGLE_GRAB_FREQUENCY), m_fFrequency);
	pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_DANGLE_MSG), &msg);
}
void CTaskNMDangle::ControlBehaviour(CPed* pPed)
{
	CTaskNMBehaviour::ControlBehaviour(pPed);

	// Automatically apply a rotation constraint to help with ragdoll instability caused by the rage 'looseness' bug
	if (m_bDoGrab && !m_RotConstraintHandle.IsValid() && fwTimer::GetTimeInMilliseconds() > m_nStartTime + 4000.0f)
	{
		phConstraintHinge::Params constraint;
		constraint.instanceA = pPed->GetRagdollInst();
		static u16 comp = 3;
		constraint.componentA = comp;
		Vec3V pos = pPed->GetTransform().GetPosition();
		constraint.worldAnchor = pos;
		constraint.worldAxis = Vec3V(V_Z_AXIS_WZERO);
		static float min = -0.7f;
		static float max = 0.7f;
		constraint.minLimit = min;
		constraint.maxLimit = max;
		m_RotConstraintHandle = PHCONSTRAINT->Insert(constraint);
	}
	else if (!m_bDoGrab && m_RotConstraintHandle.IsValid())
	{
		if (m_RotConstraintHandle.IsValid())
		{
			PHCONSTRAINT->Remove(m_RotConstraintHandle);
			m_RotConstraintHandle.Reset();
		}
	}

	// Set the ankle bounds to be looser, since both feet are already getting constrained
	if (pPed->GetRagdollConstraintData() && pPed->GetRagdollConstraintData()->BoundAnklesAreEnabled())
	{
		pPed->GetRagdollConstraintData()->LoosenAnkleBounds();
	}

	// reset the physics frame indicator
	m_FirstPhysicsFrame = true;
}

void CTaskNMDangle::UpdateHookConstraint(const Vector3& position, const Vector3& vecRotMinLimits, const Vector3& vecRotMaxLimits)
{
	m_LastHookPos = m_CurrentHookPos;
	m_CurrentHookPos = position;
	m_MinAngles = vecRotMinLimits;
	m_MaxAngles = vecRotMaxLimits;
}

bool CTaskNMDangle::ProcessPhysics( float fTimeStep, int UNUSED_PARAM(nTimeSlice) )
{
	// Update the hook to world constraint
	if (GetPed()->GetRagdollConstraintData())
	{
		if (m_FirstPhysicsFrame && m_CurrentHookPos.Mag2() > 0.1f)
		{
			GetPed()->GetRagdollConstraintData()->UpdateHookToWorldConstraint(m_CurrentHookPos, m_MinAngles, m_MaxAngles);
		}
		else if (m_CurrentHookPos.Mag2() > 0.1f && m_LastHookPos.Mag2() > 0.1f)
		{
			// extrapolate based on the last and current hook position
			Vector3 nextPos = m_CurrentHookPos + (m_CurrentHookPos - m_LastHookPos) / 2.0f;
			GetPed()->GetRagdollConstraintData()->UpdateHookToWorldConstraint(nextPos, m_MinAngles, m_MaxAngles);

			// Add forces to reduce swinging
			static float forceMult1 = 2000.0f;
			static float forceMult2 = 2000.0f;
			static float forceMult3 = 2000.0f;
			static float forceMult4 = 2000.0f;
			phBoundComposite *bound = GetPed()->GetRagdollInst()->GetCacheEntry()->GetBound();
			phArticulatedCollider *collider = GetPed()->GetRagdollInst()->GetCacheEntry()->GetHierInst()->articulatedCollider;
			Matrix34 partMat;
			partMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(RAGDOLL_BUTTOCKS)), RCC_MATRIX34(GetPed()->GetRagdollInst()->GetMatrix())); 
			Vector3 toCenter = nextPos - partMat.d;
			toCenter.z = 0.0f;
			collider->GetBody()->ApplyForce(RAGDOLL_BUTTOCKS, toCenter * forceMult1, partMat.d - RCC_MATRIX34(collider->GetMatrix()).d, ScalarV(fTimeStep).GetIntrin128ConstRef());
			partMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(RAGDOLL_SPINE2)), RCC_MATRIX34(GetPed()->GetRagdollInst()->GetMatrix())); 
			toCenter = nextPos - partMat.d;
			toCenter.z = 0.0f;
			collider->GetBody()->ApplyForce(RAGDOLL_SPINE2, toCenter * forceMult2, partMat.d - RCC_MATRIX34(collider->GetMatrix()).d, ScalarV(fTimeStep).GetIntrin128ConstRef());
			partMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(RAGDOLL_SPINE3)), RCC_MATRIX34(GetPed()->GetRagdollInst()->GetMatrix())); 
			toCenter = nextPos - partMat.d;
			toCenter.z = 0.0f;
			collider->GetBody()->ApplyForce(RAGDOLL_SPINE3, toCenter * forceMult3, partMat.d - RCC_MATRIX34(collider->GetMatrix()).d, ScalarV(fTimeStep).GetIntrin128ConstRef());
			partMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(RAGDOLL_FOOT_LEFT)), RCC_MATRIX34(GetPed()->GetRagdollInst()->GetMatrix())); 
			toCenter = nextPos - partMat.d;
			toCenter.z = 0.0f;
			collider->GetBody()->ApplyForce(RAGDOLL_FOOT_LEFT, toCenter * forceMult4, partMat.d - RCC_MATRIX34(collider->GetMatrix()).d, ScalarV(fTimeStep).GetIntrin128ConstRef());
			partMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(RAGDOLL_FOOT_RIGHT)), RCC_MATRIX34(GetPed()->GetRagdollInst()->GetMatrix())); 
			toCenter = nextPos - partMat.d;
			toCenter.z = 0.0f;
			collider->GetBody()->ApplyForce(RAGDOLL_FOOT_RIGHT, toCenter * forceMult4, partMat.d - RCC_MATRIX34(collider->GetMatrix()).d, ScalarV(fTimeStep).GetIntrin128ConstRef());
		}
	}

	// reset the physics frame indicator
	m_FirstPhysicsFrame = false;

	return true;
}

bool CTaskNMDangle::FinishConditions(CPed* pPed)
{
	if (pPed->IsDead())
		return true;

	if (pPed->GetRagdollConstraintData() && !pPed->GetRagdollConstraintData()->BoundAnklesAreEnabled())
	{
		m_nSuggestedNextTask = CTaskTypes::TASK_NM_BALANCE;
		return true;
	}

	return false;
}
