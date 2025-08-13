#ifndef VEHICLE_DEFINES_H_INCLUDED
#define VEHICLE_DEFINES_H_INCLUDED

#include "system/bit.h"

#include "renderer/HierarchyIds.h"
#include "shader_source/vehicles/vehicle_common_values.h"
#include "system/nelem.h"


namespace CPoolHelpers
{
	// Defined in 'vehicles/Vehicle.cpp':
	int GetVehiclePoolSize();
	void SetVehiclePoolSize(int poolSize);
}
#define VEHICLE_POOL_SIZE		(CPoolHelpers::GetVehiclePoolSize())	// Used to be (200), but now it can be set through 'gameconfig.xml'.

#define NUM_VEH_CWHEELS_MAX		(10)
#define NUM_VEH_DOORS_MAX		(9)

#define MAX_VEHICLE_SEATS		(16)
#define MAX_ENTRY_EXIT_POINTS	(16)
#define MAX_PASSENGERS			(MAX_VEHICLE_SEATS-1)

#define MAX_VEHICLE_STR_DEPENDENCIES (14)	// Riot swat van

#define WHEEL_POOL_SIZE			(VEHICLE_POOL_SIZE * 5)

// 9 passengers + 1 driver, each with up to 4 attached entities
#define MAX_INDEPENDENT_VEHICLE_ENTITY_LIST		((MAX_PASSENGERS+1)*MAX_NUM_ENTITIES_ATTACHED_TO_PED)

#define CAR_ALARM_NONE			(0)
#define CAR_ALARM_SET			(MAX_UINT16)
#define CAR_ALARM_DURATION			(10000)
#define CAR_ALARM_RANDOM_DURATION	(10000)

#define CREATED_VEHICLE_HEALTH		(1000.0f)
#define CAR_ON_FIRE_HEALTH 			(250.0f)
#define CAR_REMOVED_AFTER_WRECKED	60000

#define MAX_ROT_SPEED_HELI_BLADES	(1.0f)
#define MIN_ROT_SPEED_HELI_CONTROL	(0.60f)
#define FLY_INPUT_NULL				(-9999.99f)

#define LIGHTS_IGNORE_DAMAGE			(1<<0)
#define LIGHTS_IGNORE_HEADLIGHTS		(1<<1)
#define LIGHTS_IGNORE_TAILLIGHTS		(1<<2)
#define LIGHTS_IGNORE_INTERIOR_LIGHT	(1<<3)
#define LIGHTS_ALWAYS_HAVE_POWER		(1<<4)	//used by the trailer so the light code ignores the engine on check
#define LIGHTS_IGNORE_DRIVER_CHECK		(1<<5)	//used by the trailer so the light code ignores the driver check
#define LIGHTS_FORCE_BRAKE_LIGHTS		(1<<6)	//used by script to force on brake lights
#define LIGHTS_FORCE_INTERIOR_LIGHT		(1<<7)	//used by the submarine, to use the interior lights all the time.
#define LIGHTS_USE_VOLUMETRIC_LIGHTS	(1<<8)	//used by the submarine, to enable volumetric light under water.
#define LIGHTS_UNDERWATER_CORONA_FADE	(1<<9)  //used by submarine, to ignore coronas if the camera and headlights are not underwater together or vice vec
#define LIGHTS_USE_EXTRA_AS_HEADLIGHTS	(1<<10)
#define LIGHTS_FORCE_LOWRES_VOLUME		(1<<11) //used by submarine, to force the volumetric lights to be low res

#define MAX_ENGINE_TEMPERATURE		(120.f)		// 120 degrees is the maximum temperature any engine can be
#define ENGINE_THERMAL_CONSTANT		(0.07f)	// rate of thermal change of engine block
#define ENGINE_TEMP_FAN_ON			(107.f)
#define ENGINE_TEMP_FAN_OFF			(80.1f)

#define HIGHEST_BRAKING_WITHOUT_LIGHT	(0.095f)

#define PLACEONROAD_DEFAULTHEIGHTUP		(5.0f)
#define PLACEONROAD_DEFAULTHEIGHTDOWN	(5.0f)

