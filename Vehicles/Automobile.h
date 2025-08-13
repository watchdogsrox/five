// Title	:	Automobile.h
// Author	:	Richard Jobling/William Henderson
// Started	:	25/08/99
//
//				27/03/01 - Andrzej: AddWheelDirt() added;
//
//
//
#ifndef _AUTOMOBILE_H_
#define _AUTOMOBILE_H_

// Rage Headers
#include "phCore/segment.h"
#include "physics/constrainthandle.h"
#include "physics/intersection.h"

// Game Headers
#include "renderer/hierarchyIds.h"
#include "vehicles/handlingMgr.h"
#include "vehicles/vehicle.h"

#include "network/objects/entities/netObjAutomobile.h"


class phInstGta;
struct ConfigVehiclePositionLightSettings;
struct ConfigVehicleWhiteLightSettings;
namespace rage { class CNodeAddress; }

#define HORN_TIME	34	// how many frames the horn should be played (audio will create patterns)

#define MAX_ASYNC_PLACE_ON_ROAD_ENTRIES (16)
#define INVALID_ASYNC_ENTRY ((u32)-1)

//
//
//
class CAutomobile : public CVehicle
{
public:
	struct CAutomobileFlags
	{
		u8	bTaxiLight : 1;			// Is the wee light on the taxi switched on
		u8	bWaterTight : 1;		// If TRUE then non-player characters inside won't take damage if the car ends up in water
		u8	bIsBoggedDownInSand : 1;	// car is bogged down (sorta stuck but can move slowly) in sand
        u8  bFireNetworkCannon : 1; // if this is a networked fire truck this indicates the fire truck is firing it's cannon
        u8	bBurnoutModeEnabled : 1;	// car is in script burnout mode
		u8	bInBurnout			: 1;	// car is in a player driven burnout
        u8	bReduceGripModeEnabled : 1;	// car is in reduce grip mode
		u8	bPlayerIsRevvingEngineThisFrame : 1;
		u8  bLastSpaceAvaliableToChangeBounds : 1;
		u8  bDropsMoneyWhenBlownUp : 1;  // Used for security vans, etc. Will generate a bunch of money pickups when blown up
		u8  bHydraulicsBounceRaising : 1;
		u8  bHydraulicsBounceLanding : 1;
		u8  bHydraulicsJump : 1;
		u8	bForceUpdateGroundClearance : 1;
		u8	bWheelieModeEnabled : 1;
		u8	bInWheelieMode : 1;
		u8	bInWheelieModeWheelsUp : 1;
	};

	struct PlaceOnRoadAsyncData
	{
		Matrix34* matrix;
		u32 modelInfo;
		typedef WorldProbe::CShapeTestResults RoughHeightResults;
		typedef WorldProbe::CShapeTestResults WheelResults;
		RoughHeightResults* roughHeightResults;
		WheelResults* wheelResults;
		Vector3 aWheelPos[4];
		float frontHeightAboveGround;
		float rearHeightAboveGround;
		u32 id;
		RegdAutomobile automobile;
	};

protected:
	// This is PROTECTED as only CVEHICLEFACTORY is allowed to call this - make sure you use CVehicleFactory!
	// DONT make this public!
	CAutomobile(const eEntityOwnedBy ownedBy, u32 popType, VehicleType vehType);
	friend class CVehicleFactory;

public:
	~CAutomobile();

	virtual void SetModelId(fwModelId modelId);

	// virtual wheel stuff
	virtual void InitWheels();
	virtual void SetupWheels(const void* basePtr);
	virtual bool IsInAir(bool bCheckForZeroVelocity = true) const;

	// virtual door stuff
	virtual void InitDoors();
	// so derived versions of InitDoors can set the car door pointer in vehicle.h
	CCarDoor* GetCarDoorPointer() {return m_aCarDoors;}

