#ifndef REPLAYSETTINGS_H
#define REPLAYSETTINGS_H

#define CONSOLE_REPLAY (1 && (RSG_DURANGO || RSG_ORBIS))
#define GTA_REPLAY (1 && (RSG_PC || CONSOLE_REPLAY) && !__GAMETOOL)
#define GTA_REPLAY_OVERLAY				(1 && GTA_REPLAY && __BANK)
#if GTA_REPLAY_OVERLAY
#define GTA_REPLAY_OVERLAY_ONLY(...)	__VA_ARGS__
#else
#define GTA_REPLAY_OVERLAY_ONLY(...)
#endif

#if GTA_REPLAY

#define STRINGIFY(x)	#x

#ifndef DO_REPLAY_OUTPUT
#define DO_REPLAY_OUTPUT 1 && !__NO_OUTPUT
#endif 

#ifndef DO_REPLAY_OUTPUT_XML
#define DO_REPLAY_OUTPUT_XML REPLAY_DEBUG && 0
#endif 

#ifndef INCLUDE_FROM_GAME
#define INCLUDE_FROM_GAME 1
#endif

#define DO_CUSTOM_WORKING_DIR	!__FINAL

#define REPLAY_FAILSAFE 1

#define DO_VERSION_CHECK 1
#if DO_VERSION_CHECK
#define VERSION_CHECK(...)	__VA_ARGS__
#else
#define VERSION_CHECK(...)
#endif // DO_VERSION_CHECK

#if REPLAY_FAILSAFE
	#define REPLAY_ASSERTF	replayAssertf
#else
	#define REPLAY_ASSERTF	replayFatalAssertf
#endif

