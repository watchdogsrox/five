#ifndef WEAPON_TARGET_H
#define WEAPON_TARGET_H

// Base class
#include "AI/AITarget.h"

//////////////////////////////////////////////////////////////////////////
// CWeaponTarget
//////////////////////////////////////////////////////////////////////////

class CWeaponTarget : public CAITarget
{
public:

	// Default constructor
	CWeaponTarget() {}

	// Construct with a target entity
	CWeaponTarget(const CEntity* pTargetEntity) : CAITarget(pTargetEntity) {}

	// Construct with a target entity and a local space offset from that entity
	CWeaponTarget(const CEntity* pTargetEntity, const Vector3& vTargetOffset) : CAITarget(pTargetEntity, vTargetOffset) {}

	// Construct with a world space target position
	CWeaponTarget(const Vector3& vTargetPosition) : CAITarget(vTargetPosition) {}

	// Copy constructor
	CWeaponTarget(const CAITarget& other) : CAITarget(other) {}

	// The purpose of this is to update the target entity/position based on other inputs such as player lockon
	void Update(CPed* pPed);

	// Get the target position in world space
	bool GetPositionWithFiringOffsets(const CPed* pPed, Vector3& vTargetPosition) const;

	static void InitTunables();

private:

	static bool ms_bAimAtHeadForSlowMovingVehicleToggle;
};

#endif // WEAPON_TARGET_H
