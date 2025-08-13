#ifndef TASK_VEHICLE_MISSION_H
#define TASK_VEHICLE_MISSION_H


enum VehMissionType
{
	MISSION_NONE,

	// The following can be ubermissions as well as lower missions (or only ubermissions)
	MISSION_CRUISE,
	MISSION_RAM,
	MISSION_BLOCK,
	MISSION_GOTO,
	MISSION_STOP,
	MISSION_ATTACK,
	MISSION_FOLLOW,
	MISSION_FLEE,
	MISSION_CIRCLE,
	MISSION_ESCORT_LEFT,
	MISSION_ESCORT_RIGHT,
	MISSION_ESCORT_REAR,
	MISSION_ESCORT_FRONT,
	MISSION_GOTO_RACING,
	MISSION_FOLLOW_RECORDING,
	MISSION_POLICE_BEHAVIOUR,
	MISSION_PARK_PERPENDICULAR,
	MISSION_PARK_PARALLEL,
	MISSION_LAND,
	MISSION_LAND_AND_WAIT,
	MISSION_CRASH,
	MISSION_PULL_OVER,
	MISSION_PROTECT,

	MISSION_LAST_UBERMISSION,
	// The following are lower level only and should be at the end of the enum
	MISSION_PARK_PERPENDICULAR_1 = MISSION_LAST_UBERMISSION,
	MISSION_PARK_PERPENDICULAR_2,
	MISSION_PARK_PARALLEL_1,
	MISSION_PARK_PARALLEL_2,
	MISSION_GOTO_STRAIGHTLINE,
	MISSION_LEAVE_ALONE,			// This car is controlled by a recording and shouldn't have ai applied to it.
	MISSION_BLOCK_BACKANDFORTH,
	MISSION_TURN_CLOCKWISE_GOING_FORWARD,
	MISSION_TURN_CLOCKWISE_GOING_BACKWARD,
	MISSION_TURN_COUNTERCLOCKWISE_GOING_FORWARD,
	MISSION_TURN_COUNTERCLOCKWISE_GOING_BACKWARD,
	MISSION_BLOCK_FRONT,
	MISSION_BLOCK_BEHIND,

	MISSION_LAST,
};


enum VehTempActType
{
	TEMPACT_NONE,
	TEMPACT_WAIT,
	TEMPACT_EMPTYTOBEREUSED,
	TEMPACT_REVERSE,
	TEMPACT_HANDBRAKETURNLEFT,
	TEMPACT_HANDBRAKETURNRIGHT,
	TEMPACT_HANDBRAKESTRAIGHT,
	TEMPACT_TURNLEFT,
	TEMPACT_TURNRIGHT,
	TEMPACT_GOFORWARD,
	TEMPACT_SWERVELEFT,
	TEMPACT_SWERVERIGHT,
	TEMPACT_EMPTYTOBEREUSED2,
	TEMPACT_REVERSE_LEFT,
	TEMPACT_REVERSE_RIGHT,
	TEMPACT_PLANE_FLY_UP,
	TEMPACT_PLANE_FLY_STRAIGHT,
	TEMPACT_PLANE_SHARP_LEFT,
	TEMPACT_PLANE_SHARP_RIGHT,
	TEMPACT_HEADON_COLLISION,
	TEMPACT_SWERVELEFT_STOP,
	TEMPACT_SWERVERIGHT_STOP,
	TEMPACT_REVERSE_STRAIGHT,
	TEMPACT_BOOST_USE_STEERING_ANGLE,
	TEMPACT_BRAKE,
	TEMPACT_HANDBRAKETURNLEFT_INTELLIGENT,
	TEMPACT_HANDBRAKETURNRIGHT_INTELLIGENT,
	TEMPACT_HANDBRAKESTRAIGHT_INTELLIGENT,
	TEMPACT_REVERSE_STRAIGHT_HARD,
	TEMPACT_EMPTYTOBEREUSED3,
	TEMPACT_BURNOUT,
	TEMPACT_REV_ENGINE,
	TEMPACT_GOFORWARD_HARD,
	TEMPACT_SURFACE_SUBMARINE,
	TEMPACT_LAST
};

