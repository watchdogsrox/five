/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    occlusion.cpp
// PURPOSE : The game now uses the rage occlusion. This file contains only game specific stuff.
// AUTHOR :  Obbe.
// CREATED : 7/10/05
//
/////////////////////////////////////////////////////////////////////////////////
#include "renderer/occlusion.h"

#if !__SPU

// rage includes
#include "grcore/debugdraw.h"
#include "grcore/viewport.h"
#include "grcore/viewport_inline.h"
#include "bank/bkmgr.h"
#include "profile/timebars.h"
#include "phsolver\contactmgr.h" // for physics scratchpad memory
#include "physics\contact.h"
#include "phcore\pool.h"
#include "system/cache.h"
#include "vector/colors.h"
#include "softrasterizer/softrasterizer.h"
#include "softrasterizer/tiler.h"
#include "softrasterizer/boxoccluder.h"
#include "parser/manager.h"
#include "system/endian.h"
#include "system/dependencyscheduler.h"
#include "system/memory.h"

// framework headers
#include "fwutil/xmacro.h"
#include "fwscene/scan/Scan.h"
#if __BANK
#include "fwscene/stores/mapdatastore.h"
#include "fwdebug/picker.h"
#endif

// game includes
#include "camera/CamInterface.h"
#include "camera/viewports/ViewportManager.h"
#include "core/game.h"
#include "Renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "Renderer/RenderPhases/RenderPhaseParaboloidShadows.h"
#include "Renderer/RenderPhases/RenderPhaseWaterReflection.h"
#include "Renderer/Lights/lights.h"
#include "scene/entity.h"
#include "scene/portals/FrustumDebug.h"
#include "peds/PlayerInfo.h"
#include "debug/vectormap.h"
#include "timecycle/TimeCycle.h"

#include "fwsys/timer.h"
#include "system/task.h"
#include "grprofile/pix.h"

#include "renderer/Debug/Raster.h"

PARAM(occlusiondebugdraw, "Enable occlusion debug draw");
PARAM(noocclusiondebugdraw, "Disable occlusion debug draw");

#define _USE_DEPENDANT_TASKS_RASTERIZER 1

#define SPU_OCCLUSION_SETUP (__PPU)

DECLARE_TASK_INTERFACE( occlusionsync );
DECLARE_FRAG_INTERFACE( ScanOcclusionWrapper );


// -- per frame allocations support

struct PhysicsScratchPadAllocator
{
	u8* Alloc() { return PHSIM->GetContactMgr()->GetScratchpad();}
	void Free(u8*  ) { }
	int GetSize() { return PHSIM->GetContactMgr()->GetScratchpadSize(); }
	void setMaxUsed( int v) {return PHSIM->GetContactMgr()->MarkScratchpadUsed(v);}
};

typedef ScratchPadAllocator<PhysicsScratchPadAllocator>	OcclusionScratchPadAllocator;
OcclusionScratchPadAllocator	g_tempAllocator;
BANK_ONLY(bool					g_showPerFrameAllocations =  true);



bool				COcclusion::bWaterOcclusionDisabled = FALSE;
// 5/3/13 - cthomas - Disable rasterizing the water quads in GBuf, as it 
// causes performance issues with Occlusion and doesn't really help much at all.
bool				COcclusion::bRasterizeWaterQuadsInHiZ = false;

#if __DEV
bool				COcclusion::bOcclusionMeshesDisabled = FALSE;
bool				COcclusion::bOcclusionBoxesDisabled = FALSE;
#endif

// EJ: Box Occluder Optimization
s32	COcclusion::s_boxOccluderUsedCount = 0;
s32	COcclusion::s_boxOccluderUsedBucketCount = 0;
atRangeArray<CBoxOccluderBucket, MAX_NUM_BOX_OCCLUDER_BUCKETS> COcclusion::s_boxOccluderBuckets;

// EJ: Occlude Model Optimization
s32	COcclusion::s_occludeModelUsedCount = 0;
s32 COcclusion::s_occludeModelUsedIndex = 0;
s32	COcclusion::s_occludeModelUsedBucketCount = 0;
atRangeArray<COccludeModelBucket, MAX_NUM_OCCLUDE_MODEL_BUCKETS> COcclusion::s_occludeModelBuckets;

bool COcclusion::m_occlusionJobStarted = true;

bool COcclusion::ms_bScriptDisabledOcclusionThisFrame = false;

template<class T, int SIZE>
class SlottedList
{
	int								m_size;
	atRangeArray<T,SIZE>			m_models;
	atRangeArray<int,SIZE>			m_slots;
public:
	SlottedList() : m_size(0)
	{}

	int GetCount() const { return m_size; }
	const T operator[](int index) const
	{
		return m_models[index];
	}
	void Add( T& v, int slot)
	{
		Assert(m_size < SIZE);
		if (m_size < SIZE)
		{
			m_models[m_size]=v;
			m_slots[m_size++]=slot;
		}
	}
	template<class Remover>
	void DestroyLessEqual(int slot, Remover rem)
	{

		for (int x=m_size-1; x>=0; x--)
		{
			if (m_slots[x] <= slot)
			{
				// Delete this entry.
				rem(m_models[x]);
				m_slots[x] = m_slots[--m_size];
				m_models[x] = m_models[m_size];
			}
		}
	}
	template<class Remover>
	void DestroyEqual(int slot, Remover rem)
	{

		for (int x=m_size-1; x>=0; x--)
		{
			if (m_slots[x] == slot)
			{
				// Delete this entry.
				rem(m_models[x]);
				m_slots[x] = m_slots[--m_size];
				m_models[x] = m_models[m_size];
			}
		}
	}
};


#if SUPPORT_OCCLUDER_MESH
#define MAX_NUM_SHADOW_MODELS		128
SlottedList<CheapShadowMesh*,MAX_NUM_SHADOW_MODELS>		ShadowModels;
SlottedList<CheapShadowMesh*,MAX_NUM_SHADOW_MODELS>		FreedShadowModels;
#endif


atRangeArray<SoftRasterizer*, SCAN_OCCLUSION_STORAGE_COUNT>	COcclusion::m_rasterizers;
atRangeArray<Mat44V, SCAN_OCCLUSION_TRANSFORM_COUNT>		COcclusion::m_transMtx;
atRangeArray<float, SCAN_OCCLUSION_TRANSFORM_COUNT>			COcclusion::m_rasDepth;

float				COcclusion::m_occluderThreshold[SCAN_OCCLUSION_STORAGE_COUNT];
float				COcclusion::m_boxoccluderThreshold[SCAN_OCCLUSION_STORAGE_COUNT];
bool				COcclusion::m_primeZBuffer=false;
float				COcclusion::m_GBufTestBias = 0.00002f;

float				COcclusion::m_ShadowTestBias = 0.0001f;

float				COcclusion::m_waterQuadHiZBias = -1.5f;
float				COcclusion::m_waterQuadStencilBias = -0.25f; // this is enough to fix BS#1372055
grcEffectTechnique	COcclusion::m_primeDepthTechnique;
grcBlendStateHandle	COcclusion::m_BS_WriteDepthOnly;


atRangeArray<bool, SCAN_OCCLUSION_STORAGE_COUNT>			COcclusion::m_rasterizerInUse;
#if __BANK
atRangeArray<sysIpcMutex, SCAN_OCCLUSION_STORAGE_COUNT>		COcclusion::m_rasterizerMutexes;
bool	COcclusion::m_displayWaterOccluder = false;
bool	COcclusion::m_bDisplayOccluders = false;
bool	COcclusion::m_bWaterOnlyOccluders = false;
bool	COcclusion::m_bEnableDepthTest = true;
bool	COcclusion::m_enableBackFaceCulling=true;
int		COcclusion::m_debugDrawRasterizer;

u32		COcclusion::m_NumOfObjectsOccluded[SCAN_OCCLUSION_STORAGE_COUNT+1] = { 0};
u32		COcclusion::m_NumOfOcclusionTests[SCAN_OCCLUSION_STORAGE_COUNT+1] = { 0  };
u32		COcclusion::m_NumObjectsTrivialAcceptFrustum[SCAN_OCCLUSION_STORAGE_COUNT+1] = { 0 };
u32		COcclusion::m_NumObjectsTrivialAcceptMinZ[SCAN_OCCLUSION_STORAGE_COUNT+1] = { 0 };
u32		COcclusion::m_NumObjectsTrivialAcceptVisPixel[SCAN_OCCLUSION_STORAGE_COUNT+1] = { 0 };

u32 g_Occluded[SCAN_OCCLUSION_STORAGE_COUNT+1] = { 0 };
u32 g_OccludeTested[SCAN_OCCLUSION_STORAGE_COUNT+1] = { 0 };
u32 g_TrivialAcceptActiveFrustum[SCAN_OCCLUSION_STORAGE_COUNT + 1] = { 0 };
u32 g_TrivialAcceptMinZ[SCAN_OCCLUSION_STORAGE_COUNT + 1] = { 0 };
u32 g_TrivialAcceptVisPixel[SCAN_OCCLUSION_STORAGE_COUNT + 1] = { 0 };
#endif

#define MAX_WATER_STENCIL_VERTS 3600 // actual count is 3136 with current water quads, this is likely to change ..

typedef atRangeArray<Vec3V, MAX_WATER_STENCIL_VERTS>  WaterOcclusionVertsType;		
WaterOcclusionVertsType*  WaterOcclusionVertsStencil;
u32   NumWaterOcclusionVertsStencil  =0;

WaterOcclusionVertsType* WaterOcclusionVertsHiZ;
u32   NumWaterOcclusionVertsHiZ  =0;

bool	g_AddWaterStencil = false;
float	g_SelectExactQueryThreshold = 0.004f; //0.3f;

Vec4V*	m_tempHiZBuffer = 0;


#if __BANK
bool g_useSimpleQueries =  false;
bool g_useNewOcclusionAABBExact =  true;
bool g_useTrivialAcceptTest = true;
bool g_ForceExactOcclusionTest = true;
bool g_RenderActiveFrustum = false;
bool g_UseTrivialAcceptVisiblePixelTest = true;
bool g_UseProjectedShadowOcclusionTest= true;
bool g_UseProjectedShadowOcclusionClipping= true;
bool g_DebugFreezeProjectedShadowBbox = false;
bool g_DebugTestSelectedEntityMinZ = false;
bool g_DebugTestSelectedEntityDisplayInfo = false;
bool g_DebugRenderSelectedEntityAABB = false;
bool g_DebugRenderSelectedEntityHull = false;
bool g_useMinZClipping = true;
bool g_DebugRenderSelectedEntityShadowOOBB = false;
bool g_DebugRenderSelectedEntityShadowHullOOBB = false;
float g_DebugSelectedEntityMinZValue = 0.0f;
float g_DebugSelectedEntityShadowMinZValue = 0.0f;
float g_DebugMinZBufferValue = 0.0f;

ScalarV g_ActiveFrustumBoundsMinX = ScalarV(V_ZERO);
ScalarV g_ActiveFrustumBoundsMinY = ScalarV(V_ZERO);
ScalarV g_ActiveFrustumBoundsMaxX = ScalarV(V_ONE);
ScalarV g_ActiveFrustumBoundsMaxY = ScalarV(V_ONE);
ScalarV g_newOcclussionAABBExactPixelsThresholdSimple = THRESHOLD_PIXELS_SIMPLE_TEST;
#endif


extern bool		g_render_lock;

atRangeArray<rstTransformToScreenSpace, SCAN_OCCLUSION_TRANSFORM_COUNT> s_HiZTransform;


#if SUPPORT_OCCLUDER_MESH

class StreamingMeshAllocator : public CheapShadowMesh::MeshAllocator
{
public:
	void* Allocate(int size )
	{
		USE_MEMBUCKET(MEMBUCKET_RENDER);
		MEM_USE_USERDATA(MEMUSERDATA_MESH_ALLOC);
		return strStreamingEngine::GetAllocator().Allocate( size, 128, MEMTYPE_RESOURCE_PHYSICAL);		
	}
	void Free( void* address )
	{
		MEM_USE_USERDATA(MEMUSERDATA_MESH_ALLOC);

		if (address)
			strStreamingEngine::GetAllocator().Free( address);
	}
	bool IsDynamic()
	{
	#if __XENON
		return true;	// 360: see CL#2272295 (to avoid "ERR[D3D]: Resource is in cached memory but wasn't created as D3DUSAGE_CPU_CACHED_MEMORY.")
	#elif __PS3
		return false;	// true=MainRam; false=VRam
	#else
		return false;
	#endif
	}
};
StreamingMeshAllocator g_MeshAllocator;

#endif
#if STENCIL_WRITER_DEBUG
static bool		g_debugStencilEnableWaterStencil = true;
static bool		g_debugStencilSimpleClipper = false;
static int		g_debugStencilBoundsExpandX0 = 0;
static int		g_debugStencilBoundsExpandY0 = 0;
static int		g_debugStencilBoundsExpandX1 = 0;
static int		g_debugStencilBoundsExpandY1 = 0;
static float	g_debugStencilPixelOffsetX = 0.0f;
static float	g_debugStencilPixelOffsetY = 0.0f;
#endif // STENCIL_WRITER_DEBUG

