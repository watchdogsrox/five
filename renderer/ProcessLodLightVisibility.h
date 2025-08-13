#ifndef _INC_PROCESSLODLIGHTCORONAS_H_
#define _INC_PROCESSLODLIGHTCORONAS_H_

#include "../vfx/misc/Coronas.h"
#include "fwtl/StepAllocator.h"
#include "spatialdata/transposedplaneset.h"
#include "../vfx/misc/LODLightManager.h"
#include "fwscene/scan/ScanEntities.h"

#define MAX_VISIBLE_LOD_LIGHTS_ASYNC	128

#if __XENON || __PS3
	#define MAX_VISIBLE_LOD_LIGHTS 1024
#else
	// NG and PC want more lod lights and have altered ranges and streaming extents accordingly
	#define MAX_VISIBLE_LOD_LIGHTS 6144
#endif

struct LodLightVisibility
{
	u16 bucketIndex;
	u16 lightIndex;
#if !__FINAL
	u16 mapDataSlotIndex;
	u16 parentMapDataSlotIndex;
#endif
};
struct LodLightVisibilityData
{
	spdTransposedPlaneSet8	m_cullPlanes;
	spdTransposedPlaneSet8	m_waterReflectionCullPlanes;
	Vec3V					m_vCameraPos;
	Vec3V					m_vWaterReflectionCameraPos;
	ScalarV					m_vDirMultFadeOffStart;
	ScalarV					m_vDirMultFadeOffDist;
	float					m_LodLightRange[LODLIGHT_CATEGORY_COUNT];

	float					m_LodLightCoronaRange[LODLIGHT_CATEGORY_COUNT];
	float					m_LodLightFadeRange[LODLIGHT_CATEGORY_COUNT];

	int						m_hour;
	float					m_fade;
	float					m_brightness;
	float					m_coronaSize;

	Corona_t*				m_pCoronaArray;
	LodLightVisibility*		m_pVisibilityArray;
	u32*					m_pCoronaCount;
	u32*					m_pVisibilityCount;

	float					m_lodLightCoronaIntensityMult;
	float					m_coronaZBias;
	u8						m_bCheckReflectionPhase:1;
	u8						m_bIsCutScenePlaying:1;
	u8						m_pad:6;

	u32*					m_coronaLightHashes;
	u32						m_numCoronaLightHashes;

#if __DEV
	u32*					m_pLodLightsOcclusionTrivialAcceptActiveFrustum;
	u32*					m_pLodLightsOcclusionTrivialAcceptMinZ;
	u32*					m_pLodLightsOcclusionTrivialAcceptVisiblePixel;
#endif

#if __BANK
	u32*					m_pOverflowCount;
	u32*					m_pLODLightCoronaCount;
	u8						m_bPerformLightVisibility:1;
	u8						m_bPerformOcclusion:1;
	u8						m_bPerformSort:1;
	u8						m_debugPad:5;
#endif
};

#if __SPU || __PPU
		#define PROCESSLODLIGHT_CORONAS_SCRATCH_SIZE	(121*1024)
#else
	#if RSG_PC || RSG_ORBIS || RSG_DURANGO
		#define PROCESSLODLIGHT_CORONAS_SCRATCH_SIZE	(58*1024)
	#else
		#define PROCESSLODLIGHT_CORONAS_SCRATCH_SIZE	(48*1024)
	#endif
#endif

class ProcessLodLightVisibility
{
public:
	static void ProcessVisibility(
		fwStepAllocator *pScratchAllocator,
		LodLightVisibility* pLocalVisibilityArray,
		LodLightVisibility *pLocalVisibilitySpotArray,
		LodLightVisibility *pLocalVisibilityPointArray,
		LodLightVisibility *pLocalVisibilityCapsuleArray,
		Corona_t *pLocalCoronaArray,
		CLODLightBucket* const* LODLightsToRender,
		int bucketCount,
		u32* aSceneLightHashes,
		int numSceneLightHashes,
		u32* aBrokenLightHashes,
		int numBrokenLightHashes,
		u32* aCoronaLightHashes,
		int numCoronaLightHashes,
		const fwScanBaseInfo* scanBaseInfo,
		const Vec4V* hiZBuffer,
		LodLightVisibilityData* pData
		BANK_ONLY(, bool bProcessAsync)
		);
	static bool RunFromDependency(const sysDependency& dependency);
};


#endif // _INC_PROCESSLODLIGHTCORONAS_H_
