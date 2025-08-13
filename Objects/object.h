//
// Title	:	Object.h
// Author	:	Richard Jobling
// Started	:	27/01/00
//
//
//
//
//
//
//
#ifndef _OBJECT_H_
#define _OBJECT_H_

#if !__SPU
	#include "fwtl/pool.h"

	#include "audio/objectaudioentity.h"
	#include "Pathserver/PathServer_DataTypes.h"
	#include "physics/Deformable.h"
	#include "Scene/Physical.h"
	#include "scene/RegdRefTypes.h"
	#include "system/noncopyable.h"
	#include "task/Combat/Cover/Cover.h"

	// network headers
	#include "network/Objects/entities/NetObjObject.h"
	#include "objects/objectintelligence.h"
#endif //!__SPU...

class CMoveObject;
class CWeapon;
class CWeaponComponent;
class CProjectile;
class CProjectileRocket;
class CProjectileThrown;
class CCoverTuning;
class CCustomShaderEffectTint;

//
//
//
//
struct CObjectFlags
{
	u32 bIsPickUp								: 1;
	u32 bIsDoor									: 1;
	u32 bHasBeenUprooted						: 1;
	u32 bCanBeTargettedByPlayer					: 1;	// true if this object can be locked on by the player
	u32 bIsATargetPriority						: 1;	// If set to targetable then this object is higher priority target than normal
	u32 bTargettableWithNoLos					: 1;	// If set to targetable then this object can be targeted with no line of sight

	u32 bIsStealable							: 1;	// objects stealable in the procedural interior code
	u32 bVehiclePart							: 1;	// this object is a bit that broke of a vehicle - it requires the vehicle to still exist to render
	u32 bPedProp								: 1;	// this object is a ped prop that needs to be destroyed when the ped is removed
	u32 bFadeOut								: 1;
	u32 bRemoveFromWorldWhenNotVisible			: 1;

	u32 bMovedInNetworkGame						: 1;
	u32 bBoundsNeedRecalculatingForNavigation	: 1;	// Set on an object when it fragments, so that its navigation bounds can be recalculated

	u32 bFloater								: 1;	// object is created on the water, so always floats and stays active when ever the player is near
	u32 bWeaponWillFireOnImpact					: 1;
	u32 bAmbientProp							: 1;	// if this object is an ambient prop
	u32 bDetachedPedProp						: 1;	// is a prop object which has been detached from a ped
	u32 bFixedForSmallCollisions				: 1;	// if this object uses fixed physics until it collides with high mass objects and vehicles
	u32 bSkipTempObjectRemovalChecks			: 1;	// allows temp object to be deleted when near/onscreen
	u32 ScriptBrainStatus						: 3;

	u32 bWeWantOwnership						: 1;	// Used in network game
	u32 bActivatePhysicsAsSoonAsUnfrozen		: 1;	// Set by script so that objects placed on slopes will drop to correct position as soon as possible
	u32 bCarWheel								: 1;	// need to identify wheels that fall off cars so they render correctly
	u32 bCanBePickedUp							: 1;	// certain objects can't be picked up
	u32 bIsHelmet								: 1;	// object is a helmet
	u32 bIsRageEnvCloth							: 1;	// object is a rage environment cloth attached to moving entity
	u32 bOnlyCleanUpWhenOutOfRange				: 1;	// clean up when out of range only
	u32 bPopTires								: 1;	// causes tires to pop when collided with
	u32 bHasInitPhysics							: 1;	// the object has initialized physics
	u32 bHasAddedPhysics						: 1;	// the object has added physics

	u32 bIsVehicleGadgetPart					: 1;	// The object is part of a vehicle gadget. Used by network to ensure the owner of the vehicle and the owner of the vehicle gadget parts are consistent.
	u32	bIsNetworkedFragment					: 1;	// The object is a fragment that is being networked
};
//CompileTimeAssert(sizeof(CObjectFlags) == (sizeof(u32) + 1));



#if !__SPU
#define SIZE_LARGEST_OBJECT_CLASS	1024

#define TEMPOBJ_1ST_LIMIT 10
#define TEMPOBJ_1ST_LIFE_MULT (0.5f)
#define TEMPOBJ_2ND_LIMIT 20
#define TEMPOBJ_2ND_LIFE_MULT (0.2f)
#define TEMPOBJ_MAX_NO 150

