// Title	:	Bike.h
// Author	:	Alexander Roger
// Started	:	08/10/2001 (adaped from Automobile.h)
//
//
//
//
#ifndef _BIKE_H_
#define _BIKE_H_

// Rage headers
#include "phCore/segment.h"
#include "physics/constrainthandle.h"
#include "physics/intersection.h"

// Game headers
#include "renderer/hierarchyIds.h"
#include "vehicles/door.h"
#include "vehicles/handlingMgr.h"
#include "vehicles/vehicle.h"
#include "vehicles/wheel.h"

#include "network/objects/entities/netObjBike.h"
#include "System/PadGestures.h"					// for define

namespace rage { class CNodeAddress; }

///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////
extern float BIKE_ON_STAND_STEER_ANGLE;


#define NUM_BIKE_CWHEELS_MAX (2)

#define	SANCHEZ_MP_DRIVE_FORCE_ADJUSTMENT 17.5f

class CBike : public CVehicle
{
#if GTA_REPLAY
	friend class VehicleBikePacket;
#endif

public:
	struct CBikeFlags
	{
		u8	bWaterTight : 1;		// If TRUE then non-player characters inside won't take damage if the car ends up in water
		u8	bGettingPickedUp : 1;	// if true then stabilisation activated to raise bike to vertical
		u8	bOnSideStand : 1;		// if true stabilisation activated and bike sits at angle (until knocked over)
		u8	bJumpPressed : 1;
		u8	bJumpReleased : 1;
		u8	bLateralJump : 1;
		u8  bHasFrontSuspension : 1;
		u8  bHasRearSuspension : 1; 
		u8  bJumpOffTrain : 1;		//On the mission where we drive along a train we want to vary the jumps
		u8  bInputsUpToDate : 1;	//Keeps track of whether inputs are up to date.
		u8  bUsingMouseSteeringInput : 1; // Are we currently using mouse steering.
	};
	
protected:
	// This is PROTECTED as only CVEHICLEFACTORY is allowed to call this - make sure you use CVehicleFactory!
	// DONT make this public!
	CBike(const eEntityOwnedBy ownedBy, u32 popType, VehicleType vehType);
	friend class CVehicleFactory;

	virtual void UpdatePhysicsFromEntity(bool bWarp=false);

public:
	~CBike();

	virtual int  InitPhys();
	virtual void RemovePhysics();
	virtual void InitDummyInst();
	virtual bool UsesDummyPhysics(const eVehicleDummyMode vdm) const;

	void SwitchToFullFragmentPhysics(const eVehicleDummyMode prevMode);
	void SwitchToDummyPhysics(const eVehicleDummyMode prevMode);
	void SwitchToSuperDummyPhysics(const eVehicleDummyMode prevMode);

	// PURPOSE: If a bike is above the player and lying on the ground, we
	// should orientate it up to avoid funky issues with ik.
	// PARAMS:
	// - pVehicle: Bike to check if should be orientated up.
	// RETURNS: True if the bike is lying on its side and above the height of
	// the ped.
	virtual bool ShouldOrientateVehicleUpForPedEntry(const CPed* pPed) const;

	// virtual wheel stuff
	virtual void InitWheels();
	virtual void InitDoors();
	virtual void InitCompositeBound();	

	virtual void SetupWheels(const void* basePtr);
	virtual bool IsInAir(bool bCheckForZeroVelocity = true) const;
	virtual void ProcessProbes(const Matrix34& testMat);

	bool IsUnderwater() const;

	virtual float GetSteerRatioForAnim() { return m_fAnimSteerRatio; }
	float GetHeightAboveRoad() const { return m_fHeightAboveRoad; }
	float GetLeanAngle() const { return m_fBikeLeanAngle; }

	virtual float ProcessDriveForce(float fTimeStep, float fDriveWheelSpeedsAverage, Vector3::Vector3Param vDriveWheelGroundVelocitiesAverage, int nNumDriveWheels, int nNumDriveWheelsOnGround);

