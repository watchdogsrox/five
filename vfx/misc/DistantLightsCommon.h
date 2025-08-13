///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	DistantLights2.h
//	BY	: 	Thomas Randall (Adapted from original by Mark Nicholson)
//	FOR	:	Rockstar North
//	ON	:	11 Jun 2013
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef DISTANTLIGHTS_COMMON_H
#define DISTANTLIGHTS_COMMON_H

#include "grcore/config.h"
#include "../vfx_shared.h"
#include "vectormath/classes.h"

#include "profile/element.h"
#include "profile/page.h"
#include "system/dependency.h"
#include "vector/color32.h"
#include "vectormath/scalarv.h"
#include "vectormath/vec3v.h"

///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

// Common Constants.
#define MAX_POINTS_PER_GROUP				(32)
#define MIN_POINTS_PER_GROUP				(4)

// Constants used in the original distant lights code.
#define DISTANTLIGHTS_MAX_BLOCKS				(16)
#define DISTANTLIGHTS_MAX_GROUPS				(600)
#define DISTANTLIGHTS_MAX_POINTS				(6000)
#define DISTANTLIGHTS_MAX_LINE_DATA				(MAX_POINTS_PER_GROUP + 6)
#define DISTANTLIGHTS2_MAX_COLOR_LIGHT_LIMIT	(1500)

// New distant lights constants.
#define DISTANTLIGHTS2_MAX_BLOCKS			(32)
#define DISTANTLIGHTS2_MAX_GROUPS			(2000)
#define DISTANTLIGHTS2_MAX_POINTS			(9000)
#define DISTANTLIGHTS2_MAX_LINE_DATA		(MAX_POINTS_PER_GROUP + 6)
#define DISTANTLIGHTS2_NUM_SPEED_GROUPS		(4)

#define	DISTANTLIGHTS_NUM_RANDOM_VALUES		(16)

#define	DISTANTLIGHTS_BLOCK_RADIUS			(0.7f * ((WORLDLIMITS_REP_XMAX-WORLDLIMITS_REP_XMIN)/DISTANTLIGHTS_MAX_BLOCKS))

// Runtime constants (based on the current type of distant lights).
#if USE_DISTANT_LIGHTS_2
#define DISTANTCARS_MAX						(6000)		// Looking at the city from a high vantage point, I'm seeing no more that 2,500 distant cars at the moment
#define DISTANTLIGHTS_CURRENT_MAX_BLOCKS	(DISTANTLIGHTS2_MAX_BLOCKS)
#define DISTANTLIGHTS_CURRENT_MAX_GROUPS	(DISTANTLIGHTS2_MAX_GROUPS)

#else
#if __D3D11 || RSG_ORBIS || __XENON || __PS3
#define DISTANTCARS_MAX						(3000)		// Looking at the city from a high vantage point, I'm seeing no more that 2,500 distant cars at the moment
#endif
#define DISTANTLIGHTS_CURRENT_MAX_BLOCKS	(DISTANTLIGHTS_MAX_BLOCKS)
#define DISTANTLIGHTS_CURRENT_MAX_GROUPS	(DISTANTLIGHTS_MAX_GROUPS)
#endif

#define ENABLE_DISTANTLIGHTS_PROFILING		(0)

#if ENABLE_DISTANTLIGHTS_PROFILING
#define DISTANTLIGHTS_PF_START(x)			PF_START(x)
#define DISTANTLIGHTS_PF_STOP(x)			PF_STOP(x)
#define DISTANTLIGHTS_PF_FUNC(x)			PF_FUNC(x)
#else
#define DISTANTLIGHTS_PF_START(x)
#define DISTANTLIGHTS_PF_STOP(x)
#define DISTANTLIGHTS_PF_FUNC(x)
#endif // ENABLE_DISTANTLIGHTS_PROFILING

///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////


#if __WIN32
#pragma warning(push)
#pragma warning(disable:4201)	// nonstandard extension used : nameless struct/union
#endif


// DistantLightGroup_t
// - defines a group of points that will have sliding lights created on it
struct DistantLightGroup_t
{
	s16	centreX;
	s16	centreY;
	s16	centreZ;
	s16	radius;

	s16	pointOffset;
	s16	pointCount;

	union
	{
		struct
		{
			u16 DECLARE_BITFIELD_6(
				density, 4,
				speed, 4,
				numLanes, 4,
				twoWay, 1,
				useStreetlights, 1,
				unused, 2
				);
		};
		u16 displayProperties;
	};
};

#if __WIN32
#pragma warning(pop)
#endif

// The new distant lights code needs extra information but the beginning must remain the same as
// DistantLightGroup_t.
struct DistantLightGroup2_t : public DistantLightGroup_t 
{
	float distanceOffset;

	// As we are using deterministic lights so that groups can align up, we need to
	// use the same seed for related groups.
	s32   randomSeed;
};

