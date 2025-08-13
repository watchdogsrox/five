// Started	:	10/04/2003
//
//
//
#ifndef _HELI_H_
#define _HELI_H_

//rage includes
#include "fwutil/idgen.h"

// game includes
#include "camera/helpers/HandShaker.h"
#include "vehicles/automobile.h"
#include "vehicles/Propeller.h"
#include "vehicles/landinggear.h"
#include "scene/RegdRefTypes.h"
#include "vehicles/VehicleGadgets.h"
#include "network/objects/entities/netObjHeli.h"
#include "System/PadGestures.h"						// for define

extern float sfHeliAbandonedYawMult;
extern float sfHeliAbandonedPitchMult;
extern float HELI_CRASH_THROTTLE;
extern float HELI_CRASH_PITCH;
extern float HELI_CRASH_YAW;
extern float HELI_CRASH_ROLL;


//#define MAXNUMHELIS (2)		// The number of police helis we can have

#define HELI_NUM_STORED_IMPACTS (20)

// Helis in network games are put in a number of different height slots. This is that number of slots.
#define NUM_RAISED_HEIGHT_LEVELS_NETWORK (3)

class CWeapon;
class CHeliIntelligence;

namespace rage
{
    class netPlayer;
	class ropeInstance;
	struct ropeAttachment;
}

const float SECOND_HELI_EXTRA_HEIGHT = 12.0f;

class CRotaryWingAircraft;
struct sHeliRotorImpact
{
	fwRegdRef<CRotaryWingAircraft> m_pAircraft;
	RegdEnt m_pOtherEntity;
	Vector3 m_vecHitNorm;
	Vector3 m_vecHitPos;
	Vector3 m_vecOtherPosOffset;
	Vector3 m_vecImpulse;
	int m_nHeliComp;
	int m_nOtherComp;
	phMaterialMgr::Id m_nMaterialId;
};

const int NUM_HELI_WINGS_MAX = 2;


//////////////////////////////////////////////////////////////////////////
// Class CRotaryWingAircraft
//////////////////////////////////////////////////////////////////////////
// Parent class for all rotary wing aircraft
// E.g. Helicopters, Gyrocopters etc.
// This class deals with rotor impacts, rendering, rotor damage etc. that is common to all rotary wing aircraft
// Child classes must implement flight handling and control processing
class CRotaryWingAircraft : public CAutomobile
{
	friend struct CVehicleOffsetInfo;

#if GTA_REPLAY
	friend class CPacketHeliUpdate;
#endif

public:

	enum eHeliRotors
	{
		Rotor_Main,
		Rotor_Rear,
		Num_Heli_Rotors,

		Rotor_Main2 = Num_Heli_Rotors,
		Rotor_Rear2,
		Num_Drone_Rotors,

		Max_Num_Rotors = Num_Drone_Rotors
	};

	enum eHeliTailBoomBreaking
	{
		Break_Off_Tail_Boom_Immediately,
		Break_Off_Tail_Boom_Pending_Bound_Update,
		Num_Break_Off_Tail_Boom_States
	};

	virtual ~CRotaryWingAircraft();

	virtual void InitDoors();
	virtual int  InitPhys();
	virtual void InitCompositeBound();

	virtual bool WantsToBeAwake();