	// process functions
	virtual void DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep);
	virtual void ProcessDriverInputOptions();
	virtual void ProcessFlightHandling(float fTimeStep);
	void ProcessSubmarineHandling(float fTimeStep);
	virtual void ProcessDriverInputsForStability(float fTimeStep);

	// physics
	virtual ePhysicsResult ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice);
	void StartWheelIntegratorTask(phCollider* pCollider, float fTimeStep);
	virtual ePhysicsResult ProcessPhysics2(float fTimeStep, int nTimeSlice);
	virtual ePhysicsResult ProcessPhysics3(float fTimeStep, int nTimeSlice);
#if GTA_REPLAY
	void ProcessReplayPhysics(float fTimeStep, int nTimeSlice);
#endif 

	void ProcessPossiblyTouchesWater(float fTimeStep, int nTimeSlice, const CVehicleModelInfo* pModelInfo);

	virtual void ProcessProbes(const Matrix34& testMat);
	void ProcessProbeResults(const Matrix34& testMat);

	void UpdateAirResistance();

	virtual int  InitPhys();
	virtual void InitCompositeBound();
	virtual void InitDummyInst();
	virtual void RemoveDummyInst();
	virtual bool UsesDummyPhysics(const eVehicleDummyMode vdm) const;
	virtual void ChangeDummyConstraints(const eVehicleDummyMode dummyMode, bool bIsStatic);
	virtual void UpdateDummyConstraints(const Vec3V * pDistanceConstraintPos=NULL, const float * pDistanceConstraintMaxLength=NULL);
	virtual bool HasDummyConstraint() const {return m_dummyStayOnRoadConstraintHandle.IsValid();}

#if __BANK
	void ValidateDummyConstraints(Vec3V_In vVehPos, Vec3V_In vConstraintPos, float fConstraintLength);
