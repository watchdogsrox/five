///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	timecycle/TimeCycle.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	19 Aug 2008
//	WHAT:	 
//
///////////////////////////////////////////////////////////////////////////////

#ifndef TIMECYCLE_H
#define TIMECYCLE_H

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

// rage
#include "vectormath/vec4v.h"
#include "string/stringhash.h"
#include "timecycle/tcmanager.h"

// framework
#include "timecycle/tcconfig.h"
#include "timecycle/tckeyframe.h"
#include "fwscene/world/InteriorLocation.h"
#include "fwmaths/PortalCorners.h"

// game
#include "TimeCycleDebug.h"
#include "control/replay/ReplaySettings.h"
#include "Renderer/RenderThread.h"
#include "vfx/VisualEffects.h"

///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#define TIMECYCLE_MAX_INTERIOR_MODIFIERS		(20)
#define PLAYERSWITCH_FORCES_LIGHTDIR			(0)

#define MAX_LOCAL_PORTAL						(8)

struct CSkySettings;

#if __ASSERT

#define TC_ATSTRINGHASH(string,hash) ATSTRINGHASH(string,hash),string

#else

#define TC_ATSTRINGHASH(string,hash) ATSTRINGHASH(string,hash),NULL

#endif // __ASSERT
///////////////////////////////////////////////////////////////////////////////
//  ENUMERATIONS
///////////////////////////////////////////////////////////////////////////////

#if GTA_REPLAY
enum eTimeCyclePhase
{
	TIME_CYCLE_PHASE_REPLAY_FREECAM = 0,
	TIME_CYCLE_PHASE_REPLAY_NORMAL,
	TIME_CYCLE_PHASE_GAME,

	TIME_CYCLE_PHASE_MAX
};
#endif // GTA_REPLAY

enum TimeCycleVar_e
{
	// light variables
	TCVAR_LIGHT_DIR_COL_R				= 0,
	TCVAR_LIGHT_DIR_COL_G,
	TCVAR_LIGHT_DIR_COL_B,
	TCVAR_LIGHT_DIR_MULT,

	TCVAR_LIGHT_DIRECTIONAL_AMB_COL_R,
	TCVAR_LIGHT_DIRECTIONAL_AMB_COL_G,
	TCVAR_LIGHT_DIRECTIONAL_AMB_COL_B,
	TCVAR_LIGHT_DIRECTIONAL_AMB_INTENSITY,
	TCVAR_LIGHT_DIRECTIONAL_AMB_INTENSITY_MULT,
	TCVAR_LIGHT_DIRECTIONAL_AMB_BOUNCE_ENABLED,

	TCVAR_LIGHT_AMB_DOWN_WRAP,

	TCVAR_LIGHT_NATURAL_AMB_DOWN_COL_R,
	TCVAR_LIGHT_NATURAL_AMB_DOWN_COL_G,
	TCVAR_LIGHT_NATURAL_AMB_DOWN_COL_B,
	TCVAR_LIGHT_NATURAL_AMB_DOWN_INTENSITY,

	TCVAR_LIGHT_NATURAL_AMB_BASE_COL_R,
	TCVAR_LIGHT_NATURAL_AMB_BASE_COL_G,
	TCVAR_LIGHT_NATURAL_AMB_BASE_COL_B,
	TCVAR_LIGHT_NATURAL_AMB_BASE_INTENSITY,
	TCVAR_LIGHT_NATURAL_AMB_BASE_INTENSITY_MULT,

	TCVAR_LIGHT_NATURAL_PUSH,
	TCVAR_LIGHT_AMBIENT_BAKE_RAMP,

	TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_R,
	TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_G,
	TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_COL_B,
	TCVAR_LIGHT_ARTIFICIAL_INT_AMB_DOWN_INTENSITY,

	TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_COL_R,
	TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_COL_G,
	TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_COL_B,
	TCVAR_LIGHT_ARTIFICIAL_INT_AMB_BASE_INTENSITY,

	TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_DOWN_COL_R,
	TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_DOWN_COL_G,
	TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_DOWN_COL_B,
	TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_DOWN_INTENSITY,

	TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_BASE_COL_R,
	TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_BASE_COL_G,
	TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_BASE_COL_B,
	TCVAR_LIGHT_ARTIFICIAL_EXT_AMB_BASE_INTENSITY,

	TCVAR_PED_LIGHT_COL_R,
	TCVAR_PED_LIGHT_COL_G,
	TCVAR_PED_LIGHT_COL_B,
	TCVAR_PED_LIGHT_MULT,
	TCVAR_PED_LIGHT_DIRECTION_X,
	TCVAR_PED_LIGHT_DIRECTION_Y,
	TCVAR_PED_LIGHT_DIRECTION_Z,

	TCVAR_LIGHT_AMB_OCC_MULT,
	TCVAR_LIGHT_AMB_OCC_MULT_PED,
	TCVAR_LIGHT_AMB_OCC_MULT_VEH,
	TCVAR_LIGHT_AMB_OCC_MULT_PROP,
	TCVAR_LIGHT_AMB_VOLUMES_IN_DIFFUSE,
	TCVAR_LIGHT_SSAO_INTEN,
	TCVAR_LIGHT_SSAO_TYPE,
	TCVAR_LIGHT_SSAO_CP_STRENGTH,
	TCVAR_LIGHT_SSAO_QS_STRENGTH,

	TCVAR_LIGHT_PED_RIM_MULT,
	TCVAR_LIGHT_DYNAMIC_BAKE_TWEAK,
	TCVAR_LIGHT_VEHICLE_SECOND_SPEC_OVERRIDE,
	TCVAR_LIGHT_VEHICLE_INTENSITY_SCALE,

	TCVAR_LIGHT_DIRECTION_OVERRIDE,
	TCVAR_LIGHT_DIRECTION_OVERRIDE_OVERRIDES_SUN,
	TCVAR_SUN_DIRECTION_X,
	TCVAR_SUN_DIRECTION_Y,
	TCVAR_SUN_DIRECTION_Z,
	TCVAR_MOON_DIRECTION_X,
	TCVAR_MOON_DIRECTION_Y,
	TCVAR_MOON_DIRECTION_Z,

	// light ray variables
	TCVAR_LIGHT_RAY_COL_R,
	TCVAR_LIGHT_RAY_COL_G,
	TCVAR_LIGHT_RAY_COL_B,
	TCVAR_LIGHT_RAY_MULT,
	TCVAR_LIGHT_RAY_UNDERWATER_MULT,
	TCVAR_LIGHT_RAY_DIST,
	TCVAR_LIGHT_RAY_HEIGHTFALLOFF,
	TCVAR_LIGHT_RAY_HEIGHTFALLOFFSTART,
	
	TCVAR_LIGHT_RAY_ADD_REDUCER,
	TCVAR_LIGHT_RAY_BLIT_SIZE,
	TCVAR_LIGHT_RAY_RAY_LENGTH,

	// post fx variables
	TCVAR_POSTFX_EXPOSURE,
	TCVAR_POSTFX_EXPOSURE_MIN,
	TCVAR_POSTFX_EXPOSURE_MAX,

	TCVAR_POSTFX_BRIGHT_PASS_THRESH_WIDTH,
	TCVAR_POSTFX_BRIGHT_PASS_THRESH,
	TCVAR_POSTFX_INTENSITY_BLOOM,

	TCVAR_POSTFX_CORRECT_COL_R,
	TCVAR_POSTFX_CORRECT_COL_G,
	TCVAR_POSTFX_CORRECT_COL_B,
	TCVAR_POSTFX_CORRECT_CUTOFF,

	TCVAR_POSTFX_SHIFT_COL_R,
	TCVAR_POSTFX_SHIFT_COL_G,
	TCVAR_POSTFX_SHIFT_COL_B,
	TCVAR_POSTFX_SHIFT_CUTOFF,

	TCVAR_POSTFX_DESATURATION,

	TCVAR_POSTFX_NOISE,
	TCVAR_POSTFX_NOISE_SIZE,