#define OBJ_HIT			(0)
#define OBJ_DESTROYED	(1)

//#define DO_KEEPYUPPY

//
// damage flags from "object.dat":
//
enum
{
	D_EFFECT_NONE							=0,
	D_EFFECT_CHANGE_MODEL					=1,
	D_EFFECT_SMASH_COMPLETELY				=20,
	D_EFFECT_CHANGE_THEN_SMASH				=21,
	D_EFFECT_BREAKABLE						=200,	
	D_EFFECT_BREAKABLE_REMOVE				=202,
};


//
// special cases of collision models or detection ("object.dat"):
//
//0:none(default) 1:lampost 2:smallbox 3:bigbox 4:fencepart
enum {
	OB_COL_DEFAULT		=0,
	OB_COL_LPOST		=1,
	OB_COL_SMALLBOX		=2,
	OB_COL_BIGBOX		=3,
	OB_COL_FENCE		=4,
	OB_COL_GRENADE		=5,
	OB_COL_SWINGDOOR	=6,
	OB_COL_LOCKDOOR		=7,
	OB_COL_HANGING		=8,
	OB_COL_POOLBALL		=9
};
	
enum {
		CAMERA_DISALLOW_COLLISION,
		CAMERA_ALLOW_COLLISION_PED_ONLY,
		CAMERA_ALLOW_COLLISION,
		CAMERA_ALLOW_COLLISION_VEHICLE_ONLY	 
/*
	  CAMERA_AVOID_THIS=0,			// Nothing Collides ( when this enum says avoid - it means avoid allowing collision with this )
	  CAMERA_AVOID_VEHICLE_ONLY, 	// Only Cars !!!avoided!!!	  ... This means only peds will be allowed to collide with this object
	  CAMERA_DONT_AVOID,			// Everything collides 
	  CAMERA_AVOID_PED_ONLY			// Only Peds !!!avoided!!!    ... This means only vehicles will be allowed to collide with this object
*/
	 };


#define OBJ_MESSAGE_AMOUNT_MULT (5)

#define MAX_OBJECT_COVER_BOUNDING_BOX_WIDTH (16.0f)
#define MIN_OBJECT_COVER_BOUNDING_BOX_HEIGHT (0.58f)
#define MIN_OBJECT_COVER_BOUNDING_BOX_WIDTH (0.2f)


class CFire;
namespace rage
{
class audSound;
}



//
//
//
//
class CObject : public CPhysical
{
	DECLARE_RTTI_DERIVED_CLASS(CObject, CPhysical);

private:
    friend struct CObjectOffsetInfo;
	friend class CObjectPopulation;
	NON_COPYABLE(CObject);

protected:
	CObject(const eEntityOwnedBy ownedBy);
	CObject(const eEntityOwnedBy ownedBy, class CDummyObject* pDummy);
	explicit CObject(const eEntityOwnedBy ownedBy, u32 nModelIndex, bool bCreateDrawable = true, bool bInitPhys = true, bool bCalcAmbientScales = true);
	~CObject();

public:

	enum
	{
		OBJECT_DOESNT_USE_SCRIPT_BRAIN = 0, 
		OBJECT_WAITING_TILL_IN_BRAIN_RANGE,		//	used to be called OBJECT_SCRIPT_BRAIN_NOT_LOADED, 
		OBJECT_WAITING_FOR_SCRIPT_BRAIN_TO_LOAD, 
		OBJECT_WAITING_TILL_OUT_OF_BRAIN_RANGE,
		OBJECT_RUNNING_SCRIPT_BRAIN 
	};

public:

	FW_REGISTER_CLASS_POOL(CObject);

	// Use CObjectPopulation::CreateObject to make new objects

	virtual void SetModelId(fwModelId modelId);

	virtual bool CreateDrawable();
	virtual void DeleteDrawable();
	virtual int  InitPhys();
	virtual void AddPhysics();
	virtual void RemovePhysics();

	// If object is a frag this repairs it back to its original state
	void Fix();

	void Init();

	virtual void UpdateRenderFlags();

	bool CanBeDeleted(void) const;

