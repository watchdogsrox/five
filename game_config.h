//
// name:		game_config.h
// description: Place your preprocessor defines in here. This file is used in both core code and game project
//
#ifndef INC_GAME_CONFIG_H_
#define INC_GAME_CONFIG_H_

#include "optimisations.h"

// warning we want for jimmy project that Rage isn't ready for yet
#ifdef __SNC__
#pragma diag_warning 828	//  unreferenced_function_param, entity-kind "entity" was never referenced
#endif

#if RSG_ORBIS
// #pragma GCC diagnostic ignored "-Wlogical-op-parentheses"
// #pragma GCC diagnostic ignored "-Wformat-security"
// #pragma GCC diagnostic ignored "-Wdeprecated-writable-strings"
#endif

// Make sure HACK_RDR3 has a definition, so we can use it with #if rather than #ifdef
// for any RDR3-specific code changes.
#ifndef HACK_RDR3
#define HACK_RDR3 0
#endif

#define XENON_OR_PS3 (RSG_XENON || RSG_PS3)

// Physics
#define PHYSIC_REQUEST_LIST_SIZE				(XENON_OR_PS3? 1200 : 8192)
#define MAX_ACTIVE_INTERIOR_PHYSICS				(XENON_OR_PS3 ? 1700 : 2400) // bumped this on win32pc only because we were hitting the limit in the heightmap/navmesh tool

// Streaming Pools
#define MAX_ACTIVE_ENTITIES						(XENON_OR_PS3? 6144 : 61440)
#define MAX_NEEDED_ENTITIES						(XENON_OR_PS3? 3072 : 12288)
#define MAX_BASE_HANDLES				

// Timebars / Telemetry
#define MAIN_THREAD_MAX_TIMEBARS				(8192)

// Graphics
#define DESIRED_FRAME_RATE						(RSG_PC ? 60 : 30)
#define	DISTANTLIGHTS_VERTEXBUFFER_MAX_BATCHES	(XENON_OR_PS3 ? 30 : 64)	// the max number of draw calls we can make in one frame
#define MAX_HAIR_ORDER							(XENON_OR_PS3 ? 20 : 48)

// Vehicle/Peds/Spawning
#define MAX_NUM_IN_LOADED_MODEL_GROUP			(XENON_OR_PS3 ? 64 : 256)
#define SCENARIO_MAX_POINTS_IN_AREA				(XENON_OR_PS3 ? 2000 : 4096)
#define SCENARIO_MAX_SPAWNED_VEHICLES			(XENON_OR_PS3 ? 32 : 64)
#define SCENARIO_MAX_SPAWNED_PED				(XENON_OR_PS3 ? 128 : 256)
#define MAX_GENERATION_LINKS					(XENON_OR_PS3 ? 1600 : 4096)
#define MAX_VEHICLES_AT_JUNCTION				(XENON_OR_PS3 ? 64 : 128)
#define MAX_TRAFFIC_LIGHT_INFOS					(XENON_OR_PS3 ? 85 : 512)

#define JUNCTIONS_MAX_NUMBER					(XENON_OR_PS3 ? 60 : 128)		// Maximum number of junctions active at any one time.


// Attachments (this might need increasing as Peds and Vehicles pools are increased)!
#define MAX_NUM_POST_PRE_RENDER_ATTACHMENTS		(XENON_OR_PS3 ? 200 : 200)

#endif // INC_GAME_CONFIG_H_