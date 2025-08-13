///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	TimeCycle.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	19 Aug 2008
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////												

#include "TimeCycle.h"

#include "TimeCycleDebug.h"

// Rage headers
#include "grcore/debugdraw.h"
#include "math/vecmath.h"
#include "spatialdata/sphere.h"
#include "system/nelem.h"
#include "system/threadtype.h"

// Framework headers
#include "timecycle/tccycle.h"
#include "timecycle/tcinstbox.h"
#include "timecycle/tcmanager.h"
#include "timecycle/optimisations.h"

// Game headers
#include "debug/Rendering/DebugLighting.h"
#include "debug/MarketingTools.h"
#include "debug/LightProbe.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "camera/scripted/ScriptDirector.h"
#include "camera/viewports/ViewportManager.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "control/gamelogic.h"
#include "Core/Game.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "Cutscene/CutSceneCameraEntity.h"
#include "frontend/NewHud.h"
#include "frontend/PauseMenu.h"
#include "game/Clock.h"
#include "game/weather.h"
#include "PathServer/ExportCollision.h"
#include "Peds/ped.h"
#include "Peds/PlayerSpecialAbility.h"
#include "Peds/rendering/PedVariationStream.h"
#include "Renderer/Renderer.h"

#include "renderer/PostProcessFX.h"
#include "renderer/PostProcessFXHelper.h"
#include "renderer/Water.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "Scene/DataFileMgr.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "Scene/Portals/Portal.h"
#include "Scene/Portals/PortalTracker.h"
#include "scene/world/GameWorld.h"
#include "Scene/world/GameWorldWaterHeight.h"
#include "scene/lod/LodScale.h"
#include "script/script.h"
#include "streaming/streamingrequestlist.h"
#include "timecycle/TimeCycleConfig.h"
#include "Vfx/VfxHelper.h"
#include "vfx/sky/SkySettings.h"
#include "vfx/sky/Sky.h"
#include "vfx/misc/LinearPiecewiseFog.h"
#include "camera/viewports/ViewportManager.h"
#include "camera/viewports/viewport.h"
#include "camera/CamInterface.h"
#include "camera/base/BaseCamera.h"
#include "camera/replay/ReplayRecordedCamera.h"
#include "control/replay/ReplayRecording.h"


#include "camera/CamInterface.h"
#include "camera/replay/ReplayDirector.h"

#if GTA_REPLAY
//To track down a bug where it looks as though the TC data isnt recording.
PARAM(replayTCrecordOutput,"TTY output for replay TC recording");
#endif //GTA_REPLAY

#ifndef NAVMESH_EXPORT
#define NAVMESH_EXPORT 0
#endif

CompileTimeAssert(TCVAR_NUM == TIMECYCLE_VAR_COUNT); // might as well catch this early

// optimisations
TIMECYCLE_OPTIMISATIONS()

///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													

// constants
const Vec3V	SUN_LIGHT_UNDERWATER						(0.0f, 0.0f, 1.0f);
const float SUN_RISE_TIME								= 6.0f;
const float SUN_SET_TIME								= 20.0f;
const float SUN_MOON_TIME								= SUN_SET_TIME + 1.5f;
const float MOON_SUN_TIME								= SUN_RISE_TIME - 1.5f;
const float SUN_RISE_TO_SET_TIME						= SUN_SET_TIME-SUN_RISE_TIME;
const float SUN_SET_TO_MIDNIGHT_TIME					= 24.0f-SUN_SET_TIME; 

// tweakables

dev_float 	INTERIOR_BLEND_SCALE  						= 10.0f;	
dev_float 	INTERIOR_BLEND_OFFSET 						= 0.9f;	
dev_float 	INTERIOR_BLEND_EXP    						= 1.0f;
dev_float	INTERIOR_BLEND_FOG_MAX						= 0.1f;
dev_bool 	INTERIOR_BLEND_FOG_FADE						= false;
dev_bool 	INTERIOR_BLEND_MATCH_SKY_TO_FAR_CLIP		= false;


extern bool g_cheapmodeHogEnabled;

///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////													


CTimeCycle	g_timeCycle;

#if __BANK
const char* g_debugGroupNames[] = 
{
	"Lights",
	"Post Fx",
	"Night Vision",
	"Heat Haze",
	"Lens Distortion",
	"Lens Artefacts",
	"Blur Vignetting",
	"Screen Blur",
	"Sky - Colour",
	"Sky - Sun",
	"Sky - Stars / Moon",
	"Sky - Cloud Gen",
	"Sky - Main Clouds",
	"Sky - Filler Clouds",
	"Cloud Shadows",
	"Directional Shadows",
	"Sprites",
	"Water",
	"Mirror",
	"Fog",
	"Reflection",
	"Misc",
	"LOD"
};
#define TIMECYCLE_NUM_DEBUG_GROUPS (NELEM(g_debugGroupNames))

const char* g_userFlagNames[] = 
{
	"Underwater Only",
	"Interior Only",
	"Expand Ambient Light Box",
	"Interpolate Ambient Mult",
	"Reset Interior Ambient Cache"
};
#define TIMECYCLE_NUM_USER_FLAGS (NELEM(g_userFlagNames))

#define BANK_ONLY_PARAM6(param0,param1,param2,param3,param4,param5) ,param0,param1,param2,param3,param4,param5
#else
#define BANK_ONLY_PARAM6(param0,param1,param2,param3,param4,param5)
#endif //__BANK

enum 
{
	tcflag_underwateronly,
	tcflag_interioronly,
	tcflag_expandAmbientLightBox,
	tcflag_interpolateAmbient,
	tcflag_resetInteriorAmbCache,
	tcflag_count
};

