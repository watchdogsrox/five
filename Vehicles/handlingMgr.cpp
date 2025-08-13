// Title	:	HandlingMgr.cpp
// Author	:	Bill Henderson
// Started	:	10/12/1999

// c headers
#include <stdio.h>	// for sscanf,etc
#include <string.h>	// for memset, strncpy, strncmp

// rage headers
// game headers
#include "core/game.h"
#include "debug/debug.h"
#include "modelInfo/vehicleModelInfo.h"
#include "scene/DataFileMgr.h"
#include "system/fileMgr.h"
#include "vehicles/handlingMgr.h"
#include "vehicles/transmission.h"
#include "vehicles/vehicle.h"
#include "vehicles/Metadata/AIHandlingInfo.h"
#include "vehicles/MetaData/HandlingInfo_parser.h"

VEHICLE_OPTIMISATIONS()
AI_OPTIMISATIONS()


// Vehicle handling parameters stored in this file
const char* HandlingFilename = "Handling.dat";

// Number of handling data preallocations
#define NUM_HANDLING_DATA_PREALLOCATIONS (GetHandlingDataPoolSize())

enum eLoadVehicleWeaponList
{
	LOAD_WEAPON_IDENTIFIER = 0,
	LOAD_WEAPON_HANDLING_NAME,
	LOAD_WEAPON_1,
	LOAD_WEAPON_1_SEAT,
	LOAD_WEAPON_2,
	LOAD_WEAPON_2_SEAT,
	LOAD_WEAPON_3,
	LOAD_WEAPON_3_SEAT,
	LOAD_WEAPON_TURRET_SPEED_1,
	LOAD_WEAPON_TURRET_SPEED_2,
	LOAD_WEAPON_TURRET_PITCH_MIN_1,
	LOAD_WEAPON_TURRET_PITCH_MIN_2,
	LOAD_WEAPON_TURRET_PITCH_MAX_1,
	LOAD_WEAPON_TURRET_PITCH_MAX_2,
	LOAD_WEAPON_TURRET_CAM_PITCH_MIN_1,
	LOAD_WEAPON_TURRET_CAM_PITCH_MIN_2,
	LOAD_WEAPON_TURRET_CAM_PITCH_MAX_1,
	LOAD_WEAPON_TURRET_CAM_PITCH_MAX_2,
	LOAD_WEAPON_UV_ANIM_MULT,
	LOAD_WEAPON_MISC_VAR_1,
	LOAD_WEAPON_WHEEL_IMPACT_OFFSET,
	LOAD_BULLET_VELOCITY_FOR_GRAVITY,
	LOAD_TURRECT_PITCH_FORWARD_MIN
};

enum eLoadBikeList
{
	LOAD_BIKE_IDENTIFIER = 0,
	LOAD_BIKE_HANDLING_NAME,

	LOAD_BIKE_LEAN_FWD_COM_MULT,
	LOAD_BIKE_LEAN_FWD_FORCE_MULT,
	LOAD_BIKE_LEAN_BACK_COM_MULT,
	LOAD_BIKE_LEAN_BACK_FORCE_MULT,
	LOAD_BIKE_LEAN_MAX_ANGLE,
	LOAD_BIKE_LEAN_MAX_ANIM_ANGLE,

	LOAD_BIKE_DES_LEAN_RETURN_FRAC,
	LOAD_BIKE_STICK_LEAN_MULT,
	LOAD_BIKE_BRAKE_STABILITY_MULT,

	LOAD_BIKE_IN_AIR_STEER_MULT,
	LOAD_BIKE_WHEELIE_BALANCE_POINT,
	LOAD_BIKE_STOPPIE_BALANCE_POINT,
	LOAD_BIKE_WHEELIE_STEER_MULT,
	LOAD_BIKE_REAR_BALANCE_MULT,
	LOAD_BIKE_FRONT_BALANCE_MULT,

	LOAD_BIKE_ON_SIDE_FRICTION_MULT,
	LOAD_BIKE_ON_SIDE_WHEEL_FRICTION_MULT,
	LOAD_BIKE_ON_STAND_LEAN_ANGLE,
	LOAD_BIKE_ON_STAND_STEER_ANGLE,

	LOAD_BIKE_JUMP_FORCE
};

enum eLoadFlyingList
{
	LOAD_FLYING_IDENTIFIER = 0,
	LOAD_FLYING_HANDLING_NAME,

	LOAD_FLYING_THRUST,
	LOAD_FLYING_THRUST_FALLOFF,
	LOAD_FLYING_THRUST_VECTOR,

	LOAD_FLYING_YAW_MULT,
	LOAD_FLYING_YAW_STAB,
	LOAD_FLYING_SIDESLIP_MULT,

	LOAD_FLYING_ROLL_MULT,
	LOAD_FLYING_ROLL_STAB,

	LOAD_FLYING_PITCH_MULT,
	LOAD_FLYING_PITCH_STAB,

	LOAD_FLYING_FORM_LIFT,
	LOAD_FLYING_ATTACK_LIFT,
	LOAD_FLYING_ATTACK_DIVE,

	LOAD_FLYING_GEAR_UP_RES_MULT,
	LOAD_FLYING_GEAR_DOWN_LIFT_MULT,
	LOAD_FLYING_WIND_MULT,

	LOAD_FLYING_MOVE_RES,

	LOAD_FLYING_TURN_RES_X,
	LOAD_FLYING_TURN_RES_Y,
	LOAD_FLYING_TURN_RES_Z,

	LOAD_FLYING_SPEED_RES_X,
	LOAD_FLYING_SPEED_RES_Y,
	LOAD_FLYING_SPEED_RES_Z,

	LOAD_GEAR_DOOR_FRONT_OPEN,
	LOAD_GEAR_DOOR_REAR_OPEN,
	LOAD_GEAR_DOOR_REAR_OPEN2,
	LOAD_GEAR_DOOR_REAR_MIDDLE_OPEN,
	
	LOAD_TURBULENCE_MAGNITUDE_MAX,
	LOAD_TURBULENCE_FORCE_MULTI,
	LOAD_TURBULENCE_ROLL_TORQUE_MULTI,
	LOAD_TURBULENCE_PITCH_TORQUE_MULTI,

	LOAD_BODY_DAMAGE_CONTROL_EFFECT_MULT,

	LOAD_INPUT_SENSITIVITY_FOR_DIFFICULTY,

	LOAD_ON_GROUND_YAW_BOOST_PEAK,
	LOAD_ON_GROUND_YAW_BOOST_CAP,
	LOAD_ENGINE_OFF_GLIDE_MULTI
};

enum eLoadBoatList
{
	LOAD_BOAT_IDENTIFIER = 0,
	LOAD_BOAT_HANDLING_NAME,

	LOAD_BOAT_BBOX_FRONT,
	LOAD_BOAT_BBOX_BACK,
	LOAD_BOAT_BBOX_SIDE,
	LOAD_BOAT_SAMPLE_TOP,
	LOAD_BOAT_SAMPLE_BOTTOM,

	LOAD_BOAT_AQUAPLANE_FORCE,
	LOAD_BOAT_AQUAPLANE_FORCEWATER_MULT,
	LOAD_BOAT_AQUAPLANE_FORCEWATER_CAP,
	LOAD_BOAT_AQUAPLANE_FORCEWATER_APPLY,

	LOAD_BOAT_RUDDER_FORCE,
	LOAD_BOAT_RUDDER_SUBMERGE_OFFSET,
	LOAD_BOAT_RUDDER_FORCE_OFFSET,
    LOAD_BOAT_RUDDER_FORCE_OFFSET_Z_MULT,
	LOAD_BOAT_WAVE_AUDIO,

	LOAD_BOAT_MOVE_RES_X,
	LOAD_BOAT_MOVE_RES_Y,
	LOAD_BOAT_MOVE_RES_Z,

	LOAD_BOAT_TURN_RES_X,
	LOAD_BOAT_TURN_RES_Y,
	LOAD_BOAT_TURN_RES_Z,

	LOAD_BOAT_CAMERA_LOOK_LR,

	LOAD_BOAT_DRAG_COEFF,

	LOAD_KEEL_SPHERE_SIZE,

	LOAD_PROP_RADIUS,

	LOAD_LOW_LOD_ANG_OFFSET,
	LOAD_LOW_LOD_DRAUGHT_OFFSET,

    LOAD_IMPELLER_OFFSET,

	LOAD_IMPELLER_FORCE_MULT,

	LOAD_DINGHY_SPHERE_BUOY_CONST,

	LOAD_PROW_RAISE_MULT,

	LOAD_TRANSMISSION_MULTIPLIER,

	LOAD_TRACTION_MULTIPLIER
};

enum eHandlingDataLoadList
{
	LOAD_NAME = 0,
	LOAD_MASS,
	LOAD_DRAG_COEFF,
	LOAD_SUBMERGED,

	LOAD_COM_X,
	LOAD_COM_Y,
	LOAD_COM_Z,

	LOAD_COM_ABS_X,
	LOAD_COM_ABS_Y,
	LOAD_COM_ABS_Z,

	LOAD_INERTIA_MULTIPLIER_X,
	LOAD_INERTIA_MULTIPLIER_Y,
	LOAD_INERTIA_MULTIPLIER_Z,

	LOAD_DRIVE_TYPE,
	LOAD_DRIVE_GEARS,
	LOAD_DRIVE_FORCE,
	LOAD_DRIVE_INERTIA,
	LOAD_DRIVE_UPSHIFT_CLUTCH_SPEED,
	LOAD_DRIVE_DOWNSHIFT_CLUTCH_SPEED,

	LOAD_DRIVE_MAX_VEL,

	LOAD_BRAKE_FORCE,
	LOAD_BRAKE_BIAS,
	LOAD_BRAKE_HANDBRAKE,
	LOAD_STEER_LOCK,

	LOAD_TRACTION_MAX,
	LOAD_TRACTION_MIN,
	LOAD_TRACTION_LAT,
	//      LOAD_TRACTION_LONG,
	LOAD_TRACTION_SPRING_DELTA,
    LOAD_LOW_SPEED_TRACTION_LOSS_MULT,
	LOAD_CAMBER_STIFFNESS,
	//		LOAD_TRACTION_SPRING_MULT,
	//		LOAD_TRACTION_SPEED_MULT,
	LOAD_TRACTION_BIAS,
	LOAD_TRACTION_LOSS_MULT,

	LOAD_SUSPENSION_FORCE,
	LOAD_SUSPENSION_COMP_DAMP,
	LOAD_SUSPENSION_REBOUND_DAMP,
	LOAD_SUSPENSION_UPPER,
	LOAD_SUSPENSION_LOWER,
	LOAD_SUSPENSION_RAISE,
	LOAD_SUSPENSION_BIAS,
	LOAD_ANTIROLLBAR_FORCE,
	LOAD_ANTIROLLBAR_BIAS,
	LOAD_ROLLCENTRE_HEIGHT_FRONT,
	LOAD_ROLLCENTRE_HEIGHT_REAR,

	LOAD_COLLISION_DAMAGE_SCALE,
	LOAD_WEAPON_DAMAGE_SCALE,
	LOAD_DEFORMATION_DAMAGE_SCALE,
	LOAD_ENGINE_DAMAGE_SCALE,

	LOAD_PETROL_TANK_VOLUME,
	LOAD_OIL_VOLUME,

	LOAD_SEAT_OFFSET_DIST_X,
	LOAD_SEAT_OFFSET_DIST_Y,
	LOAD_SEAT_OFFSET_DIST_Z,
	LOAD_CASH_VALUE,

	LOAD_MODEL_FLAGS,
	LOAD_HANDLING_FLAGS,

	LOAD_CLIP_SET,

	LOAD_AI_HANDLING_INFO,
	LOAD_DOOR_DAMAGE_FLAGS,

	LOAD_DOWNFORCE_MODIFIER,

	LOAD_POPUP_LIGHT_ROTATION,

	LOAD_ROCKET_BOOST_CAPACITY
};

enum eLoadSubList
{
	LOAD_SUB_IDENTIFIER = 0,
	LOAD_SUB_HANDLING_NAME,
	LOAD_SUB_PITCH_MULT,
	LOAD_SUB_PITCH_ANGLE,
	LOAD_SUB_YAW_MULT,
	LOAD_SUB_DIVE_SPEED,
	LOAD_SUB_ROLL_MULT,
	LOAD_SUB_ROLL_STAB,
	LOAD_SUB_TURN_RES_X,
	LOAD_SUB_TURN_RES_Y,
	LOAD_SUB_TURN_RES_Z,
	LOAD_SUB_MOVE_RES_XY,
	LOAD_SUB_MOVE_RES_Z
};

enum eLoadTrailerList
{
	LOAD_TRAILER_IDENTIFIER = 0,
	LOAD_TRAILER_HANDLING_NAME,
	LOAD_TRAILER_ATTACH_LIMIT_PITCH,
	LOAD_TRAILER_ATTACH_LIMIT_ROLL,
	LOAD_TRAILER_ATTACH_LIMIT_YAW,
	LOAD_TRAILER_UPRIGHT_SPRING_CONSTANT,
	LOAD_TRAILER_UPRIGHT_DAMPING_CONSTANT,
	LOAD_TRAILER_ATTACHED_MAX_DISTANCE,
	LOAD_TRAILER_ATTACHED_MAX_PENETRATION,
	LOAD_TRAILER_ATTACH_RAISE_Z,
	LOAD_TRAILER_POS_CONSTRAINT_MASS_RATIO
};

enum eLoadCarList
{
	LOAD_CAR_IDENTIFIER = 0,
	LOAD_CAR_HANDLING_NAME,
	LOAD_CAR_BACK_END_POP_UP_CAR_IMPULSE_MULT,
	LOAD_CAR_BACK_END_POP_UP_BUILDING_IMPUSLE_MULT,
	LOAD_CAR_BACK_END_POP_UP_MAX_DELTA_SPEED
};

int GetHandlingDataPoolSize()
{
	int iPoolSize = fwConfigManager::GetInstance().GetSizeOfPool(ATSTRINGHASH("HandlingData", 0x32AA4601), CONFIGURED_FROM_FILE);
	Assertf(iPoolSize != 0, "Trying to get the handling data pool size before it's been set.");
	// We also do a trap here, to catch it in non-assert builds, but more importantly,
	// to catch it in case it's used from static initialization code when asserts may
	// not yet be working.
	TrapZ(iPoolSize);
	return iPoolSize;
}

class CVehicleHandlingFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
#if CONVERT_HANDLING_DAT_TO_META
		CHandlingDataMgr::ms_bParsingDLC = true;
		CHandlingDataMgr::GetInstance()->Init();
		char destinationName[RAGE_MAX_PATH]={0};
		ASSET.RemoveExtensionFromPath(destinationName,RAGE_MAX_PATH,file.m_filename);
		CHandlingDataMgr::LoadOldHandlingData(file.m_filename,true);
		Verifyf(PARSER.SaveObject(destinationName, "meta", CHandlingDataMgr::GetInstance(), parManager::XML), "Failed to save vehicle handling data");
		CHandlingDataMgr::ms_bParsingDLC = false;

#else
		CHandlingDataMgr::AppendHandlingMetaData(file.m_filename);