#define MAX_NUM_VEHICLE_WEAPONS (6)
#define MAX_NUM_BATTERY_WEAPONS (38)
#define MAX_NUM_VEHICLE_TURRETS_PER_SLOT (4)	// 1, 2, 3, 4
#define MAX_NUM_VEHICLE_TURRET_SLOTS (3)		// 1, A1, B1
#define MAX_NUM_VEHICLE_TURRETS (MAX_NUM_VEHICLE_TURRETS_PER_SLOT * MAX_NUM_VEHICLE_TURRET_SLOTS)

#define NO_PREFERENCE_ENTRY_PRIORITY	1
#define INDIRECT_ENTRY_PRIORITY	1
#define DIRECT_ENTRY_PRIORITY	2
#define PREFERED_SEAT_PRIORITY	3
#define PREFER_JACKING_PRIORITY	4
#define IN_AIR_ENTRY_PRIORITY	1
#define GROUND_ENTRY_PRIORITY	5

#define MAX_NUM_SPOILERS 2

// To catch DLC vehicles missing correct horn data - Update when new horns are added
#define NUM_VEHICLE_HORNS		58

// Standard max speed is 80
#define MAX_PLANE_SPEED (160.0f)

// Vehicle LOD levels
enum eVehicleDummyMode
{
	VDM_NONE,
	VDM_REAL,
	VDM_DUMMY,
	VDM_SUPERDUMMY,
};

enum eCarLockState
{	
	CARLOCK_NONE = 0, 
	CARLOCK_UNLOCKED, 
	CARLOCK_LOCKED,
	CARLOCK_LOCKOUT_PLAYER_ONLY,
	CARLOCK_LOCKED_PLAYER_INSIDE,
	CARLOCK_LOCKED_INITIALLY,		// for police cars and suchlike 
	CARLOCK_FORCE_SHUT_DOORS,		// not locked, but force player to shut door after they getout
	CARLOCK_LOCKED_BUT_CAN_BE_DAMAGED,
	CARLOCK_LOCKED_BUT_BOOT_UNLOCKED,
	CARLOCK_LOCKED_NO_PASSENGERS,
	CARLOCK_CANNOT_ENTER,			// Peds cannot even attempt to enter
	CARLOCK_PARTIALLY,				// doors can be locked individually
	CARLOCK_LOCKED_EXCEPT_TEAM1,	// car lock states for only allowing certain teams to use a vehicle
	CARLOCK_LOCKED_EXCEPT_TEAM2,
	CARLOCK_LOCKED_EXCEPT_TEAM3,
	CARLOCK_LOCKED_EXCEPT_TEAM4,
	CARLOCK_LOCKED_EXCEPT_TEAM5,
	CARLOCK_LOCKED_EXCEPT_TEAM6,
	CARLOCK_LOCKED_EXCEPT_TEAM7,
	CARLOCK_LOCKED_EXCEPT_TEAM8,
	CARLOCK_NUM_STATES
};

#if __BANK
extern const char* VehicleLockStateStrings[];
#endif // __BANK

enum eFlightModel
{
	FLIGHTMODEL_DODO = 0,
	FLIGHTMODEL_PLANE,
	FLIGHTMODEL_PLANE_LOWPOWER,
	FLIGHTMODEL_PLANE_GLIDER,
	FLIGHTMODEL_SPACE_FIGHTER,
	FLIGHTMODEL_HELI,
	FLIGHTMODEL_AUTOGYRO
};

enum eVehicleCheats
{
	// auto reset cheats
	VEH_CHEAT_ABS		= BIT(0),
	VEH_CHEAT_TC		= BIT(1),
	VEH_CHEAT_SC		= BIT(2),
	VEH_CHEAT_GRIP1		= BIT(3),
	VEH_CHEAT_GRIP2		= BIT(4),
	VEH_CHEAT_POWER1	= BIT(5),
	VEH_CHEAT_POWER2	= BIT(6),

	// permanent cheat settings
	VEH_SET_ABS			= BIT(16),
	VEH_SET_ABS_ALT		= BIT(17),
	VEH_SET_TC			= BIT(18),
	VEH_SET_SC			= BIT(19),
	VEH_SET_GRIP1		= BIT(20),
	VEH_SET_GRIP2		= BIT(21),
	VEH_SET_POWER1		= BIT(22),
	VEH_SET_POWER2		= BIT(23)
};
#define VEH_CHEAT_RESET_MASK (BIT(16) - 1)

