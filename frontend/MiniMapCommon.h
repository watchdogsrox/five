#ifndef __MINIMAP_COMMON_H__
#define __MINIMAP_COMMON_H__

#if defined GTA_VERSION && GTA_VERSION > 500
#define USE_LOCALISED_STRING 1
#else
#define USE_LOCALISED_STRING 0
#endif


// rage:
#include "scaleform/scaleform.h"
#include "system/poolallocator.h"
#include "vector/vector2.h"
#include "fwtl/Pagedpool.h"

#if USE_LOCALISED_STRING
#include "fwlocalisation/localisedString.h"
#else
#include "atl/string.h" // I'd prefer localisedString, but that doesn't exist on dev_network
#endif

// game:
#include "frontend/hud_colour.h"
#include "scene/RegdRefTypes.h"
#include "text/TextFile.h"

#if RSG_PC 
	#define MAX_FOW_TARGETS	(3 /*+ MAX_GPUS*/)
	#define NUM_FOW_TARGETS (3 /*+ GRCDEVICE.GetGPUCount()*/)
#endif // RSG_PC

#define NUMBER_OF_SUPERTILE_ROWS		(9)
#define NUMBER_OF_SUPERTILE_COLUMNS		(8)
#define WIDTH_OF_SUPERTILE				(6)
#define HEIGHT_OF_SUPERTILE				(6)

#define WIDTH_OF_DWD_SUPERTILE				(3)
#define HEIGHT_OF_DWD_SUPERTILE				(3)

#define MINIMAP_WORLD_TILE_SIZE_X	(NUMBER_OF_SUPERTILE_COLUMNS * WIDTH_OF_SUPERTILE)
#define MINIMAP_WORLD_TILE_SIZE_Y	(NUMBER_OF_SUPERTILE_ROWS * HEIGHT_OF_SUPERTILE)


#define MIN_MINIMAP_RANGE			(18.0f)
#define MAX_MINIMAP_RANGE			(900.0f)

#define MINIMAP_DISTANCE_SCALER_3D	(2500.0f)
#define MINIMAP_DISTANCE_SCALER_2D	(1000.0f)
#define MINIMAP_HEIGHT_SCALER		(2.0f)
#define MAX_CAPPED_DISTANCE			(0.3f)



#define MAX_BLIP_NAME_SIZE			(80)  // max chars in the blip name

#if GTA_VERSION >= 500
#define MAX_NUM_BLIPS				(1500) // because of UGC, they think they want a ludicrous number of these. So let's oblige them.
#else
#define MAX_NUM_BLIPS				(920)  // max number of blips allowed to be created at any one time (not necessarily displayed!)
#endif

#define ENABLE_FOG_OF_WAR			(1)

#if ENABLE_FOG_OF_WAR
	#define FOG_OF_WAR_ONLY(x) x
#else
	#define FOG_OF_WAR_ONLY(x)
#endif

#define FOG_OF_WAR_RT_SIZE			(128)

#define DEFAULT_BLIP_ROTATION		(360.0f)

enum eZoomLevel
{
	ZOOM_LEVEL_0 = 0,  // initial entry in SP
	ZOOM_LEVEL_1,  // max zoomed out
	ZOOM_LEVEL_2,
	ZOOM_LEVEL_3,
	ZOOM_LEVEL_4,  // max zoomed in

	// additional zoom levels can be set under here:
	ZOOM_LEVEL_GOLF_COURSE,
	ZOOM_LEVEL_INTERIOR,
	ZOOM_LEVEL_GALLERY,
	ZOOM_LEVEL_GALLERY_MAXIMIZE,

	MAX_ZOOM_LEVELS
};

#define MAX_ZOOMED_OUT_PROLOGUE (ZOOM_LEVEL_2)
#define MAX_ZOOMED_OUT (ZOOM_LEVEL_1)
#define MAX_ZOOMED_IN (ZOOM_LEVEL_4)

// flags for properties of blips
enum eBLIP_PROPERTY_FLAGS
{
	BLIP_FLAG_BRIGHTNESS = 1,						// whether it is to be drawn bright or standard colours
	BLIP_FLAG_FLASHING,								// whether it should flash
	BLIP_FLAG_SHORTRANGE,							// short range or long range
	BLIP_FLAG_ROUTE,								// whether it is to have a GPS route attached to it
	BLIP_FLAG_SHOW_HEIGHT,							// whether to show the height indicator arrows flashing on a blip
	BLIP_FLAG_MARKER_LONG_DIST,						// whether markers are set to draw at long distances or not (ideal for races with high speed vehicles)
	BLIP_FLAG_MINIMISE_ON_EDGE,						// whether this blip should go small on edge of map
	BLIP_FLAG_DEAD,									// whether this blip is considered "dead"
	BLIP_FLAG_USE_EXTENDED_HEIGHT_THRESHOLD,		// If true then use a larger vertical distance before displaying the up/down arrows
	BLIP_FLAG_CREATED_FOR_RELATIONSHIP_GROUP_PED,	// Added to a ped belonging to a relationship group on which SET_BLIP_PEDS_IN_RELATIONSHIP_GROUP(TRUE) has been called
	BLIP_FLAG_SHOW_CONE,							// whether to show the direction cone on a blip
	BLIP_FLAG_MISSION_CREATOR,						// whether this blip should go small on edge of map
	BLIP_FLAG_HIGH_DETAIL,							// whether this blip is high detail (true) or low detail (false) for the pausemap legend
	BLIP_FLAG_HIDDEN_ON_LEGEND,						// whether to display this blip on the legend
	BLIP_FLAG_SHOW_TICK,							// whether to show the tick indicator on a blip
	BLIP_FLAG_SHOW_GOLD_TICK,						// whether to show the gold tick indicator on a blip
	BLIP_FLAG_SHOW_FOR_SALE,						// whether to show the for sale indicator ($) on a blip
	BLIP_FLAG_SHOW_HEADING_INDICATOR,				// whether to show the direction indicator on a blip
	BLIP_FLAG_SHOW_OUTLINE_INDICATOR,				// whether to show the outline indicator (used by some missions, set by an old FRIEND INDICATOR command)
	BLIP_FLAG_SHOW_FRIEND_INDICATOR,				// whether to show the friend indicator
	BLIP_FLAG_SHOW_CREW_INDICATOR,					// whether to show the crew indicator
	BLIP_FLAG_USE_HEIGHT_ON_EDGE,					// If true then always show the height indicator even if off the edge of the minimap
	BLIP_FLAG_HOVERED_ON_PAUSEMAP,
	BLIP_FLAG_USE_SHORT_HEIGHT_THRESHOLD,			// If true then use a shorter vertical distance before displaying the up/down arrows