enum DrivingFlags
{
	// Avoidance
	DF_StopForCars					= BIT(0),
	DF_StopForPeds					= BIT(1),
	DF_SwerveAroundAllCars			= BIT(2),
	DF_SteerAroundStationaryCars	= BIT(3),
	DF_SteerAroundPeds				= BIT(4),
	DF_SteerAroundObjects			= BIT(5),
	DF_DontSteerAroundPlayerPed		= BIT(6),
	DF_StopAtLights					= BIT(7),
	DF_GoOffRoadWhenAvoiding		= BIT(8),

	DF_DriveIntoOncomingTraffic		= BIT(9),
	DF_DriveInReverse				= BIT(10),
	DF_UseWanderFallbackInsteadOfStraightLine = BIT(11),	// If pathfinding fails, cruise randomly instead of going on a straight line.
	DF_AvoidRestrictedAreas			= BIT(12),
	
	// These only work on MISSION_CRUISE
	DF_PreventBackgroundPathfinding	= BIT(13),
	DF_AdjustCruiseSpeedBasedOnRoadSpeed	= BIT(14),

	DF_PreventJoinInRoadDirectionWhenMoving		= BIT(15),
	DF_DontAvoidTarget				= BIT(16),
	DF_TargetPositionOverridesEntity= BIT(17),

	DF_UseShortCutLinks				= BIT(18),
	DF_ChangeLanesAroundObstructions= BIT(19),
	DF_AvoidTargetCoors				= BIT(20),	//pathfind away from instead of towards m_target
	DF_UseSwitchedOffNodes			= BIT(21),
	DF_PreferNavmeshRoute			= BIT(22),
	
	// Only works for planes using MISSION_GOTO, will cause them to drive along the ground instead of fly
	DF_PlaneTaxiMode				= BIT(23),

	DF_ForceStraightLine			= BIT(24),	// if set, go on a straight line instead of following roads, etc
	DF_UseStringPullingAtJunctions	= BIT(25),
	
	DF_AvoidAdverseConditions		= BIT(26),	// Avoids "adverse conditions" (shocking events, etc.) when cruising.
	DF_AvoidTurns					= BIT(27),	// Avoids turns when cruising.
	DF_ExtendRouteWithWanderResults	= BIT(28),
	DF_AvoidHighways				= BIT(29),
	DF_ForceJoinInRoadDirection		= BIT(30),

	DF_DontTerminateTaskWhenAchieved= BIT(31),

	DF_LastFlag						= DF_DontTerminateTaskWhenAchieved,

	//if you change one of these, please remember to update
	//commands_vehicle.sch in the script project!!!
	DMode_StopForCars = DF_StopForCars|DF_StopForPeds|DF_SteerAroundObjects|DF_SteerAroundStationaryCars|DF_StopAtLights|DF_UseShortCutLinks|DF_ChangeLanesAroundObstructions,		// Obey lights too
	DMode_StopForCars_Strict = DF_StopForCars|DF_StopForPeds|DF_StopAtLights|DF_UseShortCutLinks,		// Doesn't deviate an inch.
	DMode_AvoidCars = DF_SwerveAroundAllCars|DF_SteerAroundObjects|DF_UseShortCutLinks|DF_ChangeLanesAroundObstructions|DF_StopForCars,
	DMode_AvoidCars_Reckless = DF_SwerveAroundAllCars|DF_SteerAroundObjects|DF_UseShortCutLinks|DF_ChangeLanesAroundObstructions,
	DMode_PloughThrough = DF_UseShortCutLinks,
	DMode_StopForCars_IgnoreLights = DF_StopForCars|DF_SteerAroundStationaryCars|DF_StopForPeds|DF_SteerAroundObjects|DF_UseShortCutLinks|DF_ChangeLanesAroundObstructions,
	DMode_AvoidCars_ObeyLights = DF_SwerveAroundAllCars|DF_StopAtLights|DF_SteerAroundObjects|DF_UseShortCutLinks|DF_ChangeLanesAroundObstructions|DF_StopForCars,
	DMode_AvoidCars_StopForPeds_ObeyLights = DF_SwerveAroundAllCars|DF_StopAtLights|DF_StopForPeds|DF_SteerAroundObjects|DF_UseShortCutLinks|DF_ChangeLanesAroundObstructions|DF_StopForCars,
};

#endif