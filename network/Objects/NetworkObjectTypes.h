//
// NetworkObjectTypes.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef NETWORK_OBJECT_TYPES_H
#define NETWORK_OBJECT_TYPES_H

#include "fwnet/nettypes.h"

enum NetworkObjectTypes
{
    NET_OBJ_TYPE_AUTOMOBILE,
    NET_OBJ_TYPE_BIKE,
    NET_OBJ_TYPE_BOAT,
	NET_OBJ_TYPE_DOOR,
    NET_OBJ_TYPE_HELI,
    NET_OBJ_TYPE_OBJECT,
    NET_OBJ_TYPE_PED,
    NET_OBJ_TYPE_PICKUP,
    NET_OBJ_TYPE_PICKUP_PLACEMENT,
	NET_OBJ_TYPE_PLANE,
	NET_OBJ_TYPE_SUBMARINE,
    NET_OBJ_TYPE_PLAYER,
    NET_OBJ_TYPE_TRAILER,
    NET_OBJ_TYPE_TRAIN,
	NET_OBJ_TYPE_GLASS_PANE,
	NUM_NET_OBJ_TYPES
};

inline bool IsVehicleObjectType(NetworkObjectType type)
{
    switch(type)
    {
    case NET_OBJ_TYPE_AUTOMOBILE:
    case NET_OBJ_TYPE_BIKE:
    case NET_OBJ_TYPE_BOAT:
    case NET_OBJ_TYPE_HELI:
    case NET_OBJ_TYPE_PLANE:
    case NET_OBJ_TYPE_SUBMARINE:
    case NET_OBJ_TYPE_TRAILER:
    case NET_OBJ_TYPE_TRAIN:
        return true;
    default:
        return false;
    }
}

inline bool IsAPendingTutorialSessionReassignType(NetworkObjectType type)
{
	switch(type)
	{
	case NET_OBJ_TYPE_PLAYER:
		return false;
	default:
		return true;
	}
}

//PURPOSE
// Enumeration of result codes for the CanClone() function
enum
{
    CC_DIFFERENT_BUBBLE = CC_FAIL_USER, // The player we are trying to clone the object on is in a different roaming bubble
    CC_INVALID_PLAYER_INDEX,            // The player we are trying to clone the object on has an invalid player index
    CC_REMOVE_FROM_WORLD_FLAG,          // The ped object we are trying to clone is in the process of being removed from the world
    CC_HAS_SP_MODEL,                    // The player object we are trying to clone is currently using a single player model
	CC_MONEY_PICKUP,					// The pickup is a money pickup
};


enum
{
	CTD_VEH_OCCUPANT_CANT_DEL = CTD_BASE_END,	// Vehicle's Occupant Can't Be Deleted
	CTD_VEH_CHILD_CANT_DEL,						// Vehicle's Child Can't Be Deleted
	CTD_VEH_DUMMY_CHILD_CANT_DEL,				// Vehicle's Dummy Child Can't Be Deleted
	CTD_VEH_BEING_TOWED,						// Vehicle Being Towed
	CTD_PICKUP_PENDING_COLLECTION,				// Pickup Pending Collection
	CTD_PICKUP_WAITING_TO_SYNC,					// Pickup is desynced with at least one player
	CTD_RESPAWNING,								// Local Object Respawning
};


