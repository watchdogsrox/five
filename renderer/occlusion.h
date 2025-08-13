/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    occlusion.h
// PURPOSE : The game now uses the rage occlusion. This file contains only game specific stuff.
// AUTHOR :  Obbe.
// CREATED : 7/10/05
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _OCCLUSION_H_
#define _OCCLUSION_H_

// rage includes
#include "fwscene/scan/ScanDebug.h"
#include "fwscene/scan/ScanEntities.h"
#include "softrasterizer/boxoccluder.h"
#include "softrasterizer/occludeModel.h"
#include "softrasterizer/softrasterizer.h"
#include "spatialdata/aabb.h"

#include "Camera/viewports/ViewportManager.h"

namespace rage {
	// fwd ref
	class SoftRasterizer;
	struct rstTileList;
};

// EJ: Box Occluder Optimization
#define MAX_NUM_BOX_OCCLUDER_BUCKETS	((__XENON || __PS3) ? 48 : 64)
typedef atRangeArray<CBoxOccluderBucket, MAX_NUM_BOX_OCCLUDER_BUCKETS> CBoxOccluderBucketArray;

// EJ: Occlude Model Optimization
#define MAX_NUM_OCCLUDE_MODEL_BUCKETS	((__XENON || __PS3) ? 48 : 64)
typedef atRangeArray<COccludeModelBucket, MAX_NUM_OCCLUDE_MODEL_BUCKETS> COccludeModelBucketArray;

class COcclusion
{
public:
#if __DEV
	// Debug stuff
	static	bool				bOcclusionMeshesDisabled;
	static	bool				bOcclusionBoxesDisabled;
#endif // __DEV
	static	bool				bWaterOcclusionDisabled;
	static	bool				bRasterizeWaterQuadsInHiZ;
#if __BANK
	static	u32					m_NumOfObjectsOccluded[SCAN_OCCLUSION_STORAGE_COUNT+1];
	static	u32					m_NumOfOcclusionTests[SCAN_OCCLUSION_STORAGE_COUNT+1];
	static	u32					m_NumObjectsTrivialAcceptFrustum[SCAN_OCCLUSION_STORAGE_COUNT+1];
	static	u32					m_NumObjectsTrivialAcceptMinZ[SCAN_OCCLUSION_STORAGE_COUNT+1];
	static	u32					m_NumObjectsTrivialAcceptVisPixel[SCAN_OCCLUSION_STORAGE_COUNT+1];
#endif // __BANK

	static	void	Init(unsigned initMode);
	static	void	Shutdown(unsigned shutdownMode);
	static	void	BeforeSetupRender(bool storeWaterOccluders, bool isGBuf, int shadowType,bool useOcclusion,const grcViewport &viewport);
	static	void	AfterPreRender();
	static	void	BeforeRender();
	static	void	DrawDebug();

	// EJ: Box Occluder Optimization
	static	void	AddBoxOccluder(CBoxOccluderArray& boxOccluders, s32 mapDataSlotIndex);
	static	void	RemoveBoxOccluder(s32 mapDataSlotIndex);

	// EJ: Occlude Model Optimization
	static	void	AddOccludeModel(COccludeModelArray& occludeModels, s32 mapDataSlotIndex);	
	static	void	RemoveOccludeModel(s32 mapDataSlotIndex);	

	static	void	Update();
	static	void	KickOcclusionJobs();
	static	void	KickShadowOcclusionJobs();


	static void		SetOcclusionTransform(const int occlusionTransform, const grcViewport &vp);
	static void		GetRasterizerTransform(const int occluderPhase,  Mat44V_InOut transform, float& depthRescale );
	static void		RasterizeOccluders(const int occluderPhase, const grcViewport &vp);
	static void		RasterizeWater(const grcViewport &vp);
	static Vec4V*	GetHiZBuffer(const int occluderPhase);
	static Vec4V_Out GetMinMaxBoundsForBuffer(const int occluderPhase);
	static float	GetMinZForBuffer(const int occluderPhase);
	static Vec4V*	GetHiStencilBuffer();

	static void		AllocatePerFrameResources();
	static void		FreePerFrameResources();

	static void		WaitForAllRasterizersToFinish();

	static bool		IsSphereVisible(const spdSphere& sphere);
	static bool		IsConeVisible(const Mat34V& mtx, float radius, float spreadf );