	TCVAR_POSTFX_TONEMAP_FILMIC_OVERRIDE_DARK,
	TCVAR_POSTFX_TONEMAP_FILMIC_EXPOSURE_DARK,
	TCVAR_POSTFX_TONEMAP_FILMIC_A_DARK,
	TCVAR_POSTFX_TONEMAP_FILMIC_B_DARK,
	TCVAR_POSTFX_TONEMAP_FILMIC_C_DARK,
	TCVAR_POSTFX_TONEMAP_FILMIC_D_DARK,
	TCVAR_POSTFX_TONEMAP_FILMIC_E_DARK,
	TCVAR_POSTFX_TONEMAP_FILMIC_F_DARK,
	TCVAR_POSTFX_TONEMAP_FILMIC_W_DARK,

	TCVAR_POSTFX_TONEMAP_FILMIC_OVERRIDE_BRIGHT,
	TCVAR_POSTFX_TONEMAP_FILMIC_EXPOSURE_BRIGHT,
	TCVAR_POSTFX_TONEMAP_FILMIC_A_BRIGHT,
	TCVAR_POSTFX_TONEMAP_FILMIC_B_BRIGHT,
	TCVAR_POSTFX_TONEMAP_FILMIC_C_BRIGHT,
	TCVAR_POSTFX_TONEMAP_FILMIC_D_BRIGHT,
	TCVAR_POSTFX_TONEMAP_FILMIC_E_BRIGHT,
	TCVAR_POSTFX_TONEMAP_FILMIC_F_BRIGHT,
	TCVAR_POSTFX_TONEMAP_FILMIC_W_BRIGHT,

	// vignetting 
	TCVAR_POSTFX_VIGNETTING_INTENSITY,
	TCVAR_POSTFX_VIGNETTING_RADIUS,
	TCVAR_POSTFX_VIGNETTING_CONTRAST,
	TCVAR_POSTFX_VIGNETTING_COL_R,
	TCVAR_POSTFX_VIGNETTING_COL_G,
	TCVAR_POSTFX_VIGNETTING_COL_B,

	// colour gradient variables
	TCVAR_POSTFX_GRAD_TOP_COL_R,
	TCVAR_POSTFX_GRAD_TOP_COL_G,
	TCVAR_POSTFX_GRAD_TOP_COL_B,

	TCVAR_POSTFX_GRAD_MIDDLE_COL_R,
	TCVAR_POSTFX_GRAD_MIDDLE_COL_G,
	TCVAR_POSTFX_GRAD_MIDDLE_COL_B,

	TCVAR_POSTFX_GRAD_BOTTOM_COL_R,
	TCVAR_POSTFX_GRAD_BOTTOM_COL_G,
	TCVAR_POSTFX_GRAD_BOTTOM_COL_B,

	TCVAR_POSTFX_GRAD_MIDPOINT,
	TCVAR_POSTFX_GRAD_TOP_MIDDLE_MIDPOINT,
	TCVAR_POSTFX_GRAD_MIDDLE_BOTTOM_MIDPOINT,

	// postfx scanline
	TCVAR_POSTFX_SCANLINEINTENSITY,
	TCVAR_POSTFX_SCANLINE_FREQUENCY_0,			
	TCVAR_POSTFX_SCANLINE_FREQUENCY_1,			
	TCVAR_POSTFX_SCANLINE_SPEED,	

	// postfx motion blur length
	TCVAR_POSTFX_MOTIONBLUR_LENGTH,

	// depth of field variables
	TCVAR_DOF_FAR,

	TCVAR_DOF_BLUR_MID,
	TCVAR_DOF_BLUR_FAR,
	TCVAR_DOF_ENABLE_HQDOF,
	TCVAR_DOF_HQDOF_SMALLBLUR,
	TCVAR_DOF_HQDOF_SHALLOWDOF,
	TCVAR_DOF_HQDOF_NEARPLANE_OUT,
	TCVAR_DOF_HQDOF_NEARPLANE_IN,
	TCVAR_DOF_HQDOF_FARPLANE_OUT,
	TCVAR_DOF_HQDOF_FARPLANE_IN,

	TCVAR_ENV_BLUR_IN,
	TCVAR_ENV_BLUR_OUT,
	TCVAR_ENV_BLUR_SIZE,

	TCVAR_BOKEH_BRIGHTNESS_MIN,
	TCVAR_BOKEH_BRIGHTNESS_MAX,
	TCVAR_BOKEH_FADE_MIN,
	TCVAR_BOKEH_FADE_MAX,

	// night vision variables
	TCVAR_NV_LIGHT_DIR_MULT,
	TCVAR_NV_LIGHT_AMB_DOWN_MULT,
	TCVAR_NV_LIGHT_AMB_BASE_MULT,

	TCVAR_NV_LOWLUM,
	TCVAR_NV_HIGHLUM,
	TCVAR_NV_TOPLUM,

	TCVAR_NV_SCALERLUM,

	TCVAR_NV_OFFSETLUM,
	TCVAR_NV_OFFSETLOWLUM,
	TCVAR_NV_OFFSETHIGHLUM,

	TCVAR_NV_NOISELUM,
	TCVAR_NV_NOISELOWLUM,
	TCVAR_NV_NOISEHIGHLUM,

	TCVAR_NV_BLOOMLUM,

	TCVAR_NV_COLORLUM_R,
	TCVAR_NV_COLORLUM_G,
	TCVAR_NV_COLORLUM_B,
	TCVAR_NV_COLORLOWLUM_R,
	TCVAR_NV_COLORLOWLUM_G,
	TCVAR_NV_COLORLOWLUM_B,
	TCVAR_NV_COLORHIGHLUM_R,
	TCVAR_NV_COLORHIGHLUM_G,
	TCVAR_NV_COLORHIGHLUM_B,

	// heat haze variables
	TCVAR_HH_RANGESTART,
	TCVAR_HH_RANGEEND,
	TCVAR_HH_MININTENSITY,
	TCVAR_HH_MAXINTENSITY,
	TCVAR_HH_DISPU,
	TCVAR_HH_DISPV,
	TCVAR_HH_TEX1_USCALE,
	TCVAR_HH_TEX1_VSCALE,
	TCVAR_HH_TEX1_UOFFSET,
	TCVAR_HH_TEX1_VOFFSET,
	TCVAR_HH_TEX2_USCALE,
	TCVAR_HH_TEX2_VSCALE,
	TCVAR_HH_TEX2_UOFFSET,
	TCVAR_HH_TEX2_VOFFSET,
	TCVAR_HH_TEX1_OFFSET,
	TCVAR_HH_TEX1_FREQUENCY,
	TCVAR_HH_TEX1_AMPLITUDE,
	TCVAR_HH_TEX1_SCROLLING,
	TCVAR_HH_TEX2_OFFSET,
	TCVAR_HH_TEX2_FREQUENCY,
	TCVAR_HH_TEX2_AMPLITUDE,
	TCVAR_HH_TEX2_SCROLLING,

	// lens distortion
	TCVAR_LENS_DISTORTION_COEFF,
	TCVAR_LENS_DISTORTION_CUBE_COEFF,
	TCVAR_LENS_CHROMATIC_ABERRATION_COEFF,
	TCVAR_LENS_CHROMATIC_ABERRATION_CUBE_COEFF,

	// lens artefacts
	TCVAR_LENS_ARTEFACTS_INTENSITY,
	TCVAR_LENS_ARTEFACTS_INTENSITY_MIN_EXP,
	TCVAR_LENS_ARTEFACTS_INTENSITY_MAX_EXP,

	// blur vignetting
	TCVAR_BLUR_VIGNETTING_RADIUS,
	TCVAR_BLUR_VIGNETTING_INTENSITY,

	// screen blur
	TCVAR_SCREEN_BLUR_INTENSITY,

	// Sky variables
	
	// Sky colouring
	TCVAR_SKY_ZENITH_TRANSITION_POSITION,
	TCVAR_SKY_ZENITH_TRANSITION_EAST_BLEND,
	TCVAR_SKY_ZENITH_TRANSITION_WEST_BLEND,

