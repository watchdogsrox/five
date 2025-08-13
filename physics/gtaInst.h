//
// physics/inst.h
//

#ifndef GTA_PHYSICS_INST_H
#define GTA_PHYSICS_INST_H

#include "debug\debug.h"
#include "fragment\instance.h"
#include "fragment\manager.h"
#include "phframework\fraginst.h"
#include "phframework\inst.h"
#include "phglass\glassinstance.h"
#include "physics\inst.h"
#include "scene\RegdRefTypes.h"
#include "fwtl/pool.h"

#if __WIN32
#pragma warning (push)
#endif

#define ART_MONOLITHIC_STATIC
#define NMUTILS_NODLL
#include "fragmentnm\instance.h"
#include "art\artfeedback.h"

#if __WIN32
#pragma warning (pop)
#endif

class CEntity;
class CPed;
class CPhysical;

namespace rage
{
	class crClip;
}

////////////////////////////////////////////////////////////
// instance type info
// Please copy any changes to this inst type enum to the identical one in NmRsCharacter.h in the ART namespace
enum
{
	PH_INST_GTA = fragInst::PH_INST_GDF_LAST+1,	// marker for start of GTA phInst's
	PH_INST_MAPCOL,
	PH_INST_BUILDING,
	PH_INST_VEH,
	PH_INST_PED,
	PH_INST_OBJECT,
	PH_INST_PROJECTILE,
	PH_INST_EXPLOSION,
	PH_INST_PED_LEGS,

	PH_INST_FRAG_GTA,							// marker for start of GTA fragInst's
	PH_INST_FRAG_VEH,
	PH_INST_FRAG_PED,
	PH_INST_FRAG_OBJECT,
	PH_INST_FRAG_BUILDING, 
	PH_INST_FRAG_CACHE_OBJECT,

	PH_INST_FRAG_LAST
};

enum
{
	FRAG_IMPULSE_SOURCE_TYPE_FLAG_PED=1<<0,
	FRAG_IMPULSE_SOURCE_TYPE_FLAG_VEHICLE=1<<1,
	FRAG_IMPULSE_SOURCE_TYPE_FLAG_WEAPON=1<<2,
	FRAG_IMPULSE_SOURCE_TYPE_FLAG_EXPLOSION=1<<3
};

// PURPOSE
//   Physics instances for GTA entity types derived from RAGE physics phInst
//   derivation primarily to impliment virtual functions to provide specific collision behaviors
//   and to allow collision response
//
class phInstGta: public phfwInst
{
public:

	enum eGtaInstFlags
	{
		// compile time assert in gtaInst.cpp to make sure the first value is equal to phfwInst::FLAG_USER_0
		FLAG_USERDATA_PARENT_INST		= BIT(10),	// user data contains fragment parent (thing we broke off) entity pointer
		FLAG_NO_COLLISION				= BIT(11),	// turn off collision for this inst
		FLAG_IS_DOOR					= BIT(12),	// flag as a door (don't collide with other doors)
		FLAG_UNUSED						= BIT(13),	// don't allow any fragment breaking
		FLAG_BEING_DELETED				= BIT(14),	// identify when instances are being deleted, so we can skip certain asserts
		FLAG_NM_NO_PLAYER_PED_ACTIVATE	= BIT(15),	// ragdoll activation control flags
		// only 16 bits available for inst flags
	};

	phInstGta (s32 nInstType);
	phInstGta (datResource & rsc);
	virtual ~phInstGta();

	FW_REGISTER_CLASS_POOL(phInstGta);

	// Helper function called from ShouldFindImpacts() if the instance is a door, shared with fragInstGta.
	static bool ShouldFindImpactsDoorHelper(const phInst* pOtherInst);

////////////////////////////////////////////////////////////
// overridden phInst functions
	virtual bool ShouldFindImpacts(const phInst* pInst) const;
	virtual void PreComputeImpacts (phContactIterator impacts);

	virtual Vec3V_Out GetExternallyControlledVelocity () const;
	virtual Vec3V_Out GetExternallyControlledAngVelocity () const;

	virtual phInst *PrepareForActivation(phCollider **collider, phInst* otherInst, const phConstraintBase * constraint);
	virtual void OnActivate(phInst* otherInst);
	virtual bool PrepareForDeactivation(bool colliderManagedBySim, bool forceDeactivate);
	virtual void OnDeactivate();

#if __DEV
	PH_NON_SPU_VIRTUAL void InvalidStateDump() const;
#endif // __DEV
};