//PURPOSE
// Enumeration of result codes for the CanPassControl() function
enum
{
    CPC_FAIL_NO_GAME_OBJECT = CPC_FAIL_USER, // The object has no game object
    CPC_FAIL_SPECTATING,                     // Can't pass control of script objects to spectating players
    CPC_FAIL_NOT_SCRIPT_PARTICIPANT,         // Target player is not a participant of the script that created the object
	CPC_FAIL_DEBUG_NO_PROXIMITY,             // Proximity migrations have been disabled in rag
	CPC_FAIL_DEBUG_NO_OWNERSHIP_CHANGE,      // All migrations have been disabled in rag
    CPC_FAIL_PLAYER_NO_ACCEPT,               // The player state is preventing control passing
    CPC_FAIL_PROXIMITY_TIMER,                // The proximity migration timer is active
    CPC_FAIL_SCRIPT_TIMER,                   // The script migration timer is active
    CPC_FAIL_DELETION_TIMER,                 // The object has no game object
    CPC_FAIL_OVERRIDING_CRITICAL_STATE,      // The object is having critical state overridden on remote machines
    CPC_FAIL_IN_CUTSCENE,                    // The object is in a cutscene
    CPC_FAIL_NOT_ADDED_TO_WORLD,             // The game object has not been added to the world yet
    CPC_FAIL_REMOVING_FROM_WORLD,            // The game object is being removed from the world
	CPC_FAIL_PICKUP_PLACEMENT,               // Pickup placement fail
	CPC_FAIL_PICKUP_PLACEMENT_DESTROYED,     // Pickup placement being destroyed
    CPC_FAIL_PHYSICAL_ATTACH,                // Physically attached object can only migrate with what they are attached to
    CPC_FAIL_PED_TASKS,                      // One or more ped tasks prevent the object migrating
    CPC_FAIL_IS_PLAYER,                      // Players cannot migrate to other machines
    CPC_FAIL_IS_TRAIN,                       // Trains do not migrate to other machines
    CPC_FAIL_VEHICLE_OCCUPANT,               // One of more vehicle occupants cannot be passed
    CPC_FAIL_VEHICLE_OCCUPANT_CLONE_STATE,   // The clone state is not equal between the vehicle and one or more occupants
	CPC_FAIL_VEHICLE_OCCUPANT_SEAT_STATE_UNSYNCED, // One or more peds last in one of the vehicle seats have not synced their vehicle state
	CPC_FAIL_VEHICLE_DRIVER_CANT_MIGRATE,	 // The driver of the vehicle cannot migrate
    CPC_FAIL_VEHICLE_RESPOT,                 // The vehicle is in the process of being respotted
    CPC_FAIL_VEHICLE_COMPONENT_IN_USE,       // One or more vehicle components are in use
    CPC_FAIL_VEHICLE_PLAYER_DRIVER,          // The vehicle is being driven by a player
	CPC_FAIL_VEHICLE_TASK,					 // The vehicle's AI task prevented migration
	CPC_FAIL_CRITICAL_STATE_UNSYNCED,        // The critical state of the object is not properly synced 
	CPC_FAIL_BEING_TELEPORTED,               // The object is being teleported
	CPC_FAIL_SCHEDULED_FOR_CREATION,		 // The vehicle is pending creation
	CPC_FAIL_RESPAWN_IN_PROGRESS,		     // The network respawn event is in progress
	CPC_FAIL_OWNERSHIP_TOKEN_UNSYNCED,	     // The ownership token has not been synced by all players
	CPC_FAIL_VEHICLE_HAS_SCHEDULED_OCCUPANTS,// The vehicle has occupants pending
	CPC_FAIL_VEHICLE_IN_ROAD_BLOCK,          // The vehicle is in a road block
	CPC_FAIL_VEHICLE_ATTACHED_TO_CARGOBOB,   // The vehicle is attached to the cargobob helicopter by rope
	CPC_FAIL_PHYSICAL_ATTACH_STATE_MISMATCH, // The attachment state of the entity does not match that dictated by network updates
	CPC_FAIL_WRECKED_VEHICLE_IN_AIR,		 // The vehicle is wrecked and in the air, prevent CPC until on the ground
	CPC_FAIL_VEHICLE_GADGET_OBJECT,			 // The object is a part of a vehicle gadget and so should remain under the ownership of the player controlling the parent vehicle.
	CPC_PICKUP_WARPING,						 // The pickup is warping to an accessible location
	CPC_PICKUP_HAS_PLACEMENT,				 // The pickup has a pickup placement
	CPC_PICKUP_COMING_TO_REST,			     // The pickup is coming to rest
	CPC_FAIL_PED_GROUP_UNSYNCED,             // The ped's group is not synced with the player being passed control to
	CPC_FAIL_REMOVE_FROM_WORLD_WHEN_INVISIBLE, // The object has the bRemoveFromWorldWhenNotVisible flag set so dont pass ownership
	CPC_PICKUP_ATTACHED_TO_LOCAL_PED,		 // The pickup is attached to a local ped
	CPC_FAIL_CONCEALED,						 // Physical object is concealed
	CPC_FAIL_ATTACHED_CHILD_NOT_CLONED,	 // If any of the networked child attachments are not cloned
	CPC_FAIL_TRAIN_ENGINE_LOCAL,			// Can't pass of train that has an engine that is locally owned
};