	TCVAR_SKY_ZENITH_BLEND_START,

	TCVAR_SKY_ZENITH_COL_R,
	TCVAR_SKY_ZENITH_COL_G,
	TCVAR_SKY_ZENITH_COL_B,
	TCVAR_SKY_ZENITH_COL_INTEN,

	TCVAR_SKY_ZENITH_TRANSITION_COL_R,
	TCVAR_SKY_ZENITH_TRANSITION_COL_G,
	TCVAR_SKY_ZENITH_TRANSITION_COL_B,
	TCVAR_SKY_ZENITH_TRANSITION_COL_INTEN,

	TCVAR_SKY_AZIMUTH_TRANSITION_POSITION,

	TCVAR_SKY_AZIMUTH_EAST_COL_R,
	TCVAR_SKY_AZIMUTH_EAST_COL_G,
	TCVAR_SKY_AZIMUTH_EAST_COL_B,
	TCVAR_SKY_AZIMUTH_EAST_COL_INTEN,

	TCVAR_SKY_AZIMUTH_TRANSITION_COL_R,
	TCVAR_SKY_AZIMUTH_TRANSITION_COL_G,
	TCVAR_SKY_AZIMUTH_TRANSITION_COL_B,
	TCVAR_SKY_AZIMUTH_TRANSITION_COL_INTEN,

	TCVAR_SKY_AZIMUTH_WEST_COL_R,
	TCVAR_SKY_AZIMUTH_WEST_COL_G,
	TCVAR_SKY_AZIMUTH_WEST_COL_B,
	TCVAR_SKY_AZIMUTH_WEST_COL_INTEN,

	TCVAR_SKY_HDR,

	TCVAR_SKY_PLANE_COL_R,
	TCVAR_SKY_PLANE_COL_G,
	TCVAR_SKY_PLANE_COL_B,
	TCVAR_SKY_PLANE_INTEN,

	// Sun variables
	TCVAR_SKY_SUN_COL_R,
	TCVAR_SKY_SUN_COL_G,
	TCVAR_SKY_SUN_COL_B,

	TCVAR_SKY_SUN_DISC_COL_R,
	TCVAR_SKY_SUN_DISC_COL_G,
	TCVAR_SKY_SUN_DISC_COL_B,

	TCVAR_SKY_SUN_DISC_SIZE,

	TCVAR_SKY_SUN_HDR,

	TCVAR_SKY_SUN_MIEPHASE,
	TCVAR_SKY_SUN_MIESCATTER,
	TCVAR_SKY_SUN_MIE_INTENSITY_MULT,

	TCVAR_SKY_SUN_INFLUENCE_RADIUS,
	TCVAR_SKY_SUN_SCATTER_INTEN,

	// Stars / Moon variables
	TCVAR_SKY_MOON_COL_R,
	TCVAR_SKY_MOON_COL_G,
	TCVAR_SKY_MOON_COL_B,

	TCVAR_SKY_MOON_DISC_SIZE,

	TCVAR_SKY_MOON_ITEN,
	TCVAR_SKY_STARS_ITEN,

	TCVAR_SKY_MOON_INFLUENCE_RADIUS,
	TCVAR_SKY_MOON_SCATTER_INTEN,

	// Cloud variables
	
	// Cloud generation
	TCVAR_SKY_CLOUD_GEN_FREQUENCY,
	TCVAR_SKY_CLOUD_GEN_SCALE,
	TCVAR_SKY_CLOUD_GEN_THRESHOLD,
	TCVAR_SKY_CLOUD_GEN_SOFTNESS,
	TCVAR_SKY_CLOUD_DENSITY_MULT,
	TCVAR_SKY_CLOUD_DENSITY_BIAS,

	// Main clouds
	TCVAR_SKY_CLOUD_MID_COL_R,
	TCVAR_SKY_CLOUD_MID_COL_G,
	TCVAR_SKY_CLOUD_MID_COL_B,

	TCVAR_SKY_CLOUD_BASE_COL_R,
	TCVAR_SKY_CLOUD_BASE_COL_G,
	TCVAR_SKY_CLOUD_BASE_COL_B,

	TCVAR_SKY_CLOUD_BASE_STRENGTH,

	TCVAR_SKY_CLOUD_SHADOW_COL_R,
	TCVAR_SKY_CLOUD_SHADOW_COL_G,
	TCVAR_SKY_CLOUD_SHADOW_COL_B,

	TCVAR_SKY_CLOUD_SHADOW_STRENGTH,
	TCVAR_SKY_CLOUD_GEN_DENSITY_OFFSET,
	TCVAR_SKY_CLOUD_OFFSET,

	TCVAR_SKY_CLOUD_OVERALL_STRENGTH,
	TCVAR_SKY_CLOUD_OVERALL_COLOR,
	TCVAR_SKY_CLOUD_EDGE_STRENGTH,

	TCVAR_SKY_CLOUD_FADEOUT,
	TCVAR_SKY_CLOUD_HDR,
	TCVAR_SKY_CLOUD_DITHER_STRENGTH,

	TCVAR_SKY_SMALL_CLOUD_COL_R,
	TCVAR_SKY_SMALL_CLOUD_COL_G,
	TCVAR_SKY_SMALL_CLOUD_COL_B,

	TCVAR_SKY_SMALL_CLOUD_DETAIL_STRENGTH,
	TCVAR_SKY_SMALL_CLOUD_DETAIL_SCALE,
	TCVAR_SKY_SMALL_CLOUD_DENSITY_MULT,
	TCVAR_SKY_SMALL_CLOUD_DENSITY_BIAS,

	// cloud shadows
	TCVAR_CLOUD_SHADOW_DENSITY,
	TCVAR_CLOUD_SHADOW_SOFTNESS,
	TCVAR_CLOUD_SHADOW_OPACITY,

	// directional (cascade) shadows
	TCVAR_DIRECTIONAL_SHADOW_NUM_CASCADES,
	TCVAR_DIRECTIONAL_SHADOW_DISTANCE_MULTIPLIER,
	TCVAR_DIRECTIONAL_SHADOW_SOFTNESS,
	TCVAR_DIRECTIONAL_SHADOW_CASCADE0_SCALE,

	// sprite (corona and distant lights) variables
	TCVAR_SPRITE_BRIGHTNESS,
	TCVAR_SPRITE_SIZE,
	TCVAR_SPRITE_CORONA_SCREENSPACE_EXPANSION, // corona-only
	TCVAR_LENSFLARE_VISIBILITY, // Lensflare
	TCVAR_SPRITE_DISTANT_LIGHT_TWINKLE,

	// water variables
	TCVAR_WATER_REFLECTION_FIRST,
	TCVAR_WATER_REFLECTION = TCVAR_WATER_REFLECTION_FIRST,
	TCVAR_WATER_REFLECTION_FAR_CLIP,
	TCVAR_WATER_REFLECTION_LOD,
	TCVAR_WATER_REFLECTION_SKY_FLOD_RANGE,
	TCVAR_WATER_REFLECTION_LOD_RANGE_ENABLED,
	TCVAR_WATER_REFLECTION_LOD_RANGE_HD_START,
	TCVAR_WATER_REFLECTION_LOD_RANGE_HD_END,
	TCVAR_WATER_REFLECTION_LOD_RANGE_ORPHANHD_START,
	TCVAR_WATER_REFLECTION_LOD_RANGE_ORPHANHD_END,
	TCVAR_WATER_REFLECTION_LOD_RANGE_LOD_START,
	TCVAR_WATER_REFLECTION_LOD_RANGE_LOD_END,
	TCVAR_WATER_REFLECTION_LOD_RANGE_SLOD1_START,
	TCVAR_WATER_REFLECTION_LOD_RANGE_SLOD1_END,
	TCVAR_WATER_REFLECTION_LOD_RANGE_SLOD2_START,
	TCVAR_WATER_REFLECTION_LOD_RANGE_SLOD2_END,
	TCVAR_WATER_REFLECTION_LOD_RANGE_SLOD3_START,
	TCVAR_WATER_REFLECTION_LOD_RANGE_SLOD3_END,
	TCVAR_WATER_REFLECTION_LOD_RANGE_SLOD4_START,
	TCVAR_WATER_REFLECTION_LOD_RANGE_SLOD4_END,
	TCVAR_WATER_REFLECTION_HEIGHT_OFFSET,
	TCVAR_WATER_REFLECTION_HEIGHT_OVERRIDE,
	TCVAR_WATER_REFLECTION_HEIGHT_OVERRIDE_AMOUNT,
	TCVAR_WATER_REFLECTION_DISTANT_LIGHT_INTENSITY,
	TCVAR_WATER_REFLECTION_CORONA_INTENSITY,
	TCVAR_WATER_REFLECTION_LAST = TCVAR_WATER_REFLECTION_CORONA_INTENSITY,