// check we're using the correct first available user data flag
CompileTimeAssert(phfwInst::eInstFlags(phInstGta::FLAG_USERDATA_PARENT_INST) == phfwInst::FLAG_USER_0);


// gta's version of a fragmentable instance (see above for virtual function explanations)
class fragInstGta: public phfwFragInst
{
public:

	fragInstGta (s32 nInstType, const fragType* type, const Matrix34& matrix, u32 guid = 0);
	fragInstGta ();
	virtual ~fragInstGta();

	FW_REGISTER_CLASS_POOL(fragInstGta);

	static void LoadVehicleFragImpulseFunction();
#if __BANK
	static void SaveVehicleFragImpulseFunction();
#endif
	static void ReleaseVehicleFragImpulseFunction();
	static void AddVehicleFragImpulseFunctionWidgets(bkBank& bank);

////////////////////////////////////////////////////////////
// overridden Inst functions
	bool GetDontBreakFlag(s64 nFlag) const {return 0!=(m_nDontBreakCompFlags & nFlag);}
	void SetDontBreakFlag(s64 nFlag) {m_nDontBreakCompFlags |= nFlag;}
	void ClearDontBreakFlag(s64 nFlag) {m_nDontBreakCompFlags &= ~nFlag;}
	void SetDontBreakFlagAllChildren(s64 nChild);
	void ClearDontBreakFlagAllChildren(s64 nChild);

	virtual bool ShouldFindImpacts(const phInst* pInst) const;
	virtual void PreComputeImpacts (phContactIterator impacts);

	virtual phInst *PrepareForActivation(phCollider **collider, phInst* otherInst, const phConstraintBase * constraint);
	virtual bool PrepareForDeactivation(bool colliderManagedBySim, bool forceDeactivate);
	virtual void OnActivate(phInst* otherInst);
	virtual void OnDeactivate();
	virtual void PrepareBrokenInst(fragInst *brokenInst) const;
	virtual void PartsBrokeOff(const ComponentBits& brokenGroups, phInst* newInst);
	virtual void PartsRestored(const ComponentBits& restoredGroups);
	virtual void CollisionDamage(const Collision& msg, phContactIterator *impacts);

	virtual bool IsBreakable(phInst* otherInst) const;
	virtual bool FindBreakStrength(const Vector3* componentImpulses, const Vector4* componentPositions, float* breakStrength, phBreakData* breakData) const;
	virtual bool ShouldBeInCacheToDraw() const;
	virtual void LosingCacheEntry();
	virtual fragCacheEntry* PutIntoCache(); 
	virtual bool IsGroupBreakable(int nGroupIndex) const;

	virtual Mat34V_ConstRef GetLastMatrix() const;

	virtual Vec3V_Out GetExternallyControlledVelocity () const;
	virtual Vec3V_Out GetExternallyControlledAngVelocity () const;

	virtual ScalarV_Out GetInvTimeStepForComponentExternalVelocity () const;

	virtual ScalarV_Out ModifyImpulseMag (int iMyComponent, int iOtherComponent, int iNumComponentImpulses, ScalarV_In fImpulseMagSquared, const phInst* otherInst) const;

	virtual int GetClassType () const { return m_classType; }
	void SetClassType (int type) { m_classType = type; }

#if __DEV
	PH_NON_SPU_VIRTUAL void InvalidStateDump() const;
#endif // __DEV

private:
	s64 m_nDontBreakCompFlags;
	int m_classType;

	//Called locally on a frag to detect if the destroy event should be triggered.
	//  - It check's to see if the object has a damage entity set (is only set for the parent object)
	//      and check's if that entity is the player.
	bool  TriggerEventEntityDestroyed(CPhysical* entity);
};



class ARTFeedbackInterfaceGta : public ART::ARTFeedbackInterface
{
public:
	ARTFeedbackInterfaceGta();
	virtual ~ARTFeedbackInterfaceGta();

	void SetParentInst(phInst* pParent) {m_pParentInst = pParent;}
	phInst* GetParentInst() {return m_pParentInst;}