//PURPOSE
// Enumeration of result codes for the CanAcceptControl() function
enum
{
	CAC_FAIL_SPECTATING = CAC_FAIL_USER,    // Can't accept control of script objects when spectating
	CAC_FAIL_SCRIPT_REJECT,                 // The script handler rejected control
	CAC_FAIL_NOT_SCRIPT_PARTICIPANT,        // We are not a participant of the script that created the object
	CAC_FAIL_NOT_ADDED_TO_WORLD,            // The game object has not been added to the world yet
	CAC_FAIL_PED_TASKS,                     // One or more tasks running on the ped prevent accepting control
	CAC_FAIL_VEHICLE_OCCUPANT,              // One or more of the occupants of the vehicle cannot be accepted
	CAC_FAIL_PENDING_TUTORIAL_CHANGE,       // Can't accept control of script objects when local player is pending a tutorial session change
	CAC_FAIL_COLLECTED_WITH_PICKUP,         // Can't accept control of a collected pickup placement if we haven't received the clone remove of its pickup
	CAC_FAIL_UNMANAGED_RAGDOLL,             // The ped is ragdolling without an NM task to handle it in the queriable interface
	CAC_FAIL_UNEXPLODED,				    // The object is still trying to explode itself
	CAC_FAIL_SYNCED_SCENE_NO_TASK,          // The ped is running a synced scene without a synced scene task
	CAC_FAIL_VEHICLE_I_AM_IN_NOT_CLONED,    // This is a script ped that is in a car we don't know about!    
	CAC_FAIL_CONCEALED,						// Physical object is concealed
};

//PURPOSE
// Enumeration of result codes for the CanBlend() function
enum
{
    CB_SUCCESS,							// Can Blend
    CB_FAIL_NO_GAME_OBJECT,				// Can't blend when we don't have a game object
    CB_FAIL_BLENDING_DISABLED,			// Can't blend when blending has been disabled
    CB_FAIL_ATTACHED,					// Can't blend when attached to another object
    CB_FAIL_FIXED,						// Can't blend when fixed (unless CanBlendWhenFixed() returns true)
    CB_FAIL_COLLISION_TIMER,			// Can't blend when the collision timer is active
    CB_FAIL_IN_VEHICLE,					// Can't blend peds in vehicles
    CB_FAIL_BLENDER_OVERRIDDEN,			// Can't blend when the blender is being overridden
	CB_FAIL_FRAG_OBJECT,				// Frag objects are only cloned and do not get synced
    CB_FAIL_STOPPED_PREDICTING,			// Can't blend when network objects haven't received an update for too long
	CB_FAIL_SYNCED_SCENE,				// Can't blend objects running a synced scene
};

//PURPOSE
// Enumeration of result codes for the NetworkBlenderIsOverridden() function
enum
{
	NBO_NOT_OVERRIDDEN,             // Not overridden
	NBO_COLLISION_TIMER,            // Disabled because the object was recently involved in a collision
	NBO_PED_TASKS,                  // One of more ped tasks are overriding the network blender
	NBO_DIFFERENT_TUTORIAL_SESSION, // This is a player in a different tutorial session
	NBO_FRAG_OBJECT,                // Frag objects are only cloned and do not get synced
	NBO_ATTACHED_PICKUP,            // Attached pickup
	NBO_SYNCED_SCENE,               // This object is part of a synced scene
	NBO_PICKUP_CLOSE_TO_TARGET,		// Pickups stop blending when they are close to their target
	NBO_DEAD_PED,					// The ped is dead
	NBO_FADING_OUT,					// The entity is fading out
	NBO_FAIL_STOPPED_PREDICTING,    // Used when network objects haven't received an update for too long
    NBO_NOT_ACCEPTING_OBJECTS,      // The local player is not accepting objects and passing objects on to other players
    NBO_FULL_BODY_SECONDARY_ANIM,   // A player ped is using a full body secondary anim - network blender causes glitches in this case
    NBO_FAIL_RUNNING_CUTSCENE,      // The entity is currently part of a mo-cap cutscene
	NBO_MAGNET_PICKUP,				// The entity is part of a magnet pick-up and so is being controlled locally for smoothness.
	NBO_VEHICLE_FRAG_SETTLED,       // This is a vehicle frag that has settled.
    NBO_FOCUS_ENTITY_DEBUG,         // The focus entity is having it's network blender overridden via a debug widget
    NBO_ARENA_BALL_TIMER            // The Arena ball is overriding the blender for a time
};