#if __DEV
#define MAX_DEBUG_BOUNDS 2048
spdAABB	g_debugBounds[MAX_DEBUG_BOUNDS];
Color32 g_debugColor[MAX_DEBUG_BOUNDS];

atRangeArray<Vec3V,MAX_DEBUG_BOUNDS> g_debugLines;
atRangeArray<bool,MAX_DEBUG_BOUNDS>  g_debugLinesResult;
u32		g_debugBoundsCnt =0;
u32		g_debugBoundsLinesCnt =0;
bool	g_debugBoundsActive = false;

void COcclusion::AddDebugBound( Vec3V_In aabbMin, Vec3V_In aabbMax, bool res  )
{
	if ( g_debugBoundsActive && g_debugBoundsCnt < MAX_DEBUG_BOUNDS )
	{
		int cnt=sysInterlockedIncrement( &g_debugBoundsCnt);
		g_debugColor[cnt-1]= res ? Color32(0.f,1.f,0.f) : Color32(1.f,1.f,1.f);
		g_debugBounds[cnt-1]= spdAABB( aabbMin, aabbMax);		
	}	
}
void COcclusion::RenderDebugBounds()
{
	for (u32 i=0;i<g_debugBoundsCnt;i++ )
	{
		grcWorldIdentity();
		Vec3V minp = g_debugBounds[i].GetMin();
		Vec3V maxp = g_debugBounds[i].GetMax();
		grcDrawBox( VEC3V_TO_VECTOR3(minp),  VEC3V_TO_VECTOR3(maxp), g_debugColor[i]);
	}
	g_debugBoundsCnt =0;
	if ( !g_debugBoundsLinesCnt)
		return;

	grcWorldIdentity();
	
	for (int i = 0; i < g_debugBoundsLinesCnt; i+=3)
	{
		grcColor( g_debugLinesResult[i] ? Color32(0.f,1.f,0.f) : Color32(1.f,1.f,1.f) );

		grcBegin(drawLines, 6);
			grcVertex3f( VEC3V_TO_VECTOR3(g_debugLines[i] ));
			grcVertex3f( VEC3V_TO_VECTOR3(g_debugLines[i+1] ));

			grcVertex3f( VEC3V_TO_VECTOR3(g_debugLines[i+1] ));
			grcVertex3f( VEC3V_TO_VECTOR3(g_debugLines[i+2] ));

			grcVertex3f( VEC3V_TO_VECTOR3(g_debugLines[i+2] ));
			grcVertex3f( VEC3V_TO_VECTOR3(g_debugLines[i] ));
		grcEnd();
	}
	g_debugBoundsLinesCnt = 0;
}

void COcclusion::AddDebugLines( Vec3V* pts, u8* indices,  int numIndices, bool res )
{
	if ( g_debugBoundsActive && g_debugBoundsLinesCnt < (MAX_DEBUG_BOUNDS-numIndices) )
	{
		for (int i = 0; i< numIndices; i++)
		{
			g_debugLinesResult[g_debugBoundsLinesCnt] = res;
			g_debugLines[ g_debugBoundsLinesCnt++] = pts[indices[i]];			
		}
	}
}
#endif // __BANK


XPARAM(si_auto);

//
// name:			Init
// description:		Sets stuff up
//
void COcclusion::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
#if __DEV && __PS3
		if (PARAM_si_auto.Get()) // disable occlusion when running automatic streaming iterators
		{
			bWaterOcclusionDisabled = true;
			bOcclusionMeshesDisabled = true;
			bOcclusionBoxesDisabled = true;
		}
#endif // __DEV && __PS3

#if SUPPORT_OCCLUDER_MESH
		CheapShadowMesh::Init();
#endif		
		OccludeModel::InitClass();// Init base stateblocks for debug drawing

		USE_MEMBUCKET(MEMBUCKET_RENDER);
		for (int i = 0; i < SCAN_OCCLUSION_STORAGE_COUNT; ++i)
		{
			m_rasterizers[i] = rage_new SoftRasterizer(NULL, false );
			m_rasterizerInUse[i] = false;
#if __BANK			
			m_rasterizerMutexes[i] = sysIpcCreateMutex();
#endif
		}

		m_boxoccluderThreshold[SCAN_OCCLUSION_STORAGE_PRIMARY] = 20.f;
		m_boxoccluderThreshold[SCAN_OCCLUSION_STORAGE_CASCADE_SHADOWS] = .5f;

		m_occluderThreshold[SCAN_OCCLUSION_STORAGE_PRIMARY] = 96.f;
		m_occluderThreshold[SCAN_OCCLUSION_STORAGE_CASCADE_SHADOWS] = .5f;
		
		grcEffect& defaultEffect = GRCDEVICE.GetDefaultEffect();
		m_primeDepthTechnique = defaultEffect.LookupTechnique("WriteDepthOnlyMRT");

		grcBlendStateDesc bsDesc;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_NONE;
		
		m_BS_WriteDepthOnly = grcStateBlock::CreateBlendState(bsDesc);
		Assert(m_BS_WriteDepthOnly != grcStateBlock::BS_Invalid);

#if __BANK
		if ( PARAM_occlusiondebugdraw.Get() )
			m_bDisplayOccluders = true;

		if ( PARAM_noocclusiondebugdraw.Get() )
			m_bDisplayOccluders = false;
#endif
    }
    else if(initMode == INIT_BEFORE_MAP_LOADED)
    {
		// EJ: Box Occluder Optimization
		s_boxOccluderUsedCount = 0;
	    s_boxOccluderUsedBucketCount = 0;

		// EJ: Occlude Model Optimization
		s_occludeModelUsedCount = 0;
		s_occludeModelUsedIndex = 0;
		s_occludeModelUsedBucketCount = 0;

		NumWaterOcclusionVertsHiZ = 0;
		NumWaterOcclusionVertsStencil = 0;
		WaterOcclusionVertsStencil = 0;	 
		WaterOcclusionVertsHiZ = 0;
   }
  
	// can prime it now if we want to
	m_primeZBuffer=false;
}

//
// name:			Shutdown
// description:		Does everything needed when shutting down.
//

void COcclusion::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
		for (int i = 0; i < SCAN_OCCLUSION_STORAGE_COUNT; ++i)
		{
			delete m_rasterizers[i];
			m_rasterizers[i] = NULL;

			BANK_ONLY(sysIpcDeleteMutex( m_rasterizerMutexes[i] ));
		}

#if SUPPORT_OCCLUDER_MESH
		CheapShadowMesh::Destroy();
#endif
    }
}
Vec4V* COcclusion::GetHiZBuffer(const int occluderPhase) {
	return m_rasterizers[ occluderPhase ]->GetHiZBuffer();
}

Vec4V_Out COcclusion::GetMinMaxBoundsForBuffer(const int occluderPhase)
{
	return m_rasterizers[occluderPhase]->GetMinMaxBounds();
}

float COcclusion::GetMinZForBuffer(const int occluderPhase)
{
	return m_rasterizers[occluderPhase]->GetMinimumZ();
}

Vec4V* COcclusion::GetHiStencilBuffer()
{
	return m_rasterizers[ SCAN_OCCLUSION_STORAGE_PRIMARY ]->GetHiStencilBuffer();
}

void	COcclusion::WaitForScanToFinish(const int occluderPhase)
{
	m_rasterizers[occluderPhase]->WaitForScanTask();
}

void	COcclusion::WaitForRasterizerToFinish(const int occluderPhase)
{	
	PIXBegin(1,"Wait on Rasterizer");
	m_rasterizers[occluderPhase]->Wait();
#if __BANK	
	if ( m_rasterizerInUse[occluderPhase] )
	{
		m_rasterizerInUse[occluderPhase] = false;
		AssertVerify( sysIpcUnlockMutex( m_rasterizerMutexes[occluderPhase] ) );
	}
#endif
	m_rasterizerInUse[occluderPhase] = false;
	PIXEnd();
}

void	COcclusion::WaitForAllRasterizersToFinish()
{
	PF_AUTO_PUSH_TIMEBAR("WaitForAllRasterizersToFinish");

	for (int i = 0; i < SCAN_OCCLUSION_STORAGE_COUNT; ++i)
		COcclusion::WaitForRasterizerToFinish(i);

#if __BANK
	Vec4V minMaxActiveFrustum = m_rasterizers[SCAN_OCCLUSION_STORAGE_PRIMARY]->GetMinMaxBounds();
	g_ActiveFrustumBoundsMinX = minMaxActiveFrustum.GetX();
	g_ActiveFrustumBoundsMinY = minMaxActiveFrustum.GetY();
	g_ActiveFrustumBoundsMaxX = minMaxActiveFrustum.GetZ();
	g_ActiveFrustumBoundsMaxY = minMaxActiveFrustum.GetW();
#endif

	if (OcclusionEnabled())
		 FreePerFrameResources();
	//NumWaterOcclusionVerts = 0; // -- don't reset these here, then we can't draw them in COcclusion::DrawDebug
	//NumWaterOcclusionIndices = 0;
}

// Temporary selected model and box occluder data

const int		MaxNumberBoxOccludersRasterized = MAX_NUM_BOX_OCCLUDERS_RASTERIZED;
const int		MaxNumberOccludeModelsRasterized = MAX_NUM_OCCLUDE_MODELS;

typedef atRangeArray<atRangeArray<u8, MaxNumberBoxOccludersRasterized+16>, SCAN_OCCLUSION_STORAGE_COUNT> BoxOccluderClippedType;
typedef atRangeArray<atRangeArray<BoxOccluder, MaxNumberBoxOccludersRasterized+16>, SCAN_OCCLUSION_STORAGE_COUNT> RasterizedBoxOccludersType;

BoxOccluderClippedType*			isBoxOccluderClipped =0;
RasterizedBoxOccludersType*		rasterizedBoxOccluders =0;

typedef atRangeArray<atRangeArray<OccludeModel, MaxNumberOccludeModelsRasterized>, SCAN_OCCLUSION_STORAGE_COUNT> RasterizedOccludeModelsType;
typedef atRangeArray<atRangeArray<u8, MaxNumberOccludeModelsRasterized>, SCAN_OCCLUSION_STORAGE_COUNT> OccludeModelsClippedType;

OccludeModelsClippedType* 			isOccludeModelClipped=0;
RasterizedOccludeModelsType*		rasterizedOccludeModels=0;

OccludeModel *						g_waterOccluders = 0;
u8*									g_waterOccludersClipped = 0;

//------------

void COcclusion::SetOcclusionTransform(const int occlusionTransform, const grcViewport &vp)
{
	// prep Hi Z transforms
	rstTransformToScreenSpace trans( &vp, true NV_SUPPORT_ONLY(, occlusionTransform == SCAN_OCCLUSION_TRANSFORM_PRIMARY));
	m_transMtx[ occlusionTransform ] = trans.m_comMtx;
	m_rasDepth[ occlusionTransform ] = trans.GetRescaleDepth().Getf();
	s_HiZTransform[ occlusionTransform ] = trans;
}