	virtual int onBehaviourFailure();
	virtual int onBehaviourSuccess();
	virtual int onBehaviourStart();
	virtual int onBehaviourFinish();
	virtual int onBehaviourEvent();
	//virtual int onBehaviourRequest()  { return -1; }

private:
	phInst* m_pParentInst;
};


enum eNMAssetId
{
	NM_ASSET_FRED = 0,
	NM_ASSET_WILMA = 1,
	NM_ASSET_FRED_LARGE = 2, 
	NM_ASSET_WILMA_LARGE = 3, 
	NM_ASSET_ALIEN = 4 
};

//class CBulletForce
//{
//public:
//	enum eBulletForceFlags
//	{
//		LOCAL_POS		= BIT(0),
//		SPREAD_UP1		= BIT(1),
//		SPREAD_DOWN1	= BIT(2),
//		SPREAD_DOWN2	= BIT(3)
//	};
//
//	CBulletForce();
//
//	void InitPedImpact(CPed* pPed, float fMomentum, const Vector3& vecDir, const Vector3& vecWorldPos, int nComponent, u32 nWeaponHash);
//	void ProcessPhysics(float fTimeStep);
//	bool IsActive() const {return m_pTarget != NULL;}
//	void Reset();
//private:
//	RegdPhy m_pTarget;
//	Vector3 m_vecForce;
//	Vector3 m_vecPos;
//	float m_fTime;
//	float m_fMassMult;
//
//	u16 m_nComponent;
//	u16 m_nFlags;
//
//public:
//	static float ms_fPedBulletApplyTime;
//	static float ms_fPedMeleeApplyTime;
//	static float ms_fPedBulletMassMult;
//	static float ms_fPedMeleeMassMult;
//};
//
//#define NUM_BULLET_FORCES (128)

// natural motion should only be compiled on specific platforms
// but fragInstNMGta is primarily the ped specific fragInst type
class fragInstNMGta: public fragInstNM
{
public:
	fragInstNMGta (s32 nInstType, const fragType* type, const Matrix34& matrix, u32 guid = 0);
	fragInstNMGta (datResource & rsc);
	virtual ~fragInstNMGta ();

	enum eBodyPartGroup
	{
		kSpine = 0,
		kNeck,
		kLimb,
		kInvalidPartGroup
	};

	enum eRampType
	{
		kStiffness = 0,
		kProportionalGain,
		kInvalidRampType
	};

	FW_REGISTER_CLASS_POOL(fragInstNMGta);

	// gta instances use the user data pointer to store their parent entity (may be null)
	void SetEntity(CEntity *pEnt) { SetUserData((void*)pEnt); }

////////////////////////////////////////////////////////////
// overridden Inst functions
	virtual int GetClassType () const									{ return m_nType; }

	virtual bool ShouldFindImpacts(const phInst* pInst) const;
	virtual void PreComputeImpacts (phContactIterator impacts);

	bool CheckCanActivate(const CPed* pPed, bool bAssert) const;
	virtual void OnActivate(phInst* otherInst);
	virtual void OnDeactivate();
	virtual phInst *PrepareForActivation(phCollider **collider, phInst* otherInst, const phConstraintBase * constraint);
	virtual bool PrepareForDeactivation(bool colliderManagedBySim, bool forceDeactivate);

#if __BANK
	void CheckRagdollsUsedDebug();
#endif

	virtual bool ShouldBeInCacheToDraw() const;
	virtual void LosingCacheEntry();
	virtual fragCacheEntry* PutIntoCache(); 

	virtual Vec3V_Out GetExternallyControlledVelocity () const;

	virtual bool FindBreakStrength(const Vector3* componentImpulses, const Vector4* componentPositions, float* breakStrength, phBreakData* breakData) const;
	virtual bool IsBreakable(phInst* otherInst) const;

	void SetActivePoseFromClip(const crClip* pClip, float fPhase=0.0f, bool bUseWorldCoords=false);
	void SetIncomingTransformsFromAnim(const crClip* pClip, const float fPhase=0.0f);
	void SetActivePoseFromSkel(const crSkeleton* pSkel);

	virtual void PoseBoundsFromSkeleton(bool onlyAdjustForInternalMotion, bool updateLastBoundsMatrices, bool fullBvhRebuild = true, s8 numFragsToDisable = 0, const s8 *pFragsToDisable = NULL);

	Vec3V_Out SendAnimationAverageVelToNM(float fTimeStep);