//PURPOSE
// Enumeration of result codes for the CanProcessPendingAttachment() function
enum
{
	CPA_SUCCESS,						// Can process attachment
	CPA_OVERRIDDEN,						// Attachment is overridden
	CPA_NO_ENTITY,						// There is no game entity 
	CPA_NOT_NETWORKED,					// The target attach entity is not networked
	CPA_NO_TARGET_ENTITY,				// The target entity does not exist 
	CPA_CIRCULAR_ATTACHMENT,			// The target attach entity is attached to this entity
	CPA_PED_IN_CAR,						// The ped is still in a car
	CPA_PED_HAS_ATTACHED_RAGDOLL,		// The ped's blender is ragdoll attached
	CPA_PED_RAGDOLLING,					// The ped is ragdolling
	CPA_VEHICLE_DUMMY,					// The vehicle is a dummy and attached to the correct entity
	CPA_LOCAL_PLAYER_ATTACHMENT,		// The entity is being attached to the local player. This shouldn't happen from sync messages
	CPA_LOCAL_PLAYER_DRIVER_ATTACHMENT  // The entity is being attached to a vehicle driven by the local player
};

//PURPOSE
// Enumeration of result codes for the CPhysical::SetFixedByNetwork() function
enum
{
	FBN_NONE,							    // The entity is not fixed
	FBN_HIDDEN_FOR_CUTSCENE,			    // The entity is hidden during a MP cutscene
	FBN_UNSTREAMED_INTERIOR,			    // The entity is in an unstreamed interior
	FBN_ON_RETAIN_LIST,					    // The entity is on an interior retain list
	FBN_BOX_STREAMER,					    // The box streamer says no collision 
	FBN_DISTANCE,						    // The entity is too far away
	FBN_TUTORIAL,						    // The entity is hidden for a tutorial
	FBN_ATTACH_PARENT,					    // The entity is attached to an entity that is fixed by network
	FBN_IN_AIR,							    // The entity is in the air
	FBN_PLAYER_CONTROLS_DISABLED,		    // The clone player has controls disabled
	FBN_SYNCED_SCENE_NO_TASK,			    // The entity is a ped is running a synced scene without a synced scene task
    FBN_HIDDEN_UNDER_MAP,                   // The entity is a ped being hidden under the map
    FBN_CANNOT_SWITCH_TO_DUMMY,             // The entity is a vehicle that cannot switch to dummy physics mode
    FBN_PARENT_VEHICLE_FIXED_BY_NETWORK,    // The entity is a trailer attached to a parent vehicle that is fixed by network
    FBN_EXPOSE_LOCAL_PLAYER_AFTER_CUTSCENE, // The entity is the local player being exposed after a cutscene
	FBN_FIXED_FOR_LOCAL_CONCEAL,            // This entity is locally concealed and fixed by network
	FBN_NOT_PROCESSING_FIXED_BY_NETWORK,	// Not processing fixed by network. Check NPFBN for more details
};

// PURPOSE
// Enumeration of result codes for CNetObjPhysical::FixPhysicsWhenNoMapOrInterior function
enum
{
	NPFBN_NONE,								// Default case
	NPFBN_LOCAL_IN_WATER,					// Local entity is in water
	NPFBN_CLONE_IN_WATER,					// Clone entity is in water
	NPFBN_ATTACHED,							// Entity is attached
	NPFBN_IN_AIR,							// Entity is in air
	NPFBN_IS_PARACHUTE,						// Entity is parachute
	NPFBN_IS_VEHICLE_GADGET,				// Entity is vehicle gadget
	NPFBN_LOW_LOD_NAVMESH,					// Low LOD ped is on navmesh
	NPFBN_ANIMATED_Z_POS,					// Ped's z position is set to by animation
};