	TCVAR_WATER_FOGLIGHT,
	TCVAR_WATER_INTERIOR,
	TCVAR_WATER_ENABLEFOGSTREAMING,
	TCVAR_WATER_FOAM_INTENSITY_MULT,
	TCVAR_WATER_DRYING_SPEED_MULT,
	TCVAR_WATER_SPECULAR_INTENSITY,

	// mirror variables
	TCVAR_MIRROR_REFLECTION_LOCAL_LIGHT_INTENSITY,

	// fog variables
	TCVAR_FOG_START,
	TCVAR_FOG_NEAR_COL_R,
	TCVAR_FOG_NEAR_COL_G,
	TCVAR_FOG_NEAR_COL_B,
	TCVAR_FOG_NEAR_COL_A,

	TCVAR_FOG_SUN_COL_R,
	TCVAR_FOG_SUN_COL_G,
	TCVAR_FOG_SUN_COL_B,
	TCVAR_FOG_SUN_COL_A,
	TCVAR_FOG_SUN_LIGHTING_CALC_POW,

	TCVAR_FOG_MOON_COL_R,
	TCVAR_FOG_MOON_COL_G,
	TCVAR_FOG_MOON_COL_B,
	TCVAR_FOG_MOON_COL_A,
	TCVAR_FOG_MOON_LIGHTING_CALC_POW,

	TCVAR_FOG_FAR_COL_R,
	TCVAR_FOG_FAR_COL_G,
	TCVAR_FOG_FAR_COL_B,
	TCVAR_FOG_FAR_COL_A,

	TCVAR_FOG_DENSITY,
	TCVAR_FOG_HEIGHT_FALLOFF,
	TCVAR_FOG_BASE_HEIGHT,
	TCVAR_FOG_ALPHA,
	TCVAR_FOG_HORIZON_TINT_SCALE,
	TCVAR_FOG_HDR,
	
	TCVAR_FOG_HAZE_COL_R,
	TCVAR_FOG_HAZE_COL_G,
	TCVAR_FOG_HAZE_COL_B,
	TCVAR_FOG_HAZE_DENSITY,
	TCVAR_FOG_HAZE_ALPHA,
	TCVAR_FOG_HAZE_HDR,
	TCVAR_FOG_HAZE_START,

	// Linear piecewise fog.
	TCVAR_FOG_SHAPE_LOW,	// Height of bottom of the interpolated shape.
	TCVAR_FOG_SHAPE_HIGH,	// Height of top of the interpolated shape.
	TCVAR_FOG_LOG_10_MIN_VISIBILIY, // Log 10 of minimum visibility (i.e. distance you can see with fog at full thickness).
	TCVAR_FOG_SHAPE_WEIGHT_0,	// Weight 0.	
	TCVAR_FOG_SHAPE_WEIGHT_1,	// Weight 1.	
	TCVAR_FOG_SHAPE_WEIGHT_2,	// Weight 2.	
	TCVAR_FOG_SHAPE_WEIGHT_3,	// Weight 3.	

	// fog shadows
	TCVAR_FOG_SHADOW_AMOUNT, // maximum amount of fog shadow (below TCVAR_FOG_BASE_HEIGHT)
	TCVAR_FOG_SHADOW_FALLOFF, // height above TCVAR_FOG_BASE_HEIGHT where fog shadow goes to zero
	TCVAR_FOG_SHADOW_BASE_HEIGHT,

	// volumetric lights,
	TCVAR_FOG_VOLUME_RANGE,
	TCVAR_FOG_VOLUME_FADE_RANGE,
	TCVAR_FOG_VOLUME_INTENSITY_SCALE,
	TCVAR_FOG_VOLUME_SIZE_SCALE,

	// Fog ray
	TCVAR_FOG_FOGRAY_CONTRAST,
	TCVAR_FOG_FOGRAY_INTENSITY,
	TCVAR_FOG_FOGRAY_DENSITY,
	TCVAR_FOG_FOGRAY_NEARFADE,
	TCVAR_FOG_FOGRAY_FARFADE,

	// Reflection
	TCVAR_REFLECTION_LOD_RANGE_START,
	TCVAR_REFLECTION_LOD_RANGE_END,
	TCVAR_REFLECTION_SLOD_RANGE_START,
	TCVAR_REFLECTION_SLOD_RANGE_END,
	TCVAR_REFLECTION_INTERIOR_RANGE,
	TCVAR_REFLECTION_TWEAK_INTERIOR_AMB,
	TCVAR_REFLECTION_TWEAK_EXTERIOR_AMB,
	TCVAR_REFLECTION_TWEAK_EMISSIVE,
	TCVAR_REFLECTION_TWEAK_DIRECTIONAL,
	TCVAR_REFLECTION_HDR_MULT,

	// misc variables
	TCVAR_FAR_CLIP,
	TCVAR_TEMPERATURE,
	TCVAR_PTFX_EMISSIVE_INTENSITY_MULT,
	TCVAR_VFXLIGHTNING_INTENSITY_MULT,
	TCVAR_VFXLIGHTNING_VISIBILITY,
	TCVAR_GPUPTFX_LIGHT_INTENSITY_MULT,
	TCVAR_NATURAL_AMBIENT_MULTIPLIER,
	TCVAR_ARTIFICIAL_INT_AMBIENT_MULTIPLIER,
	TCVAR_FOG_CUT_OFF,
	TCVAR_NO_WEATHER_FX,
	TCVAR_NO_GPU_FX,
	TCVAR_NO_RAIN,
	TCVAR_NO_RAIN_RIPPLES,
	TCVAR_FOGVOLUME_DENSITY_SCALAR,
	TCVAR_FOGVOLUME_DENSITY_SCALAR_INTERIOR,
	TCVAR_FOGVOLUME_FOG_SCALER,
	TCVAR_TIME_OFFSET,
	TCVAR_VEHICLE_DIRT_MOD,
	TCVAR_WIND_SPEED_MULT,
	TCVAR_ENTITY_REJECT,
	TCVAR_LOD_MULT,
	TCVAR_ENABLE_OCCLUSION,
	TCVAR_ENABLE_SHADOW_OCCLUSION,
	TCVAR_ENABLE_EXTERIOR,
	TCVAR_PORTAL_WEIGHT,

	TCVAR_LIGHT_FALLOFF_MULT,
	TCVAR_LODLIGHT_RANGE_MULT,
	TCVAR_SHADOW_DISTANCE_MULT,
	
	TCVAR_LOD_MULT_HD,
	TCVAR_LOD_MULT_ORPHANHD,
	TCVAR_LOD_MULT_LOD,
	TCVAR_LOD_MULT_SLOD1,
	TCVAR_LOD_MULT_SLOD2,
	TCVAR_LOD_MULT_SLOD3,
	TCVAR_LOD_MULT_SLOD4,

	// must be last
	TCVAR_NUM  // must match TIMECYCLE_VAR_COUNT in tckeyframe.h
};

CompileTimeAssert(TCVAR_NUM == TIMECYCLE_VAR_COUNT);

///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////
class CTimeCycle;