	void SetARTFeedbackInterfaceGta();

	void ScaleImpulseByComponentMass(Vector3& vecImpulse, int nComponent, float fMassInfluence);
	float ScaleImpulseByComponentMass(int nComponent, float fMassInfluence);

	//CBulletForce* GetBulletForce() {return &m_BulletForce;}
	void ApplyBulletForce(CPed* pPed, float fMomentum, const Vector3& vecWorldNormal, const Vector3& vecWorldPos, int nComponent, u32 nWeaponHash);

	void SetDontZeroMatricesOnActivation() {m_bDontZeroMatricesOnActivation = true;}

	void ProcessRagdoll(float fTimeStep);

	//static bool UseBulletForces() {return ms_bUseBulletForces;}

	inline void SetDisableCollisionMask(u32 iNewMask) { m_iDisableCollisionMask = iNewMask; }
	inline u32 GetDisableCollisionMask() { return m_iDisableCollisionMask;}

	inline u32 GetActivationStartTime() { return m_fActivationStartTime; }

	void SetPendingImpulseModifier(float set) { m_PendingImpulseModifier = set; }

	void SetAllowDeadPedActivationThisFrame(bool set) { m_AllowDeadPedActivationThisFrame = set; }
	
	// If the scale is 0.0, then we also set the average animation velocity to 0 in order to have the ragdoll
	// start with zero velocity, as that seems to be the desired use case.
	void SetIncomingAnimVelocityScaleReset(float set) { m_IncomingAnimVelocityScaleReset = set; }

	void UpdateCounterImpulse(CPed* pPed, bool bHurriedCounterImpulse = false);

	bool GetBulletLoosenessActive() const { return m_bBulletLoosenessActive; }

	// Used to track impact damage from a particular source over a short amount of time, so that impact damage gets applied more consistently
	void SetImpactDamageEntity(CEntity *entity);
	void SetAccumulatedImpactDamageFromLastEntity(float damage) { m_fAccumulatedImpactDamage = damage; }
	CEntity * GetLastImpactDamageEntity() const { return m_pLastImpactDamageEntity; }
	float GetAccumulatedImpactDamageFromLastEntity() const { return m_fAccumulatedImpactDamage; }
	u32 GetTimeSinceImpactDamage() const;

	void SetNMAssetSize(int newSize);

	virtual void SetCurrentPhysicsLOD(ePhysicsLOD set);

	inline bool GetSuppressHighRagdollLODOnActivation() const { return m_SuppressHighRagdollLODOnActivation; }
	inline void SetSuppressHighRagdollLODOnActivation(const bool b) { m_SuppressHighRagdollLODOnActivation = b; }
	inline void RequestPhysicsLODChangeOnActivation(s8 requestedLOD) { m_RequestedPhysicsLODOnActivation = requestedLOD; } 

	// Functions associated with ejecting from a vehicle
	void CheckForVehicleAssociation();
	void SetAssociatedWithVehicle(CVehicle *pVehicle);
	bool IsAssociatedWithVehicle(CVehicle *pVehicle) const;
	void SetRagdollEjectedFromVehicle(CVehicle *vehicle);
	inline bool IsEjectingFromVehicle() { return m_EjectedFromVehicleFrames > 0; }
	inline CVehicle* GetEjectingVehicle() { return m_VehicleBeingEjectedFrom; }
	void ProcessVehicleEjection(float fTimeStep);
	void ResetVehicleEjectionMembers(); 

	// Functions associated with wheel impacts
	void SetContactedWheel(bool set) { m_ContactedWheel = set; }
	void SetTimeTireImpulseWasApplied(u32 set) { m_uTimeTireImpulseWasApplied = set; } 
	u32 GetTimeTireImpulseWasApplied() const { return m_uTimeTireImpulseWasApplied; }
	void SetTireImpulseAppliedRatio(float set) { m_fTireImpulseAppliedRatio = set; } 
	float GetTireImpulseAppliedRatio() const { return m_fTireImpulseAppliedRatio; }
	void SetWheelTwitchStarted(bool set) { m_WheelTwitchStarted = set; } 
	bool GetWheelTwitchStarted() const { return m_WheelTwitchStarted; }