	static void		WaterOcclusionDataIsReady();
	static void		RegisterWaterPolyStencil(const Vec3V* points, int numPoints);
	static void		RegisterWaterQuadStencil(Vec3V_In p0, Vec3V_In p1, Vec3V_In p2, Vec3V_In p3);
	static void		RegisterWaterQuadStencil(float minX, float minY, float maxX, float maxY, float Z);
	static void		RegisterWaterPolyHiZ(const Vec3V* points, int numPoints);
	static void		RegisterWaterQuadHiZ(Vec3V_In p0, Vec3V_In p1, Vec3V_In p2, Vec3V_In p3);
	static void		RegisterWaterQuadHiZ(float minX, float minY, float maxX, float maxY, float Z);


	static bool		GetOcclusionJobsStarted() { return m_occlusionJobStarted; }
	static void		SetOcclusionHaveStarted() { m_occlusionJobStarted = true; }

	static __forceinline bool IsBoxVisible(const spdAABB& aabb)
	{
		if ( Unlikely(!(g_scanMutableDebugFlags & SCAN_MUTABLE_DEBUG_ENABLE_OCCLUSION)) )
			return true;

		const Vec3V extents( (float)RAST_HIZ_WIDTH, (float)RAST_HIZ_HEIGHT, 0.f);

		ScalarV zero(V_ZERO);
		Vec3V	bmin = aabb.GetMin();
		Vec3V	bmax = aabb.GetMax();

		ScalarV rescaleDepth = ScalarVFromF32(m_rasDepth[ SCAN_OCCLUSION_TRANSFORM_PRIMARY ]);
		rstSplatMat44V splatMtx ;
		rstCreateSplatMat44V( m_transMtx[ SCAN_OCCLUSION_TRANSFORM_PRIMARY ], splatMtx );

		Vec3V scbmin, scbmax;
		if (!rstCalculateScreenSpaceBoundIsVis(	splatMtx, rescaleDepth, bmin,
			bmax,scbmin, scbmax, extents))  // pipeline these two
		{
			return false;
		}

		ScalarV area = rstTestAABBExact( m_transMtx[ SCAN_OCCLUSION_TRANSFORM_PRIMARY ],
			rescaleDepth,
			GetHiZBuffer(SCAN_OCCLUSION_STORAGE_PRIMARY), bmin, bmax, ScalarV(V_ZERO));
		int result = IsGreaterThanAll( area, ScalarV(V_ZERO));

#if __DEV
		AddDebugBound( bmin, bmax, result !=0);
#endif
		return result != 0;
	}


	//Commenting out this function in case we need to revert back to this version.
	//New version which uses the new exact test is below the commented function
	/*
	static __forceinline bool IsBoxVisibleFast(const spdAABB& aabb)
	{
		if(m_useBoxVisibleFastExact)
		{
			return IsBoxVisibleFastExact(aabb);
		}

		if ( Unlikely(!(g_scanMutableDebugFlags & SCAN_MUTABLE_DEBUG_ENABLE_OCCLUSION)) )
			return true;

		const Vec3V extents( (float)RAST_HIZ_WIDTH, (float)RAST_HIZ_HEIGHT, 0.f);

		ScalarV zero(V_ZERO);
		Vec3V	bmin = aabb.GetMin();
		Vec3V	bmax = aabb.GetMax();

		ScalarV rescaleDepth = ScalarVFromF32(m_rasDepth[ SCAN_OCCLUSION_TRANSFORM_PRIMARY ]);
		rstSplatMat44V splatMtx ;
		rstCreateSplatMat44V( m_transMtx[ SCAN_OCCLUSION_TRANSFORM_PRIMARY ], splatMtx );

		Vec3V scbmin, scbmax;
		if (!rstCalculateScreenSpaceBoundIsVis(	splatMtx, rescaleDepth, bmin,
			bmax,scbmin, scbmax, extents))  // pipeline these two
		{
			return false;
		}
		VecBoolV isVis=rstScanQueryHiZLargeApprox( GetHiZBuffer(SCAN_OCCLUSION_STORAGE_PRIMARY), scbmin, scbmax );
		isVis = isVis | VecBoolV( scbmin.GetZ() < zero); // if clip front plane depth will always be in front
		isVis = isVis & VecBoolV( scbmax.GetZ() > zero); // but if behind front plane then chuck it

		int result =  !IsFalseAll(isVis );
#if __DEV
		AddDebugBound( bmin, bmax, result !=0);
#endif
		return result != 0;
	}
	*/