#endif
		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & ) {}

} g_VehicleHandlingFileMounter;


class CVehicleExtrasFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		CHandlingDataMgr::LoadVehicleExtrasFile(file.m_filename);
		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & ) {}

} g_VehicleExtrasFileMounter;

CBikeHandlingData::CBikeHandlingData() : CBaseSubHandlingData()
{
	// used as a marker for whether this handling data has been loaded up
	m_fLeanFwdCOMMult = HANDLING_UNUSED_FLOAT_VAL;
	m_fLeanFwdForceMult = INIT_FLOAT;
	m_fLeanBakCOMMult = INIT_FLOAT;
	m_fLeanBakForceMult = INIT_FLOAT;
	m_fMaxBankAngle = INIT_FLOAT;
	m_fFullAnimAngle = INIT_FLOAT;
	m_fFullAnimAngleInv = INIT_FLOAT;

	m_fDesLeanReturnFrac = INIT_FLOAT;
	m_fStickLeanMult = INIT_FLOAT;
	m_fBrakingStabilityMult = INIT_FLOAT;

	m_fInAirSteerMult = INIT_FLOAT;
	m_fWheelieBalancePoint = INIT_FLOAT;
	m_fStoppieBalancePoint = INIT_FLOAT;
	m_fWheelieSteerMult = INIT_FLOAT;
	m_fRearBalanceMult = INIT_FLOAT;
	m_fFrontBalanceMult = INIT_FLOAT;

	m_fBikeGroundSideFrictionMult = INIT_FLOAT;
	m_fBikeWheelGroundSideFrictionMult = INIT_FLOAT;
	m_fBikeOnStandLeanAngle = INIT_FLOAT;
	m_fBikeOnStandSteerAngle = INIT_FLOAT;
	m_fJumpForce = INIT_FLOAT;
}

#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
void CBikeHandlingData::Load(char* pLine)
{
	int count = 0;
	char seps[] = " \t";
	char *token = NULL;

	token = strtok(pLine, seps);	// get first token
	do {
		switch(count)
		{
		case LOAD_BIKE_IDENTIFIER:
			break;

		case LOAD_BIKE_HANDLING_NAME:
			break;

		case LOAD_BIKE_LEAN_FWD_COM_MULT:
			m_fLeanFwdCOMMult = (float)atof(token);
			break;
		case LOAD_BIKE_LEAN_FWD_FORCE_MULT:
			m_fLeanFwdForceMult = (float)atof(token);
			break;
		case LOAD_BIKE_LEAN_BACK_COM_MULT:
			m_fLeanBakCOMMult = (float)atof(token);
			break;
		case LOAD_BIKE_LEAN_BACK_FORCE_MULT:
			m_fLeanBakForceMult = (float)atof(token);
			break;
		case LOAD_BIKE_LEAN_MAX_ANGLE:
			m_fMaxBankAngle = (float)atof(token);
			break;
		case LOAD_BIKE_LEAN_MAX_ANIM_ANGLE:
			m_fFullAnimAngle = (float)atof(token);
			break;

		case LOAD_BIKE_DES_LEAN_RETURN_FRAC:
			m_fDesLeanReturnFrac = (float)atof(token);
			break;
		case LOAD_BIKE_STICK_LEAN_MULT:
			m_fStickLeanMult = (float)atof(token);
			break;
		case LOAD_BIKE_BRAKE_STABILITY_MULT:
			m_fBrakingStabilityMult = (float)atof(token);
			break;

		case LOAD_BIKE_IN_AIR_STEER_MULT:
			m_fInAirSteerMult = (float)atof(token);
			break;
		case LOAD_BIKE_WHEELIE_BALANCE_POINT:
			m_fWheelieBalancePoint = (float)atof(token);
			break;
		case LOAD_BIKE_STOPPIE_BALANCE_POINT:
			m_fStoppieBalancePoint = (float)atof(token);
			break;
		case LOAD_BIKE_WHEELIE_STEER_MULT:
			m_fWheelieSteerMult = (float)atof(token);
			break;
		case LOAD_BIKE_REAR_BALANCE_MULT:
			m_fRearBalanceMult = (float)atof(token);
			break;
		case LOAD_BIKE_FRONT_BALANCE_MULT:
			m_fFrontBalanceMult = (float)atof(token);
			break;

		case LOAD_BIKE_ON_SIDE_FRICTION_MULT:
			m_fBikeGroundSideFrictionMult = (float)atof(token);
			break;
		case LOAD_BIKE_ON_SIDE_WHEEL_FRICTION_MULT:
			m_fBikeWheelGroundSideFrictionMult = (float)atof(token);
			break;
		case LOAD_BIKE_ON_STAND_LEAN_ANGLE:
			m_fBikeOnStandLeanAngle = (float)atof(token);
			break;
		case LOAD_BIKE_ON_STAND_STEER_ANGLE:
			m_fBikeOnStandSteerAngle = (float)atof(token);
			break;
		case LOAD_BIKE_JUMP_FORCE:
			m_fJumpForce = (float)atof(token);
			break;

		}
		token = strtok( NULL, seps );	// get next token
		count++;

	} while (token != NULL);
}
#endif

void CBikeHandlingData::ConvertToGameUnits()
{
	m_fMaxBankAngle = ( DtoR * m_fMaxBankAngle);
	m_fFullAnimAngleInv = 1.0f / m_fMaxBankAngle ;

	m_fFullAnimAngle = ( DtoR * m_fFullAnimAngle);

	m_fWheelieBalancePoint = ( DtoR * m_fWheelieBalancePoint);//rage::Sinf(( DtoR * m_fWheelieBalancePoint));
	m_fStoppieBalancePoint = ( DtoR * m_fStoppieBalancePoint);//rage::Sinf(( DtoR * m_fStoppieBalancePoint));
	m_fBikeOnStandLeanAngle = ( DtoR * m_fBikeOnStandLeanAngle);//rage::Sinf(( DtoR * m_fBikeOnStandLeanAngle));
}

CFlyingHandlingData::CFlyingHandlingData(eHandlingType flyingType) : CBaseSubHandlingData(), m_handlingType(flyingType)
{
	// used as a marker for whether this handling data has been loaded up
	m_fThrust = HANDLING_UNUSED_FLOAT_VAL;
	m_fThrustFallOff = INIT_FLOAT;
	m_fThrustVectoring = INIT_FLOAT;
	// Yaw
	m_fYawMult = INIT_FLOAT;
	m_fYawStabilise = INIT_FLOAT;
	m_fSideSlipMult = INIT_FLOAT;
	m_fInitialYawMult = INIT_FLOAT;
	// Roll
	m_fRollMult = INIT_FLOAT;
	m_fRollStabilise = INIT_FLOAT;
	m_fInitialRollMult = INIT_FLOAT;
	// Pitch
	m_fPitchMult = INIT_FLOAT;
	m_fPitchStabilise = INIT_FLOAT;
	m_fInitialPitchMult = INIT_FLOAT;
	// Lift
	m_fFormLiftMult = INIT_FLOAT;
	m_fAttackLiftMult = INIT_FLOAT;
	m_fAttackDiveMult = INIT_FLOAT;
	// special mults
	m_fGearDownDragV = INIT_FLOAT;
	m_fGearDownLiftMult = INIT_FLOAT;
	m_fWindMult = INIT_FLOAT;
	// Damping
	m_fMoveRes = INIT_FLOAT;
	m_vecTurnRes = Vec3V(V_ZERO);
	m_vecSpeedRes = Vec3V(V_ZERO);
    //landing gear door open amount
    m_fGearDoorFrontOpen = INIT_FLOAT;
    m_fGearDoorRearOpen = INIT_FLOAT;
	m_fGearDoorRearOpen2 = INIT_FLOAT;
	m_fGearDoorRearMOpen = INIT_FLOAT;
	// Damage effect
	m_fTurublenceMagnitudeMax = INIT_FLOAT;
	m_fTurublenceForceMulti = INIT_FLOAT;
	m_fTurublenceRollTorqueMulti = INIT_FLOAT;
	m_fTurublencePitchTorqueMulti = INIT_FLOAT;
	m_fBodyDamageControlEffectMult = INIT_FLOAT;
	// Difficulty
	m_fInputSensitivityForDifficulty = INIT_FLOAT;
	// Driving turning boost (extra yaw)
	m_fOnGroundYawBoostSpeedPeak = INIT_FLOAT;
	m_fOnGroundYawBoostSpeedCap = INIT_FLOAT;
	m_fEngineOffGlideMulti = INIT_FLOAT;
}

#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
void CFlyingHandlingData::Load(char* pLine)
{
	int count = 0;
	char seps[] = " \t";
	char *token = NULL;

	token = strtok(pLine, seps);	// get first token
	do {
		switch(count)
		{
		case LOAD_FLYING_IDENTIFIER:
			break;

		case LOAD_FLYING_HANDLING_NAME:
			break;
			// Thrust
		case LOAD_FLYING_THRUST:
			m_fThrust = (float)atof(token);
			m_fInitialThrust = m_fThrust;
			break;
		case LOAD_FLYING_THRUST_FALLOFF:
			m_fThrustFallOff = (float)atof(token);
			m_fInitialThrustFallOff = m_fThrustFallOff;
			break;
		case LOAD_FLYING_THRUST_VECTOR:
			m_fThrustVectoring = (float)atof(token);
			break;
			// Yaw
		case LOAD_FLYING_YAW_MULT:
			m_fYawMult = (float)atof(token);
			m_fInitialYawMult = m_fYawMult;
			break;
		case LOAD_FLYING_YAW_STAB:
			m_fYawStabilise = (float)atof(token);
			break;
		case LOAD_FLYING_SIDESLIP_MULT:
			m_fSideSlipMult = (float)atof(token);
			break;
			// Roll
		case LOAD_FLYING_ROLL_MULT:
			m_fRollMult = (float)atof(token);
			m_fInitialRollMult = m_fRollMult;
			break;
		case LOAD_FLYING_ROLL_STAB:
			m_fRollStabilise = (float)atof(token);
			break;
			// Pitch
		case LOAD_FLYING_PITCH_MULT:
			m_fPitchMult = (float)atof(token);
			m_fInitialPitchMult = m_fPitchMult;
			break;
		case LOAD_FLYING_PITCH_STAB:
			m_fPitchStabilise = (float)atof(token);
			break;
			// Lift
		case LOAD_FLYING_FORM_LIFT:
			m_fFormLiftMult = (float)atof(token);
			break;
		case LOAD_FLYING_ATTACK_LIFT:
			m_fAttackLiftMult = (float)atof(token);
			break;
		case LOAD_FLYING_ATTACK_DIVE:
			m_fAttackDiveMult = (float)atof(token);
			break;
			// special mults
		case LOAD_FLYING_GEAR_UP_RES_MULT:
			m_fGearDownDragV = (float)atof(token);
			break;
		case LOAD_FLYING_GEAR_DOWN_LIFT_MULT:
			m_fGearDownLiftMult = (float)atof(token);
			break;
		case LOAD_FLYING_WIND_MULT:
			m_fWindMult = (float)atof(token);
			break;
			// Move Resistance
		case LOAD_FLYING_MOVE_RES:
			m_fMoveRes = (float)atof(token);
			break;
			// Turn Resistance
		case LOAD_FLYING_TURN_RES_X:
			m_vecTurnRes.SetXf((float)atof(token));
			break;
		case LOAD_FLYING_TURN_RES_Y:
			m_vecTurnRes.SetYf((float)atof(token));
			break;
		case LOAD_FLYING_TURN_RES_Z:
			m_vecTurnRes.SetZf((float)atof(token));
			break;
			// TurnSpeed Based Damping
		case LOAD_FLYING_SPEED_RES_X:
			m_vecSpeedRes.SetXf((float)atof(token));
			break;
		case LOAD_FLYING_SPEED_RES_Y:
			m_vecSpeedRes.SetYf((float)atof(token));
			break;
		case LOAD_FLYING_SPEED_RES_Z:
			m_vecSpeedRes.SetZf((float)atof(token));
			break;
            // Landing gear open amount
        case LOAD_GEAR_DOOR_FRONT_OPEN:
            m_fGearDoorFrontOpen = (float)atof(token);//how much to open the front landing gear
            break;
        case LOAD_GEAR_DOOR_REAR_OPEN:
            m_fGearDoorRearOpen = (float)atof(token);//how much to open the rear landing gear
            break;
		case LOAD_GEAR_DOOR_REAR_OPEN2:
			m_fGearDoorRearOpen2 = (float)atof(token);//how much to open the rear landing gear
			break;
		case LOAD_GEAR_DOOR_REAR_MIDDLE_OPEN:
			m_fGearDoorRearMOpen = (float)atof(token);//how much to open the rear landing gear
			break;
		case LOAD_TURBULENCE_MAGNITUDE_MAX:
			m_fTurublenceMagnitudeMax = (float)atof(token);
			break;
		case LOAD_TURBULENCE_FORCE_MULTI:
			m_fTurublenceForceMulti = (float)atof(token);
			break;

		case LOAD_TURBULENCE_ROLL_TORQUE_MULTI:
			m_fTurublenceRollTorqueMulti = (float)atof(token);
			break;

		case LOAD_TURBULENCE_PITCH_TORQUE_MULTI:
			m_fTurublencePitchTorqueMulti = (float)atof(token);
			break;
		case LOAD_BODY_DAMAGE_CONTROL_EFFECT_MULT:
			m_fBodyDamageControlEffectMult = (float)atof(token);
			break;
		case LOAD_INPUT_SENSITIVITY_FOR_DIFFICULTY:
			m_fInputSensitivityForDifficulty = (float)atof(token);
			break;
		case LOAD_ON_GROUND_YAW_BOOST_PEAK:
			m_fOnGroundYawBoostSpeedPeak = (float)atof(token);
			break;
		case LOAD_ON_GROUND_YAW_BOOST_CAP:
			m_fOnGroundYawBoostSpeedCap = (float)atof(token);
			break;
		case LOAD_ENGINE_OFF_GLIDE_MULTI:
			m_fEngineOffGlideMulti = (float)atof(token);
			break;
		}

		token = strtok( NULL, seps );	// get next token
		count++;
	} while (token != NULL);
}
#endif

void CFlyingHandlingData::ConvertToGameUnits()
{
	m_fGearDoorFrontOpen *= DtoR;
	m_fGearDoorRearOpen  *= DtoR;
	m_fGearDoorRearOpen2 *= DtoR;
	m_fGearDoorRearMOpen *= DtoR;
}

CSeaPlaneHandlingData::CSeaPlaneHandlingData(): CBaseSubHandlingData()
{
	// Used as a marker for whether this handling data has been loaded up.

	m_fLeftPontoonComponentId = 0;
	m_fRightPontoonComponentId = 0;

	m_fPontoonBuoyConst = INIT_FLOAT;
	m_fPontoonSampleSizeFront = INIT_FLOAT;
	m_fPontoonSampleSizeMiddle = INIT_FLOAT;
	m_fPontoonSampleSizeRear = INIT_FLOAT;
	m_fPontoonLengthFractionForSamples = INIT_FLOAT;
	m_fPontoonDragCoefficient = INIT_FLOAT;
	m_fPontoonVerticalDampingCoefficientUp = INIT_FLOAT;
	m_fPontoonVerticalDampingCoefficientDown = INIT_FLOAT;
	m_fKeelSphereSize = INIT_FLOAT;
}