extern	ALIGNAS(32) CTimeCycle	g_timeCycle; // NB: CTimeCycle implicitly aligned to 32 bytes because of member frameInfo struct
extern	tcVarInfo	g_varInfos[];

///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

//  CTimeCycle ////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
#define TCVALID_UPDATE() \
{\
	Assertf(m_IsTimeCycleValid == true,"Using invalid Timecycle");\
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));\
}

#define TCVALID_RENDER() \
{\
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));\
}

class CTimeCycle : public tcManager
{	
	///////////////////////////////////
	// FRIENDS 
	///////////////////////////////////

	friend class CTimeCycleDebug;
	friend class dlCmdSetupTimeCycleFrameInfo;
	friend class dlCmdClearTimeCycleFrameInfo;

	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////
		class portalData
		{
			public:
			void Reset() 
			{ 
				screenArea = 0.0f;
				mainModifierIdx = -1;
				secondaryModifierIdx = -1;
				inUse = false;
			}

			void SetCorners(const fwPortalCorners &c) { corners = c; }
			const fwPortalCorners &GetCorners() const { return corners;}
			
			void SetScreenArea(float sa) { screenArea = sa; }
			float GetScreenArea() const { return screenArea;}

			void SetDoorOcclusion(float doc) { doorOcclusion = doc; }
			float GetDoorOcclusion() const { return doorOcclusion;}
			
			void SetMainModifierIdx(int mi) { mainModifierIdx = mi; }
			int GetMainModifierIdx() const { return mainModifierIdx;}
			
			void SetSecondaryModifierIdx(int si) { secondaryModifierIdx = si; }
			int GetSecondaryModifierIdx() const { return secondaryModifierIdx;}
			
			void SetInUse(bool iu) { inUse = iu; }
			bool GetInUse() const { return inUse;}

			void SetExpandBox(bool eb) { expandBox = eb; }
			bool GetExpandBox() const { return expandBox;}
			
			private:

			fwPortalCorners corners;
			float screenArea;
			float doorOcclusion;
			int mainModifierIdx;
			int secondaryModifierIdx;
			bool inUse;
			bool expandBox;
			
		};
		
		typedef atFixedArray<portalData, MAX_LOCAL_PORTAL> portalList;
		
		struct ALIGNAS(32) frameInfo {
			tcKeyframeUncompressed	m_keyframe;
			Vec4V					m_vBaseDirectionalLight;

			// sun info
			Vec3V					m_vSunSpriteDir;
			Vec3V					m_vMoonSpriteDir;
			Vec3V					m_vDirectionalLightDir;
			float					m_directionalLightIntensity;
			float					m_sunRollAngle;
			float					m_sunYawAngle;
			float					m_moonRollOffset;
			float					m_sunFade;
			float					m_moonFade;
			float					m_moonWobbleFrequency;
			float					m_moonWobbleAmplitude;
			float					m_moonWobbleOffset;
			float					m_moonCycleOverride;
			double					m_cycleTime;

			// day night info
			float					m_dayNightBalance;
			
			// Lod Scales
			float					m_GlobalLodScale;
			float					m_GrassLodScale;

			Vec3V					m_LightningDirection;									// used only in overriding timecycle settings for lightning
			bool					m_LightningOverride;									// used only in overriding timecycle settings for lightning
			Color32					m_LightningColor;										// used only in overriding timecycle settings for lightning
			Color32					m_LightningFogColor;									// used only in overriding timecycle settings for lightning
			
			// Portals
			portalList				m_LocalPortalCache;
		};

		static void			InitDLCCommands				();
		void 				Init						(unsigned initMode);
		void 				Shutdown					(unsigned shutdownMode);
		void 				Update						(bool finalUpdate);
		static void			UpdateWrapper()				{ g_timeCycle.Update(true); }
		
#if __ASSERT
		void				InvalidateTimeCycle			() { m_IsTimeCycleValid = false; }
#endif // __ASSERT
#if TC_DEBUG
		void				InitLevelWidgets			();
		void				ShutdownLevelWidgets		();
		void 				Render						();
#endif

		static void			SetShaderParams				();
		void 				SetShaderData				(CVisualEffects::eRenderMode mode = CVisualEffects::RM_DEFAULT);

		void				LoadModifierFiles			();
		void				AppendModifierFile			(const char* filename);
		void				RevertModifierFile			(const char* filename);
		void 				LoadVisualSettings			();

		// per entity ambient scale
		Vec2V_Out			CalcAmbientScale			(Vec3V_In vPos, fwInteriorLocation intLoc);

		// interior functions
		bool				CalcModifierAmbientScale	(const int timeCycleIndex, const int secondaryTimeCycleIndex, float blend, float &natAmbientScale, float &artAmbientScale ASSERT_ONLY(, const atHashString timeCycleName, const atHashString secondaryTimeCycleName));
		
		void				AddLocalPortal(const fwPortalCorners &portal, float area, float doorOcclusion, int mainModifierIdx, int secondaryModifierIdx);
		void				PreSetInteriorAmbientCache(const atHashString name);

		// script functions
		void 				SetScriptModifierId			(const atHashString name)	{m_scriptModifierIndex = FindModifierIndex(name,"SetScriptModifierId"); m_transitionScriptModifierIndex = -1;}
		void 				SetScriptModifierId			(int index)					{m_scriptModifierIndex = index; m_transitionScriptModifierIndex = -1;}
		int					GetScriptModifierIndex		()							{return m_scriptModifierIndex;}
		int					GetTransitionScriptModifierIndex()						{return m_transitionScriptModifierIndex;}
		
		void 				ClearScriptModifierId		()							{m_scriptModifierIndex = -1; m_transitionScriptModifierIndex = -1; m_scriptModifierStrength = 1.0f; m_transitionOut = false;}
		void				SetScriptModifierStrength	(float strength)			{m_scriptModifierStrength = strength; } 
		void				PushScriptModifier();
		void				ClearPushedScriptModifier();
		void				PopScriptModifier();
		
		void 				SetExtraModifierId			(const atHashString name)	{m_extraModifierIndex = FindModifierIndex(name,"SetExtraModifierId");}
		void 				SetExtraModifierId			(int index)					{m_extraModifierIndex = index;}
		int					GetExtraModifierIndex		()							{return m_extraModifierIndex;}
		void 				ClearExtraModifierId		()							{m_extraModifierIndex = -1;}

		void				SetTransitionToScriptModifierId(const atHashString name, float time);
		void				SetTransitionToScriptModifierId(int index, float time);
		void				SetTransitionOutOfScriptModifierId(float time);

		bool				GetIsTransitioningOut() const							{ return m_transitionOut; }

		// player modifier
		void				SetPlayerModifierIdCurrent	(const atHashString name)	{m_playerModifierIndexCurrent = FindModifierIndex(name,"SetPlayerModifierIdCurrent");}
		void				SetPlayerModifierIdCurrent	(int idx)					{m_playerModifierIndexCurrent = idx;}
		void				SetPlayerModifierIdNext		(const atHashString name)	{m_playerModifierIndexNext = FindModifierIndex(name,"SetPlayerModifierIdNext");}
		void				SetPlayerModifierIdNext		(int idx)					{m_playerModifierIndexNext = idx;}
		void				SetPlayerModifierTransitionStrength(float f)			{Assert((f>=0.0f)&&(f<=1.0f)); m_playerModifierStrength = f;}

		int					GetPlayerModifierIdCurrent	()							{ return m_playerModifierIndexCurrent;}
		int					GetPlayerModifierIdNext		()							{ return m_playerModifierIndexNext;}
		float				GetPlayerModifierTransitionStrength()					{ return m_playerModifierStrength; }

		// default lighting
		void				SetUseDefaultKeyframe		(const bool val)			{m_useDefaultKeyframe = val;}
		void				SetLongShadowsEnabled		(const bool val)			{m_longShadowsEnabled = val;}

#if PLAYERSWITCH_FORCES_LIGHTDIR
		// dir light override
		void				SetDirLightOverride(Vec3V_In light) { m_codeOverrideDirLightOrientation = light; }
		void				SetDirLightOverrideStrength(float strength) { m_codeOverrideDirLightOrientationStrength = strength; }
#endif // PLAYERSWITCH_FORCES_LIGHTDIR