	static __forceinline bool IsBoxVisibleFast(const spdAABB& aabb, bool isShadows = false)
	{
		if ( Unlikely(!(g_scanMutableDebugFlags & SCAN_MUTABLE_DEBUG_ENABLE_OCCLUSION)) )
			return true;
		u8 occlusionTransformIndex = (u8)(isShadows? SCAN_OCCLUSION_TRANSFORM_CASCADE_SHADOWS:SCAN_OCCLUSION_TRANSFORM_PRIMARY);
		u8 occlusionStorageIndex = (u8)(isShadows? SCAN_OCCLUSION_STORAGE_CASCADE_SHADOWS:SCAN_OCCLUSION_STORAGE_PRIMARY);
		ScalarV rescaleDepth = ScalarVFromF32(m_rasDepth[ SCAN_OCCLUSION_TRANSFORM_PRIMARY ]);

		ScalarV area = rstTestAABBExactEdgeList(isShadows,  m_transMtx[ occlusionTransformIndex ], 
			rescaleDepth, GetHiZBuffer(occlusionStorageIndex), 
			gVpMan.GetCurrentGameGrcViewport()->GetCameraPosition(), aabb.GetMin(), aabb.GetMax(), ScalarV(V_ZERO)
			, m_rasterizers[occlusionStorageIndex]->GetMinMaxBounds(), ScalarVFromF32(m_rasterizers[occlusionStorageIndex]->GetMinimumZ())
			DEV_ONLY(, &(m_NumObjectsTrivialAcceptFrustum[occlusionTransformIndex]), &(m_NumObjectsTrivialAcceptMinZ[occlusionTransformIndex]), &(m_NumObjectsTrivialAcceptVisPixel[occlusionTransformIndex]))
			BANK_ONLY(, COcclusion::GetNewOcclusionAABBExactSimpleTestPixelsThreshold(), true, COcclusion::UseTrivialAcceptVisiblePixelTest(), COcclusion::UseMinZClipping())
			);
		
		int result = IsGreaterThanAll( area, ScalarV(V_ZERO));

#if __DEV
		AddDebugBound( aabb.GetMin(), aabb.GetMax(), result !=0);
#endif
		return result != 0;
	}

	static void		PrimeZBuffer();
	static bool		WantToPrimeZBufferWithOccluders();

	static float	GetExactQueryThreshold();

#if __BANK
	static Vec4V_Out GetScreenOcclusionExtents(const spdAABB& aabb);
	static void ToggleGBUFOccluderOverlay();
	static void ToggleGBUFStencilOverlay();
	static bool IsGBUFStencilOverlayEnabled();
	static void RenderDebugBounds();
	static void DebugSelectedEntity();
#if __DEV
	static void AddDebugBound(Vec3V_In ,Vec3V_In ,bool);
	static void AddDebugLines( Vec3V* pts, u8* indices,  int numIndices, bool res );
#endif // __DEV

	static bool		UseSimpleQueriesOnly();
	static bool		UseNewOcclusionAABBExact();
	static bool		UseTrivialAcceptTest();
	static bool		UseTrivialAcceptVisiblePixelTest();
	static bool		UseMinZClipping();
	static bool		ForceExactOcclusionTest();
	static ScalarV_Out GetNewOcclusionAABBExactSimpleTestPixelsThreshold();
	static void		DrawOccluders(bool depthOnly );
	static void		DumpWaterOccluderModels();
	static void		InitWidgets();
	static bool		UseProjectedShadowOcclusionTest();
	static bool		UseProjectedShadowOcclusionClipping();

	static void		GetDebugCounters( u32*& oc,	u32*& oct, u32*&trivFrustum, u32*&trivMinZ, u32*&trivVisPixel)
	{
		oc = m_NumOfObjectsOccluded;
		oct = m_NumOfOcclusionTests;
		trivFrustum = m_NumObjectsTrivialAcceptFrustum;
		trivMinZ = m_NumObjectsTrivialAcceptMinZ;
		trivVisPixel = m_NumObjectsTrivialAcceptVisPixel;
	}
#endif // __BANK

