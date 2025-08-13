#///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	DistantLights2.h
//	BY	: 	Thomas Randall (Adapted from original by Mark Nicholson)
//	FOR	:	Rockstar North
//	ON	:	12 Jun 2013
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#include "DistantLightsCommon.h"
#include "DistantLights.h"
#include "DistantLights2.h"

#include "system/FileMgr.h"
#include "profile/group.h"

namespace DistantLightSaveData
{

s32 Read(FileHandle id,s32 *pData,s32 count)
{
	s32 result = CFileMgr::Read(id,(char*)pData,count*sizeof(s32));
#if !__BE
	for (s32 i=0; i<result/sizeof(s32); i++)
		pData[i] = sysEndian::Swap(pData[i]);
#endif
	return result;
}


s32 Read(FileHandle id,s16 *pData,s32 count)
{
	s32 result = CFileMgr::Read(id,(char*)pData,count*sizeof(s16));
#if !__BE
	for (s32 i=0; i<result/sizeof(s16); i++)
		pData[i] = sysEndian::Swap(pData[i]);
#endif
	return result;
}

s32 Read(FileHandle id,DistantLightGroup_t *pData,s32 count)
{
	s32 result = CFileMgr::Read(id,(char*)pData,count*sizeof(DistantLightGroup_t));
#if !__BE
	for (s32 i=0; i<count; i++)
	{
		sysEndian::SwapMe(pData[i].centreX);
		sysEndian::SwapMe(pData[i].centreY);
		sysEndian::SwapMe(pData[i].centreZ);
		sysEndian::SwapMe(pData[i].radius);
		sysEndian::SwapMe(pData[i].pointCount);
		sysEndian::SwapMe(pData[i].pointOffset);
		sysEndian::SwapMe(pData[i].displayProperties);
	}
#endif
	return result;
}

s32 Read(FileHandle id,DistantLightGroup2_t *pData,s32 count)
{
	s32 result = CFileMgr::Read(id,(char*)pData,count*sizeof(DistantLightGroup2_t));
#if !__BE
	for (s32 i=0; i<count; i++)
	{
		sysEndian::SwapMe(pData[i].centreX);
		sysEndian::SwapMe(pData[i].centreY);
		sysEndian::SwapMe(pData[i].centreZ);
		sysEndian::SwapMe(pData[i].radius);
		sysEndian::SwapMe(pData[i].pointCount);
		sysEndian::SwapMe(pData[i].pointOffset);
		sysEndian::SwapMe(pData[i].displayProperties);
		sysEndian::SwapMe(pData[i].distanceOffset);
	}
#endif
	return result;
}

#if RSG_BANK
s32 Write(FileHandle id,s32 *pData,s32 count)
{
#if !__BE
	for (s32 i=0; i<count; i++)
		pData[i] = sysEndian::Swap(pData[i]);
#endif

	s32 result = CFileMgr::Write(id,(char*)pData,count*sizeof(s32));

#if !__BE		// Swap it back again to preserve data
	for (s32 i=0; i<count; i++)
		pData[i] = sysEndian::Swap(pData[i]);
#endif

	return result;
}

s32 Write(FileHandle id,s16 *pData,s32 count)
{
#if !__BE
	for (s32 i=0; i<count; i++)
		pData[i] = sysEndian::Swap(pData[i]);
#endif

	s32 result = CFileMgr::Write(id,(char*)pData,count*sizeof(s16));

#if !__BE // Swap it back again to preserve data
	for (s32 i=0; i<count; i++)
		pData[i] = sysEndian::Swap(pData[i]);
#endif

	return result;
}

s32 Write(FileHandle id,DistantLightGroup_t *pData,s32 count)
{
#if !__BE
	for (s32 i=0; i<count; i++)
	{
		sysEndian::SwapMe(pData[i].centreX);
		sysEndian::SwapMe(pData[i].centreY);
		sysEndian::SwapMe(pData[i].centreZ);
		sysEndian::SwapMe(pData[i].radius);
		sysEndian::SwapMe(pData[i].pointCount);
		sysEndian::SwapMe(pData[i].pointOffset);
		sysEndian::SwapMe(pData[i].displayProperties);
	}
#endif

	s32 result = CFileMgr::Write(id,(char*)pData,count*sizeof(DistantLightGroup_t));

#if !__BE
	for (s32 i=0; i<count; i++)
	{
		sysEndian::SwapMe(pData[i].centreX);
		sysEndian::SwapMe(pData[i].centreY);
		sysEndian::SwapMe(pData[i].centreZ);
		sysEndian::SwapMe(pData[i].radius);
		sysEndian::SwapMe(pData[i].pointCount);
		sysEndian::SwapMe(pData[i].pointOffset);
		sysEndian::SwapMe(pData[i].displayProperties);
	}
#endif

	return result;
}



s32 Write(FileHandle id,DistantLightGroup2_t *pData,s32 count)
{
#if !__BE
	for (s32 i=0; i<count; i++)
	{
		sysEndian::SwapMe(pData[i].centreX);
		sysEndian::SwapMe(pData[i].centreY);
		sysEndian::SwapMe(pData[i].centreZ);
		sysEndian::SwapMe(pData[i].radius);
		sysEndian::SwapMe(pData[i].pointCount);
		sysEndian::SwapMe(pData[i].pointOffset);
		sysEndian::SwapMe(pData[i].displayProperties);
		sysEndian::SwapMe(pData[i].distanceOffset);
	}
#endif

	s32 result = CFileMgr::Write(id,(char*)pData,count*sizeof(DistantLightGroup2_t));

#if !__BE
	for (s32 i=0; i<count; i++)
	{
		sysEndian::SwapMe(pData[i].centreX);
		sysEndian::SwapMe(pData[i].centreY);
		sysEndian::SwapMe(pData[i].centreZ);
		sysEndian::SwapMe(pData[i].radius);
		sysEndian::SwapMe(pData[i].pointCount);
		sysEndian::SwapMe(pData[i].pointOffset);
		sysEndian::SwapMe(pData[i].displayProperties);
		sysEndian::SwapMe(pData[i].distanceOffset);
	}
#endif

	return result;
}

#endif // RSG_BANK

}



