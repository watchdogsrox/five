//
// Title	:	Boat.h
// Author	:	William Henderson
// Started	:	23/02/00
//
//
//
//
//
//
//
#ifndef _BOAT_H_
#define _BOAT_H_
#include "renderer\hierarchyIds.h"
//#include "vehicles/AnchorHelper.h"
#include "vehicles\door.h"
#include "vehicles\handlingMgr.h"
#include "vehicles\vehicle.h"
#include "network\objects\entities\netObjBoat.h"
#include "System/PadGestures.h"					// for define
#include "control/replay/ReplaySettings.h"

//
//
//
//
typedef enum
{
	BFT_THRUST,
	BFT_THRUST_HEIGHT,
	BFT_THRUST_VECTOR_HEIGHT,
	BFT_HEIGHT_MULT,
	BFT_RUDDER_ANGLE,
	BFT_RESIST_X,
	BFT_RESIST_Y,
	BFT_RESIST_Z,
	BFT_TURN_RESIST_X,
	BFT_TURN_RESIST_Y,
	BFT_TURN_RESIST_Z,
	BFT_MAX
} tBFT;

// What will the boat do when it gets wrecked?
enum eBoatWreckedAction
{
	BWA_UNDECIDED,
	BWA_FLOAT,
	BWA_SINK
};

#define NUM_WAKE_PTS (32)
#define NUM_WAKE_BOATS (4)

#define BOATFX_CULL_DIST (80.0f)
#define BOATFX_BLEND_DIST (50.0f)

#define BOAT_WATER_SAMPLES (25)

static const int nMaxNumBoatPropellers = 2;

struct ConfigBoatLightSettings;

class CBoatHandling 
{

public:
	CBoatHandling();

	void SetModelId( CVehicle* pVehicle, fwModelId modelId );
	void DoProcessControl( CVehicle* pVehicle );
	ePhysicsResult ProcessPhysics( CVehicle* pVehicle, float fTimeStep );

	float GetLowLodBuoyancyDistance() const					{ return m_fAnchorBuoyancyLodDistance; }
	void SetLowLodBuoyancyDistance( float fLodDistance )	{ m_fAnchorBuoyancyLodDistance = fLodDistance; }
	bool OverridesLowLodBuoyancyDistance() const			{ return m_fAnchorBuoyancyLodDistance >= 0.0f; }

	void SetPitchFwdInput( float pitchFwdInput )	{ m_fPitchFwdInput = pitchFwdInput; }
	float GetPitchFwdInput()						{ return m_fPitchFwdInput; }

	float GetOutOfWaterTime() const			{ return m_fOutOfWaterCounter; }
	float GetOutOfWaterTimeForAnims() const { return m_fOutOfWaterForAnimsCounter; }

	void UpdatePropeller( CVehicle* pVehicle );

#if GTA_REPLAY
	float GetRudderAngle() const						{ return m_fRudderAngle; }
	void SetRudderAngle( float angle )					{ m_fRudderAngle = angle;}
	const Vector3& GetPropDeformOffset1() const			{ return m_vecPropDefomationOffset; }
	void SetPropDeformOffset1( const Vector3& offset )	{ m_vecPropDefomationOffset = offset; }
	const Vector3& GetPropDeformOffset2() const			{ return m_vecPropDefomationOffset2; }
	void SetPropDeformOffset2( const Vector3& offset )	{ m_vecPropDefomationOffset2 = offset; }
#endif // GTA_REPLAY

	bool IsInWater() const { return m_nBoatFlags.bBoatInWater; }
	u8 GetWreckedAction() { return m_nBoatFlags.nBoatWreckedAction; }
	void SetWreckedAction( u8 wreckedAction ) { m_nBoatFlags.nBoatWreckedAction = wreckedAction; }
	float GetCapsizedTimer() { return m_fBoatCapsizedTimer; }