#endif

	bool RemoveConstraints();

	bool UpdateDesiredGroundGroundClearance(float fGroundClearance, bool brokenWheels = false, bool force = false);    // Make sure the bounds of the vehicle is always higher than the ground clearance, set force to true to bypass the check that this has already been done
	float GetGroundClearance(){return m_fGroundClearance;}
	void SetMinHydraulicsGroundClearance();

	bool IsCarFloating();	// Car is floating in water but hasn't started to sink yet

	// render
	virtual ePrerenderStatus PreRender(const bool bIsVisibleInMainViewport = true);
	virtual void PreRender2(const bool bIsVisibleInMainViewport = true);

	void ProcessSuspensionTransforms();
    void ProcessSuspensionTransformsForMiniTank();
    void ProcessFakeBodyMovement(bool bIsGamePaused);
	void PreRenderSuspensionMod(const float fFakeSuspensionLowerAmount);
	void SetFakeSuspensionLoweringAmount(float fSuspensionLowering){m_fFakeSuspensionLowerAmount = fSuspensionLowering;}
	float GetFakeSuspensionLoweringAmount() const { return m_fFakeSuspensionLowerAmount; }
	float GetFakeSuspensionLoweringAmountApplied() const { return m_fFakeSuspensionLowerAmountApplied; }
	void SetHydraulicsBounds(float fUpperBound, float lowerBound){m_fHydraulicsUpperBound = fUpperBound; m_fHydraulicsLowerBound = lowerBound;}
	void SetHydraulicsRate(float fRate){m_fHydraulicsRate = fRate;}
	void GetHydraulicsBounds(float &fUpperBound, float &lowerBound){fUpperBound = m_fHydraulicsUpperBound; lowerBound = m_fHydraulicsLowerBound;}
	float GetHydraulicsRate(){ return m_fHydraulicsRate; }
	bool HasHydraulicSuspension() const {return m_fHydraulicsUpperBound != 0.0f ||  m_fHydraulicsLowerBound != 0.0f;}
	void SetTimeHyrdaulicsModified(u32 uTimeModified) { m_iTimeHydraulicsModified = uTimeModified; }
	u32 GetTimeHydraulicsModified() const { return m_iTimeHydraulicsModified; }

	// damage
    virtual void Fix(bool resetFrag = true, bool allowNetwork = false);
	virtual void BlowUpCar( CEntity *pCulprit, bool bInACutscene = false, bool bAddExplosion = true, bool bNetCall = false, u32 weaponHash = WEAPONTYPE_EXPLOSION, bool bDelayedExplosion = false);
	virtual const eHierarchyId* GetExtraBonesToDeform(int& extraBoneCount);
	virtual void NotifyWheelPopped(CWheel *pWheel);
	virtual void ApplyDeformationToBones(const void* basePtr);

	void ApplySteeringBias(float bias, float lifetime);

	// script
	virtual void CleanupMissionState();

	// misc AI
	void ScanForCrimes();

	static const u32 DEFAULT_DOOR_TIME_TO_OPEN = 500;
	static const u32 DEFAULT_DOOR_TIME_TO_CLOSE = DEFAULT_DOOR_TIME_TO_OPEN;

	enum eStatus
	{
		OPEN_THEN_CLOSE,
		CLOSE_ONLY
	};

	void SetAutoDoorTimer(u32 nTime, eStatus status, u32 uDoorTimeToOpen = DEFAULT_DOOR_TIME_TO_OPEN, u32 uDoorTimeToClose = DEFAULT_DOOR_TIME_TO_CLOSE);
	void ProcessAutoBusDoors();
	void ProcessAutoTurretDoors();

	virtual void PlayCarHorn(bool bOneShot = false, u32 hornTypeHash = 0);

	void SetTaxiLight(bool bOn) { m_nAutomobileFlags.bTaxiLight = bOn; }

	// misc
	static void GetVehicleMovementStickInput(const CControl& rControl, float& fStickX, float& fStickY);
	static bool PlaceOnRoadProperly(CAutomobile* pAutomobile, Matrix34 *pMat, CInteriorInst*& pDestInteriorInst, s32& destRoomIdx, bool& setWaitForAllCollisionsBeforeProbe, u32 MI, phInst* pTestException, bool useVirtualRoad, CNodeAddress* aNodes, s16 * aLinks, s32 numNodes, float HeightSampleRangeUp = PLACEONROAD_DEFAULTHEIGHTUP, float HeightSampleRangeDown = PLACEONROAD_DEFAULTHEIGHTDOWN);	
	static u32 PlaceOnRoadProperlyAsync(CAutomobile* pAutomobile, Matrix34 *pMat, u32 MI, float HeightSampleRangeUp = PLACEONROAD_DEFAULTHEIGHTUP, float HeightSampleRangeDown = PLACEONROAD_DEFAULTHEIGHTDOWN);	
	static bool HasPlaceOnRoadProperlyFinished(u32 id, CInteriorInst*& pDestInteriorInst, s32& destRoomIdx, u32& failed, RegdEnt* hitEntities = NULL, float* hitPos = NULL);
	float GetHeightAboveRoad() const { return m_fHeightAboveRoad; }

	void EnableBurnoutMode(bool bEnableBurnout);
    virtual bool GetBurnoutMode() const { return m_nAutomobileFlags.bBurnoutModeEnabled; } //script burnout mode
	bool IsInBurnout() { return m_nAutomobileFlags.bInBurnout; } //player driven burnout
	void SetInBurnout(bool inBurnout) { m_nAutomobileFlags.bInBurnout = inBurnout; }

    void EnableReduceGripMode(bool bEnableReduceGrip) { m_nAutomobileFlags.bReduceGripModeEnabled = bEnableReduceGrip; }
    bool GetReduceGripMode(){ return m_nAutomobileFlags.bReduceGripModeEnabled; }
	void SetReduceGripLevel( int reducedGripLevel );

	bool GetReducedSuspensionForce();
	void SetReducedSuspensionForce( bool bReduceSuspensionForce );

	// Handbrake
	// For automobiles we fade the handbrake over time
	// So use this function to get the strength
	float GetHandBrakeForce() const;
	float GetHandBrakeGripMult() const;	// Lower traction on wheels after pulling the handbrake.. to try and make it easier to drift

	void SetSteeringBiasScalar(float fScalar) {m_fSteeringBiasScalar = fScalar;}