	// Control
	virtual void DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep);
	virtual bool CanPedJumpOutCar(CPed* UNUSED_PARAM(pPed), bool UNUSED_PARAM(bForceSpeedCheck)=false) const  {return true;}

	virtual bool GetCanMakeIntoDummy(eVehicleDummyMode UNUSED_PARAM(dummyMode)) { BANK_ONLY(SetNonConversionReason("Veh is a heli");) return false; }
	virtual bool TryToMakeIntoDummy(eVehicleDummyMode UNUSED_PARAM(dummyMode), bool UNUSED_PARAM(bSkipClearanceTest)=false) { BANK_ONLY(SetNonConversionReason("Veh is a heli");) return false; }

	// Should be called first by subclasses to do common flight handling
	virtual void ProcessFlightHandling(float fTimeStep);

	// Rendering
	virtual ePrerenderStatus	PreRender(const bool bIsVisibleInMainViewport = true);
	virtual void				PreRender2(const bool bIsVisibleInMainViewport = true);		
	virtual void				ApplyDeformationToBones(const void* basePtr);
	virtual void				Fix(bool resetFrag = true, bool allowNetwork = false);
	
	virtual void ProcessFlightModel(float fTimestep) = 0;

    float        ComputeAdditionalYawFromTransform(float fYaw, const fwTransform& transform, float fFlatMoveSpeed);
	

	// Rotor impacts
	virtual void ProcessPreComputeImpacts(phContactIterator impacts);

	virtual void ProcessPostPhysics();

	virtual void BlowUpCar( CEntity *pCulprit, bool bInACutscene = false, bool bAddExplosion = true, bool bNetCall = false, u32 weaponHash = WEAPONTYPE_EXPLOSION, bool bDelayedExplosion = false );
	virtual void FinishBlowingUpVehicle( CEntity *pCulprit, bool bInACutscene = false, bool bAddExplosion = true, bool bNetCall = false, u32 weaponHash = WEAPONTYPE_EXPLOSION, bool bDelayedExplosion = false );//to be called after blowupcar
	virtual const eHierarchyId* GetExtraBonesToDeform(int& extraBoneCount);
	
	virtual void Teleport(const Vector3& vecSetCoors, float fSetHeading=-10.0f, bool bCalledByPedTask=false, bool bTriggerPortalRescan = true, bool bDetachFromParent = true, bool bWarp=true, bool bKeepRagdoll = false, bool bResetPlants=true);

    void SetDampingForFlight(float fLinearDampingMult = 1.0f);
	virtual void SetIsAbandoned(const CPed *pOriginalDriver = NULL);
	virtual void SetIsOutOfControl();
	bool StartCrashingBehavior(CEntity *pCulprit = NULL, bool bIsAbandend = false);

	// Flight control functions
	// Maybe want a superclass (CAircraft?) so that CPlane uses the same stuff
	float GetYawControl() const { return m_fYawControl; }
	float GetRollControl() const { return m_fRollControl; }
	float GetPitchControl() const { return m_fPitchControl; }
	float GetThrottleControl() const { return m_fThrottleControl; }
	float GetCollectiveControl() const { return m_fCollectiveControl; }

	void SetYawControl(float fYawControl) { Assert(fYawControl == fYawControl); m_fYawControl = fYawControl;}
	void SetRollControl(float fRollControl) { Assert(fRollControl == fRollControl); m_fRollControl = fRollControl; }
	void SetPitchControl(float fPitchControl) { Assert(fPitchControl == fPitchControl); m_fPitchControl = fPitchControl; }
	void SetThrottleControl(float fThrottleControl) { Assert(fThrottleControl == fThrottleControl); m_fThrottleControl = fThrottleControl; } 
	void SetCollectiveControl(float fCollectiveControl) {Assert(fCollectiveControl == fCollectiveControl); m_fCollectiveControl = fCollectiveControl; } 
	void SetStrafeMode( bool strafeMode ) { GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_HELI_STRAFE_MODE) ? m_bStrafeMode = strafeMode : m_bStrafeMode = false; }
	bool GetStrafeMode() { return m_bStrafeMode; }

    void SetHoverMode(bool bHoverMode){m_bHoverMode = bHoverMode;}
    virtual bool GetHoverMode() const {return m_bHoverMode;}

    float GetHoverModeYawMult();
    float GetHoverModePitchMult();

    void SetThrustVectoringMode(bool bThrustVectoringMode){m_bEnableThrustVectoring = bThrustVectoringMode;}
    bool GetThrustVectoringMode() {return m_bEnableThrustVectoring;}
	void DisableTurbulenceThisFrame(bool in_value) { m_bDisableTurbulanceThisFrame = in_value; }
    void SetHoverModeDesiredTarget(Vector3 &vTarget){ m_vHoverModeDesiredTarget = vTarget; }
    Vector3 GetHoverModeDesiredTarget(){ return m_vHoverModeDesiredTarget;}

	// Returns TRUE if part is set up and has broken off
	// Returns FALSE if the part hasn't been set up or if it hasn't broken
	bool GetIsMainRotorBroken() const;
	bool GetIsRearRotorBroken() const;
	bool GetIsTailBoomBroken() const;

	void BreakOffMainRotor();
	void BreakOffRearRotor();
	void BreakOffTailBoom(int eBreakingState = Break_Off_Tail_Boom_Immediately);
	virtual void PostBoundDeformationUpdate();
	virtual bool HasBoundUpdatePending() const;

	float GetMainRotorHealth() const { return m_fMainRotorHealth; }
	float GetRearRotorHealth() const { return m_fRearRotorHealth; }
	float GetTailBoomHealth() const { return m_fTailBoomHealth; }
	bool GetCanBreakOffTailBoom() const {return m_bCanBreakOffTailBoom;}
	bool GetBreakOffTailBoomPending() const {return m_uBreakOffTailBoomPending == Break_Off_Tail_Boom_Pending_Bound_Update;}

	void SetMainRotorHealth(float fMainRotorHealth) { m_fMainRotorHealth = fMainRotorHealth; }
	void SetRearRotorHealth(float fRearRotorHealth) { m_fRearRotorHealth = fRearRotorHealth; }
	void SetTailBoomHealth(float fTailBoomHealth) { m_fTailBoomHealth = fTailBoomHealth; }
	void SetCanBreakOffTailBoom(bool bCanBreakOffTailBoom) {m_bCanBreakOffTailBoom = bCanBreakOffTailBoom;}

	virtual void SetMainRotorSpeed(float fMainRotorSpeed) { m_fMainRotorSpeed = fMainRotorSpeed; }
	float GetMainRotorSpeed() const { return m_fMainRotorSpeed; }

	void UpdateRotorBounds();

	s32 GetMainRotorChild() const { return m_propellerCollisions[Rotor_Main].GetFragChild(); }
	s32 GetRearRotorChild() const { return m_propellerCollisions[Rotor_Rear].GetFragChild(); }
	s32 GetMain2RotorChild() const { return m_propellerCollisions[Rotor_Main2].GetFragChild(); }
	s32 GetRear2RotorChild() const { return m_propellerCollisions[Rotor_Rear2].GetFragChild(); }
	s32 GetMainRotorGroup() const { return m_propellerCollisions[Rotor_Main].GetFragGroup(); }
	s32 GetRearRotorGroup() const { return m_propellerCollisions[Rotor_Rear].GetFragGroup(); }
	s32 GetMain2RotorGroup() const { return m_propellerCollisions[Rotor_Main2].GetFragGroup(); }
	s32 GetRear2RotorGroup() const { return m_propellerCollisions[Rotor_Rear2].GetFragGroup(); }
	s32 GetMainRotorDisc() const { return m_propellerCollisions[Rotor_Main].GetFragDisc(); }
	s32 GetRearRotorDisc() const { return m_propellerCollisions[Rotor_Rear].GetFragDisc(); }
	s32 GetMain2RotorDisc() const { return m_propellerCollisions[Rotor_Main2].GetFragDisc(); }
	s32 GetRear2RotorDisc() const { return m_propellerCollisions[Rotor_Rear2].GetFragDisc(); }

	s8 GetTailBoomGroup() const { return m_nTailBoomGroup; }

	virtual bool IsPropeller (int nComponent) const;

    CSearchLight *GetSearchLight();

	bool GetIsDrone() const;
	bool GetIsJetPack() const;

	void SetSearchLightTarget(CPhysical* pTarget) { GetSearchLight()->SetSearchLightTarget(pTarget); }
	CPhysical* GetSearchLightTarget() { return GetSearchLight()->GetSearchLightTarget(); }
	bool GetSearchLightOn();
	void SetSearchLightOn(bool bOn);

    bool ApplyDamageToPropellers( CEntity* inflictor, float fApplyDamage, int nComponent ); //return true if the props were hit

	bool GetHeliRotorDestroyedByPed();

	CPropeller& GetMainPropeller() { return m_propellers[Rotor_Main]; }
	CPropeller& GetRearPropeller() { return m_propellers[Rotor_Rear]; }

	static SearchLightInfo& GetSearchLightInfo() { return ms_SearchLightInfo; }

	void SetPilotNoiseScalar(float in_value) {m_pilotSkillNoiseScalar = in_value;}
	void SetControlLaggingRateMulti(float in_value) {m_fControlLaggingRateMulti = in_value;}