#define NO_RETURN_VALUE
#define REPLAY_CHECK(cond, errorRet, fmt, ...)	if((cond) == false) \
	{	REPLAY_ASSERTF(false, fmt, ##__VA_ARGS__);	return errorRet; }

#define REPLAY_VIEWER					0

#define REPLAY_ONLY(...)				__VA_ARGS__
#define NOT_REPLAY(...)					

#define MIN_SYS_MEM_TO_ENABLE_REPLAY	( (u32)(1.4f * 1024.0f * 1024.0f * 1024.0f) )

#if RSG_ORBIS
	#define REPLAY_UNUSEAD_VAR(x)				(void)x
#else
	#define REPLAY_UNUSEAD_VAR(x)
#endif

//useful if you are testing and don't want to go through the rigmarole of saving each time
#define USE_SAVE_SYSTEM (0)	//	 1 || !__BANK

#define ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE	(0)

#define DISPLAY_MESSAGE_IF_REPLAY_SAVE_FAILS		(0)

#if ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE
	#define SCRIPT_PREP_ONLY(...)				__VA_ARGS__
#else
	#define SCRIPT_PREP_ONLY(...)
#endif

#define MARKETING_DISABLE_WATERMARK		0
#define MARKETING_REPLAY_FREE_CAMERA	0

#define REPLAY_ALIGN(x, y)	(((x) + (y - 1)) & ~(y - 1))
#define REPLAY_IS_ALIGNED(x, y)	!(x & (y - 1))

// To be tweaked
static const u32 MIN_BYTES_PER_REPLAY_BLOCK		= 1 * 1024 * 1024;
static const u32 MAX_BYTES_PER_REPLAY_BLOCK		= 4 * 1024 * 1024;
static const u16 MIN_NUM_REPLAY_BLOCKS			= 3;
static const u16 MAX_NUM_REPLAY_BLOCKS			= 36;
static const u16 NUMBER_OF_TEMP_REPLAY_BLOCKS	= 6;

static const u32 MIN_REPLAY_STREAMING_SIZE		= 250;
static const u32 MAX_REPLAY_STREAMING_SIZE		= 2048;

static const bool REPLAY_COMPRESSION_ENABLED	= true;

#define MAX_NUMBER_OF_ALLOCATED_BLOCKS  MAX_NUM_REPLAY_BLOCKS + NUMBER_OF_TEMP_REPLAY_BLOCKS

const unsigned int	TRACKABLES	= 256;

const unsigned int	MAX_REPLAY_RECORDING_THREADS = 4;

//default replay memory values -----------------------------

//please don't change them unless you know what you're doing, block sizes needs to be a power of 2
//for it to play nice with the buddy allocator stuff otherwise the memory bloats to a massive size
#define BYTES_IN_REPLAY_BLOCK			(4 * 1024 * 1024)
#define NUMBER_OF_REPLAY_BLOCKS			(7)
#define REPLAY_DEFAULT_ALIGNMENT		16
// ----------------------------------------------------------

#define NUMBER_OF_TRAFFIC_LIGHTS		(50)

#define REPLAY_ROOT_PATH				"replay:/"
#define REPLAY_CLIPS_PATH				"replay:/videos/clips/"
#define REPLAY_MONTAGE_PATH				"replay:/videos/projects/"
#if RSG_PC
#define REPLAY_VIDEOS_PATH				"appdata:/videos/rendered/"
#define REPLAY_DATA_PATH				"appdata:/videos/data/"
#else
#define REPLAY_VIDEOS_PATH				"replay:/videos/rendered/"
#define REPLAY_DATA_PATH				"replay:/videos/data/"
#endif // else RSG_PC

#define REPLAY_MONTAGE_FILE_EXT			".vid"
#define REPLAY_CLIP_FILE_EXT			".clip"

// for use with multiple users sharing storage code
#define DISABLE_PER_USER_STORAGE		(1 && RSG_PC)
#define SEE_ALL_USERS_FILES				CONSOLE_REPLAY

// video upload defines
#define USE_ORBIS_UPLOAD				0

#define REPLAY_DATETIME_STRING_LENGTH	32

//max amount of replay buffer event markers
#define REPLAY_HEADER_EVENT_MARKER_MAX	1

// DO NOT ENABLE THIS BECAUSE IT HURTS PERFORMANCE!
#define CLEAR_REPLAY_MEMORY				0

#define REPLAYFILELENGTH				(64)
#define REPLAYPATHLENGTH				RAGE_MAX_PATH
#define REPLAYDIRMINLENGTH				(3)

// format: 0xmm-dd-hh:mm

#define REPLAY_MIN_FRAMES						(60)
#define REPLAY_PREPLAY_TIME						(2000)
#define REPLAY_PRELOAD_TIME						(4000)		// milliseconds
#define REPLAY_PRELOAD_TIME_MAX					(10000)		// milliseconds
#define REPLAY_LOAD_MODEL_TIME_MAX				(5000.0f)	// milliseconds
#define REPLAY_LOAD_MODEL_TIME_FALLBACK			(1000.0f)	// milliseconds
#define REPLAY_MAX_PRELOADS						544
#define REPLAY_MAX_MS							(90*1000)	// 90sec
#define REPLAY_VOLUMES_COUNT					(9)
#define REPLAY_MONTAGE_MUSIC_FADE_TIME			(5000) // auto fade out time - milliseconds
#define REPLAY_MONTAGE_SCORE_MUSIC_FADEIN_TIME	(1000)
#define REPLAY_MONTAGE_SFX_FADE_TIME			(1000) // auto fade out time - milliseconds
#define REPLAY_MONTAGE_AMBINET_FADE_TIME		(1000) // auto fade out time - milliseconds

#define REPLAY_WAIT_FOR_HD_MODELS		1

#define REPLAY_DEBUG					__BANK			// active in build with bank turn on

#define REPLAY_OVERWATCH				true

#if REPLAY_DEBUG
#define REPLAY_GUARD_ENABLE				1
#else
#define REPLAY_GUARD_ENABLE				1
#endif 

#define REPLAY_SHIFT_BLOCK_ORDER		1				// On saving the oldest session block becomes the first memory block

#if REPLAY_GUARD_ENABLE
#define REPLAY_GUARD_ONLY(...)			__VA_ARGS__
#else
#define REPLAY_GUARD_ONLY(...)
#endif

#define TEMPBUFFERDEBUG(fmt, ...)			replayDebugf3(fmt, ##__VA_ARGS__)
#define BUFFERDEBUG(fmt, ...)				replayDebugf3("BD:" fmt, ##__VA_ARGS__)
#define REPLAYTRACE(fmt, ...)				do {replayDebugf1(fmt, ##__VA_ARGS__);} while (false);
#define PRELOADDEBUG					!__FINAL
#if PRELOADDEBUG
#define PRELOADDEBUG_ONLY(...)			__VA_ARGS__
#else
#define PRELOADDEBUG_ONLY(...)
#endif // PRELOADDEBUG

typedef u32 tPacketSize;
typedef u8 tPacketVersion;
const tPacketVersion InitialVersion = 0;

// Memory optimization flags.
#define REPLAY_USE_MEM_OPTIMIZATIONS					1 // Master switch.

#define REPLAY_VEHICLES_USE_SKELETON_BONE_DATA_PACKET	(1 && REPLAY_USE_MEM_OPTIMIZATIONS)
#define REPLAY_VEHICLES_ALL_WHEELS_IN_ONE_PACKET		(1 && REPLAY_USE_MEM_OPTIMIZATIONS)
#define REPLAY_WHEELS_USE_PACKED_VECTORS				(1 && REPLAY_USE_MEM_OPTIMIZATIONS)
#define REPLAY_DELAYS_QUANTIZATION						(1 && REPLAY_USE_MEM_OPTIMIZATIONS)

//water settings
#define REPLAY_ENABLE_WATER 1
const static u32 REPLAY_WATER_KEYFRAME_FREQ = 500; //in ms

#define REPLAY_WATER_ENABLE_DEBUG (0 && __BANK)
#define REPLAY_WATER_ENABLE_PER_FRAME_REC 1 // (0 && REPLAY_WATER_ENABLE_DEBUG)
#define REPLAY_WATER_QUANTIZE_PACKETS (1 || !REPLAY_WATER_ENABLE_DEBUG)

enum eFramePacketFlags
{	// **** If you edit this please also edit 'framePacketFlagStrings' below ****
	FRAME_PACKET_DISABLE_CAMERA_MOVEMENT = (1 << 0),
	FRAME_PACKET_RECORDING_DISABLED = (1 << 1),
	FRAME_PACKET_RECORDED_CUTSCENE = (1 << 2),
	FRAME_PACKET_RECORDED_FIRST_PERSON = (1 << 3),
	FRAME_PACKET_RECORDED_CAM_IN_VEHICLE = (1 << 4),
	FRAME_PACKET_RECORDED_CAM_IN_VEHICLE_VFX = (1 << 5),
	FRAME_PACKET_RECORDED_MULTIPLAYER = (1 << 6),
	FRAME_PACKET_RECORDED_USE_AIRCRAFT_SHADOWS = (1 << 7),
	FRAME_PACKET_RECORDED_DIRECTOR_MODE = (1 << 8)
};

static const char* framePacketFlagStrings[] =
{
	"Disable Camera Movement",
	"Recording Disabled",
	"Recorded Cutscene",
	"Recorded First Person", 
	"Recorded Camera In Vehicle",
	"Recorded Camera In Vehicle VFX",
	"Recorded Multiplayer",
	"Recorded Force Aircraft Shadows",
	"Recorded Director Mode"
};	

#define REPLAY_FORCE_LOAD_SCENE_DURING_EXPORT	(1)
#define USE_SRLS_IN_REPLAY						(0)
#define REPLAY_JUMP_EPSILON (0.001f)

#else // GTA_REPLAY

#define REPLAY_VIEWER					 0
#define REPLAY_WATER_ENABLE_DEBUG 		 0

#define REPLAY_ONLY(...)
#define NOT_REPLAY(...)					__VA_ARGS__

#define SEE_ALL_USERS_FILES				0

#endif // GTA_REPLAY

#endif // REPLAYSETTINGS_H
