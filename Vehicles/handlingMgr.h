// Title	:	HandlingMgr.h
// Author	:	Bill Henderson
// Started	:	10/12/1999
//
//
//	01/08/2001	-	Andrzej:	- HF_HAS_NO_ROOF flag added;
//
//
//
//
#ifndef HANDLING_DATA_MGR
#define HANDLING_DATA_MGR

//#include "vehicles/AnchorHelper.h"
#include "vehicles/VehicleDefines.h"
#include "fwtl/pool.h"
#include "modelinfo/VehicleModelInfoColors.h"

#if !__SPU
#include "system/fileMgr.h"
#endif
#include "vector/vector3.h"
#include "fwtl/Pool.h"

class CAIHandlingInfo;

#define VERIFY_OLD_HANDLING_DAT 0
#define CONVERT_HANDLING_DAT_TO_META 0

#define INIT_FLOAT (0.0f)

typedef enum
{
	WS_ROLLING,
	WS_SPINNING,
	WS_SKIDDING,
	WS_LOCKED
} tWheelState;


enum VehModelFlags
{
	MF_IS_VAN						= 0x00000001,
	MF_IS_BUS						= 0x00000002,
	MF_IS_LOW						= 0x00000004,
	MF_IS_BIG						= 0x00000008,

	MF_ABS_STD						= 0x00000010,
	MF_ABS_OPTION					= 0x00000020,
	MF_ABS_ALT_STD					= 0x00000040,
	MF_ABS_ALT_OPTION				= 0x00000080,
	
	MF_NO_DOORS						= 0x00000100,
	MF_TANDEM_SEATING				= 0x00000200,
	MF_SIT_IN_BOAT					= 0x00000400,
	MF_HAS_TRACKS					= 0x00000800,
	
	MF_NO_EXHAUST					= 0x00001000,
	MF_DOUBLE_EXHAUST				= 0x00002000,
	MF_NO_1STPERSON_LOOKBEHIND		= 0x00004000,
	MF_CAN_ENTER_IF_NO_DOOR			= 0x00008000,
	
	MF_AXLE_F_TORSION				= 0x00010000,
	MF_AXLE_F_SOLID					= 0x00020000,
	MF_AXLE_F_MCPHERSON				= 0x00040000,
	MF_ATTACH_PED_TO_BODYSHELL		= 0x00080000,
	
	MF_AXLE_R_TORSION				= 0x00100000,
	MF_AXLE_R_SOLID					= 0x00200000,
	MF_AXLE_R_MCPHERSON				= 0x00400000,
	MF_DONT_FORCE_GRND_CLEARANCE	= 0x00800000,
	
	MF_DONT_RENDER_STEER			= 0x01000000,
	MF_NO_WHEEL_BURST				= 0x02000000,
	MF_INDESTRUCTIBLE				= 0x04000000,
	MF_DOUBLE_FRONT_WHEELS			= 0x08000000,
	
	MF_IS_RC						= 0x10000000,	// Signal remote controlled cars
	MF_DOUBLE_REAR_WHEELS			= 0x20000000,
	MF_NO_WHEEL_BREAK				= 0x40000000,
	MF_EXTRA_CAMBER					= 0x80000000	// The vehicle needs extra wheel camber to stop the wheels clipping the arches - this is only used for the lowrider cars at the moment
};

enum VehHandlingFlags
{
	HF_SMOOTHED_COMPRESSION			= 0x00000001,
	HF_REDUCED_MOD_MASS				= 0x00000002,
	HF_HAS_KERS						= 0x00000004,		// Has a KERS energy recovery system
	HF_HAS_RALLY_TYRES				= 0x00000008,

	HF_NO_HANDBRAKE					= 0x00000010,
	HF_STEER_REARWHEELS				= 0x00000020,
	HF_HANDBRAKE_REARWHEELSTEER		= 0x00000040,		// Steer the rear wheels when the handbrake is on
	HF_STEER_ALL_WHEELS				= 0x00000080,
	
	HF_FREEWHEEL_NO_GAS				= 0x00000100,
	HF_NO_REVERSE					= 0x00000200,
	HF_REDUCED_RIGHTING_FORCE		= 0x00000400,
	HF_STEER_NO_WHEELS				= 0x00000800,
	
	HF_CVT							= 0x00001000,
	HF_ALT_EXT_WHEEL_BOUNDS_BEH		= 0x00002000,		// Alternative extra wheel bound behavior. Offset extra wheel bounds forward so they act as bumpers and enable all collisions with them.
	HF_DONT_RAISE_BOUNDS_AT_SPEED	= 0x00004000,		// some vehicles bounds dont respond well to be raised up too far, so this turns off the extra bound raising at speed.
	HF_EXT_WHEEL_BOUNDS_COL			= 0x00008000,		// Extra wheel bounds collide with other wheels.
	
	HF_LESS_SNOW_SINK				= 0x00010000,
	HF_TYRES_CAN_CLIP				= 0x00020000,		// Tyres can clip into the ground when bottoming out.
	HF_REDUCED_DRIVE_OVER_DAMAGE	= 0x00040000,		// Don't explode vehicles when driving over them
	HF_ALT_EXT_WHEEL_BOUNDS_SHRINK	= 0x00080000,
	
	HF_OFFROAD_ABILITIES			= 0x00100000,	// Extra gravity
	HF_OFFROAD_ABILITIES_X2			= 0x00200000,	// Even more gravity
	HF_TYRES_RAISE_SIDE_IMPACT_THRESHOLD					= 0x00400000, // this vehicle may be used for driving over vehicles to raise the side impact threshold so we dont miss contacts when driving over vehicles.
	HF_OFFROAD_INCREASED_GRAVITY_NO_FOLIAGE_DRAG	= 0x00800000,
	
	HF_ENABLE_LEAN					= 0x01000000,
	HF_FORCE_NO_TC_OR_SC			= 0x02000000,
	HF_HEAVYARMOUR					= 0x04000000, // Vehicle is resistant to explosions
	HF_ARMOURED						= 0x08000000,	// Vehicle is bullet proof
	
	HF_SELF_RIGHTING_IN_WATER		= 0x10000000,
	HF_IMPROVED_RIGHTING_FORCE		= 0x20000000, // Adds extra force when trying to right the car when upside down.

