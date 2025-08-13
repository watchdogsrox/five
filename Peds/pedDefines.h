// Title	:	Ped.h
// Author	:	Gordon Speirs
// Started	:	02/09/99

#ifndef _PED_DEFINES_H_
#define _PED_DEFINES_H_

#include "Peds/PedMotionData.h"

namespace CPoolHelpers
{
	// Defined in 'Peds/PedFactory.cpp':
	int GetPedPoolSize();
	void SetPedPoolSize(int poolSize);
}
#define MAXNOOFPEDS			(CPoolHelpers::GetPedPoolSize())	// Used to be (90), but now it can be set through 'gameconfig.xml'.

// 0 means the velocity is extracted and applied to the physics like normal
// 1 means the velocity is extracted and applied to the placeable (physics capsule stays where it was)

//#define PED_MAX_WEAPON_SLOTS (13) // extra for gift (10). Another one for parachute.

#define PED_REMOVED_AFTER_DEATH			(30000)		// Ped will be removed even if close to cam (30sec)
#define PED_REMOVED_AFTER_DEATH_INRAMPAGE	(15000)		// Ped will be removed even if close to cam (15secs) while rampage running
#define PED_REMOVED_AFTER_DEATH_GANGWARS	(8000)		// Ped will be removed even if close to cam (8secs) if part of an attack wave
#define PED_REMOVAL_FADE_OUT_TIME		(5000)		// How long will the fading out take when peds get removed by script

// minimum impulse magnitude for making some damage
#define PED_MINDAMAGEIMPULSE			(6.0f)
#define PED_PLAYER_MINDAMAGEIMPULSE		(10.0f)
#define PED_DIEDAMAGEIMPULSE			(12.0f)

#define PED_HIT_HEAD_CLEAR (99999.99f)

#define PED_MAX_DIST_TO_WARP_SQR (25.0f)

//
#define TIME_BETWEEN_DEATH_AND_BLOOD_POOL_MS	(2000)

// Drowning timers
#define PED_MAX_TIME_IN_WATER					(30.0f)
#define PED_MAX_TIME_UNDERWATER					(7.0f)
#define PLAYER_MAX_TIME_IN_WATER				(-1.0f)
#define PLAYER_MAX_TIME_UNDERWATER				(10.0f)

//line tests in processprobes().
#define PED_GROUNDCOLLINE_OFFSET (-0.25f)
#define PED_GROUNDCOLLINE_OFFSET_TIMESTEP (0.15f)
#define PED_GROUNDPOS_RESET_Z (-1.0e6f)

#define PED_MAX_SECOND_SURFACE_DEPTH_ANIM		(1.0f)
#define PED_MAX_SECOND_SURFACE_DEPTH_RAGDOLL	(1.0f)
#define PED_MAX_SECOND_SURFACE_DEPTH_SKI		(0.1f)

#define LAZY_RAGDOLL_BOUNDS_UPDATE 0

// The maximum number which can be attached, including weapons, props & script-attached objects.
#define MAX_NUM_ENTITIES_ATTACHED_TO_PED		8

#define PED_GROUND_POS_HISTORY_SIZE				(2)

#define FPS_MODE_SUPPORTED						1
#if FPS_MODE_SUPPORTED
#define FPS_MODE_SUPPORTED_ONLY(...)  			__VA_ARGS__
#else
#define FPS_MODE_SUPPORTED_ONLY(...)
#endif // FPS_MODE_SUPPORTED

// Arrest state, set by the tasks, records when the ped is arrested
enum eArrestState
{
	ArrestState_None = 0,
	ArrestState_Arrested,
	ArrestState_Max
};
CompileTimeAssert(ArrestState_Max < BIT(2));

// Death state, set by the tasks, records when the ped is dead or dying
enum eDeathState
{
	DeathState_Alive = 0,
	DeathState_Dying,
	DeathState_Dead,
	DeathState_Max
};
CompileTimeAssert(DeathState_Max < BIT(2));

enum eRagdollState
{
	RAGDOLL_STATE_ANIM_LOCKED,			// locked on animation - never switch
	RAGDOLL_STATE_ANIM_PRESETUP,		// animation still getting set up, can't activate till after the ped has been rendered at least once!
	RAGDOLL_STATE_ANIM,					// fully animated ragdoll (doesn't react to collisions, only test probes will hit)
	RAGDOLL_STATE_ANIM_DRIVEN,			// ragdoll is active but driven by animation (player only)
	RAGDOLL_STATE_PHYS_ACTIVATE,		// ragdoll turned on and controlling ped, need to remove constrained collider
	RAGDOLL_STATE_PHYS,					// fully physical ragdoll (controls animation entirely)
	RAGDOLL_STATE_MAX = RAGDOLL_STATE_PHYS
};
CompileTimeAssert(RAGDOLL_STATE_MAX < BIT(3));