#if __BANK
	static void InitWidgets();
#endif

	void SetDisableAutomaticCrashTask(bool bDisable) { m_bDisableAutomaticCrashTask = bDisable; }
	bool GetDisableAutomaticCrashTask() const { return m_bDisableAutomaticCrashTask; }

protected:

	CRotaryWingAircraft(const eEntityOwnedBy ownedBy, u32 popType, VehicleType veh);
	friend class CVehicleFactory;

	virtual void ProcessControlInputsInactive(CPed *) {}		// Function called when player isn't in control of vehicle (e.g. cutscene)

	//PURPOSE:
	// Calculate the priority to use a searchlight for this helicopter.
	void ProcessSearchLightScoring();

	//PURPOSE:
	// Return true if this helicopter has priority over all other helicopters to use the search light.
	bool HasSearchLightPriority();

	void ResetSearchLightScores()	{ ms_fBestSearchLightScore = 1.0f; }
	void ModifyControlsBasedOnFlyingStats( CPed* pPilot, CFlyingHandlingData* pFlyingHandling, float &fYawControl, float &fPitchControl, float &fRollControl, float &fThrottleControl, float fTimeStep );

#if __BANK
	void VisualisePitchRoll();
#endif

	// Member variables
	// Do not make public plx

	// AI/Controls stuff
	float m_fYawControl;
	float m_fPitchControl;
	float m_fRollControl;
	float m_fThrottleControl;

    bool m_bHoverMode;
    bool m_bEnableThrustVectoring;
	bool m_bDisableTurbulanceThisFrame;
	bool m_bIsInAir;
	Vector3 m_vecRearRotorPosition;
    Vector3 m_vHoverModeDesiredTarget;
    float m_fHoverRollControl;
    float m_fHoverPitchControl;
	float m_fCollectiveControl;

	float m_fHeliOutOfControlRoll;
	float m_fHeliOutOfControlYaw;
    float m_fHeliOutOfControlPitch;
	float m_fHeliOutOfControlThrottle;
    u32 m_uHeliOutOfControlRecoverStart;
    s32 m_iHeliOutOfControlRecoverPeriods;
    bool m_bHeliOutOfControlRecovering;
	bool m_bStrafeMode;

	float m_fStrafeModeTargetHeight;

	float m_fJoystickPitch;
	float m_fJoystickRoll;

	// Rotor rendering
	float m_fMainRotorSpeed;
	float m_fPrevMainRotorSpeed;	// What was the rotor speed at the end of the last call to UpdateRotorSpeed?

	// Index to rotor components
	s8 m_nTailBoomGroup;
	bool m_bCanBreakOffTailBoom;

	float m_fMainRotorHealth;
	float m_fRearRotorHealth;
	float m_fTailBoomHealth;

	float m_fMainRotorHealthDamageScale;
	float m_fRearRotorHealthDamageScale;
	float m_fTailBoomHealthDamageScale;

	float m_fMainRotorBrokenOffSmokeLifeTime;
	float m_fRearRotorBrokenOffSmokeLifeTime;

	CPropellerBlurred m_propellers[ Max_Num_Rotors ];
	CPropellerCollision m_propellerCollisions[ Max_Num_Rotors ];
	int m_iNumPropellers;

	bool	m_bHadSearchLightLastFrame;
	float	m_fSearchLightScore;
	float	m_fLastWheelContactTime;

    CSearchLight *m_pSearchLight;

	//when damage is applied to rotors this is set to true if the damager is a ped and the rotors where destroyed.
	bool m_bHeliRotorDestroyedByPed;

	// Blow up the vehicle in ProcessPostPhysics.
	bool m_bDoBlowUpVehicle;

	float m_pilotSkillNoiseScalar;
	float m_fControlLaggingRateMulti;

	float m_fJetpackStrafeForceScale;
	float m_fJetPackThrusterThrotle; 

	float m_fWingAngle[ NUM_HELI_WINGS_MAX ];

	u8 m_uBreakOffTailBoomPending;

	void CacheWindowBones();

	s16	   m_windowBoneIndices[4];
	bool   m_windowBoneCached;
	bool m_bDisableAutomaticCrashTask;

	// Static methods