	HF_LOW_SPEED_WHEELIES			= 0x40000000,
	
	HF_LAST_AVAILABLE_FLAG			= 0x80000000
};

enum VehDoorDamageFlags
{
	DF_DRIVER_SIDE_FRONT_DOOR			= 0x00000001,
	DF_DRIVER_SIDE_REAR_DOOR			= 0x00000002,
	DF_DRIVER_PASSENGER_SIDE_FRONT_DOOR	= 0x00000004,
	DF_DRIVER_PASSENGER_SIDE_REAR_DOOR	= 0x00000008,

	DF_BONNET							= 0x00000010,
	DF_BOOT								= 0x00000020
};

enum CarAdvancedFlags
{
    CF_DIFF_FRONT               = 0x00000001,
    CF_DIFF_REAR                = 0x00000002,
    CF_DIFF_CENTRE              = 0x00000004,

    // IF WE WANT TORSEN DIFFS THEY WILL NEED TO KNOW RESISTANCE AT THE WHEEL AS APPLYING THE BRAKES SHOULD APPLY FORCE BACK TO THE WHEEL ON THE GROUND
    CF_DIFF_LIMITED_FRONT       = 0x00000008,
    CF_DIFF_LIMITED_REAR        = 0x00000010,
    CF_DIFF_LIMITED_CENTRE      = 0x00000020,

    CF_DIFF_LOCKING_FRONT       = 0x00000040,
    CF_DIFF_LOCKING_REAR        = 0x00000080,
    CF_DIFF_LOCKING_CENTRE      = 0x00000100,

    CF_GEARBOX_FULL_AUTO        = 0x00000200,
    CF_GEARBOX_MANUAL           = 0x00000400,
    CF_GEARBOX_DIRECT_SHIFT     = 0x00000800,
    CF_GEARBOX_ELECTRIC         = 0x00001000,

    CF_ASSIST_TRACTION_CONTROL  = 0x00002000, // JUST REDUCE THROTTLE
    CF_ASSIST_STABILITY_CONTROL = 0x00004000, // APPLY BRAKES TO INDIVIDUAL WHEELS

	CF_ALLOW_REDUCED_SUSPENSION_FORCE = 0x00008000, // Reduce suspension force can be used for "stancing" cars

    CF_HARD_REV_LIMIT           = 0x00010000,
    CF_HOLD_GEAR_WITH_WHEELSPIN = 0x00020000,

    CF_INCREASE_SUSPENSION_FORCE_WITH_SPEED = 0x00040000,
    CF_BLOCK_INCREASED_ROT_VELOCITY_WITH_DRIVE_FORCE = 0x00080000,

	CF_REDUCED_SELF_RIGHTING_SPEED	= 0x00100000,
	CF_CLOSE_RATIO_GEARBOX			= 0x00200000,

	CF_FORCE_SMOOTH_RPM				= 0x00400000,

	CF_ALLOW_TURN_ON_SPOT			= 0x00800000,
	CF_CAN_WHEELIE					= 0x01000000,

	CF_ENABLE_WHEEL_BLOCKER_SIDE_IMPACTS = 0x02000000,

    CF_FIX_OLD_BUGS = 0x04000000,
    CF_USE_DOWNFORCE_BIAS = 0x08000000,
    CF_REDUCE_BODY_ROLL_WITH_SUSPENSION_MODS = 0x10000000,
	CF_ALLOWS_EXTENDED_MODS =				   0x20000000,

};

enum SpecialFlightModeFlags
{
	SF_WORKS_UPSIDE_DOWN = 0x00000001,
	SF_DONT_RETRACT_WHEELS = 0x00000002,
	SF_REDUCE_DRAG_WITH_SPEED = 0x00000004,
	SF_MAINTAIN_HOVER_HEIGHT = 0x00000008,

	SF_STEER_TOWARDS_VELOCITY = 0x00000010,
	SF_FORCE_MIN_THROTTLE_WHEN_TURNING = 0x00000020,

	SF_LIMIT_FORCE_DELTA = 0x00000040,

	SF_FORCE_SPECIAL_FLIGHT_WHEN_DRIVEN = 0x00000080,
};


enum eHandlingType
{
	HANDLING_TYPE_INVALID = -1,
	HANDLING_TYPE_BIKE = 0,
	HANDLING_TYPE_FLYING,
	HANDLING_TYPE_VERTICAL_FLYING,
	HANDLING_TYPE_BOAT,
	HANDLING_TYPE_SEAPLANE,
	HANDLING_TYPE_SUBMARINE,
	HANDLING_TYPE_TRAIN,
	HANDLING_TYPE_TRAILER,
	HANDLING_TYPE_CAR,
	HANDLING_TYPE_WEAPON,
	HANDLING_TYPE_SPECIAL_FLIGHT,
	HANDLING_TYPE_MAX_TYPES,
};

#define HANDLING_UNUSED_FLOAT_VAL (-999.99f)
#define HANDLING_NAME_LENGTH	(16)


#define MAX_HANDLING_TYPES_PER_VEHICLE 4
#define MAX_NUM_BOARDING_POINTS	8

// down force bias data
class CAdvancedData
{
public:
    CAdvancedData()  {}

	s32 m_Slot;
    s32 m_Index; // the mod index that the value is applied to
    float m_Value; // the value that is applied to the mod with the above mod index

    PAR_PARSABLE
};


// Individual boarding-point
class CBoardingPoint
{
public:
	float m_fPosition[3];
	float m_fHeading;

	inline Vector3 GetPosition() const { return Vector3(m_fPosition[0], m_fPosition[1], m_fPosition[2]); }
	inline float GetHeading() const { return m_fHeading; }
};
// Boarding-points
class CBoardingPoints
{
	friend class CHandlingDataMgr;
	friend class CBoardingPointEditor;

public:

	CBoardingPoints()
	{
		m_iNumBoardingPoints = 0;
#if !__FINAL
		m_fNavMaxHeightChange = 0.35f;
		m_fNavMinZDist = 0.5f;
		m_fNavRes = 1.0f;
		m_pAuthoredMeshName = NULL;
#endif
	}
	~CBoardingPoints()
	{
#if !__FINAL
		if(m_pAuthoredMeshName)
			StringFree(m_pAuthoredMeshName);
#endif
	}
	inline int GetNumBoardingPoints() const { return m_iNumBoardingPoints; }
	inline const CBoardingPoint & GetBoardingPoint(const int i) const { return m_BoardingPoints[i]; }
	const CBoardingPoint * GetClosestBoardingPoint(const Vector3 & vInputPos);

#if !__FINAL
	inline float GetNavMaxHeightChange() const { return m_fNavMaxHeightChange; }
	inline float GetNavMinZDist() const { return m_fNavMinZDist; }
	inline float GetNavRes() const { return m_fNavRes; }
	inline const char * GetAuthoredMeshName() const { return m_pAuthoredMeshName; }
#endif

protected:

	int m_iNumBoardingPoints;
	CBoardingPoint m_BoardingPoints[MAX_NUM_BOARDING_POINTS];

#if !__FINAL
	// used by navmesh creation to define the maximum height change that may occur under a navmesh triangle.
	// a high value causes the mesh to 'flow' all over the boat, disregarding bumps & ridges in the collision.
	float m_fNavMaxHeightChange;
	// used by navmesh creation to define the minimum height difference between consecutive Z samplings at
	// the same (X,Y) location.
	float m_fNavMinZDist;
	// used by navmesh creation to define resolution
	float m_fNavRes;
	// name of authored mesh file
	char * m_pAuthoredMeshName;
#endif

};

class CHandlingObject
{
public:
	CHandlingObject() {}
	virtual ~CHandlingObject() {}

	// PURPOSE: Pool declaration
	FW_REGISTER_CLASS_POOL(CHandlingObject);

	PAR_PARSABLE;
};

class CBaseSubHandlingData : public CHandlingObject
{
public:
	CBaseSubHandlingData() {}
	virtual ~CBaseSubHandlingData(){}
#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
	virtual void Load(char* UNUSED_PARAM(pLine)) {}
#endif
	virtual eHandlingType GetHandlingType() { return HANDLING_TYPE_INVALID; }
	virtual void ConvertToGameUnits() {}

	PAR_PARSABLE
};

// Bike specific handling data
class CBikeHandlingData : public CBaseSubHandlingData
{
public:
	CBikeHandlingData();

	bool IsUsed() {return m_fLeanFwdCOMMult > HANDLING_UNUSED_FLOAT_VAL;}
#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
	void Load(char* pLine);
#endif
	virtual void ConvertToGameUnits();
	virtual eHandlingType GetHandlingType(){return HANDLING_TYPE_BIKE;}

public:
	float m_fLeanFwdCOMMult;
	float m_fLeanFwdForceMult;
	float m_fLeanBakCOMMult;
	float m_fLeanBakForceMult;
	float m_fMaxBankAngle;
	float m_fFullAnimAngle;
	float m_fFullAnimAngleInv;

	float m_fDesLeanReturnFrac;
	float m_fStickLeanMult;
	float m_fBrakingStabilityMult;

	float m_fInAirSteerMult;
	float m_fWheelieBalancePoint;
	float m_fStoppieBalancePoint;
	float m_fWheelieSteerMult;
	float m_fRearBalanceMult;
	float m_fFrontBalanceMult;

	float m_fBikeGroundSideFrictionMult;
	float m_fBikeWheelGroundSideFrictionMult;
	float m_fBikeOnStandLeanAngle;
	float m_fBikeOnStandSteerAngle;
	float m_fJumpForce;

	PAR_PARSABLE
};




// flying vehicle handling
class CFlyingHandlingData : public CBaseSubHandlingData
{
public:
	CFlyingHandlingData(): m_handlingType(HANDLING_TYPE_FLYING) {}
	CFlyingHandlingData(eHandlingType flyingType);

	bool IsUsed() {return m_fThrust > HANDLING_UNUSED_FLOAT_VAL;}
#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
	virtual void Load(char* pLine);
#endif
	virtual eHandlingType GetHandlingType(){return m_handlingType ;}

	virtual void ConvertToGameUnits();

public:
	// Thrust
	float m_fThrust;
	float m_fThrustFallOff;
	float m_fThrustVectoring;
	float m_fInitialThrust;
	float m_fInitialThrustFallOff;

	// Yaw
	float m_fYawMult;
	float m_fYawStabilise;	//fRudderMult;
	float m_fSideSlipMult;
	float m_fInitialYawMult;
	// Roll
	float m_fRollMult;
	float m_fRollStabilise;
	float m_fInitialRollMult;
	// Pitch
	float m_fPitchMult;
	float m_fPitchStabilise;	//fRCTailMult;
	float m_fInitialPitchMult;
	// Lift
	float m_fFormLiftMult;
	float m_fAttackLiftMult;
	float m_fAttackDiveMult;
	// special mults
	float m_fGearDownDragV;
	float m_fGearDownLiftMult;
	float m_fWindMult;
	// Damping
	float m_fMoveRes;
	Vec3V m_vecTurnRes;
	// SpeedBased TurnDamping
	Vec3V m_vecSpeedRes;
    // Landing gear properties
    float m_fGearDoorFrontOpen;//how much to open the front landing gear
	float m_fGearDoorRearOpen;//how much to open the rear landing gear
	float m_fGearDoorRearOpen2;//how much to open the rear landing gear
	float m_fGearDoorRearMOpen;//how much to open the rear landing gear
	// Damage effect
	float m_fTurublenceMagnitudeMax;
	float m_fTurublenceForceMulti;
	float m_fTurublenceRollTorqueMulti;
	float m_fTurublencePitchTorqueMulti;
	float m_fBodyDamageControlEffectMult;
	// Difficulty
	float m_fInputSensitivityForDifficulty;
	// Driving turning boost (extra yaw)
	float m_fOnGroundYawBoostSpeedPeak;
	float m_fOnGroundYawBoostSpeedCap;
	float m_fEngineOffGlideMulti;
	float m_fAfterburnerEffectRadius;
	float m_fAfterburnerEffectDistance;
	float m_fAfterburnerEffectForceMulti;
	float m_fSubmergeLevelToPullHeliUnderwater;

	float m_fExtraLiftWithRoll;
private:
	eHandlingType m_handlingType;

public:
	PAR_PARSABLE
};