#if __BANK
	virtual void RenderDebug() const;
	void DrawGForce() const;
	void DrawAngularAcceleration() const;

	static u32 GetAsyncQueueCount();
#endif

	float GetFakeRotationY() {return m_fFakeRotationY;}

	void ResetHydraulics();
	void UpdateHydraulicsFlags( CAutomobile* pAutomobile );

protected:
	virtual bool PlaceOnRoadAdjustInternal(float HeightSampleRangeUp, float HeightSampleRangeDown, bool bJustSetCompression);

	// lights
	void DoWhiteLightEffects(s32 boneIdx, const ConfigVehicleWhiteLightSettings &lightParam, bool frontLight, float distFade);
	void DoPosLightEffects(s32 boneIdx, const ConfigVehiclePositionLightSettings &lightParam, bool isRight, bool isAnnihilator, float distFade);

	bool ShouldUpdateVehicleDamageEveryFrame() const;

	void ModifyControlsIfDucked(const CPed* pDriver, float fTimeStep);
	void FixHydraulicSuspensionClipping();

	static s32 CleanUpPlacementArray(bool getFirstFree);

	// Why are these member variables public?
	// Todo: make protected

public:
	WheelIntegrationState m_WheelIntegrator;

	static float CAR_INAIR_ROT_X;
	static float CAR_INAIR_ROT_Y;
	static float CAR_INAIR_ROT_Z;

	static float CAR_STUCK_ROT_X;
	static float CAR_STUCK_ROT_Y;
	static float CAR_STUCK_ROT_Y_SELF_RIGHT; // Torque only allowed in self righting direction

	static float CAR_INAIR_ROTLIM;
	static float CAR_INAIR_REDUCED_ROTLIM;

	static float CAR_BURNOUT_SPEED;

	static float CAR_BURNOUT_ROT_Z;
	static float CAR_BURNOUT_ROTLIM;
	static float CAR_BURNOUT_WHEELS_OFFGROUND_MULT;

	static float QUADBIKE_BURNOUT_ROT_Z;
	static float QUADBIKE_BURNOUT_ROTLIM;
	static float QUADBIKE_BURNOUT_WHEEL_SPEED_MULT;

	static float OFFROAD_VEH_BURNOUT_ROT_Z;
	static float OFFROAD_VEH_BURNOUT_ROTLIM;
	static float OFFROAD_VEH_BURNOUT_WHEEL_SPEED_MULT;

	static float BIG_CAR_BURNOUT_ROT_Z;
	static float BIG_CAR_BURNOUT_ROTLIM;
	static float BIG_CAR_BURNOUT_WHEEL_SPEED_MULT;

	static float BURNOUT_WHEEL_SPEED_MULT;
	static float BURNOUT_TORQUE_MULT;

	static float POSSIBLY_STUCK_SPEED;
	static float COMPLETELY_STUCK_SPEED;

	static bool ms_bUseAsyncWheelProbes;
	static float ms_fBumpSeverityMulti;
	BANK_ONLY(static bool ms_bDrawWheelProbeResults;)

	phConstraintHandle m_dummyStayOnRoadConstraintHandle;
	phConstraintHandle m_dummyRotationalConstraint;

	CAutomobileFlags m_nAutomobileFlags;

	virtual void ProcessTimeStartedFailingRealConversion();
	void SetStartedFailingRealConversion();
	u32 GetTimeStartedFailingRealConversion() const {return m_TimeStartedFailingRealConversion;}
	float GetMaxLengthForDummyConstraintsAfterFailedConversion(bool& bMaxShrinkage) const;
	virtual bool HasShrunkDummyConstraintsAsMuchAsPossible() const
	{
		bool bMaxShrinkage = false;
		GetMaxLengthForDummyConstraintsAfterFailedConversion(bMaxShrinkage);
		return bMaxShrinkage;
	}
	virtual void ResetRealConversionFailedData()
	{
		m_TimeStartedFailingRealConversion = 0;
		m_fConstraintDistanceWhenRealConversionFailed = -1.0f;
	}

	float m_fConstraintDistanceWhenRealConversionFailed;
	u32 m_TimeStartedFailingRealConversion;
	 
	u32 m_nAutoDoorTimer;
	u32 m_nAutoDoorStart;
	float m_fHeightAboveRoad;
	
	float m_fGroundClearance;
	float m_fLastGroundClearance;
	float m_fLastGroundClearanceHeightAboveGround;
	u32 m_uLastGroundClearanceCheck;

    float m_bIgnoreWorldHeightMapForWheelProbes;

	float m_fTimeInWater;			// How long we have been sinking in the water (i.e. car is dead in water) Only resets when we leave water... so can accumulate over time
	float m_fOrigBuoyancyForceMult;	// Keep track of original float multiplier so we can reset it

	bool CanDeployParachute() { return m_bCanDeployParachute; }

	// Request start car parachuting sequence
	void StartParachuting();

	// Finish parachuting sequence
	// This attempts to finish parachuting once and is not guaranteed to finish parachuting (if the parachute is being created at the time of calling for example)
	void FinishParachuting();

	// Put in a request to finish parachuting
	// This repeatedly attempts to destroy the parachute until it is successful
	void RequestFinishParachuting() { m_bFinishParachutingRequested = true; }

	// Set/Get for whether the player can cancel parachuting
	void SetCanPlayerDetachParachute(bool bNewValue) { m_bCanPlayerDetachParachute = bNewValue; }
	bool GetCanPlayerDetachParachute() { return m_bCanPlayerDetachParachute; }

	bool IsFinishParachutingRequested() { return m_bFinishParachutingRequested; }

	void SetCloneParachuting(bool parachuting)
	{
		if(GetNetworkObject() && GetNetworkObject()->IsClone())
		{
			parachuting ? m_carParachuteState = CLONE_CAR_PARACHUTING : m_carParachuteState = CAR_NOT_PARACHUTING;
		}
	}

	// Accessor to check if a car is in parachute mode
	bool IsParachuting() const { return m_carParachuteState != CAR_NOT_PARACHUTING; }

	bool IsDeployingParachute() const { return m_carParachuteState == CAR_DEPLOYING_PARACHUTE; } 

	const CObject* GetParachuteObject() const { return m_pParachuteObject.Get(); }

	void SetParachuteTintIndex(u8 idx)	{ m_nParachuteTintIndex=idx;	}
	u8	 GetParachuteTintIndex() const	{ return m_nParachuteTintIndex;	};

	void GetCloneParachuteStickValues(float &xVal, float &yVal) { xVal = m_fCloneParachuteStickX; yVal = m_fCloneParachuteStickY; }

	void SetCloneParachuteStickValues(float xVal, float yVal) { m_fCloneParachuteStickX = xVal; m_fCloneParachuteStickY = yVal; }