	MAX_BLIP_PROPERTY_FLAGS
};


// flags for actions on blips to update them on the stage
enum eBLIP_TRIGGER_FLAG
{
	BLIP_FLAG_VALUE_CHANGED_COLOUR = 33,	// when a colour changes
	BLIP_FLAG_VALUE_CHANGED_FLASH,			// when the flash changes
	BLIP_FLAG_VALUE_REINIT_STAGE,			// when blip must be re-initialised on the stage
	BLIP_FLAG_VALUE_REMOVE_FROM_STAGE,		// when blip must be removed from the stage prior to destroying the object
	BLIP_FLAG_VALUE_DESTROY_BLIP_OBJECT,	// when blip object is to completely destoryed
	BLIP_FLAG_ON_STAGE,						// set if the blip was on the stage last time the minimap was rendered
	BLIP_FLAG_VALUE_CHANGED_PULSE,			// blip flagged to pulse
	BLIP_FLAG_UPDATE_ALPHA_ONLY,			// update alpha directly without invoking SET_RADIUS_BLIP_COLOUR
	BLIP_FLAG_VALUE_SET_NUMBER,				// when to reinitialise the number shown on a blip
	BLIP_FLAG_EXECUTED_VIA_DLC,				// will be TRUE once the blip has been executed on the drawlist
	BLIP_FLAG_EXECUTED_VIA_DLC_LAST_FRAME,	// when blip was sent to the renderthread last frame
	BLIP_FLAG_UPDATE_STAGE_DEPTH,		// blip has had its priority (or hover state) tweaked and needs its depth adjusted on the stage
	BLIP_FLAG_VALUE_CHANGED_TICK,			// when the tick changes and needs to be updated
	BLIP_FLAG_VALUE_CHANGED_GOLD_TICK,		// when the gold tick changes and needs to be updated
	BLIP_FLAG_VALUE_CHANGED_FOR_SALE,		// when the for sale icon on a blip should be updated
	BLIP_FLAG_VALUE_CHANGED_OUTLINE_INDICATOR,
	BLIP_FLAG_VALUE_CHANGED_FRIEND_INDICATOR,
	BLIP_FLAG_VALUE_CHANGED_CREW_INDICATOR,
	BLIP_FLAG_UPDATED_LOW_FREQ_FLAGS,		// optimization; true when we've thrown a few less common flags
	BLIP_FLAG_VALUE_TURNED_OFF_SHOW_HEIGHT,	// when the SHOW_HEIGHT property is turned off - used to make sure the height blip is returned to level

#if __BANK
	BLIP_FLAG_VALUE_SET_LABEL,				// when the label changes
	BLIP_FLAG_VALUE_REMOVE_LABEL,			// when the label changes
#endif // __BANK

	MAX_BLIP_TRIGGER_FLAGS
};

#define MAX_BLIP_FLAGS (MAX_BLIP_TRIGGER_FLAGS)

enum eBLIP_FLAG_TYPES
{
	BLIP_FLAG_PROPERTY = 0,
	BLIP_FLAG_TRIGGER,
	MAX_BLIP_FLAG_TYPES
};

enum
{
	BLIP_CATEGORY_INVALID = 0,
	BLIP_CATEGORY_LOCAL_PLAYER,
	BLIP_CATEGORY_WAYPOINT,

	BLIP_CATEGORY_START_OF_SCRIPTED_CATEGORIES,

	//
	// scripted categories are slot in here from script
	//

	BLIP_CATEGORY_JOBS = BLIP_CATEGORY_START_OF_SCRIPTED_CATEGORIES,		// jobs 
	BLIP_CATEGORY_MY_JOBS,		// my jobs 
	BLIP_CATEGORY_MISSION,		// mission blips
	BLIP_CATEGORY_ACTIVITY,		// activity blips
	BLIP_CATEGORY_PLAYER,		// other players  // if this changes from '7' please notify code
	BLIP_CATEGORY_SHOP,			// shops
	BLIP_CATEGORY_RACE,			// races
	BLIP_CATEGORY_PROPERTY,		// property
	BLIP_CATEGORY_APARTMENT,	// apartment

	BLIP_CATEGORY_OTHER = (MAX_UINT8)
};

#define MAX_LENGTH_OF_MINIMAP_FILENAME		(100)


// Forward declarations
class CEntity;
class CPickupPlacement;


// RDR has switched over to a hashed enum for BlipIds instead of a linear enum
// because GTAV is live and GTAVNG is a separate beast, this change won't work there.
#if defined RDR_VERSION && RDR_VERSION >= 300
#define HASHED_BLIP_IDS 1
#else
#define HASHED_BLIP_IDS 0
#endif