	CDummyObject* GetRelatedDummy() const { return m_pRelatedDummy;}
	fwInteriorLocation GetRelatedDummyLocation() { return(m_relatedDummyLocation); }
	void SetRelatedDummy(CDummyObject* pDummy, fwInteriorLocation loc);
	void ClearRelatedDummy();

	CMoveObject &GetMoveObject()			{ Assert(GetAnimDirector()); return (CMoveObject &) (GetAnimDirector()->GetMove()); }
	const CMoveObject&GetMoveObject()const	 { Assert(GetAnimDirector()); return (CMoveObject &) (GetAnimDirector()->GetMove()); }

	CEntity* GetFragParent() {return m_pFragParent;}
	const CEntity* GetFragParent() const {return m_pFragParent;}
	void SetFragParent(CEntity* pEnt);
	void ClearFragParent();

	void InstantAiUpdate();
	void InstantAnimUpdate();

	virtual CDynamicEntity*	GetProcessControlOrderParent() const;

	virtual u32 GetStartAnimSceneUpdateFlag() const;

	virtual void OnActivate(phInst* pInst, phInst* pOtherInst);
	
	virtual bool RequiresProcessControl() const;
	virtual bool ProcessControl();
	virtual ePhysicsResult ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice);
	
	virtual void ProcessPreComputeImpacts(phContactIterator);

	virtual void ProcessCollision(phInst const * myInst, CEntity* pHitEnt, phInst const* hitInst, const Vector3& vMyHitPos,
		const Vector3& vOtherHitPos, float fImpulseMag, const Vector3& vMyNormal, int iMyComponent, int iOtherComponent,
		phMaterialMgr::Id iOtherMaterial, bool bIsPositiveDepth, bool bIsNewContact);
	
	virtual ePrerenderStatus PreRender(const bool bIsVisibleInMainViewport = true);
	virtual void PreRender2(const bool bIsVisibleInMainViewport = true);

	virtual void FlagToDestroyWhenNextProcessed();

	void ProcessControlLogic();
	static void SetMatrixForTrainCrossing(Matrix34 *pMatr, float Angle);
	void ProcessTrainCrossingBehaviour();
	virtual bool ObjectDamage(float fImpulse, const Vector3* pColPos, const Vector3* pColNormal, CEntity *pEntityResponsible, u32 nWeaponUsedHash, phMaterialMgr::Id hitMaterial = 0);

	bool isRageEnvCloth() const { return m_nObjectFlags.bIsRageEnvCloth;}
	bool IsObjectDamaged() const { if( m_nFlags.bRenderDamaged==true || GetIsVisible()==false) return true; return false; }
#if GTA_REPLAY
	bool IsRenderDamaged() { return m_nFlags.bRenderDamaged; }
	void SetRenderDamaged(bool isDamaged) { m_nFlags.bRenderDamaged = isDamaged; } 
#endif
	void SetObjectTargettable(bool bTargettable);
	bool CanBeTargetted(void) const;
	
	virtual void Teleport(const Vector3& vecSetCoors, float fSetHeading=-10.0f, bool bCalledByPedTask=false, bool bTriggerPortalRescan = true, bool bCalledByPedTask2=false, bool bWarp=true, bool bKeepRagdoll=false, bool bResetPlants=true); // bCalledByPedTask, bKeepRagdoll is only used in the ped Teleport function

	void RemoveNextFrame();
																											
	void ObjectHasBeenMoved(CEntity* pMover, bool bRegisterWithNoMover = false);
	static void NetworkRegisterAllObjects();

	bool CanBeUsedToTakeCoverBehind();
	// Get the last cover orientation
	CoverOrientation GetCoverOrientation() { return m_eLastCoverOrientation; }
	void SetCoverOrientation(CoverOrientation eOrientation ) { m_eLastCoverOrientation = eOrientation; }

#if !__NO_OUTPUT
	static void PrintSkeletonData();
#endif
#if !__FINAL
	static void DumpObjectPool();