void COcclusion::RasterizeOccluders(const int occluderPhase, const grcViewport &vp)
{
	Assert( m_rasterizerInUse[occluderPhase]== false );
	if ( m_rasterizerInUse[occluderPhase] )
		return; 
	m_rasterizerInUse[occluderPhase] = true;

#if __BANK
	

	
	AssertVerify( sysIpcLockMutex( m_rasterizerMutexes[occluderPhase] ) );


	g_Occluded[occluderPhase] = m_NumOfObjectsOccluded[occluderPhase];
	g_OccludeTested[occluderPhase] = m_NumOfOcclusionTests[occluderPhase];
	g_TrivialAcceptActiveFrustum[occluderPhase] = m_NumObjectsTrivialAcceptFrustum[occluderPhase];
	g_TrivialAcceptMinZ[occluderPhase] = m_NumObjectsTrivialAcceptMinZ[occluderPhase];
	g_TrivialAcceptVisPixel[occluderPhase] = m_NumObjectsTrivialAcceptVisPixel[occluderPhase];

	m_NumOfObjectsOccluded[occluderPhase] = 0;
	m_NumOfOcclusionTests[occluderPhase] = 0;
	m_NumObjectsTrivialAcceptFrustum[occluderPhase] = 0;
	m_NumObjectsTrivialAcceptMinZ[occluderPhase] = 0;
	m_NumObjectsTrivialAcceptVisPixel[occluderPhase] = 0;

	// Copy lights and particles results
	g_Occluded[SCAN_OCCLUSION_STORAGE_COUNT] = m_NumOfObjectsOccluded[SCAN_OCCLUSION_STORAGE_COUNT];
	g_OccludeTested[SCAN_OCCLUSION_STORAGE_COUNT] = m_NumOfOcclusionTests[SCAN_OCCLUSION_STORAGE_COUNT];
	
	m_NumOfObjectsOccluded[SCAN_OCCLUSION_STORAGE_COUNT] = 0;
	m_NumOfOcclusionTests[SCAN_OCCLUSION_STORAGE_COUNT] = 0;
#endif

/*	if (occluderPhase==SCAN_OCCLUSION_STORAGE_CASCADE_SHADOWS){
		
		ShadowSquares squares;
		squares.Default();
		
		fwScanCascadeShadowInfo scanInfo = CRenderPhaseCascadeShadowsInterface::GetScanCascadeShadowInfo();
		const grcViewport& cvp=CRenderPhaseCascadeShadowsInterface::GetLastCascadeViewport();
		rstTransformToScreenSpace trans( &cvp, false);
		Vec4V px,py,pz,rr;
		scanInfo.GetCascades(trans.m_comMtx, px, py, pz, rr);

		squares.m_spheresX = px;
		squares.m_spheresY = py;
		squares.m_spheresR = rr;

		//squares.m_spheresR =ScalarV(0.25);
		squares.m_spheresR.SetW(ScalarV(V_ZERO));

		Vec4V xaxis = Multiply(trans.m_comMtx, Vec4V(V_X_AXIS_WZERO) );
		Vec4V yaxis = Multiply(trans.m_comMtx, Vec4V(V_Y_AXIS_WZERO) );

		squares.m_axisXY=Vec4V( xaxis.GetXY(),yaxis.GetXY());

		m_rasterizers[occluderPhase]->setShadowSquares(squares);
	}*/

	// EJ: Box Occluder Optimization	
	int numBoxOccluderUsedBuckets = s_boxOccluderUsedBucketCount;

	// EJ: Occlude Model Optimization
	// 5/3
#if RSG_XENON || RSG_PS3
	// 5/3/13 - cthomas - Disable using model (mesh) occluders in Cascade Shadow Occlusion, as they are 
	// very expensive, causing a performance problem with Occlusion being done in time before Visibility 
	// scanning needs to start, and they offer only minimal benefit.
	int numOccludeModelUsedBuckets = (occluderPhase == SCAN_OCCLUSION_STORAGE_CASCADE_SHADOWS) ? 0 : s_occludeModelUsedBucketCount;	
#else
	int numOccludeModelUsedBuckets = s_occludeModelUsedBucketCount;	
#endif

#if __DEV
	if (bOcclusionMeshesDisabled)
	{
		numOccludeModelUsedBuckets = 0;
	}

	if (bOcclusionBoxesDisabled)
	{
		numBoxOccluderUsedBuckets = 0;
	}

#endif // __DEV


#if _USE_DEPENDANT_TASKS_RASTERIZER

	m_rasterizers[occluderPhase]->BeginSubmit(&vp NV_SUPPORT_ONLY(,occluderPhase == SCAN_OCCLUSION_STORAGE_PRIMARY));

	PIXBegin(1,"Fire Box Occluders Task");
	m_rasterizers[occluderPhase]->SelectAndRasterizeBoxOccluders( numBoxOccluderUsedBuckets, s_boxOccluderBuckets.GetElements(), 
		&(*isBoxOccluderClipped)[occluderPhase][0], MaxNumberBoxOccludersRasterized, m_boxoccluderThreshold[occluderPhase],
		&(*rasterizedBoxOccluders)[occluderPhase][0] );
	PIXEnd();

	PIXBegin(1,"Fire Occlude Models Task");
	m_rasterizers[occluderPhase]->SelectAndRasterizeModelOccluders( numOccludeModelUsedBuckets, s_occludeModelBuckets.GetElements(), 
		&(*isOccludeModelClipped)[occluderPhase][0], m_occluderThreshold[occluderPhase],
		&(*rasterizedOccludeModels)[occluderPhase][0] );
	PIXEnd();

	if(bRasterizeWaterQuadsInHiZ && occluderPhase == SCAN_OCCLUSION_STORAGE_PRIMARY)
	{
		m_rasterizers[occluderPhase]->PrepareSubmitGenerateWaterTris(&(*WaterOcclusionVertsHiZ)[0], 
			&NumWaterOcclusionVertsHiZ);
	}

	bool jobStarted = m_rasterizers[occluderPhase]->EndSubmit( NULL, false );
	m_occlusionJobStarted = m_occlusionJobStarted || jobStarted;
#else
	BoxOccluder rasterizedBoxOccluders[MaxNumberBoxOccludersRasterized];
	rstTransformToScreenSpace clipAndTransform(&vp,false);  // move to start function


	int amtOcc = rage::rstSelectBoxOccluders( clipAndTransform, s_boxOccluderBuckets.GetElements(), (*rasterizedBoxOccluders), 
		(*isBoxOccluderClipped),
		numBoxOccluderUsedBuckets, 
		MaxNumberBoxOccludersRasterized, ScalarVFromF32( m_occluderThreshold)
		);

	m_rasterizers[occluderPhase]->BeginSubmit(&vp);
	m_rasterizers[occluderPhase]->Reset();
	for (int i =0; i < amtOcc; i++)
	{
		Vec3V verts[8];
		(*rasterizedBoxOccluders)[i].CalculateVerts( verts);

		m_rasterizers[occluderPhase]->RasterizeModel(
			verts, 
			BoxOccluder::GetOccluderIndices(), 
			BoxOccluder::GetNumOccluderIndices(),
			8,
			(*isBoxOccluderClipped)[i]!=0);
	}
	bool jobStarted = m_rasterizers[occluderPhase]->EndSubmit( NULL, false );
	m_occlusionJobStarted = m_occlusionJobStarted || jobStarted;
#endif
}

void COcclusion::BeforeRender()
{
}

static Vec3V IcosahedronPts[]  =
{ 
	Vec3V( 0.f  ,-0.525731f,  0.850651f),
	Vec3V( 0.850651f , 0.f,  0.525731f),
	Vec3V( 0.850651f , 0.f , -0.525731f),
	Vec3V( -0.850651f , 0.f,  -0.525731f),
	Vec3V( -0.850651f , 0.f,  0.525731f),
	Vec3V( -0.525731f,  0.850651f,  0.f),
	Vec3V( 0.525731f, 0.850651f , 0.f),
	Vec3V( 0.525731f , -0.850651f , 0.f),
	Vec3V( -0.525731f,  -0.850651f , 0.f),
	Vec3V( 0.f , -0.525731f , -0.850651f),
	Vec3V( 0.f , 0.525731f,  -0.850651f),
	Vec3V( 0.f , 0.525731f,  0.850651f)
};
static u8 IcosahedronIndices[]  =
{
	2-1,3-1,7-1,
	2-1  ,8-1,  3-1,
	4-1 , 5-1 , 6-1,
	5-1 , 4-1,  9-1,
	7-1 , 6-1 , 12-1,
	6-1 , 7-1 , 11-1,
	10-1 , 11-1 , 3-1,
	11-1,  10-1 , 4-1,
	8-1 , 9-1  ,10-1,
	9 -1 ,8-1 , 1-1,
	12-1 , 1-1 , 2-1,
	1-1  ,12-1 , 5-1,
	7-1  ,3-1 , 11-1,
	2-1  ,7-1 , 12-1,
	4-1 , 6-1 , 11-1,
	6-1 , 5-1 , 12-1,
	3-1 , 8-1  ,10-1,
	8-1 , 2-1 , 1-1,
	4-1 , 10-1 , 9-1,
	5-1 , 9-1 , 1-1,
};
bool COcclusion::IsSphereVisible(const spdSphere& sphere) 
{
	// generate a icosahedron
	Vec3V pointList[ NELEM(IcosahedronPts)];
	for (int i = 0;i< NELEM(IcosahedronPts) ;i++)
	{
		pointList[i]=AddScaled( sphere.GetCenter(), IcosahedronPts[i], sphere.GetRadius() );
	}
	ScalarV area = rstTestModelExact( s_HiZTransform[ SCAN_OCCLUSION_STORAGE_PRIMARY ].m_comMtx, 
			s_HiZTransform[ SCAN_OCCLUSION_STORAGE_PRIMARY ].GetRescaleDepth(), 
			GetHiZBuffer(SCAN_OCCLUSION_STORAGE_PRIMARY), pointList, IcosahedronIndices,
			NELEM(IcosahedronPts), NELEM(IcosahedronIndices), ScalarV(V_ZERO));

	bool result = IsTrue( area > ScalarV(V_ZERO));
#if __DEV
	AddDebugLines( pointList, IcosahedronIndices,  NELEM(IcosahedronIndices), result);
#endif
	return result;
//	return COcclusion::IsBoxVisible( spdAABB(sphere) );
}

static Vec3V ConePts[]  =
{ 
	Vec3V( 0.f  ,-0.525731f,  0.850651f),
	Vec3V( 0.850651f , 0.f,  0.525731f),
	Vec3V( 0.850651f , 0.f , -0.525731f),
	Vec3V( 0.525731f , 0.850651f , 0.f),
	Vec3V( 0.525731f , -0.850651f , 0.f),
	Vec3V( 0.f , -0.525731f , -0.850651f),
	Vec3V( 0.f , 0.525731f,  -0.850651f),
	Vec3V( 0.f , 0.525731f,  0.850651f)
};
static u8 ConeIndices[]  =
{
	2,3,4,
	2  ,5,  3,
	0 , 4 , 7,
	6 , 7 , 3,
	7,  6 , 0,
	5 , 0  ,6,
	0 ,5 , 1,
	8, 1 , 2,
	1 ,8 , 0,
	4  ,3 , 7,
	2  ,4 , 8,			
	3 , 5  ,6,
	5 , 2 , 1,	
};
Vec3V_Out AddScaled4( Vec3V_In pos,Vec3V_In v1, ScalarV_In v1s,Vec3V_In v2,ScalarV_In v2s, Vec3V_In v3, ScalarV_In v3s)
{
	return AddScaled(AddScaled( AddScaled( pos, v1, v1s), v2, v2s ), v3, v3s);
}

bool COcclusion::IsConeVisible(const Mat34V& mtx, float radius, float spreadf )
{
	Assert( IsTrueXYZ(IsNotNan(mtx.a())));
	Assert( IsTrueXYZ(IsNotNan(mtx.b())));
	Assert( IsTrueXYZ(IsNotNan(mtx.c())));
	Assert( IsTrueXYZ(IsNotNan(mtx.d())));
	// generate a icosahedron
	Vec3V pointList[ NELEM(ConePts)+1];
	const Vec3V a = mtx.a();
	const Vec3V b = mtx.b();
	const Vec3V c = mtx.c();

	// Get location of the cap.
	const ScalarV computedRange = ScalarVFromF32(radius); 

	const ScalarV spread = ScalarVFromF32(spreadf);
	// const ScalarV halfSpread = Scale( spread, ScalarV(V_HALF));
	const Vec3V endcap = AddScaled( mtx.d(), c, -computedRange /* +halfSpread */);

	pointList[0] = mtx.d();
	const Vec3V as = a*spread;
	const Vec3V bs = b* spread;
	const Vec3V cs = c * -spread;
	for (int i = 0; i<NELEM(ConePts); i++)
	{
		pointList[i+1]= AddScaled4( endcap, as,  ConePts[i].GetY(), bs,  ConePts[i].GetZ() , cs, ConePts[i].GetX());  
	}

	ScalarV area = rstTestModelExact( s_HiZTransform[ SCAN_OCCLUSION_STORAGE_PRIMARY ].m_comMtx, 
			s_HiZTransform[ SCAN_OCCLUSION_STORAGE_PRIMARY ].GetRescaleDepth(), 
		GetHiZBuffer(SCAN_OCCLUSION_STORAGE_PRIMARY), pointList, ConeIndices,
		NELEM(ConePts)+1, NELEM(ConeIndices), ScalarV(V_ZERO));

	bool result = IsTrue( area > ScalarV(V_ZERO));
#if __DEV
	AddDebugLines( pointList, ConeIndices,  NELEM(ConeIndices), result);
#endif
	return result;
	//	return COcclusion::IsBoxVisible( spdAABB(sphere) );
}
#if SUPPORT_OCCLUDER_MESH
static void DeleteShadowModelDeferred( CheapShadowMesh* smesh )
{
	FreedShadowModels.Add( smesh, TIME.GetFrameCount());
}
#endif