//PURPOSE
// Enumeration of result codes for the IsInScope() function
enum
{
	SCOPE_NONE,										// No scope reason

	// in scope reasons:
	SCOPE_IN_ALWAYS,								// GLOBALFLAG_CLONEALWAYS is set on the entity
	SCOPE_IN_PROXIMITY_RANGE_PLAYER,				// The entity has come into range with the player's ped
	SCOPE_IN_PROXIMITY_RANGE_CAMERA,				// The entity has come into range with the player's camera
	SCOPE_IN_SCRIPT_PARTICIPANT,					// The remote player is running the script that created the object
	SCOPE_IN_ENTITY_ALWAYS_CLONED,					// The entity has been set to always clone for this player (via SET_NETWORK_ID_ALWAYS_EXISTS_FOR_PLAYER)
	SCOPE_IN_PED_VEHICLE_CLONED,					// The ped's vehicle has been cloned for this player
	SCOPE_IN_PED_DRIVER_VEHICLE_OWNER,				// The ped is driving a vehicle owned by this player
	SCOPE_IN_PED_RAPPELLING_HELI_CLONED,			// The rappelling ped's heli is cloned for this player
	SCOPE_IN_PED_RAPPELLING_HELI_IN_SCOPE,		// The rappelling ped's heli is in scope with this player
	SCOPE_IN_VEHICLE_OCCUPANT_IN_SCOPE,				// The vehicle has an occupant in scope with this player
	SCOPE_IN_VEHICLE_OCCUPANT_OWNER,				// The vehicle has an occupant owned by this player
	SCOPE_IN_VEHICLE_OCCUPANT_PLAYER,				// The vehicle has this player as an occupant
	SCOPE_IN_VEHICLE_ATTACH_PARENT_IN_SCOPE,		// The vehicle is attached to another vehicle in scope with this player
	SCOPE_IN_OBJECT_ATTACH_CHILD_IN_SCOPE,			// The object has a child attachment that is in scope with this player
	SCOPE_IN_OBJECT_ATTACH_PARENT_IN_SCOPE,			// The attached parent of the object is in scope so this should be too
	SCOPE_IN_OBJECT_VEHICLE_GADGET_IN_SCOPE,		// The object is part of a vehicle gadget on a vehicle that has been cloned for this player
	SCOPE_IN_PICKUP_DEBUG_CREATED,				    // This is a debug created pickup
	SCOPE_IN_PICKUP_PLACEMENT_REGENERATING,			// The pickup placement is regenerating
	SCOPE_IN_CHILD_ATTACHMENT_OWNER,				// The child attachment is owned by this player

	// out of scope reasons:
	SCOPE_OUT_NON_PHYSICAL_PLAYER,					// The remote player is not physical
	SCOPE_OUT_NOT_SCRIPT_PARTICIPANT,				// The remote player is not running the script that created the object
	SCOPE_OUT_PROXIMITY_RANGE,						// The entity is out of proximity range of the player
	SCOPE_OUT_BENEATH_WATER,						// The player is above water and the entity below
	SCOPE_OUT_PLAYER_BENEATH_WATER,					// The entity is above water and the player below
	SCOPE_OUT_ENTITY_NOT_ADDED_TO_WORLD,			// The entity has not been added to the world yet
	SCOPE_OUT_PLAYER_IN_DIFFERENT_TUTORIAL,			// The player is in a different tutorial
	SCOPE_OUT_NO_GAME_OBJECT,						// The network object has no game object
	SCOPE_OUT_PED_RESPAWN_FAILED,					// The player failed to respawn the ped
	SCOPE_OUT_PED_VEHICLE_NOT_CLONED,				// The ped's vehicle has not been cloned for this player yet
	SCOPE_OUT_PED_RAPPELLING_NOT_CLONED,			// The rappelling ped is out of a vehicle and not cloned yet
	SCOPE_OUT_VEHICLE_GARAGE_INDEX_MISMATCH,		// The vehicle's garage index does not match this players
	SCOPE_OUT_OBJECT_HEIGHT_RANGE,					// The ambient prop is out of range vertically
	SCOPE_OUT_OBJECT_NO_FRAG_PARENT,				// The ambient prop has no frag parent
	SCOPE_OUT_PICKUP_UNINITIALISED,					// The pickup is uninitialised
	SCOPE_OUT_PICKUP_PLACEMENT_DESTROYED,			// The pickup placement is being destroyed
	SCOPE_OUT_VEHICLE_ATTACH_PARENT_OUT_SCOPE,      // The vehicle parent for the trailer is out of scope
	SCOPE_OUT_OBJECT_FRAGMENT_PARENT_OUT_SCOPE,		// The object is a fragment of vehicle that is out of scope now
	SCOPE_OUT_PICKUP_IS_MONEY,						// Never clone money pickups