#endif

	void DecoupleFromMap();

	// Fixed waiting for collision update
	virtual bool ShouldFixIfNoCollisionLoadedAroundPosition();
	virtual void OnFixIfNoCollisionLoadedAroundPosition(bool bFix);

    static CEntity *GetPointerToClosestMapObjectForScript(float NewX, float NewY, float NewZ, float Radius, int ObjectModelHashKey, s32 TypeFlags = ENTITY_TYPE_MASK_BUILDING|ENTITY_TYPE_MASK_OBJECT|ENTITY_TYPE_MASK_DUMMY_OBJECT, s32 OwnedByFlags = 0xFFFFFFFF);
	
	CWeapon* GetWeapon() { return m_pWeapon; }
	const CWeapon* GetWeapon() const { return m_pWeapon; }
	void SetWeapon(CWeapon* pWeapon);

	CWeaponComponent* GetWeaponComponent() { return m_pWeaponComponent; }
	const CWeaponComponent* GetWeaponComponent() const { return m_pWeaponComponent; }
	void SetWeaponComponent(CWeaponComponent* pWeapon);

	virtual float GetLockOnTargetableDistance() const { return m_fTargetableDistance; }
	virtual void SetLockOnTargetableDistance(float fTargetableDistance) { m_fTargetableDistance = fTargetableDistance; }

	audPlaceableTracker* GetPlaceableTracker()
	{
		return &m_PlaceableTracker;
	}

	const audPlaceableTracker* GetPlaceableTracker() const
	{
		return &m_PlaceableTracker;
	}

	audObjectAudioEntity* GetObjectAudioEntity() { return &m_AudioEntity; }
	const audObjectAudioEntity* GetObjectAudioEntity() const { return &m_AudioEntity; }
	
	virtual void SetHasExploded(bool *bNetworkExplosion);

	void MakeBoneSwayWithWind(C2dEffect* pEffect);

	// Overridden so we can update the skeleton on animated objects when running drive-to-pose
	void SetActivePoseFromSkel();

	// Get the lock on position for this object
	virtual void GetLockOnTargetAimAtPos(Vector3& aimAtPos) const;

	virtual bool PlaceOnGroundProperly(float fMaxRange = 10.0f, bool bAlign = true, float fHeightOffGround = 0.0f, bool bIncludeWater = false, bool bUpright = false, bool *pInWater = NULL, bool bIncludeObjects = false, bool useExtendedProbe = false);

	inline TDynamicObjectIndex GetPathServerDynamicObjectIndex() const { return m_iPathServerDynObjId; }
	inline void SetPathServerDynamicObjectIndex(const TDynamicObjectIndex i) { m_iPathServerDynObjId = i; }

	virtual bool IsAScriptEntity() const { return GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT; }
	virtual void SetupMissionState();
	virtual void CleanupMissionState();

#if DEFORMABLE_OBJECTS
	virtual bool HasDeformableData() const { return m_pDeformableData ? true : false; }
	virtual void SetDeformableData(CDeformableObjectEntry* ptr) { m_pDeformableData = ptr; }
	virtual const CDeformableObjectEntry* GetDeformableData() const { return m_pDeformableData; }
