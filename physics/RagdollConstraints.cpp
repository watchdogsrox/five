 // 
// physics/RagdollConstraints.cpp
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

// Rage headers
#include "phbound/boundcomposite.h"
#include "physics/constraintrotation.h"
#include "physics/constraintspherical.h"
#include "physics/constraintdistance.h"
#include "physics/constraintfixed.h"

// Game headers
#include "physics/gtaInst.h"
#include "physics\RagdollConstraints.h"
#include "peds\Ped.h"
#include "Peds/PedDefines.h"
#include "peds/peddebugvisualiser.h"

CRagdollConstraintComponent::CRagdollConstraintComponent() :
	m_ShouldCuffHandsWhenRagdolling(false),
	m_ShouldBoundAnklesWhenRagdolling(false)
{

}

CRagdollConstraintComponent::~CRagdollConstraintComponent()
{
	RemoveRagdollConstraints();
}

void CRagdollConstraintComponent::CreateHookToWorldConstraint(CPhysical *hook, const Vector3 &position, bool UNUSED_PARAM(fixRotation), const Vector3 &vecRotMinLimits, const Vector3 &vecRotMaxLimits)
{
	if (hook && hook->GetCurrentPhysicsInst())
	{
		// Disable hook collisions
		hook->GetCurrentPhysicsInst()->SetInstFlag(phInstGta::FLAG_NO_COLLISION, true);

		phConstraintSpherical::Params constraint;
		constraint.instanceA = hook->GetCurrentPhysicsInst();
		constraint.worldPosition = VECTOR3_TO_VEC3V(position);
		m_HookLinHandle = PHCONSTRAINT->Insert(constraint);

		phConstraintRotation::Params rotConstraint;
		rotConstraint.instanceA = hook->GetCurrentPhysicsInst();
		rotConstraint.worldDrawPosition = VECTOR3_TO_VEC3V(position);
		rotConstraint.worldAxis = Vec3V(V_Z_AXIS_WZERO);
		rotConstraint.minLimit = vecRotMinLimits.x;
		rotConstraint.maxLimit = vecRotMaxLimits.x;
		m_RotHandle = PHCONSTRAINT->Insert(rotConstraint);
	}
}

void CRagdollConstraintComponent::UpdateHookToWorldConstraint(const Vector3 &position, const Vector3 &vecRotMinLimits, const Vector3 &vecRotMaxLimits)
{
	// Raise the hook a bit (a hack so that I don't have to wait for script to do it)
	static float raise = 0.2f;
	Vector3 pos = position;
	pos.z += raise;

	if (m_HookLinHandle.IsValid())
	{
		phConstraintSpherical* constraint = static_cast<phConstraintSpherical*>( PHCONSTRAINT->GetTemporaryPointer(m_HookLinHandle) );
		
		if (constraint)//Verifyf(constraint, "hook to world constraint no longer exists in manager"))
			constraint->SetWorldPosB(VECTOR3_TO_VEC3V(pos));
		else
			m_HookLinHandle.Reset();
	}

	if (m_RotHandle.IsValid())
	{
		phConstraintRotation* constraint = static_cast<phConstraintRotation*>( PHCONSTRAINT->GetTemporaryPointer(m_RotHandle) );
		
		if (constraint)//Verifyf(constraint, "hook to world constraint no longer exists in manager"))
		{
			constraint->SetMinLimit(vecRotMinLimits.x);
			constraint->SetMaxLimit(vecRotMaxLimits.x);
		}
		else
			m_RotHandle.Reset();
	}
}

void CRagdollConstraintComponent::EnableHandCuffs(CPed *pPed, bool bEnable, bool bChangeDesiredState)
{
	// Set the desired state
	if (bChangeDesiredState)
		m_ShouldCuffHandsWhenRagdolling = bEnable;

	// If ragdolling, update the constraint immediately.  Otherwise the constraint will be updated automatically
	// when activating and deactivating the ragdoll.
	if (pPed->GetRagdollState() >= RAGDOLL_STATE_ANIM_DRIVEN)
	{
		if (bEnable)
		{
			Assert(pPed->GetRagdollInst()->GetCurrentPhysicsLOD() == fragInst::RAGDOLL_LOD_HIGH);
			if (pPed->GetRagdollInst()->GetCurrentPhysicsLOD() == fragInst::RAGDOLL_LOD_HIGH)
			{
				if( !(m_CuffsConstraintHandle.IsValid() || m_CuffToPelvisConstraintHandle.IsValid()) )
				{
					bool bUseFixedConstraint = true;
	#if __BANK
					bUseFixedConstraint = CPedDebugVisualiserMenu::ms_bFreezeRelativeWristPositions;
	#endif
					if (bUseFixedConstraint)
					{
						phConstraintFixed::Params params;
						params.instanceA = pPed->GetRagdollInst();
						params.instanceB = pPed->GetRagdollInst();
						params.componentA = RAGDOLL_HAND_LEFT;
						params.componentB = RAGDOLL_HAND_RIGHT;
						m_CuffsConstraintHandle = PHCONSTRAINT->Insert(params);

						params.componentB = RAGDOLL_BUTTOCKS;
						m_CuffToPelvisConstraintHandle = PHCONSTRAINT->Insert(params);
					}
					else
					{
						float length = 0.06f;
	#if __BANK
						length = CPedDebugVisualiserMenu::ms_bHandCuffLength;
	#endif
						phConstraintDistance::Params params;
													 params.instanceA = pPed->GetRagdollInst();
													 params.instanceB = pPed->GetRagdollInst();
													 params.componentA = RAGDOLL_HAND_LEFT;
													 params.componentB = RAGDOLL_HAND_RIGHT;
													 params.maxDistance = length;

						phConstraintBase*	  pConstraintBase;
						phConstraintDistance* pConstraintDist;

						m_CuffsConstraintHandle = PHCONSTRAINT->InsertAndReturnTemporaryPointer(params, pConstraintBase);

						if(Verifyf(m_CuffsConstraintHandle.IsValid(), "Failed to allocate ragdoll wrist constraint"))
						{
							pConstraintDist = static_cast<phConstraintDistance*>(pConstraintBase);
							pConstraintDist->SetLocalPosA(Vec3V(V_ZERO));
							pConstraintDist->SetLocalPosB(Vec3V(V_ZERO));
						}
					}
				}
			}
		}
		else 
		{
			if (m_CuffsConstraintHandle.IsValid())
			{
				PHCONSTRAINT->Remove(m_CuffsConstraintHandle);
				m_CuffsConstraintHandle.Reset();
			}
		}
	}
}

