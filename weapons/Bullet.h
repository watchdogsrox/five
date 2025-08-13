#ifndef BULLET_H
#define BULLET_H

// Rage headers
#include "physics/intersection.h"

// Game headers
#include "physics/WorldProbe/worldprobe.h"
#include "Scene/RegdRefTypes.h"
#include "Weapons/Info/WeaponInfo.h"

// Forward declarations
namespace rage { class phPolygon; }

////////////////////////////////////////////////////////////////////////////////

class CBullet
{
public:
	FW_REGISTER_CLASS_POOL(CBullet);

	friend class CBulletManager;

	// Statics
	static const s32 MAX_STORAGE = 60;
	static const s32 MAX_BULLETS_IN_BATCH = 16;
	static const s32 MAX_BULLETS_COLLISION_PAIRS = 10;

#if __BANK
	static bool ms_CollideWithMoverCollision;
#endif // __BANK

	CBullet();
	CBullet(CEntity* pFiringEntity, const CWeaponInfo* pWeaponInfo, const Vector3& vStart, const Vector3& vEnd, const f32 fVelocity, const f32 fLifeTime, const f32 fDamage, u32 uWeaponHash, bool bCreatesTrace, bool bAllowDamageToVehicle, bool bAllowDamageToVehicleOccupants, const Vector3& vMuzzlePosition, const Vector3& vMuzzleDirection, bool bIsAccurate, bool bPerformPenetration, bool bPerformBulletProofGlassPenetration, float fRecoilAccuracyWhenFired, bool bSilenced, float fFallOffRangeModifer, float fFallOffDamageModifer, bool bIsBentBulletFromAimAssist, CWeapon* pWeaponOwner = NULL);
	~CBullet();

	// Cloud tunables
	static void InitTunables();

	// PURPOSE: Initialise class
	static void Init();

	// PURPOSE: Shutdown class
	static void Shutdown();

	// PURPOSE: Simulates the bullet
	// RETURNS: true - carry on processing, false - delete from manager
	bool Process();

	// PURPOSE: Add an extra probe to this bullet - simulates shotguns
	bool Add(const Vector3& vStart, const Vector3& vEnd, const f32 fVelocity, u32 uWeaponHash, bool bCreatesTrace, bool bIsAccurate);

	// PURPOSE: Return if there is a bullet fired in an angled area
	bool ComputeIsBulletInAngledArea(Vector3& vPos1, Vector3& vPos2, float areaWidth, const CEntity* pFiringEntity);

	// PURPOSE: Return if there is a bullet fired in the specified sphere
	bool ComputeIsBulletInSphere(const Vector3& vPosition, f32 fRadius, const CEntity* pFiringEntity = NULL) const;

	// PURPOSE: Return if there is a bullet fired in the specified box
	bool ComputeIsBulletInBox(const Vector3& vMin, const Vector3& vMax, const CEntity* pFiringEntity = NULL) const;

#if __BANK
	// PURPOSE: Debug
	void RenderDebug() const;
#endif // __BANK

private:

	//
	// Internal structures
	//

	// PURPOSE: Individual bullet data
	struct sBulletInstance
	{
		FW_REGISTER_CLASS_POOL(sBulletInstance);

		// Statics
		static const s32 MAX_STORAGE = 128;

		sBulletInstance(const CWeaponInfo* pWeaponInfo, const Vector3& vStart, const Vector3& vEnd, const f32 fVelocity, u32 uWeaponHash, bool bCreatesTrace, bool bIsAccurate);

		// PURPOSE: Current bullet position
		Vector3 vPosition;

		// PURPOSE: Last bullet position
		Vector3 vOldPosition;

		// PURPOSE: Bullet velocity
		Vector3 vVelocity;

		// PURPOSE: Current penetration
		f32 fPenetration;

		// PURPOSE: The hash of the weapon that created the bullet
		u32 uWeaponHash;

		// PURPOSE: Should be removed
		bool bToBeRemoved : 1;

		// PURPOSE: Should this bullet create trace vfx
		bool bCreatesTrace : 1;

		// PURPOSE: Allow head shots/engine fire
		bool bIsAccurate : 1;

#if __BANK
		// PURPOSE: Used to keep track of bullets
		u32 uGenerationId;

		// PURPOSE: Used to allocate new generation id
		static u32 s_uNewGenerationId;
#endif // __BANK
	};

	// PURPOSE: Individual bullet impacts
	struct sBulletImpacts
	{
		// Statics
		static const s32 MAX_IMPACTS = 8;

		sBulletImpacts() : entryImpacts(entryImpactResults, MAX_IMPACTS), exitImpacts(exitImpactResults, MAX_IMPACTS) {}

		typedef WorldProbe::CShapeTestResults Impacts;
		WorldProbe::CShapeTestHitPoint entryImpactResults[MAX_IMPACTS];
		WorldProbe::CShapeTestHitPoint exitImpactResults[MAX_IMPACTS];
		Impacts entryImpacts;
		Impacts exitImpacts;
	};