PF_PAGE(VFX, "Visual Effects");
PF_GROUP(DistantLights);
PF_LINK(VFX, DistantLights);

PF_TIMER(Update, DistantLights);
PF_TIMER(Render, DistantLights);
PF_TIMER(RenderGroups, DistantLights);
PF_TIMER(RenderBuffer, DistantLights);
PF_TIMER(Stall, DistantLights);

PF_TIMER(Render_NoStall, DistantLights);
PF_TIMER(RenderGroups_NoStall, DistantLights);
PF_TIMER(RenderBuffer_NoStall, DistantLights);

PF_TIMER(Setup, DistantLights);
PF_TIMER(CalcLanes, DistantLights);
PF_TIMER(CalcLayout, DistantLights);
PF_TIMER(CalcCars, DistantLights);
PF_TIMER(CalcCarsPos, DistantLights);
PF_TIMER(CalcCarsLightColor, DistantLights);

distLightsParam g_distLightsParam;


float g_randomSwitchOnValues[DISTANTLIGHTS_NUM_RANDOM_VALUES] = 
{
	0.10f, 0.70f, 0.50f, 0.96f, 
	0.34f, 0.75f, 0.20f, 0.40f, 
	0.65f, 0.05f, 0.76f, 0.83f, 
	0.59f, 0.34f, 0.66f, 0.43f
};

DistantLightsVisibilityOutput g_DistantLightsVisibility[2][DISTANTLIGHTS_CURRENT_MAX_GROUPS] ;

DistantLightsVisibilityData	g_DistantLightsVisibilityData ;
u32						g_DistantLightsCount[2];
sysDependency			g_DistantLightsVisibilityDependency;
u32						g_DistantLightsVisibilityDependencyRunning;

#if __PPU
DECLARE_FRAG_INTERFACE(DistantLightsVisibility);
#endif

#if __BANK
DistantLightsVisibilityBlockOutput g_DistantLightsBlockVisibility[2][DISTANTLIGHTS_CURRENT_MAX_BLOCKS * DISTANTLIGHTS_CURRENT_MAX_BLOCKS] ;
u32						g_DistantLightsMainVPCount[2];
u32						g_DistantLightsMirrorReflectionCount[2];
u32						g_DistantLightsWaterReflectionCount[2];
u32						g_DistantLightsBlocksCount[2];
u32						g_DistantLightsMirrorReflectionBlocksCount[2];
u32						g_DistantLightsWaterReflectionBlocksCount[2];

s32 g_distantLightsDensityFilter = -1;
s32 g_distantLightsSpeedFilter = -1;
s32 g_distantLightsWidthFilter = -1;
bool g_distantLightsDebugColorByDensity = false;
bool g_distantLightsDebugColorBySpeed = false;
bool g_distantLightsDebugColorByWidth = false;
#endif

#if __DEV
u32						g_DistantLightsOcclusionTrivialAcceptActiveFrustum[2];
u32						g_DistantLightsOcclusionTrivialAcceptActiveMinZ[2];
u32						g_DistantLightsOcclusionTrivialAcceptVisiblePixel[2];
#endif

ALIGNAS(16) tDistantLightRenderer g_distantLights;