#if HASHED_BLIP_IDS
typedef atHashWithStringBank BlipLinkage;
#else
typedef s32 BlipLinkage;
#endif

//IMPORTANT: Keep in sync with BLIP_SPRITE in commands_hud.sch 
enum eBLIP_NAMES
{
	//
	// CODE BLIPS BELOW HERE:
	//
	BLIP_HIGHER = 0,
	BLIP_LEVEL,
	BLIP_LOWER,

	BLIP_POLICE_PED,
	BLIP_WANTED_RADIUS,
	BLIP_AREA,

	BLIP_CENTRE,
	BLIP_NORTH,
	BLIP_WAYPOINT,

	BLIP_RADIUS,
	BLIP_RADIUS_OUTLINE,

	BLIP_WEAPON_HIGHER,
	BLIP_WEAPON_LOWER,

	BLIP_AI_HIGHER,
	BLIP_AI_LOWER,

	//
	// SCRIPT BLIPS BELOW HERE:
	//
	BLIP_POLICE_HELI_SPIN,    // if this is changed and no longer 15, then change the start of the BLIPS enum in commands_hud.sch and inform Gareth Evans to change ActionScript
	BLIP_POLICE_PLANE_MOVE,
	RADAR_TRACE_NUMBERED_1,
	RADAR_TRACE_NUMBERED_2,
	RADAR_TRACE_NUMBERED_3,
	RADAR_TRACE_NUMBERED_4,
	RADAR_TRACE_NUMBERED_5,

	MAX_CODE_BLIPS,

	BLIP_AREA_OUTLINE = 482	// Do not expose to script so exist within the enum but outside the concerns of script
};

// Blip Priority Type
enum eBLIP_PRIORITY_TYPE
{
	BLIP_PRIORITY_GANG_HOUSE = 0,
	BLIP_PRIORITY_WEAPON,
	BLIP_PRIORITY_SHOP,
	BLIP_PRIORITY_DYING,
	BLIP_PRIORITY_DEFAULT,
	BLIP_PRIORITY_OTHER_TEAM,
	BLIP_PRIORITY_SAME_TEAM,
	BLIP_PRIORITY_CUFFED_OR_KEYS,
	BLIP_PRIORITY_ENEMY_AI,
	BLIP_PRIORITY_PLAYER_NOISE,
	BLIP_PRIORITY_HIGHLIGHTED, 
	BLIP_PRIORITY_WANTED
};

// Blip Priorities
enum eBLIP_PRIORITY
{
	BLIP_PRIORITY_LOW_SPECIAL = 0,  // used for the centre icon when it should appear below everything
	BLIP_PRIORITY_LOWEST,
	BLIP_PRIORITY_LOW_LOWEST,
	BLIP_PRIORITY_LOW,	// special cases - some blips must appear below all others
	BLIP_PRIORITY_LOW_MED,
	BLIP_PRIORITY_MED,		// default for any sprite blips
	BLIP_PRIORITY_MED_HIGH,
	BLIP_PRIORITY_HIGH,		// default for any mission (LEVEL/BELOW/ABOVE) blips
	BLIP_PRIORITY_HIGH_HIGHEST,
	BLIP_PRIORITY_HIGHEST,	// special cases - some blips must appear above all others
	BLIP_PRIORITY_HIGHEST_SPECIAL_LOW,  // horrible new priority added in for 1730606
	BLIP_PRIORITY_HIGHES_SPECIAL_MED,  // horrible new priority added in for 1730606
	BLIP_PRIORITY_HIGHEST_SPECIAL_HIGH,  // horrible new priority added in for 1730606
	BLIP_PRIORITY_HIGHEST_SPECIAL,	// used for the centre icon that should appear above everything
	BLIP_PRIORITY_ONTOP_OF_EVERYTHING,
	BLIP_PRIORITY_MAX
};
CompileTimeAssert(BLIP_PRIORITY_MAX < 256);

enum eBLIP_TYPE
{
	BLIP_TYPE_UNUSED = 0,
	BLIP_TYPE_CAR,
	BLIP_TYPE_CHAR,
	BLIP_TYPE_OBJECT,
	BLIP_TYPE_COORDS,
	BLIP_TYPE_CONTACT,
	BLIP_TYPE_PICKUP,
	BLIP_TYPE_RADIUS,
	BLIP_TYPE_WEAPON_PICKUP,
	BLIP_TYPE_COP,
	BLIP_TYPE_STEALTH,
	BLIP_TYPE_AREA,
	BLIP_TYPE_CUSTOM,
	BLIP_TYPE_PICKUP_OBJECT,
	BLIP_TYPE_MAX
};
CompileTimeAssert(BLIP_TYPE_MAX < 128); // only 7 bits for packing

enum eBLIP_DISPLAY_TYPE
{
	BLIP_DISPLAY_NEITHER = 0,
	BLIP_DISPLAY_MARKERONLY,
	BLIP_DISPLAY_BLIPONLY,
	BLIP_DISPLAY_PAUSEMAP,
	BLIP_DISPLAY_BOTH,
	BLIP_DISPLAY_MINIMAP,
	BLIP_DISPLAY_PAUSEMAP_ZOOMED,
	BLIP_DISPLAY_BIGMAP_FULL_ONLY,
	BLIP_DISPLAY_CUSTOM_MAP_ONLY,
	BLIP_DISPLAY_MINIMAP_OR_BIGMAP,
	BLIP_DISPLAY_MAX
};
CompileTimeAssert(BLIP_DISPLAY_MAX < 256);