		// sky settings
		void				GetSkySettings				(CSkySettings* pSkySettings);
		void				GetMinimalSkySettings		(CSkySettings* pSkySettings);
		void				GetNoiseSkySettings			(CSkySettings* pSkySettings);

		// access functions 
		void				SetupRenderThreadFrameInfo();
		void				ClearRenderFrameInfo();

		//Lightning settigs
		bool				GetLightningOverrideRender		() const				{ TCVALID_RENDER(); return m_currFrameInfo->m_LightningOverride; }
		const Color32&		GetLightningColorRender			() const				{ TCVALID_RENDER(); return m_currFrameInfo->m_LightningColor; }
		const Color32		GetLightningFogColorRender		() const				{ TCVALID_RENDER(); return m_currFrameInfo->m_LightningFogColor; }
		Vec3V_ConstRef		GetLightningDirectionRender		() const				{ TCVALID_RENDER(); return m_currFrameInfo->m_LightningDirection; }

		bool				GetLightningOverrideUpdate		() const				{ TCVALID_UPDATE(); return m_frameInfo.m_LightningOverride; }
		const Color32&		GetLightningColorUpdate			() const				{ TCVALID_UPDATE(); return m_frameInfo.m_LightningColor; }
		const Color32		GetLightningFogColorUpdate		() const				{ TCVALID_UPDATE(); return m_frameInfo.m_LightningFogColor; }
		Vec3V_ConstRef		GetLightningDirectionUpdate		() const				{ TCVALID_UPDATE(); return m_frameInfo.m_LightningDirection; }

		void				SetLightningOverride			(bool value) 			{ m_frameInfo.m_LightningOverride = value; }
		void				SetLightningColor				(const Color32& color)	{ m_frameInfo.m_LightningColor = color; }
		void				SetLightningFogColor			(const Color32& color)	{ m_frameInfo.m_LightningFogColor = color; }
		void				SetLightningDirection			(Vec3V_In direction)	{ m_frameInfo.m_LightningDirection = direction; }
		

		// Prev frame access functions, because sometime, we don't care.
		Vec4V_Out			GetStaleBaseDirLight			() const				{ return m_frameInfo.m_vBaseDirectionalLight; }

		float				GetStaleTemperature				() const				{ return m_frameInfo.m_keyframe.GetVar(TCVAR_TEMPERATURE); }
		float				GetStaleDirtModifier			() const				{ return m_frameInfo.m_keyframe.GetVar(TCVAR_VEHICLE_DIRT_MOD); }
		float				GetStaleNoWeatherFX  			() const				{ return m_frameInfo.m_keyframe.GetVar(TCVAR_NO_WEATHER_FX); }
		float				GetStaleNoGpuFX  				() const				{ return m_frameInfo.m_keyframe.GetVar(TCVAR_NO_GPU_FX); }
		float				GetStaleNoRain  				() const				{ return m_frameInfo.m_keyframe.GetVar(TCVAR_NO_RAIN); }
		float				GetStaleNoRainRipples			() const				{ return m_frameInfo.m_keyframe.GetVar(TCVAR_NO_RAIN_RIPPLES); }
		float				GetStaleFogVolumeDensityScalar	() const				{ return m_frameInfo.m_keyframe.GetVar(TCVAR_FOGVOLUME_DENSITY_SCALAR); }
		float				GetStaleFogVolumeDensityScalarInterior() const			{ return m_frameInfo.m_keyframe.GetVar(TCVAR_FOGVOLUME_DENSITY_SCALAR_INTERIOR); }
		float				GetStaleFogVolumeFogScalar		() const				{ return m_frameInfo.m_keyframe.GetVar(TCVAR_FOGVOLUME_FOG_SCALER); }
		float				GetStaleFarClip					() const				{ return m_frameInfo.m_keyframe.GetVar(TCVAR_FAR_CLIP); }
		float				GetStaleInteriorBlendStrength	() const				{ return m_interiorBlendStrength;}
		float				GetStaleEntityReject			() const				{ return m_frameInfo.m_keyframe.GetVar(TCVAR_ENTITY_REJECT) ; }

		float				GetStaleEnableOcclusion			() const				{ return m_frameInfo.m_keyframe.GetVar(TCVAR_ENABLE_OCCLUSION); }
		float				GetStaleEnableShadowOcclusion	() const				{ return m_frameInfo.m_keyframe.GetVar(TCVAR_ENABLE_SHADOW_OCCLUSION); }
		float				GetStaleRenderExterior			() const				{ return m_frameInfo.m_keyframe.GetVar(TCVAR_ENABLE_EXTERIOR); }
		
		float				GetStaleSpriteBrightness		() const				{ return m_frameInfo.m_keyframe.GetVar(TCVAR_SPRITE_BRIGHTNESS); }
		float				GetStaleSpriteSize				() const				{ return m_frameInfo.m_keyframe.GetVar(TCVAR_SPRITE_SIZE); }

		float				GetStaleWaterReflection			() const				{ return m_frameInfo.m_keyframe.GetVar(TCVAR_WATER_REFLECTION); }

		float				GetStaleDayNightBalance			() const				{ return m_frameInfo.m_dayNightBalance; }

		const tcKeyframeUncompressed& GetCurrUpdateKeyframe	() const				{TCVALID_UPDATE(); return m_frameInfo.m_keyframe;}
		Vec4V_ConstRef		GetBaseDirLight				() const					{TCVALID_UPDATE(); return m_frameInfo.m_vBaseDirectionalLight;}
		Vec3V_ConstRef		GetDirectionalLightDirection() const					{TCVALID_UPDATE(); return m_frameInfo.m_vDirectionalLightDir;}
		float				GetDirectionalLightIntensity() const					{TCVALID_UPDATE(); return m_frameInfo.m_directionalLightIntensity;}
		Vec3V_ConstRef		GetSunSpriteDirection		() const					{TCVALID_UPDATE(); return m_frameInfo.m_vSunSpriteDir;}
		Vec3V_ConstRef		GetMoonSpriteDirection		() const					{TCVALID_UPDATE(); return m_frameInfo.m_vMoonSpriteDir;}