private:

	static dev_float	ms_fPreviousSearchLightOwnerBonus;
	static dev_float	ms_fUnblockedSearchLightBonus;
	
	static float		ms_fBestSearchLightScore;
	static bool			ms_bNeedToResetSearchLightScoring;

	static float ms_fHeliControlLaggingMinBlendingRate;
	static float ms_fHeliControlLaggingMaxBlendingRate;
	static float ms_fHeliControlAbilityControlDampMin;
	static float ms_fHeliControlAbilityControlDampMax;
	static float ms_fHeliControlAbilityControlRandomMin;
	static float ms_fHeliControlAbilityControlRandomMax;
	static float ms_fTimeBetweenRandomControlInputsMin;
	static float ms_fTimeBetweenRandomControlInputsMax;
	static float ms_fRandomControlLerpDown;
	static float ms_fMaxAbilityToAdjustDifficulty;


	static SearchLightInfo ms_SearchLightInfo;

public:

#if USE_SIXAXIS_GESTURES
	static bank_float MOTION_CONTROL_PITCH_MIN;
	static bank_float MOTION_CONTROL_PITCH_MAX;
	static bank_float MOTION_CONTROL_ROLL_MIN;
	static bank_float MOTION_CONTROL_ROLL_MAX;
	static bank_float MOTION_CONTROL_YAW_MULT;