enum eBLIP_COLOURS
{
	// the 1st set of these are called by scripters:
	BLIP_COLOUR_DEFAULT = 0,
	BLIP_COLOUR_RED,
	BLIP_COLOUR_GREEN,
	BLIP_COLOUR_BLUE,
	BLIP_COLOUR_WHITE,
	BLIP_COLOUR_YELLOW,
	BLIP_COLOUR_NET_PLAYER1,
	BLIP_COLOUR_NET_PLAYER2,
	BLIP_COLOUR_NET_PLAYER3,
	BLIP_COLOUR_NET_PLAYER4,
	BLIP_COLOUR_NET_PLAYER5,
	BLIP_COLOUR_NET_PLAYER6,
	BLIP_COLOUR_NET_PLAYER7,
	BLIP_COLOUR_NET_PLAYER8,
	BLIP_COLOUR_NET_PLAYER9,
	BLIP_COLOUR_NET_PLAYER10,
	BLIP_COLOUR_NET_PLAYER11,
	BLIP_COLOUR_NET_PLAYER12,
	BLIP_COLOUR_NET_PLAYER13,
	BLIP_COLOUR_NET_PLAYER14,
	BLIP_COLOUR_NET_PLAYER15,
	BLIP_COLOUR_NET_PLAYER16,
	BLIP_COLOUR_NET_PLAYER17,
	BLIP_COLOUR_NET_PLAYER18,
	BLIP_COLOUR_NET_PLAYER19,
	BLIP_COLOUR_NET_PLAYER20,
	BLIP_COLOUR_NET_PLAYER21,
	BLIP_COLOUR_NET_PLAYER22,
	BLIP_COLOUR_NET_PLAYER23,
	BLIP_COLOUR_NET_PLAYER24,
	BLIP_COLOUR_NET_PLAYER25,
	BLIP_COLOUR_NET_PLAYER26,
	BLIP_COLOUR_NET_PLAYER27,
	BLIP_COLOUR_NET_PLAYER28,
	BLIP_COLOUR_NET_PLAYER29,
	BLIP_COLOUR_NET_PLAYER30,
	BLIP_COLOUR_NET_PLAYER31,
	BLIP_COLOUR_NET_PLAYER32,
	BLIP_COLOUR_FREEMODE,
	BLIP_COLOUR_INACTIVE_MISSION,
	BLIP_COLOUR_STEALTH_GREY,
	BLIP_COLOUR_WANTED,

	BLIP_COLOUR_MICHAEL,
	BLIP_COLOUR_FRANKLIN,
	BLIP_COLOUR_TREVOR,

	BLIP_COLOUR_GOLF_P1,
	BLIP_COLOUR_GOLF_P2,
	BLIP_COLOUR_GOLF_P3,
	BLIP_COLOUR_GOLF_P4,

	BLIP_COLOUR_FRIENDLY,
	BLIP_COLOUR_PURPLE,
	BLIP_COLOUR_ORANGE,
	BLIP_COLOUR_GREENDARK,
	BLIP_COLOUR_BLUELIGHT,
	BLIP_COLOUR_BLUEDARK,
	BLIP_COLOUR_GREY,
	BLIP_COLOUR_YELLOWDARK,
	BLIP_COLOUR_HUDCOLOUR_BLUE,
	BLIP_COLOUR_PURPLEDARK,
	BLIP_COLOUR_HUDCOLOUR_RED,
	BLIP_COLOUR_HUDCOLOUR_YELLOW,
	BLIP_COLOUR_PINK,
	BLIP_COLOUR_HUDCOLOUR_GREYLIGHT,

	BLIP_COLOUR_GANG1,
	BLIP_COLOUR_GANG2,
	BLIP_COLOUR_GANG3,

	// the ones below here are not set by the scripters:
	BLIP_COLOUR_COORD,
	BLIP_COLOUR_CONTACT,
	BLIP_COLOUR_OBJECT,
	BLIP_COLOUR_MISSION_OBJECT,
	BLIP_COLOUR_CHECKPOINT,
	BLIP_COLOUR_DESTINATION,
	BLIP_COLOUR_GANGS,
	BLIP_COLOUR_MISSION_MARKER,
	BLIP_COLOUR_MISSION_ARROW,
	BLIP_COLOUR_POLICE,
	BLIP_COLOUR_POLICE_INACTIVE,
	BLIP_COLOUR_POLICE2,
	BLIP_COLOUR_POLICE2_INACTIVE,
	BLIP_COLOUR_POLICE_RADAR_RED,
	BLIP_COLOUR_POLICE_RADAR_BLUE,
	BLIP_COLOUR_FRONTEND_YELLOW,
	BLIP_COLOUR_SIMPLEBLIP_DEFAULT,
	BLIP_COLOUR_WAYPOINT,

	BLIP_COLOUR_USE_COLOUR32,
	BLIP_COLOUR_MAX
};
CompileTimeAssert(BLIP_COLOUR_MAX < 256);

enum eSETUP_STATE
{
	MINIMAP_MODE_STATE_NONE = 0,
	MINIMAP_MODE_STATE_SETUP_FOR_MINIMAP,
	MINIMAP_MODE_STATE_SETUP_FOR_PAUSEMAP,
	MINIMAP_MODE_STATE_SETUP_FOR_BIGMAP,
	MINIMAP_MODE_STATE_SETUP_FOR_CUSTOMMAP,
	MINIMAP_MODE_STATE_SETUP_FOR_GOLFMAP,
	MINIMAP_MODE_STATE_SETUP_FOR_MINIMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD,
	MINIMAP_MODE_STATE_SETUP_FOR_PAUSEMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD,
	MINIMAP_MODE_STATE_SETUP_FOR_BIGMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD,
	MINIMAP_MODE_STATE_SETUP_FOR_CUSTOMMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD,
	MINIMAP_MODE_STATE_SETUP_FOR_GOLFMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD
};