tcVarInfo g_varInfos[] = 
{
	// TCVARID										NAME										DEFAULT		VARTYPE				MODTYPE									DEBUGMODMENU		DEBUGGROUP				DEBUGNAME										DEBUGMIN	DEBUGMAX	DEBUGDELTA

	// light variables
	{TCVAR_LIGHT_DIR_COL_R, 						"light_dir_col_r", 							0.890f,		VARTYPE_COL3_LIN,	true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Directional Colour",							0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_DIR_COL_G, 						"light_dir_col_g", 							0.675f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_DIR_COL_B, 						"light_dir_col_b", 							0.392f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_DIR_MULT,							"light_dir_mult",							14.750f,	VARTYPE_FLOAT,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Directional Intensity",						0.0f,		64.0f,		1.0f)},

	{TCVAR_LIGHT_DIRECTIONAL_AMB_COL_R, 			"light_directional_amb_col_r", 				0.000f,		VARTYPE_COL3_LIN,	true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Directional Ambient - Colour",					0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_DIRECTIONAL_AMB_COL_G, 			"light_directional_amb_col_g", 				0.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_DIRECTIONAL_AMB_COL_B, 			"light_directional_amb_col_b", 				0.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_DIRECTIONAL_AMB_INTENSITY,			"light_directional_amb_intensity",			0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Directional Ambient - Intensity",				0.0f,		255.0f,		1.0f)},
	{TCVAR_LIGHT_DIRECTIONAL_AMB_INTENSITY_MULT,	"light_directional_amb_intensity_mult",		1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Directional Ambient - Intensity Mult",			0.0f,		16.0f,		0.01f)},
	{TCVAR_LIGHT_DIRECTIONAL_AMB_BOUNCE_ENABLED,	"light_directional_amb_bounce_enabled",		1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Directional Ambient - Enable Bounce",			0.0f,		1.0f,		1.0f)},

	{TCVAR_LIGHT_AMB_DOWN_WRAP,						"light_amb_down_wrap",						1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Ambient - Down Wrap",							0.0f,		1.0f,		0.01f)},

	{TCVAR_LIGHT_NATURAL_AMB_DOWN_COL_R,			"light_natural_amb_down_col_r",				0.541f,		VARTYPE_COL3_LIN,	true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Natural Ambient - Down Colour",				0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_NATURAL_AMB_DOWN_COL_G,			"light_natural_amb_down_col_g", 			0.655f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_NATURAL_AMB_DOWN_COL_B,			"light_natural_amb_down_col_b", 			1.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_NATURAL_AMB_DOWN_INTENSITY,		"light_natural_amb_down_intensity",			6.250f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Natural Ambient - Down Intensity",				0.0f,		16.0f,		1.0f)},

	{TCVAR_LIGHT_NATURAL_AMB_BASE_COL_R, 			"light_natural_amb_up_col_r", 				1.000f,		VARTYPE_COL3_LIN,	true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Natural Ambient - Base Colour",				0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_NATURAL_AMB_BASE_COL_G, 			"light_natural_amb_up_col_g", 				0.847f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_NATURAL_AMB_BASE_COL_B, 			"light_natural_amb_up_col_b", 				0.459f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_NATURAL_AMB_BASE_INTENSITY,		"light_natural_amb_up_intensity",			8.500f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Natural Ambient - Base Intensity",				0.0f,		16.0f,		1.0f)},
	{TCVAR_LIGHT_NATURAL_AMB_BASE_INTENSITY_MULT,	"light_natural_amb_up_intensity_mult",		1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Natural Ambient - Base Intensity Mult",		0.0f,		16.0f,		0.01f)},

	{TCVAR_LIGHT_NATURAL_PUSH,						"light_natural_push",						0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Natural Ambient Push",							0.0f,		1.0f,		0.01f)},
	{TCVAR_LIGHT_AMBIENT_BAKE_RAMP, 				"light_ambient_bake_ramp",					1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Lights",				"Natural/Artificial Ambient Ramp",				0.0f,		1.0f,		0.1f)},	

	{TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_R,		"light_artificial_int_down_col_r",			0.541f,		VARTYPE_COL3_LIN,	true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Artificial Interior Ambient - Down Colour",	0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_G,	 	"light_artificial_int_down_col_g", 			0.655f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_B,	 	"light_artificial_int_down_col_b", 			1.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_INTENSITY, "light_artificial_int_down_intensity",		0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Artificial Interior Ambient - Down Intensity",	0.0f,		255.0f,		1.0f)},

	{TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_COL_R, 	"light_artificial_int_up_col_r", 			1.000f,		VARTYPE_COL3_LIN,	true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Artificial Interior Ambient - Base Colour",	0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_COL_G, 	"light_artificial_int_up_col_g", 			0.847f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_COL_B, 	"light_artificial_int_up_col_b", 			0.459f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_INTENSITY,	"light_artificial_int_up_intensity",		0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Artificial Interior Ambient - Base Intensity",	0.0f,		255.0f,		1.0f)},

	{TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_DOWN_COL_R,		"light_artificial_ext_down_col_r",			0.000f,		VARTYPE_COL3_LIN,	true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Artificial Exterior Ambient - Down Colour",	0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_DOWN_COL_G,	 	"light_artificial_ext_down_col_g", 			0.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_DOWN_COL_B,	 	"light_artificial_ext_down_col_b", 			0.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_DOWN_INTENSITY, "light_artificial_ext_down_intensity",		0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Artificial Exterior Ambient - Down Intensity",	0.0f,		255.0f,		1.0f)},

	{TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_BASE_COL_R, 	"light_artificial_ext_up_col_r", 			0.000f,		VARTYPE_COL3_LIN,	true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Artificial Exterior Ambient - Base Colour",	0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_BASE_COL_G, 	"light_artificial_ext_up_col_g", 			0.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_BASE_COL_B, 	"light_artificial_ext_up_col_b", 			0.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_BASE_INTENSITY,	"light_artificial_ext_up_intensity",		0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Artificial Exterior Ambient - Base Intensity",	0.0f,		255.0f,		1.0f)},

	{TCVAR_PED_LIGHT_COL_R, 						"ped_light_col_r",							0.500f,		VARTYPE_COL3_LIN,	true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Ped Light Colour",								0.0f,		0.0f,		0.0f)},
	{TCVAR_PED_LIGHT_COL_G, 						"ped_light_col_g", 							0.682f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_PED_LIGHT_COL_B, 						"ped_light_col_b", 							1.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_PED_LIGHT_MULT,							"ped_light_mult",							0.400f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Ped Light Intensity",							0.0f,		64.0f,		1.0f)},
	{TCVAR_PED_LIGHT_DIRECTION_X,					"ped_light_direction_x",					-0.825f,	VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Lights",				"Ped Light Direction X",						-1.0f, 		1.0f, 		0.1f)},
	{TCVAR_PED_LIGHT_DIRECTION_Y,					"ped_light_direction_y",					0.565f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Lights",				"Ped Light Direction Y",						-1.0f, 		1.0f, 		0.1f)},
	{TCVAR_PED_LIGHT_DIRECTION_Z,					"ped_light_direction_z",					0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Lights",				"Ped Light Direction Z",						-1.0f, 		1.0f, 		0.1f)},

	{TCVAR_LIGHT_AMB_OCC_MULT,						"light_amb_occ_mult",						1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Lights",				"Ambient Occlusion Multiplier",					0.0f, 		4.0f, 		0.1f)},
	{TCVAR_LIGHT_AMB_OCC_MULT_PED,					"light_amb_occ_mult_ped",					1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Lights",				"Ped AO Multiplier",							0.0f, 		1.0f, 		0.1f)},
	{TCVAR_LIGHT_AMB_OCC_MULT_VEH,					"light_amb_occ_mult_veh",					1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Lights",				"Vehicle AO Multiplier",						0.0f, 		1.0f, 		0.1f)},
	{TCVAR_LIGHT_AMB_OCC_MULT_PROP,					"light_amb_occ_mult_prop",					1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Lights",				"Prop AO Multiplier",							0.0f, 		1.0f, 		0.1f)},
	{TCVAR_LIGHT_AMB_VOLUMES_IN_DIFFUSE,			"light_amb_volumes_in_diffuse",				0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Lights",				"Ambient volumes in diffuse",					0.0f, 		1.0f, 		0.1f)},
	{TCVAR_LIGHT_SSAO_INTEN,						"ssao_inten",								3.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Lights",				"SSAO Intensity",								0.0f, 		16.0f, 		0.1f)},
	{TCVAR_LIGHT_SSAO_TYPE,							"ssao_type",								0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Lights",				"SSAO Type",									0.0f, 		1.0f,		1.0f)},
	{TCVAR_LIGHT_SSAO_CP_STRENGTH,					"ssao_cp_strength",							0.500f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Lights",				"SSAO cp strength",								0.0f, 		1.0f, 		0.01f)},
	{TCVAR_LIGHT_SSAO_QS_STRENGTH,					"ssao_qs_strength",							1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Lights",				"SSAO qs strength",								0.0f, 		1.0f, 		0.01f)},

	{TCVAR_LIGHT_PED_RIM_MULT,						"light_ped_rim_mult",						0.500f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Ped Rim Multiplier",							0.0f, 		1.0f, 		0.01f)},
	{TCVAR_LIGHT_DYNAMIC_BAKE_TWEAK,				"light_dynamic_bake_tweak",					0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Dynamic Bake Tweak",							-2.0f, 		2.0f, 		0.01f)},
	{TCVAR_LIGHT_VEHICLE_SECOND_SPEC_OVERRIDE,		"light_vehicle_second_spec_override",		0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Vehicle 2nd Spec override",					0.0f, 		1.0f, 		0.01f)},
	{TCVAR_LIGHT_VEHICLE_INTENSITY_SCALE,			"light_vehicle_intenity_scale",				1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Vehicle Intensity Scale",						0.0f, 		1.0f, 		0.01f)},

	// Sun pos override
	{TCVAR_LIGHT_DIRECTION_OVERRIDE,				"light_direction_override",					0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Light Position Override",						0.0f, 		1.0f, 		1.0f)},
	{TCVAR_LIGHT_DIRECTION_OVERRIDE_OVERRIDES_SUN,	"light_direction_override_overrides_sun",	1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Light Position Override Override Sun",			0.0f, 		1.0f, 		1.0f)},
	{TCVAR_SUN_DIRECTION_X,							"sun_direction_x",							0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Sun Position X",								-1.0f, 		1.0f,		0.1f)},
	{TCVAR_SUN_DIRECTION_Y,							"sun_direction_y",							-1.000f,	VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Sun Position Y",								-1.0f, 		1.0f,		0.1f)},
	{TCVAR_SUN_DIRECTION_Z,							"sun_direction_z",							0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Sun Position Z",								-1.0f, 		1.0f,		0.1f)},
	{TCVAR_MOON_DIRECTION_X,						"moon_direction_x",							0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Moon Position X",								-1.0f, 		1.0f,		0.1f)},
	{TCVAR_MOON_DIRECTION_Y,						"moon_direction_y",							-1.000f,	VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Moon Position Y",								-1.0f, 		1.0f,		0.1f)},
	{TCVAR_MOON_DIRECTION_Z,						"moon_direction_z",							0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Moon Position Z",								-1.0f, 		1.0f,		0.1f)},

	// light ray variables
	{TCVAR_LIGHT_RAY_COL_R, 						"light_ray_col_r", 							0.5f,		VARTYPE_COL3_LIN,	true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Lightrays Colour",								0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_RAY_COL_G, 						"light_ray_col_g", 							0.5f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_RAY_COL_B, 						"light_ray_col_b", 							0.5f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_LIGHT_RAY_MULT,							"light_ray_mult",							1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Lightrays Intensity",							0.0f,		32.0f,		1.0f)},
	{TCVAR_LIGHT_RAY_UNDERWATER_MULT,				"light_ray_underwater_mult",				1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Underwater Lightrays Intensity",				0.0f,		32.0f,		1.0f)},
	{TCVAR_LIGHT_RAY_DIST,							"light_ray_dist",							100.0f,		VARTYPE_FLOAT,		true,		MODTYPE_CLAMP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Lightrays Distance",							1.0f,		4000.0f,	1.0f)},
	{TCVAR_LIGHT_RAY_HEIGHTFALLOFF,							"light_ray_heightfalloff",			1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Lightrays HeightFallOff",							0.0f,		1000.0f,		0.10f)},
	{TCVAR_LIGHT_RAY_HEIGHTFALLOFFSTART,			"light_ray_height_falloff_start",			500.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",					"Lightrays Height FallOffStart",				0.0f,		1000.0f,	0.010f)},

	// Screen Space Light rays data
	{TCVAR_LIGHT_RAY_ADD_REDUCER, 					"light_ray_add_reducer", 					4.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Lightray Addative Reducer",					1.0f,		8.0f,		0.1f)},
	{TCVAR_LIGHT_RAY_BLIT_SIZE, 					"light_ray_blit_size", 						10.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Lightray Blit Size",							0.1f,		10.0f,		0.1f)},
	{TCVAR_LIGHT_RAY_RAY_LENGTH,					"light_ray_length",							8.5f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lights",				"Lightray Ray Length",							0.1f,		20.0f,		0.1f)},

	// post fx variables
	{TCVAR_POSTFX_EXPOSURE,							"postfx_exposure",							0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Post Fx",				"Exposure Tweak",							  -32.0f,	    32.0f,		0.01f)},
	{TCVAR_POSTFX_EXPOSURE_MIN,						"postfx_exposure_min",					   -3.500f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Post Fx",				"Exposure Min",								  -32.0f,	    32.0f,		0.01f)},
	{TCVAR_POSTFX_EXPOSURE_MAX,						"postfx_exposure_max",						3.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Post Fx",				"Exposure Max",								  -32.0f,	    32.0f,		0.01f)},

	{TCVAR_POSTFX_BRIGHT_PASS_THRESH_WIDTH,			"postfx_bright_pass_thresh_width",		    0.000f,		VARTYPE_FLOAT,		false,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Post Fx",				"Bright Pass Threshold (Width)",				0.0f, 		10.0f,		0.01f)},
	{TCVAR_POSTFX_BRIGHT_PASS_THRESH,				"postfx_bright_pass_thresh",				0.000f,		VARTYPE_FLOAT,		false,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Post Fx",				"Bright Pass Threshold (Max)",					0.0f, 		10.0f,		0.01f)},
	{TCVAR_POSTFX_INTENSITY_BLOOM,					"postfx_intensity_bloom",					2.880f,		VARTYPE_FLOAT,		false,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Post Fx",				"Intensity Bloom",								0.0f, 		16.0f,		0.1f)},

	{TCVAR_POSTFX_CORRECT_COL_R,					"postfx_correct_col_r", 					0.451f,		VARTYPE_COL3_LIN,	true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Post Fx",				"Color Correction",								0.0f,		0.0f,		0.0f)},
	{TCVAR_POSTFX_CORRECT_COL_G,					"postfx_correct_col_g", 					0.478f,		VARTYPE_NONE,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_POSTFX_CORRECT_COL_B,					"postfx_correct_col_b", 					0.443f,		VARTYPE_NONE,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_POSTFX_CORRECT_CUTOFF,					"postfx_correct_cutoff",					0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"Color Correction Upper Cutoff",				0.0f, 		16.0f, 		0.001f)},

	{TCVAR_POSTFX_SHIFT_COL_R, 						"postfx_shift_col_r", 						0.000f,		VARTYPE_COL3_LIN,	true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"Color Shift",									0.0f,		0.0f,		0.0f)},
	{TCVAR_POSTFX_SHIFT_COL_G, 						"postfx_shift_col_g", 						0.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_POSTFX_SHIFT_COL_B, 						"postfx_shift_col_b", 						0.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_POSTFX_SHIFT_CUTOFF,						"postfx_shift_cutoff",						0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"Color Shift Upper Cutoff",						0.0f, 		16.0f, 		0.001f)},

	{TCVAR_POSTFX_DESATURATION,						"postfx_desaturation",						0.700f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Post Fx",				"Desaturation",									0.0f, 		10.0f,		0.01f)},

	{TCVAR_POSTFX_NOISE, 							"postfx_noise",								0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"Noise",										0.0f, 		10.0f,		0.1f)},
	{TCVAR_POSTFX_NOISE_SIZE, 						"postfx_noise_size",						1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"Noise Size",									0.0f, 		1.0f,		0.01f)},

	{TCVAR_POSTFX_TONEMAP_FILMIC_OVERRIDE_DARK,		"postfx_tonemap_filmic_override_dark",		0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_SPECIAL,	"Post Fx",				"Override dark settings",						0.0f, 		1.0f,		1.00f)},
	{TCVAR_POSTFX_TONEMAP_FILMIC_EXPOSURE_DARK, 	"postfx_tonemap_filmic_exposure_dark",		1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_SPECIAL,	"Post Fx",				"Dark Filmic Tonemap Exposure",				   -4.0f, 		4.0f,		0.01f)},
	{TCVAR_POSTFX_TONEMAP_FILMIC_A_DARK, 			"postfx_tonemap_filmic_a",					0.22f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_SPECIAL,	"Post Fx",				"Dark Filmic Tonemap A",					  -16.0f, 		16.0f,		0.01f)},
	{TCVAR_POSTFX_TONEMAP_FILMIC_B_DARK, 			"postfx_tonemap_filmic_b",					0.3f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_SPECIAL,	"Post Fx",				"Dark Filmic Tonemap B",					  -16.0f, 		16.0f,		0.01f)},
	{TCVAR_POSTFX_TONEMAP_FILMIC_C_DARK, 			"postfx_tonemap_filmic_c",					0.1f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_SPECIAL,	"Post Fx",				"Dark Filmic Tonemap C",					  -16.0f, 		16.0f,		0.01f)},
	{TCVAR_POSTFX_TONEMAP_FILMIC_D_DARK, 			"postfx_tonemap_filmic_d",					0.2f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_SPECIAL,	"Post Fx",				"Dark Filmic Tonemap D",					  -16.0f, 		16.0f,		0.01f)},
	{TCVAR_POSTFX_TONEMAP_FILMIC_E_DARK, 			"postfx_tonemap_filmic_e",					0.01f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_SPECIAL,	"Post Fx",				"Dark Filmic Tonemap E",					  -16.0f, 		16.0f,		0.01f)},
	{TCVAR_POSTFX_TONEMAP_FILMIC_F_DARK, 			"postfx_tonemap_filmic_f",					0.3f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_SPECIAL,	"Post Fx",				"Dark Filmic Tonemap F",					  -16.0f, 		16.0f,		0.01f)},
	{TCVAR_POSTFX_TONEMAP_FILMIC_W_DARK, 			"postfx_tonemap_filmic_w",					4.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_SPECIAL,	"Post Fx",				"Dark Filmic Tonemap W",						0.0f, 		32.0f,		0.10f)},

	{TCVAR_POSTFX_TONEMAP_FILMIC_OVERRIDE_BRIGHT,	"postfx_tonemap_filmic_override_bright",	0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_SPECIAL,	"Post Fx",				"Override bright settings",						0.0f, 		1.0f,		1.00f)},
	{TCVAR_POSTFX_TONEMAP_FILMIC_EXPOSURE_BRIGHT, 	"postfx_tonemap_filmic_exposure_bright",	-2.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_SPECIAL,	"Post Fx",				"Bright Filmic Tonemap Exposure",			   -4.0f, 		4.0f,		0.01f)},
	{TCVAR_POSTFX_TONEMAP_FILMIC_A_BRIGHT, 			"postfx_tonemap_filmic_a_bright",			0.22f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_SPECIAL,	"Post Fx",				"Bright Filmic Tonemap A",					  -16.0f, 		16.0f,		0.01f)},
	{TCVAR_POSTFX_TONEMAP_FILMIC_B_BRIGHT, 			"postfx_tonemap_filmic_b_bright",			0.3f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_SPECIAL,	"Post Fx",				"Bright Filmic Tonemap B",					  -16.0f, 		16.0f,		0.01f)},
	{TCVAR_POSTFX_TONEMAP_FILMIC_C_BRIGHT, 			"postfx_tonemap_filmic_c_bright",			0.1f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_SPECIAL,	"Post Fx",				"Bright Filmic Tonemap C",				      -16.0f, 		16.0f,		0.01f)},
	{TCVAR_POSTFX_TONEMAP_FILMIC_D_BRIGHT, 			"postfx_tonemap_filmic_d_bright",			0.2f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_SPECIAL,	"Post Fx",				"Bright Filmic Tonemap D",					  -16.0f, 		16.0f,		0.01f)},
	{TCVAR_POSTFX_TONEMAP_FILMIC_E_BRIGHT, 			"postfx_tonemap_filmic_e_bright",			0.01f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_SPECIAL,	"Post Fx",				"Bright Filmic Tonemap E",					  -16.0f, 		16.0f,		0.01f)},
	{TCVAR_POSTFX_TONEMAP_FILMIC_F_BRIGHT, 			"postfx_tonemap_filmic_f_bright",			0.3f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_SPECIAL,	"Post Fx",				"Bright Filmic Tonemap F",					  -16.0f, 		16.0f,		0.01f)},
	{TCVAR_POSTFX_TONEMAP_FILMIC_W_BRIGHT, 			"postfx_tonemap_filmic_w_bright",			4.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_SPECIAL,	"Post Fx",				"Bright Filmic Tonemap W",						0.0f, 		32.0f,		0.10f)},

	// vignetting intensity
	{TCVAR_POSTFX_VIGNETTING_INTENSITY, 			"postfx_vignetting_intensity",				0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Post Fx",				"Vignetting Intensity",							0.0f, 		1.0f,		0.01f)},
	{TCVAR_POSTFX_VIGNETTING_RADIUS,				"postfx_vignetting_radius",					10.000f,	VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Post Fx",				"Vignetting Radius",							0.0f, 		500.0f,		0.01f)},
	{TCVAR_POSTFX_VIGNETTING_CONTRAST,				"postfx_vignetting_contrast",				0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Post Fx",				"Vignetting Contrast",							0.0f, 		1.0f,		0.01f)},
	{TCVAR_POSTFX_VIGNETTING_COL_R,					"postfx_vignetting_col_r",					0.000f,		VARTYPE_COL3_LIN,	true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Post Fx",				"Vignetting Colour",							0.0f, 		0.0f,		0.0f)},
	{TCVAR_POSTFX_VIGNETTING_COL_G,					"postfx_vignetting_col_g",					0.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"",												0.0f, 		0.0f,		0.0f)},
	{TCVAR_POSTFX_VIGNETTING_COL_B,					"postfx_vignetting_col_b",					0.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"",												0.0f, 		0.0f,		0.0f)},

	// colour gradient variables
	{TCVAR_POSTFX_GRAD_TOP_COL_R,					"postfx_grad_top_col_r",					1.000f,		VARTYPE_COL3_LIN,	true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Post Fx",				"Gradient Top Colour",							0.0f, 		0.0f,		0.0f)},
	{TCVAR_POSTFX_GRAD_TOP_COL_G,					"postfx_grad_top_col_g",					1.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"",												0.0f, 		0.0f,		0.0f)},
	{TCVAR_POSTFX_GRAD_TOP_COL_B,					"postfx_grad_top_col_b",					1.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"",												0.0f, 		0.0f,		0.0f)},

	{TCVAR_POSTFX_GRAD_MIDDLE_COL_R,				"postfx_grad_middle_col_r",					1.000f,		VARTYPE_COL3_LIN,	true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Post Fx",				"Gradient Middle Colour",						0.0f, 		0.0f,		0.0f)},
	{TCVAR_POSTFX_GRAD_MIDDLE_COL_G,				"postfx_grad_middle_col_g",					1.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"",												0.0f, 		0.0f,		0.0f)},
	{TCVAR_POSTFX_GRAD_MIDDLE_COL_B,				"postfx_grad_middle_col_b",					1.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"",												0.0f, 		0.0f,		0.0f)},

	{TCVAR_POSTFX_GRAD_BOTTOM_COL_R, 				"postfx_grad_bottom_col_r",					1.000f,		VARTYPE_COL3_LIN,	true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Post Fx",				"Gradient Bottom Colour",						0.0f, 		0.0f,		0.0f)},
	{TCVAR_POSTFX_GRAD_BOTTOM_COL_G, 				"postfx_grad_bottom_col_g",					1.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"",												0.0f, 		0.0f,		0.0f)},
	{TCVAR_POSTFX_GRAD_BOTTOM_COL_B, 				"postfx_grad_bottom_col_b",					1.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"",												0.0f, 		0.0f,		0.0f)},

	{TCVAR_POSTFX_GRAD_MIDPOINT, 					"postfx_grad_midpoint",						0.500f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Post Fx",				"Gradient Midpoint",							0.0f, 		1.0f,		0.01f)},
	{TCVAR_POSTFX_GRAD_TOP_MIDDLE_MIDPOINT, 		"postfx_grad_top_middle_midpoint",			0.500f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Post Fx",				"Gradient Top-Middle Midpoint",					0.0f, 		1.0f,		0.01f)},
	{TCVAR_POSTFX_GRAD_MIDDLE_BOTTOM_MIDPOINT, 		"postfx_grad_middle_bottom_midpoint",		0.500f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Post Fx",				"Gradient Middle-Bottom Midpoint",				0.0f, 		1.0f,		0.01f)},

	{TCVAR_POSTFX_SCANLINEINTENSITY,				"postfx_scanlineintensity",					0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Post Fx",				"Scanline Intensity",							0.0f, 		1.0f,	0.1f)},
	{TCVAR_POSTFX_SCANLINE_FREQUENCY_0,				"postfx_scanline_frequency_0",				800.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Post Fx",				"Scanline Frequency 0",							0.0f, 		4000.0f,	0.1f)},
	{TCVAR_POSTFX_SCANLINE_FREQUENCY_1,				"postfx_scanline_frequency_1",				800.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Post Fx",				"Scanline Frequency 1",							0.0f, 		4000.0f,		0.1f)},
	{TCVAR_POSTFX_SCANLINE_SPEED,					"postfx_scanline_speed",					0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Post Fx",				"Scanline Speed",								0.0f, 		1.0f,		0.1f)},

	{TCVAR_POSTFX_MOTIONBLUR_LENGTH,				"postfx_motionblurlength",					0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Post Fx",				"Motion Blur Length",							0.0f, 		1.0f,		0.1f)},

	// depth of field variables
	{TCVAR_DOF_FAR,									"dof_far",									130.750f,	VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"DOF Far",										0,			5000,		1)},

	{TCVAR_DOF_BLUR_MID,							"dof_blur_mid", 							0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"DOF Mid Blur",									0.0f, 		1.0f, 		0.1f)},
	{TCVAR_DOF_BLUR_FAR,							"dof_blur_far", 							1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"DOF Far Blur",									0.0f, 		1.0f, 		0.1f)},

	{TCVAR_DOF_ENABLE_HQDOF,						"dof_enable_hq", 							0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"Enable HQ DOF",								0.0f, 		1.0f, 		1.0f)},
	{TCVAR_DOF_HQDOF_SMALLBLUR,						"dof_hq_smallblur", 						0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"HQ DOF uses Small Blur",						0.0f, 		1.0f, 		1.0f)},
	{TCVAR_DOF_HQDOF_SHALLOWDOF,					"dof_hq_shallowdof", 						0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"HQ DOF uses Shallow DOF",						0.0f, 		1.0f, 		1.0f)},
	{TCVAR_DOF_HQDOF_NEARPLANE_OUT,					"dof_hq_nearplane_out", 					0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"Near start of focus plane",					-20.0f, 	25000.0f, 	1.0f)},
	{TCVAR_DOF_HQDOF_NEARPLANE_IN,					"dof_hq_nearplane_in", 						0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"Near end focus plane",							-20.0f, 	25000.0f, 	1.0f)},
	{TCVAR_DOF_HQDOF_FARPLANE_OUT,					"dof_hq_farplane_out", 						0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"Far start of focus plane",						-20.0f, 	25000.0f, 	1.0f)},
	{TCVAR_DOF_HQDOF_FARPLANE_IN,					"dof_hq_farplane_in", 						0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"Near end focus plane",							-20.0f, 	25000.0f, 	1.0f)},

	{TCVAR_ENV_BLUR_IN,								"environmental_blur_in", 					10000.000f,	VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"Environmental Blur In",						-20.0f, 	25000.0f, 	0.001f)},
	{TCVAR_ENV_BLUR_OUT,							"environmental_blur_out", 					10000.000f,	VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"Environmental Blur Out",						-20.0f, 	25000.0f, 	0.001f)},
	{TCVAR_ENV_BLUR_SIZE,							"environmental_blur_size", 					1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"Environmental Blur Size",						0.0f, 		32.0f, 		1.0f)},

	{TCVAR_BOKEH_BRIGHTNESS_MIN,					"bokeh_brightness_min", 					4.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"Bokeh Brightness Threshold Min",				0.0f, 		10.0f, 		0.01f)},
	{TCVAR_BOKEH_BRIGHTNESS_MAX,					"bokeh_brightness_max", 					3.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"Bokeh Brightness Threshold Max",				0.0f, 		10.0f, 		0.01f)},
	{TCVAR_BOKEH_FADE_MIN,							"bokeh_fade_min", 							1.320f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"Bokeh Brightness Threshold Fade Min",			0.0f, 		10.0f, 		0.01f)},
	{TCVAR_BOKEH_FADE_MAX,							"bokeh_fade_max", 							0.060f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Post Fx",				"Bokeh Brightness Threshold Fade Max",			0.0f, 		10.0f, 		0.01f)},

	// night vision variables
	{TCVAR_NV_LIGHT_DIR_MULT,						"nv_light_dir_mult",						1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Night Vision",			"Directional Multiplier",						0.0f,		255.0f,		1.0f)},
	{TCVAR_NV_LIGHT_AMB_DOWN_MULT,					"nv_light_amb_down_mult",					1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Night Vision",			"Ambient Down Multiplier",						0.0f,		255.0f,		1.0f)},
	{TCVAR_NV_LIGHT_AMB_BASE_MULT,					"nv_light_amb_up_mult",						1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Night Vision",			"Ambient Base Multiplier",						0.0f,		255.0f,		1.0f)},

	{TCVAR_NV_LOWLUM, 								"nv_lowLum",								0.050f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Night Vision",			"Low Lum",										0.0f,		2.0f,		0.01f)},
	{TCVAR_NV_HIGHLUM, 								"nv_highLum",								0.600f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Night Vision",			"High Lum",										0.0f,		2.0f,		0.01f)},
	{TCVAR_NV_TOPLUM, 								"nv_topLum",								0.750f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Night Vision",			"Top Lum",										0.0f,		2.0f,		0.01f)},

	{TCVAR_NV_SCALERLUM, 							"nv_scalerLum",								10.00f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Night Vision",			"Scaler Lum",									0.0f,		256.0f,		0.01f)},

	{TCVAR_NV_OFFSETLUM, 							"nv_offsetLum",								0.200f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Night Vision",			"Offset",										-2.0f,		2.0f,		0.01f)},
	{TCVAR_NV_OFFSETLOWLUM, 						"nv_offsetLowLum",							0.180f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Night Vision",			"Offset Low Lum",								-2.0f,		2.0f,		0.01f)},
	{TCVAR_NV_OFFSETHIGHLUM, 						"nv_offsetHighLum",							0.200f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Night Vision",			"Offset High Lum",								-2.0f,		2.0f,		0.01f)},

	{TCVAR_NV_NOISELUM, 							"nv_noiseLum",								0.050f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Night Vision",			"Noise",										0.0f,		1.0f,		0.01f)},
	{TCVAR_NV_NOISELOWLUM, 							"nv_noiseLowLum",							0.050f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Night Vision",			"Noise Low Lum",								0.0f,		1.0f,		0.01f)},
	{TCVAR_NV_NOISEHIGHLUM, 						"nv_noiseHighLum",							0.300f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Night Vision",			"Noise High Lum",								0.0f,		1.0f,		0.01f)},

	{TCVAR_NV_BLOOMLUM, 							"nv_bloomLum",								0.010f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Night Vision",			"Bloom",										0.0f,		4.0f,		0.01f)},

	{TCVAR_NV_COLORLUM_R,							"nv_colorLum_r",				 			0.000f,		VARTYPE_COL3_LIN,	true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Night Vision",			"Colour",										0.0f,		0.0f,		0.0f)},
	{TCVAR_NV_COLORLUM_G,							"nv_colorLum_g",				 			1.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Night Vision",			"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_NV_COLORLUM_B,							"nv_colorLum_b",				 			0.220f,		VARTYPE_NONE,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Night Vision",			"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_NV_COLORLOWLUM_R,						"nv_colorLowLum_r",							0.000f,		VARTYPE_COL3_LIN,	true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Night Vision",			"Low Lum Colour",								0.0f,		0.0f,		0.0f)},
	{TCVAR_NV_COLORLOWLUM_G,						"nv_colorLowLum_g",							0.500f,		VARTYPE_NONE,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Night Vision",			"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_NV_COLORLOWLUM_B,						"nv_colorLowLum_b",							1.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Night Vision",			"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_NV_COLORHIGHLUM_R,						"nv_colorHighLum_r",						0.670f,		VARTYPE_COL3_LIN,	true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Night Vision",			"High Lum Colour",								0.0f,		0.0f,		0.0f)},
	{TCVAR_NV_COLORHIGHLUM_G,						"nv_colorHighLum_g",						1.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Night Vision",			"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_NV_COLORHIGHLUM_B,						"nv_colorHighLum_b",						0.340f,		VARTYPE_NONE,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Night Vision",			"",												0.0f,		0.0f,		0.0f)},

	// heat haze variables
	{TCVAR_HH_RANGESTART,							"hh_startRange",							180.000f,	VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Heat Haze",			"Start range",									0.0f,		5000.0f,	0.5f)},
	{TCVAR_HH_RANGEEND,								"hh_farRange",								500.000f,	VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Heat Haze",			"Far range",									0.0f,		5000.0f,	0.5f)},
	{TCVAR_HH_MININTENSITY,							"hh_minIntensity",							0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Heat Haze",			"Min Intensity",								0.0f,		2.0f,		0.01f)},
	{TCVAR_HH_MAXINTENSITY,							"hh_maxIntensity",							1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Heat Haze",			"Max Intensity",								0.0f,		2.0f,		0.01f)},
	{TCVAR_HH_DISPU,								"hh_displacementU",							0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Heat Haze",			"Displacement Scale U",							-128.0f,	128.0f,		0.01f)},
	{TCVAR_HH_DISPV,								"hh_displacementV",							0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Heat Haze",			"Displacement Scale V",							-128.0f,	128.0f,		0.01f)},
	{TCVAR_HH_TEX1_USCALE,							"hh_tex1UScale",							1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Heat Haze",			"Tex1 U scale",									-128.0f,	128.0f,		0.01f)},
	{TCVAR_HH_TEX1_VSCALE,							"hh_tex1VScale",							1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Heat Haze",			"Tex1 V scale",									-128.0f,	128.0f,		0.01f)},
	{TCVAR_HH_TEX1_UOFFSET,							"hh_tex1UOffset",							0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Heat Haze",			"Tex1 U offset",								-128.0f,	128.0f,		0.01f)},
	{TCVAR_HH_TEX1_VOFFSET,							"hh_tex1VOffset",							0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Heat Haze",			"Tex1 V offset",								-128.0f,	128.0f,		0.01f)},
	{TCVAR_HH_TEX2_USCALE,							"hh_tex2UScale",							0.500f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Heat Haze",			"Tex2 U scale",									-128.0f,	128.0f,		0.01f)},
	{TCVAR_HH_TEX2_VSCALE,							"hh_tex2VScale",							0.500f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Heat Haze",			"Tex2 V scale",									-128.0f,	128.0f,		0.01f)},
	{TCVAR_HH_TEX2_UOFFSET,							"hh_tex2UOffset",							0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Heat Haze",			"Tex2 U offset",								-128.0f,	128.0f,		0.01f)},
	{TCVAR_HH_TEX2_VOFFSET,							"hh_tex2VOffset",							0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Heat Haze",			"Tex2 V offset",								-128.0f,	128.0f,		0.01f)},
	{TCVAR_HH_TEX1_OFFSET,							"hh_tex1UFrequencyOffset",					PI/2.000f,	VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Heat Haze",			"Tex1 U Frequency offset",						-10.0f*PI,	10.0f*PI,	PI/10.0f)},
	{TCVAR_HH_TEX1_FREQUENCY,						"hh_tex1UFrequency",						PI/2.000f,	VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Heat Haze",			"Tex1 U Frequency",								-10.0f*PI,	10.0f*PI,	PI/10.0f)},
	{TCVAR_HH_TEX1_AMPLITUDE,						"hh_tex1UAmplitude",						3.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Heat Haze",			"Tex1 U Amplitude",								-64.0f,		64.0f,		0.1f)},
	{TCVAR_HH_TEX1_SCROLLING,						"hh_tex1VScrollingSpeed",					20.000f,	VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Heat Haze",			"Tex1 V Scrolling speed",						-64.0f,		64.0f,		0.1f)},
	{TCVAR_HH_TEX2_OFFSET,							"hh_tex2UFrequencyOffset",					0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Heat Haze",			"Tex2 U Frequency offset",						-10.0f*PI,	10.0f*PI,	PI/10.0f)},
	{TCVAR_HH_TEX2_FREQUENCY,						"hh_tex2UFrequency",						PI/5.000f,	VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Heat Haze",			"Tex2 U Frequency",								-10.0f*PI,	10.0f*PI,	PI/10.0f)},
	{TCVAR_HH_TEX2_AMPLITUDE,						"hh_tex2UAmplitude",						0.500f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Heat Haze",			"Tex2 U Amplitude",								-64.0f,		64.0f,		0.1f)},
	{TCVAR_HH_TEX2_SCROLLING,						"hh_tex2VScrollingSpeed",					3.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Heat Haze",			"Tex2 V Scrolling speed",						-64.0f,		64.0f,		0.1f)},

	// lens distortion
	{TCVAR_LENS_DISTORTION_COEFF,					"lens_dist_coeff",							0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lens Distortion",		"Distortion Coefficient 1",						-1.0f,		1.0f,		0.01f)},
	{TCVAR_LENS_DISTORTION_CUBE_COEFF,				"lens_dist_cube_coeff",						0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lens Distortion",		"Distortion Coefficient 2",						-1.0f,		1.0f,		0.01f)},
	{TCVAR_LENS_CHROMATIC_ABERRATION_COEFF,			"chrom_aberration_coeff",					0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lens Distortion",		"Chrom Aberration Coefficient 1",				-1.0f,		1.0f,		0.01f)},
	{TCVAR_LENS_CHROMATIC_ABERRATION_CUBE_COEFF,	"chrom_aberration_coeff2",					0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lens Distortion",		"Chrom Aberration Coefficient 2",				-1.0f,		1.0f,		0.01f)},

	{TCVAR_LENS_ARTEFACTS_INTENSITY,				"lens_artefacts_intensity",					0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lens Artefacts",		"Lens Artefacts Global Multiplier",				0.0f,		1.0f,		0.01f)},
	{TCVAR_LENS_ARTEFACTS_INTENSITY_MIN_EXP,		"lens_artefacts_min_exp_intensity",			2.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lens Artefacts",		"Lens Artefacts Min Exposure Fade Mult",		0.0f,		10.0f,		0.01f)},
	{TCVAR_LENS_ARTEFACTS_INTENSITY_MAX_EXP,		"lens_artefacts_max_exp_intensity",			0.200f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Lens Artefacts",		"Lens Artefacts Max Exposure Fade Mult",		0.0f,		10.0f,		0.01f)},

	// blur vignetting
	{TCVAR_BLUR_VIGNETTING_RADIUS,					"blur_vignetting_radius",					0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Blur Vignetting",		"Radius",										0.0f,		500.0f,		0.01f)},
	{TCVAR_BLUR_VIGNETTING_INTENSITY,				"blur_vignetting_intensity",				0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Blur Vignetting",		"Intensity",									0.0f,		1.0f,		0.01f)},

	// screen blur
	{TCVAR_SCREEN_BLUR_INTENSITY,					"screen_blur_intensity",					0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Screen Blur",			"Intensity",									0.0f,		1.0f,		0.01f)},

	// sky variables
	// Sky colouring
	{TCVAR_SKY_ZENITH_TRANSITION_POSITION,			"sky_zenith_transition_position",			0.500f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"Zenith Transition (Vertical)",					0.0f, 		1.0f,		0.01f)},
	{TCVAR_SKY_ZENITH_TRANSITION_EAST_BLEND,		"sky_zenith_transition_east_blend",			0.500f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"Zenith Transition East Blend",					0.0f, 		1.0f,		0.01f)},
	{TCVAR_SKY_ZENITH_TRANSITION_WEST_BLEND,		"sky_zenith_transition_west_blend",			0.500f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"Zenith Transition West Blend",					0.0f, 		1.0f,		0.01f)},

	{TCVAR_SKY_ZENITH_BLEND_START,					"sky_zenith_blend_start",					0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"Zenith Blend Start",							0.0f, 		1.0f,		0.01f)},

	{TCVAR_SKY_ZENITH_COL_R,		 				"sky_zenith_col_r", 						0.500f,		VARTYPE_COL3,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"Zenith Color",									0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_ZENITH_COL_G, 						"sky_zenith_col_g", 						0.500f,		VARTYPE_NONE,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_ZENITH_COL_B, 						"sky_zenith_col_b",							0.500f,		VARTYPE_NONE,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_ZENITH_COL_INTEN, 					"sky_zenith_col_inten",						1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"Zenith Color Intensity",						0.0f,		64.0f,		0.01f)},

	{TCVAR_SKY_ZENITH_TRANSITION_COL_R,				"sky_zenith_transition_col_r", 				0.500f,		VARTYPE_COL3,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"Zenith Transition Color",						0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_ZENITH_TRANSITION_COL_G, 			"sky_zenith_transition_col_g", 				0.500f,		VARTYPE_NONE,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_ZENITH_TRANSITION_COL_B, 			"sky_zenith_transition_col_b",				0.500f,		VARTYPE_NONE,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_ZENITH_TRANSITION_COL_INTEN, 		"sky_zenith_transition_col_inten",			1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"Zenith Transition Intensity",					0.0f,		64.0f,		0.01f)},

	{TCVAR_SKY_AZIMUTH_TRANSITION_POSITION,			"sky_azimuth_transition_position",			0.500f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"Azimuth Transition (Horizontal)",				0.0f, 		1.0f,		0.01f)},

	{TCVAR_SKY_AZIMUTH_EAST_COL_R,	 				"sky_azimuth_east_col_r", 					0.500f,		VARTYPE_COL3,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"Azimuth East Color",							0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_AZIMUTH_EAST_COL_G, 					"sky_azimuth_east_col_g", 					0.500f,		VARTYPE_NONE,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_AZIMUTH_EAST_COL_B, 					"sky_azimuth_east_col_b", 					0.500f,		VARTYPE_NONE,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_AZIMUTH_EAST_COL_INTEN,	 			"sky_azimuth_east_col_inten",				1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"Azimuth East Intensity",						0.0f,		64.0f,		0.01f)},

	{TCVAR_SKY_AZIMUTH_TRANSITION_COL_R,			"sky_azimuth_transition_col_r",				0.500f,		VARTYPE_COL3,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"Azimuth Transition Color",						0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_AZIMUTH_TRANSITION_COL_G,			"sky_azimuth_transition_col_g",				0.500f,		VARTYPE_NONE,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_AZIMUTH_TRANSITION_COL_B,			"sky_azimuth_transition_col_b",				0.500f,		VARTYPE_NONE,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_AZIMUTH_TRANSITION_COL_INTEN,		"sky_azimuth_transition_col_inten",			1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"Azimuth Transition Intensity",					0.0f,		64.0f,		0.01f)},

	{TCVAR_SKY_AZIMUTH_WEST_COL_R,	 				"sky_azimuth_west_col_r", 					0.500f,		VARTYPE_COL3,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"Azimuth West Color",							0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_AZIMUTH_WEST_COL_G, 					"sky_azimuth_west_col_g", 					0.500f,		VARTYPE_NONE,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_AZIMUTH_WEST_COL_B, 					"sky_azimuth_west_col_b", 					0.500f,		VARTYPE_NONE,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_AZIMUTH_WEST_COL_INTEN,				"sky_azimuth_west_col_inten",				1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"Azimuth West Intensity",						0.0f,		64.0f,		0.01f)},

	{TCVAR_SKY_HDR,									"sky_hdr",									7.0f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"HDR Intensity",								0.0f, 		255.0f,		0.1f)},

	{TCVAR_SKY_PLANE_COL_R,	 						"sky_plane_r", 								0.000f,		VARTYPE_COL3,		true,		MODTYPE_INTERP_MULT BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"Plane Color",									0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_PLANE_COL_G, 						"sky_plane_g", 								0.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_PLANE_COL_B, 						"sky_plane_b", 								0.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_PLANE_INTEN,							"sky_plane_inten",							1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Colour",			"Plane Intensity",								0.0f, 		32.0f,		0.01f)},


	// Sun variables
	{TCVAR_SKY_SUN_COL_R,							"sky_sun_col_r",							0.500f,		VARTYPE_COL3,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Sun",			"Sun Color",									0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_SUN_COL_G,							"sky_sun_col_g",							0.500f,		VARTYPE_NONE,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Sun",			"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_SUN_COL_B,							"sky_sun_col_b",							0.500f,		VARTYPE_NONE,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Sun",			"",												0.0f,		0.0f,		0.0f)},

	{TCVAR_SKY_SUN_DISC_COL_R,						"sky_sun_disc_col_r",						0.000f,		VARTYPE_COL3,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Sun",			"Sun Disc Color",								0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_SUN_DISC_COL_G,						"sky_sun_disc_col_g",						0.000f,		VARTYPE_NONE,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Sun",			"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_SUN_DISC_COL_B,						"sky_sun_disc_col_b",						0.000f,		VARTYPE_NONE,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Sun",			"",												0.0f,		0.0f,		0.0f)},

	{TCVAR_SKY_SUN_DISC_SIZE,						"sky_sun_disc_size",						2.5f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Sun",			"Disc Size",									0.0f, 		16.0f,		0.1f)},

	{TCVAR_SKY_SUN_HDR,								"sky_sun_hdr",								10.0f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Sun",			"HDR Intensity",								0.0f, 		255.0f,		0.1f)},

	{TCVAR_SKY_SUN_MIEPHASE,						"sky_sun_miephase",							1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Sun",			"Mie Phase",									0.0f, 		1.0f,		0.01f)},
	{TCVAR_SKY_SUN_MIESCATTER,						"sky_sun_miescatter",						1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Sun",			"Mie Scatter",									1.0f, 		1024.0f,	1.00f)},
	{TCVAR_SKY_SUN_MIE_INTENSITY_MULT,				"sky_sun_mie_intensity_mult",				1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Sun",			"Mie Intensity Mult",							0.0f, 		32.0f,		0.01f)},

	{TCVAR_SKY_SUN_INFLUENCE_RADIUS,				"sky_sun_influence_radius",					5.0f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Sun",			"Influence Radius",								0.0f, 		128.0f,		0.01f)},
	{TCVAR_SKY_SUN_SCATTER_INTEN,					"sky_sun_scatter_inten",					2.0f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Sun",			"Scatter Intensity",							0.0f, 		128.0f,		0.01f)},

	// Stars / Moon variables
	{TCVAR_SKY_MOON_COL_R,							"sky_moon_col_r",							1.000f,		VARTYPE_COL3,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Stars / Moon",	"Moon Color",									0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_MOON_COL_G,							"sky_moon_col_g",							1.000f,		VARTYPE_NONE,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Stars / Moon",	"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_MOON_COL_B,							"sky_moon_col_b",							1.000f,		VARTYPE_NONE,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Stars / Moon",	"",												0.0f,		0.0f,		0.0f)},

	{TCVAR_SKY_MOON_DISC_SIZE,						"sky_moon_disc_size",						2.5f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Stars / Moon",	"Disc Size",									0.0f, 		16.0f,		0.01f)},

	{TCVAR_SKY_MOON_ITEN,							"sky_moon_iten",							1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Stars / Moon",	"Moon Intensity",								0.0f, 		16.0f,		0.01f)},
	{TCVAR_SKY_STARS_ITEN,							"sky_stars_iten",							1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Stars / Moon",	"Starfield Intensity",							0.0f, 		16.0f,		0.01f)},

	{TCVAR_SKY_MOON_INFLUENCE_RADIUS,				"sky_moon_influence_radius",				5.0f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Stars / Moon",	"Moon Influence Radius",						0.0f, 		128.0f,		0.01f)},
	{TCVAR_SKY_MOON_SCATTER_INTEN,					"sky_moon_scatter_inten",					2.0f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Stars / Moon",	"Moon Scatter Intensity",						0.0f, 		64.0f,		0.01f)},

	// Cloud variables
	// Cloud generation
	{TCVAR_SKY_CLOUD_GEN_FREQUENCY,					"sky_cloud_gen_frequency",					2.0f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Cloud Gen",		"Frequency",									0.0f, 		16.0f,		0.01f)},
	{TCVAR_SKY_CLOUD_GEN_SCALE,						"sky_cloud_gen_scale",						64.0f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Cloud Gen",		"Scale",										1.0f, 		256.0f,		0.01f)},
	{TCVAR_SKY_CLOUD_GEN_THRESHOLD,					"sky_cloud_gen_threshold",					0.65f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Cloud Gen",		"Threshold",									0.0f, 		1.0f,		0.01f)},
	{TCVAR_SKY_CLOUD_GEN_SOFTNESS,					"sky_cloud_gen_softness",					0.2f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Cloud Gen",		"Softness",										0.0f, 		1.0f,		0.01f)},
	{TCVAR_SKY_CLOUD_DENSITY_MULT,					"sky_cloud_density_mult",					2.24f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Cloud Gen",		"Density Multiplier",							0.0f, 		6.0f,		0.01f)},
	{TCVAR_SKY_CLOUD_DENSITY_BIAS,					"sky_cloud_density_bias",					0.66f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Cloud Gen",		"Density Bias",									0.0f, 		1.0f,		0.01f)},

	// Main clouds
	{TCVAR_SKY_CLOUD_MID_COL_R,						"sky_cloud_mid_col_r",						0.500f,		VARTYPE_COL3,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Main Clouds",	"Mid Color",									0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_CLOUD_MID_COL_G,						"sky_cloud_mid_col_g",						0.500f,		VARTYPE_NONE,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Main Clouds",	"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_CLOUD_MID_COL_B,						"sky_cloud_mid_col_b",						0.500f,		VARTYPE_NONE,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Main Clouds",	"",												0.0f,		0.0f,		0.0f)},

	{TCVAR_SKY_CLOUD_BASE_COL_R,					"sky_cloud_base_col_r",						0.500f,		VARTYPE_COL3,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Main Clouds",	"Base Color",									0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_CLOUD_BASE_COL_G,					"sky_cloud_base_col_g",						0.500f,		VARTYPE_NONE,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Main Clouds",	"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_CLOUD_BASE_COL_B,					"sky_cloud_base_col_b",						0.500f,		VARTYPE_NONE,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Main Clouds",	"",												0.0f,		0.0f,		0.0f)},

	{TCVAR_SKY_CLOUD_BASE_STRENGTH,					"sky_cloud_base_strength",					0.9f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Main Clouds",	"Base Strength",								0.0f, 		1.0f,		0.01f)},

	{TCVAR_SKY_CLOUD_SHADOW_COL_R,					"sky_cloud_shadow_col_r",					0.500f,		VARTYPE_COL3,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Main Clouds",	"Shadow Color",									0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_CLOUD_SHADOW_COL_G,					"sky_cloud_shadow_col_g",					0.500f,		VARTYPE_NONE,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Main Clouds",	"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_CLOUD_SHADOW_COL_B,					"sky_cloud_shadow_col_b",					0.500f,		VARTYPE_NONE,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Main Clouds",	"",												0.0f,		0.0f,		0.0f)},

	{TCVAR_SKY_CLOUD_SHADOW_STRENGTH,				"sky_cloud_shadow_strength",				0.2f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Main Clouds",	"Shadow Strength",								-1.0f, 		1.0f,		0.01f)},
	{TCVAR_SKY_CLOUD_GEN_DENSITY_OFFSET,			"sky_cloud_gen_density_offset",				0.15f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Main Clouds",	"Shadow Threshold",								0.0f, 		1.0f,		0.01f)},
	{TCVAR_SKY_CLOUD_OFFSET,						"sky_cloud_offset",							0.1f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Main Clouds",	"Color / Shadow Offset",						0.0f, 		1.0f,		0.01f)},

	{TCVAR_SKY_CLOUD_OVERALL_STRENGTH,				"sky_cloud_overall_strength",				0.5f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Main Clouds",	"Detail Overlay",								0.0f, 		1.0f,		0.01f)},
	{TCVAR_SKY_CLOUD_OVERALL_COLOR,					"sky_cloud_overall_color",					0.5f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Main Clouds",	"Detail Overlay Color",							0.0f, 		1.0f,		0.01f)},
	{TCVAR_SKY_CLOUD_EDGE_STRENGTH,					"sky_cloud_edge_strength",					0.5f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Main Clouds",	"Edge Detail",									0.0f, 		1.0f,		0.01f)},

	{TCVAR_SKY_CLOUD_FADEOUT,						"sky_cloud_fadeout",						0.3f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Main Clouds",	"Fadeout",										0.0f, 		1.0f,		0.01f)},
	{TCVAR_SKY_CLOUD_HDR,							"sky_cloud_hdr",							0.8f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Main Clouds",	"HDR Intensity",								0.0f, 		256.0f,		0.01f)},
	{TCVAR_SKY_CLOUD_DITHER_STRENGTH,				"sky_cloud_dither_strength",				50.0f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Main Clouds",	"Dither Strength",								0.0f, 		2048.0f,	0.1f)},

	{TCVAR_SKY_SMALL_CLOUD_COL_R,					"sky_small_cloud_col_r",					0.500f,		VARTYPE_COL3,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Filler Clouds",	"Cloud Color",									0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_SMALL_CLOUD_COL_G,					"sky_small_cloud_col_g",					0.500f,		VARTYPE_NONE,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Filler Clouds",	"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_SKY_SMALL_CLOUD_COL_B,					"sky_small_cloud_col_b",					0.500f,		VARTYPE_NONE,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Filler Clouds",	"",												0.0f,		0.0f,		0.0f)},

	{TCVAR_SKY_SMALL_CLOUD_DETAIL_STRENGTH,			"sky_small_cloud_detail_strength",			0.5f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Filler Clouds",	"Strength",										0.0f, 		1.0f,		0.01f)},
	{TCVAR_SKY_SMALL_CLOUD_DETAIL_SCALE,			"sky_small_cloud_detail_scale",				3.6f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Filler Clouds",	"Scale",										0.0f, 		256.0f,		0.01f)},
	{TCVAR_SKY_SMALL_CLOUD_DENSITY_MULT,			"sky_small_cloud_density_mult",				1.7f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Filler Clouds",	"Density Multiplier",							0.0f, 		6.0f,		0.01f)},
	{TCVAR_SKY_SMALL_CLOUD_DENSITY_BIAS,			"sky_small_cloud_density_bias",				0.558f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sky - Filler Clouds",	"Density Bias",									0.0f, 		6.0f,		0.01f)},

	// cloud shadows
	{TCVAR_CLOUD_SHADOW_DENSITY,					"cloud_shadow_density",						0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Cloud Shadows",		"Density",										0.0f, 		1.0f,		0.01f)},
	{TCVAR_CLOUD_SHADOW_SOFTNESS,					"cloud_shadow_softness",					0.1f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Cloud Shadows",		"Softness",										0.0f, 		1.0f,		0.01f)},
	{TCVAR_CLOUD_SHADOW_OPACITY,					"cloud_shadow_opacity",						1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_NONE		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Cloud Shadows",		"Opacity",										0.0f, 		1.0f,		0.01f)},

	// directional (cascade) shadows
	{TCVAR_DIRECTIONAL_SHADOW_NUM_CASCADES,			"dir_shadow_num_cascades",					4.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Directional Shadows",	"Num Cascades",									1.0f, 		4.0f,		1.0f)},
	{TCVAR_DIRECTIONAL_SHADOW_DISTANCE_MULTIPLIER,	"dir_shadow_distance_multiplier",			1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Directional Shadows",	"Distance Multiplier",							0.01f, 		10.0f,		0.01f)},
	{TCVAR_DIRECTIONAL_SHADOW_SOFTNESS,				"dir_shadow_softness",						0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Directional Shadows",	"Softness",									   -1.00f, 		1.0f,		0.1f)},
	{TCVAR_DIRECTIONAL_SHADOW_CASCADE0_SCALE,		"dir_shadow_cascade0_scale",				1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Directional Shadows",	"Cascade0 Scale",							    0.01f, 		3.0f,		0.1f)},


	// sprite (corona and distant lights) variables
	{TCVAR_SPRITE_BRIGHTNESS,						"sprite_brightness",						1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sprites",				"Brightness",									0.0f, 		100.0f, 	1.0f)},
	{TCVAR_SPRITE_SIZE,								"sprite_size",								1.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sprites",				"Scale",										0.0f, 		100.0f, 	1.0f)},
	{TCVAR_SPRITE_CORONA_SCREENSPACE_EXPANSION,		"sprite_corona_screenspace_expansion",		0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sprites",				"Corona Screenspace Expansion",					0.0f,		100.0f,		0.1f)},
	{TCVAR_LENSFLARE_VISIBILITY,					"Lensflare_visibility",						1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sprites",				"Lensflare Visibility Mult",					0.0f,		10.0f,		0.1f)},
	{TCVAR_SPRITE_DISTANT_LIGHT_TWINKLE,			"sprite_distant_light_twinkle",				1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Sprites",				"Distant Lights Twinkle Mult",					0.0f,		1.0f,		0.1f)},

	// water variables
	{TCVAR_WATER_REFLECTION,						"water_reflection",							0.25f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Reflection",									0.0f,		3.0f,		0.05f)},
	{TCVAR_WATER_REFLECTION_FAR_CLIP,				"water_reflection_far_clip",				7000.0f,	VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Reflection Far Clip",							0.0f,		7000.0f,	0.05f)},
	{TCVAR_WATER_REFLECTION_LOD,					"water_reflection_lod",						0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Reflection LOD",								0.0f,		5.0f,		1.00f)},
	{TCVAR_WATER_REFLECTION_SKY_FLOD_RANGE,			"water_reflection_sky_flod_range",			0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Reflection Sky FLOD Range",					0.0f,		1.0f,		0.01f)},
	{TCVAR_WATER_REFLECTION_LOD_RANGE_ENABLED,		"water_reflection_lod_range_enabled",		0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Reflection LOD Range Enabled",					0.0f,		1.0f,		0.25f)},
	{TCVAR_WATER_REFLECTION_LOD_RANGE_HD_START,		"water_reflection_lod_range_hd_start",		-1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Reflection LOD Range HD Start",				-1.0f,		16000.0f,	0.25f)},
	{TCVAR_WATER_REFLECTION_LOD_RANGE_HD_END,		"water_reflection_lod_range_hd_end",		-1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Reflection LOD Range HD End",					-1.0f,		16000.0f,	0.25f)},
	{TCVAR_WATER_REFLECTION_LOD_RANGE_ORPHANHD_START,"water_reflection_lod_range_orphanhd_start",-1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Reflection LOD Range Orphan HD Start",			-1.0f,		16000.0f,	0.25f)},
	{TCVAR_WATER_REFLECTION_LOD_RANGE_ORPHANHD_END,	 "water_reflection_lod_range_orphanhd_end",	 -1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Reflection LOD Range Orphan HD End",			-1.0f,		16000.0f,	0.25f)},
	{TCVAR_WATER_REFLECTION_LOD_RANGE_LOD_START,	"water_reflection_lod_range_lod_start",		-1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Reflection LOD Range LOD Start",				-1.0f,		16000.0f,	0.25f)},
	{TCVAR_WATER_REFLECTION_LOD_RANGE_LOD_END,		"water_reflection_lod_range_lod_end",		-1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Reflection LOD Range LOD End",					-1.0f,		16000.0f,	0.25f)},
	{TCVAR_WATER_REFLECTION_LOD_RANGE_SLOD1_START,	"water_reflection_lod_range_slod1_start",	-1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Reflection LOD Range SLOD1 Start",				-1.0f,		16000.0f,	0.25f)},
	{TCVAR_WATER_REFLECTION_LOD_RANGE_SLOD1_END,	"water_reflection_lod_range_slod1_end",		-1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Reflection LOD Range SLOD1 End",				-1.0f,		16000.0f,	0.25f)},
	{TCVAR_WATER_REFLECTION_LOD_RANGE_SLOD2_START,	"water_reflection_lod_range_slod2_start",	-1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Reflection LOD Range SLOD2 Start",				-1.0f,		16000.0f,	0.25f)},
	{TCVAR_WATER_REFLECTION_LOD_RANGE_SLOD2_END,	"water_reflection_lod_range_slod2_end",		-1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Reflection LOD Range SLOD2 End",				-1.0f,		16000.0f,	0.25f)},
	{TCVAR_WATER_REFLECTION_LOD_RANGE_SLOD3_START,	"water_reflection_lod_range_slod3_start",	-1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Reflection LOD Range SLOD3 Start",				-1.0f,		16000.0f,	0.25f)},
	{TCVAR_WATER_REFLECTION_LOD_RANGE_SLOD3_END,	"water_reflection_lod_range_slod3_end",		-1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Reflection LOD Range SLOD3 End",				-1.0f,		16000.0f,	0.25f)},
	{TCVAR_WATER_REFLECTION_LOD_RANGE_SLOD4_START,	"water_reflection_lod_range_slod4_start",	-1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Reflection LOD Range SLOD4 Start",				-1.0f,		16000.0f,	0.25f)},
	{TCVAR_WATER_REFLECTION_LOD_RANGE_SLOD4_END,	"water_reflection_lod_range_slod4_end",		-1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Reflection LOD Range SLOD4 End",				-1.0f,		16000.0f,	0.25f)},
	{TCVAR_WATER_REFLECTION_HEIGHT_OFFSET,			"water_reflection_height_offset",			0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Reflection Height Offset",						-1000.0f,	1000.0f,	0.01f)},
	{TCVAR_WATER_REFLECTION_HEIGHT_OVERRIDE,		"water_reflection_height_override",			0.0f,		VARTYPE_FLOAT,		false,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Reflection Height Override",					-1000.0f,	1000.0f,	0.01f)},
	{TCVAR_WATER_REFLECTION_HEIGHT_OVERRIDE_AMOUNT,	"water_reflection_height_override_amount",	0.0f,		VARTYPE_FLOAT,		false,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Reflection Height Override Amount",			0.0f,		1.0f,		0.01f)},
	{TCVAR_WATER_REFLECTION_DISTANT_LIGHT_INTENSITY,"water_reflection_distant_light_intensity",	1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Reflection Distant Light Intensity",			0.0f,		5.0f,		0.01f)},
	{TCVAR_WATER_REFLECTION_CORONA_INTENSITY,		"water_reflection_corona_intensity",		1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Reflection Corona Intensity",					0.0f,		5.0f,		0.01f)},

	{TCVAR_WATER_FOGLIGHT,							"water_foglight",							1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Fog Light",									0.0f,		5.0f,		0.05f)},
	{TCVAR_WATER_INTERIOR,							"water_interior",							0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Fog Interior",									0.0f,		1.0f,		0.1f)},
	{TCVAR_WATER_ENABLEFOGSTREAMING,				"water_fogstreaming",						1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Enable Fog Streaming",							0.0f,		1.0f,		0.1f)},
	{TCVAR_WATER_FOAM_INTENSITY_MULT,				"water_foam_intensity_mult",				1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Foam Intensity Mult",							0.0f,		10.0f,		0.1f)},
	{TCVAR_WATER_DRYING_SPEED_MULT,					"water_drying_speed_mult",					1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Drying Speed Mult",							0.01f,		2.0f,		0.01f)},
	{TCVAR_WATER_SPECULAR_INTENSITY,				"water_specular_intensity",					3.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Water",				"Specular Intensity",							0.0f,		400.0f,		0.1f)},

	// mirror variables
	{TCVAR_MIRROR_REFLECTION_LOCAL_LIGHT_INTENSITY,	"mirror_reflection_local_light_intensity",	1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Mirror",				"Reflection Local Light Intensity",				0.0f,		2.0f,		0.01f)},

	// fog variables
	{TCVAR_FOG_START,								"fog_start",								73.000f,	VARTYPE_FLOAT,		true,		MODTYPE_CLAMP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Start",										-1000.0f,	5000.0f,	1.0f)},
	{TCVAR_FOG_NEAR_COL_R, 							"fog_near_col_r", 							0.012f,		VARTYPE_COL4_LIN,	true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Near Colour",									0.0f,		0.0f,		0.0f)},
	{TCVAR_FOG_NEAR_COL_G, 							"fog_near_col_g", 							0.604f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_FOG_NEAR_COL_B, 							"fog_near_col_b", 							1.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_FOG_NEAR_COL_A, 							"fog_near_col_a", 							0.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"",												0.0f,		0.0f,		0.0f)},

	{TCVAR_FOG_SUN_COL_R,							"fog_col_r",								0.412f,		VARTYPE_COL4_LIN,	true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Sun Colour",									0.0f,		0.0f,		0.0f)},
	{TCVAR_FOG_SUN_COL_G,							"fog_col_g",								0.584f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_FOG_SUN_COL_B, 							"fog_col_b",								0.780f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_FOG_SUN_COL_A, 							"fog_col_a",								0.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_FOG_SUN_LIGHTING_CALC_POW, 				"fog_sun_lighting_calc_pow",				8.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Sun lighting calc power",						0.0f,		0.0f,		0.0f)},

	{TCVAR_FOG_MOON_COL_R,							"fog_moon_col_r",							0.5f,		VARTYPE_COL4_LIN,	true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Moon Colour",									0.0f,		0.0f,		0.0f)},
	{TCVAR_FOG_MOON_COL_G,							"fog_moon_col_g",							0.5f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_FOG_MOON_COL_B, 							"fog_moon_col_b",							0.5f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_FOG_MOON_COL_A, 							"fog_moon_col_a",							0.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_FOG_MOON_LIGHTING_CALC_POW, 				"fog_moon_lighting_calc_pow",				18.000f,	VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Moon ligting calc power",						0.0f,		0.0f,		0.0f)},

	{TCVAR_FOG_FAR_COL_R, 							"fog_east_col_r", 							0.412f,		VARTYPE_COL4_LIN,	true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Far Colour",									0.0f,		0.0f,		0.0f)},
	{TCVAR_FOG_FAR_COL_G, 							"fog_east_col_g", 							0.584f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_FOG_FAR_COL_B, 							"fog_east_col_b", 							0.780f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_FOG_FAR_COL_A, 							"fog_east_col_a", 							0.000f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"",												0.0f,		0.0f,		0.0f)},

	{TCVAR_FOG_DENSITY, 							"fog_density", 								1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_CLAMP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Fog Density",									0.0f,		10000.0f,	0.001f)},
	{TCVAR_FOG_HEIGHT_FALLOFF, 						"fog_falloff", 								0.5f,		VARTYPE_FLOAT,		true,		MODTYPE_CLAMP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Fog Height Falloff",							0.0f,		1000.0f,	0.001f)},
	{TCVAR_FOG_BASE_HEIGHT,							"fog_base_height",							0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_CLAMP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Fog Base Height",								-1000.0f,	1000.0f,	1.0f)},

	{TCVAR_FOG_ALPHA,								"fog_alpha",								1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_CLAMP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Fog Alpha",									0.0f,		1.0f,		0.001f)},
	{TCVAR_FOG_HORIZON_TINT_SCALE,					"fog_horizon_tint_scale",					4.0f,		VARTYPE_FLOAT,		true,		MODTYPE_CLAMP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Fog Horizon Tint Scale",						0.0f,		100.0f,		0.01f)},
	{TCVAR_FOG_HDR,									"fog_hdr",									1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Fog HDR Intensity",							0.0f, 		255.0f,		0.1f)},

	{TCVAR_FOG_HAZE_COL_R, 							"fog_haze_col_r", 							0.412f,		VARTYPE_COL3_LIN,	true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Haze Colour",									0.0f,		0.0f,		0.0f)},
	{TCVAR_FOG_HAZE_COL_G, 							"fog_haze_col_g", 							0.584f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_FOG_HAZE_COL_B, 							"fog_haze_col_b", 							0.780f,		VARTYPE_NONE,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"",												0.0f,		0.0f,		0.0f)},
	{TCVAR_FOG_HAZE_DENSITY, 						"fog_haze_density", 						0.5f,		VARTYPE_FLOAT,		true,		MODTYPE_CLAMP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Haze Density",									0.0f,		1000.0f,	0.001f)},
	{TCVAR_FOG_HAZE_ALPHA,							"fog_haze_alpha",							1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_CLAMP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Haze Alpha",									0.0f,		1.0f,		0.010f)},
	{TCVAR_FOG_HAZE_HDR,							"fog_haze_hdr",								1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Haze HDR Intensity",							0.0f, 		255.0f,		0.1f)},
	{TCVAR_FOG_HAZE_START,							"fog_haze_start",							0.000f,		VARTYPE_FLOAT,		true,		MODTYPE_CLAMP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Haze Start",									-0.0f,		5000.0f,	1.0f)},

	// Linear piecewise fog.
	{TCVAR_FOG_SHAPE_LOW,							"fog_shape_bottom",							0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Fog shape bottom",								-1000.0f,	10000.0f,	1.0f)},
	{TCVAR_FOG_SHAPE_HIGH,							"fog_shape_top",							0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Fog shape top",								-1000.0f,	10000.0f,	1.0f)},
	{TCVAR_FOG_LOG_10_MIN_VISIBILIY,				"fog_shape_log_10_of_visibility",			5.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Fog log10(visibility)",						0.0f,		5.0f,		0.001f)},
	{TCVAR_FOG_SHAPE_WEIGHT_0,						"fog_shape_weight_0",						1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"weight 0",										0.0f,		1.0f,		0.001f)},
	{TCVAR_FOG_SHAPE_WEIGHT_1,						"fog_shape_weight_1",						0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"weight 0",										0.0f,		1.0f,		0.001f)},
	{TCVAR_FOG_SHAPE_WEIGHT_2,						"fog_shape_weight_2",						0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"weight 0",										0.0f,		1.0f,		0.001f)},
	{TCVAR_FOG_SHAPE_WEIGHT_3,						"fog_shape_weight_3",						0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"weight 0",										0.0f,		1.0f,		0.001f)},

	// fog shadows
	{TCVAR_FOG_SHADOW_AMOUNT,						"fog_shadow_amount",						0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Fog Shadow Amount",							0.0f,		1.0f,		0.010f)},
	{TCVAR_FOG_SHADOW_FALLOFF,						"fog_shadow_falloff",						1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Fog Shadow Falloff",							0.0f, 		1024.0f,	0.010f)},
	{TCVAR_FOG_SHADOW_BASE_HEIGHT,					"fog_shadow_base_height",					0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Fog Shadow Base Height",						-1024.0f,	1024.0f,	0.010f)},

	// volumetric lights,
	{TCVAR_FOG_VOLUME_RANGE,						"fog_volume_light_range",					50.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Fog Volumetric Lights Range",					0.0f,		3000.0f,	0.1f)},
	{TCVAR_FOG_VOLUME_FADE_RANGE,					"fog_volume_light_fade",					50.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Fog Volumetric Lights Fade Range",				0.0f,		3000.0f,	0.1f)},
	{TCVAR_FOG_VOLUME_INTENSITY_SCALE,				"fog_volume_light_intensity",				0.025f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Fog Volumetric Lights Intensity Scale",		0.0f,		2.0f,		0.010f)},
	{TCVAR_FOG_VOLUME_SIZE_SCALE,					"fog_volume_light_size",					1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Fog Volumetric Lights Size Scale",				0.0f,		2.0f,		0.010f)},

	// fog ray
	{TCVAR_FOG_FOGRAY_CONTRAST,						"fogray_contrast",						1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Fog Ray Contrast",							0.0f,		10.0f,		0.010f)},
	{TCVAR_FOG_FOGRAY_INTENSITY,					"fogray_intensity",						0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Fog Ray Intensity",							0.0f,		1.0f,		0.010f)},
	{TCVAR_FOG_FOGRAY_DENSITY,						"fogray_density",						1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Fog Ray Density",							0.0f,		20.0f,		0.010f)},
	{TCVAR_FOG_FOGRAY_NEARFADE,						"fogray_nearfade",						0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Fog Ray NearFade",							0.0f,		20.0f,		0.010f)},
	{TCVAR_FOG_FOGRAY_FARFADE,						"fogray_farfade",						1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Fog",					"Fog Ray FarFade",							0.0f,		200.0f,		0.010f)},

	// refection variables
	{TCVAR_REFLECTION_LOD_RANGE_START,				"reflection_lod_range_start",				0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Reflection",			"Reflection LOD range - start",					-1.0f,		4096.0f,	1.0f)},
	{TCVAR_REFLECTION_LOD_RANGE_END,				"reflection_lod_range_end",					50.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Reflection",			"Reflection LOD range - end",					-1.0f,		4096.0f,	1.0f)},
	{TCVAR_REFLECTION_SLOD_RANGE_START,				"reflection_slod_range_start",				150.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Reflection",			"Reflection SLOD range - start",				-1.0f,		4096.0f,	1.0f)},
	{TCVAR_REFLECTION_SLOD_RANGE_END,				"reflection_slod_range_end",				3000.0f,	VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Reflection",			"Reflection SLOD range - end",					-1.0f,		4096.0f,	1.0f)},
	{TCVAR_REFLECTION_INTERIOR_RANGE,				"reflection_interior_range",				10.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Reflection",			"Reflection Interior range",					-1.0f,		4096.0f,	1.0f)},
	{TCVAR_REFLECTION_TWEAK_INTERIOR_AMB,			"reflection_tweak_interior_amb",			2.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Reflection",			"Reflection Interior Ambient Boost",			 0.0f,		64.0f,		1.0f)},
	{TCVAR_REFLECTION_TWEAK_EXTERIOR_AMB,			"reflection_tweak_exterior_amb",			2.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Reflection",			"Reflection Exterior Ambient Boost",			 0.0f,		64.0f,		1.0f)},
	{TCVAR_REFLECTION_TWEAK_EMISSIVE,				"reflection_tweak_emissive",				12.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Reflection",			"Reflection Emissive Boost",					 0.0f,		64.0f,		1.0f)},
	{TCVAR_REFLECTION_TWEAK_DIRECTIONAL,			"reflection_tweak_directional",				1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Reflection",			"Reflection Directional Multiplier",			 0.0f,		1.0f,		0.01f)},
	{TCVAR_REFLECTION_HDR_MULT,						"reflection_hdr_mult",						1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Reflection",			"Reflection HDR Mult",							 0.0f, 		1.0f,		0.1f)},

	// misc variables
	{TCVAR_FAR_CLIP,								"far_clip",									1500.000f,	VARTYPE_FLOAT,		false,		MODTYPE_CLAMP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Misc",					"Far Clip",										0.0f,		10000.0f,	1.0f)},
	{TCVAR_TEMPERATURE,								"temperature",								10.000f,	VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Misc",					"Temperature",									-40.0f,		50.0f,		1.0f)},
	{TCVAR_PTFX_EMISSIVE_INTENSITY_MULT,			"particle_emissive_intensity_mult",			1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Misc",					"Particle Emissive Intensity Mult",				0.0f,		10.0f,		0.1f)},
	{TCVAR_VFXLIGHTNING_INTENSITY_MULT,				"vfxlightning_intensity_mult",				1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Misc",					"Vfx Lightning Intensity Mult",					0.0f,		50.0f,		0.1f)},	
	{TCVAR_VFXLIGHTNING_VISIBILITY,					"vfxlightning_visibility",					1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Misc",					"Vfx Lightning Visibility",						0.0f,		1.0f,		0.1f)},	
	{TCVAR_GPUPTFX_LIGHT_INTENSITY_MULT,			"particle_light_intensity_mult",			0.800f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP_MULT	BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Misc",					"GPU Particle Light Intensity Mult",			0.0f,		64.0f,		0.05f)},
	{TCVAR_NATURAL_AMBIENT_MULTIPLIER, 				"natural_ambient_multiplier",				1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Misc",					"Natural Ambient Multiplier",					0.0f,		1.0f,		0.1f)},
	{TCVAR_ARTIFICIAL_INT_AMBIENT_MULTIPLIER, 		"artificial_int_ambient_multiplier",		0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Misc",					"Artifical Ambient Multiplier",					0.0f,		1.0f,		0.1f)},
	{TCVAR_FOG_CUT_OFF, 							"fog_cut_off", 								1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Misc",					"Fog Cut Off",									0.0f,		1.0f,		0.1f)},
	{TCVAR_NO_WEATHER_FX,							"no_weather_fx",							0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_MAX			BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Misc",					"No Weather Fx",								0.0f,		1.0f,		1.0f)},
	{TCVAR_NO_GPU_FX,								"no_gpu_fx",								0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_MAX			BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Misc",					"No GPU Fx",									0.0f,		1.0f,		1.0f)},
	{TCVAR_NO_RAIN,									"no_rain",									0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_MAX			BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Misc",					"No Rain",										0.0f,		1.0f,		1.0f)},
	{TCVAR_NO_RAIN_RIPPLES,							"no_rain_ripples",							0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_MAX			BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Misc",					"No Rain Ripple",								0.0f,		1.0f,		1.0f)},
	{TCVAR_FOGVOLUME_DENSITY_SCALAR,				"fogvolume_density_scalar",					1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_MAX			BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Misc",					"FogVolume Density Scalar",						0.0f,		10.0f,		0.01f)},
	{TCVAR_FOGVOLUME_DENSITY_SCALAR_INTERIOR,		"fogvolume_density_scalar_interior",		1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_MAX			BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Misc",					"FogVolume Density Scalar Interior",			0.0f,		10.0f,		0.01f)},
	{TCVAR_FOGVOLUME_FOG_SCALER,					"fogvolume_fog_scaler",						1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_MAX			BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Misc",					"FogVolume Fog Scaler",							0.0f,		10.0f,		0.01f)},
	{TCVAR_TIME_OFFSET,								"time_offset",								0.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Misc",					"Time Offset",									-120.0f,	120.0f,		1.0f)},
	{TCVAR_VEHICLE_DIRT_MOD,						"vehicle_dirt_mod",							1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_BASIC,		"Misc",					"Vehicle Dirt Mod",								0.0f,		1.0f,		0.05f)},
	{TCVAR_WIND_SPEED_MULT,							"wind_speed_mult",							1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Misc",					"Wind Speed Mult",								0.0f,		1.0f,		0.01f)},
	{TCVAR_ENTITY_REJECT,							"entity_reject",							16000.0f,	VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Misc",					"Entity reject",								1.0f,		16000.0f,	1.0f)},
	{TCVAR_LOD_MULT,								"lod_mult",									1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"LOD",					"Lod Mult (Global)",							0.1f,		2.0f,		0.1f)},
	{TCVAR_ENABLE_OCCLUSION,						"enable_occlusion",							1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_MAX			BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Misc",					"Enable Occlusion",								0.0f,		1.0f,		1.0f)},
	{TCVAR_ENABLE_SHADOW_OCCLUSION,					"enable_shadow_occlusion",					1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_MAX			BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Misc",					"Enable Shadow Occlusion",						0.0f,		1.0f,		1.0f)},
	{TCVAR_ENABLE_EXTERIOR,							"render_exterior",							1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_MAX			BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Misc",					"Enable Exterior",								0.0f,		1.0f,		1.0f)},
	{TCVAR_PORTAL_WEIGHT,							"portal_weight",							1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_MAX			BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"Misc",					"Portal Weight",								1.0f,		10.0f,		0.1f)},

	{TCVAR_LIGHT_FALLOFF_MULT,						"light_falloff_mult",						1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"LOD",					"Light FallOff Mult",							0.001f,		1.0f,		0.01f)},
	{TCVAR_LODLIGHT_RANGE_MULT,						"lodlight_range_mult",						1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"LOD",					"LOD Light Range Mult",							0.001f,		1.0f,		0.01f)},
	{TCVAR_SHADOW_DISTANCE_MULT,					"shadow_distance_mult",						1.0f,		VARTYPE_FLOAT,		true,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"LOD",					"Shadow Distance Mult",							0.0f,		1.0f,		0.1f)},
	{TCVAR_LOD_MULT_HD,								"lod_mult_hd",								1.0f,		VARTYPE_FLOAT,		false,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"LOD",					"Lod Mult HD",									0.1f,		10.0f,		0.1f)},
	{TCVAR_LOD_MULT_ORPHANHD,						"lod_mult_orphanhd",						1.0f,		VARTYPE_FLOAT,		false,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"LOD",					"Lod Mult orphan HD",							0.1f,		10.0f,		0.1f)},
	{TCVAR_LOD_MULT_LOD,							"lod_mult_lod",								1.0f,		VARTYPE_FLOAT,		false,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"LOD",					"Lod Mult LOD",									0.1f,		10.0f,		0.1f)},
	{TCVAR_LOD_MULT_SLOD1,							"lod_mult_slod1",							1.0f,		VARTYPE_FLOAT,		false,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"LOD",					"Lod Mult SLOD1",								0.1f,		10.0f,		0.1f)},
	{TCVAR_LOD_MULT_SLOD2,							"lod_mult_slod2",							1.0f,		VARTYPE_FLOAT,		false,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"LOD",					"Lod Mult SLOD2",								0.1f,		10.0f,		0.1f)},
	{TCVAR_LOD_MULT_SLOD3,							"lod_mult_slod3",							1.0f,		VARTYPE_FLOAT,		false,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"LOD",					"Lod Mult SLOD3",								0.1f,		10.0f,		0.1f)},
	{TCVAR_LOD_MULT_SLOD4,							"lod_mult_slod4",							1.0f,		VARTYPE_FLOAT,		false,		MODTYPE_INTERP		BANK_ONLY_PARAM6(	MODMENU_DEFAULT,	"LOD",					"Lod Mult SLOD4",								0.1f,		10.0f,		0.1f)},
};
CompileTimeAssert(NELEM(g_varInfos) == TCVAR_NUM);

#if GTA_REPLAY
//interior modifiers that we want to disable
CTimeCycle::EffectTableEntry CTimeCycle::m_InteriorReplayLookupTable[] = 
{
	{ ATSTRINGHASH("v_strip3", 0x4e55a04e) }
};

//Script modifiers that we want to disable in free cam
CTimeCycle::EffectTableEntry CTimeCycle::m_ScriptEffectFreeCamRemovalReplayLookupTable[] = 
{
	{ ATSTRINGHASH("DRUNK",0x3fdba102) },
	{ ATSTRINGHASH("STONED",0x1c70c68d) },
	{ ATSTRINGHASH("scanline_cam",0x340c3607) },
	{ ATSTRINGHASH("CAMERA_secuirity",0x5f4cf7dd) },
	{ ATSTRINGHASH("CAMERA_secuirity_FUZZ",0x93787e89) },
	{ ATSTRINGHASH("Drug_deadman",0xbd6b639) }
};


//Script modifiers that we want to disable in game cam
CTimeCycle::EffectTableEntry CTimeCycle::m_ScriptEffectAllCamsRemovalReplayLookupTable[] = 
{
	{ ATSTRINGHASH("TrevorColorCodeBasic",0xafe3459c) },
	{ ATSTRINGHASH("MP_Arena_theme_hell", 0xf602e42a) },
	{ ATSTRINGHASH("MP_Arena_theme_atlantis", 0xa3b56c8b) },
	{ ATSTRINGHASH("FixerShortTrip",0x529EA9DB) },
};


#endif // GTA_REPLAY

///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////		

CTimeCycle::frameInfo *CTimeCycle::m_currFrameInfo = NULL;

class dlCmdSetupTimeCycleFrameInfo : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_SetupTimeCycleFrameInfo,
	};
	
	dlCmdSetupTimeCycleFrameInfo();
	s32 GetCommandSize()						{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdSetupTimeCycleFrameInfo &) cmd).Execute(); }
	void Execute();

private:			
	CTimeCycle::frameInfo* newFrameInfo;
};

class dlCmdClearTimeCycleFrameInfo : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_ClearTimeCycleFrameInfo,
		REQUIRED_ALIGNMENT = 0,
	};
	
	s32 GetCommandSize()						{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdClearTimeCycleFrameInfo &) cmd).Execute(); }
	void Execute();
};

dlCmdSetupTimeCycleFrameInfo::dlCmdSetupTimeCycleFrameInfo()
{
	newFrameInfo = gDCBuffer->AllocateObjectFromPagedMemory<CTimeCycle::frameInfo>(DPT_LIFETIME_RENDER_THREAD, sizeof(CTimeCycle::frameInfo));
	sysMemCpy(newFrameInfo, &g_timeCycle.m_frameInfo, sizeof(CTimeCycle::frameInfo));
	newFrameInfo->m_GlobalLodScale = g_LodScale.GetGlobalScale();
	newFrameInfo->m_GrassLodScale = ( g_LodScale.GetGrassBatchScale() * g_LodScale.GetGlobalScale() );
}

void dlCmdSetupTimeCycleFrameInfo::Execute()
{
	CTimeCycle::m_currFrameInfo = newFrameInfo;
	g_LodScale.SetGlobalScaleForRenderthread(newFrameInfo->m_GlobalLodScale);
	g_LodScale.SetGrassScaleForRenderthread(newFrameInfo->m_GrassLodScale);
}


void dlCmdClearTimeCycleFrameInfo::Execute()
{
	CTimeCycle::m_currFrameInfo = NULL;
}



///////////////////////////////////////////////////////////////////////////////
//  CLASS ShadowFreezing
///////////////////////////////////////////////////////////////////////////////		

PARAM(NoShadowFreezer,"");

float DistXY( const Vector3& a, const Vector3& b)
{
	Vector3 v = b-a;
	return v.XYMag();
}

ShadowFreezing::ShadowFreezing(){
	m_MaxAngleOffset				=0.06f;// rdr2 was 0.05
	m_CameraMovementCatcupScale		=0.1f;
	m_CameraRotationCatchupScale	=0.0f;
	m_CameraTranslationCatchupScale	=1.0f;
	m_AngleMovementAcceleration		=0.05f;  
	m_PrevAngleMovement				=0.0f;
	m_PlayerActive=true;
	m_firstFrame=true;
	m_prevHeading=0.f;
	m_inited=false;
	m_isEnabled=true;
	BANK_ONLY(m_isCurrentlyFrozen=false;)
}

Vec3V_Out ShadowFreezing::Update(const Vector3& trueLightDir, 
	const Vector3& camPos, 
	bool isCut, 
	float heading, 	
	const Vector3& playerP, 
	bool playerActive,
	bool iscutscene
	){
		if (!m_inited)
		{
			m_inited = true;
			BANK_ONLY(m_isEnabled = !PARAM_NoShadowFreezer.Get();)
		}
		Vector3 lightDir		=m_PrimaryShadowLightDir;

		Vector3 playerPos;
		if(playerActive)
		{
			playerPos=playerP;
			playerPos.y=0.0f;
		}
		else
			playerPos=m_PrevPlayerPos;

		float angleThreshhold=m_MaxAngleOffset;
		float angleMovementScale=m_CameraMovementCatcupScale;
		float angleMovement=0;

		bool cameraMoved = (m_PrevCamPos - camPos).Mag() > 0.1f || Abs(heading - m_prevHeading) > 0.1f;

		if((isCut && cameraMoved) || m_firstFrame || !IsEnabled())
		{
			lightDir=trueLightDir;
			m_firstFrame=false;	
		}
		else
		{
			float bearing = heading-m_prevHeading;
			angleMovement+=Min( DistXY(m_PrevCamPos,camPos), DistXY(m_PrevPlayerPos,playerPos))*m_CameraTranslationCatchupScale;
			angleMovement+=bearing*m_CameraRotationCatchupScale;
			float angle=acos(Saturate(lightDir.Dot(trueLightDir)));
			angleMovement*=angle;
			angleMovement=Min(angleMovement, Lerp(m_AngleMovementAcceleration, m_PrevAngleMovement, angleMovement));
			angle=Min(Saturate(angle-angleMovement*angleMovementScale), angleThreshhold);
			BANK_ONLY( m_isCurrentlyFrozen=true; )

			if(( angleMovement>0.0001f || angle>=angleThreshhold )&&( !iscutscene ))
			{
				Matrix34 rotMat;	// perform camera update.
				rotMat.Identity();
				Vector3 rotAxis;
				rotAxis.Cross(trueLightDir, lightDir);
				Vector3 up(0.f,0.f,1.f);
				rotAxis.NormalizeSafe(up);
				rotMat.MakeRotateUnitAxis(rotAxis, angle);
				lightDir.Dot(trueLightDir, rotMat);
				lightDir.Normalize();
				BANK_ONLY(m_isCurrentlyFrozen=false;)
			}
		}
		m_PrevCamPos			=camPos;
		m_PrevPlayerPos			=playerPos;
		m_PrimaryShadowLightDir	=lightDir;
		m_PrevAngleMovement		=angleMovement;
		m_prevHeading=heading;
		return VECTOR3_TO_VEC3V(m_PrimaryShadowLightDir);
}
#if __BANK
void ShadowFreezing::AddWidgets(bkBank & bank)
{
	bank.PushGroup("Shadow Freezing");
	bank.AddToggle("Enabled", &m_isEnabled);
	bank.AddSlider("Camera Movement Catchup Scale", &m_CameraMovementCatcupScale, 0.0f, 0.1f, 0.001f);
	bank.AddSlider("Camera Translation Catchup Scale", &m_CameraTranslationCatchupScale, 0.0f, 10.0f, 0.5f);
	bank.AddSlider("Camera Rotation Catchup Scale", &m_CameraRotationCatchupScale, 0.0f, 10.0f, 0.5f);
	bank.AddSlider("Angle Movement Acceleration", &m_AngleMovementAcceleration, 0.0f, 1.0f, 0.1f);
	bank.AddSlider("Max Angle Offset", &m_MaxAngleOffset, 0.0f, PI/4.0f, PI/64.0f);
	bank.AddText("Prev Angle Movement", &m_PrevAngleMovement);
	bank.AddText("Is Frozen", &m_isCurrentlyFrozen);
	bank.PopGroup();
}
#endif // __BANK

ShadowFreezing	ShadowFreezer;

//This needs to set the default value here before the settings get initialised on pc.
bool CTimeCycle::m_longShadowsEnabled = false;
///////////////////////////////////////////////////////////////////////////////
//  CLASS CTimeCycle
///////////////////////////////////////////////////////////////////////////////		

void CTimeCycle::InitDLCCommands()
{
	DLC_REGISTER_EXTERNAL(dlCmdSetupTimeCycleFrameInfo);
	DLC_REGISTER_EXTERNAL(dlCmdClearTimeCycleFrameInfo);
}

class TimeCycleFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		switch(file.m_fileType)
		{
		case CDataFileMgr::TIMECYCLEMOD_FILE:
			g_timeCycle.AppendModifierFile(file.m_filename);
			break;
		default:
			break;
		}
		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & file) 
	{
		switch(file.m_fileType)
		{
		case CDataFileMgr::TIMECYCLEMOD_FILE:
			g_timeCycle.RevertModifierFile(file.m_filename);
		break;
		default:
		break;
		}
		
	}

} g_TimeCycleFileMounter;

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////													

void CTimeCycle::Init(unsigned initMode)
{
	// Moved this to the top because we call m_gameDebug.Init() in the INIT_AFTER_MAP_LOADED block, and it would assert that CTimeCycle has not been initialised yet.
#if __ASSERT
	m_IsTimeCycleValid = true;
#endif // __ASSERT

    if (initMode == INIT_CORE)
    {
#if __BANK
	    for (s32 i=0; i<TCVAR_NUM; i++)
	    {
		    tcAssertf(i == g_varInfos[i].varId, "mismatch in timecycle variable data (g_varInfos[%d].varId=%d, name=%s)", i, g_varInfos[i].varId, g_varInfos[i].name);
	    }
#endif

	    tcManager::Init(TCVAR_NUM, g_varInfos);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::TIMECYCLEMOD_FILE,&g_TimeCycleFileMounter,eDFMI_UnloadLast);
	    // scripted tc mods
	    m_scriptModifierIndex				= -1;
		m_transitionScriptModifierIndex		= -1;
		m_scriptModifierStrength			= 1.0f;
		m_transitionStrength				= 0.0f;
		m_transitionSpeed					= 0.0f;
	    m_transitionOut						= false;
	    
		m_pushedScriptModifierIndex			= -1;
		m_pushedTransitionScriptModifierIndex = -1;
		m_pushedTransitionStrength			= 0.0f;
		m_pushedTransitionSpeed				= 0.0f;
		m_pushedTransitionOut				= false;
		m_pushedScriptModifier				= false;

		m_extraModifierIndex				= -1;

	    // Player tc mods
		m_playerModifierIndexCurrent		= -1;
		m_playerModifierIndexNext			= -1;
		m_playerModifierStrength			= 0.0f;

	    m_cutsceneModifierIndex				= -1;

	    m_interiorBlendStrength				= 1.0f;

	    m_useDefaultKeyframe				= false;

	    m_regionOverride					= -1;
	    
	    m_interiorInitialised				= false;
	    m_interiorApply						= false;
	    m_interiorReset						= false;

		m_frameInfo.m_moonCycleOverride		= -1.0f;

		m_interior_main_idx					= -1;
		m_interior_secondary_idx			= -1;
		m_cutsceneModifierIndex				= -1;
		m_playerModifierIndexCurrent		= -1;
		m_playerModifierIndexNext			= -1;
		m_playerModifierStrength			= 0.0f;
		
#if PLAYERSWITCH_FORCES_LIGHTDIR
		// Code side directional light override
		m_codeOverrideDirLightOrientation = Vec3V(V_Z_AXIS_WZERO);
		m_codeOverrideDirLightOrientationStrength = 0.0f;
#endif // PLAYERSWITCH_FORCES_LIGHTDIR

#if GTA_REPLAY
		// Create a map of TCKeys (from namehash) and their corresponding index.
		for(int i=0;i<TCVAR_NUM;i++)
		{
			u32 nameHash = atStringHash(g_varInfos[i].name);
			m_EffectsMap.Insert(nameHash, (u16)i);
		}
#endif	//GTA_REPLAY

	}
    else if (initMode == INIT_AFTER_MAP_LOADED)
    {
        // initialise the time sample data
	    tcTimeSampleInfo timeSampleInfo[TIMECYCLE_MAX_TIME_SAMPLES];
	    for (s32 i=0; i<CClock::GetNumTimeSamples(); i++)
	    {
		    const TimeSample_s& timeSample = CClock::GetTimeSampleInfo(i);
		    timeSampleInfo[i].hour = (float)timeSample.hour;
		    timeSampleInfo[i].duration = (float)timeSample.duration;
#if __BANK
		    timeSampleInfo[i].name = timeSample.name;
#endif
	    }

	    // initialise the cycle data with the weather types
	    s32 numCycles = 0;
	    tcCycleInfo cycleInfo[TIMECYCLE_MAX_CYCLES];
	    for (u32 i=0; i<g_weather.GetNumTypes(); i++)
	    {
		    const CWeatherType& weatherType = g_weather.GetTypeInfo(i);
		    strcpy_s(cycleInfo[numCycles].filename, 64, weatherType.m_timeCycleFilename);
		    numCycles++;
	    }

	    // load in other cycle data
	    const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::TIMECYCLE_FILE);
	    while(DATAFILEMGR.IsValid(pData))
	    {
		    strcpy_s(cycleInfo[numCycles].filename, 64, pData->m_filename);
		    numCycles++;

		    pData = DATAFILEMGR.GetNextFile(pData);
	    }

	    tcManager::InitMap(numCycles, cycleInfo, m_numRegions, m_regionInfo, CClock::GetNumTimeSamples(), timeSampleInfo
#if __BANK
			, TIMECYCLE_NUM_DEBUG_GROUPS
			, g_debugGroupNames
			, TIMECYCLE_NUM_USER_FLAGS
			, g_userFlagNames
#else
			, 0
			, NULL
			, 0
			, NULL
#endif
			);

		// test adding a region box (the tools are set up to do this yet)
		{
			Vec3V BBMin(-1747.178f, -3506.0f, 0.0f);
			Vec3V BBMax(1264.279f, 619.43f, 500.0f);
			spdAABB bBox(BBMin, BBMax);
			AddRegionBox(bBox,ATSTRINGHASH("URBAN",0x7d7b904f), 100.0f, 750.0f, 0, 0, -1, "CTimeCycle::Init");
		}
		
		{
			Vec3V BBMin(-2517.747f, -3347.342f, 0.0f);
			Vec3V BBMax(-1713.271f, -133.549f,	271.342f);
			spdAABB bBox(BBMin, BBMax);
			AddRegionBox(bBox,ATSTRINGHASH("URBAN",0x7d7b904f), 100.0f, 750.0f, 0, 0, -1, "CTimeCycle::Init");
		}
		
		{
			Vec3V BBMin(-2017.02f, -282.304f, 0.0f);
			Vec3V BBMax(417.0f, 1085.0f,	271.342f);
			spdAABB bBox(BBMin, BBMax);
			AddRegionBox(bBox,ATSTRINGHASH("URBAN",0x7d7b904f), 100.0f, 750.0f, 0, 0, -1, "CTimeCycle::Init");
		}
		
		{
			Vec3V BBMin(1254.0f, -2489.396f, 0.0f);
			Vec3V BBMax(1444.0f, -197.25f,	267.59f);
			spdAABB bBox(BBMin, BBMax);
			AddRegionBox(bBox,ATSTRINGHASH("URBAN",0x7d7b904f), 100.0f, 750.0f, 0, 0, -1, "CTimeCycle::Init");
		}
		
		{
			Vec3V BBMin(1254.0f, -196.606f, 0.0f);
			Vec3V BBMax(1756.208f, 112.971f,	267.59f);
			spdAABB bBox(BBMin, BBMax);
			AddRegionBox(bBox,ATSTRINGHASH("URBAN",0x7d7b904f), 100.0f, 750.0f, 0, 0, -1, "CTimeCycle::Init");
		}

	    // debug init
#if TC_DEBUG
	    m_gameDebug.Init();
#endif
		g_tcInstBoxMgr.Init();
		
		tcInstBoxTypeLoader &typeStore = g_tcInstBoxMgr.GetTypeStore();
		
		int typeCount = typeStore.GetTypeCount();
		for(int i=0;i<typeCount;i++)
		{
			tcInstBoxType* boxType = typeStore.GetType(i);

			int n = FindModifierIndex(boxType->GetModifier(), "tcInstBoxTypeLoader::PostLoad");
			const tcModifier *mod = GetModifier(n);
			
			bool validNat = false;
			bool validArt = false;
			Vec2V result = GetModifierAmbientScale(mod, validNat, validArt);

			boxType->SetIsValidNaturalAmbient(validNat);
			boxType->SetNaturalAmbient(result.GetXf());
			
			boxType->SetIsValidArtificialAmbient(validArt);
			boxType->SetArtificialAmbient(result.GetYf());
		}
    }
	else if (initMode == INIT_SESSION)
	{
	    // scripted tc mods
	    m_scriptModifierIndex				= -1;
		m_transitionScriptModifierIndex		= -1;
		m_scriptModifierStrength			= 1.0f;
		m_transitionStrength				= 0.0f;
		m_transitionSpeed					= 0.0f;
	    m_transitionOut						= false;
	    
		m_pushedScriptModifierIndex			= -1;
		m_pushedTransitionScriptModifierIndex = -1;
		m_pushedTransitionStrength			= 0.0f;
		m_pushedTransitionSpeed				= 0.0f;
		m_pushedTransitionOut				= false;
		m_pushedScriptModifier				= false;

		m_extraModifierIndex				= -1;

	    // Player tc mods
		m_playerModifierIndexCurrent		= -1;
		m_playerModifierIndexNext			= -1;
		m_playerModifierStrength			= 0.0f;

	    m_cutsceneModifierIndex				= -1;

	    m_interiorBlendStrength				= 1.0f;

	    m_useDefaultKeyframe				= false;

	    m_regionOverride					= -1;
	    
	    m_interiorInitialised				= false;
	    m_interiorApply						= false;
		m_interiorReset						= false;
		
		m_frameInfo.m_moonCycleOverride		= -1.0f;

		m_interior_main_idx					= -1;
		m_interior_secondary_idx			= -1;
		m_scriptModifierIndex				= -1;
		m_cutsceneModifierIndex				= -1;
		m_playerModifierIndexCurrent		= -1;
		m_playerModifierIndexNext			= -1;
		m_playerModifierStrength			= 0.0f;
		
#if PLAYERSWITCH_FORCES_LIGHTDIR
		// Code side directional light override
		m_codeOverrideDirLightOrientation = Vec3V(V_Z_AXIS_WZERO);
		m_codeOverrideDirLightOrientationStrength = 0.0f;
#endif // PLAYERSWITCH_FORCES_LIGHTDIR
		
	}

#if GTA_REPLAY
	m_ReplayModifierHash = 0;
	m_ReplayModifierIntensity = 0.f;
	m_ReplayTCFrame.SetDefault();
	m_ReplayDefaultTCFrame.SetDefault();
	m_bUsingDefaultTCData = false;

	m_bReplayEnableExtraFX = false;
	m_ReplaySaturationIntensity = 0.0f;
	m_ReplayVignetteIntensity = 0.0f;
	m_ReplayContrastIntensity = 0.0f;
	m_ReplayBrightnessIntensity = 0.0f;
#endif //GTA_REPLAY
}

void CTimeCycle::LoadModifierFiles()
{
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::TIMECYCLEMOD_FILE);
	while(DATAFILEMGR.IsValid(pData))
	{
		LoadModifierFile(pData->m_filename);

		pData = DATAFILEMGR.GetNextFile(pData);
	}
	
	FinishModifierLoad();

}
void CTimeCycle::AppendModifierFile(const char* filename)
{
	if(Verifyf(ASSET.Exists(filename,""),"Timecycle modifier file was not found in path %s",filename))
	{
		LoadModifierFile(filename);
	}
	FinishModifierLoad();
#if TC_DEBUG
	tcDebug::RefreshTCWidgets();
#endif
}

void CTimeCycle::RevertModifierFile(const char* filename)
{
	if(Verifyf(ASSET.Exists(filename,""),"Timecycle modifier file was not found in path %s",filename))
	{
		UnloadModifierFile(filename);
	}
	FinishModifierLoad();
#if TC_DEBUG
	tcDebug::RefreshTCWidgets();
#endif
}

///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////													

void CTimeCycle::Shutdown(unsigned shutdownMode)
{
#if GTA_REPLAY
	if( shutdownMode == SHUTDOWN_CORE)
	{
		// Reset the map
		m_EffectsMap.Reset();
	}
	else
#endif	//GTA_REPLAY

    if (shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
    {
        tcManager::ShutdownMap();
    }
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////													

void			CTimeCycle::Update							(bool finalUpdate)
{
	PF_AUTO_PUSH_TIMEBAR("TimeCycle Update");

	if( finalUpdate )
		ANIMPOSTFXMGR.Update();

	Vec3V_In vCameraPos = RCC_VEC3V(camInterface::GetPos());

	// set any cutscene modifier
	//TF **REPLACE WITH NEW CUTSCENE**
	//if (CCutsceneManager::IsTimeCycleSafe())
	//{	
	//	m_cutsceneModifierIndex = FindModifierCS(CCutsceneManager::GetNameHash());
	//}
	//else
	{
		m_cutsceneModifierIndex = -1;
	}

	// update the debug info
#if TC_DEBUG
	m_gameDebug.Update();
#endif

	// get the current time to use
	s32 currHour = CClock::GetHour();
	s32 currMinute = CClock::GetMinute();
	s32 currSecond = CClock::GetSecond();

	// update the base class
	if( CPauseMenu::GetPauseRenderPhasesStatus() == false )
		tcManager::Update(vCameraPos, currHour, currMinute, currSecond);

    u32 prevHash = 0, nextHash = 0;
    float cycleInterp = 0.0f;

    // somewhat fudge the net transition
    if (g_weather.IsInNetTransition())
    {
        // work out where we are coming from 
        if(g_weather.GetNetTransitionPrevInterp() < 0.5f)
        {
            prevHash = g_weather.GetNetTransitionPrevType().m_hashName;
        }
        else
        {
            prevHash = g_weather.GetNetTransitionNextType().m_hashName;
        }

        // and where we're going to (use static next interp so this doesn't tip mid-transition)
        if(g_weather.GetNetTransitionNextInterp() < 0.5f)
        {
            nextHash = g_weather.GetPrevType().m_hashName;
        }
        else
        {
            nextHash = g_weather.GetNextType().m_hashName;
        }

        // use fast interp
        cycleInterp = g_weather.GetNetTransitionInterp();
    }
    else
    {
        prevHash = g_weather.GetPrevType().m_hashName;
        nextHash = g_weather.GetNextType().m_hashName;
        cycleInterp = g_weather.GetInterp();
    }

	// calc the base keyframe
	tcKeyframeUncompressed baseKeyframe;

#if GTA_REPLAY
	tcKeyframeUncompressed	replayKeyFrame;

	if( ShouldUseReplayTCData() )
	{
		replayKeyFrame = m_ReplayTCFrame;
		ApplyCutsceneCameraOverrides(replayKeyFrame);
	}

	bool twoPhaseDiabled = false;
#if TC_DEBUG
	twoPhaseDiabled = m_gameDebug.GetDisableReplayTwoPhaseSystem();	// Is actually 3 phase now, (1) game, (2) replay in normal cam, (3) replay in freecam
#endif // TC_DEBUG

	//if we should be using two phase recording, set our start phase to be TIME_CYCLE_PHASE_REPLAY.
	eTimeCyclePhase startTimeCyclePhase = !twoPhaseDiabled && !CReplayMgr::IsEditModeActive() && finalUpdate ? (eTimeCyclePhase)0 : TIME_CYCLE_PHASE_GAME;
	
	//Loop through all the phases
	//Phase TIME_CYCLE_PHASE_REPLAY: Calculate the time cycle data without any script FX enabled, then offload this data into m_ReplayDefaultTCFrame
	//Phase TIME_CYCLE_PHASE_DEFAULT: Calculate the time cycle data as normal
	for(u32 i = startTimeCyclePhase; i < TIME_CYCLE_PHASE_MAX; ++i)
	{
		eTimeCyclePhase currentPhase = (eTimeCyclePhase)i;
#endif // GTA_REPLAY

	CalcBaseKeyframe(baseKeyframe, currHour, currMinute, currSecond, prevHash, nextHash, cycleInterp);
	
	// get the current update buffer id
	// calc the base (un-modified) keyframe - using time and weather data
	if (m_useDefaultKeyframe)
	{
		m_frameInfo.m_keyframe.SetDefault();
	}
	else
	{
		m_frameInfo.m_keyframe = baseKeyframe;
	}

	// Cycle time
	Date d1(1,	Date::JANUARY,		2000);
	Date d2(CClock::GetDay(), Date::Month(CClock::GetMonth()), CClock::GetYear());

	const double daysBetween = (double)Date::GetDaysBetween(d1, d2);
	const double dayRatio = (double)CClock::GetDayRatio();
	const double cycleTime = daysBetween + dayRatio;

	SetCycleTime(cycleTime);

	// Turn off directional light in LastGen mode
	#if __BANK
	extern bool gLastGenMode;
	if (gLastGenMode)
	{
		m_frameInfo.m_keyframe.SetVar(TCVAR_LIGHT_DIR_MULT, 0.0);
	}

	if (DebugLighting::OverrideDirectional()) // Overwrite
	{
		m_frameInfo.m_keyframe.SetVar(TCVAR_LIGHT_DIR_MULT,  0.01f);
		m_frameInfo.m_keyframe.SetVar(TCVAR_LIGHT_DIR_COL_R, 0.01f);
		m_frameInfo.m_keyframe.SetVar(TCVAR_LIGHT_DIR_COL_G, 0.01f);
		m_frameInfo.m_keyframe.SetVar(TCVAR_LIGHT_DIR_COL_B, 0.01f);
	}
	#endif

	const float dirLightMult = PostFX::GetUseNightVision() ? 
							   (m_frameInfo.m_keyframe.GetVar(TCVAR_LIGHT_DIR_MULT) * m_frameInfo.m_keyframe.GetVar(TCVAR_NV_LIGHT_DIR_MULT) ) : 
								m_frameInfo.m_keyframe.GetVar(TCVAR_LIGHT_DIR_MULT) * m_frameInfo.m_directionalLightIntensity;

	// save the unmodified directional light data - for use with daylight fillers and light shafts
	m_frameInfo.m_vBaseDirectionalLight = Vec4V(m_frameInfo.m_keyframe.GetVar(TCVAR_LIGHT_DIR_COL_R), 
												m_frameInfo.m_keyframe.GetVar(TCVAR_LIGHT_DIR_COL_G), 
												m_frameInfo.m_keyframe.GetVar(TCVAR_LIGHT_DIR_COL_B), 
												dirLightMult);
	//Reset Lightning info
	m_frameInfo.m_LightningOverride = false;
	m_frameInfo.m_LightningDirection = m_frameInfo.m_vBaseDirectionalLight.GetXYZ();
	m_frameInfo.m_LightningColor = Color32(0,0,0,0);
	m_frameInfo.m_LightningFogColor = Color32(0,0,0,0);

	if(m_useDefaultKeyframe REPLAY_ONLY( && !ShouldUseReplayTCData() ) )
	{
		m_frameInfo.m_vDirectionalLightDir = Normalize(Vec3V(V_ONE));
	}
	else
	{
		//Now override it 
		bool suppressLightning = false;
		bool processInteriorLighting = true;

#if GTA_REPLAY
		// Only apply interior modifier if we are in the normal phase or in an interior that we don't apply ourselves.
		processInteriorLighting = currentPhase == TIME_CYCLE_PHASE_GAME || currentPhase == TIME_CYCLE_PHASE_REPLAY_NORMAL || !ShouldApplyInteriorModifier();
#endif // GTA_REPLAY

		if (m_cutsceneModifierIndex!=-1)
		{
			ApplyCutsceneModifiers(m_frameInfo.m_keyframe);
		}
		else
		{
			bool isUnderWater = Water::IsCameraUnderwater();
			
			if( isUnderWater )
			{
#if GTA_REPLAY
				//Only apply under water modifier if we're currently in the TIME_CYCLE_PHASE_DEFAULT phase
				if( currentPhase == TIME_CYCLE_PHASE_GAME )
#endif
				{
					UpdateUnderWater(m_frameInfo.m_keyframe, currHour, currMinute, currSecond); // apply any underwater weather type
				}
			}
			else
			{
				// calc the modified keyframe - based on the camera pos - skip underwater and interior boxes.
				ApplyBoxModifiers(m_frameInfo.m_keyframe,1.0f,false,false);
			}

			// apply any interior modifiers 
			bool inInterior = false;
			if( processInteriorLighting )
			{
				inInterior = ApplyInteriorModifiers(m_frameInfo.m_keyframe,isUnderWater,finalUpdate);
			}
			
			if( inInterior )
			{
				// we're using interior modifiers - suppress any lightning effects
				suppressLightning = true;
			}
		}

		// Only disable directional lighting if we are in the normal phase or in an interior that we don't apply ourselves.
		if( processInteriorLighting )
		{
			if (CPortal::GetNoDirectionalLight()==true)
			{ // don't touch the base directional light since it might be useed for shafts and fillers.
				m_frameInfo.m_keyframe.SetVar(TCVAR_LIGHT_DIR_MULT, 0.0f);
			}
		}

		// calc the sun light data
		CalcSunLightDir();

		// Now override directional Light with lightning info if we have a lightning burst
		if (!suppressLightning && m_frameInfo.m_keyframe.GetVar(TCVAR_NO_WEATHER_FX)<1.0f)
		{
			float alpha = 1.0f;
			const bool isUnderWater = Water::IsCameraUnderwater();
			if( isUnderWater )
			{
				const CMainWaterTune& waterTunings = Water::GetMainWaterTunings();
				const float start	= waterTunings.m_WaterLightningDepth;
				const float end	= start + waterTunings.m_WaterLightningFade;
				const float underWaterDepth = Water::GetWaterLevel() - camInterface::GetPos().z;				

				alpha = 1.0f - CVfxHelper::GetInterpValue(underWaterDepth, start, end);
			}
			
			if( alpha > 0.0f )
			{
				g_vfxLightningManager.PerformTimeCycleOverride(m_frameInfo, alpha);
			}

		}
	}

	// Setup reflection tweaks
	const grcViewport *currentViewport = gVpMan.GetCurrentGameGrcViewport();
	const Vector3 camDir = VEC3V_TO_VECTOR3(currentViewport->GetCameraMtx().c());

	CShaderLib::SetGlobalReflectionCamDirAndEmissiveMult(
		camDir,
		m_frameInfo.m_keyframe.GetVar(TCVAR_REFLECTION_TWEAK_EMISSIVE));

	// apply any global modifiers
	if(true TC_DEBUG_ONLY(&& !m_debug.GetModifierEditApply()))
	{
		// Always make sure the values for tonemapping are the default ones
		m_frameInfo.m_keyframe.SetVar(TCVAR_POSTFX_TONEMAP_FILMIC_OVERRIDE_DARK, false);
		m_frameInfo.m_keyframe.SetVar(TCVAR_POSTFX_TONEMAP_FILMIC_A_DARK, PostFX::GetTonemapParam(PostFX::TONEMAP_FILMIC_A_DARK));
		m_frameInfo.m_keyframe.SetVar(TCVAR_POSTFX_TONEMAP_FILMIC_B_DARK, PostFX::GetTonemapParam(PostFX::TONEMAP_FILMIC_B_DARK));
		m_frameInfo.m_keyframe.SetVar(TCVAR_POSTFX_TONEMAP_FILMIC_C_DARK, PostFX::GetTonemapParam(PostFX::TONEMAP_FILMIC_C_DARK));
		m_frameInfo.m_keyframe.SetVar(TCVAR_POSTFX_TONEMAP_FILMIC_D_DARK, PostFX::GetTonemapParam(PostFX::TONEMAP_FILMIC_D_DARK));
		m_frameInfo.m_keyframe.SetVar(TCVAR_POSTFX_TONEMAP_FILMIC_E_DARK, PostFX::GetTonemapParam(PostFX::TONEMAP_FILMIC_E_DARK));
		m_frameInfo.m_keyframe.SetVar(TCVAR_POSTFX_TONEMAP_FILMIC_F_DARK, PostFX::GetTonemapParam(PostFX::TONEMAP_FILMIC_F_DARK));
		m_frameInfo.m_keyframe.SetVar(TCVAR_POSTFX_TONEMAP_FILMIC_W_DARK, PostFX::GetTonemapParam(PostFX::TONEMAP_FILMIC_W_DARK));
		m_frameInfo.m_keyframe.SetVar(TCVAR_POSTFX_TONEMAP_FILMIC_EXPOSURE_DARK, PostFX::GetTonemapParam(PostFX::TONEMAP_FILMIC_EXPOSURE_DARK));

		m_frameInfo.m_keyframe.SetVar(TCVAR_POSTFX_TONEMAP_FILMIC_OVERRIDE_BRIGHT, false);
		m_frameInfo.m_keyframe.SetVar(TCVAR_POSTFX_TONEMAP_FILMIC_A_BRIGHT, PostFX::GetTonemapParam(PostFX::TONEMAP_FILMIC_A_BRIGHT));
		m_frameInfo.m_keyframe.SetVar(TCVAR_POSTFX_TONEMAP_FILMIC_B_BRIGHT, PostFX::GetTonemapParam(PostFX::TONEMAP_FILMIC_B_BRIGHT));
		m_frameInfo.m_keyframe.SetVar(TCVAR_POSTFX_TONEMAP_FILMIC_C_BRIGHT, PostFX::GetTonemapParam(PostFX::TONEMAP_FILMIC_C_BRIGHT));
		m_frameInfo.m_keyframe.SetVar(TCVAR_POSTFX_TONEMAP_FILMIC_D_BRIGHT, PostFX::GetTonemapParam(PostFX::TONEMAP_FILMIC_D_BRIGHT));
		m_frameInfo.m_keyframe.SetVar(TCVAR_POSTFX_TONEMAP_FILMIC_E_BRIGHT, PostFX::GetTonemapParam(PostFX::TONEMAP_FILMIC_E_BRIGHT));
		m_frameInfo.m_keyframe.SetVar(TCVAR_POSTFX_TONEMAP_FILMIC_F_BRIGHT, PostFX::GetTonemapParam(PostFX::TONEMAP_FILMIC_F_BRIGHT));
		m_frameInfo.m_keyframe.SetVar(TCVAR_POSTFX_TONEMAP_FILMIC_W_BRIGHT, PostFX::GetTonemapParam(PostFX::TONEMAP_FILMIC_W_BRIGHT));
		m_frameInfo.m_keyframe.SetVar(TCVAR_POSTFX_TONEMAP_FILMIC_EXPOSURE_BRIGHT, PostFX::GetTonemapParam(PostFX::TONEMAP_FILMIC_EXPOSURE_BRIGHT));

		ApplyGlobalModifiers(m_frameInfo.m_keyframe, finalUpdate REPLAY_ONLY(, currentPhase));
	}

	// Apply cutscene camera override
	ApplyCutsceneCameraOverrides(m_frameInfo.m_keyframe);

	if( CRenderer::GetDisableArtificialLights() )
	{
		m_frameInfo.m_keyframe.SetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_INTENSITY,CShaderLib::GetOverridesArtIntAmbDown());
		m_frameInfo.m_keyframe.SetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_INTENSITY,CShaderLib::GetOverridesArtIntAmbBase());
		m_frameInfo.m_keyframe.SetVar(TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_DOWN_INTENSITY,CShaderLib::GetOverridesArtExtAmbDown());
		m_frameInfo.m_keyframe.SetVar(TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_BASE_INTENSITY,CShaderLib::GetOverridesArtExtAmbBase());
	}
						
#if __BANK
	// output info to screen
	if ( dirLightMult==0.0f || m_frameInfo.m_keyframe.GetVar(TCVAR_LIGHT_DIR_MULT) == 0.0f ||
								(m_frameInfo.m_keyframe.GetVar(TCVAR_LIGHT_DIR_COL_R) == 0.0f && 
								m_frameInfo.m_keyframe.GetVar(TCVAR_LIGHT_DIR_COL_G) == 0.0f && 
								m_frameInfo.m_keyframe.GetVar(TCVAR_LIGHT_DIR_COL_B) == 0.0f))
	{
		grcDebugDraw::AddDebugOutput("WARNING: directional light and shadow is OFF");
	}

	const bool darkChanged = (m_frameInfo.m_keyframe.GetVar(TCVAR_POSTFX_TONEMAP_FILMIC_OVERRIDE_DARK) > 0.0f);
	const bool brightChanged = (m_frameInfo.m_keyframe.GetVar(TCVAR_POSTFX_TONEMAP_FILMIC_OVERRIDE_BRIGHT) > 0.0f);

	if ( darkChanged )
	{
		grcDebugDraw::AddDebugOutput("WARNING: using custom dark tone-map values");
	}

	if ( brightChanged )
	{
		grcDebugDraw::AddDebugOutput("WARNING: using custom bright tone-map values");
	}

	const float hdrMultiplier = LightProbe::GetSkyHDRMultiplier(); // additional sky/fog HDR multiplier

	if (hdrMultiplier != 1.0f)
	{
		m_frameInfo.m_keyframe.MultVar(TCVAR_SKY_HDR, hdrMultiplier);
		m_frameInfo.m_keyframe.MultVar(TCVAR_SKY_SUN_HDR, hdrMultiplier);
		m_frameInfo.m_keyframe.MultVar(TCVAR_SKY_CLOUD_HDR, hdrMultiplier);
		m_frameInfo.m_keyframe.MultVar(TCVAR_FOG_HDR, hdrMultiplier);
		m_frameInfo.m_keyframe.MultVar(TCVAR_FOG_HAZE_HDR, hdrMultiplier);
	}
#endif

	// check if we're paused
	// NOTE: We update and apply time cycle even when the game is paused if the marketing tools or replay editor is active.
	if ((fwTimer::IsGamePaused()) && !CMarketingTools::IsActive() REPLAY_ONLY( && !CReplayMgr::IsEditModeActive() ) )
	{
		// always last
		ApplyLodScaleModifiers(WIN32PC_ONLY(m_frameInfo.m_keyframe));

#if __ASSERT
		m_IsTimeCycleValid = true;
#endif // __ASSERT
			
#if GTA_REPLAY

		//if we're processing anything other than TIME_CYCLE_PHASE_GAME we need to continue
		//so we can process to the normal game pass.
		if( currentPhase < TIME_CYCLE_PHASE_GAME )
		{
			continue;
		}
		else
		{
			return;
		}
#else
		return;
#endif // GTA_REPLAY
	}

	// apply the visual settings to the keyframe
	ApplyVisualSettings(m_frameInfo.m_keyframe, vCameraPos);

#if __BANK
	if (g_cheapmodeHogEnabled)
		m_frameInfo.m_keyframe.SetVar(TCVAR_FAR_CLIP, 1500.f);
#endif // __BANK

#if GTA_REPLAY
	if(ShouldUseReplayTCData())
	{
		//	overwrite final with replay values
		for(int varIdx = 0; varIdx < TCVAR_NUM; ++varIdx)
		{
			if(g_varInfos[varIdx].replayOverride)
			{
				m_frameInfo.m_keyframe.SetVar(varIdx, replayKeyFrame.GetVar(varIdx));
			}
		}

		//reapply the underwater effect if we're underwater as this isn't
		//stored during a replay because it can change if the camera is moved about
		bool isUnderWater = Water::IsCameraUnderwater();
		if(isUnderWater && m_bUsingDefaultTCData)
		{
			UpdateUnderWater(m_frameInfo.m_keyframe, currHour, currMinute, currSecond);
		}

		//don't apply any modifiers if they are already part of the TC data.
		if( ShouldApplyInteriorModifier() )
		{
			// apply any interior modifiers 
			ApplyInteriorModifiers(m_frameInfo.m_keyframe,isUnderWater,finalUpdate);
		}

		ApplyReplayEditorModifiers(m_frameInfo.m_keyframe);
	}

	// Apply replay extra effects (saturation, contrast, vignetting)
	if (AreReplayExtraEffectsEnabled())
	{
		ApplyReplayExtraEffects(m_frameInfo.m_keyframe);
	}

	//cache the frame so if we are jumping we can use the cached values to make the post effects
	//consistent until we get to the point where we want to allow them to be updated again
	if(CReplayMgr::IsReplayInControlOfWorld())
	{
		if(CPauseMenu::GetPauseRenderPhasesStatus())
		{
			m_frameInfo.m_keyframe = g_KeyframeCachedForReplayJump;
		}
		else
		{
			g_KeyframeCachedForReplayJump = m_frameInfo.m_keyframe;
		}
	}
		
	RecordTimeCycleDataForReplay(currentPhase);
#endif	//GTA_REPLAY

	// always last
	ApplyLodScaleModifiers(WIN32PC_ONLY(m_frameInfo.m_keyframe));

#if GTA_REPLAY
	}
#endif // GTA_REPLAY

#if __ASSERT
	m_IsTimeCycleValid = true;
#endif // __ASSERT	

	// Update day night mix
	const float dayNightBalance = CalculateDayNightBalance();
	SetDayNightBalance(dayNightBalance);
}


///////////////////////////////////////////////////////////////////////////////
//  CalcBaseKeyframe
///////////////////////////////////////////////////////////////////////////////													

void CTimeCycle::CalcBaseKeyframe(tcKeyframeUncompressed& baseKeyframe, const u32 hours, const u32 minutes, const u32 seconds, const u32 prevCycleHashName, const u32 nextCycleHashName, const float cycleInterp)
{
	// get the time sample data
	u32 timeSampleIndex1;
	u32 timeSampleIndex2;
	float timeInterp;
#if TC_DEBUG
	if (m_debug.GetKeyframeEditActive() && m_debug.GetKeyframeEditOverrideTimeSample())
	{
		timeSampleIndex1 = m_debug.GetKeyframeEditTimeSampleIndex();
		timeSampleIndex2 = timeSampleIndex1;
		timeInterp = 0.0f;
		
		CClock::SetTime((s32)tcConfig::GetTimeSampleInfo(timeSampleIndex1).hour,0,0);
	}
	else
#endif
	{
		CalcTimeSampleInfo(hours, minutes, seconds, timeSampleIndex1, timeSampleIndex2, timeInterp);
	}

	// get the cycle data
	int cycleIndex1 = GetCycleIndex(prevCycleHashName);
	int cycleIndex2 = GetCycleIndex(nextCycleHashName);
#if TC_DEBUG
	if (m_debug.GetKeyframeEditActive() && m_debug.GetKeyframeEditOverrideCycle())
	{
		cycleIndex1 = m_debug.GetKeyframeEditCycleIndex();
		cycleIndex2 = cycleIndex1;
	}
#endif

	int baseRegionId = 0;

	// calc the base keyframe
	if (tcVerifyf(cycleIndex1>-1 && cycleIndex2>-1, "invalid cycle indices - cannot update the base keyframe (cycle 1: %d %d, cycle 2: %d %d ", prevCycleHashName, cycleIndex1, nextCycleHashName, cycleIndex2))
	{
		// calc the base keyframe in the global region
		tcCycle& cycle1 = GetCycle(cycleIndex1);
		tcCycle& cycle2 = GetCycle(cycleIndex2);

		int regionOverride = m_regionOverride;
#if TC_DEBUG		
		if (m_debug.GetKeyframeEditActive() && m_debug.GetKeyframeEditOverrideRegion())
			regionOverride = m_debug.GetKeyframeEditRegionIndex();
#endif			
		if (regionOverride != -1)
		{
			if( cycleIndex1 == cycleIndex2 )
			{
				baseKeyframe.InterpolateKeyframe(
					cycle1.GetKeyframe(regionOverride, timeSampleIndex1),
					cycle1.GetKeyframe(regionOverride, timeSampleIndex2),
					timeInterp
					);
			}
			else
			{
				baseKeyframe.InterpolateKeyframe(
					cycle1.GetKeyframe(regionOverride, timeSampleIndex1),
					cycle2.GetKeyframe(regionOverride, timeSampleIndex1),
					cycle1.GetKeyframe(regionOverride, timeSampleIndex2),
					cycle2.GetKeyframe(regionOverride, timeSampleIndex2),
					cycleInterp,
					timeInterp
					);
			}
		}
		else
		{
			if (TIMECYCLE_MAX_REGIONS > 1 && m_regionStrengths[1] == 1.0f)
			{
				baseRegionId = 1; // special-case for 2-region interpolation where the second region is 100%, e.g. within the city
			}

			if( cycleIndex1 == cycleIndex2 )
			{
				baseKeyframe.InterpolateKeyframe(
					cycle1.GetKeyframe(baseRegionId, timeSampleIndex1),
					cycle1.GetKeyframe(baseRegionId, timeSampleIndex2),
					timeInterp
					);
			}
			else
			{
				baseKeyframe.InterpolateKeyframe(
					cycle1.GetKeyframe(baseRegionId, timeSampleIndex1),
					cycle2.GetKeyframe(baseRegionId, timeSampleIndex1),
					cycle1.GetKeyframe(baseRegionId, timeSampleIndex2),
					cycle2.GetKeyframe(baseRegionId, timeSampleIndex2),
					cycleInterp,
					timeInterp
					);
			}
			
			for (int regionId = baseRegionId + 1; regionId < TIMECYCLE_MAX_REGIONS; regionId++) // apply other regions
			{

#if __BANK
				grcDebugDraw::AddDebugOutput("Region Id %d - %.3f", regionId, m_regionStrengths[regionId]);
#endif
				if (m_regionStrengths[regionId] > 0.0f)
				{
					tcKeyframeUncompressed regionKeyframeRes;

					if( cycleIndex1 == cycleIndex2 )
					{
						regionKeyframeRes.InterpolateKeyframe(
							cycle1.GetKeyframe(regionId, timeSampleIndex1),
							cycle1.GetKeyframe(regionId, timeSampleIndex2),
							timeInterp
							);
					}
					else
					{
						regionKeyframeRes.InterpolateKeyframe(
							cycle1.GetKeyframe(regionId, timeSampleIndex1),
							cycle2.GetKeyframe(regionId, timeSampleIndex1),
							cycle1.GetKeyframe(regionId, timeSampleIndex2),
							cycle2.GetKeyframe(regionId, timeSampleIndex2),
							cycleInterp,
							timeInterp
							);
					}
			

					baseKeyframe.InterpolateKeyframeBlend(
						regionKeyframeRes,
						m_regionStrengths[regionId]
						);
				}
			}
		}
	}

	u32 overTimeIdx = g_weather.GetOverTimeType();
	if( overTimeIdx != 0xffffffff )
	{
		tcCycle& cycle = GetCycle(overTimeIdx);
		
		tcKeyframeUncompressed baseOverTimeKeyFrame;

		baseOverTimeKeyFrame.InterpolateKeyframe(
			cycle.GetKeyframe(baseRegionId, timeSampleIndex1),
			cycle.GetKeyframe(baseRegionId, timeSampleIndex2),
			timeInterp
			);

		for (int regionId = baseRegionId + 1; regionId < TIMECYCLE_MAX_REGIONS; regionId++) // apply other regions
		{
			if (m_regionStrengths[regionId] > 0.0f)
			{
				tcKeyframeUncompressed regionKeyframeRes;

				regionKeyframeRes.InterpolateKeyframe(
					cycle.GetKeyframe(regionId, timeSampleIndex1),
					cycle.GetKeyframe(regionId, timeSampleIndex2),
					timeInterp
					);

				baseOverTimeKeyFrame.InterpolateKeyframeBlend(
					regionKeyframeRes,
					m_regionStrengths[regionId]
					);
			}
		}
		
		if( !fwTimer::IsGamePaused() )
		{
			// Yeah, we're doing the update here, a bit later
			// but if not, we get a nasty flash due to update ordering.
			g_weather.UpdateOverTimeTransition();
		}

		float overTimeDelta = g_weather.GetOverTimeInterp();
	
		baseKeyframe.InterpolateKeyframeBlend(baseOverTimeKeyFrame,overTimeDelta);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdateUnderWater
///////////////////////////////////////////////////////////////////////////////													

void CTimeCycle::UpdateUnderWater(tcKeyframeUncompressed& keyframe, const u32 hours, const u32 minutes, const u32 seconds)
{
	
	float underWaterDepth = Water::GetWaterLevel() - camInterface::GetPos().z;
	// get the time sample info (this is also done in CalcBaseKeyframe so could be done once in Update and passed through to both functions instead)
	u32 timeSample1;
	u32 timeSample2;
	float timeInterp;
	CalcTimeSampleInfo(hours, minutes, seconds, timeSample1, timeSample2, timeInterp);

	// apply the underwater modifier to the current keyframe
#if TC_DEBUG
	if (!g_timeCycle.GetGameDebug().GetDisableBaseUnderWaterModifier())
#endif
	{
		// apply global underwater modifier first
		const tcModifier *modifier	= FindModifier(TC_ATSTRINGHASH("underwater", 0xA2663418),"underwater");
		modifier->Apply(keyframe,1.0f);
#if __BANK
		grcDebugDraw::AddDebugOutput("UnderWater Modifier: %.3f", 1.0f);
#endif
	}

	// check if there is an underwater cycle
#if TC_DEBUG
	if (!g_timeCycle.GetGameDebug().GetDisableUnderWaterCycle()) 
#endif
	{
		// blend in the shallow cycle between the start and mid depths
		s32 waterCycleIndex = GetCycleIndex(ATSTRINGHASH("underwater_deep", 0x8eb53546));
		if (waterCycleIndex>-1)
		{
			const CMainWaterTune& waterTunings = Water::GetMainWaterTunings();
			float start	= waterTunings.m_WaterCycleDepth;
			float end	= start + waterTunings.m_WaterCycleFade;
			if (underWaterDepth > start)
			{
				float interp = CVfxHelper::GetInterpValue(underWaterDepth, start, end);
				tcCycle& shallowCycle = GetCycle(waterCycleIndex);
				tcKeyframeUncompressed waterKeyframe;

				// calc the underwater keyframe between the two time samples
				waterKeyframe.InterpolateKeyframe(
					shallowCycle.GetKeyframe(0, timeSample1),
					shallowCycle.GetKeyframe(0, timeSample2),
					timeInterp
					);

				// calc the keyframe between the weather and underwater keyframes
				keyframe.InterpolateKeyframeBlend(
					waterKeyframe,
					interp
					);
#if TC_DEBUG
				grcDebugDraw::AddDebugOutput("Underwater Shallow Cycle: %.3f", interp);
#endif

				start = waterTunings.m_DeepWaterModDepth;
				if(underWaterDepth > start)
				{
					end		= start + waterTunings.m_DeepWaterModFade;
					interp	= CVfxHelper::GetInterpValue(underWaterDepth, start, end);

					const tcModifier *modifier	= FindModifier(TC_ATSTRINGHASH("underwater_deep", 0x8eb53546),"underwater_deep");
					modifier->Apply(keyframe, interp);
#if TC_DEBUG
					grcDebugDraw::AddDebugOutput("Underwater Deep modifier: %.3f", interp);
#endif
					
				}
			}
		}
	}

	// Recalculate unmoddifed directional light, since we modified it heavily for underwater
	const float dirLightMult = PostFX::GetUseNightVision() ? 
							   (m_frameInfo.m_keyframe.GetVar(TCVAR_LIGHT_DIR_MULT) * m_frameInfo.m_keyframe.GetVar(TCVAR_NV_LIGHT_DIR_MULT) ) : 
								m_frameInfo.m_keyframe.GetVar(TCVAR_LIGHT_DIR_MULT) * m_frameInfo.m_directionalLightIntensity;

	// save the unmodified directional light data - for use with daylight fillers and light shafts
	m_frameInfo.m_vBaseDirectionalLight = Vec4V(m_frameInfo.m_keyframe.GetVar(TCVAR_LIGHT_DIR_COL_R), 
												m_frameInfo.m_keyframe.GetVar(TCVAR_LIGHT_DIR_COL_G), 
												m_frameInfo.m_keyframe.GetVar(TCVAR_LIGHT_DIR_COL_B), 
												dirLightMult);
	


	// apply the underwater modifier boxes to the current keyframe
	
#if TC_DEBUG
	if (!g_timeCycle.GetGameDebug().GetDisableUnderWaterModifiers()) 
#endif
	{
		ApplyUnderwaterModifiers(keyframe);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  InitLevelWidgets
///////////////////////////////////////////////////////////////////////////////													

#if TC_DEBUG
PARAM(noshowtcmodifierinfo,"");
void			CTimeCycle::InitLevelWidgets			()
{
	if (PARAM_noshowtcmodifierinfo.Get())
	{
		m_gameDebug.SetDisplayModifierInfo(false);
	}

	m_gameDebug.InitLevelWidgets();
	bkBank* bk=BANKMGR.FindBank("TimeCycle");
	ShadowFreezer.AddWidgets(*bk);

}
#endif


///////////////////////////////////////////////////////////////////////////////
//  ShutdownLevelWidgets
///////////////////////////////////////////////////////////////////////////////													

#if TC_DEBUG
void			CTimeCycle::ShutdownLevelWidgets		()
{
	m_gameDebug.ShutdownLevelWidgets();
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  Render
///////////////////////////////////////////////////////////////////////////////													

#if TC_DEBUG
void			CTimeCycle::Render						()
{
	// render any debug
	tcManager::Render();
}
#endif

///////////////////////////////////////////////////////////////////////////////
//  SetShaderParams
///////////////////////////////////////////////////////////////////////////////													

void CTimeCycle::SetShaderParams()
{
	g_timeCycle.SetShaderData();
}

///////////////////////////////////////////////////////////////////////////////
//  SetShaderData
///////////////////////////////////////////////////////////////////////////////													

void			CTimeCycle::SetShaderData(CVisualEffects::eRenderMode mode)
{
	tcAssertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "not called from render thread");

	const tcKeyframeUncompressed& currRenderKeyframe = GetCurrRenderKeyframe();

	Vec4V vAmbOccEffectScale;	
	vAmbOccEffectScale.SetXf(currRenderKeyframe.GetVar(TCVAR_LIGHT_AMB_OCC_MULT));
	vAmbOccEffectScale.SetYf(currRenderKeyframe.GetVar(TCVAR_LIGHT_AMB_OCC_MULT) * currRenderKeyframe.GetVar(TCVAR_LIGHT_AMB_OCC_MULT_PED));
	vAmbOccEffectScale.SetZf(currRenderKeyframe.GetVar(TCVAR_LIGHT_AMB_OCC_MULT) * currRenderKeyframe.GetVar(TCVAR_LIGHT_AMB_OCC_MULT_VEH));
	vAmbOccEffectScale.SetWf(currRenderKeyframe.GetVar(TCVAR_LIGHT_AMB_OCC_MULT_PROP));
	CShaderLib::SetGlobalAmbientOcclusionEffect(RCC_VECTOR4(vAmbOccEffectScale));

	float rimScale = currRenderKeyframe.GetVar(TCVAR_LIGHT_PED_RIM_MULT);
	CShaderLib::SetGlobalPedRimLightScale(rimScale);

	float wetness=rage::Clamp(g_weather.GetWetness(), 0.0f, 1.0f);
	if (wetness>0.1f)
	{
		CShaderLib::SetDeferredWetBlendEnable(true);
	}
	else
	{
		CShaderLib::SetDeferredWetBlendEnable(false);
	}

#if TC_DEBUG
	if (m_gameDebug.GetDisableFogAndHaze())
	{
		CShaderLib::CFogParams vFogParams; 

		CShaderLib::SetGlobalFogParams( vFogParams );
		CShaderLib::SetGlobalFogColors( Vec3V(V_ZERO), Vec3V(V_ZERO), Vec3V(V_ZERO), Vec3V(V_ZERO), Vec3V(V_ZERO) );
	#if LINEAR_PIECEWISE_FOG
		LinearPiecewiseFog_ShaderParams linearParams; // Default to off.
		CShaderLib::SetLinearPiecewiseFogParams(linearParams);
	#endif // LINEAR_PIECEWISE_FOG
	}
	else
#endif // TC_DEBUG
	{
		const grcViewport *viewport = gVpMan.GetRenderGameGrcViewport();
		const float cameraHeight = viewport->GetCameraMtx().GetM23f() ;

		const float fs = currRenderKeyframe.GetVar(TCVAR_FOG_START);
		const ScalarV fogStart(fs);
		const ScalarV fogEnd(Max(currRenderKeyframe.GetVar(TCVAR_FAR_CLIP), fs));

		// for the fog plane blit, we will use a z value that is the same as the closest fog start value.
		// that way the fog is skipped for close objects  (and frequently interiors)
		Vec3V edge = fogStart*Normalize(Vec3V(viewport->GetTanHFOV(), viewport->GetTanVFOV(), -1.0f)); // the ray along the edge of the view frustum.

		ScalarV fogStartDepth = edge.GetZ();
		fogStartDepth = SelectFT(IsLessThan(fogStartDepth,ScalarV(-viewport->GetNearClip())), ScalarV(V_ZERO),(fogStartDepth*viewport->GetProjection().c().GetZ() + viewport->GetProjection().d().GetZ())/-fogStartDepth); // calc depth in screen space.

		float fogNearDensity = currRenderKeyframe.GetVar(TCVAR_FOG_DENSITY) / 10000.0f;
		float fogNearHeightFalloff = currRenderKeyframe.GetVar(TCVAR_FOG_HEIGHT_FALLOFF) / 1000.0f;
		float fogNearAlpha = currRenderKeyframe.GetVar(TCVAR_FOG_ALPHA) ;
		
		float fogGroundHeight = currRenderKeyframe.GetVar(TCVAR_FOG_BASE_HEIGHT);	
		
		float fogHeightDensityAtViewer = expf( Min(-fogNearHeightFalloff * (cameraHeight-fogGroundHeight), 20.0f) ); // limit the value when going below the fog ground height so exp does not explode 
		float fogFarDensity = currRenderKeyframe.GetVar(TCVAR_FOG_HAZE_DENSITY) / 10000.0f;
		float fogFarAlpha = currRenderKeyframe.GetVar(TCVAR_FOG_HAZE_ALPHA) ;
		float fogHorizonTintScale = currRenderKeyframe.GetVar(TCVAR_FOG_HORIZON_TINT_SCALE);
		float fogHazeStart = currRenderKeyframe.GetVar(TCVAR_FOG_HAZE_START);

		CShaderLib::CFogParams vFogParams;

		// note we pre-negate fogFarDensity  and pre-multiply fogHeightDensityAtViewer by -fogNearDensity, to save shader instructions
		vFogParams.vFogParams[0] = VEC4V_TO_VECTOR4(Vec4V(fogStart, fogEnd, fogStartDepth, ScalarV(V_ZERO)));
		vFogParams.vFogParams[1] = VEC4V_TO_VECTOR4(Vec4V(-fogFarDensity, fogFarAlpha, fogHorizonTintScale / fogEnd.Getf(),-Min(1.0f,fogHeightDensityAtViewer*fogNearDensity)));
		vFogParams.vFogParams[2] = VEC4V_TO_VECTOR4(Vec4V(fogHazeStart, fogNearAlpha, fogNearHeightFalloff, Clamp(1.0f-exp(fogFarDensity*fogEnd.Getf()),0.0f, 1.0f)*fogFarAlpha));	 
		vFogParams.vSunDirAndPower = VEC4V_TO_VECTOR4(Vec4V((GetLightningOverrideRender()?GetLightningDirectionRender():GetCurrSunSpriteDirection()), ScalarV(currRenderKeyframe.GetVar(TCVAR_FOG_SUN_LIGHTING_CALC_POW))));
		vFogParams.vMoonDirAndPower = VEC4V_TO_VECTOR4(Vec4V((GetLightningOverrideRender()?GetLightningDirectionRender():GetCurrMoonSpriteDirection()), ScalarV(currRenderKeyframe.GetVar(TCVAR_FOG_MOON_LIGHTING_CALC_POW))));

		CShaderLib::SetGlobalFogParams( vFogParams );

		CShaderLib::SetFogRayIntensity(currRenderKeyframe.GetVar(TCVAR_FOG_FOGRAY_INTENSITY));

		const float hdrMult = (mode == CVisualEffects::RM_CUBE_REFLECTION) ? currRenderKeyframe.GetVar(TCVAR_REFLECTION_HDR_MULT) : 1.0f;

		// get fog colours and pre-blend near fog to east and west far fog
		const ScalarV hdrMultiplier = ScalarVFromF32(currRenderKeyframe.GetVar(TCVAR_FOG_HDR) * hdrMult);

		Vec3V vFogColorSun = (GetLightningOverrideRender()? GetLightningFogColorRender().GetRGB():currRenderKeyframe.GetVarV3(TCVAR_FOG_SUN_COL_R)) * hdrMultiplier;
		Vec3V vFogColorMoon = (GetLightningOverrideRender()? GetLightningFogColorRender().GetRGB():currRenderKeyframe.GetVarV3(TCVAR_FOG_MOON_COL_R)) * hdrMultiplier;
		Vec3V vFogColorAtmosphere = currRenderKeyframe.GetVarV3(TCVAR_FOG_FAR_COL_R) * hdrMultiplier;
		Vec3V vFogColorGround = currRenderKeyframe.GetVarV3(TCVAR_FOG_NEAR_COL_R) * hdrMultiplier;
		Vec3V vFogColorHaze = currRenderKeyframe.GetVarV3(TCVAR_FOG_HAZE_COL_R) * ScalarVFromF32(currRenderKeyframe.GetVar(TCVAR_FOG_HAZE_HDR) * hdrMult);

		// lerp the far and sky colors to the near color based on our fog density at the eye point
		ScalarV fogColorLerp = ScalarV(powf(Clamp(fogHeightDensityAtViewer,0.0f,1.0f),0.3f));
		vFogColorAtmosphere = Lerp(fogColorLerp, vFogColorAtmosphere, vFogColorGround);

		float fade = Max(m_frameInfo.m_moonFade,m_frameInfo.m_sunFade);
		CShaderLib::SetGlobalFogColors(Lerp(ScalarV(fade),vFogColorAtmosphere,vFogColorSun), vFogColorAtmosphere, vFogColorGround, vFogColorHaze, vFogColorMoon);

	#if LINEAR_PIECEWISE_FOG
		LinearPiecewiseFog_ShaderParams linearParams;

		// TODO:- Get from time cycle. Just use the values tweaked by RAG for now.
		LinearPiecewiseFog::FogKeyframe fogKeyFrame;

		fogKeyFrame.m_minHeight = currRenderKeyframe.GetVar(TCVAR_FOG_SHAPE_LOW);
		fogKeyFrame.m_maxHeight = currRenderKeyframe.GetVar(TCVAR_FOG_SHAPE_HIGH);
		fogKeyFrame.m_log10MinVisibility = currRenderKeyframe.GetVar(TCVAR_FOG_LOG_10_MIN_VISIBILIY);
		fogKeyFrame.m_shapeWeights = currRenderKeyframe.GetVarV4(TCVAR_FOG_SHAPE_WEIGHT_0);

		LinearPiecewiseFog::ComputeShaderParams(linearParams, cameraHeight, fogKeyFrame);
		CShaderLib::SetLinearPiecewiseFogParams(linearParams);
	#endif // LINEAR_PIECEWISE_FOG
	}
}

///////////////////////////////////////////////////////////////////////////////
//  LoadVisualSettings
///////////////////////////////////////////////////////////////////////////////													

void			CTimeCycle::LoadVisualSettings				()
{
	// check the visual settings are loaded
	tcAssertf(g_visualSettings.GetIsLoaded(), "visual settings aren't loaded");

	// read in the visual settings from the data file

	// hd/sd multipliers
	if (GRCDEVICE.GetHiDef())
	{
		m_vsDOFblurScale = g_visualSettings.Get("misc.DOFBlurMultiplier.HD", 1.0f);
	}
	else
	{
		m_vsDOFblurScale = g_visualSettings.Get("misc.DOFBlurMultiplier.SD", 0.5f);
	}

	// altitude base far clip multiplier
	float heightStart = g_visualSettings.Get("misc.Multiplier.heightStart", 100.0f);
	float heightEnd = g_visualSettings.Get("misc.Multiplier.heightEnd", 250.0f);
	float clipMultiplier = g_visualSettings.Get("misc.Multiplier.farClipMultiplier", 2.0f);
	float fogMultiplier = g_visualSettings.Get("misc.Multiplier.nearFogMultiplier", 0.5f);

	m_vsFarClipMultiplier = clipMultiplier;
	m_vsNearFogMultiplier = fogMultiplier;
	m_vsMultiplierHeightStart = heightStart;
	m_vsOoMultiplierRange = 1.0f / (heightEnd-heightStart);

	m_vsRimLight = g_visualSettings.Get("misc.CrossPMultiplier.RimLight", 1.0f);
	m_vsGlobalEnvironmentReflection = g_visualSettings.Get("misc.CrossPMultiplier.GlobalEnvironmentReflection", 1.0f);
	m_vsGamma = g_visualSettings.Get("misc.CrossPMultiplier.Gamma", 1.0f);
	m_vsMidBlur = g_visualSettings.Get("misc.CrossPMultiplier.MidBlur", 1.0f);
	m_vsFarBlur = g_visualSettings.Get("misc.CrossPMultiplier.Farblur", 1.0f);
	m_vsSkyMultiplier = g_visualSettings.Get("misc.CrossPMultiplier.SkyMultiplier", 1.0f);
	m_vsDesaturation = g_visualSettings.Get("misc.CrossPMultiplier.Desaturation", 1.0f);

	// cutscene near blur factor
	m_vsCutsceneNearBlurMin = g_visualSettings.Get("misc.cutscene.nearBlurMin", 0.2f);

	// hi dof blur factor
	m_vsSpecularOffset = g_visualSettings.Get("raincollision", "specularoffset",  1.0f);

	// Moon dimming due to lunar cycle
	m_vsMoonDimMult = g_visualSettings.Get("misc.MoonDimMult", 1.0f);

	m_vsNextGenModifer = g_visualSettings.Get("misc.NextGenModifier", 1.0f);

}


///////////////////////////////////////////////////////////////////////////////
//  ApplyVisualSettings
///////////////////////////////////////////////////////////////////////////////													

void CTimeCycle::ApplyVisualSettings(tcKeyframeUncompressed& keyframe, Vec3V_In vCameraPos) const
{
	tcAssert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	
	// calc the far clip and near fog multipliers (can these be done in UpdateFinalize where they are used?)
	const float scaler = Clamp((vCameraPos.GetZf() - m_vsMultiplierHeightStart)*(m_vsOoMultiplierRange), 0.0f, 1.0f);
	float farClipMult = 1.0f + (m_vsFarClipMultiplier - 1.0f) * scaler;
	float nearFogMult = 1.0f + (m_vsNearFogMultiplier - 1.0f) * scaler;

	// apply scalers
	keyframe.MultVar(TCVAR_FOG_NEAR_COL_A, nearFogMult);
	keyframe.MultVar(TCVAR_FAR_CLIP, farClipMult);

	//TF **REPLACE WITH NEW CUTSCENE**
//	if (CCutsceneManager::IsTimeCycleSafe())
//	{
//		keyframe.SetVar(TCVAR_DOF_BLUR_MID, Max(keyframe.GetVar(TCVAR_DOF_BLUR_MID), m_vsCutsceneNearBlurMin));
//	}

	keyframe.MultVar(TCVAR_DOF_BLUR_MID, m_vsDOFblurScale);
	keyframe.MultVar(TCVAR_DOF_BLUR_FAR, m_vsDOFblurScale);
	keyframe.MultVar(TCVAR_LIGHT_PED_RIM_MULT, m_vsRimLight);
	keyframe.MultVar(TCVAR_DOF_BLUR_MID, m_vsMidBlur);
	keyframe.MultVar(TCVAR_DOF_BLUR_FAR, m_vsFarBlur);
	keyframe.MultVar(TCVAR_SKY_HDR, m_vsSkyMultiplier);
	keyframe.MultVar(TCVAR_POSTFX_DESATURATION, m_vsDesaturation);
	
#if TC_DEBUG
 	if (!g_timeCycle.GetGameDebug().GetDisableHighAltitudeModifier()) 
#endif
	{
		const tcModifier *modifier = FindModifier(atHashString("highaltitude",0x8CF23FD5),"ApplyVisualSettings",true);
		if( modifier )
			modifier->Apply(keyframe, scaler);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ApplyCutsceneModifiers
///////////////////////////////////////////////////////////////////////////////													

void CTimeCycle::ApplyCutsceneModifiers(tcKeyframeUncompressed& keyframe)
{
	tcAssert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	
#if TC_DEBUG
 	if (g_timeCycle.GetGameDebug().GetDisableCutsceneModifiers()) 
 	{
 		return;
	}
#endif

	// override the cutscene modifier if we are currently editing a modifier
	const tcModifier *cutsceneModifier = GetModifier(m_cutsceneModifierIndex);
#if TC_DEBUG
	if (m_debug.GetModifierEditApply())		
	{
		cutsceneModifier = GetModifier(m_debug.GetModifierEditId());
	}

	// output info to screen
	if (g_timeCycle.GetGameDebug().GetDisplayModifierInfo())
	{
		grcDebugDraw::AddDebugOutput("Cutscene Modifier: %s(%d) - 1.0", GetModifierName(m_cutsceneModifierIndex),FindModifierRecordIndex(m_cutsceneModifierIndex));
	}
#endif // TC_DEBUG		

	// modify the keyframe using this modifier
	cutsceneModifier->Apply(keyframe, 1.0f);

	// set the interior blend strength to be full
	m_interiorBlendStrength = 1.0f;	
}

#define SetCSOverride(varIdx, value)\
if( value != -1.0f )\
{\
	keyframe.SetVar(varIdx,value);\
}

#define MinCSOverride(varIdx, value)\
if( value != -1.0f )\
{\
	float var = keyframe.GetVar(varIdx);\
	keyframe.SetVar(varIdx,Min(var,value));\
}

#define MulCSOverride(varIdx, value)\
if( value != -1.0f )\
{\
	float var = keyframe.GetVar(varIdx);\
	keyframe.SetVar(varIdx,var*value);\
}

void CTimeCycle::ApplyCutsceneCameraOverrides(tcKeyframeUncompressed& keyframe)
{
#if TC_DEBUG
	if( g_timeCycle.GetGameDebug().GetDisableCutsceneModifiers())
		return;
#endif

	CutSceneManager* csMgr = CutSceneManager::GetInstance();
	Assert(csMgr);

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		const cutfCameraCutEventArgs* replayArgs = csMgr->GetReplayCutsceneCameraArgs();

		if(replayArgs)
		{
			SetCSOverride(TCVAR_LOD_MULT,replayArgs->GetMapLodScale());
			SetCSOverride(TCVAR_REFLECTION_LOD_RANGE_START, replayArgs->GetReflectionLodRangeStart());
			SetCSOverride(TCVAR_REFLECTION_LOD_RANGE_END, replayArgs->GetReflectionLodRangeEnd());
			SetCSOverride(TCVAR_REFLECTION_SLOD_RANGE_START, replayArgs->GetReflectionSLodRangeStart());
			SetCSOverride(TCVAR_REFLECTION_SLOD_RANGE_END, replayArgs->GetReflectionSLodRangeEnd());
			SetCSOverride(TCVAR_LOD_MULT_HD, replayArgs->GetLodMultHD());
			SetCSOverride(TCVAR_LOD_MULT_ORPHANHD, replayArgs->GetLodMultOrphanHD());
			SetCSOverride(TCVAR_LOD_MULT_LOD, replayArgs->GetLodMultLod());
			SetCSOverride(TCVAR_LOD_MULT_SLOD1, replayArgs->GetLodMultSlod1());
			SetCSOverride(TCVAR_LOD_MULT_SLOD2, replayArgs->GetLodMultSlod2());
			SetCSOverride(TCVAR_LOD_MULT_SLOD3, replayArgs->GetLodMultSlod3());
			SetCSOverride(TCVAR_LOD_MULT_SLOD4, replayArgs->GetLodMultSlod4());
			SetCSOverride(TCVAR_WATER_REFLECTION_FAR_CLIP, replayArgs->GetWaterReflectionFarClip());
			SetCSOverride(TCVAR_LIGHT_SSAO_INTEN, replayArgs->GetLightSSAOInten());
			MulCSOverride(TCVAR_LIGHT_DIR_MULT, replayArgs->GetDirectionalLightMultiplier());
			MulCSOverride(TCVAR_LENS_ARTEFACTS_INTENSITY, replayArgs->GetLensArtefactMultiplier());
			MinCSOverride(TCVAR_POSTFX_INTENSITY_BLOOM,replayArgs->GetBloomMax());
		}
		
		return;
	}
#endif

	if( csMgr->IsCutscenePlayingBack() )
	{
		const CCutSceneCameraEntity* cam = csMgr->GetCamEntity();
		if( cam )
		{
			SetCSOverride(TCVAR_LOD_MULT,cam->GetMapLodScale());
			SetCSOverride(TCVAR_REFLECTION_LOD_RANGE_START, cam->GetReflectionLodRangeStart());
			SetCSOverride(TCVAR_REFLECTION_LOD_RANGE_END, cam->GetReflectionLodRangeEnd());
			SetCSOverride(TCVAR_REFLECTION_SLOD_RANGE_START, cam->GetReflectionSLodRangeStart());
			SetCSOverride(TCVAR_REFLECTION_SLOD_RANGE_END, cam->GetReflectionSLodRangeEnd());
			SetCSOverride(TCVAR_LOD_MULT_HD, cam->GetLodMultHD());
			SetCSOverride(TCVAR_LOD_MULT_ORPHANHD, cam->GetLodMultOrphanHD());
			SetCSOverride(TCVAR_LOD_MULT_LOD, cam->GetLodMultLod());
			SetCSOverride(TCVAR_LOD_MULT_SLOD1, cam->GetLodMultSlod1());
			SetCSOverride(TCVAR_LOD_MULT_SLOD2, cam->GetLodMultSlod2());
			SetCSOverride(TCVAR_LOD_MULT_SLOD3, cam->GetLodMultSlod3());
			SetCSOverride(TCVAR_LOD_MULT_SLOD4, cam->GetLodMultSlod4());
			SetCSOverride(TCVAR_WATER_REFLECTION_FAR_CLIP, cam->GetWaterReflectionFarClip());
			SetCSOverride(TCVAR_LIGHT_SSAO_INTEN, cam->GetLightSSAOInten());
			MulCSOverride(TCVAR_LIGHT_DIR_MULT, cam->GetDirectionalLightMultiplier());
			MulCSOverride(TCVAR_LENS_ARTEFACTS_INTENSITY, cam->GetLensArtefactMultiplier());
			MinCSOverride(TCVAR_POSTFX_INTENSITY_BLOOM,cam->GetBloomMax());
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//  ApplyBoxModifiers
///////////////////////////////////////////////////////////////////////////////
void CTimeCycle::ApplyBoxModifiers(tcKeyframeUncompressed& keyframe, float alpha, bool interiorOnly, bool underWaterOnly)
{
	tcAssert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	
#if TC_DEBUG
	if (g_timeCycle.GetGameDebug().GetDisableBoxModifiers()) 
	{
		return;
	}
#endif

	// set water height override to current water height, so modifiers can interpolate it properly
	keyframe.SetVar(TCVAR_WATER_REFLECTION_HEIGHT_OVERRIDE, CGameWorldWaterHeight::GetWaterHeight());

	m_interiorReset = false;
	
	// modify the keyframe with the active modifiers 
	for (s32 n=0; n<GetModifiersCount(); n++)
	{
		if (m_modifierStrengths[n] > 0.0f)
		{
			const tcModifier *modifier = GetModifier(n);

			// do the modify
			if(modifier->GetUserFlag(tcflag_underwateronly) == !underWaterOnly) //skip underwater
				continue;

			if(modifier->GetUserFlag(tcflag_interioronly) == !interiorOnly) //skip interior
				continue;

			m_interiorReset |= modifier->GetUserFlag(tcflag_resetInteriorAmbCache);
			
			// output info to screen
#if TC_DEBUG
			// Gate this with a check for GetDisplayDebugText() - even if its off and won't render, doing the GetModifierName(), 
			// FindModifierRecordIndex() and some of the code in AddDebugOutput() before it kicks out for every modifier adds up.
			if (grcDebugDraw::GetDisplayDebugText() && g_timeCycle.GetGameDebug().GetDisplayModifierInfo())
			{
				grcDebugDraw::AddDebugOutput("Box Modifier: %s(%d) - %.3f", GetModifierName(n), FindModifierRecordIndex(n), m_modifierStrengths[n]);
			}
#endif

			modifier->Apply(keyframe, m_modifierStrengths[n] * alpha);
		}
	}
}

void CTimeCycle::ApplyUnderwaterModifiers(tcKeyframeUncompressed& keyframe)
{
	tcAssert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

#if TC_DEBUG
	if (g_timeCycle.GetGameDebug().GetDisableBoxModifiers())
	{
		return;
	}
#endif

	// modify the keyframe with the active modifiers 
	for (s32 n=0; n<GetModifiersCount(); n++)
	{
		if (m_modifierStrengths[n] > 0.0f)
		{
			const tcModifier *mod = GetModifier(n);

			if(!mod->GetUserFlag(tcflag_underwateronly))//skip non underwater modifiers, might need an enum rather than magic numbers
				continue;

			if(mod->GetUserFlag(tcflag_interioronly))//skip non underwater modifiers, might need an enum rather than magic numbers
				continue;

#if TC_DEBUG
			// output info to screen
			if (g_timeCycle.GetGameDebug().GetDisplayModifierInfo())
			{
				grcDebugDraw::AddDebugOutput("Underwater Box Modifier: %s(%d) - %.3f", GetModifierName(n), FindModifierRecordIndex(n), m_modifierStrengths[n]);
			}
#endif

			// do the modify
			mod->Apply(keyframe, m_modifierStrengths[n]);
		}
	}
	
}


///////////////////////////////////////////////////////////////////////////////
//  ApplyInteriorModifiers
///////////////////////////////////////////////////////////////////////////////													

bool CTimeCycle::ApplyInteriorModifiers(tcKeyframeUncompressed& keyframe, bool isUnderWater, bool finalUpdate)
{
	tcAssert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	
#if TC_DEBUG
	if (g_timeCycle.GetGameDebug().GetDisableInteriorModifiers()) 
	{
		return false;
	}
#endif

	//
	// 1. GET INFO ABOUT ANY INTERIOR WE SHOULD BE USING
	//

	// Reset portal lists
	const Vec3V camPos = VECTOR3_TO_VEC3V(camInterface::GetPos());
	
	for(int i=0;i<m_frameInfo.m_LocalPortalCache.GetCount();i++)
	{
		portalData &portal = m_frameInfo.m_LocalPortalCache[i];
		const fwPortalCorners &portalCorners = portal.GetCorners();
		const ScalarV sphereDistSq = portalCorners.CalcSimpleDistanceToPointSquared(camPos);

		const Vec3V extent = (portalCorners.GetCornerV(0) - portalCorners.GetCornerV(2))*ScalarV(V_HALF);
		const ScalarV extentMagSquared = Max(ScalarV(V_FIFTEEN)*ScalarV(V_FIFTEEN), MagSquared(extent));
		
		if(IsGreaterThanAll(sphereDistSq, extentMagSquared))
		{
			portal.SetScreenArea(0.0f);
			portal.SetInUse(false);
		}
	}
	
	// get the current interior info based on the camera 
	CInteriorProxy* pInteriorProxy = NULL;
	CInteriorInst* pInteriorInst = NULL;
	s32 roomIdx = 0;
	s32 scannedInteriorProxyId = CRenderer::GetScannedVisiblePortalInteriorProxyID();

	if( g_SceneToGBufferPhaseNew && g_SceneToGBufferPhaseNew->GetPortalVisTracker() )
	{
		CViewport *cvp = g_SceneToGBufferPhaseNew->GetViewport();
		CPortalVisTracker *portalTracker = g_SceneToGBufferPhaseNew->GetPortalVisTracker();
		Vector3 cameraPosition = camInterface::GetPos();
		
		const CEntity* pAttachEntity = cvp->GetAttachEntity();
		bool bFirstPerson = pAttachEntity && pAttachEntity->GetIsTypePed() && static_cast<const CPed*>(pAttachEntity)->IsFirstPersonShooterModeEnabledForPlayer(false);
		entitySelectID targetEntityID = pAttachEntity? CEntityIDHelper::ComputeEntityID(pAttachEntity) : 0;

		if (cvp->GetAttachEntity() && (!bFirstPerson || portalTracker->ForceUpdateUsingTarget() || targetEntityID != portalTracker->GetLastTargetEntityId())) {
			portalTracker->UpdateUsingTarget(cvp->GetAttachEntity(), cameraPosition);
		} else {
			portalTracker->Update(cameraPosition);
		}

		portalTracker->SetLastTargetEntityId(targetEntityID);

		pInteriorProxy = portalTracker->m_pInteriorProxy;
		pInteriorInst = pInteriorProxy ? pInteriorProxy->GetInteriorInst() : NULL;
		roomIdx = portalTracker->m_roomIdx;
		
		CPortalVisTracker::StorePrimaryData(portalTracker->m_pInteriorProxy, portalTracker->m_roomIdx);	
	}

	// override this info based on which interiors we are 'looking at'
	const grcViewport &viewPort = CRenderer::FixupScannedVisiblePortalViewport(*gVpMan.GetUpdateGameGrcViewport());
	if( finalUpdate )
	{
		CRenderer::ResetScannedVisiblePortal();
	}
	if (pInteriorInst==NULL || roomIdx<= 0)
	{
		// we are outside - check if we are looking close enough at one
		if (scannedInteriorProxyId>=0)
		{
			pInteriorProxy = CInteriorProxy::GetPool()->GetSlot(scannedInteriorProxyId);
			pInteriorInst = pInteriorProxy->GetInteriorInst();
		}

		roomIdx = 0;
	}	
	else
	{
		// we are inside an interior
		CRenderer::SetScannedVisiblePortalInteriorProxyID(CInteriorProxy::GetPool()->GetJustIndex(pInteriorProxy));
	}

	// check that any interior that we have chosen to use it valid
	if (pInteriorInst!=NULL) 
	{
		// validate the interior instance - is it ready to be used?
		if (pInteriorInst->m_bInUse==false || pInteriorInst->m_bIsPopulated==false)
		{
			pInteriorInst = NULL;
			roomIdx = 0;
		}
	}

	// 1.1 WORK OUT THE INTERIOR AMBIENT COLORS AND SCREAM WHILE DOING IT BECAUSE WE LIKE SCREAMING
	if( roomIdx == 0 )
	{
		s32 mainTcIdx = CRenderer::GetScannedVisiblePortalInteriorProxyMainTCModifier();
		s32 secondaryTcIdx = CRenderer::GetScannedVisiblePortalInteriorProxySecondaryTCModifier();
  
		m_interiorApply = true;
		if( mainTcIdx != -1 && m_interior_main_idx != mainTcIdx )
		{
			SetInteriorCache(mainTcIdx, secondaryTcIdx);
			
			CRenderer::SetScannedVisiblePortalInteriorProxyMainTCModifier(-1);
			CRenderer::SetScannedVisiblePortalInteriorProxySecondaryTCModifier(-1);
		}
	}
	else
	{
		m_interiorApply = false;
	}


	//
	// 1.8. SCREAM SOME MORE
	//
	if( m_interiorReset )
	{
		m_interior_main_idx = -1;
		m_interior_secondary_idx = -1;
		m_interiorInitialised = false;
		m_interiorReset = false;
	}	

	//
	// 2. GET MODIFIER INFO ABOUT THE INTERIOR
	//

	// get information about the modifiers in this interior
	static CPortal::modifierQueryResults modifiers;
	if( CPauseMenu::GetPauseRenderPhasesStatus() == false )
	{
		modifiers.Reset();
		if (pInteriorInst!=NULL)
		{
			// valid interior 
			CPortal::FindModifiersForInteriors(viewPort, pInteriorInst, roomIdx, &modifiers);
		}
	}

	// return early if that are no interior modifers
	if (modifiers.GetCount() == 0)
	{
		m_interiorBlendStrength = 0.0f;
		return false;
	}

	//
	// 3. CALC THE INTERIOR KEYFRAME THAT SHOULD BE BLENDED INTO THE CURRENT KEYFRAME
	//

	// Take the first modifier's portal weight
	float modifierWeightScale = 1.0f;
	if (modifiers[0].mainModifier != -1)
	{
		const tcModifier* currModifier = GetModifier(modifiers[0].mainModifier);
		currModifier->CreateAndLockVarMap();
		// Get portal's weight
		modifierWeightScale = currModifier->GetModDataValAUnsafe(TCVAR_PORTAL_WEIGHT);

		// Punch in interior ambient values
		if( roomIdx != 0 )
		{
			float punch_int_down_col_r = currModifier->GetModDataValAUnsafe(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_R);
			float punch_int_down_col_g = currModifier->GetModDataValAUnsafe(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_G);
			float punch_int_down_col_b = currModifier->GetModDataValAUnsafe(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_B);
			float punch_int_down_intensity = currModifier->GetModDataValAUnsafe(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_INTENSITY);
																			
			float punch_int_up_col_r = currModifier->GetModDataValAUnsafe(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_COL_R);
			float punch_int_up_col_g = currModifier->GetModDataValAUnsafe(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_COL_G);
			float punch_int_up_col_b = currModifier->GetModDataValAUnsafe(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_COL_B);
			float punch_int_up_intensity = currModifier->GetModDataValAUnsafe(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_INTENSITY);

			keyframe.SetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_R, punch_int_down_col_r);
			keyframe.SetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_G, punch_int_down_col_g);
			keyframe.SetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_B, punch_int_down_col_b);
			keyframe.SetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_INTENSITY, punch_int_down_intensity);
																									
			keyframe.SetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_COL_R, punch_int_up_col_r);
			keyframe.SetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_COL_G, punch_int_up_col_g);
			keyframe.SetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_COL_B, punch_int_up_col_b);
			keyframe.SetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_INTENSITY,	punch_int_up_intensity);
		}

		currModifier->UnlockVarMap();
	}
	

	// create a final interior modifier and initialise it to the keyframe that was passed in
	static tcModifier finalInteriorModifier; 
	finalInteriorModifier.Init(keyframe);
	
	// go through the interior modifiers
	float exteriorStrength = 0.0f;
	float interiorStrength = 0.0f;
	float exteriorFxStrength = 0.0f;
	bool noExteriorFog = false;

	for (s32 i=0; i<modifiers.GetCount(); i++)
	{
		const CPortal::modifierData &current = modifiers[i];
		
		float blendWeight = Clamp(current.blendWeight * modifierWeightScale,0.0f,1.0f);
		
		// check the portal type
		if (current.mainModifier == -1)
		{
			// exterior portal - update the exterior strength
			exteriorStrength = (current.strength*blendWeight) + (exteriorStrength*(1.0f-blendWeight));

			// output info to screen
#if TC_DEBUG
			if (g_timeCycle.GetGameDebug().GetDisplayModifierInfo())
			{
				grcDebugDraw::AddDebugOutput("Portal Modifier: exterior(-1) - %.3fx%.3f = %.3f", current.strength, blendWeight, exteriorStrength );
			}
#endif
		}
		else
		{
			// interior portal - update the strengths
			interiorStrength = (current.strength*blendWeight) + (interiorStrength*(1.0f-blendWeight));
			exteriorFxStrength = (1.0f*blendWeight)+(exteriorFxStrength*(1.0f-blendWeight));

			// get the current modifier
			const tcModifier* currModifier = GetModifier(current.mainModifier);

			// commented out due to false positives with TC mods added by script with ADD_TCMODIFIER_OVERRIDE();
			// see: B*3665495 - Error: currModifier == FindModifier(current.mainModifierName,"interior");
			//Assert(currModifier == FindModifier(current.mainModifierName,"interior")); 

			if (currModifier && currModifier->GetUserFlag(tcflag_underwateronly) == isUnderWater)
			{
				currModifier->CreateAndLockVarMap();

				const tcModifier* secondaryModifier = NULL;
				if( current.secondaryModifier>=0 TC_DEBUG_ONLY(&& g_timeCycle.GetGameDebug().GetDisableSecondaryInteriorModifiers() == false) )
				{
					secondaryModifier = GetModifier(current.secondaryModifier);
					if( secondaryModifier )
					{
						Assert(secondaryModifier == FindModifier(current.secondaryModifierName,"interior"));
						secondaryModifier->CreateAndLockVarMap();
					}
				}
								
				// blend the current modifier into the passed in keyframe with the correct strength
				static tcModifier tempModifier; 
				tempModifier.Init(keyframe);
				if (secondaryModifier)
				{
					tempModifier.Blend(*currModifier, *secondaryModifier, current.strength);
				}
				else
				{
					tempModifier.Blend(*currModifier, current.strength);
				}

				// blend this into the final interior modifier using the blend weight
				finalInteriorModifier.Blend(tempModifier, blendWeight);

				// check if we don't want any exterior fog
				if (currModifier->GetModDataValAUnsafe(TCVAR_FOG_NEAR_COL_A)==0.0f)
				{
					if (currModifier->GetModDataValAUnsafe(TCVAR_FOG_CUT_OFF)==0.0f)
					{
						noExteriorFog = true;
					}
				}
				
				if (secondaryModifier)
				{
					// check if we don't want any exterior fog
					if (secondaryModifier->GetModDataValAUnsafe(TCVAR_FOG_NEAR_COL_A)==0.0f)
					{
						if (secondaryModifier->GetModDataValAUnsafe(TCVAR_FOG_CUT_OFF)==0.0f)
						{
							noExteriorFog = true;
						}
					}
				}
				
				currModifier->UnlockVarMap();
				if (secondaryModifier)
				{
					 secondaryModifier->UnlockVarMap();
				}

				// output info to screen
#if TC_DEBUG
				if (g_timeCycle.GetGameDebug().GetDisplayModifierInfo())
				{
					int modifierIndex = GetModifierIndex((tcModifier*)currModifier);

					if( secondaryModifier )
					{
						int secondaryModifierIndex = GetModifierIndex((tcModifier*)secondaryModifier);
						grcDebugDraw::AddDebugOutput("Portal Modifier: %s(%d) (ovr %s(%d)) - %.3fx%.3f = %.3f", GetModifierName(modifierIndex), FindModifierRecordIndex(modifierIndex), GetModifierName(secondaryModifierIndex), FindModifierRecordIndex(secondaryModifierIndex), current.strength, blendWeight, interiorStrength );
					}
					else
					{
						grcDebugDraw::AddDebugOutput("Portal Modifier: %s(%d) - %.3fx%.3f = %.3f", GetModifierName(modifierIndex), FindModifierRecordIndex(modifierIndex), current.strength, blendWeight, interiorStrength );
					}
				}
#endif
			}
		}
	}

	// output info to screen
#if TC_DEBUG
	if (g_timeCycle.GetGameDebug().GetDisplayModifierInfo())
	{
		grcDebugDraw::AddDebugOutput("Blend Int %.3f Ext %.3f == %.3f", interiorStrength, exteriorStrength, interiorStrength * (1.0f-exteriorStrength));
	}
#endif


	//
	// BLEND THE INTERIOR KEYFRAME INTO THE CURRENT KEYFRAME
	//

	// store some exterior data before we blend so it can be re-applied after the blend if necessary
	float extNearFogStart = Max(keyframe.GetVar(TCVAR_FOG_START), 1.0f);
	Vector4 extNearFog(keyframe.GetVar(TCVAR_FOG_NEAR_COL_R), keyframe.GetVar(TCVAR_FOG_NEAR_COL_G), keyframe.GetVar(TCVAR_FOG_NEAR_COL_B), keyframe.GetVar(TCVAR_FOG_NEAR_COL_A));
	float extFarClip = keyframe.GetVar(TCVAR_FAR_CLIP);
	Vec3V vExtFog(keyframe.GetVar(TCVAR_FOG_SUN_COL_R), keyframe.GetVar(TCVAR_FOG_SUN_COL_G), keyframe.GetVar(TCVAR_FOG_SUN_COL_B));
	float extSkyMult = keyframe.GetVar(TCVAR_SKY_HDR);
	float extFogMult = keyframe.GetVar(TCVAR_FOG_HDR);

	// blend the final interior modifier into the passed in keyframe
	m_interiorBlendStrength = interiorStrength * (1.0f-exteriorStrength);
	finalInteriorModifier.Apply(keyframe, m_interiorBlendStrength);

	// Apply Box modifiers
	ApplyBoxModifiers(keyframe,m_interiorBlendStrength,true,isUnderWater);

	// recalc some data using the exterior settings 
#if TC_DEBUG
	if (m_gameDebug.GetDisableModifierFog()==false)
#endif
	{	
		m_interiorBlendStrength = exteriorFxStrength * (1.0f-exteriorStrength);	

		// recalc the far clip and sky mult, if required
		if (m_interiorBlendStrength<0.99f && keyframe.GetVar(TCVAR_FAR_CLIP)<extFarClip)
		{
			float blend = rage::Powf(rage::Clamp((m_interiorBlendStrength-INTERIOR_BLEND_OFFSET)*INTERIOR_BLEND_SCALE, 0.0f, 1.0f), INTERIOR_BLEND_EXP);

			keyframe.SetVar(TCVAR_FAR_CLIP, keyframe.GetVar(TCVAR_FAR_CLIP)*blend + extFarClip*(1.0f-blend));

			if (INTERIOR_BLEND_MATCH_SKY_TO_FAR_CLIP==true)
			{				
				keyframe.SetVar(TCVAR_FOG_SUN_COL_R, blend*keyframe.GetVar(TCVAR_FOG_SUN_COL_R) + (1.0f-blend) * vExtFog.GetXf());
				keyframe.SetVar(TCVAR_FOG_SUN_COL_G, blend*keyframe.GetVar(TCVAR_FOG_SUN_COL_G) + (1.0f-blend) * vExtFog.GetYf());
				keyframe.SetVar(TCVAR_FOG_SUN_COL_B, blend*keyframe.GetVar(TCVAR_FOG_SUN_COL_B) + (1.0f-blend) * vExtFog.GetZf());

				keyframe.SetVar(TCVAR_SKY_HDR, keyframe.GetVar(TCVAR_SKY_HDR)*blend + extSkyMult*(1.0f-blend));
				keyframe.SetVar(TCVAR_FOG_HDR, keyframe.GetVar(TCVAR_FOG_HDR)*blend + extFogMult*(1.0f-blend));
			}
		}

		// recalc the alpha fog, if required
		if (noExteriorFog)
		{
			keyframe.SetVar(TCVAR_FOG_ALPHA, keyframe.GetVar(TCVAR_FOG_ALPHA)*(1.0f-m_interiorBlendStrength));
		}

		// recalc the near fog and fog start, if required
		if (INTERIOR_BLEND_FOG_FADE)
		{
			float interiorFogFade = (1.0f-m_interiorBlendStrength);

			if (pInteriorInst!=NULL && m_interiorBlendStrength>0.01f)
			{
				float interiorFogDist = (pInteriorInst->GetBoundCentre()-RCC_MATRIX34(viewPort.GetCameraMtx()).d).Mag()+pInteriorInst->GetBoundRadius();

				if (m_interiorBlendStrength<0.99f && roomIdx!=0)
				{
					ScalarV portalFogDist=ScalarV(V_ZERO);
					float portalFogStrength=0.0f;

					for (s32 i=0; i<modifiers.GetCount(); i++)
					{
						if (modifiers[i].mainModifier == -1)
						{
							if (roomIdx!=0)
							{
								portalFogDist += Mag(modifiers[i].portalBoundSphere.GetCenter()-viewPort.GetCameraMtx().d());
								portalFogStrength += modifiers[i].strength*modifiers[i].blendWeight;
							}
						}
					}

					if (portalFogStrength>0.01f)
					{
						interiorFogDist = Min(interiorFogDist, portalFogDist.Getf()/portalFogStrength);
					}
				}

				float fogNum = (extNearFog.w*(interiorFogDist/extNearFogStart));
				if (fogNum>INTERIOR_BLEND_FOG_MAX)
				{
					interiorFogFade += (INTERIOR_BLEND_FOG_MAX/fogNum) * m_interiorBlendStrength;
				}
			}

			keyframe.SetVar(TCVAR_FOG_START, extNearFogStart);

			keyframe.SetVar(TCVAR_FOG_NEAR_COL_A, extNearFog.w*interiorFogFade);
			keyframe.SetVar(TCVAR_FOG_NEAR_COL_R, extNearFog.x);
			keyframe.SetVar(TCVAR_FOG_NEAR_COL_G, extNearFog.y);
			keyframe.SetVar(TCVAR_FOG_NEAR_COL_B, extNearFog.z);
		}
	}


	if( m_interiorInitialised && m_interiorApply)
	{
		// Apply interior ambient light, always.
		m_frameInfo.m_keyframe.SetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_R,m_interior_down_color_r);
		m_frameInfo.m_keyframe.SetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_G,m_interior_down_color_g);
		m_frameInfo.m_keyframe.SetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_B,m_interior_down_color_b);
		m_frameInfo.m_keyframe.SetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_INTENSITY,m_interior_down_intensity);

		m_frameInfo.m_keyframe.SetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_COL_R,m_interior_up_color_r);
		m_frameInfo.m_keyframe.SetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_COL_G,m_interior_up_color_g);
		m_frameInfo.m_keyframe.SetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_COL_B,m_interior_up_color_b);
		m_frameInfo.m_keyframe.SetVar(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_INTENSITY,m_interior_up_intensity);
	}

	// return the number of interior modifiers that were applied
	return true;
}

void			CTimeCycle::ApplyLodScaleModifiers			(WIN32PC_ONLY(tcKeyframeUncompressed& keyframe))
{
#if RSG_PC

	// don't apply scale modifiers if there's a cutscene or SRL running
	CutSceneManager* csMgr = CutSceneManager::GetInstance();
	if ( (csMgr && csMgr->IsCutscenePlayingBack()) || gStreamingRequestList.IsPlaybackActive())
	{
		return;
	}

	if (CSettingsManager::GetInstance().GetSettings().m_graphics.m_LodScale < 1.0f)
	{
		const tcModifier* pLodScaler	= FindModifier( atHashString("lodscaler", 0xd16586ab), "Lod Scale Modifier" );
		if( pLodScaler )
		{
		    const float alpha = 1.0f - CSettingsManager::GetInstance().GetSettings().m_graphics.m_LodScale;
		    pLodScaler->Apply(keyframe, alpha);
		}
	}

	if (CSettingsManager::GetInstance().GetSettings().m_graphics.m_MaxLodScale > 0.0f)
	{
		const tcModifier* pMaxLodScaler	= FindModifier( atHashString("maxlodscaler", 0x65c477df), "Max Lod Scale Modifier" );
		if( pMaxLodScaler )
		{
		    const float alpha = CSettingsManager::GetInstance().GetSettings().m_graphics.m_MaxLodScale;
		    pMaxLodScaler->Apply(keyframe, alpha);
		}
	}
#endif // RSG_PC
}

///////////////////////////////////////////////////////////////////////////////
//  ApplyGlobalModifiers
///////////////////////////////////////////////////////////////////////////////													

void			CTimeCycle::ApplyGlobalModifiers			(tcKeyframeUncompressed& keyframe, const bool finalUpdate REPLAY_ONLY(, const eTimeCyclePhase timeCyclePhase))
{
	tcAssert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
#if TC_DEBUG
	if (g_timeCycle.GetGameDebug().GetDisableGlobalModifiers()) 
	{
		return;
	}
#endif

	// initialise the data
	const tcModifier* modifier = NULL;
	float modStrength = 0.0f;

	CPed* playerPed = FindPlayerPed();

	CPlayerSpecialAbility *specAbility = playerPed ? playerPed->GetSpecialAbility() : NULL;
	if (specAbility)
	{
		specAbility->ApplyFx();
	}

#if GTA_REPLAY
	atHashString modifierScriptHash = m_scriptModifierIndex != -1 ? GetModifierKey(m_scriptModifierIndex) : "";
	atHashString modifierTransistionHash = m_transitionScriptModifierIndex != -1 ? GetModifierKey(m_transitionScriptModifierIndex) : "";

	bool bShouldDoScriptEffect = true;
	if( timeCyclePhase == TIME_CYCLE_PHASE_REPLAY_FREECAM && ShouldDisableScripFxForFreeCam(modifierScriptHash.GetHash(), modifierTransistionHash.GetHash()) )
	{
		bShouldDoScriptEffect = false;
	}
	else if( timeCyclePhase == TIME_CYCLE_PHASE_REPLAY_NORMAL && ShouldDisableScripFxForGameCam(modifierScriptHash.GetHash(), modifierTransistionHash.GetHash()) )
	{
		bShouldDoScriptEffect = false;
	}

	//Only apply script post fx if the effect isn't disabled in this phase
	if( bShouldDoScriptEffect )
#endif
	{

		// script modifier
		if (m_transitionScriptModifierIndex>=0 )
		{
			if( !fwTimer::IsGamePaused() )
				m_transitionStrength += fwTimer::GetTimeStep() * m_transitionSpeed;

			if( m_transitionStrength >= m_scriptModifierStrength )
			{
				// We're done.
				m_scriptModifierIndex = m_transitionScriptModifierIndex;
				m_transitionScriptModifierIndex = -1;
				modifier = GetModifier(m_scriptModifierIndex);
				modStrength = m_scriptModifierStrength;

#if TC_DEBUG
				if (g_timeCycle.GetGameDebug().GetDisplayModifierInfo())
				{
					grcDebugDraw::AddDebugOutput("Global Modifier: script - %s(%d) - %.3f",GetModifierName(m_scriptModifierIndex),FindModifierRecordIndex(m_scriptModifierIndex),modStrength);
				}
#endif
			}
			else
			{
				if (m_scriptModifierIndex>=0)
				{ // We're transitioning to another tc, so blend it in first.
					GetModifier(m_scriptModifierIndex)->Apply(keyframe, m_scriptModifierStrength - m_transitionStrength);
				}
				modifier = GetModifier(m_transitionScriptModifierIndex);
				modStrength = m_transitionStrength;
			
#if TC_DEBUG
				if (g_timeCycle.GetGameDebug().GetDisplayModifierInfo())
				{
					if( m_scriptModifierIndex > -1 && m_transitionScriptModifierIndex > -1 )
					{
						grcDebugDraw::AddDebugOutput("Global Modifier: script - %s(%d) - %s(%d) - %.3f",GetModifierName(m_scriptModifierIndex),FindModifierRecordIndex(m_scriptModifierIndex),GetModifierName(m_transitionScriptModifierIndex),FindModifierRecordIndex(m_transitionScriptModifierIndex),modStrength);
					}
					else if( m_scriptModifierIndex > -1 )
					{
						grcDebugDraw::AddDebugOutput("Global Modifier: script - %s(%d) - %s(%d) - %.3f",GetModifierName(m_scriptModifierIndex),FindModifierRecordIndex(m_scriptModifierIndex),"N/A",-1,modStrength);
					}
					else if( m_transitionScriptModifierIndex > -1 ) 
					{
						grcDebugDraw::AddDebugOutput("Global Modifier: script - %s(%d) - %s(%d) - %.3f","N/A",-1,GetModifierName(m_transitionScriptModifierIndex),FindModifierRecordIndex(m_transitionScriptModifierIndex),modStrength);
					}
				}
#endif
			
			}
		}
		else if (m_scriptModifierIndex>=0)
		{
			modifier = GetModifier(m_scriptModifierIndex);
			if( m_transitionOut )
			{
				if( !fwTimer::IsGamePaused() )
					m_transitionStrength -= fwTimer::GetTimeStep() * m_transitionSpeed;

				if( m_transitionStrength <= 0.0f )
				{
					m_transitionOut = false;
					m_transitionStrength = 0.0f;
					m_scriptModifierIndex = -1;
				}
			
				modStrength = m_transitionStrength;
			}
			else
			{
				modStrength = m_scriptModifierStrength;
			}
		
#if TC_DEBUG
			if (g_timeCycle.GetGameDebug().GetDisplayModifierInfo() && m_scriptModifierIndex != -1)
			{
				grcDebugDraw::AddDebugOutput("Global Modifier: script - %s(%d) - %.3f",GetModifierName(m_scriptModifierIndex),FindModifierRecordIndex(m_scriptModifierIndex),modStrength);
			}
#endif
		}
		else if(m_playerModifierIndexCurrent>=0)
		{
			if(m_playerModifierStrength >= 1.0f)
			{
				m_playerModifierIndexCurrent = m_playerModifierIndexNext;
				m_playerModifierStrength = 1.0f;

				if( m_playerModifierIndexCurrent>=0) // it has changed, so need a retest.
					modifier = GetModifier(m_playerModifierIndexCurrent);
				
				modStrength = m_playerModifierStrength;

			}
			else if( m_playerModifierStrength == 0.0f )
			{
				modifier = GetModifier(m_playerModifierIndexCurrent);
				modStrength = 1.0f;
			}
			else
			{
				GetModifier(m_playerModifierIndexCurrent)->Apply(keyframe, 1.0f - m_playerModifierStrength);
				if(m_playerModifierIndexNext>=0)
				{
					modifier = GetModifier(m_playerModifierIndexNext);
					modStrength = m_playerModifierStrength;
				}
			}
#if TC_DEBUG
			if (g_timeCycle.GetGameDebug().GetDisplayModifierInfo())
			{
				if (m_playerModifierIndexCurrent > -1 && m_playerModifierIndexNext > -1)
					grcDebugDraw::AddDebugOutput("Global Modifier: player transition - %s(%d) - %s(%d) %.3f",GetModifierName(m_playerModifierIndexCurrent),FindModifierRecordIndex(m_playerModifierIndexCurrent), GetModifierName(m_playerModifierIndexNext), FindModifierRecordIndex(m_playerModifierIndexNext), m_playerModifierStrength);
				else if (m_playerModifierIndexCurrent > -1)
					grcDebugDraw::AddDebugOutput("Global Modifier: player transition - %s(%d) - %s(%d) %.3f",GetModifierName(m_playerModifierIndexCurrent),FindModifierRecordIndex(m_playerModifierIndexCurrent), "N/A", -1, m_playerModifierStrength);	
				else if (m_playerModifierIndexNext > -1)
					grcDebugDraw::AddDebugOutput("Global Modifier: player transition - %s(%d) - %s(%d) %.3f","N/A", -1, GetModifierName(m_playerModifierIndexNext), FindModifierRecordIndex(m_playerModifierIndexNext), m_playerModifierStrength);
			}
#endif

		}

		// apply any modifier
		if (modifier)
		{	
			tcAssertf((modStrength >= 0.0f) && (modStrength <= 1.0f), "Applying %s with an outof bound strength of %f",GetModifierName(GetModifierIndex((tcModifier*)modifier)),modStrength);
			modifier->Apply(keyframe, modStrength);
		}
	}

#if (RSG_PC || RSG_DURANGO || RSG_ORBIS)
	bool bFirstPerson = camInterface::IsRenderingFirstPersonShooterCamera();
	if(!bFirstPerson)
	{
		// First person vehicle cam?
		const camBaseCamera* pDominantRenderedCamera = camInterface::GetDominantRenderedCamera();
		if(pDominantRenderedCamera && pDominantRenderedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
		{
			const camCinematicMountedCamera* pMountedCamera  = static_cast<const camCinematicMountedCamera*>(pDominantRenderedCamera);
			bFirstPerson = pMountedCamera->IsFirstPersonCamera();
		}
	}

	if( bFirstPerson )
	{
		const tcModifier* pNextGenModifier = FindModifier( atHashString("NG_first",0x3cbf7ebf), "NG_first" );
		if (pNextGenModifier)
		{
			pNextGenModifier->Apply( keyframe, m_vsNextGenModifer );
		}
	}
	else
	{
		const tcModifier* pNextGenModifier = FindModifier( atHashString("nextgen",0x254E1B2C), "NextGen" );
		if (pNextGenModifier)
		{
			pNextGenModifier->Apply( keyframe, m_vsNextGenModifer );
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// inside submarine
	CVehicle * vehicle = CGameWorld::FindLocalPlayerVehicle(); 	
	bool bPlayeIsLeavingVehicle = CGameWorld::FindLocalPlayer() && CGameWorld::FindLocalPlayer()->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle);


	if(bFirstPerson && vehicle && vehicle->InheritsFromSubmarine() && bPlayeIsLeavingVehicle == false)
	{
		const tcModifier* subModifier = FindModifier( atHashString("vehicle_subint",0xa675f3bf), "vehicle_subint" );
		if(subModifier)
		{
			subModifier->Apply(keyframe, 1.0f);
		}
	}
#endif
	
	//////////////////////////////////////////////////////////////////////////
	// first person eyewear mod
#if FPS_MODE_SUPPORTED
	if (CPedVariationStream::HasFirstPersonTimeCycleModifier())
	{
		const tcModifier* pModifier = FindModifier(CPedVariationStream::GetFirstPersonTimeCycleModifierName(), "FirstPersonEyewear");
		if (pModifier)
		{
			pModifier->Apply( keyframe, CPedVariationStream::GetFirstPersonTimeCycleModifierIntensity() );
		}
	}
#endif // FPS_MODE_SUPPORTED

	//////////////////////////////////////////////////////////////////////////
	// sniper sight

	REPLAY_ONLY( if( timeCyclePhase != TIME_CYCLE_PHASE_REPLAY_FREECAM ) )
	{	
		if (CNewHud::IsSniperSightActive())
		{
			const tcModifier* pSniperSightModifier = FindModifier( atHashString("sniper",0x94638209), "SniperSight" );
			if (pSniperSightModifier)
			{
				pSniperSightModifier->Apply( keyframe, PostFX::GetSniperScopeOverrideStrength() );
			}
		}
		else if (CVfxHelper::IsPlayerInAccurateModeWithScope())
		{
			const tcModifier* pScopeZoomInModifier = FindModifier( atHashString("scope_zoom_in",0x3B8158DA), "ScopeZoomIn" );
			if (pScopeZoomInModifier)
			{
				pScopeZoomInModifier->Apply( keyframe, 1.0f );
			}
		}
	}
	
	REPLAY_ONLY( if( timeCyclePhase != TIME_CYCLE_PHASE_REPLAY_FREECAM ) )
	{
#if __BANK
		if (PostFX::GetGlobalUseNightVision() != PostFX::GetGlobalOverrideNightVision())
#else
		if (PostFX::GetGlobalUseNightVision())
#endif
		{
			const tcModifier* pNVModifier = FindModifier( atHashString("v_dark",0x50E7A950), "v_dark" );
			if (pNVModifier)
			{
				pNVModifier->Apply( keyframe, 1.0f );
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// inside vehicle mode
	if (CVehicle::IsInsideSpecificVehicleModeEnabled(MI_PLANE_CARGOPLANE.GetName().GetHash()) && finalUpdate)
	{
		const tcModifier* pInsidePlaneModifier = FindModifier( atHashString("plane_inside_mode", 0x2c70810f), "Inside Plane Mode" );
		if (pInsidePlaneModifier)
		{
			pInsidePlaneModifier->Apply( keyframe, 1.0f);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// default flash effect triggered by camera code
	const float fDefaultFlashEffectLevel = PostFX::GetFlashEffectFadeLevel();
	if (fDefaultFlashEffectLevel != 0.0f)
	{
		atHashString flashEffectModName = PostFX::GetFlashEffectModName();
		const tcModifier* pFlashEffectModifier = FindModifier( flashEffectModName, "DefaultFlash" );
		if (pFlashEffectModifier)
		{
			pFlashEffectModifier->Apply( keyframe, fDefaultFlashEffectLevel );
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// pause menu modifiers
	REPLAY_ONLY( if( timeCyclePhase == TIME_CYCLE_PHASE_GAME ) )
	{
		PAUSEMENUPOSTFXMGR.ApplyModifiers(keyframe);
	}

	//////////////////////////////////////////////////////////////////////////
	//
	ANIMPOSTFXMGR.ApplyModifiers(keyframe REPLAY_ONLY(, timeCyclePhase ));

	//////////////////////////////////////////////////////////////////////////
	// bonus modifiers: overrides all the things.
	if( m_extraModifierIndex != -1 )
	{
		const tcModifier* extraModifier = GetModifier(m_extraModifierIndex);
		extraModifier->Apply( keyframe, 1.0f);
#if __BANK
		grcDebugDraw::AddDebugOutput("Extra Modifier: %s(%d) %.3f",GetModifierName(m_extraModifierIndex),FindModifierRecordIndex(m_extraModifierIndex), 1.0f);
#endif
	}
}


#if GTA_REPLAY	
// There are some TC vars we don't want to interpolate, just have them as constant values.
static void PatchReplayFilterConstantTCVars(const tcModifier* pModifier, tcKeyframeUncompressed& keyframe, float intensity)
{
	if (pModifier == NULL || intensity < SMALL_FLOAT)
		return;

	for (int j = 0; j < pModifier->GetModDataCount(); j++)
	{
		const int varId = pModifier->GetModDataArray()[j].varId;
		const float valA = (pModifier->GetModDataArray()[j].valA);

		if (varId == TCVAR_POSTFX_SCANLINE_FREQUENCY_0)
		{
			keyframe.SetVar(TCVAR_POSTFX_SCANLINE_FREQUENCY_0, valA);
		}
		else if (varId == TCVAR_POSTFX_SCANLINE_FREQUENCY_1)
		{
			keyframe.SetVar(TCVAR_POSTFX_SCANLINE_FREQUENCY_1, valA);
		} 
		else if (varId == TCVAR_POSTFX_SCANLINE_SPEED)
		{
			keyframe.SetVar(TCVAR_POSTFX_SCANLINE_SPEED, valA);
		}
		else if (varId == TCVAR_POSTFX_NOISE_SIZE)
		{
			keyframe.SetVar(TCVAR_POSTFX_NOISE_SIZE, valA);
		}
	}

}

void	CTimeCycle::ApplyReplayEditorModifiers(tcKeyframeUncompressed& keyframe)
{
	//////////////////////////////////////////////////////////////////////////
	// Replay modifiers: overrides all the things, should appear last
	if (m_ReplayModifierHash > 0)
	{
		s32 replayModId =  FindModifierIndex(m_ReplayModifierHash,"ReplayModifierHash");
		tcAssertf( replayModId >= 0 && replayModId < m_modifiersArray.GetCount(), "CTimeCycle::CalcAmbientScale - Unknown modifier %u", m_ReplayModifierHash  );
		const tcModifier* pModifier = GetModifier(replayModId);

		pModifier->Apply( keyframe, m_ReplayModifierIntensity);
		
		PatchReplayFilterConstantTCVars(pModifier, keyframe, m_ReplayModifierIntensity);

#if __BANK
		grcDebugDraw::AddDebugOutput("Replay Modifier: %s(%d) %.3f",GetModifierName(replayModId),FindModifierRecordIndex(replayModId), 1.0f);
#endif
	}
}
#endif //GTA_REPLAY

#if GTA_REPLAY	
void CTimeCycle::ApplyModifierWithNegativeRange(tcKeyframeUncompressed& keyframe, const tcModifier *negMod, const tcModifier *posMod, float alpha, bool linearize)
{
	float rawValue = alpha;
	
	float value = linearize ? sqrtf(abs(rawValue)) : abs(rawValue);
	if( rawValue < 0.0f )
	{
		negMod->Apply(keyframe, value);
	}
	else if( rawValue > 0.0f )
	{
		posMod->Apply(keyframe, value);
	}
	
}

void	CTimeCycle::ApplyReplayExtraEffects(tcKeyframeUncompressed& keyframe)
{
	const tcModifier* pReplaySaturationModNeg = FindModifier( atHashString("rply_saturation_neg",   0x53e8448c), "Replay Extra Effects" );
	const tcModifier* pReplaySaturationModPos = FindModifier( atHashString("rply_saturation",       0x85bf028a), "Replay Extra Effects" );
	const tcModifier* pReplayContrastModNeg = FindModifier( atHashString("rply_contrast_neg",       0xddd00563), "Replay Extra Effects" );
	const tcModifier* pReplayContrastModPos = FindModifier( atHashString("rply_contrast",           0xaffd7204), "Replay Extra Effects" );
	const tcModifier* pReplayVignetteModNeg = FindModifier( atHashString("rply_vignette_neg",       0x8813365e), "Replay Extra Effects" );
	const tcModifier* pReplayVignetteModPos = FindModifier( atHashString("rply_vignette",           0x4f6992f5), "Replay Extra Effects" );
	const tcModifier* pReplayBrightnessModNeg = FindModifier( atHashString("rply_brightness_neg",   0x33ede908), "Replay Extra Effects" );
	const tcModifier* pReplayBrightnessModPos = FindModifier( atHashString("rply_brightness",       0x4685f55f), "Replay Extra Effects" );


	if (pReplaySaturationModNeg && pReplaySaturationModPos)
	{
		ApplyModifierWithNegativeRange(keyframe, pReplaySaturationModNeg, pReplaySaturationModPos, GetReplaySaturationIntensity(), false);
	}

	if (pReplayContrastModNeg && pReplayContrastModPos)
	{
		ApplyModifierWithNegativeRange(keyframe, pReplayContrastModNeg, pReplayContrastModPos, GetReplayContrastIntensity(), true);
	}

	if (pReplayVignetteModNeg && pReplayVignetteModPos)
	{
		ApplyModifierWithNegativeRange(keyframe, pReplayVignetteModNeg, pReplayVignetteModPos, GetReplayVignetteIntensity(), false);
	}

	if( pReplayBrightnessModNeg && pReplayBrightnessModPos )
	{
		float brightness = GetReplayBrightnessIntensity();

		static float prevBrightness = 0.0f;
		bool valueChanged = prevBrightness != brightness;
		
		prevBrightness = brightness;
		
		ApplyModifierWithNegativeRange(keyframe, pReplayBrightnessModNeg, pReplayBrightnessModPos, brightness, true);
		
		if( valueChanged )
		{
			PostFX::ResetAdaptedLuminance();
		}
	}
}
#endif //GTA_REPLAY

/////////////////////////////////////////////////////////////////////////////// 
//  CalcDirectionalLightandPos
///////////////////////////////////////////////////////////////////////////////													

void			CTimeCycle::CalcDirectionalLightandPos		(Vec3V_InOut vDirectionalDir, float &directionalInten, Vec3V_InOut vSunDir, Vec3V_InOut vMoonDir, float hours, float sunSlope, float moonSlope, float sunYaw, float &sunFade, float &moonFade)
{
	// scale angle so that sun rises at 06:00 and sets at 20:00
	float angle;
	if (hours < SUN_RISE_TIME)
	{
		// blend between 0.0 and 0.5*PI
		angle = 0.5f*PI * (hours/SUN_RISE_TIME);
	}
	else if (hours < SUN_SET_TIME)
	{
		// blend between 0.5*PI and 1.5*PI
		float timeSinceSunRise = hours-SUN_RISE_TIME;
		angle = (0.5f*PI) + (PI*(timeSinceSunRise/SUN_RISE_TO_SET_TIME));
	}
	else
	{
		// blend between 1.5*PI and 2.0*PI
		float timeSinceSunSet = hours-SUN_SET_TIME;
		angle = (1.5f*PI) + (0.5f*PI * (timeSinceSunSet/SUN_SET_TO_MIDNIGHT_TIME));
	}

	const double cycleTime = m_frameInfo.m_cycleTime;

	const tcKeyframeUncompressed &keyframe = m_frameInfo.m_keyframe;
	const float lightDirOverride = keyframe.GetVar(TCVAR_LIGHT_DIRECTION_OVERRIDE);
	const float sunPosOverride = keyframe.GetVar(TCVAR_LIGHT_DIRECTION_OVERRIDE_OVERRIDES_SUN);
	
	// Calculate slopes
	const Vec3V vSunSlopVec(0.0f, rage::Cosf(sunSlope), rage::Sinf(sunSlope));
	const Vec3V vMoonSlopVec(0.0f, rage::Cosf(moonSlope), rage::Sinf(moonSlope));

	// Adjust moon angle so that it rotates faster than the sun
	float moonAngle = angle + PI * float((fmod(cycleTime, 55.0) - 27.0) / 27.0);
	const Vec3V vMoonAxisAdjust =  Vec3V(V_X_AXIS_WZERO);

	// Main axis
	Vec3V vActualMoonDir = (vMoonSlopVec    * ScalarVFromF32(-rage::Cosf(moonAngle))) + 
						   (vMoonAxisAdjust * ScalarVFromF32( rage::Sinf(moonAngle)));
	
	// Extra adjust for moon
	const float moonCyclePos = (float)fmod(cycleTime, 27.0);
	const float adjustAmount = 
		m_frameInfo.m_moonWobbleOffset + 
		rage::Sinf(moonCyclePos * 2.0f * PI * m_frameInfo.m_moonWobbleFrequency) * m_frameInfo.m_moonWobbleAmplitude;
	const Vec3V testAxis = Cross(vMoonAxisAdjust, vMoonSlopVec);
	vActualMoonDir += (testAxis * ScalarVFromF32(adjustAmount));

	vActualMoonDir = Normalize(vActualMoonDir);

	const Vec3V vOverMoonDir(keyframe.GetVar(TCVAR_MOON_DIRECTION_X),
							 keyframe.GetVar(TCVAR_MOON_DIRECTION_Y),
							 keyframe.GetVar(TCVAR_MOON_DIRECTION_Z));

	// Adjust for yaw for sun
	const float cosYaw = rage::Cosf(sunYaw);
	const float sinYaw = rage::Sinf(sunYaw);

	vSunDir = (vSunSlopVec * ScalarVFromF32(-rage::Cosf(angle))) + Vec3V(V_X_AXIS_WZERO) * ScalarVFromF32(rage::Sinf(angle)),
	vSunDir = Normalize(vSunDir);
	
	Vec3V vActualSunDir = vSunDir * Vec3V(cosYaw, cosYaw, 1.0f);
	vActualSunDir += Vec3V(vSunDir.GetY(), vSunDir.GetX(), ScalarV(V_ZERO)) * Vec3V(-sinYaw, sinYaw, 0.0f);
	const Vec3V vOverSunDir(keyframe.GetVar(TCVAR_SUN_DIRECTION_X),
							keyframe.GetVar(TCVAR_SUN_DIRECTION_Y),
							keyframe.GetVar(TCVAR_SUN_DIRECTION_Z));

	// Set directional light (moon or sun) and intensity
	directionalInten = 1.0f;

	sunFade = 1.0f;
	moonFade = 0.0f;

	if (hours > SUN_SET_TIME)
	{
		sunFade = 1.0f - Saturate((hours - SUN_MOON_TIME) * 4.0f);
		moonFade = Saturate((hours - SUN_MOON_TIME - 0.25f) * 4.0f);
	}
	else if (hours < SUN_RISE_TIME)
	{ 
		sunFade = Saturate((hours - MOON_SUN_TIME - 0.25f) * 4.0f);
		moonFade = 1.0f - Saturate((hours - MOON_SUN_TIME) * 4.0f);
	}
	
	moonFade *= Saturate(vMoonDir.GetZf() / 0.1f);

	if (Water::IsCameraUnderwater())
	{
		// override when underwater - blend towards the underwater light direction
		float underwaterDepth	= Water::GetWaterLevel() - camInterface::GetPos().z;
		float start				= Water::GetMainWaterTunings().m_GodRaysLerpStart;
		float end				= Water::GetMainWaterTunings().m_GodRaysLerpEnd;
		if (underwaterDepth > start)
		{
			float underWaterRatio = CVfxHelper::GetInterpValue(underwaterDepth, start, end);
			vActualMoonDir = vActualMoonDir + ScalarVFromF32(underWaterRatio) * (SUN_LIGHT_UNDERWATER - vActualMoonDir);
			vActualMoonDir = Normalize(vActualMoonDir);
			vActualSunDir = vActualSunDir + ScalarVFromF32(underWaterRatio) * (SUN_LIGHT_UNDERWATER - vActualSunDir);
			vActualSunDir = Normalize(vActualSunDir);
		}
	}

	float totalFade = 0.0f;

	// Blend sun and moon direction
	totalFade = (sunFade * 100.0f) + moonFade;

	vMoonDir = Normalize(Lerp(ScalarVFromF32(lightDirOverride * sunPosOverride), vActualMoonDir, vOverMoonDir));
	vSunDir = Normalize(Lerp(ScalarVFromF32(lightDirOverride * sunPosOverride), vActualSunDir, vOverSunDir));

	// Keep numbers stable
	if ( totalFade > FLT_EPSILON ) {
		ScalarV scSunFade = ScalarVFromF32(sunFade * 100.0f / totalFade);
		ScalarV scMoonFade = ScalarVFromF32(moonFade / totalFade);

		Vec3V vActualDirectionalDir = vActualSunDir * scSunFade + vActualMoonDir * scMoonFade;
		Vec3V vOverDirectionalDir = vOverSunDir * scSunFade + vOverMoonDir * scMoonFade;
		
		vDirectionalDir = Lerp(ScalarVFromF32(lightDirOverride), vActualDirectionalDir, vOverDirectionalDir);
		vDirectionalDir = Normalize(vDirectionalDir);
	} else {
		// Zero the vector, it'll get tweaked below
		vDirectionalDir.ZeroComponents();
	}

	// Set intensity-
	directionalInten = Max(sunFade, moonFade);

	// Moon adjust based on phase
	float normalisedLunearCycle = moonCyclePos / 26.0f;
	float cycleOverride = m_frameInfo.m_moonCycleOverride;
	if (cycleOverride >= 0.0f) { normalisedLunearCycle = cycleOverride; }

	float dimAmount = (abs(normalisedLunearCycle - 0.5f) * 2.0f) * moonFade;
	directionalInten = Lerp(dimAmount, directionalInten, directionalInten * m_vsMoonDimMult);

	// clamp to prevent big shadows	
	float directionClamp = m_longShadowsEnabled ? 0.4f : 0.33f;
	vDirectionalDir.SetZf(Max(directionClamp, vDirectionalDir.GetZf()));// clamp to around 5pm time
	vDirectionalDir = Normalize(vDirectionalDir);

	// apply shadow freezing rdr2 tech
	const camBaseCamera* activeCamera = camInterface::GetDominantRenderedCamera();
	if (activeCamera)
	{
		const grcViewport *viewport = gVpMan.GetUpdateGameGrcViewport();
		Vec3V camPos = viewport->GetCameraMtx().d();
		Vector3 playerPos = CGameWorld::FindLocalPlayerCoors();
		float heading= activeCamera->GetFrame().ComputeHeading();
		bool cameraCut=((camInterface::GetFrame().GetFlags() & (camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation)) > 0);
		bool playerActive = activeCamera->GetAttachParent() != NULL;
		bool cutScenePlaying = camInterface::GetCutsceneDirector().IsCutScenePlaying() BANK_ONLY( && !CutSceneManager::GetInstance()->IsPaused() );

		if (!cutScenePlaying)
		{
			if ( camInterface::GetScriptDirector().IsRendering() )
					cutScenePlaying = true;		
		}
		bool isCutscene = cutScenePlaying || (CTheScripts::GetNumberOfMiniGamesInProgress() >0);
		isCutscene = isCutscene || CPauseMenu::IsActive( PM_EssentialAssets );// so shadows don't move in pause menu

		cameraCut = cameraCut || (lightDirOverride > 0.0f); // disable shadow freezor if light override is active.

		Vec3V frozenDir = ShadowFreezer.Update( VEC3V_TO_VECTOR3(vDirectionalDir), 
			VEC3V_TO_VECTOR3(camPos), 
			cameraCut,
			heading, 
			playerPos,
			playerActive,
			isCutscene
			);
	
		vDirectionalDir = Lerp(ScalarVFromF32(lightDirOverride), frozenDir, vDirectionalDir);
	}
	
#if PLAYERSWITCH_FORCES_LIGHTDIR
	// Apply code dir light override;
	vDirectionalDir = Normalize(Lerp(ScalarVFromF32(m_codeOverrideDirLightOrientationStrength), vDirectionalDir, m_codeOverrideDirLightOrientation));
#endif // PLAYERSWITCH_FORCES_LIGHTDIR
}


/////////////////////////////////////////////////////////////////////////////// 
//  CalcSunLightDir
///////////////////////////////////////////////////////////////////////////////													

void			CTimeCycle::CalcSunLightDir					()
{
#if TC_DEBUG
	if( m_gameDebug.GetRenderSunPosition() || m_gameDebug.GetDumpSunMoonPosition())
	{
		Vector3 playerPos = CGameWorld::FindLocalPlayerCoors();
		Vec3V vPlayerPos = RCC_VEC3V(playerPos);

		int mult = m_gameDebug.GetHourDivider();

		if ((m_gameDebug.GetDumpSunMoonPosition()))
		{
			Displayf("Hour (h/24.0) : Sun Dir X : Sun Dir Y : Sun Dir Z : Intensity Mult");
		}
		
		for(int i=0;i<24*mult;i++)
		{
			Vec3V vSunLightDir;
			Vec3V vSunSpritePos;
			Vec3V vMoonSpritePos;
			
			const int minutes = i % mult;

			const float hours = ((i - minutes) / mult) + (minutes / float(mult));
			const float sunSlope = m_frameInfo.m_sunRollAngle;
			const float moonSlope = m_frameInfo.m_sunRollAngle + m_frameInfo.m_moonRollOffset;
			const float sunYaw = m_frameInfo.m_sunYawAngle;
			float inten;
			float sunFade , moonFade;

			CalcDirectionalLightandPos(
				vSunLightDir, 
				inten, 
				vSunSpritePos, 
				vMoonSpritePos, 
				hours, 
				sunSlope, 
				moonSlope, 
				sunYaw, 
				sunFade, 
				moonFade);

			if (m_gameDebug.GetRenderSunPosition())
			{
				// blend in the moon light
				Vec3V vStartPos, vEndPos;

				vStartPos = vPlayerPos;
				vEndPos = vPlayerPos+vMoonSpritePos * ScalarVFromF32(20.0f);
				grcDebugDraw::Line(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos), Color32(0x00,0x00,0x00), Color32(0x00,0xff,0xff));

				vStartPos = vPlayerPos;
				vEndPos = vPlayerPos+vSunSpritePos * ScalarVFromF32(20.0f);
				grcDebugDraw::Line(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos), Color32(0x00,0x00,0x00), Color32(0xff,0xff,0x00));
			}

			if (m_gameDebug.GetDumpSunMoonPosition())
			{
				Displayf("%.6f : %.6f : %.6f : %.6f : %.6f", 
					hours, 
					vSunLightDir.GetXf(),
					vSunLightDir.GetYf(),
					vSunLightDir.GetZf(),
					inten
					);
			}
		}

		if (m_gameDebug.GetDumpSunMoonPosition()) 
		{
			m_gameDebug.SetDumpSunMoonPosition(false);
		}
	}
#endif // __BANK

	// get the time to use 
	Time timeCycleTime(CClock::GetHour(), CClock::GetMinute(), CClock::GetSecond());
#if TC_DEBUG
	if (m_debug.GetKeyframeEditActive() && m_debug.GetKeyframeEditOverrideLightDirection())
	{
		const TimeSample_s& timeSample = CClock::GetTimeSampleInfo(m_debug.GetKeyframeEditTimeSampleIndex());
		timeCycleTime.Set(timeSample.hour, 0, 0);
	}
#endif

	if (CClock::GetMode()==CLOCK_MODE_FIXED)
	{
		// in fixed mode we may want to offset the time a little so that the sun position can change
		timeCycleTime.Add(0, static_cast<s32>(m_frameInfo.m_keyframe.GetVar(TCVAR_TIME_OFFSET)), 0);
	}

	// scale angle so that sun rises at 06:00 and sets at 20:00
	const float hours = CClock::GetDayRatio(timeCycleTime) * 24.0f;
	const float sunSlope = m_frameInfo.m_sunRollAngle;
	const float moonSlope = m_frameInfo.m_sunRollAngle + m_frameInfo.m_moonRollOffset;
	const float sunYaw = m_frameInfo.m_sunYawAngle;
	
	CalcDirectionalLightandPos(m_frameInfo.m_vDirectionalLightDir, 
							   m_frameInfo.m_directionalLightIntensity,
							   m_frameInfo.m_vSunSpriteDir, 
							   m_frameInfo.m_vMoonSpriteDir, 
							   hours, 
							   sunSlope, 
							   moonSlope, 
							   sunYaw,
							   m_frameInfo.m_sunFade,
							   m_frameInfo.m_moonFade);
}

///////////////////////////////////////////////////////////////////////////////
//  CalcModifierAmbientScale
///////////////////////////////////////////////////////////////////////////////													

bool	CTimeCycle::CalcModifierAmbientScale(const int timeCycleIndex, const int secondaryTimeCycleIndex, float blend, float &natAmbientScale, float &artAmbientScale ASSERT_ONLY(, const atHashString /*timeCycleName*/, const atHashString secondaryTimeCycleName))
{
	const tcModifier *pMod = GetModifier(timeCycleIndex);
	const tcModifier *pModSecondary = NULL;
	if( secondaryTimeCycleIndex >=0 )
	{
		pModSecondary = GetModifier(secondaryTimeCycleIndex);
	}
#if __ASSERT
	// commented out due to false positives with TC mods added by script with ADD_TCMODIFIER_OVERRIDE();
	// see: B*3666677 - Error: pMod == FindModifier(timeCycleName,"AmbScale"): was looking for modifier mp_gr_int01_black, found mp_gr_int01_black
	//Assertf(pMod == FindModifier(timeCycleName,"AmbScale"),"was looking for modifier %s, found %s (%p,%d)",timeCycleName.GetCStr(),GetModifierName(timeCycleIndex),pMod,timeCycleIndex);

	Assert(pModSecondary == NULL || pModSecondary == FindModifier(secondaryTimeCycleName,"AmbScale"));
#endif

	bool validNatAmb = true;
	bool validArtAmb = true;
	Vec2V vModifierAmbientScale = GetModifierAmbientScale(pMod,validNatAmb,validArtAmb);
	float natAmbient = vModifierAmbientScale.GetXf();
	float artAmbient = vModifierAmbientScale.GetYf();

	if( pModSecondary )
	{
		bool secValidNatAmb = true;
		bool secValidArtAmb = true;
		Vec2V vSecondaryAmbientScale = GetModifierAmbientScale(pModSecondary,secValidNatAmb,secValidArtAmb);
		if( secValidNatAmb )
		{
			natAmbient = vSecondaryAmbientScale.GetXf();
			validNatAmb = secValidNatAmb;
		}
		if( secValidArtAmb )
		{
			artAmbient = vSecondaryAmbientScale.GetYf();
			validArtAmb = secValidArtAmb;
		}
	}

	if( validNatAmb )
	{
		natAmbientScale=(1.0f-blend)+natAmbient*blend;
	}

	if( validArtAmb )
	{
		artAmbientScale=(1.0f-blend)+artAmbient*blend;
	}
	
	return pMod->GetUserFlag(tcflag_expandAmbientLightBox) || (pModSecondary && pModSecondary->GetUserFlag(tcflag_expandAmbientLightBox));
}

Vec2V_Out			CTimeCycle::GetModifierAmbientScale(const tcModifier *mod, bool &validNat, bool &validArt)
{
	Vec2V result;
	mod->CreateAndLockVarMap();

	// Natural Ambient
	float natAmb;
	const tcModData* natAmbModData = mod->GetModDataUnsafe(TCVAR_NATURAL_AMBIENT_MULTIPLIER);
	validNat = (natAmbModData != NULL);
	if( natAmbModData )
	{
		natAmb = natAmbModData->valA;
	}

	// Artificial Ambient
	float artAmb;
	const tcModData* artAmbModData = mod->GetModDataUnsafe(TCVAR_ARTIFICIAL_INT_AMBIENT_MULTIPLIER);
	validArt = (artAmbModData != NULL);
	if( artAmbModData )
	{
		artAmb = artAmbModData->valA;
	}

	result = Vec2V(natAmb,artAmb);

	mod->UnlockVarMap();

	return result;
}

Vec2V_Out CTimeCycle::GetModifierAmbientScaleWithDefault (const tcModifier *mod)
{
	bool validNat = false;
	bool validArt = false;
	Vec2V result = GetModifierAmbientScale(mod, validNat, validArt);
	if( validNat == false )
	{
		result.SetXf(m_frameInfo.m_keyframe.GetVar(TCVAR_NATURAL_AMBIENT_MULTIPLIER));
	}
	
	if( validArt == false )
	{
		result.SetYf(m_frameInfo.m_keyframe.GetVar(TCVAR_ARTIFICIAL_INT_AMBIENT_MULTIPLIER));
	}
	
	return result;
}

void CTimeCycle::SetInteriorCache(int mainTcIdx, int secondaryTcIdx)
{
	const tcModifier* currModifier = GetModifier(mainTcIdx);
	currModifier->CreateAndLockVarMap();
			
	float int_down_col_r = 0.0f;
	float int_down_col_g = 0.0f;
	float int_down_col_b = 0.0f;
	float int_down_intensity = 0.0f;

	float int_up_col_r = 0.0f;	 
	float int_up_col_g = 0.0f;
	float int_up_col_b = 0.0f;
	float int_up_intensity = 0.0f;

	int_down_col_r = currModifier->GetModDataValAUnsafe(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_R);
	int_down_col_g = currModifier->GetModDataValAUnsafe(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_G);
	int_down_col_b = currModifier->GetModDataValAUnsafe(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_B);
	int_down_intensity = currModifier->GetModDataValAUnsafe(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_INTENSITY);
																			
	int_up_col_r = currModifier->GetModDataValAUnsafe(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_COL_R);
	int_up_col_g = currModifier->GetModDataValAUnsafe(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_COL_G);
	int_up_col_b = currModifier->GetModDataValAUnsafe(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_COL_B);
	int_up_intensity = currModifier->GetModDataValAUnsafe(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_INTENSITY);
			
	currModifier->UnlockVarMap();
			
	if(secondaryTcIdx > -1)
	{
		const tcModifier* secondaryModifier = GetModifier(secondaryTcIdx);
		secondaryModifier->CreateAndLockVarMap();
				
		const tcModData* pModData;
				
		pModData = secondaryModifier->GetModDataUnsafe(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_R);
		if( pModData )
			int_down_col_r = pModData->valA;
		pModData = secondaryModifier->GetModDataUnsafe(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_G);
		if( pModData )
			int_down_col_g = pModData->valA;
		pModData = secondaryModifier->GetModDataUnsafe(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_B);
		if( pModData )
			int_down_col_b = pModData->valA;
		pModData = secondaryModifier->GetModDataUnsafe(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_INTENSITY);
		if( pModData )
			int_down_intensity = pModData->valA;
																				
		pModData = secondaryModifier->GetModDataUnsafe(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_COL_R);
		if( pModData )
			int_up_col_r = pModData->valA;
		pModData = secondaryModifier->GetModDataUnsafe(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_COL_G);
		if( pModData )
			int_up_col_g = pModData->valA;
		pModData = secondaryModifier->GetModDataUnsafe(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_COL_B);
		if( pModData )
			int_up_col_b = pModData->valA;
		pModData = secondaryModifier->GetModDataUnsafe(TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_INTENSITY);
		if( pModData )
			int_up_intensity = pModData->valA;
					
		secondaryModifier->UnlockVarMap();
	}

	m_interiorInitialised = true;
			
	m_interior_down_color_r = int_down_col_r;
	m_interior_down_color_g = int_down_col_g;
	m_interior_down_color_b = int_down_col_b;
	m_interior_down_intensity = int_down_intensity;

	m_interior_up_color_r = int_up_col_r;
	m_interior_up_color_g = int_up_col_g;
	m_interior_up_color_b = int_up_col_b;
	m_interior_up_intensity = int_up_intensity;

	m_interior_main_idx = mainTcIdx;
	m_interior_secondary_idx = secondaryTcIdx;
}

void CTimeCycle::AddLocalPortal(const fwPortalCorners &corners, float area, float doorOcclusion, int mainModifierIdx, int secondaryModifierIdx)
{
	for(int i=0;i<m_frameInfo.m_LocalPortalCache.GetCount();i++)
	{
		if( m_frameInfo.m_LocalPortalCache[i].GetInUse() )
		{
			if( IsEqualAll(m_frameInfo.m_LocalPortalCache[i].GetCorners().GetCornerV(0), corners.GetCornerV(0)) )
			{ // Let's pretend checking one corner is enough to cover our ass...
				
				// Update the area and skip
				m_frameInfo.m_LocalPortalCache[i].SetScreenArea(area);
				m_frameInfo.m_LocalPortalCache[i].SetDoorOcclusion(doorOcclusion);
#if __BANK
				const tcModifier* currModifier = GetModifier(mainModifierIdx);
				tcAssert(currModifier);
				m_frameInfo.m_LocalPortalCache[i].SetExpandBox(currModifier->GetUserFlag(tcflag_expandAmbientLightBox));
#endif				
				return;
			}

		}
	}

	portalData *portal = NULL;
	
	if( !m_frameInfo.m_LocalPortalCache.IsFull() )
	{
		portalData &newP = m_frameInfo.m_LocalPortalCache.Append();
		portal = &newP;
	}
	else 
	{ // We're full, let's find another one.
		for(int i=0;i<m_frameInfo.m_LocalPortalCache.GetCount();i++)
		{
			if( m_frameInfo.m_LocalPortalCache[i].GetScreenArea() < area )
			{
				portal = &m_frameInfo.m_LocalPortalCache[i];
			}
		}
	}
		
	if( portal )
	{
		const tcModifier* currModifier = GetModifier(mainModifierIdx);
		tcAssert(currModifier);

		portal->SetCorners(corners);
		portal->SetScreenArea(area);
		portal->SetDoorOcclusion(doorOcclusion);
		portal->SetMainModifierIdx(mainModifierIdx);
		portal->SetSecondaryModifierIdx(secondaryModifierIdx);
		portal->SetInUse(true);
		portal->SetExpandBox(currModifier->GetUserFlag(tcflag_expandAmbientLightBox));
		
	}
}

void CTimeCycle::PreSetInteriorAmbientCache(const atHashString name)
{
	int modIdx = FindModifierIndex(name,"PreSetInteriorAmbientCache");
	if( modIdx > -1 )
	{
		m_interiorApply = true;
		SetInteriorCache(modIdx,-1);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetTransitionToScriptModifierId
///////////////////////////////////////////////////////////////////////////////													

void				CTimeCycle::PushScriptModifier()
{
	tcAssertf(m_pushedScriptModifier == false,"trying to push a script modifier state while another has already been pushed");
			
	m_pushedScriptModifierIndex = m_scriptModifierIndex;
	m_pushedTransitionScriptModifierIndex = m_transitionScriptModifierIndex;
	m_pushedTransitionStrength = m_transitionStrength;
	m_pushedTransitionSpeed = m_transitionSpeed;
	m_pushedTransitionOut = m_transitionOut;
	m_pushedScriptModifier = true;
}

void				CTimeCycle::ClearPushedScriptModifier()
{
	m_pushedScriptModifierIndex = -1;
	m_pushedTransitionScriptModifierIndex = -1;
	m_pushedTransitionStrength = 1.0f;
	m_pushedTransitionSpeed = 0.0f;
	m_pushedTransitionOut = false;

}

///////////////////////////////////////////////////////////////////////////////
//  SetTransitionToScriptModifierId
///////////////////////////////////////////////////////////////////////////////													
void				CTimeCycle::PopScriptModifier()
{
	tcAssertf(m_pushedScriptModifier == true,"trying to pop a script modifier state while none has been pushed");

    m_scriptModifierIndex = m_pushedScriptModifierIndex;
    m_transitionScriptModifierIndex = m_pushedTransitionScriptModifierIndex;
    m_transitionStrength = m_pushedTransitionStrength;
    m_transitionSpeed = m_pushedTransitionSpeed;
    m_transitionOut = m_pushedTransitionOut;
    m_pushedScriptModifier = false;
}

///////////////////////////////////////////////////////////////////////////////
//  SetTransitionToScriptModifierId
///////////////////////////////////////////////////////////////////////////////													

void				CTimeCycle::SetTransitionToScriptModifierId(const atHashString name, float time)
{
	tcAssertf(m_transitionOut == false,"trying to transition to a script modifier while transitioning out");
	m_transitionScriptModifierIndex = FindModifierIndex(name,"SetTransitionToScriptModifierId");
	m_transitionStrength = 0.0f;
	m_transitionSpeed = m_scriptModifierStrength / time;
}

///////////////////////////////////////////////////////////////////////////////
//  SetTransitionToScriptModifierId
///////////////////////////////////////////////////////////////////////////////													

void				CTimeCycle::SetTransitionToScriptModifierId(int index, float time)
{
	tcAssertf(m_transitionOut == false,"trying to transition to a script modifier while transitioning out");
	m_transitionScriptModifierIndex = index;
	m_transitionStrength = 0.0f;
	m_transitionSpeed = m_scriptModifierStrength / time;
}

///////////////////////////////////////////////////////////////////////////////
//  SetTransitionOutOfScriptModifierId
///////////////////////////////////////////////////////////////////////////////													

void				CTimeCycle::SetTransitionOutOfScriptModifierId(float time)
{
	tcAssertf(m_scriptModifierIndex >= 0,"trying to transition out of a script modifier while none is set");
	m_transitionOut = true;
	m_transitionStrength = m_scriptModifierStrength;
	m_transitionSpeed = m_scriptModifierStrength / time;
}

///////////////////////////////////////////////////////////////////////////////
//  CalcAmbientScale
///////////////////////////////////////////////////////////////////////////////													

Vec2V_Out			CTimeCycle::CalcAmbientScale				(Vec3V_In vPos, fwInteriorLocation intLoc)
{
#if NAVMESH_EXPORT
	if(CNavMeshDataExporter::IsExportingCollision())
	{
		return Vec2V(V_ONE);
	}
#endif
	
	// if we're using a cutscene modifier then return its ambient scale
	if (m_cutsceneModifierIndex != -1)
	{	
		const tcModifier *mod = GetModifier(m_cutsceneModifierIndex);
		Vec2V result = GetModifierAmbientScaleWithDefault(mod);

		if( CRenderer::GetDisableArtificialLights() )
		{
			result.SetYf(CShaderLib::GetOverridesArtAmbScale());
		}
		
		return result;
	}

	// initialise the ambient scale
	float naturalAmbientScale = 1.0f;
	float artificialAmbientScale = 0.0f;

	// query the modifier box tree to get a list of boxes that we are potentially inside
	bool inInterior = intLoc.IsValid();
	bool isUnderWater = Water::IsCameraUnderwater();
	
	int numBoxes = 0;
	int boxIndices[TIMECYCLE_MAX_MODIFIER_BOX_QUERY];
	
	const int hours = CClock::GetHour();
	const int minutes = CClock::GetMinute();
	const int seconds = CClock::GetSecond();

	// if room idx is invalid then it's most likely attached to a portal on the outside of the building
#if TC_DEBUG
	if( !m_gameDebug.GetDisableBoxModifierAmbientScale() )
#endif
	{
		QueryModifierBoxTree(vPos, boxIndices, TIMECYCLE_MAX_MODIFIER_BOX_QUERY, numBoxes); 
	}

	// Apply Exterior modifier boxes, if required
	if (numBoxes > 0 && inInterior == false)
	{
		bool needLerp = false;
		
		float finalNaturalAmbientScale = 1.0f;
		float finalArtificialAmbientScale = 0.0f; 

		fwPool<tcBox>* pPool = tcBox::GetPool();
		Assert(pPool);

		for (s32 b=0; b<numBoxes; b++)
		{
			const tcBox* pBox = pPool->GetSlot(boxIndices[b]);
			Assert(pBox);

			float strengthMult = pBox? pBox->CalcStrengthMult(vPos, hours, minutes, seconds) : 0.0f;
			if (strengthMult > 0.0f)
			{
				float boxStrength = pBox->GetStrength();
				int n = pBox->GetIndex();

				const tcModifier *mod = GetModifier(n);
				needLerp |= mod->GetUserFlag(tcflag_interpolateAmbient);
				
				if( !mod->GetUserFlag(tcflag_interpolateAmbient) && !mod->GetUserFlag(tcflag_interioronly) && mod->GetUserFlag(tcflag_underwateronly) == isUnderWater)
				{
					// return the modifier ambient scale
					bool validNat = false;
					bool validArt = false;
					Vec2V result = GetModifierAmbientScale(mod, validNat, validArt);

					strengthMult *= boxStrength;

					const float invStrengthMult = 1.0f - strengthMult;

					if( validNat )
					{
						const float natural = (naturalAmbientScale * invStrengthMult) + result.GetXf() * strengthMult;
						finalNaturalAmbientScale = (natural < finalNaturalAmbientScale) ? natural : finalNaturalAmbientScale;
					}

					if( validArt )
					{
						const float artificial = (artificialAmbientScale * invStrengthMult) + result.GetYf() * strengthMult;
						finalArtificialAmbientScale = (artificial > finalArtificialAmbientScale) ? artificial : finalArtificialAmbientScale;
					}
				}
			}
		}
		
		spdSphere tempSphere(vPos, ScalarV(0.1f));

		tcInstBoxSearchResult* rawTcInstBoxSearchResult = Alloca(tcInstBoxSearchResult, TCINSTBOX_MAX_SEARCH_RESULTS);
		atUserArray<tcInstBoxSearchResult> tcInstBoxSearchResults(rawTcInstBoxSearchResult, TCINSTBOX_MAX_SEARCH_RESULTS);

		g_tcInstBoxMgr.Search( spdAABB(tempSphere), tcInstBoxSearchResults);
		for (s32 i=0; i<tcInstBoxSearchResults.GetCount(); i++)
		{
			const tcInstBoxSearchResult &result = tcInstBoxSearchResults[i];
			
			float strengthMult = result.CalcStrengthMult(vPos, hours, minutes, seconds);
			if (strengthMult > 0.0f)
			{
				float boxStrength = result.m_pType->GetPercentage();

				if( !isUnderWater)
				{
					// return the modifier ambient scale
					bool validNat = result.m_pType->GetIsValidNaturalAmbient();
					bool validArt = result.m_pType->GetIsValidArtificialAmbient();
					float natAmb = result.m_pType->GetNaturalAmbient();
					float artAmb = result.m_pType->GetArtificialAmbient();
					
					strengthMult *= boxStrength;

					const float invStrengthMult = 1.0f - strengthMult;

					if( validNat )
					{
						const float natural = (naturalAmbientScale * invStrengthMult) + natAmb * strengthMult;
						finalNaturalAmbientScale = (natural < finalNaturalAmbientScale) ? natural : finalNaturalAmbientScale;
					}

					if( validArt )
					{
						const float artificial = (artificialAmbientScale * invStrengthMult) + artAmb * strengthMult;
						finalArtificialAmbientScale = (artificial > finalArtificialAmbientScale) ? artificial : finalArtificialAmbientScale;
					}
				}
			}
		}
		
		if( needLerp )
		{
			for (s32 b=0; b<numBoxes; b++)
			{
				const tcBox* pBox = pPool->GetSlot(boxIndices[b]);
				Assert(pBox);

				float strengthMult = pBox ? pBox->CalcStrengthMult(vPos, hours, minutes, seconds) : 0.0f;
				if (strengthMult > 0.0f)
				{
					float boxStrength = pBox->GetStrength();
					int n = pBox->GetIndex();

					const tcModifier *mod = GetModifier(n);
					if( mod->GetUserFlag(tcflag_interpolateAmbient) && !mod->GetUserFlag(tcflag_interioronly) && mod->GetUserFlag(tcflag_underwateronly) == isUnderWater)
					{
						// return the modifier ambient scale
						bool validNat = false;
						bool validArt = false;
						Vec2V result = GetModifierAmbientScale(mod, validNat, validArt);

						strengthMult *= boxStrength;

						if( validNat )
						{
							const float natural = result.GetXf();
							finalNaturalAmbientScale = Lerp(strengthMult, finalNaturalAmbientScale, natural);
						}

						if( validArt )
						{
							const float artificial = result.GetYf();
							finalArtificialAmbientScale = Lerp(strengthMult, finalArtificialAmbientScale, artificial);

						}
					}
				}
			}		
		}
	
		naturalAmbientScale = finalNaturalAmbientScale;
		artificialAmbientScale = finalArtificialAmbientScale;
	}

#if TC_DEBUG
	if( m_gameDebug.GetDisableIntModifierAmbientScale() == false )
#endif
	{
		if (inInterior)
		{
			Vec3V basePortalAmbientScales;
			Vec3V bleedPortalAmbientScales;
			
			 CPortal::CalcAmbientScale(vPos, basePortalAmbientScales, bleedPortalAmbientScales, intLoc);

			// apply base modifier
			const float bleeding = bleedPortalAmbientScales.GetZf();
			if( bleeding > -1.0f )
			{
				// Apply bleeding modifier
				const float bleedDelta = Clamp(bleeding,0.0f,1.0f);
				const float bleedPortalAmbient = bleedPortalAmbientScales.GetXf();
				const float bleedPortalArtificialAmbient = bleedPortalAmbientScales.GetYf();

				naturalAmbientScale = Lerp(bleedDelta, bleedPortalAmbient,	naturalAmbientScale);
				artificialAmbientScale = bleedPortalArtificialAmbient;
			}
			else
			{
				// the blendout is additive accross all portals, so can get out of range
				const float baseDelta = Clamp(basePortalAmbientScales.GetZf(),0.0f,1.0f);
				const float basePortalNaturalAmbient = basePortalAmbientScales.GetXf();
				const float basePortalArtificialAmbient = basePortalAmbientScales.GetYf();

				naturalAmbientScale = Lerp(baseDelta, basePortalNaturalAmbient,	naturalAmbientScale);
				artificialAmbientScale = Lerp(baseDelta, basePortalArtificialAmbient, artificialAmbientScale);
			}

			// Apply interior modifiers
			// Notice that it's not using a weird pushUp/pushDown but a more classic lerp.
			if (numBoxes > 0)
			{
				float finalNaturalAmbientScale = naturalAmbientScale;
				float finalArtificialAmbientScale = artificialAmbientScale; 

				fwPool<tcBox>* pPool = tcBox::GetPool();
				Assert(pPool);

				for (s32 b=0; b<numBoxes; b++)
				{
					const tcBox* pBox = pPool->GetSlot(boxIndices[b]);
					Assert(pBox);

					float strengthMult = pBox ? pBox->CalcStrengthMult(vPos, hours, minutes, seconds) : 0.0f;
					if (strengthMult > 0.0f)
					{
						float boxStrength = pBox->GetStrength();
						int n = pBox->GetIndex();

						const tcModifier *mod = GetModifier(n);
						if( mod->GetUserFlag(tcflag_interioronly) && mod->GetUserFlag(tcflag_underwateronly) == isUnderWater )
						{
							bool validNat = false;
							bool validArt = false;
							Vec2V result = GetModifierAmbientScale(mod, validNat, validArt);

							strengthMult *= boxStrength;

							if( validNat )
							{
								finalNaturalAmbientScale = Lerp(strengthMult, finalNaturalAmbientScale, result.GetXf());
							}
							
							if( validArt )
							{
								finalArtificialAmbientScale = Lerp(strengthMult, finalArtificialAmbientScale, result.GetYf());
							}
						}
					}
				}

				naturalAmbientScale = finalNaturalAmbientScale;
				artificialAmbientScale = finalArtificialAmbientScale;
			}
		}
		else
		{
			for(int i=0;i<m_frameInfo.m_LocalPortalCache.GetCount();i++)
			{
				int mainModIdx = m_frameInfo.m_LocalPortalCache[i].GetMainModifierIdx();
				int secondaryModIdx = m_frameInfo.m_LocalPortalCache[i].GetSecondaryModifierIdx();

				bool cutScenePlaying = camInterface::GetCutsceneDirector().IsCutScenePlaying() BANK_ONLY( && !CutSceneManager::GetInstance()->IsPaused() );
				float doorOcclusion = cutScenePlaying ? 1.0f : m_frameInfo.m_LocalPortalCache[i].GetDoorOcclusion();
				
				float natAmb = 1.0f;
				float artAmb = 0.0f;

				bool expand = CalcModifierAmbientScale(mainModIdx, secondaryModIdx, 1.0f, natAmb, artAmb ASSERT_ONLY(, mainModIdx != -1 ? GetModifierName(mainModIdx) : "N/A", secondaryModIdx != -1 ? GetModifierName(secondaryModIdx) : "N/A"));
				float blend = expand ? m_frameInfo.m_LocalPortalCache[i].GetCorners().CalcExpandedAmbientScaleFeather(vPos) : m_frameInfo.m_LocalPortalCache[i].GetCorners().CalcAmbientScaleFeather(vPos);
				
				// Blend in artifical ambient scale
				artificialAmbientScale = Lerp(blend*doorOcclusion, artificialAmbientScale, artAmb);
			}
		}
	}

#if TC_DEBUG
	if( m_debug.GetModifierEditApply() )
	{
		s32 editModId = m_debug.GetModifierEditId();
		float strength = m_debug.GetModifierEditStrength();
		const float invStrength = 1.0f - strength;
		
		const tcModifier *mod = GetModifier(editModId);
		bool validNat = false;
		bool validArt = false;
		Vec2V result = GetModifierAmbientScale(mod, validNat, validArt);

		if( validNat )
		{
			const float natural = (naturalAmbientScale * invStrength) + result.GetXf() * strength;
			naturalAmbientScale = natural;
		}
		
		if( validArt )
		{
			const float artificial = (artificialAmbientScale * invStrength) + result.GetYf() * strength;
			artificialAmbientScale = artificial;
		}
	}
#endif

	if( CRenderer::GetDisableArtificialLights() )
	{
		artificialAmbientScale = CShaderLib::GetOverridesArtAmbScale();
	}

	return Vec2V(naturalAmbientScale, artificialAmbientScale);
}

///////////////////////////////////////////////////////////////////////////////
//  GetSkySettings
///////////////////////////////////////////////////////////////////////////////													
void CTimeCycle::GetSkySettings (CSkySettings* pSkySettings)
{
	Assert(sysThreadType::IsRenderThread());

	#define GET_COL(COL) Vec3V(currKeyframe.GetVar(COL##_R), currKeyframe.GetVar(COL##_G), currKeyframe.GetVar(COL##_B))
	#define GET_FLOAT(COL) float(currKeyframe.GetVar(COL))

	const tcKeyframeUncompressed& currKeyframe = GetCurrRenderKeyframe();

	// Sky colouring
	pSkySettings->m_azimuthEastColor.m_value = GET_COL(TCVAR_SKY_AZIMUTH_EAST_COL);
	pSkySettings->m_azimuthEastColorIntensity = GET_FLOAT(TCVAR_SKY_AZIMUTH_EAST_COL_INTEN);
	
	pSkySettings->m_azimuthWestColor.m_value = GET_COL(TCVAR_SKY_AZIMUTH_WEST_COL);
	pSkySettings->m_azimuthWestColorIntensity = GET_FLOAT(TCVAR_SKY_AZIMUTH_WEST_COL_INTEN);
	
	pSkySettings->m_azimuthTransitionColor.m_value = GET_COL(TCVAR_SKY_AZIMUTH_TRANSITION_COL);
	pSkySettings->m_azimuthTransitionColorIntensity = GET_FLOAT(TCVAR_SKY_AZIMUTH_TRANSITION_COL_INTEN);
	
	pSkySettings->m_azimuthTransitionPosition.m_value = GET_FLOAT(TCVAR_SKY_AZIMUTH_TRANSITION_POSITION);

	pSkySettings->m_zenithColor.m_value = GET_COL(TCVAR_SKY_ZENITH_COL);
	pSkySettings->m_zenithColorIntensity = GET_FLOAT(TCVAR_SKY_ZENITH_COL_INTEN);

	pSkySettings->m_zenithTransitionColor.m_value = GET_COL(TCVAR_SKY_ZENITH_TRANSITION_COL);
	pSkySettings->m_zenithTransitionColorIntensity = GET_FLOAT(TCVAR_SKY_ZENITH_TRANSITION_COL_INTEN);

	pSkySettings->m_zenithTransitionPosition = GET_FLOAT(TCVAR_SKY_ZENITH_TRANSITION_POSITION);
	pSkySettings->m_zenithTransitionEastBlend = GET_FLOAT(TCVAR_SKY_ZENITH_TRANSITION_EAST_BLEND);
	pSkySettings->m_zenithTransitionWestBlend = GET_FLOAT(TCVAR_SKY_ZENITH_TRANSITION_WEST_BLEND);
	pSkySettings->m_zenithBlendStart = GET_FLOAT(TCVAR_SKY_ZENITH_BLEND_START);
	pSkySettings->m_hdrIntensity.m_value = GET_FLOAT(TCVAR_SKY_HDR);

	float lastLum = pSkySettings->m_skyPlaneColor.m_value.GetWf();
	pSkySettings->m_skyPlaneColor.m_value = Vec4V(GET_COL(TCVAR_SKY_PLANE_COL));
	
	float lum = PostFX::GetRenderLuminance();
	if (!FPIsFinite(lum))
		lum = lastLum;		// reuse last lum value, in the new one is invalid

	BANK_ONLY(lum = (camInterface::GetDebugDirector().IsFreeCamActive()) ? 0.01f : lum);
	pSkySettings->m_skyPlaneColor.m_value.SetW(GET_FLOAT(TCVAR_SKY_PLANE_INTEN) * lum);
	
	// Sun variables
	pSkySettings->m_sunColor.m_value = (GetLightningOverrideRender()?GetLightningColorRender().GetRGB():GET_COL(TCVAR_SKY_SUN_COL));
	pSkySettings->m_sunDiscColor = GET_COL(TCVAR_SKY_SUN_DISC_COL);
	pSkySettings->m_sunDiscSize = GET_FLOAT(TCVAR_SKY_SUN_DISC_SIZE);
	pSkySettings->m_sunHdrIntensity = GET_FLOAT(TCVAR_SKY_SUN_HDR);
	pSkySettings->m_reflectionHdrIntensityMult = GET_FLOAT(TCVAR_REFLECTION_HDR_MULT);

	pSkySettings->m_miePhase = GET_FLOAT(TCVAR_SKY_SUN_MIEPHASE);
	pSkySettings->m_mieScatter = GET_FLOAT(TCVAR_SKY_SUN_MIESCATTER);
	pSkySettings->m_mieIntensityMult = GET_FLOAT(TCVAR_SKY_SUN_MIE_INTENSITY_MULT);

	pSkySettings->m_sunInfluenceRadius = GET_FLOAT(TCVAR_SKY_SUN_INFLUENCE_RADIUS);
	pSkySettings->m_sunScatterIntensity = GET_FLOAT(TCVAR_SKY_SUN_SCATTER_INTEN);

	// Stars / Moon
	pSkySettings->m_moonColor.m_value = (GetLightningOverrideRender()?GetLightningColorRender().GetRGB():GET_COL(TCVAR_SKY_MOON_COL));
	pSkySettings->m_moonDiscSize = GET_FLOAT(TCVAR_SKY_MOON_DISC_SIZE);
	pSkySettings->m_moonIntensity.m_value = GET_FLOAT(TCVAR_SKY_MOON_ITEN);
	pSkySettings->m_starfieldIntensity.m_value = GET_FLOAT(TCVAR_SKY_STARS_ITEN);

	pSkySettings->m_moonInfluenceRadius = GET_FLOAT(TCVAR_SKY_MOON_INFLUENCE_RADIUS);
	pSkySettings->m_moonScatterIntensity = GET_FLOAT(TCVAR_SKY_MOON_SCATTER_INTEN);

	// Cloud variables
	pSkySettings->m_vCloudBaseColour = GET_COL(TCVAR_SKY_CLOUD_BASE_COL);
	
	pSkySettings->m_cloudFadeOut = GET_FLOAT(TCVAR_SKY_CLOUD_FADEOUT);
	pSkySettings->m_cloudHdrIntensity = GET_FLOAT(TCVAR_SKY_CLOUD_HDR);
	pSkySettings->m_cloudShadowStrength = GET_FLOAT(TCVAR_SKY_CLOUD_SHADOW_STRENGTH);
	pSkySettings->m_cloudOffset = GET_FLOAT(TCVAR_SKY_CLOUD_OFFSET);

	pSkySettings->m_vCloudShadowColour = GET_COL(TCVAR_SKY_CLOUD_SHADOW_COL);

	pSkySettings->m_cloudMidColour.m_value = GET_COL(TCVAR_SKY_CLOUD_MID_COL);

	pSkySettings->m_cloudBaseStrength = GET_FLOAT(TCVAR_SKY_CLOUD_BASE_STRENGTH);
	pSkySettings->m_cloudDensityMultiplier = GET_FLOAT(TCVAR_SKY_CLOUD_DENSITY_MULT);
	pSkySettings->m_cloudDensityBias = GET_FLOAT(TCVAR_SKY_CLOUD_DENSITY_BIAS);

	pSkySettings->m_cloudEdgeDetailStrength = GET_FLOAT(TCVAR_SKY_CLOUD_EDGE_STRENGTH);
	pSkySettings->m_cloudOverallDetailStrength = GET_FLOAT(TCVAR_SKY_CLOUD_OVERALL_STRENGTH);
	pSkySettings->m_cloudOverallDetailColor = GET_FLOAT(TCVAR_SKY_CLOUD_OVERALL_COLOR);
	pSkySettings->m_cloudDitherStrength = GET_FLOAT(TCVAR_SKY_CLOUD_DITHER_STRENGTH);
	
	pSkySettings->m_vSmallCloudColor = GET_COL(TCVAR_SKY_SMALL_CLOUD_COL);

	pSkySettings->m_smallCloudDetailStrength = GET_FLOAT(TCVAR_SKY_SMALL_CLOUD_DETAIL_STRENGTH);
	pSkySettings->m_smallCloudDetailScale = GET_FLOAT(TCVAR_SKY_SMALL_CLOUD_DETAIL_SCALE);
	pSkySettings->m_smallCloudDensityMultiplier = GET_FLOAT(TCVAR_SKY_SMALL_CLOUD_DENSITY_MULT);
	pSkySettings->m_smallCloudDensityBias = GET_FLOAT(TCVAR_SKY_SMALL_CLOUD_DENSITY_BIAS);

	pSkySettings->m_lightningDirection = GetLightningDirectionRender();

	pSkySettings->m_sunDirection = m_currFrameInfo->m_vSunSpriteDir;
	pSkySettings->m_moonDirection = m_currFrameInfo->m_vMoonSpriteDir;
}

///////////////////////////////////////////////////////////////////////////////
//  GetMinimalSkySettings
///////////////////////////////////////////////////////////////////////////////													
void CTimeCycle::GetMinimalSkySettings (CSkySettings* pSkySettings)
{
	Assert(sysThreadType::IsRenderThread());

	pSkySettings->m_lightningDirection = GetLightningDirectionRender();
	pSkySettings->m_sunDirection = m_currFrameInfo->m_vSunSpriteDir;
	pSkySettings->m_moonDirection = m_currFrameInfo->m_vMoonSpriteDir;
}

///////////////////////////////////////////////////////////////////////////////
//  GetNoiseSkySettings
///////////////////////////////////////////////////////////////////////////////
void CTimeCycle::GetNoiseSkySettings (CSkySettings* pSkySettings)
{
	tcKeyframeUncompressed& currKeyframe = m_currFrameInfo->m_keyframe;

	pSkySettings->m_noiseFrequency.m_value = GET_FLOAT(TCVAR_SKY_CLOUD_GEN_FREQUENCY);
	pSkySettings->m_noiseScale.m_value = GET_FLOAT(TCVAR_SKY_CLOUD_GEN_SCALE);
	pSkySettings->m_noiseThreshold.m_value = GET_FLOAT(TCVAR_SKY_CLOUD_GEN_THRESHOLD);
	pSkySettings->m_noiseSoftness.m_value = GET_FLOAT(TCVAR_SKY_CLOUD_GEN_SOFTNESS);
	pSkySettings->m_noiseDensityOffset.m_value = GET_FLOAT(TCVAR_SKY_CLOUD_GEN_DENSITY_OFFSET);
}

///////////////////////////////////////////////////////////////////////////////
//  SetupRenderThreadFrameInfo
///////////////////////////////////////////////////////////////////////////////													
void CTimeCycle::SetupRenderThreadFrameInfo()
{
	DLC(dlCmdSetupTimeCycleFrameInfo, ());
}

///////////////////////////////////////////////////////////////////////////////
//  ClearRenderFrameInfo
///////////////////////////////////////////////////////////////////////////////													
void CTimeCycle::ClearRenderFrameInfo()
{
	DLC(dlCmdClearTimeCycleFrameInfo, ());
}

///////////////////////////////////////////////////////////////////////////////
//  CalculateDayNightBalance
///////////////////////////////////////////////////////////////////////////////													
float CTimeCycle::CalculateDayNightBalance() const
{
	// 0.0 = full day
	// 1.0 = full night
	const u32 emissiveOn = (u32)g_visualSettings.Get("emissive.night.start.time", 20.0f);
	const u32 emissiveOff = (u32)g_visualSettings.Get("emissive.night.end.time", 7.0f);
	const u32 emissiveOnToMidnight = 24 - emissiveOn; 

	u32 timeFlags = 0;
	for (int i = 0; i < emissiveOnToMidnight + emissiveOff; i++)
	{
		timeFlags |= (1 << ((i + emissiveOn) % 24));
	}

	const u32 hour = CClock::GetHour();
	const u32 nextHour = (CClock::GetHour() + 1) % 24;
	const float fade = (float)CClock::GetMinute() + ((float)CClock::GetSecond() / 60.0f);

	// Night emissives about to turn on
	if ((timeFlags & (1 << hour)) == 0 && (timeFlags & (1 << (nextHour))) != 0)
	{
		return Saturate(fade / 60.0f);
	}

	// Night emissive about to turn off
	if ((timeFlags & (1 << hour)) != 0 && (timeFlags & (1 << (nextHour))) == 0)
	{
		return 1.0f - Saturate(fade / 60.0f);
	}

	// They are on, keep them on
	if (timeFlags & (1 << hour))
	{
		return 1.0f; 

	}

	return 0.0f; 
}

#if GTA_REPLAY

// Returns number of keys added
u32	CTimeCycle::GetTimeCycleKeyframeData(TCNameAndValue *pData, bool getDefaultData)
{
	u32 storedCount = 0;
	for(int i=0;i<TCVAR_NUM;i++)
	{
		if( g_varInfos[i].replayOverride == true )
		{
			if( !getDefaultData || ( getDefaultData && m_ReplayDefaultTCFrame.GetVar(i) != m_ReplayTCFrame.GetVar(i) ) )
			{
				pData[storedCount].nameHash = atStringHash(g_varInfos[i].name);
				pData[storedCount].value = getDefaultData ? m_ReplayDefaultTCFrame.GetVar(i) : m_ReplayTCFrame.GetVar(i);
				storedCount++;
			}
		}
	}
	return storedCount;
}

void	CTimeCycle::SetTimeCycleKeyframeData(const TCNameAndValue *pData, u32 noOfKeys, bool isSniperScope, bool isDefaultData)
{
	m_bReplaySniperScope = isSniperScope;
	m_bUsingDefaultTCData = isDefaultData;
	for(int i=0;i<noOfKeys;i++)
	{
		u16 *pIndex = m_EffectsMap.Access(pData[i].nameHash);
		if( pIndex )
		{
			m_ReplayTCFrame.SetVar(*pIndex, pData[i].value);
		}
	}
}

void	CTimeCycle::RecordTimeCycleDataForReplay(eTimeCyclePhase timeCyclePhase)
{
	// If we're not recording, then backup our TC data if we don't have some specific modifiers on.
	if(!ShouldUseReplayTCData())
	{
		if( timeCyclePhase == TIME_CYCLE_PHASE_REPLAY_FREECAM )
		{
			m_ReplayDefaultTCFrame = m_frameInfo.m_keyframe;
		}
		else if( timeCyclePhase == TIME_CYCLE_PHASE_REPLAY_NORMAL )
		{
			m_ReplayTCFrame = m_frameInfo.m_keyframe;
		}
	}
}

bool	CTimeCycle::ShouldApplyInteriorModifier() const
{
	bool shouldApplyMod = false;
	CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();
	if(pIntInst)
	{
		CInteriorProxy* pProxy = pIntInst->GetProxy();
		if(pProxy)
		{
			u32 proxyHash = pProxy->GetNameHash();
			int count = sizeof(m_InteriorReplayLookupTable) / sizeof(EffectTableEntry);

			for( int i = 0; i < count; ++i)
			{
				if( m_InteriorReplayLookupTable[i] == proxyHash )
				{
					shouldApplyMod = true;
				}
			}
		}
	}
	
	if( CReplayMgr::IsEditModeActive() )
	{
		bool bgameCameraActive = camInterface::GetReplayDirector().IsRecordedCamera();
		if( bgameCameraActive && !m_bUsingDefaultTCData )
		{
			shouldApplyMod = false;
		}
	}

	return shouldApplyMod;
}

bool	CTimeCycle::ShouldDisableScripFxForFreeCam(u32 scriptFx, u32 transistionFx) const
{
	// If we're disabled for game cam, then we should also be for free cam.
	if( ShouldDisableScripFxForGameCam(scriptFx, transistionFx ) )
	{
		return true;
	}

	int count = sizeof(m_ScriptEffectFreeCamRemovalReplayLookupTable) / sizeof(EffectTableEntry);

	for(int i = 0; i < count; ++i)
	{
		if( m_ScriptEffectFreeCamRemovalReplayLookupTable[i] == scriptFx || 
			m_ScriptEffectFreeCamRemovalReplayLookupTable[i] == transistionFx )
		{
			return true;
		}
	}

	return false;
}

bool	CTimeCycle::ShouldDisableScripFxForGameCam(u32 scriptFx, u32 transistionFx) const
{
	int count = sizeof(m_ScriptEffectAllCamsRemovalReplayLookupTable) / sizeof(EffectTableEntry);

	for(int i = 0; i < count; ++i)
	{
		if( m_ScriptEffectAllCamsRemovalReplayLookupTable[i] == scriptFx || 
			m_ScriptEffectAllCamsRemovalReplayLookupTable[i] == transistionFx )
		{
			return true;
		}
	}

	return false;
}

bool	CTimeCycle::ShouldUseReplayTCData()
{
	if( CReplayMgr::IsEditModeActive() )
	{
		// We're in replay, use recorded data
		return true;
	}
	return false;	// Not in replay, don't use data
}

#endif	//GTA_REPLAY