	void SetInWater( u8 inWater ) { m_nBoatFlags.bBoatInWater = inWater; }
	void SetEngineInWater( u8 inWater ) { m_nBoatFlags.bBoatEngineInWater = inWater; }

	float GetPropellerDepth() { return m_fPropellerDepth; }

	u32 GetLowLodBuoyancyModeTimer() { return m_nLowLodBuoyancyModeTimer; }
	void SetLowLodBuoyancyModeTimer( u32 timer ) { m_nLowLodBuoyancyModeTimer = timer; }

	bool IsWedgedBetweenTwoThings() { return m_isWedgedBetweenTwoThings; }
	void SetIsWedgedBetweenTwoThings( bool isWedged ) { m_isWedgedBetweenTwoThings = isWedged; }

	u32 GetLastTimePropInWater() { return m_nLastTimePropInWater; }

	void Fix() { m_vecPropDefomationOffset.Zero();	m_vecPropDefomationOffset2.Zero(); }

	u32 GetPlaceOnWaterTimeout() { return m_nPlaceOnWaterTimeout; }
	void SetPlaceOnWaterTimeout( u32 placeOnWaterTimeout ) { m_nPlaceOnWaterTimeout = placeOnWaterTimeout; }

	u8 GetPropellerSubmerged() { return m_nBoatFlags.bPropellerSubmerged; }

private:
	void UpdateRudder( CVehicle* pVehicle );
	void UpdateProw( CVehicle* pVehicle );
	

	struct CBoatFlags
	{
		u8 bBoatInWater 		: 1;
		u8 bBoatEngineInWater	: 1;
		u8 nBoatWreckedAction	: 3;	// What to do when the boat is wrecked, decided when InitPhysics() is called
		u8 bRaisingProw			: 1;	// Are we actively ramping the force up to lift the prow out of the water?
		u8 bPropellerSubmerged	: 1;	// Is the propeller actually under water?
	};
	CBoatFlags m_nBoatFlags;

	int m_propellersBoneIndicies[ nMaxNumBoatPropellers ];

	u32 m_nProwBounceTimer;
	u32 m_nIntervalToNextProwBump;

	u32 m_nLastTimePropInWater;

	Vector3 m_vecPropDefomationOffset;
	Vector3 m_vecPropDefomationOffset2;

	int m_nNumPropellers;
	float m_fPropellerAngle;
	float m_fPropellerDepth;
	float m_fRudderAngle;

	float m_fOutOfWaterCounter;
	float m_fOutOfWaterForAnimsCounter;
	float m_fBoatCapsizedTimer;

//protected:
	// How much the driver wants to pitch the boat forward / backward
	// -1.0f .. to 1.0f
	float m_fPitchFwdInput;

	float m_fProwForceMag;

	// If script or someone else overrides the default low LOD anchor buoyancy distance (defined in floater.cpp), this will be greater than zero.
	float m_fAnchorBuoyancyLodDistance;
	u32 m_nLowLodBuoyancyModeTimer;

	u32 m_nPlaceOnWaterTimeout;

	// B*1807324: Prevent the boat from overriding inverse angular inertia when it is trapped between two things on either side of it.
	bool m_isWedgedBetweenTwoThings;
};

//
//
//
//
class CBoat : public CVehicle
{
protected:
	// This is PROTECTED as only CVEHICLEFACTORY is allowed to call this - make sure you use CVehicleFactory!
	// DONT make this public!
	CBoat(const eEntityOwnedBy ownedBy, u32 popType);
	friend class CVehicleFactory;

protected:
	virtual bool PlaceOnRoadAdjustInternal(float HeightSampleRangeUp, float HeightSampleRangeDown, bool bJustSetCompression);

public:
	~CBoat();

	virtual void SetModelId(fwModelId modelId);

	virtual int  InitPhys();
	virtual void AddPhysics();
	virtual void RemovePhysics();

    CSearchLight *GetSearchLight();