enum eGOLF_COURSE_HOLES
{
	GOLF_COURSE_OFF = 0,
	GOLF_COURSE_HOLE_START = 1,
	GOLF_COURSE_HOLE_END = 9,
	GOLF_COURSE_HOLE_ALL
};

enum eALTIMETER_MODES
{
	ALTIMETER_OFF,
	ALTIMETER_FLYING,
	ALTIMETER_SWIMMING
};

// an object that stores a hash for a string table entry and sometimes a literal to use instead
struct atLocValue
{
	atLocValue() {};
#if USE_LOCALISED_STRING
	const char* GetStr() const							{ return !m_aAsString.IsNullOrEmpty() ? m_aAsString.GetData() : TheText.Get(m_uAsHash.GetHash(), NULL); }
#else
	const char* GetStr() const							{ return m_aAsString.GetLength()>0 ? m_aAsString.c_str() : TheText.Get(m_uAsHash.GetHash(), NULL); }
#endif

	void SetKey(const atHashWithStringBank& key)		{ m_uAsHash = key;					m_aAsString.Clear(); }
	void SetKey(const char* locKey)						{ m_uAsHash.SetFromString(locKey);	m_aAsString.Clear(); }
#if USE_LOCALISED_STRING
	void SetLoc(const char* utf8String)				{ m_uAsHash.SetFromString(utf8String); m_aAsString.Set(utf8String); } // for purposes of comparing these, hash the literal too.
#else
	void SetLoc(const char* utf8String)				{ m_uAsHash.SetFromString(utf8String); m_aAsString = utf8String; } // for purposes of comparing these, hash the literal too.
#endif

	void Reset()										{ m_uAsHash.Clear(); m_aAsString.Reset(); }


	bool IsNull() const									{ return m_uAsHash.IsNull(); } // because m_aAsString is hashed too, we can just compare this

	bool operator==(const atLocValue& src) const	{ return m_uAsHash == src.m_uAsHash && m_aAsString == src.m_aAsString; }
	bool operator!=(const atLocValue& src) const	{ return m_uAsHash != src.m_uAsHash || m_aAsString != src.m_aAsString; }

	// these may need to handle comparing a hash vs a locstring that hashes to the same value (improbable?)
	bool operator==(const char* val) const				{ return m_uAsHash == val; }
	bool operator!=(const char* val) const				{ return !(m_uAsHash == val); }// nobody's defined the != yet? 

private:
#if USE_LOCALISED_STRING
	fwLocalisedString	m_aAsString;
#else
	atString			m_aAsString;
#endif
	atHashWithStringBank m_uAsHash;
};

struct sMiniMapInterior
{
	u32 uHash;
	Vector3 vPos;
	Vector3 vBoundMin;
	Vector3 vBoundMax;
	float fRot;
	s32 iLevel;

	sMiniMapInterior();
	void Clear();
	bool ResolveDifferentHash(sMiniMapInterior& newData);
	bool ResolveDifferentOrientation(sMiniMapInterior& newData);
	bool ResolveDifferentLevels(sMiniMapInterior& newData);

	static const s32 INVALID_LEVEL = -99;
#if __BANK
	static const s32 DEBUG_INVALID_LEVEL = -98;
#endif
};


struct sMiniMapRenderStateStruct
{
	// player info:
	Vector3 m_vCentrePosition;
	float fRoll;
	float m_fPlayerVehicleSpeed;

	Vector2 m_vCurrentMiniMapPosition;
	Vector2 m_vCurrentMiniMapSize;

	Vector2 m_vCurrentMiniMapMaskPosition;
	Vector2 m_vCurrentMiniMapMaskSize;

	Vector2 m_vCurrentMiniMapBlurPosition;
	Vector2 m_vCurrentMiniMapBlurSize;

	float m_fMaxAltimeterHeight;
	float m_fMinAltimeterHeight;

	//	These are used in UpdateMapPositionAndScale
	float m_fAngle;
	float m_fOffset;
	Vector3 m_vMiniMapPosition;
	float m_fMiniMapRange;
	s32 m_iRotation;

	// interior info:
	sMiniMapInterior m_Interior;

	float m_fPauseMapScale;

	s32 m_iScriptZoomValue;

#if ENABLE_FOG_OF_WAR
#define MAX_FOG_SCRIPT_REVEALS 8
	Vector2 m_vPlayerPos;
	Vector2 m_ScriptReveal[MAX_FOG_SCRIPT_REVEALS];
	int	m_numScriptRevealRequested;	
	bool m_bUpdateFoW;
	bool m_bUpdatePlayerFoW;
	bool m_bShowFow;
	bool m_bClearFoW;
	bool m_bRevealFoW;
	bool m_bUploadFoWTextureData;
#endif // ENABLE_FOG_OF_WAR

	Vector2 m_vTilesAroundPlayer;
	Vector2 m_vCentreTileForPlayer;

	eSETUP_STATE m_MinimapModeState;
	eGOLF_COURSE_HOLES m_CurrentGolfMap;
	eALTIMETER_MODES	m_AltimeterMode;

	bool m_bShowSonarSweep;

	char m_CurrentMiniMapFilename[MAX_LENGTH_OF_MINIMAP_FILENAME];