CBoatHandlingData::CBoatHandlingData(): CBaseSubHandlingData()
{
	// used as a marker for whether this handling data has been loaded up
	m_fBoxFrontMult = HANDLING_UNUSED_FLOAT_VAL;
	m_fBoxRearMult = INIT_FLOAT;
	m_fBoxSideMult = INIT_FLOAT;
	m_fSampleTop = INIT_FLOAT;
	m_fSampleBottom = INIT_FLOAT;

	m_fAquaplaneForce = INIT_FLOAT;
	m_fAquaplanePushWaterMult = INIT_FLOAT;
	m_fAquaplanePushWaterCap = INIT_FLOAT;
	m_fAquaplanePushWaterApply = INIT_FLOAT;

	m_fRudderForce = INIT_FLOAT;
	m_fRudderOffsetSubmerge = INIT_FLOAT;
	m_fRudderOffsetForce = INIT_FLOAT;
    m_fRudderOffsetForceZMult = INIT_FLOAT;
	m_fWaveAudioMult = INIT_FLOAT;

	m_vecMoveResistance = Vec3V(V_ZERO);
	m_vecTurnResistance = Vec3V(V_ZERO);

	m_fLook_L_R_CamHeight = INIT_FLOAT;

	m_fDragCoefficient = INIT_FLOAT;

    m_fKeelSphereSize = INIT_FLOAT;

	m_fPropRadius = INIT_FLOAT;

	m_fLowLodAngOffset = INIT_FLOAT;
	m_fLowLodDraughtOffset = INIT_FLOAT;

    m_fImpellerOffset = INIT_FLOAT;

	m_fImpellerForceMult = INIT_FLOAT;

	m_fDinghySphereBuoyConst = INIT_FLOAT;

	m_fProwRaiseMult = INIT_FLOAT;

	m_fTransmissionMultiplier = INIT_FLOAT;

	m_fTractionMultiplier = INIT_FLOAT;
}

#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
void CBoatHandlingData::Load(char* pLine)
{
	int count = 0;
	char seps[] = " \t";
	char *token = NULL;

	token = strtok(pLine, seps);	// get first token
	do {
		switch(count)
		{
		case LOAD_BOAT_IDENTIFIER:
			break;

		case LOAD_BOAT_HANDLING_NAME:
			break;

			// Size bounding box
		case LOAD_BOAT_BBOX_FRONT:
			m_fBoxFrontMult = (float)atof(token);
			break;
		case LOAD_BOAT_BBOX_BACK:
			m_fBoxRearMult = (float)atof(token);
			break;
		case LOAD_BOAT_BBOX_SIDE:
			m_fBoxSideMult = (float)atof(token);
			break;

			// size water samples
		case LOAD_BOAT_SAMPLE_TOP:
			m_fSampleTop = (float)atof(token);
			break;
		case LOAD_BOAT_SAMPLE_BOTTOM:
			m_fSampleBottom = (float)atof(token);
			break;

			// Aquaplane 
		case LOAD_BOAT_AQUAPLANE_FORCE:
			m_fAquaplaneForce = (float)atof(token);
			break;
		case LOAD_BOAT_AQUAPLANE_FORCEWATER_MULT:
			m_fAquaplanePushWaterMult = (float)atof(token);
			break;
		case LOAD_BOAT_AQUAPLANE_FORCEWATER_CAP:
			m_fAquaplanePushWaterCap = (float)atof(token);
			break;
		case LOAD_BOAT_AQUAPLANE_FORCEWATER_APPLY:
			m_fAquaplanePushWaterApply = (float)atof(token);
			break;

		case LOAD_BOAT_RUDDER_FORCE:
			m_fRudderForce = (float)atof(token);
			break;
		case LOAD_BOAT_RUDDER_SUBMERGE_OFFSET:
			m_fRudderOffsetSubmerge = (float)atof(token);
			break;
		case LOAD_BOAT_RUDDER_FORCE_OFFSET:
			m_fRudderOffsetForce = (float)atof(token);
			break;
        case LOAD_BOAT_RUDDER_FORCE_OFFSET_Z_MULT:
            m_fRudderOffsetForceZMult = (float)atof(token);
            break;
		case LOAD_BOAT_WAVE_AUDIO:
			m_fWaveAudioMult = (float)atof(token);
			break;

			// Move Resistance
		case LOAD_BOAT_MOVE_RES_X:
			m_vecMoveResistance.SetXf((float)atof(token));
			break;
		case LOAD_BOAT_MOVE_RES_Y:
			m_vecMoveResistance.SetYf((float)atof(token));
			break;
		case LOAD_BOAT_MOVE_RES_Z:
			m_vecMoveResistance.SetZf((float)atof(token));
			break;
			// Turn Resistance
		case LOAD_BOAT_TURN_RES_X:
			m_vecTurnResistance.SetXf((float)atof(token));
			break;
		case LOAD_BOAT_TURN_RES_Y:
			m_vecTurnResistance.SetYf((float)atof(token));
			break;
		case LOAD_BOAT_TURN_RES_Z:
			m_vecTurnResistance.SetZf((float)atof(token));
			break;
		case LOAD_BOAT_CAMERA_LOOK_LR:
			m_fLook_L_R_CamHeight = (float)atof(token);
			break;
		case LOAD_BOAT_DRAG_COEFF:
			m_fDragCoefficient = (float)atof(token);
			break;
        case LOAD_KEEL_SPHERE_SIZE:
            m_fKeelSphereSize = (float)atof(token);
            break;
		case LOAD_PROP_RADIUS:
			m_fPropRadius = (float)atof(token);
			break;
		case LOAD_LOW_LOD_ANG_OFFSET:
			m_fLowLodAngOffset = (float)atof(token);
			break;
		case LOAD_LOW_LOD_DRAUGHT_OFFSET:
			m_fLowLodDraughtOffset = (float)atof(token);
			break;
        case LOAD_IMPELLER_OFFSET:
            m_fImpellerOffset = (float)atof(token);
            break;
		case LOAD_IMPELLER_FORCE_MULT:
			m_fImpellerForceMult = (float)atof(token);
			break;
		case LOAD_DINGHY_SPHERE_BUOY_CONST:
			m_fDinghySphereBuoyConst = (float)atof(token);
			break;
		case LOAD_PROW_RAISE_MULT:
			m_fProwRaiseMult = (float)atof(token);
			break;
		case LOAD_TRANSMISSION_MULTIPLIER:
			m_fTransmissionMultiplier = (float)atof(token);
		case LOAD_TRACTION_MULTIPLIER:
			m_fTractionMultiplier = (float)atof(token);
		}

		token = strtok( NULL, seps );	// get next token
		count++;
	} while (token != NULL);
}
#endif

CVehicleWeaponHandlingData::CVehicleWeaponHandlingData() : CBaseSubHandlingData()
{
	for(int i =0; i < MAX_NUM_VEHICLE_WEAPONS; i++)
	{
		m_WeaponSeats[i] = 0;
		m_uWeaponHash[i].Clear();
	}
	for(int i =0; i < MAX_NUM_VEHICLE_TURRETS; i++)
	{
		m_fTurretSpeed[i] = INIT_FLOAT;
		m_fTurretPitchMin[i] = INIT_FLOAT;
		m_fTurretPitchMax[i] = INIT_FLOAT;
		m_fTurretCamPitchMin[i] = INIT_FLOAT;
		m_fTurretCamPitchMax[i] = INIT_FLOAT;
		m_fBulletVelocityForGravity[i] = INIT_FLOAT;
		m_fTurretPitchForwardMin[i] = INIT_FLOAT;
	}

	m_fUvAnimationMult = INIT_FLOAT;
	m_fMiscGadgetVar = INIT_FLOAT;
    m_fWheelImpactOffset = INIT_FLOAT;
}

#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
void CVehicleWeaponHandlingData::Load(char* pLine)
{
	// This function needs updating if these defines change
	CompileTimeAssert(MAX_NUM_VEHICLE_WEAPONS == 3);
	CompileTimeAssert(MAX_NUM_VEHICLE_TURRETS == 2);

	int iCount = 0;
	char seps[] = " \t";
	char* token = strtok(pLine,seps);
	do
	{
		switch(iCount)
		{
		case LOAD_WEAPON_IDENTIFIER:
			break;
		case LOAD_WEAPON_HANDLING_NAME:
			break;

		case LOAD_WEAPON_1:
			if(strcmp(token,"NULL"))	// If NOT equal to NULL then set to hash
			{
				m_uWeaponHash[0].SetHashWithString(atHashString::ComputeHash(token), token);
			}
			break;
		case LOAD_WEAPON_1_SEAT:
			m_WeaponSeats[0] = atoi(token);
			break;
		case LOAD_WEAPON_2:
			if(strcmp(token,"NULL"))	// If NOT equal to NULL then set to hash
			{
				m_uWeaponHash[1].SetHashWithString(atHashString::ComputeHash(token), token);
			}
			break;
		case LOAD_WEAPON_2_SEAT:
			m_WeaponSeats[1] = atoi(token);
			break;
		case LOAD_WEAPON_3:
			if(strcmp(token,"NULL"))	// If NOT equal to NULL then set to hash
			{
				m_uWeaponHash[2].SetHashWithString(atHashString::ComputeHash(token), token);
			}
			break;
		case LOAD_WEAPON_3_SEAT:
			m_WeaponSeats[2] = atoi(token);
			break;
		case LOAD_WEAPON_TURRET_SPEED_1:
			m_fTurretSpeed[0] = (float)atof(token);
			break;
		case LOAD_WEAPON_TURRET_SPEED_2:
			m_fTurretSpeed[1] = (float)atof(token);
			break;
		case LOAD_WEAPON_TURRET_PITCH_MIN_1:
			m_fTurretPitchMin[0] = (float)atof(token);
			break;
		case LOAD_WEAPON_TURRET_PITCH_MIN_2:
			m_fTurretPitchMin[1] = (float)atof(token);
			break;
		case LOAD_WEAPON_TURRET_PITCH_MAX_1:
			m_fTurretPitchMax[0] = (float)atof(token);
			break;
		case LOAD_WEAPON_TURRET_PITCH_MAX_2:
			m_fTurretPitchMax[1] = (float)atof(token);
			break;
		case LOAD_WEAPON_TURRET_CAM_PITCH_MIN_1:
			m_fTurretCamPitchMin[0] = (float)atof(token);
			break;
		case LOAD_WEAPON_TURRET_CAM_PITCH_MIN_2:
			m_fTurretCamPitchMin[1] = (float)atof(token);
			break;
		case LOAD_WEAPON_TURRET_CAM_PITCH_MAX_1:
			m_fTurretCamPitchMax[0] = (float)atof(token);
			break;
		case LOAD_WEAPON_TURRET_CAM_PITCH_MAX_2:
			m_fTurretCamPitchMax[1] = (float)atof(token);
			break;
		case LOAD_WEAPON_UV_ANIM_MULT:
			m_fUvAnimationMult = (float)atof(token);
			break;
		case LOAD_WEAPON_MISC_VAR_1:
			m_fMiscGadgetVar = (float)atof(token);
			break;
        case LOAD_WEAPON_WHEEL_IMPACT_OFFSET:
            m_fWheelImpactOffset = (float)atof(token);
            break;
        case LOAD_BULLET_VELOCITY_FOR_GRAVITY:
            m_fBulletVelocityForGravity[0] = (float)atof(token);
            break;
		case LOAD_TURRECT_PITCH_FORWARD_MIN:
			m_fTurretPitchForwardMin[0] = (float)atof(token);
			break;
		}
		iCount++;
		token = strtok(NULL,seps);
	}
	while(token);
}
#endif

CSubmarineHandlingData::CSubmarineHandlingData() : CBaseSubHandlingData()
{
	m_vTurnRes.x = INIT_FLOAT;
	m_vTurnRes.y = INIT_FLOAT;
	m_vTurnRes.z = INIT_FLOAT;

	m_fMoveResXY = INIT_FLOAT;
	m_fMoveResZ = INIT_FLOAT;
	m_fPitchMult = INIT_FLOAT;
	m_fPitchAngle = INIT_FLOAT;

	m_fYawMult = INIT_FLOAT;
	m_fDiveSpeed = INIT_FLOAT;

    m_fRollMult = INIT_FLOAT;
    m_fRollStab = INIT_FLOAT;
}

#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
void CSubmarineHandlingData::Load(char* pLine)
{
	int iCount = 0;
	char seps[] = " \t";
	char* token = strtok(pLine,seps);
	do
	{
		switch(iCount)
		{
		case LOAD_SUB_IDENTIFIER:
			break;
		case LOAD_SUB_HANDLING_NAME:
			break;
		case LOAD_SUB_PITCH_MULT:
			m_fPitchMult = (float)atof(token);
			break;
		case LOAD_SUB_PITCH_ANGLE:
			m_fPitchAngle = (float)atof(token);
			break;
		case LOAD_SUB_YAW_MULT:
			m_fYawMult = (float)atof(token);
			break;
		case LOAD_SUB_DIVE_SPEED:
			m_fDiveSpeed = (float)atof(token);
			break;
        case LOAD_SUB_ROLL_MULT:
            m_fRollMult = (float)atof(token);
            break;
        case LOAD_SUB_ROLL_STAB:
            m_fRollStab = (float)atof(token);
            break;
		case LOAD_SUB_TURN_RES_X:
			m_vTurnRes.x = (float)atof(token);
			break;
		case LOAD_SUB_TURN_RES_Y:
			m_vTurnRes.y = (float)atof(token);
			break;
		case LOAD_SUB_TURN_RES_Z:
			m_vTurnRes.z = (float)atof(token);
			break;
		case LOAD_SUB_MOVE_RES_XY:
			m_fMoveResXY = (float)atof(token);
			break;
		case LOAD_SUB_MOVE_RES_Z:
			m_fMoveResZ = (float)atof(token);
			break;
		}
		iCount++;
		token = strtok(NULL,seps);
	}
	while(token);
}
#endif

CTrainHandlingData::CTrainHandlingData(): CBaseSubHandlingData()
{
}

#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
void CTrainHandlingData::Load(char* UNUSED_PARAM(pLine))
{
}
#endif


CTrailerHandlingData::CTrailerHandlingData(): CBaseSubHandlingData()
{
	// Don't use the normal init value of zero
	// The default behavior we want is to have no constraint which is triggered via negative limit values
	m_fAttachLimitPitch = -1.0f;
	m_fAttachLimitRoll = -1.0f;
	m_fAttachLimitYaw = -1.0f;
	m_fUprightSpringConstant = 0.0f;
	m_fUprightDampingConstant = 0.0f;
	m_fAttachedMaxDistance = 0.0f;
	m_fAttachedMaxPenetration = 0.0f;
	m_fAttachRaiseZ= 0.0f;
	m_fPosConstraintMassRatio = 1.0f;
}