#endif // DEFORMABLE_OBJECTS

	//
	// Task control and intelligence
	//

	const CObjectIntelligence * GetObjectIntelligence() const	{ return m_pIntelligence; }

	// Task interface (lazily create supporting classes as needed)
	CTask* GetTask(u32 const tree = CObjectIntelligence::OBJECT_TASK_TREE_PRIMARY) const;
	bool SetTask(aiTask* pTask, u32 const tree = CObjectIntelligence::OBJECT_TASK_TREE_PRIMARY, u32 const priority = CObjectIntelligence::OBJECT_TASK_PRIORITY_PRIMARY);
	bool InitAnimLazy();

	void ForcePostCameraAiUpdate(bool bForce) 
	{ 
		m_bForcePostCameraAiUpdate = bForce;
		AddToOrRemoveFromProcessPostCameraArray();
	}
	void ForcePostCameraAnimUpdate(bool bForce, bool bUseZeroTimeStep) 
	{ 
		if (bUseZeroTimeStep)
		{
			m_bForcePostCameraAnimUpdateUseZeroTimeStep = bForce;
		}
		m_bForcePostCameraAnimUpdate = bForce;
		AddToOrRemoveFromProcessPostCameraArray();
	}

	// Object Intelligence (optional component)
	CObjectIntelligence * GetObjectIntelligence()				{ return m_pIntelligence; }
	
	inline CPhysical* GetGroundPhysical() const { return m_pGroundPhysical; }

	bool NeedsProcessPostCamera() const;
	void AddToOrRemoveFromProcessPostCameraArray();
	void ProcessPostCamera();

	void ProcessSniperDamage();
	u32 GetDamageScanCode() const { return m_damageScanCode; }
	bool RecentlySniped() const { return (m_damageScanCode!=INVALID_DAMAGE_SCANCODE) && (MAX_OBJECTS_DAMAGED_BY_SNIPER > (ms_globalDamagedScanCode-m_damageScanCode)); }
	static u32 GetCurrentDamageScanCode() { return ms_globalDamagedScanCode; }

	void ScaleObject(float fScale);

	virtual const Vector3& GetVelocity() const;

	bool IsAvoidingCollision() const { return CPhysics::GetTimeSliceId() <= m_nPhysicsTimeSliceToAvoidCollision; }
	virtual bool ShouldAvoidCollision(const CPhysical* pOtherPhysical);

	static void ResetStartTimerForCollisionAgitatedEvent() { ms_uStartTimerForCollisionAgitatedEvent = 0; }

	bool GetDisablesPushCollisions() const { return m_nObjectFlags.bPedProp || m_nObjectFlags.bDetachedPedProp || m_nObjectFlags.bAmbientProp || GetAsProjectile(); }
	bool GetShouldBeIgnoredForConstrictingCollisionChecks() const { return GetDisablesPushCollisions() || IsPickup(); }

	void ResetMovedInNetworkGame() { m_nObjectFlags.bMovedInNetworkGame = false; m_bRegisteredInNetworkGame = false; }

	CCustomShaderEffectTint* GetCseTintFromObject();

	bool GetWeaponImpactsApplyGreaterForce() { return m_bWeaponImpactsApplyGreaterForce; }
	void SetWeaponImpactsApplyGreaterForce( bool applyGreaterForce ) { m_bWeaponImpactsApplyGreaterForce = applyGreaterForce; }

	void PlayAutoStartAnim();

protected:

#if __BANK
	#define ProcessAttachment(...) DebugProcessAttachment(__FILE__, __FUNCTION__, __LINE__)
	virtual void DebugProcessAttachment(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine);
#else // __BANK
	virtual void ProcessAttachment();	// Process THIS physical's attachment stuff
#endif

	// removeAndAdd used for moving (warp resets last matrix too so no collisions in between positions)

	virtual fwDrawData* AllocateDrawHandler(rmcDrawable* pDrawable);

	void AttachCustomBoundsFromExtension();

public:

#if __BANK
	static void InitWidgets();
	static void CreateDoorTuningWidgetsCB();
	static void CreateCoverTuningWidgetsCB();
