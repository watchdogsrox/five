// 
// physics/RagdollConstraints.h
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

#ifndef RAGDOLL_CONSTRAINT_H
#define RAGDOLL_CONSTRAINT_H

#include "physics\physics.h"

class CPed;

class CRagdollConstraintComponent
{
	friend class CPed;
	friend class CPhysical;

public:
	CRagdollConstraintComponent();
	~CRagdollConstraintComponent();

	// Deals with binding wrists  and ankles together when in a ragdoll state
	void EnableHandCuffs(CPed *pPed, bool bEnable, bool bChangeDesiredState = true);
	bool HandCuffsAreEnabled() { return m_ShouldCuffHandsWhenRagdolling; }
	void EnableBoundAnkles(CPed *pPed, bool bEnable, bool bChangeDesiredState = true);
	bool BoundAnklesAreEnabled() { return m_ShouldBoundAnklesWhenRagdolling; }
	void LoosenAnkleBounds();

	// Creates a special hook to world constraint for the dangle from meathook scene
	void CreateHookToWorldConstraint(CPhysical *hook, const Vector3 &position, bool fixRotation, const Vector3 &vecRotMinLimits, const Vector3 &vecRotMaxLimits);
	void UpdateHookToWorldConstraint(const Vector3 &position, const Vector3 &vecRotMinLimits, const Vector3 &vecRotMaxLimits);

	void InsertEnabledConstraints(CPed* thisPed);
	void RemoveRagdollConstraints();

protected:

	// Indicates whether this Ped's hands should be cuffed when in a ragdoll state
	bool m_ShouldCuffHandsWhenRagdolling;
	bool m_ShouldBoundAnklesWhenRagdolling;

	phConstraintHandle m_CuffsConstraintHandle;
	phConstraintHandle m_CuffToPelvisConstraintHandle;
	phConstraintHandle m_AnklesConstraintHandle;

	// Handles a hook-world constraint when dangling from a meathook 
	phConstraintHandle m_HookLinHandle;
	phConstraintHandle m_RotHandle;

};

#endif