#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
void CTrailerHandlingData::Load(char* pLine)
{
	int count = 0;
	char seps[] = " \t";
	char *token = NULL;

	token = strtok(pLine, seps);	// get first token
	do {
		switch(count)
		{
		case LOAD_TRAILER_IDENTIFIER:
			break;

		case LOAD_TRAILER_HANDLING_NAME:
			break;

		case LOAD_TRAILER_ATTACH_LIMIT_PITCH:
			m_fAttachLimitPitch = (float)atof(token);
			break;
		case LOAD_TRAILER_ATTACH_LIMIT_ROLL:
			m_fAttachLimitRoll = (float)atof(token);
			break;
		case LOAD_TRAILER_ATTACH_LIMIT_YAW:
			m_fAttachLimitYaw = (float)atof(token);
			break;
		case LOAD_TRAILER_UPRIGHT_SPRING_CONSTANT:
			m_fUprightSpringConstant = (float)atof(token);
			break;
		case LOAD_TRAILER_UPRIGHT_DAMPING_CONSTANT:
			m_fUprightDampingConstant = (float)atof(token);
			break;
		case LOAD_TRAILER_ATTACHED_MAX_DISTANCE:
			m_fAttachedMaxDistance = (float)atof(token);
			break;
		case LOAD_TRAILER_ATTACHED_MAX_PENETRATION:
			m_fAttachedMaxPenetration = (float)atof(token);
			break;
		case LOAD_TRAILER_ATTACH_RAISE_Z:
			m_fAttachRaiseZ = (float)atof(token);
			break;
		case LOAD_TRAILER_POS_CONSTRAINT_MASS_RATIO:
			m_fPosConstraintMassRatio = (float)atof(token);
		}
		token = strtok( NULL, seps );	// get next token
		count++;

	} while (token != NULL);
}
#endif

CCarHandlingData::CCarHandlingData(): CBaseSubHandlingData()
{
	m_fBackEndPopUpCarImpulseMult = 0.0f;
	m_fBackEndPopUpBuildingImpulseMult = 0.0f;
	m_fBackEndPopUpMaxDeltaSpeed = 0.0f;

    m_strAdvancedFlags.Clear();
    aFlags = 0;

}

void CCarHandlingData::ConvertToGameUnits()
{
    if( m_strAdvancedFlags )
    {
        sscanf( m_strAdvancedFlags.TryGetCStr(), "%x", &aFlags );
    }
}

CSpecialFlightHandlingData::CSpecialFlightHandlingData(): CBaseSubHandlingData()
{
	m_strFlags.Clear();
	m_flags = 0;
}

void CSpecialFlightHandlingData::ConvertToGameUnits()
{
	if( m_strFlags )
	{
		sscanf( m_strFlags.TryGetCStr(), "%x", &m_flags );
	}
}

#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
void CCarHandlingData::Load(char *pLine)
{
	int count = 0;
	char seps[] = " \t";
	char *token = NULL;

	token = strtok(pLine, seps);	// get first token
	do {
		switch(count)
		{
		case LOAD_CAR_IDENTIFIER:
			break;
		case LOAD_CAR_HANDLING_NAME:
			break;
		case LOAD_CAR_BACK_END_POP_UP_CAR_IMPULSE_MULT:
			m_fBackEndPopUpCarImpulseMult = (float)atof(token);
			break;
		case LOAD_CAR_BACK_END_POP_UP_BUILDING_IMPUSLE_MULT:
			m_fBackEndPopUpBuildingImpulseMult = (float)atof(token);
			break;
		case LOAD_CAR_BACK_END_POP_UP_MAX_DELTA_SPEED:
			m_fBackEndPopUpMaxDeltaSpeed = (float)atof(token);
			break;
		}
		token = strtok( NULL, seps );	// get next token
		count++;

	} while (token != NULL);
}
#endif

CHandlingData::CHandlingData()
{
	// used as a marker for whether this handling data has been loaded up
	m_fMass = HANDLING_UNUSED_FLOAT_VAL;

//	fTurnMass = INIT_FLOAT;
	m_fInitialDragCoeff = INIT_FLOAT;
	m_fDownforceModifier = 1.0f;
	m_fPopUpLightRotation = 0.0f;

	m_vecCentreOfMassOffset = Vec3V(V_ZERO);
    m_vecInertiaMultiplier = Vec3V(V_ZERO);

	m_fPercentSubmerged = 0;
	m_fBuoyancyConstant = INIT_FLOAT;


// TRANSMISSION DATA
	m_fDriveBiasFront = INIT_FLOAT;
	m_fDriveBiasRear = INIT_FLOAT;
	//m_nDriveType = 0;
	m_nInitialDriveGears = 0;
	m_fInitialDriveForce = INIT_FLOAT;
	m_fDriveInertia = INIT_FLOAT;
	m_fInitialDriveMaxVel = INIT_FLOAT;
	m_fClutchChangeRateScaleUpShift = INIT_FLOAT;
	m_fClutchChangeRateScaleDownShift = INIT_FLOAT;

	m_fInitialDriveMaxFlatVel = INIT_FLOAT;

	m_fBrakeForce = INIT_FLOAT;
	m_fBrakeBiasFront = INIT_FLOAT;
	m_fBrakeBiasRear = INIT_FLOAT;
	m_fHandBrakeForce = INIT_FLOAT;

	m_fSteeringLock = INIT_FLOAT;

// WHEEL TRACTION DATA
	m_fTractionCurveMax = INIT_FLOAT;
	m_fTractionCurveMax_Inv = INIT_FLOAT;
	m_fTractionCurveMin = INIT_FLOAT;
	m_fTractionCurveLateral = INIT_FLOAT;
//	m_fTractionCurveLongitudinal = INIT_FLOAT;

	m_fTractionSpringDeltaMax = INIT_FLOAT;
	m_fTractionSpringDeltaMax_Inv = INIT_FLOAT;
//	m_fTractionSpringForceMult = INIT_FLOAT;
//	m_fTractionSpeedLateralMult = INIT_FLOAT;

    m_fLowSpeedTractionLossMult = INIT_FLOAT;

	m_fCamberStiffnesss = INIT_FLOAT;

	m_fTractionBiasFront = INIT_FLOAT;
	m_fTractionBiasRear = INIT_FLOAT;

	m_fTractionLossMult = INIT_FLOAT;

// SUSPENSION
	m_fSuspensionForce = INIT_FLOAT;
	m_fSuspensionCompDamp = INIT_FLOAT;
	m_fSuspensionReboundDamp = INIT_FLOAT;

	m_fSuspensionUpperLimit = INIT_FLOAT;
	m_fSuspensionLowerLimit = INIT_FLOAT;
	m_fSuspensionRaise = INIT_FLOAT;
	m_fSuspensionBiasFront = INIT_FLOAT;
	m_fSuspensionBiasRear = INIT_FLOAT;

	m_fAntiRollBarForce = INIT_FLOAT;
	m_fAntiRollBarBiasFront = INIT_FLOAT;
	m_fAntiRollBarBiasRear = INIT_FLOAT;

    m_fRollCentreHeightFront = INIT_FLOAT;
    m_fRollCentreHeightRear = INIT_FLOAT;

// MISC
	m_fCollisionDamageMult = INIT_FLOAT;
	m_fWeaponDamageMult = 1.0f;
	m_fDeformationDamageMult = 1.0f;
	m_fEngineDamageMult = 30.0f;

    m_fPetrolTankVolume = 30.0f;
    m_fOilVolume = 5.0f;
	m_fPetrolConsumptionRate = 0.0f;

	m_fSeatOffsetDistX = 0.0f;
	m_fSeatOffsetDistY = 0.0f;
	m_fSeatOffsetDistZ = 0.0f;
	m_nMonetaryValue = 10000;

	mFlags = 0;	// model option flags
	hFlags = 0;	// handling option flags
	dFlags = 0; // door damage flags

	m_strModelFlags.Clear();
	m_strHandlingFlags.Clear();
	m_strDamageFlags.Clear();
	m_AIHandling.Clear();

	m_fEstimatedMaxFlatVel = 0.0f;

	m_pAIHandlingInfo = NULL;
	m_pBoardingPoints = NULL;

	// Max boost charge
	m_fRocketBoostCapacity = 1.25f;

// PRIVATE STUFF
#if VERIFY_OLD_HANDLING_DAT
	sysMemSet(m_OldSubHandlingData,0,MAX_HANDLING_TYPES_PER_VEHICLE * sizeof(CBaseSubHandlingData*));
#endif

#if CONVERT_HANDLING_DAT_TO_META
	for(int i = 0; i < MAX_HANDLING_TYPES_PER_VEHICLE; i++)
	{
		m_SubHandlingData.PushAndGrow(NULL);
	}
#endif
}

#if !(VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META)
CBaseSubHandlingData* CHandlingData::GetHandlingData(eHandlingType handlingType)
{
	for(int i = 0; i<m_SubHandlingData.GetCount(); i++)
	{
		CBaseSubHandlingData* handlingDataPtr =  m_SubHandlingData[i];
		if(handlingDataPtr && handlingType == handlingDataPtr->GetHandlingType())
		{
			return handlingDataPtr;
		}
	}
	return NULL;
}
#endif

#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
CBaseSubHandlingData* CHandlingData::GetHandlingData(eHandlingType handlingType, bool bMeta)
{
	for(int i = 0; i<MAX_HANDLING_TYPES_PER_VEHICLE; i++)
	{
		CBaseSubHandlingData* handlingDataPtr = NULL;
		if(bMeta)	
		{ 
			handlingDataPtr = m_SubHandlingData[i];
		}
		else
		{
			handlingDataPtr = m_OldSubHandlingData[i];
		}
		if(handlingDataPtr && handlingType == handlingDataPtr->GetHandlingType())
		{
			return handlingDataPtr;
		}
	}
	return NULL;
}

void CHandlingData::Load(char* pLine)
{
	int count = 0;
	char seps[] = " \t";
	char *token = NULL;

	token = strtok(pLine, seps);	// get first token
	do{
		switch(count)
		{
		case LOAD_NAME:
			m_handlingName = atHashString(token);
			break;
		case LOAD_MASS:
			m_fMass = (float)atof(token);
			break;
		case LOAD_DRAG_COEFF:
			m_fInitialDragCoeff = (float)atof(token);
			break;
		case LOAD_SUBMERGED:
			m_fPercentSubmerged = (float)atoi(token);
			break;

		case LOAD_COM_X:
			m_vecCentreOfMassOffset.SetXf((float)atof(token));
			break;
		case LOAD_COM_Y:
			m_vecCentreOfMassOffset.SetYf((float)atof(token));
			break;
		case LOAD_COM_Z:
			m_vecCentreOfMassOffset.SetZf((float)atof(token));
			break;

#ifdef CHECK_VEHICLE_SETUP
        case LOAD_COM_ABS_X:
            m_vecCentreOfMassAbsolute.x = (float)atof(token);
            break;
        case LOAD_COM_ABS_Y:
            m_vecCentreOfMassAbsolute.y = (float)atof(token);
            break;
        case LOAD_COM_ABS_Z:
            m_vecCentreOfMassAbsolute.z = (float)atof(token);
            break;
#else
        case LOAD_COM_ABS_X:
        case LOAD_COM_ABS_Y:
        case LOAD_COM_ABS_Z:
            break;
#endif

        case LOAD_INERTIA_MULTIPLIER_X:
            m_vecInertiaMultiplier.SetXf((float)atof(token));
            break;
        case LOAD_INERTIA_MULTIPLIER_Y:
            m_vecInertiaMultiplier.SetYf((float)atof(token));
            break;
        case LOAD_INERTIA_MULTIPLIER_Z:
            m_vecInertiaMultiplier.SetZf((float)atof(token));
            break;


		case LOAD_DRIVE_TYPE:
			m_fDriveBiasFront = (float)atof(token);
			break;
		case LOAD_DRIVE_GEARS:
			m_nInitialDriveGears = (u8)atoi(token);
			break;
		case LOAD_DRIVE_FORCE:
			m_fInitialDriveForce = (float)atof(token);
			break;
		case LOAD_DRIVE_INERTIA:
			m_fDriveInertia = (float)atof(token);
			break;
		case LOAD_DRIVE_UPSHIFT_CLUTCH_SPEED:
			m_fClutchChangeRateScaleUpShift = (float)atof(token);
			break;
		case LOAD_DRIVE_DOWNSHIFT_CLUTCH_SPEED:
			m_fClutchChangeRateScaleDownShift = (float)atof(token);
			break;
		case LOAD_DRIVE_MAX_VEL:
			m_fInitialDriveMaxFlatVel = (float)atof(token);
			break;

		case LOAD_BRAKE_FORCE:
			m_fBrakeForce = (float)atof(token);
			break;
		case LOAD_BRAKE_BIAS:
			m_fBrakeBiasFront = (float)atof(token);
			break;
		case LOAD_BRAKE_HANDBRAKE:
			m_fHandBrakeForce = (float)atof(token);
			break;
		case LOAD_STEER_LOCK:
			m_fSteeringLock = (float)atof(token);
			break;

		case LOAD_TRACTION_MAX:
			m_fTractionCurveMax = (float)atof(token);
			break;
		case LOAD_TRACTION_MIN:
			m_fTractionCurveMin = (float)atof(token);
			break;
		case LOAD_TRACTION_LAT:
			m_fTractionCurveLateral = (float)atof(token);
			break;
//		case LOAD_TRACTION_LONG:
//			m_fTractionCurveLongitudinal = (float)atof(token);
//			break;
		case LOAD_TRACTION_SPRING_DELTA:
			m_fTractionSpringDeltaMax = (float)atof(token);
			break;
        case LOAD_LOW_SPEED_TRACTION_LOSS_MULT:
            m_fLowSpeedTractionLossMult = (float)atof(token);
            break;
		case LOAD_CAMBER_STIFFNESS:
			m_fCamberStiffnesss = (float)atof(token);
			break;
//		case LOAD_TRACTION_SPRING_MULT:
//			m_fTractionSpringForceMult = (float)atof(token);
//			break;
//		case LOAD_TRACTION_SPEED_MULT:
//			m_fTractionSpeedLateralMult = (float)atof(token);
//			break;
		case LOAD_TRACTION_BIAS:
			m_fTractionBiasFront = (float)atof(token);
			break;

		case LOAD_TRACTION_LOSS_MULT:
			m_fTractionLossMult = (float)atof(token);
			break;

		case LOAD_SUSPENSION_FORCE:
			m_fSuspensionForce = (float)atof(token);
			break;
		case LOAD_SUSPENSION_COMP_DAMP:
			m_fSuspensionCompDamp = (float)atof(token);
			break;
		case LOAD_SUSPENSION_REBOUND_DAMP:
			m_fSuspensionReboundDamp = (float)atof(token);
			break;
		case LOAD_SUSPENSION_UPPER:
			m_fSuspensionUpperLimit = (float)atof(token);
			break;
		case LOAD_SUSPENSION_LOWER:
			m_fSuspensionLowerLimit = (float)atof(token);
			break;
		case LOAD_SUSPENSION_RAISE:
			m_fSuspensionRaise = (float)atof(token);
			break;
		case LOAD_SUSPENSION_BIAS:
			m_fSuspensionBiasFront = (float)atof(token);
			break;
		case LOAD_ANTIROLLBAR_FORCE:
			m_fAntiRollBarForce = (float)atof(token);
			break;
		case LOAD_ANTIROLLBAR_BIAS:
			m_fAntiRollBarBiasFront = (float)atof(token);
			break;

        case LOAD_ROLLCENTRE_HEIGHT_FRONT:
            m_fRollCentreHeightFront = (float)atof(token);
            break;
        case LOAD_ROLLCENTRE_HEIGHT_REAR:
            m_fRollCentreHeightRear = (float)atof(token);
            break;

		case LOAD_COLLISION_DAMAGE_SCALE:
			m_fCollisionDamageMult = (float)atof(token);
			break;
		case LOAD_WEAPON_DAMAGE_SCALE:
			m_fWeaponDamageMult = (float)atof(token);
			break;
		case LOAD_DEFORMATION_DAMAGE_SCALE:
			m_fDeformationDamageMult = (float)atof(token);
			break;
		case LOAD_ENGINE_DAMAGE_SCALE:
			m_fEngineDamageMult = (float)atof(token);
			break;

        case LOAD_PETROL_TANK_VOLUME:
            m_fPetrolTankVolume = (float)atof(token);
            break;
        case LOAD_OIL_VOLUME:
            m_fOilVolume = (float)atof(token);
            break;

		case LOAD_SEAT_OFFSET_DIST_X:
			m_fSeatOffsetDistX = (float)atof(token);
			break;
		case LOAD_SEAT_OFFSET_DIST_Y:
			m_fSeatOffsetDistY = (float)atof(token);
			break;
		case LOAD_SEAT_OFFSET_DIST_Z:
			m_fSeatOffsetDistZ = (float)atof(token);
			break;

		case LOAD_CASH_VALUE:
			m_nMonetaryValue = atoi(token);
			break;

		case LOAD_MODEL_FLAGS:
			//m_strModelFlags.SetHashWithString(m_strHandlingFlags.ComputeHash(token), token);
			m_strModelFlags.SetFromString(token);
			break;
		case LOAD_HANDLING_FLAGS:
			//m_strHandlingFlags.SetHashWithString(m_strHandlingFlags.ComputeHash(token), token);
			m_strHandlingFlags.SetFromString(token);
			break;
		case LOAD_CLIP_SET:
			break;

		case LOAD_AI_HANDLING_INFO:
			//m_AIHandling.SetHashWithString(m_strHandlingFlags.ComputeHash(token), token);
			m_AIHandling.SetFromString(token);
			break;
		case LOAD_DOOR_DAMAGE_FLAGS:
			//m_strDamageFlags.SetHashWithString(m_strHandlingFlags.ComputeHash(token), token);
			m_strDamageFlags.SetFromString(token);
			break;
		case LOAD_DOWNFORCE_MODIFIER:
			m_fDownforceModifier = (float)atof(token);
			break;

		case LOAD_POPUP_LIGHT_ROTATION:
			m_fPopUpLightRotation = (float)atof(token);
			break;

		case LOAD_ROCKET_BOOST_CAPACITY:
			m_fRocketBoostCapacity = (float)atof(token);
			break;

		default:
			Assertf(false, "Unknown vehicle handling data type");
			break;
		}			

		token = strtok( NULL, seps );	// get next token
		count++;
	} while (token != NULL);
}
#endif //VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META