#if USE_DISTANT_LIGHTS_2
class CDistantLights2;
typedef CDistantLights2			tDistantLightRenderer;
typedef DistantLightGroup2_t	tDistantLightGroupData;
#else
class CDistantLights;
typedef CDistantLights			tDistantLightRenderer;
typedef DistantLightGroup_t		tDistantLightGroupData;
#endif // USE_DISTANT_LIGHTS_2

// DistantLightPoint_t
// - defines a single point that is part of a group
struct DistantLightPoint_t
{
	s16	x;
	s16	y;
	s16	z;
};

#if ENABLE_DISTANT_CARS
struct DistantCar_t
{
	Vec3V pos;
	Vec3V dir;

	// The node generation tool needs both versions of distant lights.
#if USE_DISTANT_LIGHTS_2 || RSG_DEV
	Color32 color;
	bool  isDrivingAway;
#endif  // USE_DISTANT_LIGHTS_2 || RSG_DEV
};

#define DISTANT_CARS_ONLY(x)	x
#else  // ENABLE_DISTANT_CARS
#define DISTANT_CARS_ONLY(x)
#endif // ENABLE_DISTANT_CARS

struct distLightsParam 
{
	// TODO -- thse don't match the defaults in CDistantLights::UpdateVisualDataSettings anymore .. maybe we should just zero it out?
	distLightsParam()
	{
		size						= 1.25f;
		sizeReflections				= 2.5f;
		sizeMin						= 6.0f;
		sizeUpscale					= 1.0f;
		sizeUpscaleReflections		= 2.0f;
		carLightHdrIntensity		= 8.0f;
		streetLightHdrIntensity		= 8.0f;
		flicker						= 0.33f;
		twinkleSpeed				= 1.0f;
		twinkleAmount				= 0.0f;
		carLightZOffset				= 2.0f;
		streetLightZOffset			= 12.0f;
		inlandHeight				= 75.0f;
		hourStart					= 19.0f;
		hourEnd						= 6.0f;
		streetLightHourStart		= 19.0f;
		streetLightHourEnd			= 6.0f;
		sizeDistStart				= 40.0f;
		sizeDist					= 70.0f;

#if !USE_DISTANT_LIGHTS_2 || RSG_DEV
		speed0Speed					= 0.3f;
		speed3Speed					= 1.5f;
#endif // !USE_DISTANT_LIGHTS_2 || RSG_DEV

#if USE_DISTANT_LIGHTS_2 || RSG_DEV
		speed[0]					= 0.2f;
		speed[1]					= 0.35f;
		speed[2]					= 0.4f;
		speed[3]					= 0.6f;
#endif // USE_DISTANT_LIGHTS_2 || RSG_DEV

		// The node generation tool needs both versions of distant lights.
#if !USE_DISTANT_LIGHTS_2 || RSG_DEV
		carLightNearFadeDist		= 300.0f;

#if (RSG_PC || RSG_DURANGO || RSG_ORBIS)
		distantCarsNearDist			= 550.0f;
#else
		distantCarsNearDist			= 300.0f;
#endif // (RSG_PC || RSG_DURANGO || RSG_ORBIS)
#endif // !USE_DISTANT_LIGHTS_2
		carLightFarFadeDist			= 300.0f;
#if ENABLE_DISTANT_CARS
		carLightMinSpacing			= 35.0f;
		distantCarZOffset			= 1.85f;
#if USE_DISTANT_LIGHTS_2
		distantCarLightOffsetWhite	= -3.0f;
		distantCarLightOffsetRed	= 3.0f;
#else
		distantCarLightOffsetWhite	= -2.18f;
		distantCarLightOffsetRed	= 2.18f;
#endif //USE_DISTANT_LIGHTS_2
#endif
		randomizeSpeedSP			= 0.5f;
		randomizeSpacingSP			= 0.25f;
		randomizeSpeedMP			= 0.5f;
		randomizeSpacingMP			= 0.25f;
#if USE_DISTANT_LIGHTS_2
		carLightFadeIn				= 0.0f;
		carLightFadeInStart			= 400.0f;
		carLightFadeInRange			= 600.0f;
		carPopLodDistance			= 500.0f;
#endif //USE_DISTANT_LIGHTS_2
	}

	float size;
	float sizeReflections;
	float sizeMin;
	float sizeUpscale;
	float sizeUpscaleReflections;
	float carLightHdrIntensity;
	float streetLightHdrIntensity;
	float flicker;
	float twinkleSpeed;
	float twinkleAmount;
	float carLightZOffset;
	float streetLightZOffset;
	float inlandHeight;
	float hourStart;
	float hourEnd;
	float streetLightHourStart;
	float streetLightHourEnd;
	float sizeDistStart;
	float sizeDist;

#if !USE_DISTANT_LIGHTS_2 || RSG_DEV
	float speed0Speed;
	float speed3Speed;
#endif // !USE_DISTANT_LIGHTS_2 || RSG_DEV

#if USE_DISTANT_LIGHTS_2 || RSG_DEV
	float speed[DISTANTLIGHTS2_NUM_SPEED_GROUPS];
#endif // USE_DISTANT_LIGHTS_2 || RSG_DEV