private:
	// arrays of wheels and doors
	CCarDoor m_aCarDoors[NUM_VEH_DOORS_MAX];

	// The array needs to be aligned so that it can be sent to the SPU
	// The SPU task needs to know the address of the PPU wheels so that it can send them back
	CWheel* m_pCarWheels[NUM_VEH_CWHEELS_MAX] ;	// Store array of pointers so they don't have to be consecutive

	NETWORK_OBJECT_TYPE_DECL( CNetObjAutomobile, NET_OBJ_TYPE_AUTOMOBILE );

	static atFixedArray<CAutomobile::PlaceOnRoadAsyncData, MAX_ASYNC_PLACE_ON_ROAD_ENTRIES> ms_placeOnRoadArray;

	WorldProbe::CShapeTestSingleResult* m_pWheelProbeResults;
	WorldProbe::CShapeTestHitPoint* m_pWheelProbePreviousResults;

	float m_fFakeRotationY;

    float m_fHighSpeedRotationY;
    float m_fHighSpeedDirectionY;
    float m_fHighSpeedYTime;

    float m_fBumpSeverityY;
    float m_fBumpRateY;
    float m_fBumpSeverityYDecay;

    float m_fFakeSuspensionLowerAmount;
	float m_fFakeSuspensionLowerAmountApplied; // How much suspension lowering we have done in prerender 2

	float m_fHydraulicsUpperBound;
	float m_fHydraulicsLowerBound;
	float m_fHydraulicsRate;
	u32	  m_iTimeHydraulicsModified;

	float m_fBurnoutRotationTime;

	float m_fSteeringBias;
	float m_fSteeringBiasLife;
	float m_fSteeringBiasScalar;

	// PURPOSE:	Used by ProcessPhysics() to hold the last return value from
	//			CTransmission::Process(), so we can apply it if we skip this call
	//			on the second simulation step.
	float m_fLastDriveForce;

	Vector3 m_vSuspensionDeformationLF;
	Vector3 m_vSuspensionDeformationRF;

    static float m_sfPassengerMassMult;

	// Car parachute /////////////////////////////////

	RegdObj m_pParachuteObject;
	strRequest m_ModelRequestHelper;
	u32 m_iParachuteModelIndex;
	bool m_wasCloneParachuting;

	float m_fParachuteStickX;
	float m_fParachuteStickY;

	float m_fCloneParachuteStickX;
	float m_fCloneParachuteStickY;

	float m_fParachuteHatchOffset;

	u8	m_nParachuteTintIndex;

	bool m_bCanDeployParachute;

	bool m_bFinishParachutingRequested;

	bool m_bCanPlayerDetachParachute;

	bool m_bHasBeenParachuting;