#endif

};

//////////////////////////////////////////////////////////////////////////
// Class: CHeli
class CHeli : public CRotaryWingAircraft
{	
    friend class  CNetObjHeli;
	friend struct CVehicleOffsetInfo;

public:
	~CHeli();

	virtual int  InitPhys();
	virtual void InitWheels();

	CHeliIntelligence *GetHeliIntelligence() const;

	virtual void SetModelId(fwModelId modelId);

	virtual void DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep);
//	virtual void ProcessDriverInputsForPlayer(CPed *pPlayerPed);	
	virtual void BlowUpCar( CEntity *pCulprit, bool bInACutscene = false, bool bAddExplosion = true, bool bNetCall = false, u32 weaponHash = WEAPONTYPE_EXPLOSION, bool bDelayedExplosion = false  );
	virtual void FinishBlowingUpVehicle( CEntity *pCulprit, bool bInACutscene = false, bool bAddExplosion = true, bool bNetCall = false, u32 weaponHash = WEAPONTYPE_EXPLOSION, bool bDelayedExplosion = false  );//to be called after blowupcar
	virtual void ProcessPrePhysics();
	virtual ePhysicsResult ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice);
	virtual void ProcessPostPhysics();

	virtual void UpdateRotorSpeed();
	virtual void SetMainRotorSpeed(float fMainRotorSpeed);

	virtual void PreRender2( const bool bIsVisibleInMainViewport = true );		
	virtual void ApplyDeformationToBones( const void* basePtr );
	virtual void Fix( bool resetFrag = true, bool allowNetwork = false);

	virtual bool WantsToBeAwake() { return CRotaryWingAircraft::WantsToBeAwake() || ( m_bHasLandingGear && m_landingGear.WantsToBeAwake( this ) ); }


	virtual void ProcessCollision(phInst const * myInst, CEntity* pHitEnt, phInst const * hitInst, const Vector3& vMyHitPos,
		const Vector3& vOtherHitPos, float fImpulseMag, const Vector3& vMyNormal, int iMyComponent, int iOtherComponent,
		phMaterialMgr::Id iOtherMaterial, bool bIsPositiveDepth, bool bIsNewContact);
	virtual void ProcessPreComputeImpacts(phContactIterator impacts);

	netPlayer* GetOwnerPlayer() const { return m_pOwnerPlayer;}
	void SetOwnerPlayer(netPlayer* pOwnerPlayer) { m_pOwnerPlayer = pOwnerPlayer; }

    void AttachAntiSwayToEntity(CPhysical* pEntity, float fSpringDistance);

	// Driver and passenger side ropes
	enum eRopeID
	{
		eRopeDriverSide = 0,
		eRopePassengerSide = 1,
		eRopeDriverSideRear = 2,
		eRopePassengerSideRear = 3,
		eNumRopeIds,
	};
	ropeInstance*	CreateAndAttachRope(bool bDriverSide, s32 iSeatId, float fLength, float fMinLength, float fMaxLength, float fLengthChangeRate, int ropeType, int numSections, bool lockFromFront);
	ropeAttachment* AttachObjectToRope(bool bDriverSide, s32 iSeatId, CEntity* pAttachToEntity, float fEntityMoveSpeed);
	bool			AreRopesAttachedToNonHeliEntity() const;
	bool			CanEntitiesAttachToRope(bool bDriverSide, s32 iSeatId);
	
	ropeInstance*	GetRope(bool bDriverSide)
	{
		return bDriverSide ? m_Ropes[eRopeDriverSide] : m_Ropes[eRopePassengerSide]; 
	}

	ropeInstance*	GetRope(eRopeID eRope)
	{
		return m_Ropes[eRope];
	}

	void SetRope(bool bDriverSide, ropeInstance* rope)
	{
		Assert(rope);
		if(bDriverSide)
		{
			Assert(m_Ropes[eRopeDriverSide] == NULL);
			m_Ropes[eRopeDriverSide] = rope;
			return;
		}

		Assert(m_Ropes[eRopePassengerSide] == NULL);
		m_Ropes[eRopePassengerSide] = rope;
	}

	void SetRope(eRopeID eRope, ropeInstance* rope)
	{
		Assert(rope);
		Assert(m_Ropes[eRope]==NULL);
		m_Ropes[eRope]=rope;
	}

	eRopeID GetRopeIdFromSeat(bool bDriverSide, s32 iSeatId)
	{
		if(iSeatId >= 0)
		{
			switch(iSeatId)
			{
			case(2):
				return eRopeDriverSide;
			case(3):
				return eRopePassengerSide;
			case(4):
				return eRopeDriverSideRear;
			case(5):
				return eRopePassengerSideRear;
			default:
				break;
			}
		}

		return bDriverSide ? eRopeDriverSide : eRopePassengerSide;
	}

	void SetPickupRopeType(ePickupRopeGadgetType nPickupType) { m_nPickupRopeType = nPickupType; }
    CVehicleGadgetPickUpRope *AddPickupRope();
	void RemoveRopesAndHook();

	bool DoesBulletHitPropellerBound(int iComponent) const;
	bool DoesBulletHitPropellerBound(int iComponent, bool &bHitDiscBound, int &iPropellerIndex) const;
	bool NeedUpdatePropellerBound(int iPropellerIndex) const;
	bool DoesBulletHitPropeller(int iEntryComponent, const Vector3 &vEntryPos, int iExitComponent, const Vector3 &vExitPos) const;
	bool DoesProjectileHitPropeller(int iComponent) const;
	void SetTurbulenceScalar(float in_value) { m_turbulenceScalar = in_value; }
	bool GetIsCargobob() const;
	const Vector3 &GetHeliSteeringBias() const {return m_vHeliSteeringBias;}

	void SetIsPoliceDispatched(bool bIsPoliceDispatched) { m_bPoliceDispatched = bIsPoliceDispatched; }
	bool GetIsPoliceDispatched() { return m_bPoliceDispatched; }
	void SetIsSwatDispatched(bool bIsSwatDispatched) { m_bSwatDispatched = bIsSwatDispatched; }
	bool GetIsSwatDispatched() { return m_bSwatDispatched; }
	bool ShouldCreateLandingGear();
	bool HasLandingGear() { return m_bHasLandingGear; }
	bool HasLandingGear() const { return m_bHasLandingGear; }

	const CLandingGear& GetLandingGear() const { return m_landingGear; }
	CLandingGear& GetLandingGear() { return m_landingGear; }
	bool GetWheelDeformation(const CWheel *pWheel, Vector3 &vDeformation) const;

	float GetTimeSpentLanded() const { return m_fTimeSpentLanded; }
	float GetTimeSpentCollidingNotLanded() const { return m_fTimeSpentCollidingNotLanded; }

	void SetInitialRopeLength( float length ) { m_InitialRopeLength = length; }
	float GetInitialRopeLength()	{ return m_InitialRopeLength; }

	void SetDisableExplodeFromBodyDamage( bool disable ) { m_bDisableExplodeFromBodyDamage = disable; }
	bool GetDisableExplodeFromBodyDamage() { return m_bDisableExplodeFromBodyDamage; }
	
	void SetCanPickupEntitiesThatHavePickupDisabled( bool allowPickup ) { m_bCanPickupEntitiesThatHavePickupDisabled = allowPickup; }
	bool GetCanPickupEntitiesThatHavePickupDisabled() { return m_bCanPickupEntitiesThatHavePickupDisabled; }

	void SetMainRotorDamageScale( float mainRotorDamageScale ) { m_fMainRotorHealthDamageScale = mainRotorDamageScale; }
	void SetRearRotorDamageScale( float rearRotorDamageScale ) { m_fRearRotorHealthDamageScale = rearRotorDamageScale; }
	void SetTailBoomDamageScale( float tailBoomDamageScale ) { m_fTailBoomHealthDamageScale = tailBoomDamageScale; }
	void SetJetpackStrafeForceScale(float val) { m_fJetpackStrafeForceScale = val; }	
	void SetJetPackThrusterThrotle(float val) { m_fJetPackThrusterThrotle = val; }

	float GetMainRotorDamageScale() { return m_fMainRotorHealthDamageScale; }
	float GetRearRotorDamageScale() { return m_fRearRotorHealthDamageScale; }
	float GetTailBoomDamageScale() { return m_fTailBoomHealthDamageScale; }
	float GetJetpackStrafeForceScale() { return m_fJetpackStrafeForceScale; }	
	float GetJetPackThrusterThrotle() { return m_fJetPackThrusterThrotle; }

