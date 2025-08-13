// Title	:	Physical.h
// Started	:	17/11/98

#ifndef INC_PHYSICAL_H_
#define INC_PHYSICAL_H_

// Game headers
#include "control/replay/ReplaySupportClasses.h"
#include "scene/DynamicEntity.h"
#include "scene/RegdRefTypes.h"
#include "physics/Floater.h"
#include "physics/CollisionRecords.h"

// Framework headers
#include "fwsys/timer.h"
#include "fwutil/Flags.h"

// Rage headers
#include "physics/sleep.h"
#include "physics/colliderdispatch.h"
#include "physics/debugPlayback.h"
#include "security/plugins/linkdatareporterplugin.h"

#define PHYSICAL_MAX_DIST_FROM_CAMERA	100.0f	// used in audiolog.cpp
#define DEFAULT_ACCEL_LIMIT 260.0f
#define DEFAULT_IMPULSE_LIMIT 150.0f
#define DEFAULT_VELOCITY_LIMIT 1000.0f

#if __ASSERT
// Make it easier to keep assert messages and tolerances consistent.
#define FORCE_CHECK(msg, vForce, maxAccel) \
	Assertf(CheckForceInRange(vForce, maxAccel), \
	"%s Force too large for '%s' - acceleration = %f > safelimit = %f\n", #msg, GetModelName(), vForce.Mag()/rage::Max(1.0f, GetMass()), maxAccel);
// We use CheckForceInRange() below for convenience, but it is really testing that the impulse is less than
// the equivalent change in speed over the time-step multiplied by the object's mass.
#define IMPULSE_CHECK(msg, vImpulse, maxSpeedDiff) \
	Assertf(CheckForceInRange(vImpulse, maxSpeedDiff), \
	"%s Impulse too large - change in speed = %f > safelimit = %f\n", #msg, vImpulse.Mag()/rage::Max(1.0f, GetMass()), maxSpeedDiff);
// Make it easier to keep offset from bound centroid assert messages and tolerances consistent.
#define OFFSET_FROM_CENTROID_CHECK(msg, vOffset, fCentroidRadiusScale) \
	IsOffsetWithinCentroidRadius(#msg, vecOffset, fCentroidRadiusScale);
#else // __ASSERT
#define FORCE_CHECK(msg, vForce, maxAccel)
#define IMPULSE_CHECK(msg, vImpulse, maxSpeedDiff)
#define OFFSET_FROM_CENTROID_CHECK(msg, vOffset, fCentroidRadiusScale)
#endif // __ASSERT

enum eDetachFlags
{
	DETACH_FLAG_ACTIVATE_PHYSICS			= BIT(0),
	DETACH_FLAG_APPLY_VELOCITY				= BIT(1),
	DETACH_FLAG_NO_COLLISION_UNTIL_CLEAR	= BIT(2),		// Do not collide with object until you are clear of its bounds
	DETACH_FLAG_IGNORE_SAFE_POSITION_CHECK	= BIT(3),
	DETACH_FLAG_SKIP_CURRENT_POSITION_CHECK = BIT(4),		// Do not test the peds current position (when warping from a vehicle)
	DETACH_FLAG_DONT_REMOVE_BASIC_ATTACHMENTS = BIT(5),
	DETACH_FLAG_EXCLUDE_VEHICLE				= BIT(6),		// Do not test against the vehicle we are in
	DETACH_FLAG_DONT_SNAP_MATRIX_UPRIGHT	= BIT(7)		// Do not snap the matrix upright

#if __ASSERT
	, DETACH_FLAG_DONT_ASSERT_IF_IN_VEHICLE	= BIT(8)
#endif
	, DETACH_FLAG_USE_ROOT_PARENT_VELOCITY	= BIT(9)		// Try to use the velocity from the root parent (parent's parent).
	, DETACH_FLAG_USE_UPSIDE_DOWN_POINT		= BIT(10)
	, DETACH_FLAG_IN_PHYSICAL_DESTRUCTOR	= BIT(11)		// Allows us to identify the physical is being destroyed
};

// moved this out of the class because it seemed to be causing visual assist problems
enum ePhysicsResult
{
	PHYSICS_POSTPONE = 0,
	PHYSICS_POSTPONE2,
	PHYSICS_DONE,
	PHYSICS_NEED_SECOND_PASS,
	PHYSICS_NEED_THIRD_PASS
};

class CObjectCoverExtension : public fwExtension
{
private:

	// After this much time object will be considered for coverpoint regeneration
	static const u32 ms_uCoverObstructionExpirationTimeMS = 5000;
	
	// Each bit corresponds to a cached object cover index,
	// and if set indicates that cover is blocked and should not be re-added.
	// Flags size determined by CCachedObjectCover::MAX_COVERPOINTS_PER_OBJECT in cover.h
	// (currently 12, hence 16 bit flags)
	fwFlags16 m_CachedObjectCoverBlockedFlags;

	// Keep track of the last time blocked cover was removed
	u32 m_uLastSetCoverObstructedTimeMS;

public:

#if !__SPU
	EXTENSIONID_DECL(CObjectCoverExtension, fwExtension);
#endif

	CObjectCoverExtension() : m_uLastSetCoverObstructedTimeMS(0) {}

	void SetCachedObjectCoverObstructed(int objectCoverIndex) { m_CachedObjectCoverBlockedFlags.SetFlag(BIT(objectCoverIndex)); m_uLastSetCoverObstructedTimeMS = fwTimer::GetTimeInMilliseconds(); }
	void ClearAllCachedObjectCoverObstructed() { m_CachedObjectCoverBlockedFlags.ClearAllFlags(); }
	bool IsCachedObjectCoverObstructed(int objectCoverIndex) const { return m_CachedObjectCoverBlockedFlags.IsFlagSet(BIT(objectCoverIndex)); }
	bool IsCoverObstructedTimeExpired() const { return ( (m_CachedObjectCoverBlockedFlags.GetAllFlags() > 0) && (fwTimer::GetTimeInMilliseconds() - m_uLastSetCoverObstructedTimeMS > ms_uCoverObstructionExpirationTimeMS) ); }

};

class CBrokenAndHiddenComponentFlagsExtension : public fwExtension
{
public:
	CBrokenAndHiddenComponentFlagsExtension() {}

#if !__SPU
	EXTENSIONID_DECL(CBrokenAndHiddenComponentFlagsExtension, fwExtension);
#endif

	inline CBrokenAndHiddenComponentFlagsExtension& CreateAndAttach(CEntity* pEntity)
	{
		if (this == NULL)
		{
			CBrokenAndHiddenComponentFlagsExtension* pExt = rage_new CBrokenAndHiddenComponentFlagsExtension;
			pExt->ResetBrokenAndHiddenFlags();
			pEntity->GetExtensionList().Add(*pExt);
			return *pExt;
		}

		return *this;
	}

	// these methods can be called safely on NULL this (they do nothing or return false)
	inline void ResetBrokenAndHiddenFlags() { if (this) { m_brokenComponentFlags.Reset(); m_hiddenComponentFlags.Reset(); } }
	inline bool IsBrokenFlagSet(int componentId) const { return this ? m_brokenComponentFlags.IsSet(componentId) : false; }
	inline bool IsHiddenFlagSet(int componentId) const { return this ? m_hiddenComponentFlags.IsSet(componentId) : false; }
	inline bool AreAnyBrokenFlagsSet() const { return this ? m_brokenComponentFlags.AreAnySet() : false; }
	inline bool AreAnyHiddenFlagsSet() const { return this ? m_hiddenComponentFlags.AreAnySet() : false; }

	inline void SetBrokenFlag(int componentId) { m_brokenComponentFlags.Set(componentId); }
	inline void SetHiddenFlag(int componentId) { m_hiddenComponentFlags.Set(componentId); }
	inline void ClearBrokenFlag(int componentId) { m_brokenComponentFlags.Clear(componentId); }
	inline void ClearHiddenFlag(int componentId) { m_hiddenComponentFlags.Clear(componentId); }

private:
	atFixedBitSet<64> m_brokenComponentFlags;
	atFixedBitSet<64> m_hiddenComponentFlags;
};

#if __BANK
	template <class _Type, int _Size>
	class CDebugObjectIDQueue
	{
	public:

		CDebugObjectIDQueue() 
		{
			Init();
		}

		void Init() 
		{
			m_FreeIdQueue.Reset();
			for(_Type i = 0; i < _Size; ++i)
			{
				m_FreeIdQueue.Push(i);
			}
		}

		void ReturnID(_Type ID) 
		{ 
			Assertf(!m_FreeIdQueue.Find(ID), "Pushing object ID into list that already contains this ID!");
			m_FreeIdQueue.Push(ID); 
		}

		_Type GetNewID() 
		{
			// set up the logging name for this object
			if(Verifyf(m_FreeIdQueue.GetCount() > 0, "Ran out of object ID's - please increase!"))
			{
				return m_FreeIdQueue.Pop();
			}

			return 0;
		}

	private:

		atQueue<_Type, _Size> m_FreeIdQueue; 
	};
#endif

extern void ReportHealthMismatch(u32, u32);

class CPhysical : public CDynamicEntity
{
	friend class CWorld;
	friend class CNetObjPhysical;

#if GTA_REPLAY
	friend class CBasicEntityPacketData;
#endif 

	DECLARE_RTTI_DERIVED_CLASS(CPhysical, CDynamicEntity);

public:

	struct CPhysicalFlags
	{
		friend class CPhysical;
		friend class CBoat;

	private:
		u32	bIsInWater : 1;						// If this is set then the guy is floating in water

	public:
		u32	bWasInWater : 1;					// Was floating in water last frame		

		u32	bDontLoadCollision : 1;				// Doesn't try to load collision for this physical
		u32	bAllowFreezeIfNoCollision : 1;		// Should this object be allowed to deactivate from the physics sim if collision isn't loaded around it?

		u32 bNotDamagedByBullets : 1;			// This guy is bullet proof
		u32 bNotDamagedByFlames : 1;			// This guy is flame proof
		u32 bNotDamagedByCollisions : 1;		// This guy is collision proof
		u32 bNotDamagedByMelee : 1;				// This guy is melee weapon proof
		u32 bNotDamagedByAnything : 1;			// This guy can't be damaged by anything
		u32 bNotDamagedByAnythingButHasReactions: 1;	// This guy won't take any damage but will react to any impact/explosion etc.
		u32 bOnlyDamagedByPlayer : 1;			// This guy can only be damaged by the player
		u32	bIgnoresExplosions : 1;				// does entity get affected by explosions
		u32	bOnlyDamagedByRelGroup : 1;			// This guy can only be damaged by m_specificRelationshipGroup
		u32	bNotDamagedByRelGroup : 1;			// This guy can not be damaged by m_specificRelationshipGroup
		u32	bOnlyDamagedWhenRunningScript : 1;	// This guy can only be damaged when we're running the script he belongs to
		u32 bNotDamagedBySteam : 1;				// This guy is steam proof
		u32 bNotDamagedBySmoke : 1;				// This guy is smoke proof

		u32 bExplodeInstantlyWhenChecked : 1;	// This vehicle will explode instantly when a check to explode a vehicle is done
		u32 bFlyer : 1;							// Is a flying object (plane, heli)
		u32 bRenderScorched : 1;				// this guy is fucked and should be rendered black
		u32	bCarriedByRope : 1;					// We are attached to a rope (crane) at the moment
		u32	bMoved : 1;							// Set when physical is moved for the first time after a collision
		u32	bPossiblyTouchesWaterIsUpToDate : 1;
		u32	bModifiedBounds : 1;				// set when the collision archetype is modified (eg pickups)
#if HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
		u32	bSecondSurfaceImpact :1;			// Reset false each update and then set in ProcessPreComputeImpacts 
#endif
		u32 bIsNotBuoyant : 1;					// Set true to disable the buoyancy calculation (eg: for fish while swimming under water)
		u32 bNotToBeNetworked : 1;				// Set in MP when an entity is deliberately created without a network object
		u32 bUseKinematicPhysics : 1;			// Set if this entity wants to use the kinematic physics mode (can't be pushed out of the way by other objects)
		u32 bPreserveVelocityFromKinematicPhysics : 1;	// Set if this entity wants to preserve the velocity when switching out of kinematic physics
		u32 bIsRootOfOtherAttachmentTree : 1;	// Is this instance the root of a tree of attachments not known by CPhysical?
	
		u32 bAutoVaultDisabled : 1;				// This entity can't be auto-vaulted.
		u32 bClimbingDisabled : 1;				// This entity can't be climbed.
		u32 bPickUpByCargobobDisabled : 1;		// Have disabled being able to be picked up by the cargobob
#if __ASSERT
		u32 bIgnoreVelocityCheck : 1;			// Used when phys is in kinematic physics, velocity checks is not needed then
#endif
	private:
		u32 bIsUsingKinematicPhysics : 1;		// Is the entity currently in the kinematic physics mode (so we can tell when to activate / deactivate it)
		u32 bCreatedBlip			 : 1;		// optimization for cleanup - is there a blip associated with this
		u32 bCoverBoundsRemoved : 1;			// Have we already turned off collision for the extra cover collision bound if this is a frag?

		void Clear()
		{
			CompileTimeAssert(sizeof(*this) == sizeof(u64));
			*reinterpret_cast<u64*>(this) = 0; // clear all flags to false ..

			// .. and set some specific flags to true
			bDontLoadCollision = true;
			bAllowFreezeIfNoCollision = true;
		}
	};

	struct WeaponDamageInfo
	{
		WeaponDamageInfo() : pWeaponDamageEntity(NULL), weaponDamageHash(0), weaponDamageTime(0), bMeleeHit(false) {}

		RegdEnt pWeaponDamageEntity;
		u32	weaponDamageHash;
		u32 weaponDamageTime;
		bool bMeleeHit;

		bool operator==(const WeaponDamageInfo& rhs) const { return pWeaponDamageEntity == rhs.pWeaponDamageEntity && weaponDamageHash == rhs.weaponDamageHash; }

	};

#if HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
	class CSecondSurfaceConfig
	{
	public:
		CSecondSurfaceConfig();
		CSecondSurfaceConfig(float fSinkLiftFactor,
			float fTangV2LiftFactor,
			float fMaxTangV2LiftFactor,
			float fDragFactor);

#if __BANK
		void AddWidgetsToBank(bkBank& bank);
#endif

		// Lift is prop to current sink depth and tang V
		float m_fSinkLiftFactor;
		float m_fTangV2LiftFactor;

		// This is scaled by mass so value of 1 will support object's mass
		float m_fMaxTangV2LiftFactor;

		//Drag coefficient (used for drag forces)
		float m_fDragFactor;
	};
#endif	// HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
	
	const Mat34V*	m_MatrixTransformPtr;

	// Used for wind noise as objects pass camera
	//float m_fPrevDistFromCam; // RA - unused.
	CPhysicalFlags m_nPhysicalFlags;
	bool m_bDontResetDamageFlagsOnCleanupMissionState : 1;

#if !__NO_OUTPUT
	bool m_LogSetMatrixCalls : 1;
	bool m_LogVisibiltyCalls : 1; 
	bool m_LogDeletions : 1; 
	bool m_LogVariationCalls : 1;
#endif

#if __BANK
	typedef u16 DebugObjectID;
	DebugObjectID m_iDebugObjectId;

	static const int sMaxLogName = 20;
	char m_DebugLogName[sMaxLogName];
#endif

	u32 m_specificRelGroupHash;

public:
	bool IsAsleep() const
	{
		return(!GetCollider() || (GetCollider()->GetSleep() && GetCollider()->GetSleep()->IsAsleep()));
	}

#if __BANK
	DebugObjectID GetDebugObjectID() const { return m_iDebugObjectId; }

	static const int sMaxObjectIDs = 512;
	static const int sInvalidObjectID = 0xFFFF;
	static CDebugObjectIDQueue<DebugObjectID, sMaxObjectIDs> ms_aDebugObjectIDs;

	void AssignDebugObjectID()
	{
		//! get free ID.
		Assert(m_iDebugObjectId == sInvalidObjectID);
		m_iDebugObjectId = ms_aDebugObjectIDs.GetNewID();
		
		//! build debug string.
		InitDebugName();
	}

	void ResetDebugObjectID()
	{
		if(m_iDebugObjectId != sInvalidObjectID)
		{
			ms_aDebugObjectIDs.ReturnID(m_iDebugObjectId);
			m_iDebugObjectId = 0xFFFF;
		}
	}

	virtual void InitDebugName()
	{
		sprintf(m_DebugLogName, "PHYSICAL_%d", m_iDebugObjectId);
	}

	const char *GetDebugNameFromObjectID() const 
	{
		return m_DebugLogName;
	}

	static void RenderDebugObjectIDNames();

#endif

	void SetCanOnlyBeDamagedByEntity( const CEntity* damagedByEntity ) { m_canOnlyBeDamagedByEntity = damagedByEntity; }
	void SetCantBeDamagedByCollisionEntity( const CEntity* damagedByEntity ) { m_cantBeDamagedByEntity = damagedByEntity; }
	const CEntity* GetCantBeDamagedByCollisionEntity() { return m_cantBeDamagedByEntity; }

public:
	CBuoyancy m_Buoyancy;	

protected:
#if RSG_PC
	sysLinkedData<float, ReportHealthMismatch>	m_fHealth;
#else //RSG_PC
	float m_fHealth;
#endif //RSG_PC
	float	m_fMaxHealth;

	// stored the last 3 damager info
	static const s32 MAX_DAMAGE_COUNT = 3;
	typedef atFixedArray<WeaponDamageInfo, MAX_DAMAGE_COUNT> WeaponDamageInfos; 
	WeaponDamageInfos* m_pWeaponDamageInfos;

#if __ASSERT
	u16 m_physicsBacktrace;
#endif

private:
	CCollisionHistory m_frameCollisionHistory;	
	const CEntity* m_canOnlyBeDamagedByEntity;
	const CEntity* m_cantBeDamagedByEntity;

public:
	CPhysical(const eEntityOwnedBy ownedBy);
	~CPhysical();


	enum enResult
	{
		INIT_DELAYED	= 0,
		INIT_OK			= 1,
		INIT_CANCELLED	= 2,
	};

	// GTA_RAGE_PHYSICS
	virtual int InitPhys() {return INIT_OK;}		// gonna have different types of physics instances for different entity types



	inline void SetCreatedBlip(bool inBlip) { Assertf(!inBlip || !m_nPhysicalFlags.bCreatedBlip, "Too many blips for %s %s", GetDebugName(), GetModelName());
											  m_nPhysicalFlags.bCreatedBlip = inBlip; }
	inline bool GetCreatedBlip() const { return m_nPhysicalFlags.bCreatedBlip; }
	void RemoveBlip(u32 blipType);

	void SetIsInWater(const bool isInWater);
	bool GetIsInWater() const					{ return m_nPhysicalFlags.bIsInWater; }

	bool GetWasInWater() const					{ return m_nPhysicalFlags.bWasInWater; }

	// add and remove for physics world
	virtual void AddPhysics();
	virtual void RemovePhysics();
	void RemoveConstraints(phInst* pInst);

	// activate an inactive entity (wake it up)
	void ActivatePhysics();
	// deactivate - won't do any collision testing, will auto wake up if hit by something (I think)
	virtual void DeActivatePhysics();

	// fix entity so it won't react to physical forces or collisions
	virtual void SetFixedPhysics(bool bFixed, bool bNetwork = false);
	void DisablePhysicsIfFixed();

	// setting of collision flags
	void SetIsFixedWaitingForCollision(const bool bFixed);
	void SetDontLoadCollision(bool bDontLoad);

	// return if collision should be loaded for this entity
	virtual bool ShouldLoadCollision() const {return !m_nPhysicalFlags.bDontLoadCollision;}

	// default version copies collider matrix across to entity matrix but may wish to do some extra stuff here
	virtual void UpdateEntityFromPhysics(phInst *pInst, int nLoop);
	// optional function used by peds to process standing up and vehicles to do suspension lines
	virtual void ProcessProbes(const Matrix34& UNUSED_PARAM(testMat)) {;}

	virtual void SetHeading(float new_heading);
	virtual void SetPosition(const Vector3& vec, bool bUpdateGameWorld = true, bool bUpdatePhysics = true, bool bWarp = false);
	virtual void SetMatrix(const Matrix34& mat, bool bUpdateGameWorld = true, bool bUpdatePhysics = true, bool bWarp = false);

	virtual void SetVelocity(const Vector3& vecMoveSpeed);
	virtual const Vector3& GetVelocity() const;
	const Vector3& GetReferenceFrameVelocity() const;
	const Vector3& GetReferenceFrameAngularVelocity() const;

	Vec3V_Out CalculateVelocityFromMatrices(Mat34V_In mOld, Mat34V_In mNew, ScalarV_In scInvTimeStep) const;
	void SetVelocityFromMatrices(Mat34V_In mOld, Mat34V_In mNew, ScalarV_In scInvTimeStep);

	void SetAngVelocity(const float x, const float y, const float z) {SetAngVelocity(Vector3(x, y, z));}
	virtual void SetAngVelocity(const Vector3& vecTurnSpeed);
	virtual const Vector3& GetAngVelocity() const;

	Vec3V_Out CalculateAngVelocityFromQuaternions(QuatV_In qOrientationOld, QuatV_In qOrientationNew, ScalarV_In scInvTimeStep) const;
	Vec3V_Out CalculateAngVelocityFromMatrices(Mat34V_In mOld, Mat34V_In mNew, ScalarV_In scInvTimeStep) const;
	void SetAngVelocityFromMatrices(Mat34V_In mOld, Mat34V_In mNew, ScalarV_In scInvTimeStep);

	const Mat34V& GetMatrixRef() const { return *m_MatrixTransformPtr; }
#if ENABLE_MATRIX_MEMBER
	virtual void SetTransform(Mat34V_In transform, bool bUpdateGameWorld = true, bool bUpdatePhysics = true, bool bWarp = false);
#else
	virtual void SetTransform(fwTransform* transform, bool bUpdateGameWorld = true, bool bUpdatePhysics = true, bool bWarp = false);
#endif

	virtual Mat34V_Out GetMatrixPrePhysicsUpdate() const;

	float GetMass() const {Assert(GetPhysArch()); return GetPhysArch()->GetMass();}
	float GetInvMass() const {Assert(GetPhysArch()); return GetPhysArch()->GetInvMass();}
	void SetMass(float fNewMass);
	Vector3 GetAngInertia() const {Assert(GetPhysArch()); return GetPhysArch()->GetAngInertia();}
	Vector3 GetInvAngInertia() const {Assert(GetPhysArch()); return GetPhysArch()->GetInvAngInertia();}

    void SetHealth(const float fNewHealth, bool bNetCall = false, bool bAssertMissingDamageEvents = true);
    float GetHealth() const {return m_fHealth;}
	void  ChangeHealth( float fHealthChange );
	inline float GetMaxHealth() const { return m_fMaxHealth; }
	inline void SetMaxHealth(float fMaxHealth) { m_fMaxHealth = fMaxHealth; }

	float GetMaxSpeed() const;

	void SetWeaponDamageInfo(CEntity* pEntity, u32 hash, u32 time, bool meleeHit = false);
	void SetWeaponDamageInfo(WeaponDamageInfo weaponDamageInfo);
	void ResetWeaponDamageInfo() {if(m_pWeaponDamageInfos) m_pWeaponDamageInfos->Reset();}
	int GetWeaponDamageInfoCount() const { return m_pWeaponDamageInfos ? m_pWeaponDamageInfos->GetCount() : 0; }
	const WeaponDamageInfo& GetWeaponDamageInfo(int i) const {Assert(m_pWeaponDamageInfos && i < m_pWeaponDamageInfos->GetCount()); return (*m_pWeaponDamageInfos)[i];}
	u32 GetWeaponDamagedTimeByEntity(CEntity* pEntity) const;
	u32 GetWeaponDamagedTimeByHash(u32 uWeaponHash) const;
	void MakeAnonymousDamageByEntity(CEntity* pEntity);
	CEntity* GetWeaponDamageEntity() const { if(!m_pWeaponDamageInfos || m_pWeaponDamageInfos->GetCount() == 0) return NULL; return m_pWeaponDamageInfos->Top().pWeaponDamageEntity; }
	CEntity* GetWeaponDamageEntityPedSafe() const;
	u32 GetWeaponDamageHash() const { if(!m_pWeaponDamageInfos || m_pWeaponDamageInfos->GetCount() == 0) return 0; return m_pWeaponDamageInfos->Top().weaponDamageHash; }
	u32 GetWeaponDamagedTime() const { if(!m_pWeaponDamageInfos || m_pWeaponDamageInfos->GetCount() == 0) return 0; return m_pWeaponDamageInfos->Top().weaponDamageTime;  }
	bool GetWeaponDamagedMeleeHit() const { if(!m_pWeaponDamageInfos || m_pWeaponDamageInfos->GetCount() == 0) return false; return m_pWeaponDamageInfos->Top().bMeleeHit;  }

	bool HasBeenDamagedByEntity(CEntity* pEntity) const;
	bool HasBeenDamagedByHash(u32 hash) const;
	bool HasBeenDamagedByGeneralWeaponType(int generalWeaponType) const;
	bool HasBeenDamagedByAnyPed() const;
	bool HasBeenDamagedByAnyObject() const;
	bool HasBeenDamagedByAnyVehicle() const;

	virtual void SetupMissionState();

#if __ASSERT
	void SpewDamageHistory() const;
#endif

	// API for marking obstructed coverpoints on a CPhysical object
	void ClearAllCachedObjectCoverObstructed();
	void SetCachedObjectCoverObstructed(int cachedObjectCoverIndex);
	bool HasExpiredObjectCoverObstructed() const;
	bool IsCachedObjectCoverObstructed(int cachedObjectCoverIndex) const;

#if !__SPU
	inline void ResetBrokenAndHiddenFlags()				{ GetExtension<CBrokenAndHiddenComponentFlagsExtension>()->ResetBrokenAndHiddenFlags(); }
	inline bool DoesBrokenHiddenComponentExist() const	{ return GetExtension<CBrokenAndHiddenComponentFlagsExtension>() != NULL; }
	inline bool IsBrokenFlagSet(int componentId) const	{ return GetExtension<CBrokenAndHiddenComponentFlagsExtension>()->IsBrokenFlagSet(componentId); }
	inline bool IsHiddenFlagSet(int componentId) const	{ return GetExtension<CBrokenAndHiddenComponentFlagsExtension>()->IsHiddenFlagSet(componentId); }
	inline bool AreAnyBrokenFlagsSet() const			{ return GetExtension<CBrokenAndHiddenComponentFlagsExtension>()->AreAnyBrokenFlagsSet(); }
	inline bool AreAnyHiddenFlagsSet() const			{ return GetExtension<CBrokenAndHiddenComponentFlagsExtension>()->AreAnyHiddenFlagsSet(); }
	inline void SetBrokenFlag(int componentId)			{ GetExtension<CBrokenAndHiddenComponentFlagsExtension>()->CreateAndAttach(this).SetBrokenFlag(componentId); }
	inline void SetHiddenFlag(int componentId)			{ GetExtension<CBrokenAndHiddenComponentFlagsExtension>()->CreateAndAttach(this).SetHiddenFlag(componentId); }
	inline void ClearBrokenFlag(int componentId)		{ GetExtension<CBrokenAndHiddenComponentFlagsExtension>()->CreateAndAttach(this).ClearBrokenFlag(componentId); }
	inline void ClearHiddenFlag(int componentId)		{ GetExtension<CBrokenAndHiddenComponentFlagsExtension>()->CreateAndAttach(this).ClearHiddenFlag(componentId); }
#endif // !__SPU

	void ResetWaterStatus();

	void SetIgnoresExplosions(bool bIgnore) { m_nPhysicalFlags.bIgnoresExplosions = bIgnore; }
	bool GetIgnoresExplosions() const { return ( m_nPhysicalFlags.bIgnoresExplosions || m_nPhysicalFlags.bNotDamagedByAnything); }
	void SetAllowFreezeWaitingOnCollision(bool bFreeze) { m_nPhysicalFlags.bAllowFreezeIfNoCollision = bFreeze; }
	bool GetAllowFreezeWaitingOnCollision() { return m_nPhysicalFlags.bAllowFreezeIfNoCollision; }
	void SetIsNotBuoyant(bool bIsNotBuoyant) { m_nPhysicalFlags.bIsNotBuoyant = bIsNotBuoyant; }
	bool GetIsNotBuoyant() const { return m_nPhysicalFlags.bIsNotBuoyant; }

	void SetAutoVaultDisabled(bool bDisabled) { m_nPhysicalFlags.bAutoVaultDisabled = bDisabled; }
	bool GetAutoVaultDisabled() const { return m_nPhysicalFlags.bAutoVaultDisabled; }
	void SetClimbingDisabled(bool bDisabled) { m_nPhysicalFlags.bClimbingDisabled = bDisabled; }
	bool GetClimbingDisabled() const { return m_nPhysicalFlags.bClimbingDisabled; }

	virtual bool ProcessControl();
	virtual void ProcessPrePhysics( void );
	virtual void ProcessPostPhysics( void );
	// returns whether it got processed or was postponed or needs a second pass
	virtual ePhysicsResult ProcessPhysics(float UNUSED_PARAM(fTimeStep), bool UNUSED_PARAM(bCanPostpone), int UNUSED_PARAM(nTimeSlice)) {return PHYSICS_DONE;}
	virtual ePhysicsResult ProcessPhysics2(float UNUSED_PARAM(fTimeStep), int UNUSED_PARAM(nTimeSlice)) {return PHYSICS_DONE;}	
	virtual ePhysicsResult ProcessPhysics3(float UNUSED_PARAM(fTimeStep), int UNUSED_PARAM(nTimeSlice)) {return PHYSICS_DONE;}	

	virtual bool CanStraddlePortals() const;

	////////////////
	// store collision results
	////////////////

	//The records are cleared every frame. Currently the only data that persists is the latest collision time.
	//There may be multiple physics ticks per frame, in which case the collisions from all ticks merged together
	//If the inst is active at the start of a tick, then there will be complete information about what that inst is touching for that tick
	__forceinline CCollisionHistory* GetFrameCollisionHistory()
	{  
		if(!m_frameCollisionHistory.IsCurrent())
		{
			// If the collision history isn't up-to-date, reset it before giving it to the user
			m_frameCollisionHistory.Reset();
		}
		return &m_frameCollisionHistory; 
	}
	__forceinline const CCollisionHistory* GetFrameCollisionHistory() const 
	{ 
		return const_cast<CPhysical*>(this)->GetFrameCollisionHistory();
	}

	// These functions let the user access non-frame dependent information on the collision history without extra branches/resetting the history
	void SetRecordCollisionHistory(bool recordCollisionHistory = true) { m_frameCollisionHistory.SetHistoryRecorded(recordCollisionHistory); }
	bool IsRecordingCollisionHistory() const { return m_frameCollisionHistory.IsHistoryRecorded(); }

	// Helper function to make sure we still get a record of an impact with our object even when we remove that impact from the
	// simulator in a PreComputeImpacts call back.
	void AddCollisionRecordBeforeDisablingContact(phInst* pThisInst, CEntity* pOtherEntity, phContactIterator impacts);
	void AddCollisionRecordBeforeDisablingContact(phInst* pThisInst, CEntity* pOtherEntity, phCachedContactIteratorElement* pIterator);

	// Called every time there is a collision with this entity.
	// Would be neater to just pass down the contact but it has some quirky variables
	// which are easy to misinterpret so its best to explicitly pass all of the collision data
	virtual void ProcessCollision(
		phInst const * myInst, 
		CEntity* pHitEnt, 
		phInst const* hitInst,
		const Vector3& vMyHitPos,
		const Vector3& vOtherHitPos,
		float fImpulseMag,
		const Vector3& vMyNormal,
		int iMyComponent,
		int iOtherComponent,
		phMaterialMgr::Id iOtherMaterial,
		bool bIsPositiveDepth,
		bool bIsNewContact);

#if HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
	virtual void ProcessPreComputeImpacts(phContactIterator);

	//Update second surface interp value and store a record of the second surface contact.
	void ProcessSecondSurfaceImpacts(phContactIterator impacts, const CSecondSurfaceConfig& secondSurfaceConfig = sm_defaultSecondSurfaceConfig); 

	//Tuning values for second surface physics.
	static CSecondSurfaceConfig sm_defaultSecondSurfaceConfig;
#else
	virtual void ProcessPreComputeImpacts(phContactIterator) {}
#endif	// HACK_GTA4_BOUND_GEOM_SECOND_SURFACE

public:

	void DoCollisionEffects();

#if __ASSERT	// helper function for offset asserts inside apply force functions
	float GetRadiusAroundLocalOrigin();
	bool CheckForceInRange(const Vector3 &vecForce, const float maxAccel) const;
#endif
#if __ASSERT
    virtual const char *GetDebugName() const;
#elif __BANK || !__NO_OUTPUT
	const char *GetDebugName() const;
#else // __BANK
	const char *GetDebugName() const { return ""; }
#endif 

	void SetDamping( int dampingType, float dampingValue );
	Vector3 GetDamping( int dampingType );

	virtual void NotifyWaterImpact(const Vector3& vecForce, const Vector3& vecTorque, int nComponent, float fTimeStep);

	// The default timestep used for these functions is fwTimer::GetTimeStep() / CPhysics::GetNumTimeSlices().  
	// An override timestep can be supplied via timeStepOverride.
	void ApplyForce(const Vector3& vecForce, const Vector3& vecOffset, int nComponent=0, bool bCheckForces=true, float timeStepOverride = -1.0f);
	void ApplyForceCg(const Vector3& vecForce, float timeStepOverride = -1.0f);
	void ApplyTorque(const Vector3& vecForce, const Vector3& vecOffset, float timeStepOverride = -1.0f);
	void ApplyTorque(const Vector3& vecTorque, float timeStepOverride = -1.0f);
	// PURPOSE:	Apply the given torque and apply the necessary force to result in rotation about the given world position.
	void ApplyTorqueAndForce(const Vector3& vecTorque, const Vector3& vecWorldPosition, float timeStepOverride = -1.0f);

	enum ImpulseFlags { IF_InitialImpulse = BIT0};// Initial impulse can only be used to set an objects initial velocity
	void ApplyImpulse(const Vector3& vecImpulse, const Vector3& vecOffset, int nComponent=0, bool bLocalOffset=false);
	void ApplyImpulseCg(const Vector3& vecImpulse, s32 impulseFlags = 0); 
	void ApplyAngImpulse(const Vector3& vecAngImpulse, s32 impulseFlags = 0); 
	void ApplyAngImpulse(const Vector3& vecImpulse, const Vector3& vecOffset, int nComponent=0, bool bLocalOffset=false, s32 impulseFlags=0);

	void ApplyExternalForce(const Vector3& vecForce, const Vector3& vecOffset, int nComponent=0, int nPartIndex=0, phInst* pInst=NULL, float fBreakingForce=-1, float timeStepOverride = -1.0f ASSERT_ONLY(, bool ignoreCentroidCheck = false));
	void ApplyExternalImpulse(const Vector3& vecImpulse, const Vector3& vecOffset, int nComponent=0, int nPartIndex=0, phInst* pInst=NULL, float fBreakingImpulse=-1, bool bCheckOffset=true, float timeStepOverride = -1.0f);

	// these functions return the velocity at a point on the entity *in world space!"
	// the offset is in world space taken from the entity centre
	Vector3 GetLocalSpeed(const Vector3& vecOffset, bool bWorldOffset=false, int nComponent=0) const;
	Vector3 GetLocalSpeedIntegrated(const float fTimeStep, Vec3V_In vecOffset, bool bWorldOffset=false, int nComponent=0) const;
	float GetMassAlongVector(const Vector3& vecNormal, const Vector3& vecWorldPos);
	float GetMassAlongVectorLocal( const Vector3& vecNormal, const Vector3& vecLocalPos );

	// returns the true distance traveled in previous frame, going to store last position in CVehicle to achieve this for them
	float GetDistanceTravelled() const {return Mag(GetTransform().GetPosition() - VECTOR3_TO_VEC3V(GetPreviousPosition())).Getf();}

	static void PlacePhysicalRelativeToOtherPhysical(CPhysical *pBasePhysical, CPhysical *pToBePlacedPhysical, const Vector3& Offset);
	float ApplyScriptCollision(const Vector3& vecColNormal, float fElasticity = 0.5f, float fAdhesiveLimit = 2.0f, Vector3 *pVecColPos = NULL);

	bool DoPreComputeImpactsTest() const { return GetNoCollisionEntity() && (GetNoCollisionFlags() &NO_COLLISION_RESET_WHEN_NO_IMPACTS); }

	virtual bool TestNoCollision(const phInst *pOtherInst);

	// Kinematic physics interface

	// PURPOSE: Activates (or deactivates) kinematic physics mode this frame on the ped. This needs to be called every frame (will be reset after the physics update)
	void SetShouldUseKinematicPhysics(bool bUse){m_nPhysicalFlags.bUseKinematicPhysics = bUse; }
	bool ShouldUseKinematicPhysics() const { return m_nPhysicalFlags.bUseKinematicPhysics!=0;}
	bool IsUsingKinematicPhysics() const { return m_nPhysicalFlags.bIsUsingKinematicPhysics!=0;}

	void SetCoverBoundsAlreadyRemoved(bool bValue) { m_nPhysicalFlags.bCoverBoundsRemoved = bValue; }
	bool CoverBoundsAlreadyRemoved() const { return m_nPhysicalFlags.bCoverBoundsRemoved; }

	void SetPickupByCargobobDisabled(bool bValue) { m_nPhysicalFlags.bPickUpByCargobobDisabled = bValue; }
	bool GetPickupByCargobobDisabled() const { return m_nPhysicalFlags.bPickUpByCargobobDisabled; }
	
#if __ASSERT
	void SetIgnoreVelocityCheck(bool bValue) { m_nPhysicalFlags.bIgnoreVelocityCheck = bValue; }
	bool GetIgnoreVelocityCheck() const { return m_nPhysicalFlags.bIgnoreVelocityCheck; }
#endif
	// PURPOSE: Can be overridden by derived classes to disable kinematic physics mode in certain situations
	//			e.g. for ragdolling peds.
	virtual bool CanUseKinematicPhysics() const { return true; }
protected:
	// PURPOSE: Does the job of switching the physics inst to kinematic mode
	//			Should return true if the switch was successful, false otherwise
	virtual bool EnableKinematicPhysics();
	// PURPOSE: Does the job of switching the physics inst back to non kinematic mode
	//			Should return true if the switch was successful, false otherwise
	virtual bool DisableKinematicPhysics();

	// PURPOSE: Get the inst to use when activating kinematic physics
	virtual phInst* GetInstForKinematicMode() { return GetCurrentPhysicsInst(); }

public:

	/** Physical attachment system
	*
	* Purpose:
	*	Each physical object can be attached to another physical or the world using the AttachTo<X> functions.
	*	There are two fundamental types of attachment: BASIC and PHYSICAL.
	*	A BASIC attachment has its collision turned off (by default) and has its matrices updated each frame to 
	*	maintain the attachment. This makes the object effectively get stuck to the parent.
	*   A PHYSICAL attachment creates a constraint withing the physics system to handle the attachment. This is preferable
	*   when attaching an articulated body to something since it will move realistically. It also means the attached object
	*   will affect its parent through its weight and inertia.
	*   In general, use a basic attachment when possible since they are cheaper and more reliable.
	*
	* Notes on storage and processing:
	*	It is possible to create chains of attachments of any length. Each attachment stores a connection to a child, a sibling and its parent.
	*	When an object is detached or deleted its parent needs to be informed so that the tree's structure can get rearranged so that no siblings
	*	get 'cut off'.
	*   Attachments are processed recursively from the parent down. When possible attachments are processed before PreRender, but when objects are attached
	*   to non-root bones the attachment processing must be delayed until after the PreRender. This is handled in CGameWorld.
	*/	

	
	// Check all parent attachments up the tree to see if this physical is a parent
	// Useful for preventing a closed loop of attachments
	bool GetIsPhysicalAParentAttachment(CPhysical* pPhysical) const;

#if __BANK
public:
	#define ProcessAllAttachments(...) DebugProcessAllAttachments(__FILE__, __FUNCTION__, __LINE__)
	#define ProcessAttachment(...) DebugProcessAttachment(__FILE__, __FUNCTION__, __LINE__)

	#define AttachToPhysicalBasic(...) DebugAttachToPhysicalBasic(__FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
	#define AttachToWorldBasic(...) DebugAttachToWorldBasic(__FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
	#define AttachToPhysicalUsingPhysics(...) DebugAttachToPhysicalUsingPhysics(__FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
	#define AttachToPhysicalUsingPhysics(...) DebugAttachToPhysicalUsingPhysics(__FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
	#define AttachToWorldUsingPhysics(...) DebugAttachToWorldUsingPhysics(__FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

	void DebugProcessAllAttachments(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine);		// Process this physical and recursively process all children and siblings
	virtual void DebugProcessAttachment(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine);	// Process THIS physical's attachment stuff
private:
	void DebugProcessAttachmentImp(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine);

public:
	virtual void DebugAttachToPhysicalBasic(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine, CPhysical* pPhysical, s16 nEntBone, u32 nAttachFlags, const Vector3* pVecOffset, const Quaternion* pQuatOrientation, s16 nMyBone = -1);
	void DebugAttachToWorldBasic(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine, u32 nAttachFlags, const Vector3& vecWorldPos, const Quaternion* pQuatOrientation);
	bool DebugAttachToPhysicalUsingPhysics(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine, CPhysical* pPhysical, s16 nEntBone, s16 nMyBone, u32 nAttachFlags, const Vector3* pEntAnchorLocalSpace, const Quaternion* pQuatOrientation, const Vector3* pMyAnchorLocalSpace, float fStrength, bool bAllowInitialSeparation = true, float massInvScaleA = 1.0f, float massInvScaleB = 1.0f);
	bool DebugAttachToPhysicalUsingPhysics(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine, CPhysical* pPhysical, s16 nEntBone, s16 nMyBone, u32 nAttachFlags, const Vector3& vecWorldPos, float fStrength);
	bool DebugAttachToWorldUsingPhysics(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine, s16 nMyBone, bool bRotConstraint, const Vector3& vecWorldPos, const Vector3* pComponentSpacePos, float fStrength, bool bAutoActivateOnDetach = false, const Vector3* vecRotMinLimits = NULL, const Vector3* vecRotMaxLimits = NULL);

#else // __BANK
public:
	void ProcessAllAttachments();		// Process this physical and recursively process all children and siblings
	virtual void ProcessAttachment();	// Process THIS physical's attachment stuff
private:
	void ProcessAttachmentImp();

public:
	virtual void AttachToPhysicalBasic(CPhysical* pPhysical, s16 nEntBone, u32 nAttachFlags, const Vector3* pVecOffset, const Quaternion* pQuatOrientation, s16 nMyBone = -1 );
	void AttachToWorldBasic(u32 nAttachFlags, const Vector3& vecWorldPos, const Quaternion* pQuatOrientation);
	bool AttachToPhysicalUsingPhysics(CPhysical* pPhysical, s16 nEntBone, s16 nMyBone, u32 nAttachFlags, const Vector3* pEntAnchorLocalSpace, const Quaternion* pQuatOrientation, const Vector3* pMyAnchorLocalSpace, float fStrength, bool bAllowInitialSeparation = true, float massInvScaleA = 1.0f, float massInvScaleB = 1.0f);
	bool AttachToPhysicalUsingPhysics(CPhysical* pPhysical, s16 nEntBone, s16 nMyBone, u32 nAttachFlags, const Vector3& vecWorldPos, float fStrength);

	//pComponentSpacePos: If this is not specified, the point of the component currently at vecWorldPos is constrained to not move
	//					  If this is specified, then this point (specified in the local space of the component) will be attached to the world pos
	bool AttachToWorldUsingPhysics(s16 nMyBone, bool bRotConstraint, const Vector3& vecWorldPos, const Vector3* pComponentSpacePos, float fStrength, bool bAutoActivateOnDetach = false, const Vector3* vecRotMinLimits = NULL, const Vector3* vecRotMaxLimits = NULL);
	
#endif

	bool GetGlobalAttachMatrix(Mat34V_Ref attachMatInOut) const;
	inline Vector3 GetAttachOffset() const;
	inline void SetAttachOffset(Vector3::Vector3Param vOffset);
	inline Quaternion GetAttachQuat() const;
	inline void SetAttachQuat(const Quaternion& quat);
	inline s16 GetAttachBone() const;
	inline bool GetAttachFlag(eAttachFlags nFlag) const;
	inline u32 GetAttachFlags();
	inline void SetAttachFlag(eAttachFlags nFlag, bool bOn);
	inline void SetAttachFlags(u32 attachFlags);
	inline int GetAttachState() const;
	inline void SetAttachState(int nAttachState);
	inline bool GetIsAttached() const;
	inline bool GetIsAttachedInCar() const;
	inline bool GetIsAttachedOnMount() const;
	inline bool GetIsAttachedToGround() const;
	inline bool GetShouldProcessAttached();
	inline bool GetIsRootParentAttachment();

	inline fwEntity* GetAttachParent() const;
	inline fwEntity* GetAttachParentForced() const;

	inline fwEntity* GetSiblingAttachment() const;
	inline fwEntity* GetChildAttachment() const;

	bool IsEntityAParentAttachment(const fwEntity* entity) const;

	// If this CPhysical is associated with a fragInst then a part with an attachment could break off into a new physics instance and
	// we will need to fix this up. This callback allows user defined behaviour such as switching the attachment to the new inst or
	// simply detaching it.
	void AttachParentPartBrokeOff(CPhysical* pNewParent);

	// Start detaching from an entity
	virtual void DetachFromParent(u16 nDetachFlags);	
	virtual void DetachFromParentAndChildren(u16 nDetachFlags);

	//Remove children and children's children and so on that will be deleted with this by setting the pointers to NULL)
	void RemoveDeleteWithParentProgenyFromList(CEntity * list[], u32 count) const;

	// Other attachments - These are attachments maintained by a derived class
	virtual void ProcessOtherAttachments() { Assertf(0,"CPhysical doesn't do anything with other attachments."); }

	// fill in the provided storage array with collision exclusions, up to the provided max storage
	// returns the actual number of exclusions
	virtual void GeneratePhysExclusionList(const CEntity** ppExclusionListStorage, int& nStorageUsed, int nMaxStorageSize, u32 includeFlags, u32 typeFlags, const CEntity* pOptionalExclude=NULL) const;

	bool DoRelationshipGroupsMatchForOnlyDamagedByCheck(const CRelationshipGroup* pRelGroupOfInflictor) const;
	virtual bool CanPhysicalBeDamaged(const CEntity* pInflictor, u32 nWeaponUsedHash, bool bDoNetworkCloneCheck = true, bool bDisableDamagingOwner = false NOTFINAL_ONLY(, u32* uRejectionReason = NULL)) const;

	float GetDistanceFromCentreOfMassToBaseOfModel(void) const;
	virtual void UpdatePossiblyTouchesWaterFlag();

	// Converts a script entity to a random entity.
	virtual void CleanupMissionState();
	
	// Ground API
	static bool DoesGroundCauseRelativeMotion(const CPhysical* pGround);
	static void CalculateGroundVelocity(const CPhysical* pGround, Vec3V_In vPos, int nComponent, Vec3V_InOut vVel);
	static void CalculateGroundVelocityIntegrated(const CPhysical* pGround, Vec3V_In vPos, int nComponent, float fTimeStep, Vec3V_InOut vVel);
	void CalculateGroundAngularVelocity(const CPhysical* pGround, int nComponent, Vec3V_Ref vVel) const;
	static CPhysical* GetGroundPhysicalForPhysical(const CPhysical& rPhysical);
	static CPhysical* GetRootGroundPhysicalForPhysical(const CPhysical& rPhysical);

	void UpdateFixedWaitingForCollision(bool isBeingUpdatedBySimulator);
#if DR_ENABLED
	typedef void (*RecordEntityAttachmentFunc)(const phInst *pAttached, const phInst *pAttachedTo, const Matrix34 &matNew);
	static bool smb_RecordAttachments;
	static RecordEntityAttachmentFunc sm_RecordAttachmentsFunc;
#endif
protected:
	// removeAndAdd used for moving (warp resets last matrix too so no collisions inbetween positions)
	virtual void UpdatePhysicsFromEntity(bool bWarp=false);
	
private:
	//// private attachment functions... these should not need to be called by anything but the attachment processing functions

	void DeleteAllChildren(bool bCheckAttachFlags = true);		

	// Fixed waiting for collision update
	virtual bool ShouldFixIfNoCollisionLoadedAroundPosition() {return false;}
	virtual void OnFixIfNoCollisionLoadedAroundPosition(bool UNUSED_PARAM(bFix)) {}
	bool IsMountEntryExitAttachment() const;
	bool IsVehicleEntryExitAttachment() const;

#if __ASSERT
public:
	// Trying to catch a case where a plane would have fixed in place while waiting for collision to load.
	static bool sm_enableNoCollisionPlaneFixDebug;

	void DumpOutOfWorldInfo();

private:

	// Check whether the offset from the entity's transform is within the centroid's radius
	bool IsOffsetWithinCentroidRadius(const char* msg, const Vector3& vecOffset, float fCentroidRadiusScale = 1.1f) const;
#endif // __ASSERT

protected:
	virtual bool IsCollisionLoadedAroundPosition();

#if __ASSERT
public:	
	static void PrintRegisteredPhysicsCreationBacktrace(CPhysical* pPhysical);
	static void RegisterPhysicsCreationBacktrace(CPhysical* pPhysical);
#endif

#if GTA_REPLAY
public:
	void	SetReplayID(const CReplayID& id)		{	m_replayID = id;	}
	const CReplayID& GetReplayID() const			{	return m_replayID;	}
private:
	CReplayID		m_replayID;
#endif // GTA_REPLAY
};

#define USE_PHYSICAL_HISTORY 0

#if USE_PHYSICAL_HISTORY
class CPhysicalHistory
{
public:
	CPhysicalHistory()
		: m_iHistoryStart(0),
		m_iHistorySize(0)
	{

	}

	void UpdateHistory( Matrix34& mThisMat, CEntity* pEntity, Color32 colour );
	void RenderHistory( CEntity* pEntity );
private:
	enum			{MAX_HISTORY_SIZE = 50};
	static float	HISTORY_FREQUENCY_MIN;
	static float	HISTORY_FREQUENCY_MAX;
	Matrix34		m_amHistory[MAX_HISTORY_SIZE];
	Color32			m_aColors[MAX_HISTORY_SIZE];
	s32			m_iHistoryStart;
	s32			m_iHistorySize;
};
#endif

inline Vector3 CPhysical::GetAttachOffset() const
{
	if (const fwAttachmentEntityExtension *extension = GetAttachmentExtension())
	{
		return extension->GetAttachOffset();
	}

	return VEC3_ZERO;
}

inline void CPhysical::SetAttachOffset(Vector3::Vector3Param vOffset)
{
	CreateAttachmentExtensionIfMissing()->SetAttachOffset(vOffset);
}

inline Quaternion CPhysical::GetAttachQuat() const
{
	if (const fwAttachmentEntityExtension *extension = GetAttachmentExtension())
	{
		return extension->GetAttachQuat();
	}

	Quaternion identityQuat;
	identityQuat.Identity();

	return identityQuat;
}

inline void CPhysical::SetAttachQuat(const Quaternion& quat)
{
	CreateAttachmentExtensionIfMissing()->SetAttachQuat(quat);
}

inline s16 CPhysical::GetAttachBone() const
{
	if (const fwAttachmentEntityExtension *extension = GetAttachmentExtension())
	{
		return extension->GetAttachBone();
	}

	return -1;
}

inline bool CPhysical::GetAttachFlag(eAttachFlags nFlag) const
{
	if (const fwAttachmentEntityExtension *extension = GetAttachmentExtension())
	{
		return extension->GetAttachFlag(nFlag);
	}

	return false;
}

inline u32 CPhysical::GetAttachFlags()
{
	if (const fwAttachmentEntityExtension *extension = GetAttachmentExtension())
	{
		return extension->GetAttachFlags();
	}

	return 0;
}

inline void CPhysical::SetAttachFlag(eAttachFlags nFlag, bool bOn)
{
	CreateAttachmentExtensionIfMissing()->SetAttachFlag(nFlag, bOn);
}

inline void CPhysical::SetAttachFlags(u32 attachFlags)
{
	CreateAttachmentExtensionIfMissing()->SetAttachFlags(attachFlags);
}


inline int CPhysical::GetAttachState() const
{
	if (const fwAttachmentEntityExtension *extension = GetAttachmentExtension())
	{
		return extension->GetAttachState();
	}

	return 0;
}

inline void CPhysical::SetAttachState(int nAttachState)
{
	CreateAttachmentExtensionIfMissing()->SetAttachState(nAttachState);
}

inline bool CPhysical::GetIsAttached() const
{
	if (const fwAttachmentEntityExtension *extension = GetAttachmentExtension())
	{
		return extension->GetIsAttached();
	}

	return false;
}

inline bool CPhysical::GetIsAttachedInCar() const
{
	if (const fwAttachmentEntityExtension *extension = GetAttachmentExtension())
	{
		return extension->GetIsAttachedInCar();
	}

	return false;
}

inline bool CPhysical::GetIsAttachedOnMount() const
{
	if (const fwAttachmentEntityExtension *extension = GetAttachmentExtension())
	{
		return extension->GetIsAttachedOnMount();
	}

	return false;
}

inline bool CPhysical::GetIsAttachedToGround() const
{
	if (const fwAttachmentEntityExtension *extension = GetAttachmentExtension())
	{
		return extension->GetIsAttachedToGround();
	}

	return false;
}

inline bool CPhysical::GetShouldProcessAttached()
{
	if (const fwAttachmentEntityExtension *extension = GetAttachmentExtension())
	{
		return extension->GetShouldProcessAttached();
	}

	return false;
}

inline bool CPhysical::GetIsRootParentAttachment()
{
	if (const fwAttachmentEntityExtension *extension = GetAttachmentExtension())
	{
		return extension->GetIsRootParentAttachment();
	}

	return false;
}

inline fwEntity* CPhysical::GetAttachParent() const
{
	if (const fwAttachmentEntityExtension *extension = GetAttachmentExtension())
	{
		return extension->GetAttachParent();
	}

	return NULL;
}


inline fwEntity* CPhysical::GetAttachParentForced() const
{
	if (const fwAttachmentEntityExtension *extension = GetAttachmentExtension())
	{
		return extension->GetAttachParentForced();
	}

	return NULL;
}

inline fwEntity* CPhysical::GetSiblingAttachment() const
{
	if (const fwAttachmentEntityExtension *extension = GetAttachmentExtension())
	{
		return extension->GetSiblingAttachment();
	}

	return NULL;
}

inline fwEntity* CPhysical::GetChildAttachment() const
{
	if (const fwAttachmentEntityExtension *extension = GetAttachmentExtension())
	{
		return extension->GetChildAttachment();
	}

	return NULL;
}

inline void CPhysical::UpdateFixedWaitingForCollision(bool isBeingUpdatedBySimulator)
{
	// Update the fixed waiting for collision flag, this is done directly before the physics update so it
	// is current for the simulator update.
#if __ASSERT
	CPhysical::sm_enableNoCollisionPlaneFixDebug = true;
#endif

	if ( IsProtectedBaseFlagSet(fwEntity::IS_FIXED_UNTIL_COLLISION) )
	{
		if ( IsCollisionLoadedAroundPosition() || !ShouldFixIfNoCollisionLoadedAroundPosition() )
		{
			SetIsFixedWaitingForCollision(false);
		}

	}
	// If an object isn't being updated by the simulator then it shouldn't rely on map collision
	//   to not fall through the world.
	else if(isBeingUpdatedBySimulator && ShouldFixIfNoCollisionLoadedAroundPosition() )
	{
		if ( !IsCollisionLoadedAroundPosition() )
		{
			SetIsFixedWaitingForCollision(true);
		}
	}

#if __ASSERT
	CPhysical::sm_enableNoCollisionPlaneFixDebug = false;
#endif
}

#endif