// Boat specific handling data
class CBoatHandlingData : public CBaseSubHandlingData
{
public:
	CBoatHandlingData();

	bool IsUsed() {return m_fBoxFrontMult > HANDLING_UNUSED_FLOAT_VAL;}
#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
	virtual void Load(char* pLine);
#endif
	virtual eHandlingType GetHandlingType(){return HANDLING_TYPE_BOAT;}

public:
	float m_fBoxFrontMult;	// % of bbox fwd to use for water samples
	float m_fBoxRearMult;	// % of bbox rear to use for water samples
	float m_fBoxSideMult;	// % of bbox side to use for water samples
	float m_fSampleTop;		// dist above bbox centre to start water samples
	float m_fSampleBottom;
	float m_fSampleBottomTestCorrection; // Amount to raise the bottom end of the shape test to find the hull as a fraction of bounding box height.

	float m_fAquaplaneForce;
	float m_fAquaplanePushWaterMult;
	float m_fAquaplanePushWaterCap;
	float m_fAquaplanePushWaterApply;

	float m_fRudderForce;
	float m_fRudderOffsetSubmerge;
	float m_fRudderOffsetForce;
	float m_fRudderOffsetForceZMult;
	float m_fWaveAudioMult;

	Vec3V m_vecMoveResistance;
	Vec3V m_vecTurnResistance;

	float m_fLook_L_R_CamHeight;

	float m_fDragCoefficient;

	float m_fKeelSphereSize;

	float m_fPropRadius;

	float m_fLowLodAngOffset;
	float m_fLowLodDraughtOffset;

	float m_fImpellerOffset;

	float m_fImpellerForceMult;

	float m_fDinghySphereBuoyConst;

	float m_fProwRaiseMult;

	float m_fDeepWaterSampleBuoyancyMult; // Amount to scale a sample's buoyancy force by when it is deeply submerged.

	float m_fTransmissionMultiplier; // The drive force gets multiplied by this, so it makes things go fast

	float m_fTractionMultiplier;

	PAR_PARSABLE
};

// Seaplane specific handling data.
class CSeaPlaneHandlingData : public CBaseSubHandlingData
{
public:
	CSeaPlaneHandlingData();

	virtual eHandlingType GetHandlingType(){return HANDLING_TYPE_SEAPLANE;}

public:
	u32 m_fLeftPontoonComponentId;
	u32 m_fRightPontoonComponentId;
	float m_fPontoonBuoyConst;
	float m_fPontoonSampleSizeFront;
	float m_fPontoonSampleSizeMiddle;
	float m_fPontoonSampleSizeRear;
	float m_fPontoonLengthFractionForSamples;
	float m_fPontoonDragCoefficient;
	float m_fPontoonVerticalDampingCoefficientUp;
	float m_fPontoonVerticalDampingCoefficientDown;
	float m_fKeelSphereSize;

	PAR_PARSABLE
};

class CVehicleWeaponHandlingData : public CBaseSubHandlingData
{
public:
	CVehicleWeaponHandlingData();
#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
	virtual void Load(char* pLine);
#endif
	virtual eHandlingType GetHandlingType(){return HANDLING_TYPE_WEAPON;}

	struct sTurretPitchLimits
	{
		float m_fForwardAngleMin;
		float m_fForwardAngleMax;
		float m_fBackwardAngleMin;
		float m_fBackwardAngleMid;
		float m_fBackwardAngleMax;
		float m_fBackwardForcedPitch;

		PAR_PARSABLE;
	};

	inline u32 GetWeaponHash(int iWeaponIndex) const;
	inline s32 GetWeaponSeat(int iWeaponIndex) const;
	inline eVehicleModType GetWeaponModType(int iWeaponIndex) const;
	inline float GetTurretSpeed(int iTurretIndex) const;
	inline float GetTurretPitchMin(int iTurretIndex) const;
	inline float GetTurretPitchMax(int iTurretIndex) const;
	inline float GetTurretCamPitchMin(int iTurretIndex) const;
	inline float GetTurretCamPitchMax(int iTurretIndex) const;
	inline float GetBulletVelocityForGravity(int iTurretIndex) const;
	inline float GetTurretPitchForwardMin(int iTurretIndex) const;
	inline float GetUvAnimationMult() const { return m_fUvAnimationMult;}
	inline float GetMiscGadgetVar() const {return m_fMiscGadgetVar; }
    inline float GetWheelImpactOffset() const {return m_fWheelImpactOffset; }
	inline float GetPitchForwardAngleMin(int iTurretIndex) const { if (Verifyf(iTurretIndex < MAX_NUM_VEHICLE_TURRETS,"Invalid turret index")) { return m_TurretPitchLimitData[iTurretIndex].m_fForwardAngleMin; } return 0.0f; }
	inline float GetPitchForwardAngleMax(int iTurretIndex) const { if (Verifyf(iTurretIndex < MAX_NUM_VEHICLE_TURRETS,"Invalid turret index")) { return m_TurretPitchLimitData[iTurretIndex].m_fForwardAngleMax; } return 0.0f; }
	inline float GetPitchBackwardAngleMin(int iTurretIndex) const { if (Verifyf(iTurretIndex < MAX_NUM_VEHICLE_TURRETS,"Invalid turret index")) { return m_TurretPitchLimitData[iTurretIndex].m_fBackwardAngleMin; } return 0.0f; }
	inline float GetPitchBackwardAngleMid(int iTurretIndex) const { if (Verifyf(iTurretIndex < MAX_NUM_VEHICLE_TURRETS,"Invalid turret index")) { return m_TurretPitchLimitData[iTurretIndex].m_fBackwardAngleMid; } return 0.0f; }
	inline float GetPitchBackwardAngleMax(int iTurretIndex) const { if (Verifyf(iTurretIndex < MAX_NUM_VEHICLE_TURRETS,"Invalid turret index")) { return m_TurretPitchLimitData[iTurretIndex].m_fBackwardAngleMax; } return 0.0f; }
	inline float GetPitchBackwardForcedPitch(int iTurretIndex) const { if (Verifyf(iTurretIndex < MAX_NUM_VEHICLE_TURRETS,"Invalid turret index")) { return m_TurretPitchLimitData[iTurretIndex].m_fBackwardForcedPitch; } return 0.0f; }

#if !(VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META)
protected:
#endif
	atHashString m_uWeaponHash[MAX_NUM_VEHICLE_WEAPONS];
	s32			m_WeaponSeats[MAX_NUM_VEHICLE_WEAPONS];
	eVehicleModType m_WeaponVehicleModType[MAX_NUM_VEHICLE_WEAPONS];
	float		m_fTurretSpeed[MAX_NUM_VEHICLE_TURRETS];
	float		m_fTurretPitchMin[MAX_NUM_VEHICLE_TURRETS];
	float		m_fTurretPitchMax[MAX_NUM_VEHICLE_TURRETS];
	float		m_fTurretCamPitchMin[MAX_NUM_VEHICLE_TURRETS];
	float		m_fTurretCamPitchMax[MAX_NUM_VEHICLE_TURRETS];
	// Variable used if gravity takes effect on weapon bullets. If not set then gravity will not be used.
	float		m_fBulletVelocityForGravity[MAX_NUM_VEHICLE_TURRETS];
	// Variable used to limit the pitch of the turret on it's forward vector and to lerp to m_fTurretPitchMin. 0.0f not used.
	float		m_fTurretPitchForwardMin[MAX_NUM_VEHICLE_TURRETS];
	sTurretPitchLimits	m_TurretPitchLimitData[MAX_NUM_VEHICLE_TURRETS];