	// The node generation tool needs both versions of distant lights.
#if !USE_DISTANT_LIGHTS_2 || RSG_DEV
	float carLightNearFadeDist;
	float distantCarsNearDist;
#endif // !USE_DISTANT_LIGHTS_2 || RSG_DEV
	float carLightFarFadeDist;
	float randomizeSpeedSP;
	float randomizeSpacingSP;
	float randomizeSpeedMP;
	float randomizeSpacingMP;
#if ENABLE_DISTANT_CARS
	float carLightMinSpacing;

	float distantCarZOffset;
	float distantCarLightOffsetWhite;
	float distantCarLightOffsetRed;
#endif

#if USE_DISTANT_LIGHTS_2 || RSG_DEV
	float carLightFadeIn;
	float carLightFadeInStart;
	float carLightFadeInRange;
	float carPopLodDistance;
#endif //USE_DISTANT_LIGHTS_2 || RSG_DEV
};


// Visual Settings
struct distLight {
	Vec3V vColor;
	float spacingSP;
	float spacingMP;
	float speedSP;
	float speedMP;
};

struct DistantLightProperties
{
	ScalarV scCarLight1Color;
	ScalarV scCarLight2Color;
	ScalarV scStreetLightColor;
};

struct OutgoingLink
{
	OutgoingLink() {}
	OutgoingLink(s32 linkIdx, s32 nodeIdx) : m_linkIdx(linkIdx), m_nodeIdx(nodeIdx) {}
	bool IsValid() { return m_linkIdx >= 0 && m_nodeIdx >= 0; }
	s32 m_linkIdx;
	s32 m_nodeIdx;
};


#if !__SPU
typedef void* FileHandle;
namespace DistantLightSaveData
{
s32 Read(FileHandle id,s32 *pData,s32 count);
s32 Read(FileHandle id,s16 *pData,s32 count);
s32 Read(FileHandle id,DistantLightGroup_t *pData,s32 count);
s32 Read(FileHandle id,DistantLightGroup2_t *pData,s32 count);

#if RSG_BANK
s32 Write(FileHandle id,s32 *pData,s32 count);
s32 Write(FileHandle id,s16 *pData,s32 count);
s32 Write(FileHandle id,DistantLightGroup_t *pData,s32 count);
s32 Write(FileHandle id,DistantLightGroup2_t *pData,s32 count);
#endif // RSG_BANK

}

#endif // !__SPU


EXT_PF_PAGE(VFX);

EXT_PF_TIMER(Update);
EXT_PF_TIMER(Render);
EXT_PF_TIMER(RenderGroups);
EXT_PF_TIMER(RenderBuffer);
EXT_PF_TIMER(Stall);

EXT_PF_TIMER(Render_NoStall);
EXT_PF_TIMER(RenderGroups_NoStall);
EXT_PF_TIMER(RenderBuffer_NoStall);

EXT_PF_TIMER(Setup);
EXT_PF_TIMER(CalcLanes);
EXT_PF_TIMER(CalcLayout);
EXT_PF_TIMER(CalcCars);
EXT_PF_TIMER(CalcCarsPos);
EXT_PF_TIMER(CalcCarsLightColor);

#if ENABLE_DISTANT_CARS
struct DistantCarInstanceData
{
	float vPositionX, vPositionY, vPositionZ;
#if __PS3
	float vIndex; // takes up position.w slot
#endif
	float vDirectionX, vDirectionY, vDirectionZ;

#if USE_DISTANT_LIGHTS_2
	u32 Color;
#endif // USE_DISTANT_LIGHTS_2
};
#endif // ENABLE_DISTANT_CARS

extern distLightsParam						g_distLightsParam;
extern float								g_randomSwitchOnValues[DISTANTLIGHTS_NUM_RANDOM_VALUES];
extern u32									g_DistantLightsCount[2];
extern sysDependency						g_DistantLightsVisibilityDependency;
extern u32									g_DistantLightsVisibilityDependencyRunning;


#if __BANK
extern u32									g_DistantLightsMainVPCount[2];
extern u32									g_DistantLightsMirrorReflectionCount[2];
extern u32									g_DistantLightsWaterReflectionCount[2];
extern u32									g_DistantLightsBlocksCount[2];
extern u32									g_DistantLightsMirrorReflectionBlocksCount[2];
extern u32									g_DistantLightsWaterReflectionBlocksCount[2];

extern s32									g_distantLightsDensityFilter;
extern s32									g_distantLightsSpeedFilter;
extern s32									g_distantLightsWidthFilter;
extern bool									g_distantLightsDebugColorByDensity;
extern bool									g_distantLightsDebugColorBySpeed;
extern bool									g_distantLightsDebugColorByWidth;
#endif

#if __DEV
extern u32									g_DistantLightsOcclusionTrivialAcceptActiveFrustum[2];
extern u32									g_DistantLightsOcclusionTrivialAcceptActiveMinZ[2];
extern u32									g_DistantLightsOcclusionTrivialAcceptVisiblePixel[2];
#endif

extern ALIGNAS(16) tDistantLightRenderer g_distantLights;

#endif // DISTANTLIGHTS_COMMON_H