#endif

	//
	// Types
	//

	virtual const CProjectile* GetAsProjectile() const { return NULL; }
	virtual CProjectile* GetAsProjectile() { return NULL; }
	virtual const CProjectileRocket* GetAsProjectileRocket() const { return NULL; }
	virtual CProjectileRocket* GetAsProjectileRocket() { return NULL; }
	virtual const CProjectileThrown* GetAsProjectileThrown() const { return NULL; }
	virtual CProjectileThrown* GetAsProjectileThrown() { return NULL; }

	bool IsADoor() const					{ return m_nObjectFlags.bIsDoor; }
	bool IsPickup() const					{ return m_nObjectFlags.bIsPickUp; }
	bool CanShootThroughWhenEquipped() const { return m_bShootThroughWhenEquipped;}
	void SetCanShootThroughWhenEquipped(bool bSet) {m_bShootThroughWhenEquipped = bSet;}
	bool GetFixedByScriptOrCode() const { return m_bFixedByScriptOrCode; }
	void SetFixedByScriptOrCode(bool b) { m_bFixedByScriptOrCode = b; }
	bool GetForceVehiclesToAvoid() const {return m_bForceVehiclesToAvoid;}
	void SetForceVehiclesToAvoid(bool bAvoid) {m_bForceVehiclesToAvoid = bAvoid;}
	void SetHasNetworkedFrags()			{ m_bHasNetworkedFrags = true; }
	bool HasNetworkedFrags() const		{ return m_bHasNetworkedFrags; }
	void SetDisableObjectMapCollision(bool b) { m_bDisableObjectMapCollision = b; AddToOrRemoveFromProcessPostCameraArray(); }
	bool GetDisableObjectMapCollision() const { return m_bDisableObjectMapCollision; }
	void SetAttachedToHandler(bool b) { m_bIsAttachedToHandler = b; }
	bool IsAttachedToHandler() const { return m_bIsAttachedToHandler; }
	void SetTakesDamageFromCollisionsWithBuildings(bool b) { m_bTakesDamageFromCollisionsWithBuildings = b; }
	bool GetTakesDamageFromCollisionsWithBuildings() const { return m_bTakesDamageFromCollisionsWithBuildings; }
	bool GetIsParachute() const { return m_bIsParachute; }
	void SetIsParachute(bool b) { m_bIsParachute = b; }
	bool GetIsCarParachute() const { return m_bIsCarParachute; }
	void SetIsCarParachute(bool b) { m_bIsCarParachute = b; }
	bool GetUseLightOverride() const { return m_bUseLightOverride; }
	void SetUseLightOverride(bool b) { m_bUseLightOverride = b; }
    bool GetProjectilesShouldExplodeOnImpact() const { return m_bProjectilesExplodeOnImpact; }
    void SetProjectilesShouldExplodeOnImpact(bool b) { m_bProjectilesExplodeOnImpact = b; }
	bool GetIsAlbedoAlpha() const	{ return m_bIsAlbedoAlpha;}
	void SetIsAlbedoAlpha(bool b)	{ m_bIsAlbedoAlpha = b; }


	bool GetIsLeakedObject() const { return m_bIntentionallyLeakedObject; }

	s8 GetVehicleModSlot() const { return m_vehModSlot; }
	void SetVehicleModSlot(s8 s)	{ m_vehModSlot = s; }

	u8 GetSpeedBoost() const { return m_speedBoost; }
	u8 GetSpeedBoostDuration() const { return m_speedBoostDuration; }
	
	void SetSpeedBoost( u8 speedBoost ) { m_speedBoost = speedBoost; }
	void SetSpeedBoostDuration( u8 duration ) { m_speedBoostDuration = duration; }

	void DriveToJointToTarget();
//	void SnapJointToTargetRatio( int jointIndex, float fTargetOpenRatio );
	void SetDriveToTarget( bool maxAngle, int jointIndex = 0 );
	void CreateConstraintForSetDriveToTarget();
	void DriveVelocity();
	void DriveJoint( fragHierarchyInst* pHierInst, int jointIndex );
	bool IsPressurePlatePressed();
	bool IsJointAtMinAngle( int jointIndex );
    bool IsJointAtMaxAngle(int jointIndex);
	bool IsObjectAPressurePlate() const {	return m_bIsPressurePlate; }
	void SetObjectIsAPressurePlate( bool isPressurePlate ) { m_bIsPressurePlate = isPressurePlate; }
	void SetObjectIsArticulated( bool isArticulated ) {	m_bIsArticulatedProp = isArticulated; }
    bool IsObjectArticulated() const { return m_bIsArticulatedProp; }
	void SetObjectIsBall( bool isBall );
	bool GetObjectIsBall() { return m_bIsBall; }

	u8 GetJointToDriveIndex() const { return m_jointToDriveIndex; }
	bool IsDriveToMaxAngleEnabled() const { return m_bDriveToMaxAngle; }
	bool IsDriveToMinAngleEnabled() const { return m_bDriveToMinAngle; }
	void UpdateDriveToTargetFromNetwork(u8 jointToDriveIdx, bool bDriveToMinAngle, bool bDriveToMaxAngle);

	void ProcessBallCollision( CEntity* pHitEnt, const Vector3& vMyHitPos, float fImpulseMag, const Vector3& vMyNormal );
	void SetDamageInflictor( CEntity* pInflictor ) { m_pInflictorForDamageEvents = pInflictor; }
	CEntity* GetDamageInflictor() const { return m_pInflictorForDamageEvents; }

	CWeaponComponent* m_pWeaponComponent;

	//Vector3 m_vLockOnTargetingOffset;

	CObjectFlags m_nObjectFlags;

	u16	m_procObjInfoIdx;
	s16	StreamedScriptBrainToLoad;

	u32 m_nEndOfLifeTimer;	// time of creation, to see when to remove object - for temporarily created objects

	CObjectIntelligence * m_pIntelligence;	// Object intelligence - contains an ai task tree, optional

	struct {
		u32	m_parentModelIdx;		// for detached props - need to render the original prop
		u32	m_propIdxHash;
		u32	m_texIdxHash;
	} uPropData;

	struct {
		float m_fFloaterHeight;	// float height for floaters
	} uMiscData;

	CWeapon* m_pWeapon;

	audPlaceableTracker m_PlaceableTracker;
	audObjectAudioEntity m_AudioEntity;
	
	RegdVeh m_pGadgetParentVehicle;	// The vehicle that owns the vehicle gadget that consists of this object (If this object is a vehicle gadget part).
	s32		m_nParentGadgetIndex;	// The index of the gadget on the vehicle that owns this object.