	// some vehicle track specific stuff
	// if this grows too big maybe make a class and only dynamically create when needed
	float		m_fUvAnimationMult;
	// Variable for strange vehicle specific stuff (gadgets can use it for something, and there is only one gadget per vehicle)
	float		m_fMiscGadgetVar;
    // Variable to offset where wheel impacts are for the wheels, so undersized wheels can be made bigger
    float       m_fWheelImpactOffset;

public:
	PAR_PARSABLE
};

class CSubmarineHandlingData: public CBaseSubHandlingData
{
public:
	CSubmarineHandlingData();
#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
	virtual void Load(char* pLine);
#endif
	virtual eHandlingType GetHandlingType(){return HANDLING_TYPE_SUBMARINE;}

	const Vector3& GetTurnRes() const { return m_vTurnRes; }
	const float& GetMoveResXY() const { return m_fMoveResXY; }
	const float& GetMoveResZ() const { return m_fMoveResZ; }
	const float& GetPitchMult() const { return m_fPitchMult; }
	const float& GetPitchAngle() const { return m_fPitchAngle; }
	const float& GetYawMult() const { return m_fYawMult; }
	const float& GetDiveSpeed() const { return m_fDiveSpeed; }
    const float& GetRollMult() const { return m_fRollMult; }
    const float& GetRollStab() const { return m_fRollStab; }

protected:
	Vector3 m_vTurnRes;

	float m_fMoveResXY;
	float m_fMoveResZ;

	float m_fPitchMult;
	float m_fPitchAngle;
	float m_fYawMult;
	float m_fDiveSpeed;

    float m_fRollMult; // This is only used for the new sub handling
    float m_fRollStab; // This is only used for the new sub handling

public:
	PAR_PARSABLE
};

class CTrainHandlingData: public CBaseSubHandlingData
{
public:
	CTrainHandlingData();
#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
	virtual void Load(char* pLine);
#endif
	virtual eHandlingType GetHandlingType(){return HANDLING_TYPE_TRAIN;}
};

// Trailer specific handling data
class CTrailerHandlingData: public CBaseSubHandlingData
{
public:
	CTrailerHandlingData();
#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
	virtual void Load(char* pLine);
#endif
	virtual eHandlingType GetHandlingType(){return HANDLING_TYPE_TRAILER;}

public:
	// These angular limits are loaded in degrees
	float m_fAttachLimitPitch;
	float m_fAttachLimitRoll;
	float m_fAttachLimitYaw;
	float m_fUprightSpringConstant;
	float m_fUprightDampingConstant;
	float m_fAttachedMaxDistance;
	float m_fAttachedMaxPenetration;
	float m_fAttachRaiseZ;
	float m_fPosConstraintMassRatio;

	PAR_PARSABLE
};

// Car specific handling data
class CCarHandlingData: public CBaseSubHandlingData
{
public:
	CCarHandlingData();
#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
	void Load(char* pLine);
#endif
	virtual eHandlingType GetHandlingType(){return HANDLING_TYPE_CAR;}
    virtual void ConvertToGameUnits();

public:
	float m_fBackEndPopUpCarImpulseMult;
	float m_fBackEndPopUpBuildingImpulseMult;
	float m_fBackEndPopUpMaxDeltaSpeed;
    float m_fToeFront;
    float m_fToeRear;
    float m_fCamberFront;
    float m_fCamberRear;
    float m_fCastor;
    float m_fEngineResistance;
    float m_fMaxDriveBiasTransfer;
	float m_fJumpForceScale;
	float m_fIncreasedRammingForceScale;

    atFinalHashString m_strAdvancedFlags;
    u32 aFlags;	// advanced flags

    atArray<CAdvancedData> m_AdvancedData;

	PAR_PARSABLE
};

// Special vehicle specific handling data
class CSpecialFlightHandlingData: public CBaseSubHandlingData
{
public:
	CSpecialFlightHandlingData();
	virtual void ConvertToGameUnits();
	virtual eHandlingType GetHandlingType(){return HANDLING_TYPE_SPECIAL_FLIGHT;}

public:
	Vec3V m_vecAngularDamping;
	Vec3V m_vecAngularDampingMin;
	Vec3V m_vecLinearDamping;
	Vec3V m_vecLinearDampingMin;

	float m_fLiftCoefficient;
	float m_fCriticalLiftAngle;
	float m_fInitialLiftAngle;
	float m_fMaxLiftAngle;

	float m_fDragCoefficient;
	float m_fBrakingDrag;
	float m_fMaxLiftVelocity;
	float m_fMinLiftVelocity;

	float m_fRollTorqueScale;
	float m_fMaxTorqueVelocity;
	float m_fMinTorqueVelocity;

	float m_fYawTorqueScale;
	float m_fSelfLevelingPitchTorqueScale;
	float m_fSelfLevelingRollTorqueScale;