void CHandlingData::ConvertToGameUnits(bool UNUSED_PARAM(bMetaData))
{
	m_pAIHandlingInfo = CAIHandlingInfoMgr::GetAIHandlingInfoByHash(m_AIHandling);
	Assertf(m_pAIHandlingInfo, "Unknown vehicle AI handling type (%s)", m_handlingName.TryGetCStr());

	if( !Verifyf(m_pAIHandlingInfo, "No vehicle AI handling data type (%s)", m_handlingName.TryGetCStr()) )
	{
		atHashString hash("AVERAGE",0xBE32B83A);
		m_pAIHandlingInfo = CAIHandlingInfoMgr::GetAIHandlingInfoByHash(hash);
		Assertf(m_pAIHandlingInfo, "AVERAGE handling info missing!");
	}

	// store front and rear bias separately
	Assert(m_fDriveBiasFront >= 0.0f && m_fDriveBiasFront <= 1.0f);
	if(m_fDriveBiasFront < 0.1f)
	{
		m_fDriveBiasFront = 0.0f;
		m_fDriveBiasRear = 1.0f;
	}
	else if(m_fDriveBiasFront > 0.9f)
	{
		m_fDriveBiasFront = 1.0f;
		m_fDriveBiasRear = 0.0f;
	}
	else
	{
		// rear is the balance of the front
		m_fDriveBiasRear = 1.0f - m_fDriveBiasFront;
		m_fDriveBiasRear *= 2.0f;
		m_fDriveBiasFront *= 2.0f;
	}

	// store front and rear brake bias separately
	Assert(m_fBrakeBiasFront >= 0.0f && m_fBrakeBiasFront <= 1.0f);
	// rear is the balance of the front
	m_fBrakeBiasRear = 1.0f - m_fBrakeBiasFront;

	m_fBrakeBiasFront *= 2.0f;
	m_fBrakeBiasRear *= 2.0f;

	Assert(m_fTractionBiasFront >= 0.0f && m_fTractionBiasFront <= 1.0f);
	m_fTractionBiasRear = 1.0f - m_fTractionBiasFront;

	m_fTractionBiasFront *= 2.0f;
	m_fTractionBiasRear *= 2.0f;

	Assert(m_fSuspensionBiasFront >= 0.0f && m_fSuspensionBiasFront <= 1.0f);
	m_fSuspensionBiasRear = 1.0f - m_fSuspensionBiasFront;
						
	m_fSuspensionBiasFront *= 2.0f;
	m_fSuspensionBiasRear *= 2.0f;

	Assert(m_fAntiRollBarBiasFront >= 0.0f && m_fAntiRollBarBiasFront <= 1.0f);
	m_fAntiRollBarBiasRear = 1.0f - m_fAntiRollBarBiasFront;

	m_fAntiRollBarBiasFront *= 2.0f;
	m_fAntiRollBarBiasRear *= 2.0f;

	sscanf(m_strModelFlags.TryGetCStr(), "%x", &mFlags);
	sscanf(m_strHandlingFlags.TryGetCStr(), "%x", &hFlags);
	sscanf(m_strDamageFlags.TryGetCStr(), "%x", &dFlags);

	// calc buoyancy constant which will keep boat nPercentSubmerged at equilibrium
	m_fBuoyancyConstant = 100.0f / m_fPercentSubmerged;

	// need to scale collision damage multiplier by mass so that heavier cars
	//m_fCollisionDamageMultiplier = m_fCollisionDamageMultiplier*2000.0f/m_fMass;

	// some of the traction stuff is entered in degrees - need it in radians
	m_fTractionCurveLateral = ( DtoR * m_fTractionCurveLateral);
	m_fTractionCurveLateral_Inv = 1.0f / m_fTractionCurveLateral;

//	m_fTractionCurveLongitudinal = ( DtoR * m_fTractionCurveLongitudinal);
//	if(m_fTractionSpringDeltaMax > 0.0f)
//		m_fTractionSpringForceMult /= m_fTractionSpringDeltaMax;
	m_fTractionSpringDeltaMax_Inv = 1.0f / m_fTractionSpringDeltaMax;

	if(m_fTractionCurveMax > 0.0f)
	{
		m_fTractionCurveMax_Inv = 1.0f / m_fTractionCurveMax;
	}
	else
	{
		Warningf("%s has 0 (or negative) traction curve, traction cuver max %3.2f", m_handlingName.TryGetCStr(), m_fTractionCurveMax);
		m_fTractionCurveMax_Inv = LARGE_FLOAT;
	}
	if(m_fTractionCurveMax > m_fTractionCurveMin)
	{
		m_fTractionCurveMaxMinusMin_Inv = 1.0f / (m_fTractionCurveMax - m_fTractionCurveMin);
	}
	else
	{
		Warningf("%s has invalid traction curve max %3.2f, min %3.2f", m_handlingName.TryGetCStr(), m_fTractionCurveMax, m_fTractionCurveMin);
		m_fTractionCurveMaxMinusMin_Inv = LARGE_FLOAT;
	}

	static float sfDampingConversion = 0.1f;
	m_fSuspensionCompDamp *= sfDampingConversion;
	m_fSuspensionReboundDamp *= sfDampingConversion;

	static float sfDragConversion = 0.0001f;
	m_fInitialDragCoeff *= sfDragConversion;

	// do drive and gear ratio conversion stuff
	m_fInitialDriveMaxFlatVel *= (1000.0f/3600.0f);
	m_fInitialDriveMaxVel = m_fInitialDriveMaxFlatVel * 1.2f; // add 20% to velocity used for gearing

	Assertf(m_nInitialDriveGears <= MAX_NUM_GEARS, "%s has too many gears", m_handlingName.TryGetCStr());
	if(m_nInitialDriveGears > MAX_NUM_GEARS)
		m_nInitialDriveGears = MAX_NUM_GEARS;

	m_fSteeringLock = ( DtoR * m_fSteeringLock);
	m_fSteeringLockInv = 1.0f / m_fSteeringLock;

}

#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
void CHandlingData::SetHandlingData(CBaseSubHandlingData* pData, bool bMeta)
{
	for(int i = 0; i<MAX_HANDLING_TYPES_PER_VEHICLE; i++)
	{
		if(bMeta)
		{
			if(!m_SubHandlingData[i])
			{
				m_SubHandlingData[i] = pData;
				return;
			}
		}
		else
		{
			if(!m_OldSubHandlingData[i])
			{
				m_OldSubHandlingData[i] = pData;
				return;
			}
		}
	}
	Errorf("Ran out of handling types in handling definition for: '%s'", m_handlingName.TryGetCStr());
}
#endif

CHandlingData::~CHandlingData()
{
	ClearHandlingData();
}

void CHandlingData::ClearHandlingData()
{
#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
	for(int i = 0; i < MAX_HANDLING_TYPES_PER_VEHICLE; i++)
	{
		delete m_OldSubHandlingData[i];
		m_OldSubHandlingData[i] = NULL;
	}
#endif

	for(int i = 0; i < m_SubHandlingData.GetCount(); i++)
	{
		delete m_SubHandlingData[i];
		m_SubHandlingData[i] = NULL;
	}

	m_SubHandlingData.Reset();

	delete m_pBoardingPoints;
	m_pBoardingPoints = NULL;
}

u32 GetSizeOfLargestHandlingClass()
{
	size_t maxSize = 0;

	maxSize = Max(sizeof(CHandlingData), maxSize);
	maxSize = Max(sizeof(CBikeHandlingData), maxSize);
	maxSize = Max(sizeof(CFlyingHandlingData), maxSize);
	maxSize = Max(sizeof(CBoatHandlingData), maxSize);
	maxSize = Max(sizeof(CTrainHandlingData), maxSize);
	maxSize = Max(sizeof(CVehicleWeaponHandlingData), maxSize);
	maxSize = Max(sizeof(CSubmarineHandlingData), maxSize);
	maxSize = Max(sizeof(CTrailerHandlingData), maxSize); 
	maxSize = Max(sizeof(CCarHandlingData), maxSize); 
	maxSize = Max(sizeof(CSpecialFlightHandlingData), maxSize); 

	return (u32) maxSize;
}

static const int kSizeOfLargestHandlingClass = GetSizeOfLargestHandlingClass();

FW_INSTANTIATE_BASECLASS_POOL(CHandlingObject, NUM_HANDLING_DATA_PREALLOCATIONS, atHashString("CHandlingObject",0x8b11dc9f), kSizeOfLargestHandlingClass);


// create static stuff
CHandlingDataMgr CHandlingDataMgr::ms_Instance;

#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
CHandlingDataMgr CHandlingDataMgr::ms_Instance_DLC;
bool CHandlingDataMgr::ms_bParsingDLC = false;
atArray<CHandlingData*>	CHandlingDataMgr::ms_aHandlingData;
#endif

// Setup handling data
//
void CHandlingDataMgr::Init()
{
	Assertf(GetInstance()->m_HandlingData.GetCount()==0,"Double initialisation?");
#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
	GetInstance()->m_HandlingData.Reset();
	ms_aHandlingData.Reset();
	ms_aHandlingData.Reserve(NUM_HANDLING_DATA_PREALLOCATIONS);
#endif
	CDataFileMount::RegisterMountInterface(CDataFileMgr::HANDLING_FILE, &g_VehicleHandlingFileMounter);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::VEHICLEEXTRAS_FILE, &g_VehicleExtrasFileMounter);
}

#if __BANK
void CHandlingDataMgr::ReloadHandlingData()
{
	Reset();
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::HANDLING_FILE);
	if(DATAFILEMGR.IsValid(pData))
	{
		if (!pData->m_disabled)
		{
			LoadHandlingMetaData(pData->m_filename);
		}
	}
	pData = DATAFILEMGR.GetNextFile(pData);
	while(DATAFILEMGR.IsValid(pData))
	{
		if (!pData->m_disabled)
		{
			AppendHandlingMetaData(pData->m_filename);
		}
		pData = DATAFILEMGR.GetNextFile(pData);
	}
}
#endif // __BANK

void CHandlingDataMgr::LoadHandlingData(bool bMetaData)
{
	// Iterate through the files backwards, so episodic data can override any old data
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::HANDLING_FILE);

	while(DATAFILEMGR.IsValid(pData))
	{
		if (!pData->m_disabled)
		{
			if(bMetaData)
			{
				LoadHandlingMetaData(pData->m_filename);
			}
			else
			{
#if VERIFY_OLD_HANDLING_DAT
				LoadOldHandlingData("common:/data/handling.dat", false);
#endif
#if CONVERT_HANDLING_DAT_TO_META
				LoadOldHandlingData("common:/data/handling.dat", true);
#endif
			}
		}
		pData = DATAFILEMGR.GetPrevFile(pData);
	}
}

#if CONVERT_HANDLING_DAT_TO_META
void CHandlingDataMgr::SaveHandlingData()
{
	Verifyf(PARSER.SaveObject("common:/data/handling", "meta", &ms_Instance, parManager::XML), "Failed to save vehicle handling data");
}
#endif


CHandlingDataMgr* CHandlingDataMgr::GetInstance()
{
#if CONVERT_HANDLING_DAT_TO_META
	return ms_bParsingDLC? &ms_Instance_DLC :&ms_Instance;
#else
	return &ms_Instance;
#endif
}

void CHandlingDataMgr::Init(unsigned initMode)
{
	if(initMode == INIT_BEFORE_MAP_LOADED)
	{
		CAIHandlingInfoMgr::Init(initMode);
		CHandlingData::InitPool( MEMBUCKET_GAMEPLAY );
#if CONVERT_HANDLING_DAT_TO_META
		LoadHandlingData(false);
		SaveHandlingData();
		ConvertToGameUnits(true); // Save the raw handling data, then convert data to game units
#else
		LoadHandlingData();
#endif
	}
	else if(initMode == INIT_AFTER_MAP_LOADED)
    {
#if VERIFY_OLD_HANDLING_DAT
		LoadHandlingData(false);
		VerifyHandlingData();
#endif //VERIFY_OLD_HANDLING_DAT
		
        LoadVehicleExtrasFiles();
		CModelInfo::UpdateVehicleClassInfos();
	}
}

