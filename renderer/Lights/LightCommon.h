//
// name: LightCommon.h

#ifndef INC_LIGHTCOMMON_H_
#define INC_LIGHTCOMMON_H_ 

#if defined(__GTA_COMMON_FXH)
	#define LIGHT_TYPE_NONE        0
	#define LIGHT_TYPE_POINT       1
	#define LIGHT_TYPE_SPOT        2
	#define LIGHT_TYPE_CAPSULE     4
	#define LIGHT_TYPE_DIRECTIONAL 8
	#define LIGHT_TYPE_AO_VOLUME   16
#else
enum eLightType
{
	LIGHT_TYPE_NONE        = 0,
	LIGHT_TYPE_POINT       = (1<<0),
	LIGHT_TYPE_SPOT        = (1<<1),
	LIGHT_TYPE_CAPSULE     = (1<<2),
	LIGHT_TYPE_DIRECTIONAL = (1<<3),
	LIGHT_TYPE_AO_VOLUME   = (1<<4),
};
#endif

#define LIGHTVOLUME_PARAM_COUNT 2
#define LIGHT_PARAM_COUNT 14

#define LIGHT_VOLUME_USE_LOW_RESOLUTION (1 && (RSG_DURANGO || RSG_ORBIS || RSG_PC))

#if LIGHT_VOLUME_USE_LOW_RESOLUTION
	#define LOW_RES_VOLUME_LIGHT_ONLY(x)	x
	#define LOW_RES_VOLUME_LIGHT_SWITCH(_if_LOWRES_,_else_) _if_LOWRES_
#else
	#define LOW_RES_VOLUME_LIGHT_ONLY(x)
	#define LOW_RES_VOLUME_LIGHT_SWITCH(_if_LOWRES_,_else_) _else_
#endif


#define LIGHTSHAFT_USE_SHADOWS (1)

#if defined(__GTA_COMMON_FXH)

// these are defined in MapData_Extensions.h but we can't include that in shader code
#define LIGHTSHAFT_VOLUMETYPE_SHAFT               0
#define LIGHTSHAFT_VOLUMETYPE_CYLINDER            1

// these are defined in MapData_Extensions.h but we can't include that in shader code
#define LIGHTSHAFT_DENSITYTYPE_CONSTANT           0
#define LIGHTSHAFT_DENSITYTYPE_SOFT               1
#define LIGHTSHAFT_DENSITYTYPE_SOFT_SHADOW        2
#define LIGHTSHAFT_DENSITYTYPE_SOFT_SHADOW_HD     3
#define LIGHTSHAFT_DENSITYTYPE_LINEAR             4
#define LIGHTSHAFT_DENSITYTYPE_LINEAR_GRADIENT    5
#define LIGHTSHAFT_DENSITYTYPE_QUADRATIC          6
#define LIGHTSHAFT_DENSITYTYPE_QUADRATIC_GRADIENT 7

#else

__forceinline float CalculateTimeFadeCommon(const u32 timeFlags, const u32 hour, const u32 nextHour, const float fade)
{
	// Light is off now but will turn on in the next hour
	if ((timeFlags & (1 << (nextHour))) != 0 &&
		(timeFlags & (1 << hour)) == 0)
	{
		return Saturate(fade - 59.0f);
	}

	// Light is on but will switch off in the next hour
	if ((timeFlags & (1 << hour)) != 0 &&
		(timeFlags & (1 << (nextHour))) == 0)
	{
		return(1.0f - Saturate(fade - 59.0f));
	}

	// If light is on, make sure its on
	if (timeFlags & (1 << hour))
	{
		return 1.0;
	}

	return 0.0f;
}
#endif // defined(__GTA_COMMON_FXH)

#endif // INC_LIGHTCOMMON_H_