void CRagdollConstraintComponent::EnableBoundAnkles(CPed *pPed, bool bEnable, bool bChangeDesiredState)
{
	// Set the desired state
	if (bChangeDesiredState)
		m_ShouldBoundAnklesWhenRagdolling = bEnable;

	// If ragdolling, update the constraint immediately.  Otherwise the constraint will be updated automatically
	// when activating and deactivating the ragdoll.
	if (pPed->GetRagdollState() >= RAGDOLL_STATE_ANIM_DRIVEN)
	{
		if (bEnable)
		{
			Assert(pPed->GetRagdollInst()->GetCurrentPhysicsLOD() == fragInst::RAGDOLL_LOD_HIGH);
			if (pPed->GetRagdollInst()->GetCurrentPhysicsLOD() == fragInst::RAGDOLL_LOD_HIGH)
			{
				if( !m_AnklesConstraintHandle.IsValid() )
				{
					static float length = 0.1f;

					phConstraintDistance::Params params;
												 params.instanceA = pPed->GetRagdollInst();
												 params.instanceB = pPed->GetRagdollInst();
												 params.componentA = RAGDOLL_FOOT_LEFT;
												 params.componentB = RAGDOLL_FOOT_RIGHT;
												 params.maxDistance = length;

					phConstraintBase*	  pConstraintBase;
					phConstraintDistance* pConstraintDist;

					m_AnklesConstraintHandle = PHCONSTRAINT->InsertAndReturnTemporaryPointer(params, pConstraintBase);

					if(Verifyf(m_AnklesConstraintHandle.IsValid(), "Failed to allocate ragdoll ankles constraint"))
					{
						pConstraintDist = static_cast<phConstraintDistance*>(pConstraintBase);
						pConstraintDist->SetLocalPosA(Vec3V(V_ZERO));
						pConstraintDist->SetLocalPosB(Vec3V(V_ZERO));
					}
				}
			}
		}
		else 
		{
			if (m_AnklesConstraintHandle.IsValid())
			{
				PHCONSTRAINT->Remove(m_AnklesConstraintHandle);
				m_AnklesConstraintHandle.Reset();
			}
		}
	}
}

void CRagdollConstraintComponent::LoosenAnkleBounds()
{
	if (m_AnklesConstraintHandle.IsValid())
	{
		static float length = 0.4f;

		phConstraintDistance* pConstraint = static_cast<phConstraintDistance*>( PHCONSTRAINT->GetTemporaryPointer( m_AnklesConstraintHandle ) );

		if( Verifyf(pConstraint, "ankle constraint handle valid but constraint doesn't exist in the constraint manager") )
		{
			pConstraint->SetMaxDistance( length );
		}
		else
		{
			m_AnklesConstraintHandle.Reset();
		}
	}
}

void CRagdollConstraintComponent::InsertEnabledConstraints(CPed* thisPed)
{
	if(HandCuffsAreEnabled())
		EnableHandCuffs(thisPed, true, false);

	if(BoundAnklesAreEnabled())
		EnableBoundAnkles(thisPed, true, false);
}

void CRagdollConstraintComponent::RemoveRagdollConstraints()
{
	if (m_RotHandle.IsValid())
	{
		PHCONSTRAINT->Remove(m_RotHandle);
		m_RotHandle.Reset();
	}
	if (m_HookLinHandle.IsValid())
	{
		PHCONSTRAINT->Remove(m_HookLinHandle);
		m_HookLinHandle.Reset();
	}
	if (m_AnklesConstraintHandle.IsValid())
	{
		PHCONSTRAINT->Remove(m_AnklesConstraintHandle);
		m_AnklesConstraintHandle.Reset();
	}
	if (m_CuffsConstraintHandle.IsValid())
	{
		PHCONSTRAINT->Remove(m_CuffsConstraintHandle);
		m_CuffsConstraintHandle.Reset();
	}
	if (m_CuffToPelvisConstraintHandle.IsValid())
	{
		PHCONSTRAINT->Remove(m_CuffToPelvisConstraintHandle);
		m_CuffToPelvisConstraintHandle.Reset();
	}
}