	virtual void DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep);
	bool ProcessLowLodBuoyancy();

	virtual ePhysicsResult ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice);
	virtual ePhysicsResult ProcessPhysics2(float fTimeStep, int nTimeSlice);
	virtual void UpdateEntityFromPhysics(phInst *pInst, int nLoop);

	CAnchorHelper& GetAnchorHelper() { return m_AnchorHelper; }

	void ComputeLowLodBuoyancyPlaneNormal(const Matrix34& boatMat, Vector3& vPlaneNormal);
	static void UpdateBuoyancyLodMode( CVehicle* pVehicle, CAnchorHelper& anchorHelper );

	virtual void SetupMissionState();

	virtual bool WantsToBeAwake() { return (m_BoatHandling.IsInWater() || GetThrottle()); }

	virtual void ProcessPreComputeImpacts(phContactIterator impacts);

	virtual ePrerenderStatus PreRender(const bool bIsVisibleInMainViewport = true);
	virtual void PreRender2(const bool bIsVisibleInMainViewport = true);
	
	virtual void BlowUpCar( CEntity *pCulprit, bool bInACutscene = false, bool bAddExplosion = true, bool bNetCall = false, u32 weaponHash = WEAPONTYPE_EXPLOSION, bool bDelayedExplosion = false);
	virtual const eHierarchyId* GetExtraBonesToDeform(int& extraBoneCount);

	// different model setup so alt fixing model fns
	virtual void Fix(bool resetFrag = true, bool allowNetwork = false);

	virtual void ApplyDeformationToBones(const void* basePtr);

	void UpdatePropeller();

    void SetDamping();
	void UpdateAirResistance();

	virtual bool IsInAir(bool bCheckForZeroVelocity = true) const;
	virtual bool HasLandedStably() const;

	void DoLight(s32 boneID, ConfigBoatLightSettings &lightParam, float fade, CVehicleModelInfo* pModelInfo, bool addCorona, float coronaFade);
	NETWORK_OBJECT_TYPE_DECL( CNetObjBoat, NET_OBJ_TYPE_BOAT );

	CBoatHandling* GetBoatHandling() { return &m_BoatHandling; }
	const CBoatHandling* GetBoatHandling() const { return &m_BoatHandling; }
#if __BANK
	static void InitWidgets(bkBank& bank);
    virtual void RenderDebug() const;
	static void DisplayAngularOffsetForLowLod( CVehicle* pVehicle );
	static void DisplayDraughtOffsetForLowLod( CVehicle* pVehicle );
#endif

	static SearchLightInfo& GetSearchLightInfo() { return ms_SearchLightInfo; }

	static void RejuvenateMatrixIfNecessary(Matrix34& matrix);

protected:
	virtual void UpdatePhysicsFromEntity(bool bWarp=false);
#if __WIN32PC
	virtual void ProcessForceFeedback();
#endif // __WIN32PC



	///// Member data /////
public:
	CBoatHandling m_BoatHandling;
	CAnchorHelper m_AnchorHelper;

	////// Statics /////
public:
#if USE_SIXAXIS_GESTURES
	static bank_float MOTION_CONTROL_PITCH_MIN;
	static bank_float MOTION_CONTROL_PITCH_MAX;
	static bank_float MOTION_CONTROL_ROLL_MIN;
	static bank_float MOTION_CONTROL_ROLL_MAX;
#endif

	static float ms_InWaterFrictionMultiplier;		// Friction multiplier applied to ground contacts when the boat is in the water
	static float ms_DriveForceOutOfWaterFrictionMultiplier;
	static float ms_OutOfWaterFrictionMultiplier;	// Friction multiplier applied to ground contacts when the boat isn't touching water
	static float ms_OutOfWaterTime;					// How long it takes for a boat to be considered out of the water for friction purposes
	static u32 ms_OutOfWaterShockingEventTime;		// How long it takes for a boat to be considered out of the water for shocking event purposes.

	static u32 ms_nMaxPropOutOfWaterDuration;

protected:
	static SearchLightInfo ms_SearchLightInfo;

};


#endif//_BOAT_H_