	static float	GetGBufOccluderBias()  { return m_GBufTestBias; }
	static float	GetShadowOccluderBias()  { return m_ShadowTestBias; }
	static float	GetWaterQuadHiZBias() { return m_waterQuadHiZBias; }
	static float	GetWaterQuadStencilBias() { return m_waterQuadStencilBias; }

	static bool OcclusionEnabled();
	static bool FlipResult() { return ( g_scanDebugFlags & SCAN_DEBUG_FLIP_OCCLUSION_RESULT ) != 0; }

	// EJ: Box Occluder Optimization
	inline static s32 GetBoxOccluderUsedBucketCount() { return s_boxOccluderUsedBucketCount; }
	inline static const CBoxOccluderBucketArray& GetBoxOccluderBuckets() { return s_boxOccluderBuckets; }
	inline static CBoxOccluderBucket* GetBoxOccluderBucket(const u32 index) { return &s_boxOccluderBuckets[index]; }

	// EJ: Occlude Model Optimization
	inline static s32 GetOccludeModelUsedBucketCount() { return s_occludeModelUsedBucketCount; }
	inline static const COccludeModelBucketArray& GetOccludeModelBuckets() { return s_occludeModelBuckets; }
	inline static COccludeModelBucket* GetOccludeModelBucket(const u32 index) { return &s_occludeModelBuckets[index]; }

	inline static void ScriptDisableOcclusionThisFrame() { ms_bScriptDisabledOcclusionThisFrame=true; }

	static const Vec3V* GetIcosahedronPoints();
	static const u8* GetIcosahedronIndices();
	static u32 GetNumIcosahedronPoints();
	static u32 GetNumIcosahedronIndices();
	static const Vec3V* GetConePoints();
	static const u8* GetConeIndices();
	static u32 GetNumConePoints();
	static u32 GetNumConeIndices();
	static void	WaitForRasterizerToFinish(const int occluderPhase);
	static void WaitForScanToFinish(const int occluderPhase);

private:
	static atRangeArray<SoftRasterizer*, SCAN_OCCLUSION_STORAGE_COUNT> m_rasterizers;
	static atRangeArray<Mat44V, SCAN_OCCLUSION_TRANSFORM_COUNT>	m_transMtx;
	static atRangeArray<float, SCAN_OCCLUSION_TRANSFORM_COUNT>	m_rasDepth;
	
	static float	m_occluderThreshold[SCAN_OCCLUSION_STORAGE_COUNT];
	static float	m_boxoccluderThreshold[SCAN_OCCLUSION_STORAGE_COUNT];
	static bool		m_primeZBuffer;
	static float	m_GBufTestBias;
	static float	m_ShadowTestBias;
	static float	m_waterQuadHiZBias;
	static float	m_waterQuadStencilBias;

	static grcEffectTechnique m_primeDepthTechnique;
	static grcBlendStateHandle m_BS_WriteDepthOnly;

	static void		ResetWater();
	static bool		m_occlusionJobStarted;

	// EJ: Box Occluder Optimization
	static s32 s_boxOccluderUsedCount;
	static s32 s_boxOccluderUsedIndex;
	static s32 s_boxOccluderUsedBucketCount;
	static CBoxOccluderBucketArray s_boxOccluderBuckets;

	// EJ: Occlude Model Optimization
	static s32 s_occludeModelUsedCount;
	static s32 s_occludeModelUsedIndex;
	static s32 s_occludeModelUsedBucketCount;
	static COccludeModelBucketArray s_occludeModelBuckets;

	static bool ms_bScriptDisabledOcclusionThisFrame;

	static atRangeArray<bool, SCAN_OCCLUSION_STORAGE_COUNT>			m_rasterizerInUse;
#if __BANK
	static atRangeArray<sysIpcMutex, SCAN_OCCLUSION_STORAGE_COUNT>	m_rasterizerMutexes;
	static bool		m_displayWaterOccluder;
	static bool		m_bDisplayOccluders;
	static bool		m_bWaterOnlyOccluders;
	static bool		m_bEnableDepthTest;
	static bool		m_enableBackFaceCulling;
	static int		m_debugDrawRasterizer;
#endif // __BANK
};

#endif