void CHandlingDataMgr::Reset()
{
	for(s32 i=0; i<GetInstance()->m_HandlingData.GetCount(); i++)
	{
		atArray<CBaseSubHandlingData*> &subArray = GetInstance()->m_HandlingData[i]->m_SubHandlingData;
		for(s32 j= 0; j<subArray.GetCount();j++)
		{
			delete subArray[j];
		}
		subArray.Reset();
		delete GetInstance()->m_HandlingData[i];
	}
	GetInstance()->m_HandlingData.Reset();
}

void CHandlingDataMgr::Shutdown( unsigned shutdownMode )
{
	if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
	{
		CAIHandlingInfoMgr::Shutdown(shutdownMode);

		Reset();

#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
		for(s32 i=0; i<ms_aHandlingData.GetCount(); i++)
		{
			delete ms_aHandlingData[i];
		}
		ms_aHandlingData.Reset();
#endif

		// Now the pools shouldn't be referenced anywhere, so shut them down
		CHandlingData::ShutdownPool();
	}
}

void CHandlingDataMgr::ConvertToGameUnits(bool bMetaData)
{

	atArray<CHandlingData*> *pHandlingData = pHandlingData = &GetInstance()->m_HandlingData;
#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
	if(!bMetaData)
	{
		pHandlingData = &ms_aHandlingData;
	}
#else
	Assertf(bMetaData, "Should only process handling.meta when not converting nor validating");
#endif
	for(int i=0; i < pHandlingData->GetCount(); i++)
	{
		(*pHandlingData)[i]->ConvertToGameUnits(bMetaData);

		if(bMetaData)
		{
			for(int j=0; j < (*pHandlingData)[i]->m_SubHandlingData.GetCount(); j++)
			{
				if((*pHandlingData)[i]->m_SubHandlingData[j])
				{
					(*pHandlingData)[i]->m_SubHandlingData[j]->ConvertToGameUnits();
				}
			}
		}
#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
		else
		{
			for(int j=0; j < MAX_HANDLING_TYPES_PER_VEHICLE; j++)
			{
				if((*pHandlingData)[i]->m_OldSubHandlingData[j])
				{
					(*pHandlingData)[i]->m_OldSubHandlingData[j]->ConvertToGameUnits();
				}
			}
		}
#endif
	}
}

void CHandlingDataMgr::LoadHandlingMetaData(const char* pHandlingCfg)
{
	FileHandle fid = CFileMgr::OpenFile(pHandlingCfg);
	Assertf( CFileMgr::IsValidFileHandle( fid ), "%s:Cannot find handling file", pHandlingCfg);

	PARSER.LoadObject((fiStream*)fid, ms_Instance);
	CFileMgr::CloseFile(fid);

	ConvertToGameUnits(true);
}

void CHandlingDataMgr::AppendHandlingMetaData(const char* pHandlingCfg)
{
	FileHandle fid = CFileMgr::OpenFile(pHandlingCfg);
	Assertf( CFileMgr::IsValidFileHandle( fid ), "%s:Cannot find handling file", pHandlingCfg);
	CHandlingDataMgr tempMgr;
	if(PARSER.LoadObject((fiStream*)fid, tempMgr))
	{
		for(int i=0;i<tempMgr.m_HandlingData.GetCount();i++)
		{
			CHandlingData* handlingObj = tempMgr.m_HandlingData[i];
			if(!GetInstance()->GetHandlingDataByName(handlingObj->m_handlingName))
			{
				handlingObj->ConvertToGameUnits(true);
				for(int j=0;j<handlingObj->m_SubHandlingData.GetCount();j++)
				{
					CBaseSubHandlingData* subHandling = handlingObj->m_SubHandlingData[j];
					if(subHandling)
					{	
						subHandling->ConvertToGameUnits();
					}
				}
				GetInstance()->m_HandlingData.PushAndGrow(tempMgr.m_HandlingData[i]);
			}
			else
			{
				// if the handling data already exist, free up the memory from tempMgr's list.
				atArray<CBaseSubHandlingData*> &subArray = tempMgr.m_HandlingData[i]->m_SubHandlingData;
				for(s32 j= 0; j<subArray.GetCount();j++)
				{
					delete subArray[j];
					subArray[j] = NULL;
				}
				subArray.Reset();
				delete tempMgr.m_HandlingData[i];
			}
		}
	}
	CFileMgr::CloseFile(fid);

	//ConvertToGameUnits(true);
}
#if VERIFY_OLD_HANDLING_DAT
#define HANDLING_ERROR_MARGIN 1.0e-5f
void CHandlingDataMgr::VerifyHandlingData()
{
	const char *pHandlingName;
	for(int i=0; i < ms_aHandlingData.GetCount(); i++)
	{
		CHandlingData *pOldData = ms_aHandlingData[i];
		CHandlingData *pNewData = GetHandlingDataByName(pOldData->m_handlingName);
		pHandlingName = pNewData->m_handlingName.TryGetCStr();
		Displayf("old data: %s new data: %s", pOldData->m_handlingName.GetCStr(), pNewData->m_handlingName.GetCStr());
		Assert(Abs(pNewData->m_fMass - pOldData->m_fMass) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fInitialDragCoeff - pOldData->m_fInitialDragCoeff) < HANDLING_ERROR_MARGIN);
		Assert(IsTrueXYZ(IsEqual(pNewData->m_vecCentreOfMassOffset,pOldData->m_vecCentreOfMassOffset)));
		Assert(IsTrueXYZ(IsEqual(pNewData->m_vecInertiaMultiplier,pOldData->m_vecInertiaMultiplier)));
		Assert(Abs(pNewData->m_fPercentSubmerged - pOldData->m_fPercentSubmerged) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fBuoyancyConstant - pOldData->m_fBuoyancyConstant) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fDriveBiasFront - pOldData->m_fDriveBiasFront) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fDriveBiasRear - pOldData->m_fDriveBiasRear) < HANDLING_ERROR_MARGIN);
		Assert(pNewData->m_nInitialDriveGears == pOldData->m_nInitialDriveGears);
		Assert(Abs(pNewData->m_fDriveInertia - pOldData->m_fDriveInertia) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fClutchChangeRateScaleUpShift - pOldData->m_fClutchChangeRateScaleUpShift) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fClutchChangeRateScaleDownShift - pOldData->m_fClutchChangeRateScaleDownShift) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fInitialDriveForce - pOldData->m_fInitialDriveForce) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fInitialDriveMaxVel - pOldData->m_fInitialDriveMaxVel) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fInitialDriveMaxFlatVel - pOldData->m_fInitialDriveMaxFlatVel) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fBrakeForce - pOldData->m_fBrakeForce) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fBrakeBiasFront - pOldData->m_fBrakeBiasFront) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fBrakeBiasRear - pOldData->m_fBrakeBiasRear) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fHandBrakeForce - pOldData->m_fHandBrakeForce) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fSteeringLock - pOldData->m_fSteeringLock) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fSteeringLockInv - pOldData->m_fSteeringLockInv) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fTractionCurveMax - pOldData->m_fTractionCurveMax) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fTractionCurveMax_Inv - pOldData->m_fTractionCurveMax_Inv) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fTractionCurveMin - pOldData->m_fTractionCurveMin) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fTractionCurveMaxMinusMin_Inv - pOldData->m_fTractionCurveMaxMinusMin_Inv) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fTractionCurveLateral - pOldData->m_fTractionCurveLateral) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fTractionCurveLateral_Inv - pOldData->m_fTractionCurveLateral_Inv) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fTractionSpringDeltaMax - pOldData->m_fTractionSpringDeltaMax) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fTractionSpringDeltaMax_Inv - pOldData->m_fTractionSpringDeltaMax_Inv) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fLowSpeedTractionLossMult - pOldData->m_fLowSpeedTractionLossMult) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fCamberStiffnesss - pOldData->m_fCamberStiffnesss) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fTractionBiasFront - pOldData->m_fTractionBiasFront) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fTractionBiasRear - pOldData->m_fTractionBiasRear) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fTractionLossMult - pOldData->m_fTractionLossMult) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fSuspensionForce - pOldData->m_fSuspensionForce) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fSuspensionCompDamp - pOldData->m_fSuspensionCompDamp) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fSuspensionReboundDamp - pOldData->m_fSuspensionReboundDamp) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fSuspensionUpperLimit - pOldData->m_fSuspensionUpperLimit) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fSuspensionLowerLimit - pOldData->m_fSuspensionLowerLimit) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fSuspensionRaise - pOldData->m_fSuspensionRaise) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fSuspensionBiasFront - pOldData->m_fSuspensionBiasFront) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fSuspensionBiasRear - pOldData->m_fSuspensionBiasRear) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fAntiRollBarForce - pOldData->m_fAntiRollBarForce) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fAntiRollBarBiasFront - pOldData->m_fAntiRollBarBiasFront) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fAntiRollBarBiasRear - pOldData->m_fAntiRollBarBiasRear) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fRollCentreHeightFront - pOldData->m_fRollCentreHeightFront) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fRollCentreHeightRear - pOldData->m_fRollCentreHeightRear) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fCollisionDamageMult - pOldData->m_fCollisionDamageMult) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fWeaponDamageMult - pOldData->m_fWeaponDamageMult) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fDeformationDamageMult - pOldData->m_fDeformationDamageMult) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fEngineDamageMult - pOldData->m_fEngineDamageMult) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fPetrolTankVolume - pOldData->m_fPetrolTankVolume) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fOilVolume - pOldData->m_fOilVolume) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fSeatOffsetDistX - pOldData->m_fSeatOffsetDistX) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fSeatOffsetDistY - pOldData->m_fSeatOffsetDistY) < HANDLING_ERROR_MARGIN);
		Assert(Abs(pNewData->m_fSeatOffsetDistZ - pOldData->m_fSeatOffsetDistZ) < HANDLING_ERROR_MARGIN);
		Assert(pNewData->m_nMonetaryValue == pOldData->m_nMonetaryValue);
		Assert(pNewData->mFlags == pOldData->mFlags);
		Assert(pNewData->hFlags == pOldData->hFlags);
		Assert(pNewData->dFlags == pOldData->dFlags);
		Assert(Abs(pNewData->m_fEstimatedMaxFlatVel - pOldData->m_fEstimatedMaxFlatVel) < HANDLING_ERROR_MARGIN);

		for(int j=0; j < MAX_HANDLING_TYPES_PER_VEHICLE; j++)
		{
			if(j >= pNewData->m_SubHandlingData.GetCount())
			{
				Assert(j == 0 || !pOldData->m_OldSubHandlingData[j]);
				break;
			}
			CBaseSubHandlingData *pOldBaseSubData = pOldData->m_OldSubHandlingData[j];
			CBaseSubHandlingData *pNewBaseSubData = pNewData->m_SubHandlingData[j];
			if(pOldBaseSubData)
			{
				Assert(pNewBaseSubData);
				Assert(pOldBaseSubData->GetHandlingType() == pNewBaseSubData->GetHandlingType());

				switch(pOldBaseSubData->GetHandlingType())
				{
					case HANDLING_TYPE_BIKE:
					{
						CBikeHandlingData *pOldSubData = static_cast<CBikeHandlingData*>(pOldBaseSubData);
						CBikeHandlingData *pNewSubData = static_cast<CBikeHandlingData*>(pNewBaseSubData);

						Assert(Abs(pNewSubData->m_fLeanFwdCOMMult - pOldSubData->m_fLeanFwdCOMMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fLeanFwdForceMult - pOldSubData->m_fLeanFwdForceMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fLeanBakCOMMult - pOldSubData->m_fLeanBakCOMMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fLeanBakForceMult - pOldSubData->m_fLeanBakForceMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fMaxBankAngle - pOldSubData->m_fMaxBankAngle) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fFullAnimAngle - pOldSubData->m_fFullAnimAngle) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fFullAnimAngleInv - pOldSubData->m_fFullAnimAngleInv) < HANDLING_ERROR_MARGIN);


						Assert(Abs(pNewSubData->m_fDesLeanReturnFrac - pOldSubData->m_fDesLeanReturnFrac) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fStickLeanMult - pOldSubData->m_fStickLeanMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fBrakingStabilityMult - pOldSubData->m_fBrakingStabilityMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fInAirSteerMult - pOldSubData->m_fInAirSteerMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fWheelieBalancePoint - pOldSubData->m_fWheelieBalancePoint) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fStoppieBalancePoint - pOldSubData->m_fStoppieBalancePoint) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fWheelieSteerMult - pOldSubData->m_fWheelieSteerMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fRearBalanceMult - pOldSubData->m_fRearBalanceMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fFrontBalanceMult - pOldSubData->m_fFrontBalanceMult) < HANDLING_ERROR_MARGIN);

						Assert(Abs(pNewSubData->m_fBikeGroundSideFrictionMult - pOldSubData->m_fBikeGroundSideFrictionMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fBikeWheelGroundSideFrictionMult - pOldSubData->m_fBikeWheelGroundSideFrictionMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fBikeOnStandLeanAngle - pOldSubData->m_fBikeOnStandLeanAngle) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fBikeOnStandSteerAngle - pOldSubData->m_fBikeOnStandSteerAngle) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fJumpForce - pOldSubData->m_fJumpForce) < HANDLING_ERROR_MARGIN);
						break;
					}
					case HANDLING_TYPE_FLYING:
					{
						CFlyingHandlingData *pOldSubData = static_cast<CFlyingHandlingData*>(pOldBaseSubData);
						CFlyingHandlingData *pNewSubData = static_cast<CFlyingHandlingData*>(pNewBaseSubData);

						Assert(Abs(pNewSubData->m_fThrust - pOldSubData->m_fThrust) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fThrustFallOff - pOldSubData->m_fThrustFallOff) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fThrustVectoring - pOldSubData->m_fThrustVectoring) < HANDLING_ERROR_MARGIN);

						Assert(Abs(pNewSubData->m_fYawMult - pOldSubData->m_fYawMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fYawStabilise - pOldSubData->m_fYawStabilise) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fSideSlipMult - pOldSubData->m_fSideSlipMult) < HANDLING_ERROR_MARGIN);

						Assert(Abs(pNewSubData->m_fRollMult - pOldSubData->m_fRollMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fRollStabilise - pOldSubData->m_fRollStabilise) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fPitchMult - pOldSubData->m_fPitchMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fPitchStabilise - pOldSubData->m_fPitchStabilise) < HANDLING_ERROR_MARGIN);

						Assert(Abs(pNewSubData->m_fFormLiftMult - pOldSubData->m_fFormLiftMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fAttackLiftMult - pOldSubData->m_fAttackLiftMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fAttackDiveMult - pOldSubData->m_fAttackDiveMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fGearDownDragV - pOldSubData->m_fGearDownDragV) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fGearDownLiftMult - pOldSubData->m_fGearDownLiftMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fWindMult - pOldSubData->m_fWindMult) < HANDLING_ERROR_MARGIN);

						Assert(Abs(pNewSubData->m_fMoveRes - pOldSubData->m_fMoveRes) < HANDLING_ERROR_MARGIN);
						Assert(IsTrueXYZ(IsEqual(pNewSubData->m_vecTurnRes, pOldSubData->m_vecTurnRes)));
						Assert(IsTrueXYZ(IsEqual(pNewSubData->m_vecSpeedRes, pOldSubData->m_vecSpeedRes)));

						Assert(Abs(pNewSubData->m_fGearDoorFrontOpen - pOldSubData->m_fGearDoorFrontOpen) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fGearDoorRearOpen - pOldSubData->m_fGearDoorRearOpen) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fGearDoorRearOpen2 - pOldSubData->m_fGearDoorRearOpen2) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fGearDoorRearMOpen - pOldSubData->m_fGearDoorRearMOpen) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fTurublenceMagnitudeMax - pOldSubData->m_fTurublenceMagnitudeMax) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fTurublenceForceMulti - pOldSubData->m_fTurublenceForceMulti) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fTurublenceRollTorqueMulti - pOldSubData->m_fTurublenceRollTorqueMulti) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fTurublencePitchTorqueMulti - pOldSubData->m_fTurublencePitchTorqueMulti) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fBodyDamageControlEffectMult - pOldSubData->m_fBodyDamageControlEffectMult) < HANDLING_ERROR_MARGIN);


						Assert(Abs(pNewSubData->m_fInputSensitivityForDifficulty - pOldSubData->m_fInputSensitivityForDifficulty) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fOnGroundYawBoostSpeedPeak - pOldSubData->m_fOnGroundYawBoostSpeedPeak) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fOnGroundYawBoostSpeedCap - pOldSubData->m_fOnGroundYawBoostSpeedCap) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fEngineOffGlideMulti - pOldSubData->m_fEngineOffGlideMulti) < HANDLING_ERROR_MARGIN);
						break;
					}
					case HANDLING_TYPE_BOAT:
					{
						CBoatHandlingData *pOldSubData = static_cast<CBoatHandlingData*>(pOldBaseSubData);
						CBoatHandlingData *pNewSubData = static_cast<CBoatHandlingData*>(pNewBaseSubData);

						Assert(Abs(pNewSubData->m_fBoxFrontMult - pOldSubData->m_fBoxFrontMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fBoxRearMult - pOldSubData->m_fBoxRearMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fBoxSideMult - pOldSubData->m_fBoxSideMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fSampleTop - pOldSubData->m_fSampleTop) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fSampleBottom - pOldSubData->m_fSampleBottom) < HANDLING_ERROR_MARGIN);

						Assert(Abs(pNewSubData->m_fAquaplaneForce - pOldSubData->m_fAquaplaneForce) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fAquaplanePushWaterMult - pOldSubData->m_fAquaplanePushWaterMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fAquaplanePushWaterCap - pOldSubData->m_fAquaplanePushWaterCap) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fAquaplanePushWaterApply - pOldSubData->m_fAquaplanePushWaterApply) < HANDLING_ERROR_MARGIN);

						Assert(Abs(pNewSubData->m_fRudderForce - pOldSubData->m_fRudderForce) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fRudderOffsetSubmerge - pOldSubData->m_fRudderOffsetSubmerge) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fRudderOffsetForce - pOldSubData->m_fRudderOffsetForce) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fRudderOffsetForceZMult - pOldSubData->m_fRudderOffsetForceZMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fWaveAudioMult - pOldSubData->m_fWaveAudioMult) < HANDLING_ERROR_MARGIN);

						Assert(IsTrueXYZ(IsEqual(pNewSubData->m_vecMoveResistance,pOldSubData->m_vecMoveResistance)));
						Assert(IsTrueXYZ(IsEqual(pNewSubData->m_vecTurnResistance,pOldSubData->m_vecTurnResistance)));

						Assert(Abs(pNewSubData->m_fLook_L_R_CamHeight - pOldSubData->m_fLook_L_R_CamHeight) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fDragCoefficient - pOldSubData->m_fDragCoefficient) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fKeelSphereSize - pOldSubData->m_fKeelSphereSize) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fPropRadius - pOldSubData->m_fPropRadius) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fLowLodAngOffset - pOldSubData->m_fLowLodAngOffset) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fLowLodDraughtOffset - pOldSubData->m_fLowLodDraughtOffset) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fImpellerOffset - pOldSubData->m_fImpellerOffset) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fImpellerForceMult - pOldSubData->m_fImpellerForceMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fDinghySphereBuoyConst - pOldSubData->m_fDinghySphereBuoyConst) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fProwRaiseMult - pOldSubData->m_fProwRaiseMult) < HANDLING_ERROR_MARGIN);

						break;
					}
					case HANDLING_TYPE_WEAPON:
					{
						CVehicleWeaponHandlingData *pOldSubData = static_cast<CVehicleWeaponHandlingData*>(pOldBaseSubData);
						CVehicleWeaponHandlingData *pNewSubData = static_cast<CVehicleWeaponHandlingData*>(pNewBaseSubData);

						for(int i=0;i<MAX_NUM_VEHICLE_WEAPONS; i++)
						{
							Assert(pNewSubData->m_uWeaponHash[i] == pOldSubData->m_uWeaponHash[i]);
							Assert(pNewSubData->m_WeaponSeats[i] == pOldSubData->m_WeaponSeats[i]);
						}

						for(int i=0;i<MAX_NUM_VEHICLE_TURRETS; i++)
						{
							Assert(Abs(pNewSubData->m_fTurretSpeed[i] - pOldSubData->m_fTurretSpeed[i]) < HANDLING_ERROR_MARGIN);
							Assert(Abs(pNewSubData->m_fTurretPitchMin[i] - pOldSubData->m_fTurretPitchMin[i]) < HANDLING_ERROR_MARGIN);
							Assert(Abs(pNewSubData->m_fTurretPitchMax[i] - pOldSubData->m_fTurretPitchMax[i]) < HANDLING_ERROR_MARGIN);
							Assert(Abs(pNewSubData->m_fTurretCamPitchMin[i] - pOldSubData->m_fTurretCamPitchMin[i]) < HANDLING_ERROR_MARGIN);
							Assert(Abs(pNewSubData->m_fTurretCamPitchMax[i] - pOldSubData->m_fTurretCamPitchMax[i]) < HANDLING_ERROR_MARGIN);
							Assert(Abs(pNewSubData->m_fBulletVelocityForGravity[i] - pOldSubData->m_fBulletVelocityForGravity[i]) < HANDLING_ERROR_MARGIN);
							Assert(Abs(pNewSubData->m_fTurretPitchForwardMin[i] - pOldSubData->m_fTurretPitchForwardMin[i]) < HANDLING_ERROR_MARGIN);
						}

						Assert(Abs(pNewSubData->m_fUvAnimationMult - pOldSubData->m_fUvAnimationMult) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fMiscGadgetVar - pOldSubData->m_fMiscGadgetVar) < HANDLING_ERROR_MARGIN);
						Assert(Abs(pNewSubData->m_fWheelImpactOffset - pOldSubData->m_fWheelImpactOffset) < HANDLING_ERROR_MARGIN);

						break;
					}
					default:
						break;
				}
			}
		}
	}

}
#endif //VERIFY_OLD_HANDLING_DAT