public:
	enum ECarParachuteState
	{
		CAR_NOT_PARACHUTING,
		CAR_STABILISING,
		CAR_CREATING_PARACHUTE,
		CAR_DEPLOYING_PARACHUTE,
		CAR_PARACHUTING,
		CLONE_CAR_PARACHUTING
	};

private:

	ECarParachuteState m_carParachuteState;

	void ProcessParachutePhysics();
	void ProcessCarParachute();
	void ProcessCarParachuteTint();
	void StreamInParachute();
	bool IsParachuteStreamedIn() { return m_ModelRequestHelper.HasLoaded(); }
	void CreateParachute();
	void AttachParachuteToVehicle();
	void DetachParachuteFromVehicle();
	void DestroyParachute();
	

public:
	bool IsParachuteDeployed() const;
	void SetParachuteState(ECarParachuteState parachuteState)	{ m_carParachuteState = parachuteState; } // this was added for replay
	ECarParachuteState GetParachuteState()						{ return m_carParachuteState; } // this was added for replay

	// End Car parachute


private:

	void SlipperyBoxInitialise();

	float m_fSlipperyBoxLimitX;
	float m_fSlipperyBoxLimitY;

    void ApplyDifferentials();

public:
	void SlipperyBoxUpdateLimits( float limitModifier );
	void SlipperyBoxBreakOff();

};



//
//
//
class CQuadBike : public CAutomobile
{
protected:
    // This is PROTECTED as only CVEHICLEFACTORY is allowed to call this - make sure you use CVehicleFactory!
    // DONT make this public!
    CQuadBike(const eEntityOwnedBy ownedBy, u32 popType);
    friend class CVehicleFactory;

public:
    ~CQuadBike();

    // physics
	virtual ePhysicsResult ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice);
	virtual void ProcessPostPhysics();
	virtual void ProcessDriverInputsForStability( float fTimeStep );
	virtual ePrerenderStatus PreRender( const bool bIsVisibleInMainViewport );

#if __BANK
	static void InitWidgets(bkBank& pBank);
#endif // __BANk

	static bank_float ms_fQuadBikeSelfRightingTorqueScale;
	static bank_float ms_fQuadBikeAntiRollOverTorqueScale;
};

#endif//_AUTOMOBILE_H_