	virtual void DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep);

	virtual ePhysicsResult ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice);
	virtual ePhysicsResult ProcessPhysics2(float fTimeStep, int nTimeSlice);
	virtual ePhysicsResult ProcessPhysics3(float fTimeStep, int nTimeSlice);
	virtual void ProcessPreComputeImpacts( phContactIterator impacts );
	virtual void ProcessPostPhysics();

    void DoStabilisation(bool bDoStabilisation, float &fDesiredLeanAngle, u32 nNumWheelsOnGround, Vector3 &vBikeVelocity, Vector3 &vTyreForceApplied, float fTimeStep);
	
    void NewLeanMethod( float fDesiredLeanAngle, float fTimeStep);
    void OldLeanMethod( float fDesiredLeanAngle, float fTimeStep);

    void EnforceMinimumGroundClearance();
	float ComputeInstabilityModifier() const;
    void ProcessStability(bool bDoStabilisation, float fDesiredLeanAngle, const Vector3& vecBikeVelocity, float fTimeStep);
	void UpdateStoredFwdVector(const Matrix34& mat, float fTimeStep = -1.0f);	// timestep < 0.0f means reset blending
	void ProcessDriverInputsForStability(float fTimeStep, float& fLeanCgFwd, Vector3 &vecBikeVelocity);

	bool GetIsOffRoad() const;
	void GetBikeLeanInputs(float& fInputX, float& fInputY);

	void ProcessBikeJumping();

	virtual bool WantsToBeAwake();
	virtual void Teleport(const Vector3& vecSetCoors, float fSetHeading=-10.0f, bool bCalledByPedTask=false, bool bTriggerPortalRescan = true, bool bCalledByPedTask2=false, bool bWarp=true, bool bKeepRagdoll = false, bool bResetPlants=true);

	virtual ePrerenderStatus PreRender(const bool bIsVisibleInMainViewport = true);
	virtual void PreRender2(const bool bIsVisibleInMainViewport = true);

	void FixUpSkeletonNoSuspension();

	virtual void BlowUpCar( CEntity *pCulprit, bool bInACutscene = false, bool bAddExplosion = true, bool bNetCall = false, u32 weaponHash = WEAPONTYPE_EXPLOSION, bool bDelayedExplosion = false );

	void SetBikeOnSideStand(bool bOn, bool bSetSteeringAndLeanAngles=false, float fOnStandSteerAngle = FLT_MAX, float fOnStandLeanAngle = FLT_MAX);

	virtual bool UseBikeGetOn() const {return true;}
	
	static bool PlaceOnRoadProperly(CBike* pBike, Matrix34 *pMat, CInteriorInst*& pDestInteriorInst, s32& destRoomIdx, bool& setWaitForAllCollisionsBeforeProbe, u32 MI, phInst* pTestException, bool useVirtualRoad, CNodeAddress* aNodes, s16 * aLinks, s32 numNodes, float HeightSampleRangeUp = PLACEONROAD_DEFAULTHEIGHTUP, float HeightSampleRangeDown = PLACEONROAD_DEFAULTHEIGHTDOWN, bool bDontChangeMatrix = false);	

	void PlayHornIfNecessary(void);
	virtual void PlayCarHorn(bool bOneShot = false,u32 hornTypeHash = 0);

	void CalcAnimSteerRatio();

	void InitConstraint();
	void ProcessConstraint(bool bConstrainLean);
	void RemoveConstraint();

	void KnockAllPedsOffBike();
	bool IsConsideredStill(const Vector3& vVelocity, bool bIgnorePreventLeanTest = false) const;

	virtual void CleanupMissionState();

	void SetRearBrake(float fRearBrake){m_fRearBrake = fRearBrake;} // Amount the rear brake button is pressed.

	Vector3 GetBikeWheelUp(){ return m_vecBikeWheelUp;}

	static void SetDisableExtraTrickForces( bool bDisableForces ) { ms_bDisableTrickForces = bDisableForces; }

protected:
	virtual bool PlaceOnRoadAdjustInternal(float HeightSampleRangeUp, float HeightSampleRangeDown, bool bJustSetCompression);

	void EnforceMinimumGroundClearanceInternal(float leanAngle, float clearance);

