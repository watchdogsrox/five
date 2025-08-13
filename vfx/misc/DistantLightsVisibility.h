#ifndef _INC_DISTANTLIGHTSVISIBILITY_H_
#define _INC_DISTANTLIGHTSVISIBILITY_H_

#include "DistantLightsCommon.h"

#include "fwtl/StepAllocator.h"
#include "spatialdata/transposedplaneset.h"
#include "fwscene/scan/ScanEntities.h"
#include "fwscene/world/WorldLimits.h"

#include "../vfx_shared.h"

#if __WIN32
#pragma warning(push)
#pragma warning(disable:4201)	// nonstandard extension used : nameless struct/union
#endif


// Used to index params in an array by name.
enum DistantLightParams
{
	DLP_SRATCH_SIZE = 0,
	DLP_GROUPS,
	DLP_BLOCK_FIRST_GROUP,
	DLP_BLOCK_NUM_COSTAL_GROUPS,
	DLP_BLOCK_NUM_INLAND_GROUPS,
	DLP_VISIBILITY,
	DLP_SCAN_BASE_INFO,
	DLP_HIZ_BUFFER,
	DLP_DEPENDENCY_RUNNING,
};

enum DistantLightsVisibilitFlags
{
	DISTANT_LIGHT_VISIBLE_MAIN_VIEWPORT = (1<<0),
	DISTANT_LIGHT_VISIBLE_WATER_REFLECTION = (1<<1),
	DISTANT_LIGHT_VISIBLE_MIRROR_REFLECTION = (1<<2)
};

enum DistantLightsType
{
	DISTANT_LIGHT_TYPE_COASTAL = 0,
	DISTANT_LIGHT_TYPE_INLAND = 1
};

struct DistantLightsVisibilityOutput
{
	u16 index;

	struct
	{
		u8 DECLARE_BITFIELD_3(
			visibilityFlags, 3,
			lightType, 1,
			unused, 4
			);
		u8 flags;
	};
};
#if __WIN32
#pragma warning(pop)
#endif

#if __BANK
struct DistantLightsVisibilityBlockOutput
{
	u8 blockX;
	u8 blockY;

};
#endif

struct DistantLightsVisibilityData
{
	spdTransposedPlaneSet8	m_mainViewportCullPlanes;
	spdTransposedPlaneSet8	m_waterReflectionCullPlanes;
	spdTransposedPlaneSet8	m_mirrorReflectionCullPlanes;
	DistantLightsVisibilityOutput* m_pDistantLightsVisibilityArray;
	u32*					m_pDistantLightsVisibilityCount;

	Vec3V	m_vCameraPos;
	float m_InlandAlpha;
	bool m_PerformMirrorReflectionVisibility;
	bool m_PerformWaterReflectionVisibility;
#if __DEV
	u32*					m_pDistantLightsOcclusionTrivialAcceptActiveFrustum;
	u32*					m_pDistantLightsOcclusionTrivialAcceptMinZ;
	u32*					m_pDistantLightsOcclusionTrivialAcceptVisiblePixel;
#endif
#if __BANK
	DistantLightsVisibilityBlockOutput* m_pDistantLightsVisibilityBlockArray;
	u32*					m_pDistantLightsVisibilityBlockCount;
	u32*					m_pDistantLightsVisibilityWaterReflectionBlockCount;
	u32*					m_pDistantLightsVisibilityMirrorReflectionBlockCount;
	u32*					m_pDistantLightsVisibilityMainVPCount;
	u32*					m_pDistantLightsVisibilityWaterReflectionCount;
	u32*					m_pDistantLightsVisibilityMirrorReflectionCount;
#endif

	// These are used to cull blocks based on distance and generally remain constant
	static bank_float		ms_BlockMaxVisibleDistance;
};


extern DistantLightsVisibilityOutput		g_DistantLightsVisibility[2][DISTANTLIGHTS_CURRENT_MAX_GROUPS] ;
extern DistantLightsVisibilityData			g_DistantLightsVisibilityData ;

#if __BANK
extern DistantLightsVisibilityBlockOutput	g_DistantLightsBlockVisibility[2][DISTANTLIGHTS_CURRENT_MAX_BLOCKS * DISTANTLIGHTS_CURRENT_MAX_BLOCKS] ;
#endif


#if __SPU || __PPU
#define DISTANTLIGHTSVISIBILITY_SCRATCH_SIZE	(4*1024)
#else
#define DISTANT_LIGHTS_GROUP_SCRATCH_SIZE		(DISTANTLIGHTS_CURRENT_MAX_GROUPS * sizeof(DistantLightsVisibilityOutput))

#if __BANK
#define DISTANTLIGHTSVISIBILITY_SCRATCH_SIZE	( DISTANT_LIGHTS_GROUP_SCRATCH_SIZE + (DISTANTLIGHTS_CURRENT_MAX_BLOCKS * DISTANTLIGHTS_CURRENT_MAX_BLOCKS * sizeof(DistantLightsVisibilityBlockOutput)) )
#else
#define DISTANTLIGHTSVISIBILITY_SCRATCH_SIZE	(DISTANT_LIGHTS_GROUP_SCRATCH_SIZE)
#endif // __BANK
#endif // __SPU || __PPU

class DistantLightsVisibility
{
public:
	static bool RunFromDependency(const sysDependency& dependency);
	static void FindBlockMin(s32 blockX, s32 blockY, float& minX, float& minY);
	static void FindBlockMax(s32 blockX, s32 blockY, float& maxX, float& maxY);
	static void FindBlockCentre(s32 blockX, s32 blockY, float& posX, float& posY);
};
#endif