	// PURPOSE: Batch impacts
	struct sBatchImpacts
	{
		sBatchImpacts() : batch(MAX_BULLETS_IN_BATCH) {}

		typedef atFixedArray<sBulletImpacts, MAX_BULLETS_IN_BATCH> BatchImpacts;
		BatchImpacts batch;
	};

	struct BulletCollisionPair
	{
		BulletCollisionPair()
			:m_pEntity(NULL),
			 m_pEntry(NULL),
			 m_pExit(NULL),
			 m_bAesthetic(false)
		{
			// Nothing for now
		}
		CEntity* m_pEntity;
		WorldProbe::CShapeTestHitPoint* m_pEntry;
		WorldProbe::CShapeTestHitPoint* m_pExit;
		bool m_bAesthetic;
	};

	//
	// Internal functions
	//

	// PURPOSE: Update the position based on velocity
	void UpdatePositions();

	// PURPOSE: Detect if we've hit anything along our path
	bool ComputeImpacts(sBatchImpacts& impacts) const;

	// PURPOSE: Detect if we've hit a cloth
	void ComputeClothImpacts(const Vector3& vOldPosition, const Vector3& vPosition) const;

	// PURPOSE: Detect if we've hit a rope
	void ComputeRopeImpacts(const Vector3& vOldPosition, const Vector3& vPosition) const;

	// PURPOSE: Analyzes the impact to determine if the hit is aesthetic
	bool IsImpactAesthetic( CEntity* pImpactEntity, const WorldProbe::CShapeTestHitPoint* pImpactHitPoint );

	// PURPOSE: Analyzes the impact to determine if the hit is a headshot
	bool IsImpactHeadShot( CEntity* pImpactEntity, const WorldProbe::CShapeTestHitPoint* pImpactHitPoint );

	// PURPOSE: Detect if we've hit vehicle tyres - as they are not picked up by the physics ray cast, sort the intersection list etc.
	void PreprocessImpacts(sBulletImpacts::Impacts& impacts, WorldProbe::CShapeTestResults::SortFunctionCB sortFn) const;

	// PURPOSE: Apply damage to detected impacts
	void ProcessImpacts(sBatchImpacts& impacts);

	// PURPOSE: Process any vfx on each bullet
	void ProcessVfx();

	// PURPOSE: Detect if we have hit ocean water
	bool ProcessHitsOceanWater(u32 index);

	// PURPOSE: Detect if we have hit an air defence sphere.
	bool ProcessHitsAirDefenceZone(u32 index);


	// PURPOSE: Determine if a bullet passes through
	f32 ComputePassesThrough(const WorldProbe::CShapeTestHitPoint* entryHitPoint, const WorldProbe::CShapeTestHitPoint* exitHitPoint, f32& fInOutPenetration) const;

	// PURPOSE: Send whizzed by events to the ped event group.
	void GenerateWhizzedByEvents() const;
	void GenerateWhizzedByEventAtEndPosition(const Vector3& vPosition) const;

	// PURPOSE: Sorting the intersections by t-value
	static s32 CompareEntryIntersections(const WorldProbe::CShapeTestHitPoint* a, const WorldProbe::CShapeTestHitPoint* b);

	// PURPOSE: Sorting the intersections by t-value
	static s32 CompareExitIntersections(const WorldProbe::CShapeTestHitPoint* a, const WorldProbe::CShapeTestHitPoint* b);

	//
	// Members
	//

	// PURPOSE: Origin of bullet - used by damage calculations
	Vector3 m_vOriginPosition;

	// PURPOSE: Position of weapon muzzle position - used for vfx
	Vector3 m_vMuzzlePosition;

	// PURPOSE: Direction of weapon muzzle - used to decide if vfx is allowed
	Vector3 m_vMuzzleDirection;

	// PURPOSE: Firer of the bullet
	RegdEnt m_pFiringEntity;

	// PURPOSE: Weapon type
	RegdConstWeaponInfo m_pWeaponInfo;

	// PURPOSE: Life time of bullet
	f32 m_fLifeTime;

	// PURPOSE: Damage of bullet
	f32 m_fDamage;

	// PURPOSE: Bullet instances associated with this bullet
	typedef atFixedArray<sBulletInstance*, MAX_BULLETS_IN_BATCH> Batch;
	Batch m_Batch;

	// PURPOSE: Bullet entry and exit points
	typedef atFixedArray<BulletCollisionPair, MAX_BULLETS_COLLISION_PAIRS> BulletCollisionPairs;
	BulletCollisionPairs m_BulletCollisionPairs;

	// PURPOSE: Store the original recoil accuracy
	float m_fRecoilAccuracyWhenFired;

	//
	float m_fFallOffRangeModifier;
	float m_fFallOffDamageModifier;

	// PURPOSE: Should check vehicle tyres for impacts
	bool m_bCheckVehicleTyres : 1;

	// PURPOSE: Allow bullets to damage vehicle ped is in
	bool m_bAllowDamageToVehicle : 1;

	// PURPOSE: Allow bullets to damage to vehicle occupants of peds vehicle 
	bool m_bAllowDamageToVehicleOccupants : 1;