// EJ: Box Occluder Optimization
void COcclusion::AddBoxOccluder(CBoxOccluderArray& boxOccluders, s32 mapDataSlotIndex)
{
	// Don't add more than we can store!
	if(Verifyf((s_boxOccluderUsedCount + boxOccluders.GetCount()) < MAX_NUM_BOX_OCCLUDERS, "Box occluders array full - this one will be ignored") && 
		Verifyf(s_boxOccluderUsedBucketCount < MAX_NUM_BOX_OCCLUDER_BUCKETS, "COcclusion: ERROR - Ran out of box occluder buckets!"))	
	{
		Assert16(boxOccluders.GetElements());

		// Only continue if the occluder isn't shit
		if ((reinterpret_cast<size_t>(boxOccluders.GetElements()) & 0x0F) == 0)
		{
			CBoxOccluderBucket* pBucket = &s_boxOccluderBuckets[s_boxOccluderUsedBucketCount++];
			pBucket->count = boxOccluders.GetCount();
			pBucket->mapDataSlotIndex = mapDataSlotIndex;
			pBucket->pElements = boxOccluders.GetElements();
			pBucket->pArray = &boxOccluders;

			s_boxOccluderUsedCount += pBucket->count;
		}
	}
}

void COcclusion::RemoveBoxOccluder(s32 mapDataSlotIndex)
{
	// Delete all objects with matching map indices.
	for (int i = s_boxOccluderUsedBucketCount - 1; i >= 0; i--)
	{
		CBoxOccluderBucket& bucket = s_boxOccluderBuckets[i];

		if (bucket.mapDataSlotIndex == mapDataSlotIndex)
		{
			// Delete this entry
			s_boxOccluderUsedCount -= bucket.count;
			bucket = s_boxOccluderBuckets[--s_boxOccluderUsedBucketCount];
		}
	}
}

void COcclusion::AddOccludeModel(COccludeModelArray& occludeModels, s32 mapDataSlotIndex)
{
	// Don't add more than we can store!
	if(Verifyf((s_occludeModelUsedCount + occludeModels.GetCount()) < MAX_NUM_OCCLUDE_MODELS, "Occlude models array full - this one will be ignored") && 
		Verifyf(s_occludeModelUsedBucketCount < MAX_NUM_OCCLUDE_MODEL_BUCKETS, "COcclusion: ERROR - Ran out of occlude model buckets!"))	
	{
		Assert16(occludeModels.GetElements());

		// Only continue if the occluder isn't shit
		if ((reinterpret_cast<size_t>(occludeModels.GetElements()) & 0x0F) == 0)
		{
			// EJ: In-place endian swap
			for (int i = 0; i < occludeModels.GetCount(); ++i)
			{
				OccludeModel& in = occludeModels[i];
				if ( !OccludeModel::ByteSwapOccludeModel(in) ){
					Assertf(false," COcclusion: Error invalid occlude model for mapdata %i", mapDataSlotIndex);
					return;
				}
			}

			COccludeModelBucket* pBucket = &s_occludeModelBuckets[s_occludeModelUsedBucketCount++];
			pBucket->count = occludeModels.GetCount();
			pBucket->mapDataSlotIndex = mapDataSlotIndex;
			pBucket->pElements = occludeModels.GetElements();
			pBucket->pArray = &occludeModels;

			s_occludeModelUsedCount += pBucket->count;
			s_occludeModelUsedIndex = 0;

	#if SUPPORT_OCCLUDER_MESH
			// EJ: I don't want to touch this code right now, but possibly another area for future optimization?
			CheapShadowMesh* smesh = rage_new CheapShadowMesh(occludeModels.begin(), occludeModels.GetCount(), &g_MeshAllocator);
			if (!smesh || !smesh->IsValid())
			{
				if (smesh)
				{
					smesh->Release(&g_MeshAllocator );
					delete smesh;
				}
			}
			else
			{
				ShadowModels.Add(smesh, mapDataSlotIndex);	
			}
	#endif
		}
	}
}

void COcclusion::RemoveOccludeModel(s32 mapDataSlotIndex)
{
	// Delete all objects with matching map indices.
	for (int i = s_occludeModelUsedBucketCount - 1; i >= 0; i--)
	{
		COccludeModelBucket& bucket = s_occludeModelBuckets[i];

		if (bucket.mapDataSlotIndex == mapDataSlotIndex)
		{
			// EJ: Deferred delete in CMapData::~CMapData()
			s_occludeModelUsedCount -= bucket.count;
			bucket = s_occludeModelBuckets[--s_occludeModelUsedBucketCount];
		}
	}

#if SUPPORT_OCCLUDER_MESH
	ShadowModels.DestroyEqual(mapDataSlotIndex, DeleteShadowModelDeferred);
#endif
}



void COcclusion::AllocatePerFrameResources()
{
	Assert( WaterOcclusionVertsStencil == 0);
	Assert( WaterOcclusionVertsHiZ == 0);

	g_tempAllocator = OcclusionScratchPadAllocator(BANK_SWITCH(g_showPerFrameAllocations, false) );
	for (int i = 0; i < SCAN_OCCLUSION_STORAGE_COUNT; ++i)
	{
		m_rasterizers[i]->SetTileList(g_tempAllocator.Alloc<rstTileList>() );
	}
	WaterOcclusionVertsStencil = g_tempAllocator.Alloc<WaterOcclusionVertsType>();
	WaterOcclusionVertsHiZ = g_tempAllocator.Alloc<WaterOcclusionVertsType>();


	const bool requireStaticAllocForDebugDraw= BANK_SWITCH( m_bDisplayOccluders || m_bWaterOnlyOccluders , false);

	g_tempAllocator.AllocScratchPadArray( g_waterOccluders, OccludeModel::MaxWaterOccluders, requireStaticAllocForDebugDraw);
	g_tempAllocator.AllocScratchPadArray( g_waterOccludersClipped, OccludeModel::MaxWaterOccluders, requireStaticAllocForDebugDraw);
	m_rasterizers[0]->SetWaterOccluderResources(   g_waterOccluders, g_waterOccludersClipped);


	g_tempAllocator.AllocScratchPad( isBoxOccluderClipped, requireStaticAllocForDebugDraw);
	g_tempAllocator.AllocScratchPad( rasterizedBoxOccluders, requireStaticAllocForDebugDraw);
	g_tempAllocator.AllocScratchPad( isOccludeModelClipped, requireStaticAllocForDebugDraw);
	g_tempAllocator.AllocScratchPad( rasterizedOccludeModels, requireStaticAllocForDebugDraw);

	g_tempAllocator.AllocScratchPadArray( m_tempHiZBuffer, RAST_HIZ_SIZE_VEC, BANK_SWITCH(m_rasterizers[0]->ShowTempHiZBuffer(), false));
	m_rasterizers[0]->SetTempHiZBuffer(m_tempHiZBuffer);

}
void COcclusion::FreePerFrameResources()
{
	Assert( WaterOcclusionVertsStencil != 0);
	Assert( WaterOcclusionVertsHiZ != 0);

	g_tempAllocator.End();
	for (int i = 0; i < SCAN_OCCLUSION_STORAGE_COUNT; ++i)
	{
		m_rasterizers[i]->SetTileList(NULL);
	}												   
	WaterOcclusionVertsStencil = 0;
	WaterOcclusionVertsHiZ = 0;
	g_tempAllocator.FreeScratchPad( isBoxOccluderClipped);
	g_tempAllocator.FreeScratchPad( rasterizedBoxOccluders);
	g_tempAllocator.FreeScratchPad( isOccludeModelClipped);
	g_tempAllocator.FreeScratchPad( rasterizedOccludeModels);

	g_tempAllocator.FreeScratchPadArray( g_waterOccluders);
	g_tempAllocator.FreeScratchPadArray( g_waterOccludersClipped);

	m_rasterizers[0]->SetWaterOccluderResources(  g_waterOccluders,g_waterOccludersClipped);

	g_tempAllocator.FreeScratchPadArray( m_tempHiZBuffer);
	m_rasterizers[0]->SetTempHiZBuffer(m_tempHiZBuffer);
	BANK_ONLY( g_showPerFrameAllocations = false);
}




/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RegisterWaterQuad
// PURPOSE :  The water code calls this function for each water quad being rendered.
//			  The idea is that the occlusion code uses this to work out which objects
//			  should be rendered for the water reflection.
/////////////////////////////////////////////////////////////////////////////////

void COcclusion::RegisterWaterPolyStencil(const Vec3V* points, int numPoints)
{
	if (!OcclusionEnabled() || g_render_lock)
		return;	

	if (!AssertVerify( WaterOcclusionVertsStencil != 0 ))
		return;

#if __BANK
	if ( fwScan::ms_forceDisableOcclusion ){
		return;
	}
#endif
	for (int i=0;i<numPoints; i++) 
	{
		Assert( IsTrueXYZ(IsNotNan(points[i])));
	}

	Assert(numPoints >= 3 && numPoints <= 5);
	RegisterWaterQuadStencil(points[0], points[1], points[2], points[Min<int>(numPoints, 4) - 1]);

	if (numPoints > 4)
	{
		RegisterWaterQuadStencil(points[0], points[3], points[4], points[4]);
	}

#if RASTER
	//CRasterInterface::RegisterStencilPoly(points, numPoints);
#endif // RASTER
}

void COcclusion::RegisterWaterQuadStencil(Vec3V_In p0, Vec3V_In p1, Vec3V_In p2, Vec3V_In p3)
{		   
	Assert( WaterOcclusionVertsStencil != 0 );

	if (AssertVerify(NumWaterOcclusionVertsStencil <= (MAX_WATER_STENCIL_VERTS-4)))
	{
		int vi = NumWaterOcclusionVertsStencil;

		(*WaterOcclusionVertsStencil)[vi+0] = p0;
		(*WaterOcclusionVertsStencil)[vi+1] = p1;
		(*WaterOcclusionVertsStencil)[vi+2] = p2;
		(*WaterOcclusionVertsStencil)[vi+3] = p3;

		Assert( IsTrueXYZ(IsNotNan((*WaterOcclusionVertsStencil)[vi+0])));
		Assert( IsTrueXYZ(IsNotNan((*WaterOcclusionVertsStencil)[vi+1])));
		Assert( IsTrueXYZ(IsNotNan((*WaterOcclusionVertsStencil)[vi+2])));
		Assert( IsTrueXYZ(IsNotNan((*WaterOcclusionVertsStencil)[vi+3])));

		NumWaterOcclusionVertsStencil+=4;
	}
}

void COcclusion::RegisterWaterQuadStencil(float minX, float minY, float maxX, float maxY, float z)
{
	Assert( WaterOcclusionVertsStencil != 0 );

	if (AssertVerify(NumWaterOcclusionVertsStencil <= (MAX_WATER_STENCIL_VERTS-4)))
	{
		int vi = NumWaterOcclusionVertsStencil;

		(*WaterOcclusionVertsStencil)[vi+0] = Vec3V(minX, minY, z);
		(*WaterOcclusionVertsStencil)[vi+1] = Vec3V(maxX, minY, z);
		(*WaterOcclusionVertsStencil)[vi+2] = Vec3V(maxX, maxY, z);
		(*WaterOcclusionVertsStencil)[vi+3] = Vec3V(minX, maxY, z);

		Assert( IsTrueXYZ(IsNotNan((*WaterOcclusionVertsStencil)[vi+0])));
		Assert( IsTrueXYZ(IsNotNan((*WaterOcclusionVertsStencil)[vi+1])));
		Assert( IsTrueXYZ(IsNotNan((*WaterOcclusionVertsStencil)[vi+2])));
		Assert( IsTrueXYZ(IsNotNan((*WaterOcclusionVertsStencil)[vi+3])));	

		NumWaterOcclusionVertsStencil+=4;
	}
}

void COcclusion::RegisterWaterPolyHiZ(const Vec3V* points, int numPoints)
{
	if (!OcclusionEnabled() || g_render_lock)
		return;	

	if (!AssertVerify( WaterOcclusionVertsHiZ != 0 ))
		return;


#if __BANK
	if ( fwScan::ms_forceDisableOcclusion ){
		return;
	}
#endif
	for (int i=0;i<numPoints; i++) 
	{
		Assert( IsTrueXYZ(IsNotNan(points[i])));
	}

	Assert(numPoints >= 3 && numPoints <= 5);
	RegisterWaterQuadHiZ(points[0], points[1], points[2], points[Min<int>(numPoints, 4) - 1]);

	if (numPoints > 4)
	{
		RegisterWaterQuadHiZ(points[0], points[3], points[4], points[4]);
	}
}

void COcclusion::RegisterWaterQuadHiZ(Vec3V_In p0, Vec3V_In p1, Vec3V_In p2, Vec3V_In p3)
{
	if (AssertVerify(NumWaterOcclusionVertsHiZ <= (MAX_WATER_STENCIL_VERTS-4)))
	{
		int vi = NumWaterOcclusionVertsHiZ;

		(*WaterOcclusionVertsHiZ)[vi+0] = p0;
		(*WaterOcclusionVertsHiZ)[vi+1] = p1;
		(*WaterOcclusionVertsHiZ)[vi+2] = p2;
		(*WaterOcclusionVertsHiZ)[vi+3] = p3;

		Assert( IsTrueXYZ(IsNotNan((*WaterOcclusionVertsHiZ)[vi+0])));
		Assert( IsTrueXYZ(IsNotNan((*WaterOcclusionVertsHiZ)[vi+1])));
		Assert( IsTrueXYZ(IsNotNan((*WaterOcclusionVertsHiZ)[vi+2])));
		Assert( IsTrueXYZ(IsNotNan((*WaterOcclusionVertsHiZ)[vi+3])));
	
		NumWaterOcclusionVertsHiZ+=4;
	}
}