	float m_fMaxPitchTorque;
	float m_fMaxSteeringRollTorque;
	float m_fPitchTorqueScale;

	float m_fSteeringTorqueScale;
	float m_fMaxThrust;

	float m_fTransitionDuration;
	float m_fHoverVelocityScale;

	float m_fStabilityAssist;
	float m_fMinSpeedForThrustFalloff;
	float m_fBrakingThrustScale;

	int m_mode;

	atFinalHashString m_strFlags;
	u32 m_flags;	

	PAR_PARSABLE
};

inline u32 CVehicleWeaponHandlingData::GetWeaponHash(int iWeaponIndex) const { 
	if(Verifyf(iWeaponIndex < MAX_NUM_VEHICLE_WEAPONS, "Invalid weapon index"))
	{
		return m_uWeaponHash[iWeaponIndex].GetHash();
	}
	return 0;
} 

inline s32 CVehicleWeaponHandlingData::GetWeaponSeat(int iWeaponIndex) const { 
	if(Verifyf(iWeaponIndex < MAX_NUM_VEHICLE_WEAPONS, "Invalid weapon index"))
	{
		return m_WeaponSeats[iWeaponIndex];
	}
	return -1;
} 

inline eVehicleModType CVehicleWeaponHandlingData::GetWeaponModType(int iWeaponIndex) const { 
	return m_WeaponVehicleModType[iWeaponIndex];
} 

inline float CVehicleWeaponHandlingData::GetTurretSpeed(int iTurretIndex) const {
	if(Verifyf(iTurretIndex < MAX_NUM_VEHICLE_TURRETS,"Invalid turret index"))
	{
		return m_fTurretSpeed[iTurretIndex];
	}
	return 0.0f;
}

inline float CVehicleWeaponHandlingData::GetTurretPitchMin(int iTurretIndex) const {
	if(Verifyf(iTurretIndex < MAX_NUM_VEHICLE_TURRETS,"Invalid turret index"))
	{
		return m_fTurretPitchMin[iTurretIndex];
	}
	return 0.0f;
}

inline float CVehicleWeaponHandlingData::GetTurretPitchMax(int iTurretIndex) const {
	if(Verifyf(iTurretIndex < MAX_NUM_VEHICLE_TURRETS,"Invalid turret index"))
	{
		return m_fTurretPitchMax[iTurretIndex];
	}
	return 0.0f;
}

inline float CVehicleWeaponHandlingData::GetTurretCamPitchMin(int iTurretIndex) const {
	if(Verifyf(iTurretIndex < MAX_NUM_VEHICLE_TURRETS,"Invalid turret index"))
	{
		return m_fTurretCamPitchMin[iTurretIndex];
	}
	return 0.0f;
}

inline float CVehicleWeaponHandlingData::GetTurretCamPitchMax(int iTurretIndex) const {
	if(Verifyf(iTurretIndex < MAX_NUM_VEHICLE_TURRETS,"Invalid turret index"))
	{
		return m_fTurretCamPitchMax[iTurretIndex];
	}
	return 0.0f;
}


inline float CVehicleWeaponHandlingData::GetBulletVelocityForGravity(int iTurretIndex) const {
	if(Verifyf(iTurretIndex < MAX_NUM_VEHICLE_TURRETS,"Invalid turret index"))
	{
		return m_fBulletVelocityForGravity[iTurretIndex];
	}
	return 0.0f;
}

inline float CVehicleWeaponHandlingData::GetTurretPitchForwardMin(int iTurretIndex) const {
	if(Verifyf(iTurretIndex < MAX_NUM_VEHICLE_TURRETS,"Invalid turret index"))
	{
		return m_fTurretPitchForwardMin[iTurretIndex];
	}
	return 0.0f;
}

class CHandlingData : public CHandlingObject
{
	enum eDriveTypeFlags
	{
		DRIVE_FRONT_WHEELS = 0x01,
		DRIVE_MID_WHEELS = 0x02,
		DRIVE_REAR_WHEELS = 0x04
	};

public:
	CHandlingData();
	virtual ~CHandlingData();
	bool IsUsed() {return m_fMass > HANDLING_UNUSED_FLOAT_VAL;}
#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
	void Load(char* pLine);
#endif

public:
	atHashString m_handlingName;

	// BODY
	float m_fMass;
//	float fTurnMass;			// (used to be calculated from mass and dimensions)
	float m_fInitialDragCoeff;	// (used to be calculated from dimensions), This can be modified after creation, so check CVehicle m_fDragCoeff for actual dragCoeff
	float m_fDownforceModifier; 
	float m_fPopUpLightRotation;

	Vec3V m_vecCentreOfMassOffset;
    Vec3V m_vecInertiaMultiplier;

	float m_fPercentSubmerged;
	float m_fBuoyancyConstant;	// << calculated from percent submerged, gravity and mass

// TRANSMISSION DATA
	float m_fDriveBiasFront;
	float m_fDriveBiasRear;

//	u8 m_nDriveType;
	u8 m_nInitialDriveGears; // Check transmission for how many gears instances has, as it can be changed after creation
	float m_fDriveInertia;
	float m_fClutchChangeRateScaleUpShift;
	float m_fClutchChangeRateScaleDownShift;
	
    // Initial values for drive force and max vel, these can be modified per instance and are stored inside CTransmission
    float m_fInitialDriveForce;
	float m_fInitialDriveMaxVel;						// calculated max vel (flat vel + 20%), for gear spacing, MaxFlat*MAX_VEL_HEADROOM
	float m_fInitialDriveMaxFlatVel;					// designed max velocity on the flat

	float m_fBrakeForce;
	float m_fBrakeBias;
	float m_fBrakeBiasFront;
	float m_fBrakeBiasRear;
	float m_fHandBrakeForce;

	// STEERING
	float m_fSteeringLock;
	float m_fSteeringLockInv;

// WHEEL TRACTION DATA
	float m_fTractionCurveMax;
	float m_fTractionCurveMax_Inv;
	float m_fTractionCurveMin;
	float m_fTractionCurveMaxMinusMin_Inv;
	float m_fTractionCurveLateral;
	float m_fTractionCurveLateral_Inv;
//  float m_fTractionCurveLongitudinal;	// not used in integrator version