// axis for component rotation function
enum eRotationAxis
{
	ROT_AXIS_X=0,
	ROT_AXIS_Y,
	ROT_AXIS_Z,
	ROT_AXIS_LOCAL_X,
	ROT_AXIS_LOCAL_Y,
	ROT_AXIS_LOCAL_Z 
};

enum
{
	NO_CAR_LIGHT_OVERRIDE,
	FORCE_CAR_LIGHTS_OFF,
	FORCE_CAR_LIGHTS_ON,
	SET_CAR_LIGHTS_ON,
	SET_CAR_LIGHTS_OFF
};

enum
{
	NO_HEADLIGHT_SHADOWS			= 0x0,
	HEADLIGHTS_CAST_DYNAMIC_SHADOWS = 0x1,
	HEADLIGHTS_CAST_STATIC_SHADOWS  = 0x2,
	HEADLIGHTS_CAST_FULL_SHADOWS    = 0x3,
};

// options to tell a car what type of extras to set
enum eVehicleExtraCommands
{ 
	EXTRAS_MASK_GANG_OFF = 0,
	EXTRAS_MASK_GANG_ON,
	EXTRAS_MASK_CONV_DRY,
	EXTRAS_MASK_CONV_RAIN,
	EXTRAS_MASK_CONV_OPEN,
	EXTRAS_MASK_TAXI,
	EXTRAS_MASK_SCRIPT,
	EXTRAS_MASK_BIKE_TANK,
	EXTRAS_MASK_BIKE_FAIRING,
	EXTRAS_MASK_BIKE_EXHAUST,
	EXTRAS_MASK_BIKE_MISC,
	EXTRAS_MASK_INTERIOR,

	EXTRAS_MASK_HELI_VAR_1,
	EXTRAS_MASK_HELI_VAR_2,
	EXTRAS_MASK_HELI_VAR_3,
	
	EXTRAS_MASK_NUM_MASKS,

	EXTRAS_MASK_TYPE_ALL = 0,
	EXTRAS_MASK_TYPE_ONE,
	EXTRAS_MASK_TYPE_RANDOM,

	EXTRAS_SET_RAIN = BIT(8),
	EXTRAS_SET_GANG = BIT(9),
	EXTRAS_SET_CONV_OPEN = BIT(10)
};

typedef enum
{
	VT_NONE = -1,
	VT_LOW,
	VT_STD,
	VT_VAN,
	VT_TRUCK,
	VT_MAX
} VehicleAnimType;

//-------------------------------------------------------------------------
// Enum of suspension types
//-------------------------------------------------------------------------
typedef enum
{
	Sus_NoTilt,
	Sus_Solid,
	Sus_McPherson,
	Sus_Reverse,
	Sus_Torsion
} SuspensionType;

#if __BANK
	enum eVehicleDebug
	{
		VEH_DEBUG_OFF = 0,
		VEH_DEBUG_PERFORMANCE,
		VEH_DEBUG_HANDLING,
		VEH_DEBUG_FX,
		VEH_DEBUG_DAMAGE,
		VEH_DEBUG_END
	};
#endif // #if __BANK

//task manager defines

//Task tree enums
enum eVehicleTaskTrees
{
	VEHICLE_TASK_TREE_PRIMARY=0,
	VEHICLE_TASK_TREE_SECONDARY,
	VEHICLE_TASK_TREE_MAX
};

// enum for task priorities
enum eVehiclePrimaryTaskPriorities
{
    VEHICLE_TASK_PRIORITY_CRASH=0,
	VEHICLE_TASK_PRIORITY_PRIMARY,
	VEHICLE_TASK_PRIORITY_MAX
};

// enum for secondary task priorities
enum eVehicleSecondaryTaskPriorities
{
	VEHICLE_TASK_SECONDARY_ANIM=0,
	VEHICLE_TASK_SECONDARY_MAX
};
#endif // VEHICLE_DEFINES_H_INCLUDED