		float				GetSunRollAngle				() const					{TCVALID_UPDATE(); return m_frameInfo.m_sunRollAngle;}
		float				GetSunYawAngle				() const					{TCVALID_UPDATE(); return m_frameInfo.m_sunYawAngle;}
		float				GetMoonRollOffset			() const					{TCVALID_UPDATE(); return m_frameInfo.m_moonRollOffset;}
		float				GetDayNightBalance			() const					{TCVALID_UPDATE(); return m_frameInfo.m_dayNightBalance;}
		float				GetMoonWobbleFrequency		() const					{TCVALID_UPDATE(); return m_frameInfo.m_moonWobbleFrequency;}
		float				GetMoonWobbleAmplitude		() const					{TCVALID_UPDATE(); return m_frameInfo.m_moonWobbleAmplitude;}
		float				GetMoonWobbleOffset			() const					{TCVALID_UPDATE(); return m_frameInfo.m_moonWobbleOffset;}
		float				GetMoonCycleOverride		() const					{TCVALID_UPDATE(); return m_frameInfo.m_moonCycleOverride;}
		float				GetSunFade					() const					{TCVALID_UPDATE(); return m_frameInfo.m_sunFade;}
		float				GetMoonFade					() const					{TCVALID_UPDATE(); return m_frameInfo.m_moonFade;}
		double				GetCycleTime				() const					{TCVALID_UPDATE(); return m_frameInfo.m_cycleTime;}
		float				GetLodMult					() const					{TCVALID_UPDATE(); return m_frameInfo.m_keyframe.GetVar(TCVAR_LOD_MULT); }
		float				GetLodScale_Hd				() const					{TCVALID_UPDATE(); return m_frameInfo.m_keyframe.GetVar(TCVAR_LOD_MULT_HD); }
		float				GetLodScale_OrphanHd		() const					{TCVALID_UPDATE(); return m_frameInfo.m_keyframe.GetVar(TCVAR_LOD_MULT_ORPHANHD); }
		float				GetLodScale_Lod				() const					{TCVALID_UPDATE(); return m_frameInfo.m_keyframe.GetVar(TCVAR_LOD_MULT_LOD); }
		float				GetLodScale_Slod1			() const					{TCVALID_UPDATE(); return m_frameInfo.m_keyframe.GetVar(TCVAR_LOD_MULT_SLOD1); }
		float				GetLodScale_Slod2			() const					{TCVALID_UPDATE(); return m_frameInfo.m_keyframe.GetVar(TCVAR_LOD_MULT_SLOD2); }
		float				GetLodScale_Slod3			() const					{TCVALID_UPDATE(); return m_frameInfo.m_keyframe.GetVar(TCVAR_LOD_MULT_SLOD3); }
		float				GetLodScale_Slod4			() const					{TCVALID_UPDATE(); return m_frameInfo.m_keyframe.GetVar(TCVAR_LOD_MULT_SLOD4); }
		float				GetDirectionalLightMult		() const					{TCVALID_UPDATE(); return m_frameInfo.m_keyframe.GetVar(TCVAR_LIGHT_DIR_MULT); }
		float				GetMotionBlurLength			() const					{TCVALID_UPDATE(); return m_frameInfo.m_keyframe.GetVar(TCVAR_POSTFX_MOTIONBLUR_LENGTH); }
		void				SetSunRollAngle				(float roll)				{m_frameInfo.m_sunRollAngle = roll;}
		void				SetSunYawAngle				(float yaw)					{m_frameInfo.m_sunYawAngle = yaw;}
		void				SetMoonRollOffset			(float offset)				{m_frameInfo.m_moonRollOffset = offset;}
		void				SetMoonWobbleFrequency		(float freq)				{m_frameInfo.m_moonWobbleFrequency = freq;}
		void				SetMoonWobbleAmplitude		(float amp)					{m_frameInfo.m_moonWobbleAmplitude = amp;}
		void				SetMoonWobbleOffset			(float offset)				{m_frameInfo.m_moonWobbleOffset = offset;}
		void				SetMoonCycleOverride		(float amount)				{m_frameInfo.m_moonCycleOverride = amount;}
		void				SetCycleTime				(double time)				{m_frameInfo.m_cycleTime = time;}

		void				SetDayNightBalance			(float balance)				{m_frameInfo.m_dayNightBalance = balance;}

		const tcKeyframeUncompressed& 	GetCurrRenderKeyframe	() const			{TCVALID_RENDER(); return m_currFrameInfo->m_keyframe;}
		Vec4V_ConstRef		GetCurrBaseDirLight			() const					{TCVALID_RENDER(); return m_currFrameInfo->m_vBaseDirectionalLight;}
		float				GetCurrSunFade				() const					{TCVALID_RENDER(); return m_currFrameInfo->m_sunFade;}
		float				GetCurrMoonFade				() const					{TCVALID_RENDER(); return m_currFrameInfo->m_moonFade;}
		Vec3V_ConstRef		GetCurrMoonSpriteDirection	() const					{TCVALID_RENDER(); return m_currFrameInfo->m_vMoonSpriteDir;}
		Vec3V_ConstRef		GetCurrSunSpriteDirection	() const					{TCVALID_RENDER(); return m_currFrameInfo->m_vSunSpriteDir;}
		float				GetCurrDayNightBalance		() const					{TCVALID_RENDER(); return m_currFrameInfo->m_dayNightBalance;}
		float				GetCurrMoonCycleOverride	() const					{TCVALID_RENDER(); return m_currFrameInfo->m_moonCycleOverride;}
		double				GetCurrCycleTime			() const					{TCVALID_RENDER(); return m_currFrameInfo->m_cycleTime;}
		const  portalList&	GetLocalPortalList			() const					{TCVALID_RENDER(); return m_currFrameInfo->m_LocalPortalCache;}
		float				GetCurrNoGpuFX  			() const					{TCVALID_RENDER(); return m_currFrameInfo->m_keyframe.GetVar(TCVAR_NO_GPU_FX); }

		float				CalculateDayNightBalance	() const;

		void				ResetRegionInfo				()							{m_numRegions = 0;}
		void				AddRegionInfo				(const char* regionName)			{BANK_ONLY(strcpy(m_regionInfo[m_numRegions].name, regionName);) m_regionInfo[m_numRegions].hashName = atStringHash(regionName); m_numRegions++;}
		void				SetRegionOverride			(s32 region)				{Assert(region < m_numRegions); m_regionOverride = region;}
		int					GetRegionOverride			()							{ return m_regionOverride; }

#if GTA_REPLAY

		struct TCNameAndValue
		{
			u32		nameHash;
			float	value;
		};

		u32					GetReplayModifierHash		()							{ return m_ReplayModifierHash; }
		void				SetReplayModifierHash (u32  const modifierHash, float const intensity )					
		{
			m_ReplayModifierHash = modifierHash;
			m_ReplayModifierIntensity = intensity;
		}


		u32					GetTimeCycleKeyframeData(TCNameAndValue *pData, bool getDefaultData = false);
		void				SetTimeCycleKeyframeData(const TCNameAndValue *pData, u32 count, bool isSniperScope, bool isDefaultData = false);

		// Check if replay is active
		bool				ShouldUseReplayTCData();
		bool				ShouldApplyInteriorModifier() const;
		bool				ShouldDisableScripFxForFreeCam(u32 scriptFx, u32 transistionFx) const;
		bool				ShouldDisableScripFxForGameCam(u32 scriptFx, u32 transistionFx) const;

		void				RecordTimeCycleDataForReplay(eTimeCyclePhase timeCyclePhase);

		void				EnableReplayExtraEffects	() { m_bReplayEnableExtraFX = true; }
		void				DisableReplayExtraEffects	() { m_bReplayEnableExtraFX = false; }
		bool				AreReplayExtraEffectsEnabled() const { return m_bReplayEnableExtraFX; }

		void				SetReplaySaturationIntensity(float intensity) { m_ReplaySaturationIntensity = intensity; }
		void				SetReplayVignetteIntensity	(float intensity) { m_ReplayVignetteIntensity = intensity; }
		void				SetReplayContrastIntensity	(float intensity) { m_ReplayContrastIntensity = intensity; }
		void				SetReplayBrightnessIntensity(float intensity) { m_ReplayBrightnessIntensity = intensity; }

		float				GetReplaySaturationIntensity() const { return m_ReplaySaturationIntensity; }
		float				GetReplayVignetteIntensity	() const { return m_ReplayVignetteIntensity; }
		float				GetReplayContrastIntensity	() const { return m_ReplayContrastIntensity; }
		float				GetReplayBrightnessIntensity() const { return m_ReplayBrightnessIntensity; }
		float				GetReplayEffectIntensity() const { return m_ReplayModifierIntensity; }

#endif //GTA_REPLAY
		// access functions - debug
#if TC_DEBUG
inline	const	CTimeCycleDebug&	GetGameDebug		() const					{return m_gameDebug;}
inline			CTimeCycleDebug&	GetGameDebug		()							{return m_gameDebug;}
#endif // TC_DEBUG

		// access functions - visual settings - should not be part of this class
inline	float				GetSpecularOffset			() const					{return m_vsSpecularOffset;}		// used only in lights.cpp
	#if __ASSERT
		bool				CanAccess					() const					{ return m_IsTimeCycleValid; }
	#endif
	private: ///////////////////////////