#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
// loads handling config file into memory
//
void CHandlingDataMgr::LoadOldHandlingData(const char* pHandlingCfg, bool bMetaData)
{
	FileHandle fid;

	fid = CFileMgr::OpenFile(pHandlingCfg);
	Assertf( CFileMgr::IsValidFileHandle( fid ), "%s:Cannot find handling file", pHandlingCfg);
	
	char* pLine;
	
	atArray<CHandlingData*> *pHandlingData = &ms_aHandlingData;
	if(bMetaData)
	{
		pHandlingData = &GetInstance()->m_HandlingData;
	}
	// read a line at a time putting values into the handling data structure
	while((pLine = CFileMgr::ReadLine(fid)) != NULL)
	{
		char handlingName[HANDLING_NAME_LENGTH];
		s32 handlingIndex;
		char temp, temp1;

		// check for end of file - don't yet know how to check the file length (TBD)
		if (!strcmp(pLine, ";the end"))
			break;
		// ignore comments
		if (pLine[0] == ';')
			continue;
		if (pLine[0] == '#')
			continue;

		// parse line of file which contains data
		
		// if starts '!' then load as bike handling data
		if(pLine[0] == '!')
		{
			sscanf(pLine, "%c %s", &temp, handlingName);
			handlingIndex = RegisterBikeHandlingData(handlingName, bMetaData);

			if(handlingIndex != -1)
			{
				(*pHandlingData)[handlingIndex]->GetHandlingData(HANDLING_TYPE_BIKE, bMetaData)->Load(pLine);
			}
		}
		// if if line begins '$' load as flying handling data
		else if(pLine[0] == '$')
		{
            if(pLine[1] == 'V')
            {
                sscanf(pLine, "%c%c %s", &temp, &temp1, handlingName);
                handlingIndex = RegisterVerticalFlyingHandlingData(handlingName, bMetaData);

                if(handlingIndex != -1)
                {
                    (*pHandlingData)[handlingIndex]->GetHandlingData(HANDLING_TYPE_VERTICAL_FLYING, bMetaData)->Load(pLine);
                }
            }
            else
            {
                sscanf(pLine, "%c %s", &temp, handlingName);
                handlingIndex = RegisterFlyingHandlingData(handlingName, bMetaData);

                if(handlingIndex != -1)
                {
                    (*pHandlingData)[handlingIndex]->GetHandlingData(HANDLING_TYPE_FLYING, bMetaData)->Load(pLine);
                }
            }

		}
		// if if line begins '%' load as boat handling data
		else if(pLine[0] == '%')
		{
			sscanf(pLine, "%c %s", &temp, handlingName);
			handlingIndex = RegisterBoatHandlingData(handlingName, bMetaData);

			if(handlingIndex != -1)
			{
				(*pHandlingData)[handlingIndex]->GetHandlingData(HANDLING_TYPE_BOAT, bMetaData)->Load(pLine);
			}
		}
		else if(pLine[0] == '£')
		{
			sscanf(pLine, "%c %s", &temp, handlingName);
			handlingIndex = RegisterWeaponHandlingData(handlingName, bMetaData);

			if(handlingIndex != -1)
			{
				(*pHandlingData)[handlingIndex]->GetHandlingData(HANDLING_TYPE_WEAPON, bMetaData)->Load(pLine);
			}
		}
		else if(pLine[0] == '*')
		{
			sscanf(pLine, "%c %s", &temp, handlingName);
			handlingIndex = RegisterSubmarineHandlingData(handlingName, bMetaData);

			if(handlingIndex != -1)
			{
				(*pHandlingData)[handlingIndex]->GetHandlingData(HANDLING_TYPE_SUBMARINE, bMetaData)->Load(pLine);
			}
		}
		else if(pLine[0] == '&')
		{
			sscanf(pLine, "%c %s", &temp, handlingName);
			handlingIndex = RegisterTrainHandlingData(handlingName, bMetaData);

			if(handlingIndex != -1)
			{
				(*pHandlingData)[handlingIndex]->GetTrainHandlingData(bMetaData)->Load(pLine);
			}
		}
		else if(pLine[0] == '@')
		{
			sscanf(pLine, "%c %s", &temp, handlingName);
			handlingIndex = RegisterTrailerHandlingData(handlingName, bMetaData);

			if(handlingIndex != -1)
			{
				(*pHandlingData)[handlingIndex]->GetHandlingData(HANDLING_TYPE_TRAILER, bMetaData)->Load(pLine);
			}
		}
		else if(pLine[0] == '^')
		{
			sscanf(pLine, "%c %s", &temp, handlingName);
			handlingIndex = RegisterCarHandlingData(handlingName, bMetaData);

			if(handlingIndex != -1)
			{
				(*pHandlingData)[handlingIndex]->GetHandlingData(HANDLING_TYPE_CAR, bMetaData)->Load(pLine);
			}
		}
		// else load as car handling data
		else
		{
			sscanf(pLine, "%s", handlingName);
			handlingIndex = RegisterHandlingData(handlingName, bMetaData);
			(*pHandlingData)[handlingIndex]->Load(pLine);
		}
	}

	CFileMgr::CloseFile(fid);

#if	!CONVERT_HANDLING_DAT_TO_META
	ConvertToGameUnits(bMetaData);
#endif
}

#endif //VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META

void CHandlingDataMgr::LoadVehicleExtrasFiles()
{
	// Iterate through the files backwards, so episodic data can override any old data
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::VEHICLEEXTRAS_FILE);
	while(DATAFILEMGR.IsValid(pData))
	{
		// Read in this particular file

		if(!pData->m_disabled)
			LoadVehicleExtrasFile(pData->m_filename);

		// Get next file
		pData = DATAFILEMGR.GetPrevFile(pData);
	}
}


//	LoadVehicleExtrasFile
//	The "VehicleExtras.dat" file contains extra vehicle-related data.

void CHandlingDataMgr::LoadVehicleExtrasFile(const char* szFileName)
{
	const FileHandle fid = CFileMgr::OpenFile(szFileName);
	Assertf(CFileMgr::IsValidFileHandle(fid), "%s:Cannot open file.", szFileName);
	if(!CFileMgr::IsValidFileHandle(fid))
		return;

	const char * pBoardingPointsToken = "START_BOARDING_POINTS";
	const int iBoardingPointsSize = istrlen(pBoardingPointsToken);

	char * pLine;

	while((pLine = CFileMgr::ReadLine(fid)) != NULL)
	{
		if(!*pLine)
			continue;
		// check for end of file - don't yet know how to check the file length (TBD)
		if (!strcmp(pLine, ";the end"))
			break;
		// ignore comments
		if (pLine[0] == ';')
			continue;

		// Is this the BOARDING_POINTS section ?
		if(!strnicmp(pBoardingPointsToken, pLine, iBoardingPointsSize))
		{
			ReadVehiclesExtraBoardingPoints(fid);
		}
		// Any other sections we recognise?
	}
	CFileMgr::CloseFile(fid);
}