	// Functions related to ramping joint values (stiffness, drive gains, etc.) in two stages
	eBodyPartGroup GetBodyPartGroup(int component);
	void ProcessTwoStageRamp(fragInstNMGta::eRampType type, fragInstNMGta::eBodyPartGroup partGroup, float startStifness, float midStifness, 
		float endStiffness, float durationStartToMid, float durationMidToEnd, float timeElapsed);

#if __DEV
	PH_NON_SPU_VIRTUAL void InvalidStateDump() const;
#endif // __DEV

	void GrabRootFrame();

private:

	//This will save the root bone position each time the bounds are updated from the skeleton
	// and then reset it upon ragdoll activation.  This compensates for animation bugs where the skeleton root
	// gets modified in between the last UpdateBoundsFromSkeleton call and before the ragdoll gets activated.
	Vec3V m_RootBonePosAtBoundsUpdate;

	ARTFeedbackInterfaceGta m_feedbackInterfaceGta;

	s32 m_nType;
	float m_ImpulseModifier;
	float m_PendingImpulseModifier;
	float m_IncomingAnimVelocityScaleReset;

	// Time since the last bullet hit this rage ragdoll (not used for NM agents)
	u32 m_LastRRApplyBulletTime;

	int m_CounterImpulseCount;

	// Mask to disable impacts with certain component
	// More convenient to do here as opposed to using type and include flags
	// because NM bounds are not owned by us
	u32 m_iDisableCollisionMask;

	u32 m_fActivationStartTime;

	RegdVeh	m_VehicleBeingEjectedFrom;	

	// Used to track impact damage from a particular source over a short amount of time, so that impact damage gets applied more consistently
	RegdEnt	m_pLastImpactDamageEntity; 
	u32		m_uTimeSinceImpactDamage;
	float   m_fAccumulatedImpactDamage;

	u32		m_uTimeTireImpulseWasApplied;
	float	m_fTireImpulseAppliedRatio;

	s8 m_RequestedPhysicsLODOnActivation;
	s8 m_EjectedFromVehicleFrames;

	bool m_bDontZeroMatricesOnActivation : 1;
	bool m_AllowDeadPedActivationThisFrame : 1;
	bool m_SuppressHighRagdollLODOnActivation : 1;
	bool m_bBulletLoosenessActive : 1;
	bool m_ContactedWheel : 1;
	bool m_WheelTwitchStarted : 1;

	//static bool ms_bUseBulletForces;

public:
	static bool ms_bLogRagdollForces;

	static dev_float ms_fVehiclePedActivationSpeed;
	static dev_float ms_fVehiclePlayerActivationSpeed;
	static dev_float ms_fPlayerPedActivationSpeed;
	static dev_float ms_fPlayerGroupPedActivationSpeed;

	static float m_ArmedPedBumpResistance;

#if __BANK
	// Variables used to track the number of ragdolls that get used per level
	static bool ms_bLogUsedRagdolls;
	static int ms_MaxNMAgentsUsedCurrentLevel;
	static int ms_MaxRageRagdollsUsedCurrentLevel;
	static int ms_MaxNMAgentsUsedInComboCurrentLevel;
	static int ms_MaxRageRagdollsUsedInComboCurrentLevel;
	static int ms_MaxTotalRagdollsUsedCurrentLevel;
	static int ms_NumFallbackAnimsUsedCurrentLevel;

	static int ms_MaxNMAgentsUsedGlobally;
	static int ms_MaxRageRagdollsUsedGlobally;
	static int ms_MaxNMAgentsUsedInComboGlobally;
	static int ms_MaxRageRagdollsUsedInComboGlobally;
	static int ms_MaxTotalRagdollsUsedGlobally;
	static int ms_NumFallbackAnimsUsedGlobally;
#endif
};

// find out if a phInst is actually a fragInst (i.e. can be safely cast to a fragInst)
__forceinline bool IsFragInst(const phInst* pInst)
{
	if (pInst)
	{
		int classType = pInst->GetClassType();
		return classType == fragInst::PH_FRAG_INST || (classType >= PH_INST_FRAG_GTA && classType <= PH_INST_FRAG_CACHE_OBJECT);
	}

	return false;
}

__forceinline bool IsFragInstClassType(int instClassType)
{
	return instClassType == fragInst::PH_FRAG_INST || (instClassType >= PH_INST_FRAG_GTA && instClassType <= PH_INST_FRAG_CACHE_OBJECT);
}

#endif // end #ifndef GTA_PHYSICS_INST_H