void COcclusion::RegisterWaterQuadHiZ(float minX, float minY, float maxX, float maxY, float z)
{
	if (AssertVerify(NumWaterOcclusionVertsHiZ <= (MAX_WATER_STENCIL_VERTS-4)))
	{
		int vi = NumWaterOcclusionVertsHiZ;

		(*WaterOcclusionVertsHiZ)[vi+0] = Vec3V(minX, minY, z);
		(*WaterOcclusionVertsHiZ)[vi+1] = Vec3V(maxX, minY, z);
		(*WaterOcclusionVertsHiZ)[vi+2] = Vec3V(maxX, maxY, z);
		(*WaterOcclusionVertsHiZ)[vi+3] = Vec3V(minX, maxY, z);

		Assert( IsTrueXYZ(IsNotNan((*WaterOcclusionVertsHiZ)[vi+0])));
		Assert( IsTrueXYZ(IsNotNan((*WaterOcclusionVertsHiZ)[vi+1])));
		Assert( IsTrueXYZ(IsNotNan((*WaterOcclusionVertsHiZ)[vi+2])));
		Assert( IsTrueXYZ(IsNotNan((*WaterOcclusionVertsHiZ)[vi+3])));

		NumWaterOcclusionVertsHiZ+=4;
	}
}

void COcclusion::ResetWater()
{
	NumWaterOcclusionVertsStencil = 0;
	NumWaterOcclusionVertsHiZ = 0;
}

void COcclusion::RasterizeWater(const grcViewport &vp)
{
	ResetWater();

	if (!OcclusionEnabled())
		return;

	if (COcclusion::bWaterOcclusionDisabled)
		return;

	// could do a quick check to see if it is visible?
	m_rasterizers[ SCAN_OCCLUSION_STORAGE_WATER_REFLECTION ]->RasterizeStencilModel(
		vp,
		&(*WaterOcclusionVertsStencil)[0], 
		&NumWaterOcclusionVertsStencil,
		true,
		CRenderPhaseWaterReflectionInterface::IsOcclusionDisabledForOceanWaves()
#if STENCIL_WRITER_DEBUG
		, g_debugStencilEnableWaterStencil
		, g_debugStencilSimpleClipper
		, g_debugStencilBoundsExpandX0
		, g_debugStencilBoundsExpandY0
		, g_debugStencilBoundsExpandX1
		, g_debugStencilBoundsExpandY1
		, g_debugStencilPixelOffsetX
		, g_debugStencilPixelOffsetY
#endif // STENCIL_WRITER_DEBUG
		);

}
void COcclusion::WaterOcclusionDataIsReady()
{
	if (!OcclusionEnabled() || g_render_lock)
		return;	
#if __DEV
	if (!COcclusion::bWaterOcclusionDisabled)
#endif
	{
		m_rasterizers[ SCAN_OCCLUSION_STORAGE_WATER_REFLECTION ]->RemoveStencilDependancy();		
	}

	m_rasterizers[SCAN_OCCLUSION_STORAGE_PRIMARY]->RemoveWaterTriDependancy();
}

void COcclusion::AfterPreRender()
{}
bool g_HasSetupRasterizerForShadows=false;


#if SUPPORT_OCCLUDER_MESH
static void DeleteShadowModel( CheapShadowMesh* model )
{
	model->Release(&g_MeshAllocator);
	delete model;
}
#endif

void COcclusion::Update()
{
	// 7/6/13 - cthomas - Make sure we reset this to false every frame, 
	// regardless of whether occlusion is enabled or not, etc.
	g_HasSetupRasterizerForShadows=false;

#if __BANK
	DebugSelectedEntity();
#endif

	if ( g_render_lock )
		return;

#if SUPPORT_OCCLUDER_MESH
	// lets kill any shadow occlusion meshes if there are any
	FreedShadowModels.DestroyLessEqual( TIME.GetFrameCount()-2, DeleteShadowModel);
#endif

	// Check if occlusion should be enabled/disabled
	{
		g_scanMutableDebugFlags &= ~(SCAN_MUTABLE_DEBUG_ENABLE_OCCLUSION | SCAN_MUTABLE_DEBUG_ENABLE_SHADOW_OCCLUSION);

		if ( ( g_timeCycle.GetStaleEnableOcclusion() > 0 ) && !ms_bScriptDisabledOcclusionThisFrame )
			g_scanMutableDebugFlags |= SCAN_MUTABLE_DEBUG_ENABLE_OCCLUSION;

		if ( ( g_timeCycle.GetStaleEnableShadowOcclusion() > 0 ) && !ms_bScriptDisabledOcclusionThisFrame )
			g_scanMutableDebugFlags |= SCAN_MUTABLE_DEBUG_ENABLE_SHADOW_OCCLUSION;

		ms_bScriptDisabledOcclusionThisFrame = false;		// reset this - script have to call it each frame for safety

#if __BANK
		fwScan::UpdateForceEnableOcclusion();

		if ( Unlikely(!(g_scanMutableDebugFlags & SCAN_MUTABLE_DEBUG_ENABLE_OCCLUSION)) )
			grcDebugDraw::AddDebugOutput( "Occlusion is OFF" );

		if ( Unlikely(!(g_scanMutableDebugFlags & SCAN_MUTABLE_DEBUG_ENABLE_SHADOW_OCCLUSION)) )
			grcDebugDraw::AddDebugOutput( "Shadow occlusion is OFF" );
#endif
	}

	if ( !(g_scanDebugFlags & SCAN_DEBUG_EARLY_KICK_OCCLUSION_JOBS) ) 
	{
		// If shadow occlusion is disabled, or there is not directional light at all, we won't rasterize
		// the shadow occlusion buffer so don't set the dependency (otherwise we'll get the game hanging
		// in task code) - AP
		const bool	enableShadowOcclusion = 
			(CRenderPhaseCascadeShadowsInterface::CascadeShadowResX() > 1) &&
			(g_scanMutableDebugFlags & SCAN_MUTABLE_DEBUG_ENABLE_OCCLUSION) &&
			(g_scanMutableDebugFlags & SCAN_MUTABLE_DEBUG_ENABLE_SHADOW_OCCLUSION) &&
			Lights::GetUpdateDirectionalLight().GetIntensity() != 0.0f &&
			Lights::GetUpdateDirectionalLight().IsCastShadows();

		if ( Likely(enableShadowOcclusion) )
		{
			g_HasSetupRasterizerForShadows=true;
		}
		else 
		{
			g_HasSetupRasterizerForShadows=false;
		}
	}
}

void COcclusion::KickOcclusionJobs()
{
	if ( !OcclusionEnabled()  )
		return;
	if ( !(g_scanDebugFlags & SCAN_DEBUG_EARLY_KICK_OCCLUSION_JOBS ) )
		return;

	if ( Unlikely(!(g_scanMutableDebugFlags & SCAN_MUTABLE_DEBUG_ENABLE_OCCLUSION)) )
		return;

	ResetWater();

	if ( g_render_lock )
		return;

	AllocatePerFrameResources();
	
	const grcViewport* primaryVP = gVpMan.GetCurrentGameGrcViewport();

	SetOcclusionTransform( SCAN_OCCLUSION_TRANSFORM_PRIMARY, *primaryVP );

	RasterizeWater(*primaryVP );  // must be done before rasterize occluders so stencil task can be setup correctly

	RasterizeOccluders( SCAN_OCCLUSION_STORAGE_PRIMARY, *primaryVP );

#if RASTER
	//CRasterInterface::Clear(primaryVP);
	//CRasterInterface::RasteriseOccludeGeometry();
	//CRasterInterface::RasteriseStencilGeometry();
#endif // RASTER
}

void COcclusion::KickShadowOcclusionJobs()
{
	if ( !OcclusionEnabled()  )
		return;

	if ( !(g_scanDebugFlags & SCAN_DEBUG_EARLY_KICK_OCCLUSION_JOBS ) )
		return;

	if ( Unlikely(!(g_scanMutableDebugFlags & SCAN_MUTABLE_DEBUG_ENABLE_OCCLUSION)) )
		return;

	if ( g_render_lock )
		return;

	// If shadow occlusion is disabled, or there is not directional light at all, we won't rasterize
	// the shadow occlusion buffer so don't set the dependency (otherwise we'll get the game hanging
	// in task code) - AP
	const bool	enableShadowOcclusion =
		(CRenderPhaseCascadeShadowsInterface::CascadeShadowResX() > 1) &&
		(g_scanMutableDebugFlags & SCAN_MUTABLE_DEBUG_ENABLE_OCCLUSION) &&
		(g_scanMutableDebugFlags & SCAN_MUTABLE_DEBUG_ENABLE_SHADOW_OCCLUSION) &&
		Lights::GetUpdateDirectionalLight().GetIntensity() != 0.0f &&
		Lights::GetUpdateDirectionalLight().IsCastShadows();

	if ( Likely(enableShadowOcclusion) )
	{
		SetOcclusionTransform( SCAN_OCCLUSION_TRANSFORM_CASCADE_SHADOWS, CRenderPhaseCascadeShadowsInterface::GetViewport());
		RasterizeOccluders( SCAN_OCCLUSION_STORAGE_CASCADE_SHADOWS,      CRenderPhaseCascadeShadowsInterface::GetViewport());
		g_HasSetupRasterizerForShadows = true;
	}
}

void COcclusion::BeforeSetupRender(bool storeWaterOccluders, bool isGBuf, int shadowType, bool useOcclusion, const grcViewport &viewport)
{
	if ( !OcclusionEnabled() || !useOcclusion )
		return;

	if ( !(g_scanDebugFlags & SCAN_DEBUG_EARLY_KICK_OCCLUSION_JOBS) ) {

		Assertf(false, "We are always with the early Kick");
		PF_AUTO_PUSH_DETAIL("Setup Occlusion Before Render");

		if ( !OcclusionEnabled() || !useOcclusion )
			return;

		// The only warpshadow that uses occlusion is the one for the sun. It's a waste of space for the other lights as they only have a small range anyway.
		if (shadowType==SHADOWTYPE_PARABOLOID_SHADOWS)
		{
			return;
		}
		if (storeWaterOccluders)
			return;// don't even make any normal occluders for the water phase

		if (isGBuf)
		{
	#if __DEV
			if (COcclusion::bWaterOcclusionDisabled)
			{
				// do all water objects
				for (int i =0; i < RAST_STENCIL_SIZE_VEC; i++)
					m_rasterizers[ SCAN_OCCLUSION_STORAGE_WATER_REFLECTION ]->GetHiStencilBuffer()[i]=Vec4V(V_TRUE);
			}
			else
	#endif // __DEV
			{
				RasterizeWater(viewport);
			}

			RasterizeOccluders( SCAN_OCCLUSION_STORAGE_PRIMARY, viewport );
		}
		else if ( (shadowType == SHADOWTYPE_CASCADE_SHADOWS) && (g_scanMutableDebugFlags & SCAN_MUTABLE_DEBUG_ENABLE_SHADOW_OCCLUSION) )
		{
			for (int i =0; i < RAST_STENCIL_SIZE_VEC; i++)
				m_rasterizers[ SCAN_OCCLUSION_STORAGE_CASCADE_SHADOWS ]->GetHiStencilBuffer()[i] = Vec4V(V_TRUE);
			if ( g_HasSetupRasterizerForShadows)
				RasterizeOccluders( SCAN_OCCLUSION_STORAGE_CASCADE_SHADOWS, CRenderPhaseCascadeShadowsInterface::GetViewport());
			g_HasSetupRasterizerForShadows=false;
		}

	}
}



bool COcclusion::WantToPrimeZBufferWithOccluders(){
	return m_primeZBuffer;
}
 void	COcclusion::PrimeZBuffer()
{	

#if SUPPORT_OCCLUDER_MESH
	grcEffect& defaultEffect = GRCDEVICE.GetDefaultEffect();
	grcStateBlock::SetStates( grcStateBlock::RS_Default, CShaderLib::DSS_Default_Invert, m_BS_WriteDepthOnly);

	grcWorldIdentity();

	if ( defaultEffect.BeginDraw(m_primeDepthTechnique,true) ){
		defaultEffect.BeginPass(0);

		Assert(grcEffect::IsInDraw());

		grmModel::ModelCullbackType cullback = grmModel::GetModelCullback();
		Assertf( cullback ,"Please Setup a cullback for performance");
					  
		int amt = ShadowModels.GetCount();
		for ( int i=0;i<amt;i++ )
		{
			if (  cullback( ShadowModels[i]->m_bound) != 0)
				ShadowModels[i]->Render();
		}
		defaultEffect.EndPass();
		defaultEffect.EndDraw();
	}
#endif
}