	eHUD_COLOURS m_eFlashColour;
	u32			 m_uFlashStartTime;
	bool m_bFlashWantedOverlay;
	bool bInCreator;
	bool bInsideInterior;
	bool bIsInParachute;
	bool bDeadOrArrested;
	bool bInPlaneOrHeli;
	bool bDrawCrosshair;
	bool m_bIsInBigMap;
	bool m_bBigMapFullZoom;
	bool m_bIsInPauseMap;
	bool m_bLockedToDistanceZoom;
	bool m_bIsInCustomMap;
	bool m_bIsActive;
	bool m_bBackgroundMapShouldBeHidden;
	bool m_bShowPrologueMap;
	bool m_bShowIslandMap;
	bool m_bShowMainMap;
	bool m_bHideInteriorMap;
	bool m_bBitmapOnly;
	bool m_bShouldProcessMiniMap;
	bool m_bShouldRenderMiniMap;
	bool m_bRangeLockedToBlip;
	bool m_bInsideReappearance;
	bool m_bDisplayPlayerNames;
	bool m_bColourAltimeter;
	eHUD_COLOURS m_AltimeterColor;

#if __BANK
//	s32 m_iDebugGolfCourseMap;
	bool m_bDisplayAllBlipNames;
	bool m_bHasDebugInterior;
#endif

	sMiniMapRenderStateStruct();
};

struct sSonarBlipStruct
{
	s32 iId;
	Vector2 vPos;
	float fSize;
	eHUD_COLOURS iHudColour;
	u8 iFramesAlive;
	RegdPed pPed;  // pPed only to be used by UT - do not use/check this on the RT side
	bool bShouldOverridePos;
};

struct sSonarBlipStructRT
{
	sSonarBlipStructRT() {}

	void Update(const sSonarBlipStruct& sonarBlip)
	{
		iId = sonarBlip.iId; 
		vPos = sonarBlip.vPos;
		fSize = sonarBlip.fSize;
		iHudColour = sonarBlip.iHudColour;
		iFramesAlive = sonarBlip.iFramesAlive;
	}
	s32 iId;
	Vector2 vPos;
	float fSize;
	eHUD_COLOURS iHudColour;
	u8 iFramesAlive;
};


#define MAX_CONE_ANGLES	(4)

class CBlipCone
{
public :
	CBlipCone(s32 iActualIdOfBlip, 
		float fVisualFieldMinAzimuthAngle, float fVisualFieldMaxAzimuthAngle, float fGazeAngle, 
		float fFocusRange, float fPeripheralRange, 
		float fPosX, float fPosY, float fRotation, u8 alpha, bool bForceRedraw, eHUD_COLOURS color = HUD_COLOUR_BLUEDARK)
		: m_iActualIdOfBlip(iActualIdOfBlip), 
		m_fVisualFieldMinAzimuthAngle(fVisualFieldMinAzimuthAngle), m_fVisualFieldMaxAzimuthAngle(fVisualFieldMaxAzimuthAngle), m_fGazeAngle(fGazeAngle), 
		m_fFocusRange(fFocusRange), m_fPeripheralRange(fPeripheralRange), 
		m_fPosX(fPosX), m_fPosY(fPosY), m_fRotation(fRotation), m_alpha(alpha), m_bForceRedraw(bForceRedraw), m_color(color)
	{
	}

	s32 GetActualId() { return m_iActualIdOfBlip; }
	float GetVisualFieldMinAzimuthAngle() { return m_fVisualFieldMinAzimuthAngle; }
	float GetVisualFieldMaxAzimuthAngle() { return m_fVisualFieldMaxAzimuthAngle; }
	float GetGazeAngle() { return m_fGazeAngle; }
	float GetFocusRange() { return m_fFocusRange; }
	float GetPeripheralRange() { return m_fPeripheralRange; }
	float GetPosX() { return m_fPosX; }
	float GetPosY() { return m_fPosY; }
	float GetRotation() { return m_fRotation; }
	u8 GetAlpha() { return m_alpha; }
	bool GetForceRedraw() { return m_bForceRedraw; }

	eHUD_COLOURS GetColor() { return m_color; }

private:
	s32 m_iActualIdOfBlip;
	float m_fVisualFieldMinAzimuthAngle;
	float m_fVisualFieldMaxAzimuthAngle;
	float m_fGazeAngle;
	float m_fFocusRange;
	float m_fPeripheralRange;
	float m_fPosX;
	float m_fPosY;
	float m_fRotation;
	eHUD_COLOURS m_color;
	u8 m_alpha;
	bool m_bForceRedraw;
};



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMapBlip
// PURPOSE:	base class for blips
/////////////////////////////////////////////////////////////////////////////////////
class CMiniMapBlip
{
public:
	CMiniMapBlip(bool bComplex) {m_bIsComplex = bComplex;}
	
	s32		m_iActualId;
	s32		m_iUniqueId;
	u16		m_iReferenceIndex;
	bool	m_bIsComplex;
	
#if __DEV // NOTE: If you move/add anything to this class, check the alignment of CBlipComplex's Vector3, as its align pragma means it may bloat the structure
	// check such alignment with /d1reportSingleClassLayoutCBlipComplex on a MS compiler (x64, X360... Durango?)
	char	m_cBlipCreatedByScriptName[8];
#endif
	bool IsComplex() const { return m_bIsComplex; }
};




/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CBlipSimple
// PURPOSE:	holds all the details for simple blips (not as big as complex blips)
/////////////////////////////////////////////////////////////////////////////////////
class CBlipSimple : public CMiniMapBlip
{
public:
	CBlipSimple() : CMiniMapBlip(false) {}
	Vector2 vPosition;  // position (x & y only)
};