private:

	enum
	{
		MAX_OBJECTS_DAMAGED_BY_SNIPER	= 100,	// try to keep the 100 most recently sniped objects from reverting to dummy state
		INVALID_DAMAGE_SCANCODE			= 0
	};

	RegdEnt m_pInflictorForDamageEvents; // The entity to use for damage events triggered by collisions with this object
	RegdDummyObject m_pRelatedDummy;
	RegdEnt m_pFragParent;
	RegdPhy m_pGroundPhysical;
	fwInteriorLocation	m_relatedDummyLocation;

	u32 m_damageScanCode;
	static u32 ms_globalDamagedScanCode;

	// The last orientation the object tried to generate cover at
	CoverOrientation m_eLastCoverOrientation;

	u32 m_nPhysicsTimeSliceToAvoidCollision;

	s32 m_firstMovedTimer;
	float m_fTargetableDistance; // The los targeting checks for lock on pass within this distance

	TDynamicObjectIndex m_iPathServerDynObjId;
	s8 m_vehModSlot; // when vehicle mods break off, need to keep track of which one
	u8 m_speedBoost; // GTAV - DLC - For the stunt pack we have speed boost pads 
	u8 m_speedBoostDuration; // GTAV - DLC - For the stunt pack we have speed boost pads 
	u8 m_jointToDriveIndex;

	bool m_bShootThroughWhenEquipped : 1;	// if this object can be shot through by bullets when a ped is holding it.
	bool m_bForcePostCameraAiUpdate : 1;	// should the object force a post camera ai task update this frame (resets in processpostcamera)
	bool m_bForcePostCameraAnimUpdate : 1;	// Should the object force a post camera anim update this frame (resets in processpostcamera)
	bool m_bForcePostCameraAnimUpdateUseZeroTimeStep : 1;	// Should we use a zero timestep when frocing the update
	bool m_bFixedByScriptOrCode : 1;		// Distinguish that this object's fixed state is not the same as its default state
	bool m_bForceVehiclesToAvoid	: 1;	// Set by script. Force vehicles to avoid this even if it's small
	bool m_bHasNetworkedFrags : 1;			// Used in MP
	bool m_bDisableObjectMapCollision : 1;	// Reset flag. Used by synced scenes and scripted animation.
	bool m_bIsAttachedToHandler :1;			// Should a player ped should ragdoll when hit by the big container model used with the handler?
	bool m_bTakesDamageFromCollisionsWithBuildings :1; // the object takes collision damage when hitting buildings.
	bool m_bIsParachute : 1;
	bool m_bIsCarParachute : 1;             // Is this a parachute for a car? (Ruiner gadget car)
	bool m_bDisableCarCollisionsWithCarParachute : 1;
	bool m_bRegisteredInNetworkGame	: 1;
	bool m_bIntentionallyLeakedObject : 1;
	bool m_bUseLightOverride : 1;			// Applies a light override.
    bool m_bProjectilesExplodeOnImpact : 1;
	bool m_bDriveToMaxAngle : 1;
	bool m_bDriveToMinAngle : 1;
	bool m_bIsPressurePlate : 1;
	bool m_bWeaponImpactsApplyGreaterForce : 1;
	bool m_bIsArticulatedProp : 1;
	bool m_bIsBall : 1;
	bool m_bBallShunted : 1;
	bool m_bExtraBounceApplied : 1;
	bool m_bIsAlbedoAlpha : 1;