#if __BANK
void COcclusion::DebugSelectedEntity()
{
	g_DebugSelectedEntityMinZValue = 0.0f;
	g_DebugSelectedEntityShadowMinZValue = 0.0f;
	if(m_rasterizers[SCAN_OCCLUSION_STORAGE_PRIMARY])
	{
		g_DebugMinZBufferValue = GetMinZForBuffer(SCAN_OCCLUSION_STORAGE_PRIMARY);
	}

	if(g_DebugTestSelectedEntityMinZ)
	{
		const CEntity* selectedEntity   = (CEntity*)g_PickerManager.GetSelectedEntity();
		if(selectedEntity)
		{
			spdAABB aabb;
			selectedEntity->GetAABB(aabb);
			if(g_DebugRenderSelectedEntityAABB)
			{
				grcDebugDraw::BoxAxisAligned(aabb.GetMin(), aabb.GetMax(), Color_blue, false);
			}

			const grcViewport* vp = gVpMan.GetCurrentGameGrcViewport();
			if(vp)
			{
				rstTransformToScreenSpace trans( vp, true);

				const int NUM_AABB_VERTS = 8;
				Vec3V aabbVerts[NUM_AABB_VERTS];
				ScalarV Eplision = ScalarVConstant<FLOAT_TO_INT(0.001f)>();//0.001f or  1 millimetre
				Vec3V bmin = aabb.GetMin();
				Vec3V bmax = Max(aabb.GetMax(), aabb.GetMin()+Vec3V(Eplision));  // get rid of degenerate edges
				
				u8 *hullIndices = NULL;
				u8 hullVertCount = (u8)rstGetBoxHull( vp->GetCameraPosition(), bmin, bmax, hullIndices,aabbVerts);
				Vec3V ssHullVerts[32];
				int ssHullVertsCount = 0;
				Vec3V scMinV, scMaxV;
				ScalarV minZ;
				rstTransformHullAndGetMinZ(vp->GetFullCompositeMtx(), Vec2V((float)vp->GetWidth(), (float)vp->GetHeight()), aabbVerts, hullIndices, hullVertCount, trans.GetRescaleDepth(), scMinV, scMaxV, minZ, ssHullVerts, ssHullVertsCount);
				g_DebugSelectedEntityMinZValue = minZ.Getf();
				Vec3V scVertsBBX[8];
				float minZFromOcclusionBuffer = GetMinZForBuffer(SCAN_OCCLUSION_TRANSFORM_PRIMARY);
				for (int i =0; i < 8; i++)
				{		
					Vec4V vout = rstFullTransformHomo( vp->GetFullCompositeMtx(), aabbVerts[i]);

					ScalarV recip = Invert(Abs(vout.GetW()));
					// extra newton raphson iteration
					recip = AddScaled(recip, recip, SubtractScaled(ScalarV(V_ONE), vout.GetW(), recip));

					scVertsBBX[i] = vout.GetXYZ() * recip;
					scVertsBBX[i].SetZ( vout.GetZ()*trans.GetRescaleDepth());	

					char buffer[255];
					if(g_DebugTestSelectedEntityDisplayInfo && g_DebugRenderSelectedEntityAABB)
					{
						sprintf(
							buffer, 
							"Index: %d\nP:<%.2f %.2f %.2f>\nSP:<%.10f %.10f %.10f>",
							i, aabbVerts[i].GetXf(), aabbVerts[i].GetYf(), aabbVerts[i].GetZf(),
							scVertsBBX[i].GetXf(), scVertsBBX[i].GetYf(), scVertsBBX[i].GetZf());
						grcDebugDraw::Text(aabbVerts[i], Color32(255, 255, 255, 255), 0, 0, buffer, false);

					}
				}

				if(g_DebugRenderSelectedEntityHull)
				{
					static const Vec2V ooScreenSize = Invert(Vec2V((float)vp->GetWidth(), (float)vp->GetHeight()));
					for(int i=0; i<ssHullVertsCount; i++)
					{
						int nextI = (i < (ssHullVertsCount-1)? i+1:0);
						grcDebugDraw::Line(ssHullVerts[i].GetXY() * ooScreenSize, ssHullVerts[nextI].GetXY() * ooScreenSize, ((g_DebugSelectedEntityMinZValue<minZFromOcclusionBuffer)? Color_green:Color_magenta));
					}

				}


				if(g_DebugRenderSelectedEntityShadowOOBB || g_DebugRenderSelectedEntityShadowHullOOBB)
				{
					Vec3V shadowOBB[8];
					Vec3V shadowTransformedOBB[8];
					u8* hullIndices=0;

					const Mat33V shadowToWorldMtx = CRenderPhaseCascadeShadowsInterface::GetScanCascadeShadowInfo().GetShadowToWorld33();
					const ScalarV groundLevel(CRenderPhaseCascadeShadowsInterface::GetMinimumRecieverShadowHeight());
					Vec3V obbMin, obbMax;
					Mat34V OBBToWorldMtx;
					Vec3V campos;
					
					fwScanEntities::CalculateProjectOcclusionOBB(
						CRenderPhaseCascadeShadowsInterface::GetScanCascadeShadowInfo(), 
						aabb, 
						shadowToWorldMtx, 
						groundLevel, 
						obbMin,
						obbMax,
						OBBToWorldMtx BANK_ONLY(, COcclusion::UseProjectedShadowOcclusionClipping()));

					Vec3V eyeOOBBSpace =UnTransformOrtho( OBBToWorldMtx, vp->GetCameraPosition()); // transform eye into OOBB space
					int numPoints= rstGetBoxHull(  eyeOOBBSpace, obbMin, obbMax, hullIndices,shadowOBB);
					for(int i=0;i<8;i++){
						shadowTransformedOBB[i]=Transform( OBBToWorldMtx, shadowOBB[i]);
					}
					//int numPoints = COcclusion::DrawProjectedOcclusion(	
						//gVpMan.GetUpdateGameGrcViewport()->GetCameraPosition(), aabbVerts, hullIndices, shadowOBB);

					Mat44V	OBBToWorldMtx44(Vec4V(OBBToWorldMtx.GetCol0(), ScalarV(V_ZERO)),
						Vec4V(OBBToWorldMtx.GetCol1(), ScalarV(V_ZERO)),
						Vec4V(OBBToWorldMtx.GetCol2(), ScalarV(V_ZERO)),
						Vec4V(OBBToWorldMtx.GetCol3(), ScalarV(V_ONE)));

					Mat44V OBBToScreenMtx;
					Multiply( OBBToScreenMtx, vp->GetFullCompositeMtx(), OBBToWorldMtx44);
					Vec3V ssShadowHullVerts[32];
					Vec3V scShadowBbox[8];
					int ssShadowHullVertsCount = 0;
					ScalarV minZ;
					rstTransformHullAndGetMinZ(OBBToScreenMtx, Vec2V((float)vp->GetWidth(), (float)vp->GetHeight()), shadowOBB, hullIndices, numPoints, trans.GetRescaleDepth(), scMinV, scMaxV, minZ, ssShadowHullVerts, ssShadowHullVertsCount);
					g_DebugSelectedEntityShadowMinZValue = minZ.Getf();
					for(int i=0;i<8;i++)
					{
						Vec4V vout = rstFullTransformHomo( vp->GetFullCompositeMtx(), shadowTransformedOBB[i]);

						ScalarV recip = Invert(Abs(vout.GetW()));
						// extra newton raphson iteration
						recip = AddScaled(recip, recip, SubtractScaled(ScalarV(V_ONE), vout.GetW(), recip));

						scShadowBbox[i]  = vout.GetXYZ() * recip;
						scShadowBbox[i].SetZ( vout.GetZ()*trans.GetRescaleDepth());	
					}

					static Vec3V cachedOOBBMin, cachedOOBBMax;
					static Mat34V cachedOBBToWorldMtx;
					static Vec3V cachedShadowTransformedOBB[8];
					static Vec3V cachedScShadowBbox[8];
					if(g_DebugFreezeProjectedShadowBbox)
					{
						cachedOOBBMin = obbMin;
						cachedOOBBMax = obbMax;
						cachedOBBToWorldMtx = OBBToWorldMtx;

						for(int i = 0; i<8; i++)
						{
							cachedShadowTransformedOBB[i] = shadowTransformedOBB[i];
							cachedScShadowBbox[i] = scShadowBbox[i];							
						}
					}

					if(g_DebugRenderSelectedEntityShadowOOBB)
					{
						if(g_DebugFreezeProjectedShadowBbox)
						{
							grcDebugDraw::BoxOriented( cachedOOBBMin, cachedOOBBMax, cachedOBBToWorldMtx, Color_blue, false);

						}
						else
						{
							grcDebugDraw::BoxOriented( obbMin, obbMax, OBBToWorldMtx, Color_blue, false);
						}
						char buffer[255];
						for(int i = 0; i<8; i++)
						{
							if(g_DebugTestSelectedEntityDisplayInfo)
							{
								if(g_DebugFreezeProjectedShadowBbox)
								{
									sprintf(
										buffer, 
										"Index: %d\nP:<%.2f %.2f %.2f>\nSP:<%.10f %.10f %.10f>",
										i, cachedShadowTransformedOBB[i].GetXf(), cachedShadowTransformedOBB[i].GetYf(), cachedShadowTransformedOBB[i].GetZf(),
										cachedScShadowBbox[i].GetXf(), cachedScShadowBbox[i].GetYf(), cachedScShadowBbox[i].GetZf());
									grcDebugDraw::Text(cachedShadowTransformedOBB[i], Color32(255, 255, 255, 255), 0, 0, buffer, false);

								}
								else
								{
									sprintf(
										buffer, 
										"Index: %d\nP:<%.2f %.2f %.2f>\nSP:<%.10f %.10f %.10f>",
										i, shadowTransformedOBB[i].GetXf(), shadowTransformedOBB[i].GetYf(), shadowTransformedOBB[i].GetZf(),
										scShadowBbox[i].GetXf(), scShadowBbox[i].GetYf(), scShadowBbox[i].GetZf());
									grcDebugDraw::Text(shadowTransformedOBB[i], Color32(255, 255, 255, 255), 0, 0, buffer, false);

								}

							}
						}
					}	

					if(g_DebugRenderSelectedEntityShadowHullOOBB)
					{
						for(int i=0;i<numPoints;i++)
						{
							grcDebugDraw::Line( shadowTransformedOBB[hullIndices[i]], shadowTransformedOBB[hullIndices[(i+1)%numPoints]], Color_white);					
						}
						static const Vec2V ooScreenSize = Invert(Vec2V((float)vp->GetWidth(), (float)vp->GetHeight()));
						for(int i=0; i<ssShadowHullVertsCount; i++)
						{
							int nextI = ((i+1) % ssShadowHullVertsCount);
							grcDebugDraw::Line(ssShadowHullVerts[i].GetXY() * ooScreenSize, ssShadowHullVerts[nextI].GetXY() * ooScreenSize, ((g_DebugSelectedEntityShadowMinZValue<minZFromOcclusionBuffer)? Color_green:Color_magenta));
						}
					}
				}
			}
		}
	}

}
static int SortOccludeModelByDepth(int a, int b)
{
	Vec3V pos1 = ((*rasterizedOccludeModels)[SCAN_OCCLUSION_STORAGE_PRIMARY][a].GetMin() + (*rasterizedOccludeModels)[SCAN_OCCLUSION_STORAGE_PRIMARY][a].GetMax()) * ScalarV(V_HALF);
	Vec3V pos2 = ((*rasterizedOccludeModels)[SCAN_OCCLUSION_STORAGE_PRIMARY][b].GetMin() + (*rasterizedOccludeModels)[SCAN_OCCLUSION_STORAGE_PRIMARY][b].GetMax()) * ScalarV(V_HALF);

	Vec3V screenSpace1 = grcViewport::GetCurrent()->Transform(pos1);
	Vec3V screenSpace2 = grcViewport::GetCurrent()->Transform(pos2);

	return IsGreaterThanAll(screenSpace1.GetZ(), screenSpace2.GetZ());
}