void CHandlingDataMgr::ReadVehiclesExtraBoardingPoints(const FileHandle fid)
{
	const char * pToken = "VEHICLE_NAME";
	int iTokenLength = istrlen(pToken);

	const char * pScanString1 = "VEHICLE_NAME %s NUM_BOARDING_POINTS %i NAV_MAXHEIGHTCHANGE %f NAV_MINZDIST %f NAV_RES %f AUTHORED_MESH %s";
	const char * pScanString2 = "X %f Y %f Z %f HEADING %f";

	const char * pEndToken = "END_BOARDING_POINTS";
	int iEndTokenLength = istrlen(pEndToken);

	char * pLine = NULL;

	while((pLine = CFileMgr::ReadLine(fid)) != NULL)
	{
		if(!*pLine)
			continue;
		// check for end of file - don't yet know how to check the file length (TBD)
		if(!strcmp(pLine, ";the end"))
			break;
		// ignore comments
		if(pLine[0] == ';')
			continue;
		// End of section?
		if(!strnicmp(pLine, pEndToken, iEndTokenLength))
			break;
		// Main token?
		if(!strnicmp(pLine, pToken, iTokenLength))
		{
			char vehicleName[256] = { 0 };
			char authoredMesh[256] = { 0 };
			int iNumBoardingPoints = 0;
			float fNavMaxHeightChange;
			float fNavMinZDist;
			float fNavRes;

			int iNumRead = sscanf(pLine, pScanString1, vehicleName, &iNumBoardingPoints, &fNavMaxHeightChange, &fNavMinZDist, &fNavRes, authoredMesh);
			if( iNumRead>=5 && iNumBoardingPoints > 0)
			{
				CBaseModelInfo * pModelInfo = CModelInfo::GetBaseModelInfoFromName(vehicleName, NULL);
				Assertf(pModelInfo, "Error reading boarding-points : vehicle '%s' not found - has its name changed?", vehicleName);
				if(pModelInfo)
				{
					Assert(pModelInfo->GetModelType()==MI_TYPE_VEHICLE);
					if(pModelInfo && pModelInfo->GetModelType()==MI_TYPE_VEHICLE)
					{
						CVehicleModelInfo * pVehicleModelInfo = (CVehicleModelInfo*)pModelInfo;
						VehicleType nVehicleType = pVehicleModelInfo->GetVehicleType();
						if(Verifyf(nVehicleType==VEHICLE_TYPE_BOAT || nVehicleType==VEHICLE_TYPE_PLANE || nVehicleType==VEHICLE_TYPE_HELI || nVehicleType==VEHICLE_TYPE_TRAIN, "Invalid vehicle type: %d", nVehicleType))
						{
							// Obtain the handling data from this vehicle
							CHandlingData * pHandling = CHandlingDataMgr::GetHandlingDataByName(vehicleName);
							Assertf(pHandling, "Error reading boarding-points : vehicle '%s' not found - has its name changed?", vehicleName);
							if(pHandling)
							{
								//Assert(pHandling->m_pBoardingPoints == NULL);

								if(!pHandling->m_pBoardingPoints)
								{
									pHandling->m_pBoardingPoints = rage_new CBoardingPoints();
									pHandling->m_pBoardingPoints->m_iNumBoardingPoints = iNumBoardingPoints;
#if !__FINAL
									pHandling->m_pBoardingPoints->m_fNavMaxHeightChange = Max(0.35f, fNavMaxHeightChange);
									pHandling->m_pBoardingPoints->m_fNavMinZDist = Max(0.5f, fNavMinZDist);
									pHandling->m_pBoardingPoints->m_fNavRes = fNavRes;

									if(authoredMesh[0])
									{
										pHandling->m_pBoardingPoints->m_pAuthoredMeshName = StringDuplicate(authoredMesh);
									}
#endif
									for(int b=0; b<iNumBoardingPoints; b++)
									{
										pLine = CFileMgr::ReadLine(fid);
										if(!*pLine)
										{
											b--;
											continue;
										}
										iNumRead = sscanf(pLine, pScanString2,
											&pHandling->m_pBoardingPoints->m_BoardingPoints[b].m_fPosition[0],
											&pHandling->m_pBoardingPoints->m_BoardingPoints[b].m_fPosition[1],
											&pHandling->m_pBoardingPoints->m_BoardingPoints[b].m_fPosition[2],
											&pHandling->m_pBoardingPoints->m_BoardingPoints[b].m_fHeading
										);
										Assert(iNumRead==4);
									}
								}
							}
						}
					}
				}
			}
		}
	}
}


//
// name:		RegisterHandlingData
// description:	Register a handling data name, if it is already registered then return index of original version
int CHandlingDataMgr::RegisterHandlingData(atHashString nameHash, bool bMetaData)
{
	int nIndex = -1;
	if(bMetaData)
	{
		nIndex = GetHandlingIndex(nameHash);

#if CONVERT_HANDLING_DAT_TO_META	
		if(nIndex == -1)
		{
			CHandlingData* tmp = rage_new CHandlingData;
			tmp->m_handlingName = nameHash;
			GetInstance()->m_HandlingData.PushAndGrow(tmp);
			nIndex = GetInstance()->m_HandlingData.GetCount()-1;
		}
#endif
	}
	else
	{

#if VERIFY_OLD_HANDLING_DAT
		s32 index = GetHandlingIndexOld(nameHash);

		if(index == -1)
		{
			CHandlingData* tmp = rage_new CHandlingData;
			tmp->m_handlingName = nameHash;
			ms_aHandlingData.PushAndGrow(tmp);
			index = ms_aHandlingData.GetCount()-1;
		}

		return index;
#endif
	}

	return nIndex;
}

#if VERIFY_OLD_HANDLING_DAT || CONVERT_HANDLING_DAT_TO_META
//
// name:		RegisterBikeHandlingData
// description:	Register a bike handling data name, and return the index of the parent handling data it's owned by
int CHandlingDataMgr::RegisterBikeHandlingData(atHashString nameHash,bool bMetaData)
{
	s32 index = -1;
	atArray<CHandlingData*> *pHandlingData = &ms_aHandlingData;
	if(bMetaData)
	{
		index = GetHandlingIndex(nameHash);
		pHandlingData = &GetInstance()->m_HandlingData;
	}
	else
	{
		index = GetHandlingIndexOld(nameHash);
	}

	// find parent handling data with the same name
	if(index != -1)
	{
		// if parent already has bike handling data allocated, we're done
		if((*pHandlingData)[index]->GetBikeHandlingData(bMetaData))
			return index;
		else
		{
			(*pHandlingData)[index]->SetHandlingData(rage_new CBikeHandlingData, bMetaData);
		}
	}
	else
	{
		Assertf(false, "%s: Cant find base handling to match Bike data",nameHash.TryGetCStr());
	}

	return index;
}


// name:		RegisterFlyingHandlingData
// description:	Register a flying handling data name, and return the index of the parent handling data it's owned by
int CHandlingDataMgr::RegisterFlyingHandlingData(atHashString nameHash, bool bMetaData)
{
	s32 index = -1;
	atArray<CHandlingData*> *pHandlingData = &ms_aHandlingData;
	if(bMetaData)
	{
		index = GetHandlingIndex(nameHash);
		pHandlingData = &GetInstance()->m_HandlingData;
	}
	else
	{
		index = GetHandlingIndexOld(nameHash);
	}

	// find parent handling data with the same name
	if(index != -1)
	{
		// if parent already has flying handling data allocated, we're done
		if((*pHandlingData)[index]->GetFlyingHandlingData(bMetaData))
			return index;
		else
		{
			(*pHandlingData)[index]->SetHandlingData(rage_new CFlyingHandlingData(HANDLING_TYPE_FLYING), bMetaData);
		}
	}
	else
	{
		Assertf(false, "%s: Cant find base handling to match flying data",nameHash.TryGetCStr());
	}
	
	return index;
}


// name:		RegisterVerticalFlyingHandlingData
// description:	Register a vertical flying handling data name, and return the index of the parent handling data it's owned by
int CHandlingDataMgr::RegisterVerticalFlyingHandlingData(atHashString nameHash, bool bMetaData)
{
	s32 index = -1;
	atArray<CHandlingData*> *pHandlingData = &ms_aHandlingData;
	if(bMetaData)
	{
		index = GetHandlingIndex(nameHash);
		pHandlingData = &GetInstance()->m_HandlingData;
	}
	else
	{
		index = GetHandlingIndexOld(nameHash);
	}


	// find parent handling data with the same name
	if(index != -1)
	{
		// if parent already has vertical flying handling data allocated, we're done
		if((*pHandlingData)[index]->GetVerticalFlyingHandlingData(bMetaData))
			return index;
		else
		{
			(*pHandlingData)[index]->SetHandlingData(rage_new CFlyingHandlingData(HANDLING_TYPE_VERTICAL_FLYING), bMetaData);
		}
	}
	else
	{
		Assertf(false, "%s: Cant find base handling to match vertical flying data",nameHash.TryGetCStr());
	}

    return index;
}


// name:		RegisterBoatHandlingData
// description:	Register a boat handling data name, and return the index of the parent handling data it's owned by
int CHandlingDataMgr::RegisterBoatHandlingData(atHashString nameHash, bool bMetaData)
{
	s32 index = -1;
	atArray<CHandlingData*> *pHandlingData = &ms_aHandlingData;
	if(bMetaData)
	{
		index = GetHandlingIndex(nameHash);
		pHandlingData = &GetInstance()->m_HandlingData;
	}
	else
	{
		index = GetHandlingIndexOld(nameHash);
	}

	// find parent handling data with the same name
	if(index != -1)
	{
		// if parent already has boat handling data allocated, we're done
		if((*pHandlingData)[index]->GetBoatHandlingData(bMetaData))
			return index;
		else
		{
			(*pHandlingData)[index]->SetHandlingData(rage_new CBoatHandlingData, bMetaData);
		}
	}
	else
	{
		Assertf(false, "%s: Cant find base handling to match Boat data", nameHash.TryGetCStr());
	}

	return index;
}

// name:		registerWeaponHandlingData
// description:	Register a Weapon handling data name, and return the index of the parent handling data it's owned by
int CHandlingDataMgr::RegisterWeaponHandlingData(atHashString nameHash, bool bMetaData)
{
	s32 index = -1;
	atArray<CHandlingData*> *pHandlingData = &ms_aHandlingData;
	if(bMetaData)
	{
		index = GetHandlingIndex(nameHash);
		pHandlingData = &GetInstance()->m_HandlingData;
	}
	else
	{
		index = GetHandlingIndexOld(nameHash);
	}

	// find parent handling data with the same name
	if(index != -1)
	{
		// if parent already has weapon handling data allocated, we're done
		if((*pHandlingData)[index]->GetWeaponHandlingData(bMetaData))
			return index;
		else
		{
			(*pHandlingData)[index]->SetHandlingData(rage_new CVehicleWeaponHandlingData, bMetaData);
		}
	}
	else
	{
		Assertf(false, "%s: Cant find base handling to match Weapon data", nameHash.TryGetCStr());
	}

	return index;
}

// name:		RegisterSubmarineHandlingData
// description:	Register a submarine handling data name, and return the index of the parent handling data it's owned by
int CHandlingDataMgr::RegisterSubmarineHandlingData(atHashString nameHash, bool bMetaData)
{
	s32 index = -1;
	atArray<CHandlingData*> *pHandlingData = &ms_aHandlingData;
	if(bMetaData)
	{
		index = GetHandlingIndex(nameHash);
		pHandlingData = &GetInstance()->m_HandlingData;
	}
	else
	{
		index = GetHandlingIndexOld(nameHash);
	}

	// find parent handling data with the same name
	if(index != -1)
	{
		// if parent already has weapon handling data allocated, we're done
		if((*pHandlingData)[index]->GetSubmarineHandlingData(bMetaData))
			return index;
		else
		{
			(*pHandlingData)[index]->SetHandlingData(rage_new CSubmarineHandlingData, bMetaData);
		}
	}
	else
	{
		Assertf(false, "%s: Cant find base handling to match submarine data", nameHash.TryGetCStr());
	}

	return index;
}

// name:		RegisterTrainHandlingData
// description:	Register a train handling data name, and return the index of the parent handling data it's owned by
int CHandlingDataMgr::RegisterTrainHandlingData(atHashString nameHash, bool bMetaData)
{
	s32 index = -1;
	atArray<CHandlingData*> *pHandlingData = &ms_aHandlingData;
	if(bMetaData)
	{
		index = GetHandlingIndex(nameHash);
		pHandlingData = &GetInstance()->m_HandlingData;
	}
	else
	{
		index = GetHandlingIndexOld(nameHash);
	}

	// find parent handling data with the same name
	if(index != -1)
	{
		// if parent already has handling data allocated, we're done
		if((*pHandlingData)[index]->GetTrainHandlingData(bMetaData))
			return index;
		else
		{
			(*pHandlingData)[index]->SetHandlingData(rage_new CTrainHandlingData, bMetaData);
		}
	}
	else
	{
		Assertf(false, "%s: Cant find base handling to match train data", nameHash.TryGetCStr());
	}

	return index;
}

// name:		RegisterTrailerHandlingData
// description:	Register a trailer handling data name, and return the index of the parent handling data it's owned by
int CHandlingDataMgr::RegisterTrailerHandlingData(atHashString nameHash, bool bMetaData)
{
	s32 index = -1;
	atArray<CHandlingData*> *pHandlingData = &ms_aHandlingData;
	if(bMetaData)
	{
		index = GetHandlingIndex(nameHash);
		pHandlingData = &GetInstance()->m_HandlingData;
	}
	else
	{
		index = GetHandlingIndexOld(nameHash);
	}

	// find parent handling data with the same name
	if(index != -1)
	{
		// if parent already has handling data allocated, we're done
		if((*pHandlingData)[index]->GetTrailerHandlingData(bMetaData))
			return index;
		else
		{
			(*pHandlingData)[index]->SetHandlingData(rage_new CTrailerHandlingData, bMetaData);
		}
	}
	else
	{
		Assertf(false, "%s: Cant find base handling to match trailer data", nameHash.TryGetCStr());
	}

	return index;
}

// name:		RegisterCarHandlingData
// description:	Register a car handling data name, and return the index of the parent handling data it's owned by
int CHandlingDataMgr::RegisterCarHandlingData(atHashString nameHash, bool bMetaData)
{
	s32 index = -1;
	atArray<CHandlingData*> *pHandlingData = &ms_aHandlingData;
	if(bMetaData)
	{
		index = GetHandlingIndex(nameHash);
		pHandlingData = &GetInstance()->m_HandlingData;
	}
	else
	{
		index = GetHandlingIndexOld(nameHash);
	}

	// find parent handling data with the same name
	if(index != -1)
	{
		// if parent already has handling data allocated, we're done
		if((*pHandlingData)[index]->GetCarHandlingData(bMetaData))
			return index;
		else
		{
			(*pHandlingData)[index]->SetHandlingData(rage_new CCarHandlingData, bMetaData);
		}
	}
	else
	{
		Assertf(false, "%s: Cant find base handling to match car data", nameHash.TryGetCStr());
	}

	return index;
}


//
// Return the index of the handling data structure from a name
//
int CHandlingDataMgr::GetHandlingIndexOld(atHashString nameHash)
{
	for(int i=0; i<ms_aHandlingData.GetCount(); i++)
	{
		if(nameHash == ms_aHandlingData[i]->m_handlingName)
			return i;		
	}

	return -1;
}
#endif //VERIFY_OLD_HANDLING_DAT

int CHandlingDataMgr::GetHandlingIndex(atHashString nameHash)
{
	for(int i=0; i<GetInstance()->m_HandlingData.GetCount(); i++)
	{
		if(nameHash == GetInstance()->m_HandlingData[i]->m_handlingName)
			return i;		
	}

	return -1;
}

CHandlingData* CHandlingDataMgr::GetHandlingDataByName(atHashString nameHash)
{
	int nIndex = GetHandlingIndex(nameHash);

	if(nIndex > -1)
		return GetInstance()->m_HandlingData[nIndex];

	return NULL;
}

CHandlingData* CHandlingDataMgr::GetHandlingData(int nIndex)
{
	Assert(nIndex > -1 && nIndex < GetInstance()->m_HandlingData.GetCount());
	Assertf(GetInstance()->m_HandlingData[nIndex]->IsUsed(), "%s:Handling data not setup", GetInstance()->m_HandlingData[nIndex]->m_handlingName.TryGetCStr());
	return GetInstance()->m_HandlingData[nIndex];
}