	// PURPOSE: Whether or not we perform shoot through detection
	bool m_bPerformPenetration : 1;

	// PURPOSE: Whether or not we perform shoot through detection for bullet proof glass.
	bool m_bPerformBulletProofGlassPenetration : 1;

	// 
	bool m_bSilenced : 1;
	bool m_bCheckForAirDefenceZone : 1;
	bool m_bIsBentBulletFromAimAssist : 1;

	// Cloud tunables
	static bool sm_bSpecialAmmoFMJVehicleGlassAlwaysPenetrate;

#if __DEV
	// PURPOSE: Source weapon
	RegdWeapon m_WeaponOwner;
#endif // __DEV

#if !__FINAL
	static u32 m_uRejectionReason;
#endif //__FINAL
};

////////////////////////////////////////////////////////////////////////////////

class CBulletManager
{
public:

	// Temp: Batch tests are broken so only do this for multiple shot weapons
	static bool ms_bUseBatchTestsForSingleShotWeapons;

	static bool ms_bUseUndirectedBatchTests;

	// PURPOSE: Init
	static void Init();

	// PURPOSE: Shutdown
	static void Shutdown();

	// PURPOSE: Add
	static bool Add(CBullet* pBullet);

	// PURPOSE: Update
	static void Process();

	// PURPOSE: Record bullet impact for script querying
	static void AddBulletImpact(WorldProbe::CShapeTestHitPoint& impact, CEntity* pFiringEntity, bool bIsEntryImpact);

	// PURPOSE: Reset bullets
	static void ResetBullets();

	// PURPOSE: Reset recorded bullet impacts
	static void ResetBulletImpacts();

	// PURPOSE: Return if there is a bullet fired in the angled area
	static bool ComputeIsBulletInAngledArea(Vector3& vPos1, Vector3& vPos2, float areaWidth, const CEntity* pFiringEntity);

	// PURPOSE: Return if there is a bullet fired in the specified sphere
	static bool ComputeIsBulletInSphere(const Vector3& vPosition, f32 fRadius, const CEntity* pFiringEntity = NULL);

	// PURPOSE: Return if there is a bullet fired in the specified box
	static bool ComputeIsBulletInBox(const Vector3& vMin, const Vector3& vMax, const CEntity* pFiringEntity = NULL);

	// PURPOSE: Return if there is a bullet impact in the specified sphere
	static bool ComputeHasBulletImpactedInSphere(const Vector3& vPosition, f32 fRadius, const CEntity* pFiringEntity = NULL, bool bEntryOnly = true);

	// PURPOSE: Return if there is a bullet impact in the specified box
	static bool ComputeHasBulletImpactedInBox(const Vector3& vMin, const Vector3& vMax, const CEntity* pFiringEntity = NULL, bool bEntryOnly = true);

	// PURPOSE: Return if there is a bullet impact behind the specified plane
	static bool ComputeHasBulletImpactedBehindPlane( const Vector3& vPlanePoint, const Vector3& vPlaneNormal, bool bEntryOnly = true );

	// PURPOSE: Return the number of bullet impacts in the specified sphere
	static s32 ComputeNumBulletsImpactedInSphere(const Vector3& vPosition, f32 fRadius, const CEntity* pFiringEntity = NULL, bool bEntryOnly = true);

	// PURPOSE: Return the number of bullet impacts in the specified box
	static s32 ComputeNumBulletsImpactedInBox(const Vector3& vMin, const Vector3& vMax, const CEntity* pFiringEntity = NULL, bool bEntryOnly = true);

#if __BANK
	// PURPOSE: Debug
	static void RenderDebug();
#endif // __BANK

private:

	// PURPOSE: Bullet storage
	typedef atFixedArray<CBullet*, CBullet::MAX_STORAGE> Bullets;
	static Bullets ms_Bullets;

	static const s32 MAX_IMPACTS_PER_FRAME = 256;
	typedef atFixedArray<Vector3, MAX_IMPACTS_PER_FRAME> BulletImpacts;
	typedef atFixedArray<CEntity*, MAX_IMPACTS_PER_FRAME> BulletEntities;
	static BulletImpacts ms_EntryImpacts, ms_ExitImpacts;
	static BulletEntities ms_EntryEntities, ms_ExitEntities;
};

////////////////////////////////////////////////////////////////////////////////

inline s32 CBullet::CompareEntryIntersections(const WorldProbe::CShapeTestHitPoint* a, const WorldProbe::CShapeTestHitPoint* b)
{
	return a->GetHitTValue() < b->GetHitTValue() ? -1 : 1;
}

////////////////////////////////////////////////////////////////////////////////

inline s32 CBullet::CompareExitIntersections(const WorldProbe::CShapeTestHitPoint* a, const WorldProbe::CShapeTestHitPoint* b)
{
	return a->GetHitTValue() > b->GetHitTValue() ? -1 : 1;
}

////////////////////////////////////////////////////////////////////////////////

#endif // BULLET_H