	float m_fTractionSpringDeltaMax;
	float m_fTractionSpringDeltaMax_Inv;
//	float m_fTractionSpringForceMult;	// not used in integrator version
//	float m_fTractionSpeedLateralMult;	// not used in integrator version

	float m_fLowSpeedTractionLossMult;

	float m_fCamberStiffnesss;

	float m_fTractionBiasFront;
	float m_fTractionBiasRear;

	float m_fTractionLossMult;		// Scale factor for how fast tyres lose grip


// SUSPENSION
	float m_fSuspensionForce;			//	<< calculated from SuspensionForceLevel, weight and gravity
	float m_fSuspensionCompDamp;		//	<< calculated fromSuspensionDampingLevel, weight and gravity
	float m_fSuspensionReboundDamp;	// high speed compression damping multiplier!

	float m_fSuspensionUpperLimit;
	float m_fSuspensionLowerLimit;
	float m_fSuspensionRaise;

	float m_fSuspensionBiasFront;
	float m_fSuspensionBiasRear;

	float m_fAntiRollBarForce;
	float m_fAntiRollBarBiasFront;
	float m_fAntiRollBarBiasRear;

    float m_fRollCentreHeightFront;
    float m_fRollCentreHeightRear;

// MISC
	float m_fCollisionDamageMult;
	float m_fWeaponDamageMult;
	float m_fDeformationDamageMult;
	float m_fEngineDamageMult;	

    float m_fPetrolTankVolume;
    float m_fOilVolume;
	float m_fPetrolConsumptionRate;

	float m_fSeatOffsetDistX;
	float m_fSeatOffsetDistY;
	float m_fSeatOffsetDistZ;
	u32 m_nMonetaryValue;

	float m_fRocketBoostCapacity;
	float m_fBoostMaxSpeed;

// FLAGS
	u32 mFlags;	// model option flags
	u32 hFlags;	// handling option flags
	u32 dFlags;	// door damage flags

// TODO - bring handling enums over to psc 
	atFinalHashString m_strModelFlags;
	atFinalHashString m_strHandlingFlags;
	atFinalHashString m_strDamageFlags;

	atHashString m_AIHandling;
	
	float m_fEstimatedMaxFlatVel;

	CBoardingPoints * m_pBoardingPoints;

private:
	const CAIHandlingInfo* m_pAIHandlingInfo;
public:
	const CAIHandlingInfo* GetAIHandlingInfo() const { return m_pAIHandlingInfo; }

	void OverrideAIHandlingInfo(const CAIHandlingInfo* pNewHandling) {m_pAIHandlingInfo = pNewHandling;}

#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
	void SetHandlingData(CBaseSubHandlingData* pData, bool bMetaData);
#endif

	void ClearHandlingData();

	void ConvertToGameUnits(bool bMetaData);

	inline bool DriveFrontWheels()	{return (m_fDriveBiasFront > 0.0f || ( GetCarHandlingData() && GetCarHandlingData()->m_fMaxDriveBiasTransfer > -1.0f ) );}
	inline bool DriveMidWheels()	{return (m_fDriveBiasRear  > 0.0f || ( GetCarHandlingData() && GetCarHandlingData()->m_fMaxDriveBiasTransfer > -1.0f ) );}
	inline bool DriveRearWheels()	{return (m_fDriveBiasRear  > 0.0f || ( GetCarHandlingData() && GetCarHandlingData()->m_fMaxDriveBiasTransfer > -1.0f ) );}

	__forceinline float GetDriveBias(float fFrontRearSelector)		{return Selectf(fFrontRearSelector, m_fDriveBiasFront, m_fDriveBiasRear);}
	__forceinline float GetSuspensionBias(float fFrontRearSelector)	{return Selectf(fFrontRearSelector, m_fSuspensionBiasFront, m_fSuspensionBiasRear);}
	//__forceinline float GetSpringRateBias(float fFrontRearSelector)	{return Selectf(fFrontRearSelector, 1.0f, 0.75f);}
	__forceinline float GetBrakeBias(float fFrontRearSelector)		{return Selectf(fFrontRearSelector, m_fBrakeBiasFront, m_fBrakeBiasRear);}
	__forceinline float GetTractionBias(float fFrontRearSelector)	{return Selectf(fFrontRearSelector, m_fTractionBiasFront, m_fTractionBiasRear);}
	__forceinline float GetAntiRollBarBias(float fFrontRearSelector){return Selectf(fFrontRearSelector, m_fAntiRollBarBiasFront, m_fAntiRollBarBiasRear);}
	__forceinline float GetRollCentreHeightBias(float fFrontRearSelector){return Selectf(fFrontRearSelector, m_fRollCentreHeightFront, m_fRollCentreHeightRear);}

#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
	CBikeHandlingData* GetBikeHandlingData(bool bMetaData = true) {return smart_cast<CBikeHandlingData*>(GetHandlingData(HANDLING_TYPE_BIKE, bMetaData));}

	CFlyingHandlingData* GetFlyingHandlingData(bool bMetaData = true) {return smart_cast<CFlyingHandlingData*>(GetHandlingData(HANDLING_TYPE_FLYING, bMetaData));}

	CFlyingHandlingData* GetVerticalFlyingHandlingData(bool bMetaData = true) {return smart_cast<CFlyingHandlingData*>(GetHandlingData(HANDLING_TYPE_VERTICAL_FLYING, bMetaData));}

	CBoatHandlingData* GetBoatHandlingData(bool bMetaData = true) {return smart_cast<CBoatHandlingData*>(GetHandlingData(HANDLING_TYPE_BOAT, bMetaData));}

	CSubmarineHandlingData* GetSubmarineHandlingData(bool bMetaData = true) { return smart_cast<CSubmarineHandlingData*>(GetHandlingData(HANDLING_TYPE_SUBMARINE, bMetaData)); }

	CTrainHandlingData* GetTrainHandlingData(bool bMetaData = true) {return smart_cast<CTrainHandlingData*>(GetHandlingData(HANDLING_TYPE_TRAIN, bMetaData));}