protected:

	virtual void ProcessControlInputsInactive(CPed *pPlayerPed);

	virtual void ProcessFlightModel(float fTimestep);

	void ProcessAntiSway(float fTimeStep);
	void ProcessTurbulence(float fThrottleControl, float fTimeStep);
	void ProcessSteeringBias(float fTimeStep, float &fRollControl, float &fPitchControl);
	void ProcessTail(Vector3& vecAirSpeed, CFlyingHandlingData* pFlyingHandling, float fYawControl, float fEngineSpeed);

	// check if a rope is attached to an entity that isn't ourself
	bool		IsRopeAttachedToNonHeliEntity(const ropeInstance* pRopeInstance) const;
	void		GetRopeAttachPosition(Vector3& vAttachPos, bool bDriverSide, s32 iSeatId);
	void		UpdateRopes();
	void		ProcessLanded(float fTimeStep);

	CHeli(const eEntityOwnedBy ownedBy, u32 popType, VehicleType veh = VEHICLE_TYPE_HELI);
	friend class CVehicleFactory;

	atRangeArray<ropeInstance*, eNumRopeIds> m_Ropes;
	float				m_InitialRopeLength;

	// There are old heli ai variables and need cleaning up
	// They shouldn't live here!

	netPlayer* m_pOwnerPlayer;			// The player that has spawned this heli to chase this player.

	// landing gear
	CLandingGear m_landingGear;
    
	Vector3	m_vHeliSteeringBias;
    Vector3 m_vLastFramesAntiSwayEntityDistance;
	RegdPhy	m_pAntiSwayEntity; 
	float m_AntiSwaySpringDistance;
	float m_fEngineDegradeTimer;
	float m_turbulenceTimer;
	float m_turbulenceRecoverTimer;
	float m_turbulenceRoll;
	float m_turbulencePitch;
	float m_turbulenceDrop;
	float m_turbulenceScalar;
	float m_fTimeSpentLanded;
	float m_fTimeSpentCollidingNotLanded;
	float m_fJetPackGroundHeight;
	bool m_bPoliceDispatched;
	bool m_bSwatDispatched;
	bool m_bHasLandingGear;
	bool m_bDisableExplodeFromBodyDamage;
	bool m_bCanPickupEntitiesThatHavePickupDisabled;
	bool m_bDisableSelfRighting;

	ePickupRopeGadgetType m_nPickupRopeType;

	// Static methods and vars