void COcclusion::DumpWaterOccluderModels()
{
	if (m_bWaterOnlyOccluders)
		m_rasterizers[SCAN_OCCLUSION_STORAGE_PRIMARY]->DumpWaterOccluderMeshes();
}
//
// name:			DrawDebug
// description:		Draws some lines and shit to work out what's happening.
//
void COcclusion::DrawOccluders(bool depthOnly )
{
	static dev_bool		disableClipping = true;

	if (!isBoxOccluderClipped || !rasterizedBoxOccluders || !isOccludeModelClipped || !rasterizedOccludeModels)
		return;

	if (!grcViewport::GetCurrent())
		return;
	// Count the number of visible occluder meshes.
	int modelCount = 0;
	int boxCount = 0;
	int tilerCount = m_rasterizers[SCAN_OCCLUSION_STORAGE_PRIMARY]->NumActiveTilers();

	for (int x=0; x<tilerCount; x++)
	{
		modelCount += m_rasterizers[SCAN_OCCLUSION_STORAGE_PRIMARY]->GetTileModelCount(x);
		boxCount += m_rasterizers[SCAN_OCCLUSION_STORAGE_PRIMARY]->GetTileBoxCount(x);
	}

	Vec3V campos;
	campos=grcViewport::GetCurrent()->GetCameraMtx().d();
	
	if (!m_bWaterOnlyOccluders)
	{
        ScalarV drawRange(512.f);
        grcColor3f(1.f,0.f,1.f);
        for ( int i = 0 ; i < boxCount;i++ ){
            if ( disableClipping || !(*isBoxOccluderClipped)[SCAN_OCCLUSION_STORAGE_PRIMARY][i] )
            {
                const BoxOccluder& b = (*rasterizedBoxOccluders)[SCAN_OCCLUSION_STORAGE_PRIMARY][i];
                Vec3V center( b.GetCenterX(), b.GetCenterY(),b.GetCenterZ());			
                if ( IsTrue(IsLessThan( Dist(campos, center), drawRange)))
                    b.DebugDraw();
            }
        }
	}

	// Sort occluder meshes back-to-front so they're at least remotely
	// readable on the screen.
	int sortedOccluders[MAX_NUM_OCCLUDE_MODELS];
	Assert(modelCount <= MAX_NUM_OCCLUDE_MODELS);

	if ( !depthOnly ){
		for (int x=0; x<modelCount; x++)
		{
			sortedOccluders[x] = x;
		}
		std::sort(&sortedOccluders[0], &sortedOccluders[modelCount], SortOccludeModelByDepth);
	}

	for ( int i = 0 ; i < modelCount;i++ )
	{
		int occluderIndex = depthOnly ? i : sortedOccluders[i];

		if ( disableClipping || !(*isOccludeModelClipped)[SCAN_OCCLUSION_STORAGE_PRIMARY][occluderIndex] )
		{
			if (!m_bWaterOnlyOccluders || (*rasterizedOccludeModels)[SCAN_OCCLUSION_STORAGE_PRIMARY][occluderIndex].AreFlagsSet(OccludeModel::FLAG_WATER_ONLY))
                (*rasterizedOccludeModels)[SCAN_OCCLUSION_STORAGE_PRIMARY][occluderIndex].Draw( OCCLUDE_MODEL_DEFAULT_ERROR, false, !m_enableBackFaceCulling );
		}
	}

	if ( m_bWaterOnlyOccluders)
		m_rasterizers[SCAN_OCCLUSION_STORAGE_PRIMARY]->DrawWaterOccluderMeshes(depthOnly);
	

	// Draw occluder mapdata names
	{
		atMap< u32, s32 >	boxMapSlotIndices;
		atMap< u32, s32 >	modelMapSlotIndices;
		
		for (int i = 0; i < s_boxOccluderUsedBucketCount; ++i)
		{
			CBoxOccluderBucket& bucket = s_boxOccluderBuckets[i];
			for (int j = 0; j < bucket.count; j++)
			{
				BoxOccluder& occludeBox = bucket.pElements[j];
				const u32	hash = atDataHash( (const char*) &occludeBox, sizeof(BoxOccluder) );
				boxMapSlotIndices[ hash ] = s_boxOccluderBuckets[i].mapDataSlotIndex;
			}
		}

		for (int i = 0; i < s_occludeModelUsedBucketCount; ++i)
		{
			COccludeModelBucket& bucket = s_occludeModelBuckets[i];
			for (int j = 0; j < bucket.count; j++)
			{
				OccludeModel& occludeModel = bucket.pElements[j];
				const u32	hash = atDataHash( (const char*) &occludeModel, sizeof(OccludeModel) );
				modelMapSlotIndices[ hash ] = s_boxOccluderBuckets[i].mapDataSlotIndex;
			}
		}

		const grcViewport&	viewport = g_SceneToGBufferPhaseNew->GetViewport()->GetGrcViewport();
		const Mat44V		compMtx = viewport.GetCompositeMtx();

		if (!m_bWaterOnlyOccluders) 
		{
            for (int i = 0 ; i < boxCount; i++)
            {
                if ( disableClipping || !(*isBoxOccluderClipped)[SCAN_OCCLUSION_STORAGE_PRIMARY][i] )
                {
                    const BoxOccluder&	b = (*rasterizedBoxOccluders)[SCAN_OCCLUSION_STORAGE_PRIMARY][i];
                    const Vec3V			center( b.GetCenterX(), b.GetCenterY(), b.GetCenterZ() );
                    const ScalarV		drawRange(512.f);

                    if ( !viewport.IsSphereVisibleInline( Vec4V(center, ScalarV(V_ONE)) ) )
                        continue;

                    if ( IsFalse( IsLessThan( Dist(campos, center), drawRange) ) )
                        continue;

                    const u32				hash = atDataHash( (const char*) &b, sizeof(BoxOccluder) );
                    const strLocalIndex		mapSlotIndex = strLocalIndex(boxMapSlotIndices[ hash ]);
                    const fwMapDataDef*		mapData = fwMapDataStore::GetStore().GetSlot( mapSlotIndex );
                    const char*				mapDataName = mapData->m_name.GetCStr();
                    const Vec4V				homScreenCoords = Multiply( compMtx, Vec4V( center, ScalarV(V_ONE) ) );
                    const Vec4V				normScreenCoords = homScreenCoords * Invert( homScreenCoords ).GetW();
                    const Vec4V				convScreenCoords = ( normScreenCoords + Vec4V(V_ONE) ) * Vec4V(V_HALF);
                    const Vec2V				screenCoords( convScreenCoords.GetX(), ScalarV(V_ONE) - convScreenCoords.GetY() );
                    grcDebugDraw::Text( screenCoords, Color_violet, mapDataName, true );
                }
            }
		}

		for (int i = 0; i < modelCount; i++)
		{
			const int	occluderIndex = depthOnly ? i : sortedOccluders[i];
			if ( disableClipping || !(*isOccludeModelClipped)[SCAN_OCCLUSION_STORAGE_PRIMARY][occluderIndex] )
			{
				const OccludeModel&		m = (*rasterizedOccludeModels)[SCAN_OCCLUSION_STORAGE_PRIMARY][occluderIndex];

				if (m_bWaterOnlyOccluders && !m.AreFlagsSet(OccludeModel::FLAG_WATER_ONLY))
					continue;

				const Vec3V				center = ( m.GetMin() + m.GetMax() ) * Vec3V(V_HALF);

				if ( !viewport.IsSphereVisibleInline( Vec4V(center, ScalarV(V_ONE)) ) )
					continue;

				const u32				hash = atDataHash( (const char*) &m, sizeof(OccludeModel) );
				const u32				mapSlotIndex = modelMapSlotIndices[ hash ];
				const fwMapDataDef*		mapData = fwMapDataStore::GetStore().GetSlot( strLocalIndex(mapSlotIndex) );
				const char*				mapDataName = mapData->m_name.GetCStr();
				const Vec4V				homScreenCoords = Multiply( compMtx, Vec4V( center, ScalarV(V_ONE) ) );
				const Vec4V				normScreenCoords = homScreenCoords * Invert( homScreenCoords ).GetW();
				const Vec4V				convScreenCoords = ( normScreenCoords + Vec4V(V_ONE) ) * Vec4V(V_HALF);
				const Vec2V				screenCoords( convScreenCoords.GetX(), ScalarV(V_ONE) - convScreenCoords.GetY() );
				grcDebugDraw::Text( screenCoords, Color_red, mapDataName, true );
			}
		}
	}
}
#endif //__BANK...

//
//
//
//
void COcclusion::DrawDebug()
{
#if __BANK
	if ( !OcclusionEnabled() || !(m_bDisplayOccluders || m_bWaterOnlyOccluders || DEV_ONLY(g_debugBoundsActive ||  )m_displayWaterOccluder || m_rasterizers[ m_debugDrawRasterizer ]->IsDebugDrawEnabled()) )
		return;

	for (int i = 0; i < SCAN_OCCLUSION_STORAGE_COUNT; ++i)
		AssertVerify(sysIpcLockMutex( m_rasterizerMutexes[i] ));


	m_rasterizers[ m_debugDrawRasterizer ]->DebugDraw();
	{
		grcStateBlock::SetRasterizerState( m_enableBackFaceCulling ?  grcStateBlock::RS_Default : grcStateBlock::RS_NoBackfaceCull);

		grcStateBlock::SetDepthStencilState(  m_bEnableDepthTest ? CShaderLib::DSS_Default_Invert : grcStateBlock::DSS_IgnoreDepth); 

		if ( m_bDisplayOccluders || m_bWaterOnlyOccluders )
		{
			DrawOccluders(false);	
		}

		if ( m_displayWaterOccluder )
		{
			// TODO: need to not use indices and just render quads out
			grcColor3f(1.f,0.f,1.f);
			int numIndices = 0;
			int start =0;
			while( start < numIndices)
			{
				int end =  Min(start + grcBeginMax3, numIndices);
				grcBegin(drawTris,end-start);
				for (int i = start; i < end; i++)
				{		
					//grcVertex3f( VEC3V_TO_VECTOR3((*WaterOcclusionVertsStencil)[WaterOcclusionIndicesStencil[i]] ));	
				}
				start  = end;
				grcEnd();
			}
		}
	}

	if(g_RenderActiveFrustum)
	{
		m_rasterizers[m_debugDrawRasterizer]->DebugDrawActiveFrustum();
		grcWorldIdentity();
	}

#if __DEV
	if ( g_debugBoundsActive)
	{
		grcStateBlock::SetDepthStencilState(  m_bEnableDepthTest ? CShaderLib::DSS_Default_Invert : grcStateBlock::DSS_IgnoreDepth); 
		RenderDebugBounds();
	}
#endif //__DEV...

	for (int i = 0; i < SCAN_OCCLUSION_STORAGE_COUNT; ++i)
		AssertVerify(sysIpcUnlockMutex( m_rasterizerMutexes[i] ));
#endif //__BANK...
}

#if __BANK
static bool sShowGBUFOccluderOverlay = false;
void COcclusion::ToggleGBUFOccluderOverlay()
{
	if (sShowGBUFOccluderOverlay)
	{
		COcclusion::m_rasterizers[ SCAN_OCCLUSION_STORAGE_PRIMARY ]->EnableDebugDraw(true,true);
	}
	else
	{
		COcclusion::m_rasterizers[ SCAN_OCCLUSION_STORAGE_PRIMARY ]->EnableDebugDraw(false,false);
	}
}

static bool sShowGBUFStencilOverlay = false;
void COcclusion::ToggleGBUFStencilOverlay()
{
	// this function operates a bit differently from ToggleGBUFOccluderOverlay, in that it maintains its own state ..
	sShowGBUFStencilOverlay = !sShowGBUFStencilOverlay;

	if (sShowGBUFStencilOverlay)
	{
		COcclusion::m_rasterizers[ SCAN_OCCLUSION_STORAGE_WATER_REFLECTION ]->EnableDebugDraw(true,true,true);
	}
	else
	{
		COcclusion::m_rasterizers[ SCAN_OCCLUSION_STORAGE_WATER_REFLECTION ]->EnableDebugDraw(false,false,false);
	}
}

bool COcclusion::IsGBUFStencilOverlayEnabled()
{
	return sShowGBUFStencilOverlay;
}
#endif

float COcclusion::GetExactQueryThreshold()
{
	return g_SelectExactQueryThreshold;
}
#if __BANK
bool COcclusion::UseSimpleQueriesOnly()
{	
	return g_useSimpleQueries; 
}
bool COcclusion::UseNewOcclusionAABBExact()
{	
	return g_useNewOcclusionAABBExact; 
}

bool COcclusion::UseTrivialAcceptTest()
{
	return g_useTrivialAcceptTest;
}
bool COcclusion::UseTrivialAcceptVisiblePixelTest()
{
	return g_UseTrivialAcceptVisiblePixelTest;
}
bool COcclusion::UseMinZClipping()
{
	return g_useMinZClipping;
}

bool COcclusion::ForceExactOcclusionTest()
{
	return g_ForceExactOcclusionTest;
}

bool COcclusion::UseProjectedShadowOcclusionTest()
{
	return g_UseProjectedShadowOcclusionTest;
}

bool COcclusion::UseProjectedShadowOcclusionClipping()
{
	return g_UseProjectedShadowOcclusionClipping;
}

ScalarV_Out COcclusion::GetNewOcclusionAABBExactSimpleTestPixelsThreshold()
{
	return g_newOcclussionAABBExactPixelsThresholdSimple;
}

#endif
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : InitWidgets
// PURPOSE :  
/////////////////////////////////////////////////////////////////////////////////
#if __BANK
extern bool g_bAreaBasedTesting;