	CTrailerHandlingData* GetTrailerHandlingData(bool bMetaData = true) {return smart_cast<CTrailerHandlingData*>(GetHandlingData(HANDLING_TYPE_TRAILER, bMetaData));}

	CCarHandlingData* GetCarHandlingData(bool bMetaData = true) {return smart_cast<CCarHandlingData*>(GetHandlingData(HANDLING_TYPE_CAR, bMetaData));}

	CVehicleWeaponHandlingData* GetWeaponHandlingData(bool bMetaData = true) {return smart_cast<CVehicleWeaponHandlingData*>(GetHandlingData(HANDLING_TYPE_WEAPON, bMetaData));}

	CBaseSubHandlingData* m_OldSubHandlingData[MAX_HANDLING_TYPES_PER_VEHICLE];
	CBaseSubHandlingData* GetHandlingData(eHandlingType handlingType, bool bMeta = true);
#else
	CBikeHandlingData* GetBikeHandlingData() {return smart_cast<CBikeHandlingData*>(GetHandlingData(HANDLING_TYPE_BIKE));}

	CFlyingHandlingData* GetFlyingHandlingData() {return smart_cast<CFlyingHandlingData*>(GetHandlingData(HANDLING_TYPE_FLYING));}

	CFlyingHandlingData* GetVerticalFlyingHandlingData() {return smart_cast<CFlyingHandlingData*>(GetHandlingData(HANDLING_TYPE_VERTICAL_FLYING));}

	CBoatHandlingData* GetBoatHandlingData() {return smart_cast<CBoatHandlingData*>(GetHandlingData(HANDLING_TYPE_BOAT));}

	CSeaPlaneHandlingData* GetSeaPlaneHandlingData() {return smart_cast<CSeaPlaneHandlingData*>(GetHandlingData(HANDLING_TYPE_SEAPLANE));}

	CSubmarineHandlingData* GetSubmarineHandlingData() { return smart_cast<CSubmarineHandlingData*>(GetHandlingData(HANDLING_TYPE_SUBMARINE)); }

	CTrainHandlingData* GetTrainHandlingData() {return smart_cast<CTrainHandlingData*>(GetHandlingData(HANDLING_TYPE_TRAIN));}

	CTrailerHandlingData* GetTrailerHandlingData() {return smart_cast<CTrailerHandlingData*>(GetHandlingData(HANDLING_TYPE_TRAILER));}

	CCarHandlingData* GetCarHandlingData() {return smart_cast<CCarHandlingData*>(GetHandlingData(HANDLING_TYPE_CAR));}

	CVehicleWeaponHandlingData* GetWeaponHandlingData() {return smart_cast<CVehicleWeaponHandlingData*>(GetHandlingData(HANDLING_TYPE_WEAPON));}

	CSpecialFlightHandlingData* GetSpecialFlightHandlingData() {return smart_cast<CSpecialFlightHandlingData*>(GetHandlingData(HANDLING_TYPE_SPECIAL_FLIGHT));}

	CBaseSubHandlingData* GetHandlingData(eHandlingType handlingType);
#endif

	atArray<CBaseSubHandlingData*> m_SubHandlingData;

	float m_fWeaponDamageScaledToVehHealthMult;

	PAR_PARSABLE
};



// Extra Id's in a final build in case we want to add extra vehicles at a later date, but make sure we're not using them before that
#if __DEV
	#define HANDLING_EXTRA_IDS (0)
#else
	#define HANDLING_EXTRA_IDS (20)
#endif



class CHandlingDataMgr 
{
public:
	static void Init();
	static void Init(unsigned initMode);
	static void Reset();
	static void Shutdown(unsigned shutdownMode);
	static void LoadHandlingData(bool bMetaData = true);
#if __BANK
	static void ReloadHandlingData();
#endif //__BANK

	static void LoadHandlingMetaData(const char* pHandlingCfg);
	static void AppendHandlingMetaData(const char* pHandlingCfg);
#if CONVERT_HANDLING_DAT_TO_META
	static void SaveHandlingData();
#endif
	static void ConvertToGameUnits(bool bMetaData);

#if VERIFY_OLD_HANDLING_DAT
	static void VerifyHandlingData();
#endif

#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
	static void LoadOldHandlingData(const char* pHandlingCfg, bool bMetaData);
#endif

	static void LoadVehicleExtrasFiles();
	static void LoadVehicleExtrasFile(const char* szFileName);
#if !__SPU
	static void ReadVehiclesExtraBoardingPoints(const FileHandle fid);
#endif

	static CHandlingDataMgr* GetInstance();

	static int GetHandlingIndex(atHashString nameHash);
	static CHandlingData* GetHandlingData(int nHandlingIndex);
	static CHandlingData* GetHandlingDataByName(atHashString nameHash);
	
	static int RegisterHandlingData(const char* name,bool bMetaData){return RegisterHandlingData(atHashString(name),bMetaData);}

private:

	static int RegisterHandlingData(atHashString nameHash,bool bMetaData);
#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
	static int GetHandlingIndexOld(atHashString nameHash);
	static int RegisterBikeHandlingData(atHashString nameHash,bool bMetaData);
	static int RegisterFlyingHandlingData(atHashString nameHash,bool bMetaData);
	static int RegisterVerticalFlyingHandlingData(atHashString nameHash,bool bMetaData);
	static int RegisterBoatHandlingData(atHashString nameHash,bool bMetaData);
	static int RegisterWeaponHandlingData(atHashString nameHash,bool bMetaData);
	static int RegisterSubmarineHandlingData(atHashString nameHash,bool bMetaData);
	static int RegisterTrainHandlingData(atHashString nameHash,bool bMetaData);
	static int RegisterTrailerHandlingData(atHashString nameHash,bool bMetaData);
	static int RegisterCarHandlingData(atHashString nameHash,bool bMetaData);
#endif

	// PURPOSE: Static instance of metadata 
	static CHandlingDataMgr ms_Instance;

	// individual vehicle handling values
	atArray<CHandlingData*> m_HandlingData;

#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
	// OLD handling
public:
	static CHandlingDataMgr ms_Instance_DLC;
	static bool ms_bParsingDLC;
	static atArray<CHandlingData*> ms_aHandlingData;
#endif

	PAR_SIMPLE_PARSABLE
};


#endif //HANDLING_DATA_MGR

