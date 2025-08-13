

// NOTE: When applying forces to objects be sure to choose the correct type.
// If you are not sure please ask for advice.
// Generally, use FORCE and/or TORQUES when applying forces continuously (over multiple frames). e.g. simulating wind, rockets, etc.
// Use an IMPULSE or ANGULAR_IMPULSE when applying a one off kick to an object. E.g. a bullet impact, car crash, etc.
// This will ensure your forces are consistent when the frame rate varies.
ENUM APPLY_FORCE_TYPE
	APPLY_TYPE_FORCE = 0,					//continuous force applied every frame, will produce a constant acceleration independent of frame rate.
	APPLY_TYPE_IMPULSE,						//Caution a single impact applied at a point in time, will produce a step change in velocity. If applied continuously will produce an acceleration that IS dependant on frame rate and may also break the physics engine, i.e. please do
	APPLY_TYPE_EXTERNAL_FORCE,		//same as a normal force but applied as if it was an external push to the object, through the breaking system. This means that breakable objects will break apart due to the force you're applying. 	
	APPLY_TYPE_EXTERNAL_IMPULSE, 	//ditto, a nomal impulse plus breaking.			
	APPLY_TYPE_TORQUE,				// Angular equivalent of force, causes continuous angular acceleration independent of framerate
	APPLY_TYPE_ANGULAR_IMPULSE		// Angular equivalent of impulse, causes instantaneous change in angular velocity
ENDENUM

ENUM PED_RAGDOLL_COMPONENTS // THESE MUST BE KEPT UP TO DATE WITH CODE-SIDE EQUIVALENTS IN PedDefines.h!
	RAGDOLL_PELVIS = 0,
	RAGDOLL_THIGH_L,
	RAGDOLL_CALF_L,
	RAGDOLL_FOOT_L,
	RAGDOLL_THIGH_R,
	RAGDOLL_CALF_R,
	RAGDOLL_FOOT_R,
	RAGDOLL_SPINE,
	RAGDOLL_SPINE1,
	RAGDOLL_SPINE2,
	RAGDOLL_SPINE3,
	RAGDOLL_CLAVICLE_L,
	RAGDOLL_UPPERARM_L,
	RAGDOLL_LOWERARM_L,
	RAGDOLL_HAND_L,
	RAGDOLL_CLAVICLE_R,
	RAGDOLL_UPPERARM_R,
	RAGDOLL_LOWERARM_R,
	RAGDOLL_HAND_R,
	RAGDOLL_NECK,
	RAGDOLL_HEAD,
	RAGDOLL_NUM_COMPONENTS
ENDENUM

ENUM PED_NM_BEHAVIOURS
	// basic behaviours (no intelligence)
	NM_RELAX = 0,		// just relax
	NM_FOETAL,			// go into foetal pose
	NM_ROLLUP,			// not used this one yet
	NM_WRITHE,			// writhe about in pain
	// complex behaviours
	NM_BALANCE,			// use whole body to balance and stay on feet
	NM_BRACE,			// brace for impact, not that useful without params for what's gonna hit you
	NM_REACH,			// reach for ground
	NM_SHOT,			// be shot, not that useful without params
	NM_BLAST			// fly through air after explosion
ENDENUM

ENUM VEHICLE_SEAT
	VS_ANY_PASSENGER = -2,				 //Any passenger seat
	VS_DRIVER = -1,						 // Drivers seat
	VS_FRONT_RIGHT = 0,					// Front Right seat
	VS_BACK_LEFT,						//Back left 	
	VS_BACK_RIGHT,						//Back right
	VS_EXTRA_LEFT_1,
	VS_EXTRA_RIGHT_1,
	VS_EXTRA_LEFT_2,
	VS_EXTRA_RIGHT_2,
	VS_EXTRA_LEFT_3,
	VS_EXTRA_RIGHT_3	
ENDENUM