void COcclusion::InitWidgets()
{
	bkBank *pBank = BANKMGR.FindBank("Renderer");
	Assert(pBank);

	ASSERT_ONLY(bkGroup *pBankGroup = )pBank->PushGroup("Occlusion", false);
	Assert(pBankGroup);

	pBank->AddToggle("Overlay Occluders", &sShowGBUFOccluderOverlay, ToggleGBUFOccluderOverlay);
	pBank->AddToggle("Display Occluders", &m_bDisplayOccluders);
	pBank->AddToggle("Display water only occluders", &m_bWaterOnlyOccluders);
	pBank->AddToggle("Flip Result", &g_scanDebugFlags, SCAN_DEBUG_FLIP_OCCLUSION_RESULT );
	pBank->AddToggle("Enable depth test", &m_bEnableDepthTest);
	pBank->AddToggle("Enable backface culling",&m_enableBackFaceCulling);
	pBank->AddToggle("Use Area Based Testing", &g_bAreaBasedTesting);

	pBank->AddSlider("Selection Threshold Box (Gbuf)", &m_boxoccluderThreshold[SCAN_OCCLUSION_STORAGE_PRIMARY], 0.0f, 1000.f, 0.01f);
	pBank->AddSlider("Selection Threshold (Gbuf)", &m_occluderThreshold[SCAN_OCCLUSION_STORAGE_PRIMARY], 0.0f, 1000.f, 0.01f);
	pBank->AddSlider("Selection Threshold Box (Shad)", &m_boxoccluderThreshold[SCAN_OCCLUSION_STORAGE_CASCADE_SHADOWS], 0.0f, 10.f, 0.01f);
	pBank->AddSlider("Selection Threshold (Shad)", &m_occluderThreshold[SCAN_OCCLUSION_STORAGE_CASCADE_SHADOWS], 0.0f, 10.f, 0.01f);

	pBank->AddToggle("Disable Water Occlusion", &bWaterOcclusionDisabled);


#if __DEV

	pBank->AddText("Trivial Accept Frustum GBUF", (int*)&g_TrivialAcceptActiveFrustum[0]);
	pBank->AddText("Trivial Accept Min Z GBUF", (int*)&g_TrivialAcceptMinZ[0]);
	pBank->AddText("Trivial Accept Visible Pixel GBUF", (int*)&g_TrivialAcceptVisPixel[0]);

	pBank->AddText("Trivial Accept Frustum SHAD", (int*)&g_TrivialAcceptActiveFrustum[1]);
	pBank->AddText("Trivial Accept Min Z SHAD", (int*)&g_TrivialAcceptMinZ[1]);
	pBank->AddText("Trivial Accept Visible Pixel SHAD", (int*)&g_TrivialAcceptVisPixel[1]);


	pBank->AddText("Occluded Count GBUF", (int*)&g_Occluded[0]);
	pBank->AddText("Occlusion Tested GBUF", (int*)&g_OccludeTested[0]);

	pBank->AddText("Occluded Count SHAD", (int*)&g_Occluded[1]);
	pBank->AddText("Occlusion Tested SHAD", (int*)&g_OccludeTested[1]);

	pBank->AddText("Occluded Count LIGHTS", (int*)&g_Occluded[2]);
	pBank->AddText("Occlusion Tested LIGHTS", (int*)&g_OccludeTested[2]);
	
	pBank->AddToggle("Disable Occluder Meshes", &bOcclusionMeshesDisabled);
	pBank->AddToggle("Disable Occluder Boxes", &bOcclusionBoxesDisabled);
	pBank->AddToggle("Prime Z With Occluders", &m_primeZBuffer);
	// rasterizer debug

	pBank->AddToggle("Force Enable Occlusion", &fwScan::ms_forceEnableOcclusion);
	pBank->AddToggle("Force Disable Occlusion", &fwScan::ms_forceDisableOcclusion);
	pBank->AddToggle("Display Water Bounds", &m_displayWaterOccluder);

	pBank->AddToggle("Display Query AABBs", &g_debugBoundsActive);		

	pBank->AddText("Resident Occluder Models", &s_occludeModelUsedCount, true);

#if STENCIL_WRITER_DEBUG
	pBank->AddToggle("Stencil Enable Water Stencil (TEMP HACK)", &g_debugStencilEnableWaterStencil);
	pBank->AddToggle("Stencil Simple Clipper", &g_debugStencilSimpleClipper);
	pBank->AddSlider("Stencil Bounds Expand X0", &g_debugStencilBoundsExpandX0, -4, 4, 1);
	pBank->AddSlider("Stencil Bounds Expand Y0", &g_debugStencilBoundsExpandY0, -4, 4, 1);
	pBank->AddSlider("Stencil Bounds Expand X1", &g_debugStencilBoundsExpandX1, -4, 4, 1);
	pBank->AddSlider("Stencil Bounds Expand Y1", &g_debugStencilBoundsExpandY1, -4, 4, 1);
	pBank->AddSlider("Stencil Pixel Offset X", &g_debugStencilPixelOffsetX, -64.0f, 64.0f, 1.0f/32.0f);
	pBank->AddSlider("Stencil Pixel Offset Y", &g_debugStencilPixelOffsetY, -64.0f, 64.0f, 1.0f/32.0f);
#endif // STENCIL_WRITER_DEBUG
#else
	pBank->AddToggle("Force Enable Occlusion", &fwScan::ms_forceEnableOcclusion);
	pBank->AddToggle("Force Disable Occlusion", &fwScan::ms_forceDisableOcclusion);
	pBank->AddToggle("Prime Z With Occluders", &m_primeZBuffer);
#endif

	const char* s_rasterizerNames[ SCAN_OCCLUSION_STORAGE_COUNT ] = { "GBUF" , "Cascade shadows" };

	pBank->AddCombo("Rasterizer to debug draw", &m_debugDrawRasterizer, SCAN_OCCLUSION_STORAGE_COUNT, s_rasterizerNames );

	pBank->AddToggle("Use Projected Shadow Test", &g_UseProjectedShadowOcclusionTest);
	pBank->AddToggle("Use Projected Shadow Clipping", &g_UseProjectedShadowOcclusionClipping);
	pBank->AddToggle("Use Fast Queries", &g_useSimpleQueries);
	pBank->AddToggle("Use New AABB Exact Edge List Occlusion", &g_useNewOcclusionAABBExact);
	pBank->AddToggle("Use Trivial Accept Test", &g_useTrivialAcceptTest);
	pBank->AddToggle("Use Trivial Accept Visible Pixel Test", &g_UseTrivialAcceptVisiblePixelTest);
	pBank->AddToggle("Force Exact Occlusion Test", &g_ForceExactOcclusionTest);
	pBank->AddToggle("Use MinZ Clip", &g_useMinZClipping);

	pBank->PushGroup("Debug Selected Entity");
	pBank->AddText("Min Z From GBuf", &g_DebugMinZBufferValue);
	pBank->AddToggle("Debug Test Selected Entity Min Z", &g_DebugTestSelectedEntityMinZ);
	pBank->AddToggle("Render Selected Entity AABB", &g_DebugRenderSelectedEntityAABB);
	pBank->AddToggle("Render Hull", &g_DebugRenderSelectedEntityHull);
	pBank->AddText("Selected Entity Min Z", &g_DebugSelectedEntityMinZValue);
	pBank->AddToggle("Render Projected Shadow OBB", &g_DebugRenderSelectedEntityShadowOOBB);
	pBank->AddToggle("Freeze Projected Shadow OBB", &g_DebugFreezeProjectedShadowBbox);
	pBank->AddToggle("Render Projected Shadow Hull OBB", &g_DebugRenderSelectedEntityShadowHullOOBB);
	pBank->AddText("Selected Entity Projected Shadow Min Z", &g_DebugSelectedEntityShadowMinZValue);	
	pBank->AddToggle("Display Vert Info", &g_DebugTestSelectedEntityDisplayInfo);
	pBank->PopGroup();

	pBank->AddToggle("Render Active Frustum", &g_RenderActiveFrustum);
	pBank->AddSlider("Min X Active Frustum Bounds", &g_ActiveFrustumBoundsMinX, -10000000.0f, 1000000.0f, 0.0f);
	pBank->AddSlider("Min Y Active Frustum Bounds", &g_ActiveFrustumBoundsMinY, -10000000.0f, 1000000.0f, 0.0f);
	pBank->AddSlider("Max X Active Frustum Bounds", &g_ActiveFrustumBoundsMaxX, -10000000.0f, 1000000.0f, 0.0f);
	pBank->AddSlider("Max Y Active Frustum Bounds", &g_ActiveFrustumBoundsMaxY, -10000000.0f, 1000000.0f, 0.0f);

	pBank->AddSlider("Threshold Pixels for simple test", &g_newOcclussionAABBExactPixelsThresholdSimple, 0.0f, 20.0f, 0.001f);	
	pBank->AddSlider("Select Exact Query Threshold", &g_SelectExactQueryThreshold, 0.f, 2.f, 0.001f);
	pBank->AddSlider("GBuf Test Bias", &m_GBufTestBias,0.0f,.01f, 0.0000001f);
	pBank->AddSlider("Shadow Test Bias", &m_ShadowTestBias,0.0f,1.f, 0.000001f);

	pBank->AddToggle("Rasterize Water Quads in HiZ", &bRasterizeWaterQuadsInHiZ);
	pBank->AddSlider("Water Quad HiZ Bias", &m_waterQuadHiZBias, -5.0f, 0.0f, 0.1f);
	pBank->AddSlider("Water Quad Stencil Bias", &m_waterQuadStencilBias, -5.0f, 0.0f, 0.01f);
	pBank->AddButton("Dump Water Occluder Info", &DumpWaterOccluderModels);

	pBank->PushGroup( "GBUF rasterizer" );
	m_rasterizers[ SCAN_OCCLUSION_STORAGE_PRIMARY ]->AddWidgets(*pBank);
	pBank->PopGroup();

	pBank->PushGroup( "Cascade shadows rasterizer" );
	m_rasterizers[ SCAN_OCCLUSION_STORAGE_CASCADE_SHADOWS ]->AddWidgets(*pBank);
	pBank->PopGroup();

	pBank->AddToggle("Show Per Frame Allocations", &g_showPerFrameAllocations);
	pBank->PopGroup();
}
#endif // __BANK


void COcclusion::GetRasterizerTransform(const int occluderPhase, Mat44V_InOut transform, float& depthRescale)
{
	transform = s_HiZTransform[occluderPhase ].m_comMtx;
	depthRescale = s_HiZTransform[occluderPhase ].GetRescaleDepth().Getf();
}

#endif // !__SPU

#if __BANK

rage::Vec4V_Out COcclusion::GetScreenOcclusionExtents( const spdAABB& aabb )
{
	const Vec3V extents( (float)RAST_HIZ_WIDTH, (float)RAST_HIZ_HEIGHT, 0.f);

	ScalarV zero(V_ZERO);
	Vec3V	bmin = aabb.GetMin();
	Vec3V	bmax = aabb.GetMax();

	ScalarV rescaleDepth = ScalarVFromF32(m_rasDepth[ SCAN_OCCLUSION_TRANSFORM_PRIMARY ]);
	rstSplatMat44V splatMtx ;
	rstCreateSplatMat44V( m_transMtx[ SCAN_OCCLUSION_TRANSFORM_PRIMARY ], splatMtx );
	Vec3V scbmin;
	Vec3V scbmax;

	if (!rstCalculateScreenSpaceBoundIsVis(	splatMtx, rescaleDepth, bmin,
		bmax,scbmin, scbmax, extents))  // pipeline these two
	{
		return Vec4V(V_ZERO);
	}				
	// rescale from to 0. to 1.
	Vec3V invExtents( 1.f/extents.GetXf(), 1.f/extents.GetYf(), 0.f)	;
	scbmin *=invExtents;
	scbmax *=invExtents;
	return Vec4V( scbmin.GetXY(), scbmax.GetXY());
}
#endif

bool COcclusion::OcclusionEnabled()
{
	return ( g_scanMutableDebugFlags & SCAN_MUTABLE_DEBUG_ENABLE_OCCLUSION ) != 0 && !g_render_lock;
}

const Vec3V* COcclusion::GetIcosahedronPoints()
{
	return &IcosahedronPts[0];
}
const u8* COcclusion::GetIcosahedronIndices()
{
	return &IcosahedronIndices[0];	
}
u32 COcclusion::GetNumIcosahedronPoints()
{
	return NELEM(IcosahedronPts);
}
u32 COcclusion::GetNumIcosahedronIndices()
{
	return NELEM(IcosahedronIndices);
}
const Vec3V* COcclusion::GetConePoints()
{
	return &ConePts[0];
}
const u8* COcclusion::GetConeIndices()
{
	return &ConeIndices[0];
}
u32 COcclusion::GetNumConePoints()
{
	return NELEM(ConePts);
}
u32 COcclusion::GetNumConeIndices()
{
	return NELEM(ConeIndices);
}