		// keyframe calculation functions
		void				CalcBaseKeyframe			(tcKeyframeUncompressed& baseKeyframe, const u32 hours, const u32 minutes, const u32 seconds, const u32 prevCycleHashName, const u32 nextCycleHashName, const float cycleInterp);
		void				UpdateUnderWater			(tcKeyframeUncompressed& keyframe, const u32 hours, const u32 minutes, const u32 seconds);
		void				CalcSunLightDir				();
		void				CalcDirectionalLightandPos	(Vec3V_InOut vDirectionalDir, float &directionalInten, Vec3V_InOut vSunDir, Vec3V_InOut vMoonDir, float hours, float sunSlope, float moonSlope, float sunYaw, float &sunFade, float &moonFade);
		void				ApplyCutsceneModifiers		(tcKeyframeUncompressed& keyframe);
		void				ApplyCutsceneCameraOverrides(tcKeyframeUncompressed& keyframe);
		void				ApplyBoxModifiers			(tcKeyframeUncompressed& keyframe, float alpha, bool interiorOnly, bool underWaterOnly);
		void				ApplyUnderwaterModifiers	(tcKeyframeUncompressed& keyframe);
		bool				ApplyInteriorModifiers		(tcKeyframeUncompressed& keyframe, bool isUnderWater, bool finalUpdate);
		void				ApplyGlobalModifiers		(tcKeyframeUncompressed& keyframe, const bool finalUpdate REPLAY_ONLY(, const eTimeCyclePhase replayPass));
		void				ApplyLodScaleModifiers		(WIN32PC_ONLY(tcKeyframeUncompressed& keyframe));
#if GTA_REPLAY
		void				ApplyReplayEditorModifiers	(tcKeyframeUncompressed& keyframe);
		void				ApplyReplayExtraEffects		(tcKeyframeUncompressed& keyframe);	
		void				ApplyModifierWithNegativeRange	(tcKeyframeUncompressed& keyframe, const tcModifier *negMod, const tcModifier *posMod, float alpha, bool linearize);
#endif	//GTA_REPLAY
		void				ApplyVisualSettings			(tcKeyframeUncompressed& keyframe, Vec3V_In vCameraPos) const;
		Vec2V_Out			GetModifierAmbientScale		(const tcModifier *mod, bool &validNat, bool &validArt);
		Vec2V_Out			GetModifierAmbientScaleWithDefault (const tcModifier *mod);
		
		void				SetInteriorCache(int mainTcIdx, int secondaryTcIdx);

	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////

	private: //////////////////////////

#if GTA_REPLAY
		struct EffectTableEntry
		{
			atHashString m_Name;

			const bool operator == (const u32 & hash) const
			{
				return m_Name.GetHash() == hash;
			}
		};

		bool					m_bReplaySniperScope;
		tcKeyframeUncompressed	m_ReplayTCFrame;				// Dual function:-
																// In recording, is used to store the last keyframe that didn't have a weaponwheel or character select PostFX on it.
																// In playback, is used to store the data coming back out of the replay packet data.

		tcKeyframeUncompressed	m_ReplayDefaultTCFrame;			//Used to store tc data with out specified fx
		bool					m_bUsingDefaultTCData;			//Is the default data currently applied

		u32						m_ReplayModifierHash;
		float					m_ReplayModifierIntensity;

		bool					m_bReplayEnableExtraFX;
		float					m_ReplaySaturationIntensity;
		float					m_ReplayVignetteIntensity;
		float					m_ReplayContrastIntensity;
		float					m_ReplayBrightnessIntensity;

		static EffectTableEntry	m_InteriorReplayLookupTable[];

		static EffectTableEntry	m_ScriptEffectFreeCamRemovalReplayLookupTable[];
		static EffectTableEntry	m_ScriptEffectAllCamsRemovalReplayLookupTable[];

		atMap<u32,u16>			m_EffectsMap;

		tcKeyframeUncompressed	g_KeyframeCachedForReplayJump;
#endif

		frameInfo			m_frameInfo;
		static frameInfo *m_currFrameInfo;

		// region info 
		s32					m_numRegions;
		s32					m_regionOverride;
		tcRegionInfo		m_regionInfo				[TIMECYCLE_MAX_REGIONS];

		// modifier overrides
		s32					m_scriptModifierIndex;
		float				m_scriptModifierStrength;
		s32					m_transitionScriptModifierIndex;
		float				m_transitionStrength;
		float				m_transitionSpeed;
		bool				m_transitionOut;
		
		s32					m_pushedScriptModifierIndex;
		s32					m_pushedTransitionScriptModifierIndex;
		float				m_pushedTransitionStrength;
		float				m_pushedTransitionSpeed;
		bool				m_pushedTransitionOut;
		bool				m_pushedScriptModifier;
		
		// extra modifier
		s32					m_extraModifierIndex;

		// player specific modifiers.
		s32					m_playerModifierIndexCurrent;
		s32					m_playerModifierIndexNext;
		float				m_playerModifierStrength;
		
		// Cutscene
		s32					m_cutsceneModifierIndex;

		// interiors
		float				m_interiorBlendStrength;
		bool				m_interiorInitialised;
		bool				m_interiorApply;
		bool				m_interiorReset;
		float				m_interior_down_color_r;
		float 				m_interior_down_color_g;
		float 				m_interior_down_color_b;
		float 				m_interior_down_intensity;
		s32					m_interior_main_idx;
		
		float				m_interior_up_color_r;
		float				m_interior_up_color_g;
		float				m_interior_up_color_b;
		float				m_interior_up_intensity;
		s32					m_interior_secondary_idx;

		// flags
		bool				m_useDefaultKeyframe;
		static bool			m_longShadowsEnabled;
		
#if PLAYERSWITCH_FORCES_LIGHTDIR
		// Code side directional light override
		Vec3V				m_codeOverrideDirLightOrientation;
		float				m_codeOverrideDirLightOrientationStrength;
#endif // PLAYERSWITCH_FORCES_LIGHTDIR

		// visual settings
		float 				m_vsFarClipMultiplier;
		float 				m_vsNearFogMultiplier;
		float 				m_vsMultiplierHeightStart;
		float 				m_vsOoMultiplierRange;
		float				m_vsRimLight;
		float				m_vsGlobalEnvironmentReflection;
		float 				m_vsGamma;
		float 				m_vsMidBlur;
		float 				m_vsFarBlur;
		float 				m_vsSkyMultiplier;
		float 				m_vsDesaturation;
		float				m_vsDOFblurScale;
		float				m_vsCutsceneNearBlurMin;
		float				m_vsMoonDimMult;
		float				m_vsNextGenModifer;

		// visual settings - should not be part of this class
		float 				m_vsSpecularOffset;										// used only in lights.cpp


#if __ASSERT
		bool				m_IsTimeCycleValid;
#endif // __ASSERT		
		// debug
#if TC_DEBUG
		CTimeCycleDebug		m_gameDebug;
#endif

};

// wrapper class needed to interface with game skeleton code
// class CTimeCycleWrapper
// {
// public:
// 
//     static void Init(unsigned initMode);
// };


class ShadowFreezing
{

	Vector3					m_PrimaryShadowLightDir;
	Vector3					m_PrevCamPos;
	Vector3					m_PrevPlayerPos;
	Vector3					m_PlayerPos;
	float					m_CameraRotationCatchupScale;
	float					m_CameraTranslationCatchupScale;
	float					m_CameraMovementCatcupScale;
	float					m_MaxAngleOffset;
	float					m_AngleMovementAcceleration;
	float					m_PrevAngleMovement;
	bool					m_PlayerActive;
	bool					m_firstFrame;
	float					m_prevHeading;
	bool m_inited;
	bool m_isEnabled;
	BANK_ONLY(bool m_isCurrentlyFrozen;)
public:
	ShadowFreezing();

	inline bool IsEnabled() const
	{
		return m_isEnabled;
	}

	inline bool SetEnabled(bool bEnabled)
	{
		return m_isEnabled = bEnabled;
	}

	Vec3V_Out Update(const Vector3& trueLightDir, const Vector3& camPos, bool isCut, float heading, const Vector3& playerP, bool playerActive, bool iscutscene);
#if __BANK
	void AddWidgets(bkBank & bank);
#endif

};


#endif // TIMECYCLE_H