enum eBulletHitType
{
	PED_MISS_HIT = 0,
	PED_BODY_HIT,
	PED_HEAD_HIT
};

// had to add 2 big arrays of 11 matricies to ped for ragdoll stuff
#define SIZE_LARGEST_PEDDATA_CLASS	(128)
#define SIZE_LARGEST_PED_CLASS		(3216) //(2976) //(2944)	//(2896)



enum
{
	KNOCKOFFVEHICLE_DEFAULT = 0,
	KNOCKOFFVEHICLE_NEVER,
	KNOCKOFFVEHICLE_EASY,
	KNOCKOFFVEHICLE_HARD
	// only 2 bits allocated for this in CPedConfigFlags - increase storage if you want more options
};


// Gesture modes
enum eGestureModes
{
	GESTURE_MODE_DEFAULT,		     // if gesturing is enabled* gestures will blended in and out based on audio
	GESTURE_MODE_USE_ANIM_ALLOW_TAGS, // if gesturing is enabled* gestures will only be blended in during gesture allow tags
	GESTURE_MODE_USE_ANIM_BLOCK_TAGS, // if gesturing is enabled* gestures will be blended out during gesture block tags
	GESTURE_MODE_NUM			  // *if the conditions in BlockGestures are met
	// only 2 bits allocated for this in CPedConfigFlags - increase storage if you want more options
};


//-------------------------------------------------------------------------
// Enum for estimating the pose a ped is currently in
//-------------------------------------------------------------------------
enum EstimatedPose
{
	POSE_UNKNOWN = -1,
	POSE_ON_FRONT = 0,
	POSE_ON_RHS,
	POSE_ON_BACK,
	POSE_ON_LHS,
	POSE_STANDING
};

typedef enum
{
	DP_ByDoor,
	DP_Side1,
	DP_Side2,
	DP_Side3,
	DP_Side4,
	DP_OtherDoor,
	DP_Front,
	DP_Back,
	DP_FrontCorner,
	DP_BackCorner,
	DP_Roof,
	DP_SafePosNavmesh,
	DP_SafePosNavmeshLargeRadis,
	DP_SafePosCarNodes,
	DP_BackLower,
	DP_ForwardLower,
	DP_Back1,
	DP_Forward1,
	DP_Back2,
	DP_Forward2,
	DP_Back3,
	DP_Forward3,
	DP_Max
} DetachPosition;

/*
enum RagdollComponent
{
	RAGDOLL_BUTTOCKS = 0,	//0 Buttocks Char_Pelvis 
	RAGDOLL_THIGH_LEFT,	//1 Thigh_Left Char_L_Thigh 
	RAGDOLL_SHIN_LEFT,	//2 Shin_Left Char_L_Calf 
	RAGDOLL_FOOT_LEFT,	//3 Foot_Left Char_L_Foot 
	RAGDOLL_THIGH_RIGHT,	//4 Thigh_Right Char_R_Thigh 
	RAGDOLL_SHIN_RIGHT,	//5 Shin_Right Char_R_Calf 
	RAGDOLL_FOOT_RIGHT,	//6 Foot_Right Char_R_Foot 
	RAGDOLL_SPINE0,	//7 Spine0 Char_Spine 
	RAGDOLL_SPINE1,	//8 Spine1 Char_Spine1 
	RAGDOLL_SPINE2,	//9 Spine2 Char_Spine2 
	RAGDOLL_SPINE3,	//10 Spine3 Char_Spine3 
	RAGDOLL_NECK,	//11 Neck Char_Neck 
	RAGDOLL_HEAD,	//12 Head Char_Head 
	RAGDOLL_CLAVICLE_LEFT,	//13 Clavicle_Left Char_L_Clavicle 
	RAGDOLL_UPPER_ARM_LEFT,	//14 Upper_Arm_Left Char_L_UpperArm 
	RAGDOLL_LOWER_ARM_LEFT,	//15 Lower_Arm_Left Char_L_ForeArm 
	RAGDOLL_HAND_LEFT,	//16 Hand_Left Char_L_Hand 
	RAGDOLL_CLAVICLE_RIGHT,	//17 Clavicle_Right Char_R_Clavicle 
	RAGDOLL_UPPER_ARM_RIGHT,	//18 Upper_Arm_Right Char_R_UpperArm 
	RAGDOLL_LOWER_ARM_RIGHT,	//19 Lower_Arm_Right Char_R_ForeArm 
	RAGDOLL_HAND_RIGHT,	//20 Hand_Right Char_R_Hand 


	RAGDOLL_NUM_COMPONENTS
};
*/

enum RagdollComponent
{
	// These enums must match with the bound/component hierarchy defined in the Fred and Wilma binary files.
	// (.prt for PS3 and .xrt for XBox360.)