///////////////////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CEntityPoolIndexForBlip
// PURPOSE:	Wraps all access to pools for blips that are attached to entities or pickup placements
//			The pools for CVehicle and CObject are now using atPool instead of fwPool
//				so there's no GetAt() or GetIndex()
//			I'm going to try using GetSlot() and GetJustIndex() inside here
//			For consistency, I'll do the same for peds, pickups and pickup placements
//			
///////////////////////////////////////////////////////////////////////////////////////////////////////
class CEntityPoolIndexForBlip
{
public:
	CEntityPoolIndexForBlip() : m_iPoolIndex(sysMemPoolAllocator::s_invalidIndex) {}
	explicit CEntityPoolIndexForBlip(const CEntity *pEntity, eBLIP_TYPE blipType);
	explicit CEntityPoolIndexForBlip(const CPickupPlacement *pPickupPlacement);

	bool IsValid() const { return (m_iPoolIndex != sysMemPoolAllocator::s_invalidIndex); }

	bool operator==(const CEntityPoolIndexForBlip& rhs) const
	{
		return (m_iPoolIndex == rhs.m_iPoolIndex);
	}

	static eBLIP_TYPE GetBlipTypeForEntity(const CEntity *pEntity);

	CEntity *GetEntity(eBLIP_TYPE blipType) const;
	CPickupPlacement *GetPickupPlacement();

private:
	s32 m_iPoolIndex;
};


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CBlipComplex
// PURPOSE:	holds all the details for complex blips
/////////////////////////////////////////////////////////////////////////////////////
class CBlipComplex : public CMiniMapBlip
{
public:
	// PURPOSE:	Constructor
	CBlipComplex();
	
	CEntityPoolIndexForBlip m_PoolIndex;
	// FOR ALIGNMENT PURPOSES, we need this here for 16 byte alignment (assuming CMiniMapBlip is still 11 bytes)
	Vector3 vPosition;  // position

	atFixedBitSet<MAX_BLIP_FLAGS, u32> m_flags;
	atLocValue	cLocName;

	BlipLinkage iLinkageId;

	u16 iFlashInterval;  // interval of the flashing on/off of the blip
	u16 iFlashDuration;  // flash duration of the blip - after this time, the flash will automatically stop
	
	u32 iColour;  // main blip colour (or as an RGB if the flag is set)
	Color32 iColourSecondary; // secondary blip colour
	Vector2 vScale;  // scale of the blip

	float fDirection;  // direction in Degrees (since thats what Scaleform uses)

	bool bBlipIsOn3dMap : 1;  // whether the blip is placed on the 3D blip layer or the 2D blip layer
	u8 type : 7;  // type of blip
	u8 priority;  // priority of blip
	u8 display;  // display type
	u8 iAlpha;  // alpha of the blip
	u8 iCategory;  // category of the blip, used on the pausemap legend
	
	s8 iNumberToDisplay;  // number to display ontop of the blip

#if __BANK
	s8 iDebugBlipNumber;
#endif // __BANK
	
};

//
// NAME: CBlipUpdateQueue
// PURPOSE: Queue of blips to update on render thread. I've separated this from the drawlists as we can't guarantee these will be executed
class CBlipUpdateQueue
{
public:
	class QueueLock {
	public:
		QueueLock(const CBlipUpdateQueue& blipUpdateQueue) : ms_blipUpdateQueue(blipUpdateQueue) {blipUpdateQueue.LockQueue(); }
		~QueueLock() {ms_blipUpdateQueue.UnlockQueue();}
		const CBlipUpdateQueue& ms_blipUpdateQueue;
	};

	CBlipUpdateQueue() {m_Processed = true;}
	void Init();

	void LockQueue() const;
	void UnlockQueue() const;

	void Add(const CBlipComplex& blip);
	void Empty();
	void UpdateBlips();
	void RenderToFow();

private:
	fwRscMemPagedPool<CBlipComplex, true> m_Queue;
	mutable sysCriticalSectionToken m_QueueLock;
	bool m_Processed;
};

struct sMapZoomData
{
	struct sMapZoomLevel
	{
		float fZoomScale;
		float fZoomSpeed;
		float fScrollSpeed;
		Vector2 vTiles;

		PAR_SIMPLE_PARSABLE;
	};

	atArray<sMapZoomLevel> zoomLevels;

	PAR_SIMPLE_PARSABLE;
};

//
// functions common to both update and render thread implementation of minimap
//
class CMiniMap_Common
{
public:
	static const s32 ALPHA_OF_POLICE_PERCEPTION_CONES = 36;

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static bool EnumerateBlipLinkages(s32 iMovieId);

	static const char *GetBlipName(const BlipLinkage& iBlipNamesIndex);

	static Color32 GetColourFromBlipSettings(eBLIP_COLOURS nColourIndex, bool bBrightness, eHUD_COLOURS *pReturnHudColour = NULL);

	static void CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args);

	static void SetInteriorMovieActive(bool bActive) { ms_bInteriorMovieIsActive = bActive; }  // this is set up in safe area
	static bool IsInteriorMovieActive() { return ms_bInteriorMovieIsActive; }

	static void InitZoomLevels();
	static Vector2 GetTilesFromZoomLevel(s32 iZoomLevel);
	static float GetScrollSpeedFromZoomLevel(s32 iZoomLevel);
	static float GetZoomSpeedFromZoomLevel(s32 iZoomLevel, float fPressedAmount);
	static float GetScaleFromZoomLevel(s32 iZoomLevel);
	static s32 GetDisplayScaleFromZoomLevel(s32 iZoomLevel);

	// Wraps value between 0 and 359 degrees
	static float WrapBlipRotation(float fDegrees);