public:	

	static bool				bPoliceHelisAllowed;
	static bool				bHeliControlsCheat;

	static void DoHeliGenerationAndRemoval();
	static void RemovePoliceHelisUponDeathArrest();
	
	static CHeli* GenerateHeli(CPed *pTargetPed, u32 mi, u32 driverPedMi = fwModelId::MI_INVALID, bool bFadeIn = false);
	static Vector3 FindSpawnCoordinatesForHeli(const CPed* pTargetPed, bool bReturnFirstValid = false);

	static void SwitchPoliceHelis(bool bSwitch);
	
	float ComputeWindMult();
	static float WeatherTurbulenceMult();

	NETWORK_OBJECT_TYPE_DECL( CNetObjHeli, NET_OBJ_TYPE_HELI );

#if __BANK
	static void InitRotorWidgets(bkBank& bank);
	static void InitTurbulenceWidgets(bkBank& bank);
#endif

	void ToggleWings( bool deploy );
	bool GetAreWingsDeployed();
	bool GetAreWingsFullyDeployed();
	bool GetAreWingsFullyRetracted();


protected:

	void ProcessWings();
	Vector3 CalculateHoverForce( float invTimeStep );
	static bank_float ms_fRotorSpeedMults[Max_Num_Rotors];
};

//////////////////////////////////////////////////////////////////////////
// Class CAutogyro
class CAutogyro : public CRotaryWingAircraft
{
public:

	virtual void SetModelId(fwModelId modelId);

	virtual void DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep);
//	virtual void ProcessDriverInputsForPlayer(CPed *pPlayerPed);
	virtual void ProcessFlightHandling(float fTimeStep);

	virtual void Teleport(const Vector3& vecSetCoors, float fSetHeading =-10.0f , bool bCalledByPedTask = false, bool bTriggerPortalRescan = true, bool bCalledByPedTask2 = false, bool bWarp=true, bool bKeepRagdoll = false, bool bResetPlants=true);

	void SetPropellerSpeed(float fSpeed){m_fRearPropellerSpeed = fSpeed;}

protected:
	CAutogyro(const eEntityOwnedBy ownedBy, const u32 popType);
	friend class CVehicleFactory;

	virtual void ProcessFlightModel(float fTimestep);

	void UpdatePropellerSpeed();

	float m_fRearPropellerSpeed;
	float m_fSpeedThroughMainRotor;

	static bank_float ms_fCeilingLiftCutoffRange;

	// How fast do props actually turn?
	static bank_float ms_fRotorSpeedMults[Num_Heli_Rotors];

public:
#if __BANK
	static void InitRotorWidgets(bkBank& bank);
#endif
};

//////////////////////////////////////////////////////////////////////////
// Class CBlimp
class CBlimp : public CHeli
{
public:
	virtual void				SetModelId(fwModelId modelId);

	// Rendering
	virtual ePrerenderStatus	PreRender(const bool bIsVisibleInMainViewport = true);
	virtual void				UpdateRotorSpeed();		
	virtual void				FinishBlowingUpVehicle( CEntity *pCulprit, bool bInACutscene = false, bool bAddExplosion = true, bool bNetCall = false, u32 weaponHash = WEAPONTYPE_EXPLOSION, bool bDelayedExplosion = false );//to be called after blowupcar

	bool IsBlimpBroken() { return m_isBlimpBroken; }
	void BreakBlimp();
protected:
	CBlimp(const eEntityOwnedBy ownedBy, const u32 popType);
	friend class CVehicleFactory;

private:

	bool m_isBlimpBroken;
};

#endif//_HELI_H_