	RAGDOLL_INVALID = -1,	//-1  Invalid
	RAGDOLL_BUTTOCKS = 0,	//0  Buttocks
	RAGDOLL_THIGH_LEFT,		//1  Thigh_Left
	RAGDOLL_SHIN_LEFT,		//2  Shin_Left
	RAGDOLL_FOOT_LEFT,		//3  Foot_Left
	RAGDOLL_THIGH_RIGHT,	//4  Thigh_Right
	RAGDOLL_SHIN_RIGHT,		//5  Shin_Right
	RAGDOLL_FOOT_RIGHT,		//6  Foot_Right
	RAGDOLL_SPINE0,			//7  Spine0
	RAGDOLL_SPINE1,			//8  Spine1
	RAGDOLL_SPINE2,			//9  Spine2
	RAGDOLL_SPINE3,			//10 Spine3
	RAGDOLL_CLAVICLE_LEFT,	//11 Clavicle_Left
	RAGDOLL_UPPER_ARM_LEFT,	//12 Upper_Arm_Left
	RAGDOLL_LOWER_ARM_LEFT,	//13 Lower_Arm_Left
	RAGDOLL_HAND_LEFT,		//14 Hand_Left
	RAGDOLL_CLAVICLE_RIGHT,	//15 Clavicle_Right
	RAGDOLL_UPPER_ARM_RIGHT,//16 Upper_Arm_Right
	RAGDOLL_LOWER_ARM_RIGHT,//17 Lower_Arm_Right
	RAGDOLL_HAND_RIGHT,		//18 Hand_Right
	RAGDOLL_NECK,			//19 Neck
	RAGDOLL_HEAD,			//20 Head

	RAGDOLL_NUM_COMPONENTS
};

enum RagdollJoints
{
	// These enums must match with the bound/component hierarchy defined in the Fred and Wilma binary files.
	// (.prt for PS3 and .xrt for XBox360.)

	RAGDOLL_HIP_LEFT_JOINT = 0,	
	RAGDOLL_KNEE_LEFT_JOINT,		
	RAGDOLL_ANKLE_LEFT_JOINT,	
	RAGDOLL_HIP_RIGHT_JOINT,		
	RAGDOLL_KNEE_RIGHT_JOINT,	
	RAGDOLL_ANKLE_RIGHT_JOINT,		
	RAGDOLL_SPINE0_JOINT,			
	RAGDOLL_SPINE1_JOINT,			
	RAGDOLL_SPINE2_JOINT,			
	RAGDOLL_SPINE3_JOINT,			
	RAGDOLL_CLAVICLE_LEFT_JOINT,	
	RAGDOLL_SHOULDER_LEFT_JOINT,	
	RAGDOLL_ELBOW_LEFT_JOINT,	
	RAGDOLL_WRIST_LEFT_JOINT,		
	RAGDOLL_CLAVICLE_RIGHT_JOINT,	
	RAGDOLL_SHOULDER_RIGHT_JOINT,
	RAGDOLL_ELBOW_RIGHT_JOINT,
	RAGDOLL_WRIST_RIGHT_JOINT,	
	RAGDOLL_NECK_JOINT,			
	RAGDOLL_HEAD_JOINT,			

	RAGDOLL_NUM_JOINTS
};

//Task tree enums
enum ePedTaskTrees
{
	PED_TASK_TREE_PRIMARY=0,
	PED_TASK_TREE_SECONDARY,
	PED_TASK_TREE_MOVEMENT,
	PED_TASK_TREE_MOTION,
	PED_TASK_TREE_MAX
};

// enum for task priorities
enum ePedPrimaryTaskPriorities
{
	PED_TASK_PRIORITY_PHYSICAL_RESPONSE=0,
	PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP,
	PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP,
	PED_TASK_PRIORITY_PRIMARY,
	PED_TASK_PRIORITY_DEFAULT,
	PED_TASK_PRIORITY_MAX
};

// Movement tasks, run in parallel to main tasks to move the ped around the world
enum ePedMovementTaskPriorities
{
	PED_TASK_MOVEMENT_EVENT_RESPONSE = 0,
	PED_TASK_MOVEMENT_GENERAL,
	PED_TASK_MOVEMENT_DEFAULT,
	PED_TASK_MOVEMENT_MAX
};

// enum for secondary tasks. ie tasks that are run on top of the main
// tasks. eg look at 
enum ePedSecondaryTaskPriorities
{
	PED_TASK_SECONDARY_PARTIAL_ANIM,
	PED_TASK_SECONDARY_MAX
};

// enum for move blenders
enum eMotionTaskPriorities
{
	PED_TASK_MOTION_DEFAULT,
	PED_TASK_MOTION_MAX
};


#endif //_PED_DEFINES_H_