public:
	CBikeFlags m_nBikeFlags;

	WheelIntegrationState m_WheelIntegrator;

	float m_fLeanFwdInput;
	float m_fLeanSideInput;

	float m_fAnimLeanFwd;
	float m_fAnimSteerRatio;

	float m_fGroundLeanAngle;
	float m_fBikeLeanAngle;

	float m_fRearBrake;

	float m_fCurrentGroundClearanceAngle;
	float m_fCurrentGroundClearanceHeight;

	float m_fFwdDirnMult;		// This won't be needed once we are sure we want to keep ms_bSecondNewLeanMethod
	Vector3 m_vecFwdDirnStored;	// This won't be needed once we are sure we want to keep ms_bSecondNewLeanMethod
	Vector3 m_vecBikeWheelUp;

	float m_fHeightAboveRoad;

	float m_fBurnoutTorque;

	float m_fTimeUnstable;

	// How long we have been sinking in the water (i.e. car is dead in water) Only resets when we leave water... so can accumulate over time
	float m_fTimeInWater;
	float m_fOrigBuoyancyForceMult;	// Keep track of original float multiplier so we can reset it

	// Physics constraint to keep the bike at specified lean angle
	phConstraintHandle m_leanConstraintHandle;

    Vector3 m_vecAverageGroundNormal;

    bool m_RotateUpperForks;// some bikes have their forks connected as children to their handlebars so don't need rotating

	u32 m_uHandbrakeLastPressedTime; // When was the handbrake released.
	u32 m_uJumpLastPressedTime; // When was the jump button last pressed.
	u32 m_uJumpLastReleasedTime; // When was the jump button last released.
	float m_fJumpLaunchPitch;// The pitch when the player left the ground.
	float m_fJumpLaunchHeading;// The heading when the player left the ground.

	float m_fOnTrainLaunchDesiredPitch; //On the mission where we drive along a train we want to vary the jumps, this is the amount of pitch we want
	float m_fOffStuntTime;

	// Values used by skeleton fixup in PreRender for bikes without suspension
	static bool ms_bEnableSkeletonFixup;	
	static float ms_fSkeletonFixupMaxRot;			// The maximum fixup rotation angle
	static float ms_fSkeletonFixupMaxTrans;			// The maximum fixup translation distance
	static float ms_fSkeletonFixupMaxRotChange;		// How much we can change the fixup rotation angle between frames
	static float ms_fSkeletonFixupMaxTransChange;	// How much we can change the fixup translation between frames

	Vec3V m_vecSkeletonFixupPreviousTranslation; // How much we translated the bike the previous frame
	float m_fSkeletonFixupPreviousAngle;	// How much we rotated the bike's skeleton about the local x-axis last frame

	// Foliage constants specific to bikes:
	static bank_float ms_fFoliageBikeDragCoeff;
	static bank_float ms_fFoliageBikeDragDistanceScaleCoeff;
	static bank_float ms_fFoliageBikeBoundRadiusScaleCoeff;

	static bank_float ms_fMaxTimeUnstableBeforeKnockOff;

	static bank_bool ms_bNewLeanMethod;
	static bank_bool ms_bLeanBikeWhenDoingBurnout;

	static bank_float ms_fBikeWaterHeightTolerance;

	static bank_float ms_fBikeJumpFullPowerTime;

#if USE_SIXAXIS_GESTURES
	static bank_float ms_fMotionControlPitchMin;
	static bank_float ms_fMotionControlPitchMax;
	static bank_float ms_fMotionControlRollMin;
	static bank_float ms_fMotionControlRollMax;
	// Note the deadzones get scaled to fit the MIN/MAX range defined above
	static bank_float ms_fMotionControlPitchDownDeadzone;
	static bank_float ms_fMotionControlPitchUpDeadzone;
#endif

	// Second new lean values (see CBike::OldLeanMethod for uses)
	static bank_bool ms_bSecondNewLeanMethod;
	static bank_float ms_fSecondNewLeanZAxisCosine;			// ground normals that have a larger cosine than this when dotted with the z-axis will be set to the z-axis
	static bank_float ms_fSecondNewLeanGroundNormalCosine;	// ground normals that have a smaller cosine than this when dotted with the z-axis will be unmodified (anything between these two cosines will be interpolated)
	static bank_float ms_fSecondNewLeanMaxAngularVelocityGroundSlow; // Max angular velocity when on the ground at low speeds
	static bank_float ms_fSecondNewLeanMaxAngularVelocityGroundFast; // Max angular velocity when on the ground at high speeds
	static bank_float ms_fSecondNewLeanMaxAngularVelocityAirSlow;	// Max angular velocity when in air at low speeds
	static bank_float ms_fSecondNewLeanMaxAngularVelocityAirFast;	// Max angular velocity when in air at high speeds
	static bank_float ms_fSecondNewLeanMaxAngularVelocityPickup; 

	static bank_float ms_fMaxGroundTiltAngleSine;
	static bank_float ms_fMinAbsoluteTiltAngleSineForKnockoff;

	static bank_float ms_TimeBeforeLeavingStuntMode;
#if __BANK
	static void InitWidgets(bkBank& pBank);

	static void KnockFocusPedOffBike();
#endif

private:	
	CCarDoor m_aBikeDoors[1];

	// The array needs to be aligned so that it can be sent to the SPU
	// The SPU task needs to know the address of the PPU wheels so that it can send them back
	CWheel* m_pBikeWheels[NUM_BIKE_CWHEELS_MAX] ;
	static bool ms_bDisableTrickForces;

	NETWORK_OBJECT_TYPE_DECL( CNetObjBike, NET_OBJ_TYPE_BIKE );
};

#endif//_BIKE_H_
