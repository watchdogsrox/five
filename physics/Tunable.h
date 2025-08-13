//
// File: Tunable.h
// Started: 28/8/12
// Creator: Stefan Bachman
// Purpose: Handle loading metadata for tuning data of simple objects that aren't fragments
//
#ifndef TUNABLE_H_
#define TUNABLE_H_

// RAGE headers:
#include "atl/binmap.h"
#include "atl/string.h"
#include "atl/hashstring.h"
#include "parser/macros.h"
#include "vector/vector3.h"

//////////////////////////////////////////////////////////
// Stores the tuning information for a single object
//////////////////////////////////////////////////////////
class CTunableObjectEntry
{
public:
	// Accessor functions:
	u32 GetObjectNameHash() const { return m_objectName.GetHash(); }
	float GetMass() const { return m_objectMass; }
	const rage::Vector3& GetLinearSpeedDamping() const { return m_objectLinearSpeedDamping; }
	const rage::Vector3& GetLinearVelocityDamping() const { return m_objectLinearVelocityDamping; }
	const rage::Vector3& GetLinearVelocitySquaredDamping() const { return m_objectLinearVelocitySquaredDamping; }
	const rage::Vector3& GetAngularSpeedDamping() const { return m_objectAngularSpeedDamping; }
	const rage::Vector3& GetAngularVelocityDamping() const { return m_objectAngularVelocityDamping; }
	const rage::Vector3& GetAngularVelocitySquaredDamping() const { return m_objectAngularVelocitySquaredDamping; }
	const rage::Vector3& GetCentreOfMassOffset() const { return m_objectCentreOfMassOffset; }
	float GetForkliftAttachOffsetZ() const { return m_forkliftAttachOffsetZ; }
	const rage::Vector3& GetForkliftAttachAngledAreaDims() const { return m_forkliftAttachAngledAreaDims; }
	const rage::Vector3& GetForkliftAttachAngledAreaOrigin() const { return m_forkliftAttachAngledAreaOrigin; }
	bool CanBePickedUpByForklift() const { return m_forkliftAttachAngledAreaDims.IsNonZero(); }
	bool ThisFragCanBePickedUpByForkliftWhenBroken() const { return m_forkliftCanAttachWhenFragBroken; }
	bool IgnoreCollisionInLowLodBuoyancyMode() const { return m_lowLodBuoyancyNoCollision; }
	float GetLowLodBuoyancyDistance() const { return m_lowLodBuoyancyDistance; }
	float GetLowLodBuoyancyHeightOffset() const { return m_lowLodBuoyancyHeightOffset; }
	float GetBuoyancyFactor() const { return m_buoyancyFactor; }
	float GetBuoyancyDragFactor() const { return m_buoyancyDragFactor; }
	int GetBoundIndexForWaterSamples() const { return m_boundIndexForWaterSamples; }
	float GetMaxHealth() const { return m_maxHealth; }
	float GetKnockOffBikeImpulseScalar() const { return m_knockOffBikeImpulseScalar; }
	int GetGlassFrameFlags() const { return m_glassFrameFlags; }
	bool GetCanBePickedUpByCargobob() const {return m_bCanBePickedUpByCargobob;}
	bool GetIsUnreachableByMeleeNavigation() const {return m_bUnreachableByMeleeNavigation;}
	bool GetNotDamagedByFlames() const {return m_notDamagedByFlames;}
	bool GetCollidesWithLowLodVehicleChassis() const {return m_collidesWithLowLodVehicleChassis;}
	bool GetUsePositionForLockOnTarget() const {return m_usePositionForLockOnTarget;}

private:
	// Name of this object.
	atHashString m_objectName;
	// Mass parameter for this object.
	float m_objectMass;
	rage::Vector3 m_objectLinearSpeedDamping;
	rage::Vector3 m_objectLinearVelocityDamping;
	rage::Vector3 m_objectLinearVelocitySquaredDamping;
	rage::Vector3 m_objectAngularSpeedDamping;
	rage::Vector3 m_objectAngularVelocityDamping;
	rage::Vector3 m_objectAngularVelocitySquaredDamping;
	rage::Vector3 m_objectCentreOfMassOffset;

	float m_forkliftAttachOffsetZ;
	// Define a box which we will use to determine if this prop can attach to the forklift forks.
	Vector3 m_forkliftAttachAngledAreaDims; // (width, length, height)
	Vector3 m_forkliftAttachAngledAreaOrigin;
	bool m_forkliftCanAttachWhenFragBroken;

	// Low LOD buoyancy overrides.
	float m_lowLodBuoyancyDistance;
	float m_lowLodBuoyancyHeightOffset;
	// Scale the buoyancy force.
	float m_buoyancyFactor;
	// Allow objects to override the default drag coefficient in water.
	float m_buoyancyDragFactor;
	// Allow overriding of the bound chosen for props when generating water samples (it defaults to the largest by volume for non-frags).
	int m_boundIndexForWaterSamples;
	// Some small objects like the spherical buoys look bad when AI peds collide with them in low LOD buoyancy mode. Allow these kinds
	// of objects to ignore collisions in low LOD mode.
	bool m_lowLodBuoyancyNoCollision;

	// Initial CPhysical health for the object
	float m_maxHealth;

	// Some objects are inconsistent in the way they cause bike riders to be dismounted. Allow the impulse they impart to be scaled.
	float m_knockOffBikeImpulseScalar;

	// Glass frame related flags
	// Use the values in bgBreakable::eFrameFlags to mark the edges that exist (15 for all)
	int m_glassFrameFlags;

	bool m_bCanBePickedUpByCargobob;

	// Used to tag specific objects that we would like to denote as an reachable object for AI navigation
	bool m_bUnreachableByMeleeNavigation;

	// B*1787367: Allow objects to be flagged to not take damage from fire.
	bool m_notDamagedByFlames;

	// B*1918379: Allow objects to be set to collide with the low-LOD version of a vehicle's chassis.
	// Used for things like fence props to stop them intersecting and getting stuck in vehicles with open-topped, concave chassis bounds.
	// Objects with this set will only collide with the low-LOD chassis if the vehicle it collides with has been set up to allow this. 
	// By default, vehicles do not allow this and it should be enabled on a case-by-case basis.
	bool m_collidesWithLowLodVehicleChassis;

	// B*3992049: By default, objects use bounds centre for the lockon target position.
	// In the case of some objects (i.e. floating mines on chains), we need to lock the root position instead so we target correctly.
	bool m_usePositionForLockOnTarget;

	PAR_SIMPLE_PARSABLE;
};

///////////////////////////////////
// Definition of the manager class
///////////////////////////////////
class CTunableObjectManager
{
public:
	static void Load(const char* fileName);

	// Functions to get information about the tunable objects.
	static CTunableObjectManager& GetInstance() { return m_instance; }
	const CTunableObjectEntry* GetTuningEntry(atHashWithStringNotFinal hName) const;

	// Functions to assist in debugging and tuning the tunable objects.
#if __BANK
	static void Reload();
	void AddWidgetsToBank(bkBank& bank);
#endif // __BANK

private:
	// List of all objects which are tunable.
	atBinaryMap<CTunableObjectEntry, atHashWithStringNotFinal> m_TunableObjects;


	// The singleton instance.
	static CTunableObjectManager m_instance;

	PAR_SIMPLE_PARSABLE;
};

#endif // TUNABLE_H_
