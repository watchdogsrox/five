#ifndef _INC_LIGHTSVISIBILITYSORT_H_
#define _INC_LIGHTSVISIBILITYSORT_H_

#include "TiledLightingSettings.h"

#include "fwtl/StepAllocator.h"
#include "spatialdata/transposedplaneset.h"
#include "atl/bitset.h"
#include "system/dependency.h"


// --- Defines ------------------------------------------------------------------

#define FACETS_ALL_INSIDE 0x80
#define FACET_INSIDE_MASK 0x3f  // masks off all facet inside flags.

struct LightsVisibilityOutput
{
	u16 index;
};
struct LightsVisibilitySortData
{
	spdTransposedPlaneSet8	m_cullPlanes;
	Vector4 m_farClip;
	float m_nearPlaneBoundsIncrease;
	u32 m_startLightIndex;
	u32 m_endLightIndex;
	u32 m_prevLightCount;
	Vector3	m_CameraPos;
	LightsVisibilityOutput*	m_pVisibilitySortedArray;
	u32*					m_pVisibilitySortedCount;
	atFixedBitSet<MAX_STORED_LIGHTS>* m_pCamInsideOutput;
	atFixedBitSet<MAX_STORED_LIGHTS>* m_pVisibleLastFrameOutput;
};

#if __SPU || __PPU
	#define LIGHTSVISIBILITYSORT_SCRATCH_SIZE	(1*1024)
#elif RSG_PC
	#define LIGHTSVISIBILITYSORT_SCRATCH_SIZE	(1*2048)
#elif RSG_ORBIS || RSG_DURANGO
	#define LIGHTSVISIBILITYSORT_SCRATCH_SIZE	(1*1536)
#else
	#define LIGHTSVISIBILITYSORT_SCRATCH_SIZE	(1*1024)
#endif

class LightsVisibilitySort
{
public:
	static bool RunFromDependency(const sysDependency& dependency);
};


#endif // _INC_LIGHTSVISIBILITYSORT_H_
