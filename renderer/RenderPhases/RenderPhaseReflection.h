//
// filename:	RenderPhaseReflection.h
// description: renderphase classes
//

#ifndef INC_RENDERPHASEREFLECTION_H_
#define INC_RENDERPHASEREFLECTION_H_

// Rage headers
#include "grcore/stateblock.h"
#include "vectormath/vec3v.h"

// Framework headers
#include "fwscene/lod/LodTypes.h"
#include "fwscene/scan/ScanResults.h"
#include "spatialdata/transposedplaneset.h"
// Game headers
#include "RenderPhase.h"
#include "renderer/Deferred/DeferredConfig.h"

// Original was 512x256 with 4 mips, every time res goes up we need extra mips
#define REFLECTION_TARGET_WIDTH  (1024)
#define REFLECTION_TARGET_HEIGHT (512)
#define REFLECTION_TARGET_MIPCOUNT (6)
#define USE_REFLECTION_COPY		(RSG_PC)

#define TOTAL_REFLECT_BUFFERS (2)

class CRenderPhaseReflection : public CRenderPhaseScanned
{
public:
	CRenderPhaseReflection(CViewport* viewport);
	~CRenderPhaseReflection();

	static void InitClass();

	virtual void BuildRenderList();
	virtual void BuildDrawList();
	virtual void UpdateViewport();
	virtual u32 GetCullFlags() const;
	virtual void SetCullShape(fwRenderPhaseCullShape& cullShape);
#if __BANK
	virtual void AddWidgets(bkGroup& bank);
#endif // __BANK

	static void SetShaderParamsFromTimecycles();

	static void CreateCubeMapRenderTarget(int res);
	static void SetRenderTargetColor(grcRenderTarget* target) { ms_renderTarget = target; }
	static grcRenderTarget* GetRenderTargetColor() { return ms_renderTarget; }

#if USE_REFLECTION_COPY
	static void SetRenderTargetColorCopy(grcRenderTarget* target) { ms_renderTargetCopy = target; }
	static grcRenderTarget* GetRenderTargetColorCopy() { return ms_renderTargetCopy; }
#endif

	static void ReflectionMipMapAndBlur(grcRenderTarget* refMap, grcRenderTarget* refMapTex);


	static void SetReflectionMap();
	static void SetReflectionMapExt(const grcTexture* pTex);
	static void UpdateVisualDataSettings();

	// Variables
	#if __BANK
		static bool  ms_enableLodRangeCulling;
		static bool  ms_overrideLodRanges;
	#endif // __BANK
	static float ms_lodRanges[LODTYPES_DEPTH_TOTAL][2];
	
	static float ms_interiorRange;

	static float ms_targetLodRange[2];
	static float ms_targetSlodRange[2];

	static float ms_currentLodRange[2];
	static float ms_currentSlodRange[2];

	static bool  ms_enableTimeBasedLerp[2];
	static float ms_adjustSpeed[2];

	static grcRenderTarget* ms_renderTarget;
#if USE_REFLECTION_COPY
	static grcRenderTarget* ms_renderTargetCopy;
#endif // USE_REFLECTION_COPY

	static Vec3V ms_cameraPos;

	// Reflection map freezing
	static bool ms_reflectionMapFrozen;
	static s32 ms_numFramesToFreezeStart;

	static grcEffectGlobalVar         ms_reflectionTextureId;
	static grcEffectGlobalVar         ms_reflectionMipCount;

	static bool ms_EnableComputeShaderMipBlur;
	static bool ms_EnableMipBlur;

#if __DEV
	static float ms_reflectionBlurSizeScale;
	static float ms_reflectionBlurSizeScalePoisson;
	static Vec3V ms_reflectionBlurSizeScaleV; // per mip
	static bool  ms_reflectionUsePoissonFilter[4];
	static int   ms_reflectionBlurSize[6];
#endif

	// Functions
	void GenerateCubeMap();
	static void ConditionalSkyRender(u32 facet);
	static void SetSkyRendered();
	static void ResetSkyRendered();
	static void BuildFacetPlaneSets();
	#if GS_INSTANCED_CUBEMAP
		static void SetupFacetInstVP();
		static void SetupFacetInst(Vec3V cameraPos);
		static void ShutdownFacetInst();
		static bool IsUsingCubeMapInst()	{ return m_bCubeMapInst; }
	#else	
		static void SetupFacet(u32 facet, Vec3V cameraPos);
		static void ShutdownFacet(u32 facet);
	#endif
	

	static void InitialState();
	static void ShutdownState();

	static void GenerateMipMaps(u32 facet, grcRenderTarget* refMap, grcRenderTarget* refMapTex);
	static void RenderExtras();

	// Variables
#if GS_INSTANCED_CUBEMAP
	static grcRenderTarget* ms_renderTargetCubeDepth;
#else
	static grcRenderTarget* ms_renderTargetFacet[2];
	static grcRenderTarget* ms_renderTargetFacetDepth[2];
	static int				ms_renderTargetFacetIndex;
#endif

#if RSG_ORBIS
	static grcRenderTarget* ms_renderTargetCubeArray;
#endif // RSG_ORBIS

	static grcRenderTarget* ms_renderTargetCube;
#if REFLECTION_CUBEMAP_SAMPLING
	#if RSG_DURANGO
		static grcRenderTarget* ms_renderTargetCubeCopy;
	#endif
#endif

	static Mat34V ms_faceMatrices[6];
	static grcDepthStencilStateHandle ms_interiorDepthStencilState;
	static grcDepthStencilStateHandle ms_lodDepthStencilState;
	static grcDepthStencilStateHandle ms_slod3DepthStencilState;
	static grcRasterizerStateHandle ms_cullingModeState;
	static bool ms_skyRendered;
	enum { FACET_COUNT = 6 };
	static spdTransposedPlaneSet4	  ms_facetPlaneSets[FACET_COUNT];
	#if GS_INSTANCED_CUBEMAP
		static bool m_bCubeMapInst;
	#endif
	#if REFLECTION_CUBEMAP_SAMPLING
		static grcEffectGlobalVar		  ms_reflectionCubeTextureId;
	#endif
};

#endif // !INC_RENDERPHASEREFLECTION_H_