	// misc
	SCOPE_PLAYER_BUBBLE,							// The player's scope is dependent on whether we are sharing a roaming bubble with this player
	SCOPE_TRAIN_USE_ENGINE_SCOPE,					// The train is using the scope of its engine
	SCOPE_TRAIN_ENGINE_WAIING_ON_CARRIAGES,			// The train engine is waiting for the carriages to remove themselves
	SCOPE_TRAIN_ENGINE_NOT_LOCAL,					// The train engine is remotely owned
	SCOPE_NO_TRAIN_ENGINE,							// The train engine does not exist / is not networked
	SCOPE_PICKUP_USE_PLACEMENT_SCOPE,				// The pickup is using the scope of its placement	
	SCOPE_OUT_PLAYER_CONCEALED_FOR_FLAGGED_PED		// The ped should not be cloned on machines that the owner is concealed to
};

enum
{
    PLAYER_FOCUS_AT_PED,
    PLAYER_FOCUS_AT_EXTENDED_POPULATION_RANGE_CENTER,
    PLAYER_FOCUS_AT_CAMERA_POSITION,
    PLAYER_FOCUS_SPECTATING,
    PLAYER_FOCUS_AT_LOCAL_CAMERA_POSITION
};

enum
{
	APA_NONE_PHYSICAL,
	APA_NONE_PED,
	APA_ROPE_ATTACHED_ENTITY_DIFFER,
	APA_TRAILER_IS_DUMMY,
	APA_TRAILER_PARENT_IS_DUMMY,
	APA_VEHICLE_IS_DUMMY_TRAILER,
	APA_VEHICLE_IS_DUMMY,
	APA_VEHICLE_PARENT_IS_DUMMY_TRAILER,
	APA_VEHICLE_PARENT_IS_DUMMY,
	APA_VEHICLE_TOWARM_ENTITY_DIFFER,
	APA_CARGOBOB_HAS_NO_ROPE_ON_CLONE,
};

#if __BANK

//PURPOSE
// Enumeration of reasons an object has failed to migrate by proximity. Used for tracking far away local ped not migrating to nearer remote players
enum
{
    MFT_SUCCESS,                       // no failure reason
    MFT_REMOTE_NOT_ACCEPTING_OBJECTS,  // closest remote player is not accepting objects
    MFT_INTERIOR_STATE,                // interior checks are preventing the object migrating
    MFT_SCOPE_CHECKS,                  // closest remote player is not in scope with the object
    MFT_SCRIPT_CHECKS,                 // script checks are preventing the object migrating
    MFT_PROXIMITY_CHECKS,              // basic proximity checks are preventing the object migrating
    MFT_CAN_PASS_CONTROL_CHECKS        // CanPassControl() returned false for the closest remote player
};

enum
{
	SPG_SET_GHOST_PLAYER_FLAGS,
	SPG_RAG,
	SPG_PLAYER_STATE,
	SPG_PLAYER_VEHICLE,
	SPG_NON_PARTICIPANTS,
	SPG_TRAILER_INHERITS_GHOSTING,
	SPG_RESET_GHOSTING,
	SPG_RESET_GHOSTING_NON_PARTICIPANTS,
	SPG_VEHICLE_STATE,
	SPG_SCRIPT,
};
#endif // __BANK

#endif // NETWORK_OBJECT_TYPES_H