#if !__FINAL
	static bool OutputDebugTransitions() { return sm_bLogMinimapTransitions; };
#if __BANK
	static void CreateWidgets(bkBank* pBank);
#endif // __BANK
#endif // !__FINAL

#if ENABLE_FOG_OF_WAR

#if RSG_PC
	static grcRenderTarget *GetFogOfWarRT() { return ms_MiniMapFogOfWar[ms_CurExportRTIndex]; }
	static grcRenderTarget *GetFogOfWarRevealRatioRT() { return ms_MiniMapFogofWarRevealRatio[ms_CurExportRTIndex]; }
#else
	static grcRenderTarget *GetFogOfWarRT() { return ms_MiniMapFogOfWar; }
	static grcRenderTarget *GetFogOfWarRevealRatioRT() { return ms_MiniMapFogofWarRevealRatio; }
#endif
	static grcRenderTarget *GetFogOfWarCopyRT() { return ms_MiniMapFogOfWarCopy; }
	static u8 *GetFogOfWarLockPtr() { return ms_MiniMapFogOfWarLockPtr; }
	static u32 GetFogOfWarLockPitch() { return ms_MiniMapFogOfWarLockPitch; }
	static float *GetFogOfWarRevealRatioLockPtr() { return ms_MiniMapRevealRatioLockPtr; }
	static float GetFogOfWarDiscoveryRatio();
	static bool IsCoordinateRevealed(const Vector3 &coord);

#if __D3D11
#if RSG_PC
	static void	UpdateFogOfWarRBIndex()						{ ms_CurExportRTIndex = (ms_CurExportRTIndex+1) % NUM_FOW_TARGETS; }
	static grcTexture *GetFogOfWarTex()						{ return ms_MiniMapFogOfWarTex[ms_CurExportRTIndex]; }
	static grcTexture *GetFogOfWarTex(size_t idx)			{ Assert(idx < MAX_FOW_TARGETS); return ms_MiniMapFogOfWarTex[idx]; }
	static grcTexture *GetPreviousFogOfWarTex()				{ return ms_MiniMapFogOfWarTex[((ms_CurExportRTIndex - 1) + NUM_FOW_TARGETS) % NUM_FOW_TARGETS]; }
	static grcTexture *GetFogOfWarRevealRatioTex()			{ return ms_MiniMapFogofWarRevealRatioTex[ms_CurExportRTIndex]; }
	static grcTexture *GetPreviousFogOfWarRevealRatioTex()	{ return ms_MiniMapFogofWarRevealRatioTex[((ms_CurExportRTIndex - 1) + NUM_FOW_TARGETS) % NUM_FOW_TARGETS]; }
	static int GetCurrentExportRTIndex()					{ return ms_CurExportRTIndex; }
#else
	static grcTexture *GetFogOfWarTex() { return ms_MiniMapFogOfWarTex; }
	static grcTexture *GetFogOfWarRevealRatioTex() { return ms_MiniMapFogofWarRevealRatioTex; }
#endif
	static u8 *GetFogOfWarData() { return ms_MiniMapFogOfWarData; }
#endif // __D3D11
#endif // ENABLE_FOG_OF_WAR

	// Blip update queue access
	static const CBlipUpdateQueue& GetBlipUpdateQueue() {return ms_blipUpdateQueue;}
	static void EmptyBlipUpdateQueue() {ms_blipUpdateQueue.Empty();}
	static void AddBlipToUpdate(const CBlipComplex& blip) {ms_blipUpdateQueue.Add(blip);}
	static void RenderUpdate() {ms_blipUpdateQueue.UpdateBlips();}
	static void RenderBlipToFow() {ms_blipUpdateQueue.RenderToFow();}
private:
	static CBlipUpdateQueue ms_blipUpdateQueue;
	static atBinaryMap<ConstString, BlipLinkage> ms_pBlipLinkages;
	static bool ms_bInteriorMovieIsActive;

#if !__FINAL
	static bool sm_bLogMinimapTransitions;
#endif // __BANK

#if ENABLE_FOG_OF_WAR
#if RSG_PC
	static u32				ms_CurExportRTIndex;
	static grcRenderTarget* ms_MiniMapFogOfWar[MAX_FOW_TARGETS];
#else
	static grcRenderTarget* ms_MiniMapFogOfWar;
#endif

#if __D3D11
#if RSG_PC
	static grcTexture* ms_MiniMapFogOfWarTex[MAX_FOW_TARGETS];
	static grcTexture* ms_MiniMapFogofWarRevealRatioTex[MAX_FOW_TARGETS];
#else
	static grcTexture* ms_MiniMapFogOfWarTex;
	static grcTexture* ms_MiniMapFogofWarRevealRatioTex;
#endif
	static u8 ms_MiniMapFogOfWarData[FOG_OF_WAR_RT_SIZE * FOG_OF_WAR_RT_SIZE];
	static float ms_MiniMapFogofWarRevealRatioData;
	static bool m_UpdateFogOfWarData;
#endif // __D3D11

#if RSG_PC
	static grcRenderTarget* ms_MiniMapFogofWarRevealRatio[MAX_FOW_TARGETS];
#else
	static grcRenderTarget* ms_MiniMapFogofWarRevealRatio;
#endif
	static grcRenderTarget* ms_MiniMapFogOfWarCopy;
	static u8 *ms_MiniMapFogOfWarLockPtr;
	static u32 ms_MiniMapFogOfWarLockPitch;
	static float *ms_MiniMapRevealRatioLockPtr;
#endif //ENABLE_FOG_OF_WAR

	static sMapZoomData sm_zoomData;
};

#endif // #ifndef __MINIMAP_COMMON_H__

// eof



