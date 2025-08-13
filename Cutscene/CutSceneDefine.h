/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutSceneDefine.h
// PURPOSE : A place to store all cut scene consts
// AUTHOR  : Thomas French
// STARTED : 20/07/2009
//
/////////////////////////////////////////////////////////////////////////////////

#include "cutscene/cutsentity.h"

#ifndef CUTSCENE_DEFINE_H 
#define CUTSCENE_DEFINE_H 

//Store 
#define MAX_CUTSCENE_OBJECTS 100

//Bones Object
#define MAX_BONES_TO_REMOVE	(10)

//Particle effects and light names
#define MAX_EFFECT_NAME (42)

#define MAX_CUTSCENE_OBJECT_NAME_LENGTH (17)

#define MAX_ANIM_NAME (30)  

#define MAX_FRAMES	(100000)

#define VEHICLE_HEIRACHY_MODIFIER (8873)	//This is the U16Hash of "extra_1" - 1 need to calculate its position in a vehicles hierarchy

#define NUM_OF_AUDIO_BUFFERS (2)

//Temp define for a different structure of where the rage cut scene entities 
//own the cut scene objects.

#define CUTSCENE_ACTOR_IS_A_CPED 0

#define MAX_NUM_CORNERS (4)

#define MAX_NUMBER_OVERLAY_PARAMS (7)

#define MAX_NUMBER_OF_PARAMS_PER_COMMAND (5)

#define MIN_CUTSCENE_SKIP_TIME (2.0f)

#define INVALID_PLAYBACK_FLAG (-1)

#define DEFAULT_STREAMING_OFFSET (10.0f) //streaming offset in seconds ahead of the play back time 

#define DEFAULT_AUDIO_STREAMING_OFFSET (3.0f)

#define PED_VARIATION_STREAMING_OFFSET (5.0f)

#define MAX_STREAMING_TIME_BEFORE_PLAYING (30000)

enum eCutSceneParticleEffectProperty  
{
	CUTSCENE_ANIMATED_PARTICLE_EFFECT,
	CUTSCENE_TRIGGERED_PARTICLE_EFFECT
};

enum eCutSceneGameEntity
{
	CUTSCENE_PROP_GAME_ENITITY = CUTSCENE_NUM_ENTITY_TYPES, 
	CUTSCENE_WEAPON_GAME_ENTITY,
	CUTSCENE_ACTOR_GAME_ENITITY,
	CUTSCENE_VEHICLE_GAME_ENITITY,
	CUTSCENE_CAMERA_GAME_ENITITY, 
	CUTSCENE_FADE_GAME_ENITITY, 
	CUTSCENE_LIGHT_GAME_ENITITY,
	CUTSCENE_PARTICLE_EFFECT_GAME_ENTITY,
	CUTSCENE_SOUND_GAME_ENTITY,
	CUTSCENE_BLOCKING_BOUND_GAME_ENTITY, 
	CUTSCENE_HIDDEN_OBJECT_GAME_ENTITY, 
	CUTSCENE_FIX_UP_OBJECT_GAME_ENTITY,
	CUTSCENE_SUBTITLE_GAME_ENTITY,
	CUTSCENE_SCALEFORM_OVERLAY_GAME_ENTITY, 
	CUTSCENE_BINK_OVERLAY_GAME_ENTITY, 
	CUTSCENE_DECAL_GAME_ENTITY, 
	CUTSCENE_MAX_NUMBER_GAME_ENTITY
};

enum eStreamingRequestReturnValue
{
	ESRRV_INVALID_COMPONENT,
	ESRRV_ALREADY_REQUESTED,
	ESRRV_INVALID_VARIATION,
	ESRRV_SUCCESSFULL_REQUEST
};

#endif