private:
    bool IsJointAtLimitAngle(int jointIndex, bool bMinAngle);

#if DEFORMABLE_OBJECTS
	const CDeformableObjectEntry* m_pDeformableData;
#endif // DEFORMABLE_OBJECTS

public:
	// Statics
	static u32 ms_nNoTempObjects;
	
	// For not triggering multiple stealth blips in a frame
	static u32 ms_uLastStealthBlipTime;

	static u32 ms_uStartTimerForCollisionAgitatedEvent;

	static atArray<CObject*>	ms_ObjectsThatNeedProcessPostCamera;

	static dev_float ms_fMinMassForCollisionNoise;
	static dev_float ms_fMinRelativeVelocitySquaredForCollisionNoise;
	static dev_float ms_fMinMassForCollisionAgitatedEvent;
	static dev_float ms_fMinRelativeVelocitySquaredForCollisionAgitatedEvent;

	static Vector3 ms_RollerCoasterVelocity;

	static bool ms_bDisableCarCollisionsWithCarParachute;

	static bool ms_bAllowDamageEventsForNonNetworkedObjects;
	
#if !__FINAL
	static bool bInObjectCreate;
	static int inObjectDestroyCount;
	static bool bDeletingProcObject;
#endif

public:
	// network hooks
	NETWORK_OBJECT_TYPE_DECL( CNetObjObject, NET_OBJ_TYPE_OBJECT);

#if GTA_REPLAY
public:
	u32			GetHash() const						{	return m_hash;			}
	void		SetHash(u32 val)					{	m_hash = val;			}
	CObject*	ReplayGetMapObject() const			{	return m_MapObject;		}
	void 		ReplaySetMapObject(CObject* pObject){	m_MapObject = pObject;	}
private:
	u32		 m_hash;
	CObject* m_MapObject;
#endif // GTA_REPLAY
};


// ------------------------------------------------------------------------------

#define MAXCROSSINGANGLE (PI * 0.43f)



#ifdef DEBUG
	void AssertObjectPointerValid(CObject *pObject);
#else
	#define AssertObjectPointerValid( A )
#endif

#ifdef DEBUG
	void AssertObjectPointerValid_NotInWorld(CObject *pObject);
#else
	#define AssertObjectPointerValid_NotInWorld( A )
#endif

bool IsObjectPointerValid(CObject *pObject);
bool IsObjectPointerValid_NotInWorld(CObject *pObject);


//------------- locked object stuff---------------------

#define MAX_LOCKED_OBJECTS	(200)

struct CLockedObjStatus
{
	float	m_TargetRatio;		// Door open ratio
	bool	m_bLocked;			// Door is locked
	bool	m_bUnsprung;		// Door will not spring back to closed if this is set
	u32		m_modelHashKey;
	float	m_AutomaticRate;
	float	m_AutomaticDist;	// Distance threshold for automatic opening
};

typedef atMap<u64, CLockedObjStatus>	 LockedObjMap;


class CLockedObects
{
public:
	CLockedObects() {}
	~CLockedObects() {}

	void Initialise(void);
	void Shutdown(void);
	void ShutdownLevel(void);

	void Reset();

	CLockedObjStatus* SetObjectStatus(CEntity *pEntity, bool bIsLocked, float targetRatio, bool unsprung, float automaticRate, float automaticDist);
	CLockedObjStatus* SetObjectStatus(const Vector3 &pos, bool bIsLocked, float targetRatio, bool unsprung, u32 modelHash, float automaticRate, float automaticDist);

	CLockedObjStatus* GetObjectStatus(CEntity* pEntity);
	CLockedObjStatus* GetObjectStatus(const Vector3 &pos, u32 modelIdx);
	CLockedObjStatus* GetObjectStatus(const u64 posHash, u32 modelIdx);

	static u64 GenerateHash64(const Vector3 &pos);

#if !__FINAL
	void DebugDraw() const;
#endif

private:
	LockedObjMap		m_lockedObjs;
};

#if __DEV
// forward declare pool full callback so we don't get two versions of it
template<> void fwPool<CObject>::PoolFullCallback();
#endif

//extern CLockedObects		gLockedObjects;

#endif //#!__SPU...
#endif//_OBJECT_H_...

