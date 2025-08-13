//
// PlantsGrassRenderer - contains lowlevel rendering classes for CPlantMgr: CGrassRenderer & CPPTriPlantBuffer;
//
//	30/06/2005	-	Andrzej:	- initial port from SA to Rage;
//	14/07/2005	-	Andrzej:	- port to Rage fully working;
//	02/02/2007	-	Andrzej:	- support for deferred mode added;
//	24/05/2007	-	Andrzej:	- support for SPU renderer added;
//	22/06/2007	-	Andrzej:	- SPU: v2.0 master job system; sphere test for individual plants added; 
//	22/11/2007	-	Andrzej:	- support for SPU_GCM_FIFO and CBs in VRAM added;
//	----------	-	--------	------------------------------------------------------------------------
//	08/07/2008	-	Andrzej:	- PlantsMgr 2.0: code cleanup, refactor, instancing used by default;
//	14/07/2008	-	Andrzej:	- custom instanced rendering added (DL_RENDERPHASE_GBUG faster by over 1ms on CPU);
//	16/07/2008	-	Andrzej:	- PlantsMgr 2.0: micro-movements support added (local micro-movements scales are encoded in vertex colors);
//	10/11/2008	-	Andrzej:	- PlantsMgr 2.0: OPTIMIZED_VS_GRASS_SETUP added (only really required matrix regs are set into CB);
//	26/11/2008	-	Andrzej:	- PlantsMgr 2.0: SpuRenderloop 2.0 and code cleanup (grassModelSPU4 removed, etc.);
//	12/12/2008	-	Andrzej:	- PlantsMgr 2.0: SpuRenderloop 2.0: big buffer size optimizations
//									(using CALLs to bind shaders, textures and geometries instead of copying whole buffers);
//	26/04/2011	-	Andrzej:	- PlantsMgr 2.0: spuTriPlantBlockTab and BigHeap dynamically allocated from streaming heap;
//
//
//
//
//
//
//

// Rage headers
#include "grmodel/shader.h"
#include "rmcore/drawable.h"
#include "grcore/im.h"
#include "grmodel/geometry.h"
#include "grcore/grcorespu.h"
#include "rmcore/Instance.h"
#include "system/task.h"
#include "grcore/indexbuffer.h"
#include "grcore/texturegcm.h"
#include "grcore/vertexbuffer.h"
#include "grcore/wrapper_gcm.h"
#include "physics/inst.h"
#include "phbound/boundgeom.h"
#include "profile/timebars.h"
#include "system/findsize.h"

// Framework headers
#include "fwmaths\random.h"
#include "fwmaths\vector.h"
#include "fwsys\timer.h"

// Game headers
#include "scene\world\gameWorld.h"
#include "debug\debug.h"
#include "system\UseTuner.h"

#include "renderer\Renderer.h"
#include "renderer\Lights\lights.h"

#include "Objects\ProceduralInfo.h"				// PROCPLANT_xxx
#include "Renderer\PlantsGrassRenderer.h"
#include "..\shader_source\Vegetation\Grass\grass_regs.h"

#if PLANTS_USE_LOD_SETTINGS
#include "renderer\PlantsGrassRenderer_parser.h"
#endif

#if RSG_ORBIS
#include "grcore/gfxcontext_gnm.h"
#endif

extern mthRandom g_PlantsRendRand[NUMBER_OF_RENDER_THREADS];
extern CPlantMgr gPlantMgr;

RENDER_OPTIMISATIONS();

//
//
// if 1, then commandbuffers are in VRAM:
//
#define GCMFIFO_IN_VRAM				(0)

//
// few helper allocators to access & map memory,
// depending where CBs are located:
//
#if GCMFIFO_IN_VRAM
	#define PLANTS_MALLOC(SIZE,ALIGN)		physical_new((SIZE),(ALIGN))
	#define PLANTS_FREE(P)					delete [] (P)
	#define PLANTS_GCM_OFFSET				gcm::LocalOffset
#else
	#define PLANTS_MALLOC(SIZE,ALIGN)		rage_aligned_new((ALIGN)) u8[(SIZE)]
	#define PLANTS_FREE(P)					delete [] ((u8*)(P))
	#define PLANTS_GCM_OFFSET				gcm::MainOffset
#endif

#define GtaMallocAlign(SIZE, ALIGN)				rage_aligned_new((ALIGN)) u8[(SIZE)]
#define GtaFreeAlign(P)							delete [] ((u8*)(P))
#define GtaMallocVRamAlign(SIZE,ALIGN)			physical_new((SIZE),(ALIGN))
#define GtaFreeVRamAlign(P)						delete [] (P)

#define ASSERT16(P)		Assert((u32(P)&0x0F)==0)
#define ASSERT128(P)	Assert((u32(P)&0x7F)==0)


#if PSN_PLANTSMGR_SPU_RENDER
	// game includes:
	#include "PlantsGrassRendererSPU.h"
	#include "grcore\wrapper_gcm.h"
	using namespace cell::Gcm;
	#include "grcore\indexbuffer.h"
	#include "grcore\vertexbuffer.h"
	#include "grmodel\modelfactory.h"

	// default alignment for command buffers in Main memory:
	#define PLANTSMGR_CMD_DEF_ALIGN			(16)	// 128 align is not required, as SPU doesn't touch this

	u32				CGrassRenderer::sm_nNumGrassJobsAdded		= 0;
	u32				CGrassRenderer::sm_nNumGrassJobsAddedTotal	= 0;
	sysTaskHandle	CGrassRenderer::sm_GrassTaskHandle			= NULL;

	u32				CGrassRenderer::sm_BigHeapID		= 0;
#if PLANTSMGR_BIG_HEAP_IN_BLOCKS
	grassHeapBlock	CGrassRenderer::sm_BigHeapBlockArray[2][PLANTSMGR_BIG_HEAP_BLK_ARRAY_SIZE] ={0};
	s16				CGrassRenderer::sm_BigHeapBlockArrayCount[2] = {0};
#else
	u32*			CGrassRenderer::sm_BigHeap[2]		= {NULL};
	u32				CGrassRenderer::sm_BigHeapOffset[2]	= {0};
	u32				CGrassRenderer::sm_BigHeapSize[2]	= {0};
#endif
	u32				CGrassRenderer::sm_BigHeapAllocatedSize[2]		= {0};
	u32				CGrassRenderer::sm_BigHeapConsumed[2]			= {0};
	u32				CGrassRenderer::sm_BigHeapBiggestConsumed[2]	= {0};
	u32				CGrassRenderer::sm_BigHeapOverfilled[2]			= {0};
#if SPU_GCM_FIFO
	#define STARTUP_HEAP_SIZE		(256)			// 256 bytes
	u32*			CGrassRenderer::sm_StartupHeapPtr[2]	= {NULL};
	u32				CGrassRenderer::sm_StartupHeapOffset[2]	= {0};
#endif

	u32*			CGrassRenderer::gpShaderLOD0Cmd0[2]={NULL};	// BindShaderLOD0
	u32*			CGrassRenderer::gpShaderLOD1Cmd0[2]={NULL};	// BindShaderLOD1
	u32*			CGrassRenderer::gpShaderLOD2Cmd0[2]={NULL};	// BindShaderLOD2
	u32				CGrassRenderer::gpShaderLOD0Offset[2]={0};
	u32				CGrassRenderer::gpShaderLOD1Offset[2]={0};
	u32				CGrassRenderer::gpShaderLOD2Offset[2]={0};
	u32*			CGrassRenderer::gpPlantLOD2GeometryCmd0=NULL;
	u32				CGrassRenderer::gpPlantLOD2GeometryOffset=0;

	u32				CGrassRenderer::rsxLabel5Index	= u32(-1);	// indices of RSX label1
	volatile u32*	CGrassRenderer::rsxLabel5Ptr	= NULL;		// RSX: lock/unlock() buf1
	u32				CGrassRenderer::rsxLabel5_CurrentTaskID = 0;// every next task waits for bigger value, so many SPU tasks can be synchronized during the same frame

	// private aligned PPTriPlant space for SPU job to dma from:
#if PLANTSMGR_TRIPLANTTAB_IN_BLOCKS
	// single buffer of PPTriPlant[size] must fit into 4KB block
	CompileTimeAssert(sizeof(PPTriPlant)*PPTRIPLANT_BUFFER_SIZE <= PLANTSMGR_TRIPLANT_BLOCK_SIZE);
	PPTriPlant*		CGrassRenderer::spuTriPlantBlockTab[PLANTSMGR_MAX_NUM_OF_RENDER_JOBS]={0};
	s32				CGrassRenderer::spuTriPlantBlockTabCount = 0;
#else
	PPTriPlant		CGrassRenderer::spuTriPlantTab[PLANTSMGR_MAX_NUM_OF_RENDER_JOBS][PPTRIPLANT_BUFFER_SIZE] ALIGNED(128);
#endif
	CompileTimeAssert(sizeof(PPTriPlant)==PSN_SIZEOF_PPTRIPLANT);
	CompileTimeAssert(sizeof(PPTriPlant)*PPTRIPLANT_BUFFER_SIZE <= 4*1024);	// must fit into 4KB
	//FindSize(PPTriPlant);

	spuGrassParamsStruct	CGrassRenderer::inSpuGrassStructTab[PLANTSMGR_MAX_NUM_OF_RENDER_JOBS] ALIGNED(128);
	CompileTimeAssert(sizeof(grassModel)			== PSN_SIZEOF_GRASSMODEL);
	//FindSize(grassModel);
	CompileTimeAssert(sizeof(spuGrassParamsStruct)	== PSN_SIZEOF_SPUGRASSPARAMSTRUCT);
	//FindSize(spuGrassParamsStruct);
#endif //PSN_PLANTSMGR_SPU_RENDER...

#if RSG_DURANGO && GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS
PARAM(grassInstanceCBSize, "Size of the instanced constant buffer for grass" );
#endif // RSG_DURANGO && GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS

// 360: Leaving it disabled for now, because this doesn't help much on Xenos for some reason
//      and total GPU execution time/CB size is longer/bigger when enabled (?!?);
//      probably D3D does similiar caching tricks under the hood when calling SetVertexShaderConstant();
//
#define OPTIMIZED_VS_GRASS_SETUP	(1 && __PPU)

//
//
//
//
#if 0
struct instGrassDataDrawStruct
{
	Matrix44				m_WorldMatrix;
#if GRASS_INSTANCING
	Color32					m_PlantColor32;
	Color32					m_GroundColorAmbient32;
#else
	Vector4					m_PlantColor;
	Vector4					m_GroundColor;
#endif

	///////////////////////////////////////////////////////
#if OPTIMIZED_VS_GRASS_SETUP
	Matrix34				m_prevWorldMatrix;
#endif

public:
	inline void Reset()		{
#if OPTIMIZED_VS_GRASS_SETUP
								m_prevWorldMatrix.a =
								m_prevWorldMatrix.b =
								m_prevWorldMatrix.c =
								m_prevWorldMatrix.d = Vector3(-1,-1,-1);
#endif //OPTIMIZED_VS_GRASS_SETUP...
	}

#if OPTIMIZED_VS_GRASS_SETUP
	inline bool UseOptimizedVsSetup(const Matrix34& m34)
	{
		const bool bDifferent =	m_prevWorldMatrix.a.IsNotEqual(m34.a) ||
								m_prevWorldMatrix.b.IsNotEqual(m34.b) ||
								m_prevWorldMatrix.c.IsNotEqual(m34.c);
		if(bDifferent)
		{
			m_prevWorldMatrix.a = m34.a;
			m_prevWorldMatrix.b = m34.b;
			m_prevWorldMatrix.c = m34.c;
			return(FALSE);
		}
		return(TRUE);
	}
#else //OPTIMIZED_VS_GRASS_SETUP
	inline bool UseOptimizedVsSetup(const Matrix34& ) {return(FALSE);}
#endif //OPTIMIZED_VS_GRASS_SETUP

#if GRASS_INSTANCING
	// n/a
#else
	// full setup: 6 registers
	inline s32			GetRegisterCountFull() const	{ return( (sizeof(m_WorldMatrix)/16) + (sizeof(m_PlantColor)/16) + (sizeof(m_GroundColor)/16) );}
	inline const float*	GetRegisterPointerFull() const	{ return( (const float*)&m_WorldMatrix	);														}

	// optimized setup: 3 registers (only position from matrix is taken):
	inline s32			GetRegisterCountOpt() const		{ return( (sizeof(m_WorldMatrix.d)/16) + (sizeof(m_PlantColor)/16) + (sizeof(m_GroundColor)/16) );}
	inline const float*	GetRegisterPointerOpt() const	{ return( (const float*)&m_WorldMatrix.d	);														}
#endif
};
#endif


//
//
//
//
Vector3	CGrassRenderer::ms_vecCameraPos[2]		= {Vector3(0,0,0)};
Vector3	CGrassRenderer::ms_vecCameraFront[2]	= {Vector3(0,0,0)};
bool	CGrassRenderer::ms_cameraFppEnabled[2]	= {false, false};
bool	CGrassRenderer::ms_cameraUnderwater[2]	= {false, false};
bool	CGrassRenderer::ms_forceHdGrassGeom[2]	= {false, false};
Vector3	CGrassRenderer::ms_vecPlayerPos[2]		= {Vector3(0,0,0)};
float	CGrassRenderer::ms_fPlayerCollRSqr[2]	= {1.0f, 1.0f};
spdTransposedPlaneSet8	CGrassRenderer::ms_cullFrustum[2];
bool	CGrassRenderer::ms_interiorCullAll[2]	= {false};
Vector2	CGrassRenderer::ms_windBending[2]		= {Vector2(0.0f, 0.0f)};
Vector3 CGrassRenderer::ms_fakeGrassNormal[2]	= {Vector3(0,0,1)};
#if __BANK
bool	CGrassRenderer::ms_enableDebugDraw[2]	= {false};
#endif
#if FURGRASS_TEST_V4
Vector4	CGrassRenderer::ms_vecPlayerLFootPos[2]	= {Vector4(0,0,0,0)};
Vector4	CGrassRenderer::ms_vecPlayerRFootPos[2]	= {Vector4(0,0,0,0)};
#endif


bool	CGrassRenderer::ms_bVehCollisionEnabled[2][NUMBER_OF_RENDER_THREADS][NUM_COL_VEH]	= {FALSE};
Vector4	CGrassRenderer::ms_vecVehCollisionB[2][NUMBER_OF_RENDER_THREADS][NUM_COL_VEH]		= {Vector4(0,0,0,0)};
Vector4	CGrassRenderer::ms_vecVehCollisionM[2][NUMBER_OF_RENDER_THREADS][NUM_COL_VEH]		= {Vector4(0,0,0,0)};
Vector4	CGrassRenderer::ms_vecVehCollisionR[2][NUMBER_OF_RENDER_THREADS][NUM_COL_VEH]		= {Vector4(0,0,0,0)};

grcVertexBuffer*		CGrassRenderer::ms_plantLOD2VertexBuffer	= NULL;
grcVertexDeclaration*	CGrassRenderer::ms_plantLOD2VertexDecl		= NULL;

static
CPPTriPlantBuffer	PPTriPlantBuffer;


//
//
//
//
grmShaderGroup*		CGrassRenderer::ms_ShaderGroup				= NULL;
grmShader*			CGrassRenderer::ms_Shader					= NULL;
grcEffectVar		CGrassRenderer::ms_shdTextureID				= grcevNONE;
grcEffectVar		CGrassRenderer::ms_shdGBuf0TextureID		= grcevNONE;
#if PSN_PLANTSMGR_SPU_RENDER
	u32				CGrassRenderer::ms_shdTextureTexUnit		= (u32)-1;
#endif
grcEffectVar		CGrassRenderer::ms_grassRegTransform		= grcevNONE;
grcEffectVar		CGrassRenderer::ms_grassRegPlantCol			= grcevNONE;
grcEffectVar		CGrassRenderer::ms_grassRegGroundCol		= grcevNONE;
grcEffectVar		CGrassRenderer::ms_shdCameraPosID			= grcevNONE;
grcEffectVar		CGrassRenderer::ms_shdPlayerPosID			= grcevNONE;
grcEffectGlobalVar	CGrassRenderer::ms_shdVehCollisionEnabledID[NUM_COL_VEH]= {grcegvNONE};
grcEffectVar		CGrassRenderer::ms_shdVehCollisionBID[NUM_COL_VEH]		= {grcevNONE};
grcEffectVar		CGrassRenderer::ms_shdVehCollisionMID[NUM_COL_VEH]		= {grcevNONE};
grcEffectVar		CGrassRenderer::ms_shdVehCollisionRID[NUM_COL_VEH]		= {grcevNONE};
grcEffectVar		CGrassRenderer::ms_shdFadeAlphaDistUmTimerID= grcevNONE;
grcEffectVar		CGrassRenderer::ms_shdFadeAlphaLOD1DistID	= grcevNONE;
grcEffectVar		CGrassRenderer::ms_shdFadeAlphaLOD2DistID	= grcevNONE;
grcEffectVar		CGrassRenderer::ms_shdFadeAlphaLOD2DistFarID= grcevNONE;

#if !PSN_PLANTSMGR_SPU_RENDER
	grcEffectVar	CGrassRenderer::ms_shdPlantColorID			= grcevNONE;
	grcEffectVar	CGrassRenderer::ms_shdUMovementParamsID		= grcevNONE;
	grcEffectVar	CGrassRenderer::ms_shdDimensionLOD2ID		= grcevNONE;
	grcEffectVar	CGrassRenderer::ms_shdCollParamsID			= grcevNONE;
#endif
grcEffectVar		CGrassRenderer::ms_shdFakedGrassNormal		= grcevNONE;
#if CPLANT_WRITE_GRASS_NORMAL
	grcEffectVar	CGrassRenderer::ms_shdTerrainNormal			= grcevNONE;
#endif
#if DEVICE_MSAA
	grcEffectVar		CGrassRenderer::ms_shdAlphaToCoverageScale	= grcevNONE;
#endif
grcEffectTechnique	CGrassRenderer::ms_shdDeferredLOD0TechniqueID= grcetNONE;
grcEffectTechnique	CGrassRenderer::ms_shdDeferredLOD1TechniqueID= grcetNONE;
grcEffectTechnique	CGrassRenderer::ms_shdDeferredLOD2TechniqueID= grcetNONE;
#if __BANK
grcEffectTechnique	CGrassRenderer::ms_shdDeferredLOD0DbgTechniqueID= grcetNONE;
grcEffectTechnique	CGrassRenderer::ms_shdDeferredLOD1DbgTechniqueID= grcetNONE;
grcEffectTechnique	CGrassRenderer::ms_shdDeferredLOD2DbgTechniqueID= grcetNONE;
#endif
grcEffectTechnique	CGrassRenderer::ms_shdBlitFakedGBufID		= grcetNONE;

#if !PSN_PLANTSMGR_SPU_RENDER
DECLARE_MTR_THREAD grcEffectTechnique	CGrassRenderer::ms_shdDeferredLOD0TechniqueID_ToUse= grcetNONE;
DECLARE_MTR_THREAD grcEffectTechnique	CGrassRenderer::ms_shdDeferredLOD1TechniqueID_ToUse= grcetNONE;
DECLARE_MTR_THREAD grcEffectTechnique	CGrassRenderer::ms_shdDeferredLOD2TechniqueID_ToUse= grcetNONE;
spdTransposedPlaneSet8 CGrassRenderer::ms_cullingFrustum[NUMBER_OF_RENDER_THREADS];
#endif

#if GRASS_INSTANCING
CGrassRenderer::InstanceBucket		CGrassRenderer::ms_instanceBucketsLOD0[CGRASS_RENDERER_LOD_BUCKET_SIZE];
CGrassRenderer::InstanceBucket		CGrassRenderer::ms_instanceBucketsLOD1[CGRASS_RENDERER_LOD_BUCKET_SIZE];
CGrassRenderer::InstanceBucket_LOD2 CGrassRenderer::ms_instanceBucketsLOD2[CGRASS_RENDERER_LOD_BUCKET_SIZE];

CGrassRenderer::LODBucket < CGRASS_RENDERER_LOD_BUCKET_SIZE > CGrassRenderer::ms_LOD0Bucket;
CGrassRenderer::LODBucket < CGRASS_RENDERER_LOD_BUCKET_SIZE > CGrassRenderer::ms_LOD1Bucket;
CGrassRenderer::LODBucket < CGRASS_RENDERER_LOD_BUCKET_SIZE > CGrassRenderer::ms_LOD2Bucket;
#endif // GRASS_INSTANCING

#if PLANTS_CAST_SHADOWS
#if __BANK
bool CGrassRenderer::ms_drawShadows = true;
#endif

grcEffectGlobalVar	CGrassRenderer::ms_depthValueBias = grcegvNONE;
grcEffectTechnique	CGrassRenderer::ms_shdDeferredLOD0ShadowTechniqueID = grcetNONE;
grcEffectTechnique	CGrassRenderer::ms_shdDeferredLOD1ShadowTechniqueID = grcetNONE;
grcEffectTechnique	CGrassRenderer::ms_shdDeferredLOD2ShadowTechniqueID = grcetNONE;

CGrassShadowParams CGrassRenderer::ms_currentShadowEnvelope;
#if PLANTS_USE_LOD_SETTINGS
CGrassShadowParams_AllLODs CGrassRenderer::ms_shadowEnvelope_AllLods;
#endif
#endif //PLANTS_CAST_SHADOWS


#if PLANTS_USE_LOD_SETTINGS
CGrassScalingParams CGrassRenderer::ms_currentScalingParams;
CGrassScalingParams_AllLODs CGrassRenderer::ms_scalingParams_AllLods;
#endif //PLANTS_USE_LOD_SETTINGS

//
//
//
//
CGrassRenderer::CGrassRenderer()
{
	// do nothing
}


//
//
//
//
CGrassRenderer::~CGrassRenderer()
{
	// do nothing
}



//
//
//
//
bool CGrassRenderer::Initialise()
{
	FastAssert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	sysMemUseMemoryBucket b(MEMBUCKET_RENDER);

	//
	// shader stuff:
	//
	ms_Shader      = grmShaderFactory::GetInstance().Create();
	Assert(ms_Shader);
	ms_ShaderGroup = grmShaderFactory::GetInstance().CreateGroup();
	Assert(ms_ShaderGroup);

	if(!ms_Shader->Load("grass"))
	{
		Assertf(FALSE, "Error loading 'grass' shader!");
	}
	ms_ShaderGroup->InitCount(1);
	ms_ShaderGroup->Add(ms_Shader);


	ms_shdDeferredLOD0TechniqueID = ms_Shader->LookupTechnique("deferredLOD0_draw", TRUE);
	Assert(ms_shdDeferredLOD0TechniqueID);

	ms_shdDeferredLOD1TechniqueID = ms_Shader->LookupTechnique("deferredLOD1_draw", TRUE);
	Assert(ms_shdDeferredLOD1TechniqueID);

	ms_shdDeferredLOD2TechniqueID = ms_Shader->LookupTechnique("deferredLOD2_draw", TRUE);
	Assert(ms_shdDeferredLOD2TechniqueID);

#if __BANK
	ms_shdDeferredLOD0DbgTechniqueID = ms_Shader->LookupTechnique("deferredLOD0_dbgdraw", TRUE);
	Assert(ms_shdDeferredLOD0DbgTechniqueID);

	ms_shdDeferredLOD1DbgTechniqueID = ms_Shader->LookupTechnique("deferredLOD1_dbgdraw", TRUE);
	Assert(ms_shdDeferredLOD1DbgTechniqueID);

	ms_shdDeferredLOD2DbgTechniqueID = ms_Shader->LookupTechnique("deferredLOD2_dbgdraw", TRUE);
	Assert(ms_shdDeferredLOD2DbgTechniqueID);
#endif

	ms_shdBlitFakedGBufID = ms_Shader->LookupTechnique("blitFakedGBuf", TRUE);
	Assert(ms_shdBlitFakedGBufID);

	ms_shdTextureID = ms_Shader->LookupVar("grassTexture0", TRUE);
	Assert(ms_shdTextureID);

#if !PSN_PLANTSMGR_SPU_RENDER
	ms_shdDeferredLOD0TechniqueID_ToUse =  ms_shdDeferredLOD0TechniqueID;
	ms_shdDeferredLOD1TechniqueID_ToUse =  ms_shdDeferredLOD1TechniqueID;
	ms_shdDeferredLOD2TechniqueID_ToUse =  ms_shdDeferredLOD2TechniqueID;
#endif

#if CPLANT_STORE_LOCTRI_NORMAL
	ms_shdGBuf0TextureID = ms_Shader->LookupVar("gbufferTexture0", TRUE);
	Assert(ms_shdGBuf0TextureID);
#endif

#if PSN_PLANTSMGR_SPU_RENDER
	// grab texUnit from shader's instanceData:
	const grcInstanceData& instanceData = ms_Shader->GetInstanceData();
	const int index = (int)ms_shdTextureID - 1;
	grcInstanceData::Entry *pEntry = &instanceData.Data()[index];
	//plantsDebugf1("\n CGrassRenderer::Initialise: grassTexture0: count=0x%02X, reg=0x%02X, handle=0x%02X, tex=0x%X", pEntry->Count, pEntry->Register, pEntry->Handle, (u32)pEntry->Texture);
	ms_shdTextureTexUnit = pEntry->Register;
#endif //PSN_PLANTSMGR_SPU_RENDER...

	ms_grassRegTransform = ms_Shader->LookupVar("matGrassTransform", TRUE);
	Assert(ms_grassRegTransform );

	ms_grassRegPlantCol = ms_Shader->LookupVar("plantColor", TRUE);
	Assert(ms_grassRegPlantCol);

	ms_grassRegGroundCol = ms_Shader->LookupVar("groundColor", TRUE);
	Assert(ms_grassRegGroundCol);

	ms_shdCameraPosID = ms_Shader->LookupVar("vecCameraPos0", TRUE);
	Assert(ms_shdCameraPosID);

	ms_shdPlayerPosID = ms_Shader->LookupVar("vecPlayerPos0", TRUE);
	Assert(ms_shdPlayerPosID);

	for(u32 i=0; i<NUM_COL_VEH; i++)
	{
		char var_name[128];
		sprintf(var_name, "bVehColl%dEnabled0", i);	// "bVehColl0Enabled0"
		ms_shdVehCollisionEnabledID[i] = grcEffect::LookupGlobalVar(var_name, TRUE);
		Assert(ms_shdVehCollisionEnabledID[i]);

		sprintf(var_name, "vecVehColl%dB0", i);		// "vecVehColl0B0"
		ms_shdVehCollisionBID[i] = ms_Shader->LookupVar(var_name, TRUE);
		Assert(ms_shdVehCollisionBID[i]);

		sprintf(var_name, "vecVehColl%dM0", i);		// "vecVehColl0M0"
		ms_shdVehCollisionMID[i] = ms_Shader->LookupVar(var_name, TRUE);
		Assert(ms_shdVehCollisionMID[i]);

		sprintf(var_name, "vecVehColl%dR0", i);		// "vecVehColl0R0"
		ms_shdVehCollisionRID[i] = ms_Shader->LookupVar(var_name, TRUE);
		Assert(ms_shdVehCollisionRID[i]);
	}

	ms_shdFadeAlphaDistUmTimerID = ms_Shader->LookupVar("fadeAlphaDistUmTimer0", TRUE);
	Assert(ms_shdFadeAlphaDistUmTimerID);

	ms_shdFadeAlphaLOD1DistID = ms_Shader->LookupVar("fadeAlphaLOD1Dist0", TRUE);
	Assert(ms_shdFadeAlphaLOD1DistID);

	ms_shdFadeAlphaLOD2DistID = ms_Shader->LookupVar("fadeAlphaLOD2Dist0", TRUE);
	Assert(ms_shdFadeAlphaLOD2DistID);

	ms_shdFadeAlphaLOD2DistFarID = ms_Shader->LookupVar("fadeAlphaLOD2DistFar0", TRUE);
	Assert(ms_shdFadeAlphaLOD2DistFarID);
#if !PSN_PLANTSMGR_SPU_RENDER
	ms_shdCollParamsID = ms_Shader->LookupVar("vecCollParams0", TRUE);
	Assert(ms_shdCollParamsID);

	ms_shdPlantColorID = ms_Shader->LookupVar("plantColor0");
	Assert(ms_shdPlantColorID);

	ms_shdUMovementParamsID = ms_Shader->LookupVar("uMovementParams0", TRUE);
	Assert(ms_shdUMovementParamsID);

	ms_shdDimensionLOD2ID = ms_Shader->LookupVar("dimensionLOD20", TRUE);
	Assert(ms_shdDimensionLOD2ID);
#endif

	ms_shdFakedGrassNormal = ms_Shader->LookupVar("fakedGrassNormal0", TRUE);
	Assert(ms_shdFakedGrassNormal);

#if CPLANT_WRITE_GRASS_NORMAL
	ms_shdTerrainNormal = ms_Shader->LookupVar("terrainNormal0", TRUE);
	Assert(ms_shdTerrainNormal);
#endif

#if DEVICE_MSAA
	ms_shdAlphaToCoverageScale = ms_Shader->LookupVar("gAlphaToCoverageScale", false);
#endif

#if PSN_PLANTSMGR_SPU_RENDER
	Assert(SPU_GCM_FIFO >= 1);			// always make sure we have SPU_GCM_FIFO

	PPTriPlantBuffer.AllocateTextureBuffers();
	PPTriPlantBuffer.AllocateModelsBuffers();

#if PLANTSMGR_TRIPLANTTAB_IN_BLOCKS
	s32 nTriPlantBytesToAlloc = PLANTSMGR_TRIPLANTTAB_SIZE0;
	while(nTriPlantBytesToAlloc > 0)
	{
		const u32 i = spuTriPlantBlockTabCount;
		spuTriPlantBlockTab[i] = (PPTriPlant*)PLANTS_MALLOC_STR(PLANTSMGR_TRIPLANT_BLOCK_SIZE, 128);
		Assert(spuTriPlantBlockTab[i]);
		ASSERT128(spuTriPlantBlockTab[i]);
	
		spuTriPlantBlockTabCount++;
		Assertf(spuTriPlantBlockTabCount <= PLANTSMGR_MAX_NUM_OF_RENDER_JOBS, "spuTriPlantBlockTabCount exceeded!");

		nTriPlantBytesToAlloc -= PLANTSMGR_TRIPLANT_BLOCK_SIZE;
	}
#endif

#if PLANTSMGR_BIG_HEAP_IN_BLOCKS
	// allocate 768KB in chunks of 8KB:
	s32 nHeapBytesToAlloc = PLANTSMGR_BIG_HEAP_SIZE0;
	while(nHeapBytesToAlloc > 0)
	{
		const u32 b0 = sm_BigHeapBlockArrayCount[0];
		const u32 b1 = sm_BigHeapBlockArrayCount[1];

		sm_BigHeapBlockArray[0][b0].ea		= (u32*)PLANTS_MALLOC_STR(PLANTSMGR_LOCAL_HEAP_SIZE, 128);
		sm_BigHeapBlockArray[0][b0].offset	= PLANTS_GCM_OFFSET(sm_BigHeapBlockArray[0][b0].ea);
		sm_BigHeapBlockArray[1][b1].ea		= (u32*)PLANTS_MALLOC_STR(PLANTSMGR_LOCAL_HEAP_SIZE, 128);
		sm_BigHeapBlockArray[1][b1].offset	= PLANTS_GCM_OFFSET(sm_BigHeapBlockArray[1][b1].ea);

		Assert(sm_BigHeapBlockArray[0][b0].ea);
		Assert(sm_BigHeapBlockArray[1][b1].ea);
		ASSERT128(sm_BigHeapBlockArray[0][b0].ea);
		ASSERT128(sm_BigHeapBlockArray[1][b1].ea);

		sm_BigHeapBlockArrayCount[0]++;
		sm_BigHeapBlockArrayCount[1]++;
		Assertf(sm_BigHeapBlockArrayCount[0] <= PLANTSMGR_BIG_HEAP_BLK_ARRAY_SIZE, "sm_BigHeapBlockArrayCount[0] exceeded!");
		Assertf(sm_BigHeapBlockArrayCount[1] <= PLANTSMGR_BIG_HEAP_BLK_ARRAY_SIZE, "sm_BigHeapBlockArrayCount[1] exceeded!");

		nHeapBytesToAlloc -= PLANTSMGR_LOCAL_HEAP_SIZE;
	}
#else
	sm_BigHeapSize[0]		=
	sm_BigHeapSize[1]		= PLANTSMGR_BIG_HEAP_SIZE0;
	sm_BigHeap[0]			= (u32*)PLANTS_MALLOC_STR(sm_BigHeapSize[0], 128);
	Assert(sm_BigHeap[0]);
	sm_BigHeap[1]			= (u32*)PLANTS_MALLOC_STR(sm_BigHeapSize[1], 128);
	Assert(sm_BigHeap[1]);
	sm_BigHeapOffset[0]		= PLANTS_GCM_OFFSET(sm_BigHeap[0]);
	sm_BigHeapOffset[1]		= PLANTS_GCM_OFFSET(sm_BigHeap[1]);
	Assert(sm_BigHeap[0]);
	Assert(sm_BigHeap[1]);
	ASSERT128(sm_BigHeap[0]);
	ASSERT128(sm_BigHeap[1]);
	plantsDebugf1("RenderSPU: BigHeap0=%X, BigHeapOffset0=%X", (u32)sm_BigHeap[0], (u32)sm_BigHeapOffset[0]);
	plantsDebugf1("RenderSPU: BigHeap1=%X, BigHeapOffset1=%X", (u32)sm_BigHeap[1], (u32)sm_BigHeapOffset[1]);
#endif

#if SPU_GCM_FIFO
	sm_StartupHeapPtr[0]	= (u32*)PLANTS_MALLOC(STARTUP_HEAP_SIZE, 128);
	sm_StartupHeapPtr[1]	= (u32*)PLANTS_MALLOC(STARTUP_HEAP_SIZE, 128);
	sm_StartupHeapOffset[0]	= PLANTS_GCM_OFFSET(sm_StartupHeapPtr[0]);
	sm_StartupHeapOffset[1]	= PLANTS_GCM_OFFSET(sm_StartupHeapPtr[1]);
	Assert(sm_StartupHeapPtr[0]);
	Assert(sm_StartupHeapPtr[1]);
	ASSERT128(sm_StartupHeapPtr[0]);
	ASSERT128(sm_StartupHeapPtr[1]);
#endif //SPU_GCM_FIFO...


	// Map the main memory into io offset space so it can be viewed by the gpu
//	if( cellGcmMapMainMemory(mHeaps[0], HEAP_SIZE, &mHeapOffsets[0]) != CELL_OK)
//		plantsErrorf("Failure in cellGcmMapMainMemory\n");
//	if( cellGcmMapMainMemory(mHeaps[1], HEAP_SIZE, &mHeapOffsets[1]) != CELL_OK)
//		plantsErrorf("Failure in cellGcmMapMainMemory\n");

	gpShaderLOD0Cmd0[0] = (u32*)PLANTS_MALLOC(PLANTSMGR_LOCAL_BINDSHADERLOD0_MAXCMDSIZE*sizeof(u32), PLANTSMGR_CMD_DEF_ALIGN);
	Assert(gpShaderLOD0Cmd0[0]);
	gpShaderLOD0Cmd0[1] = (u32*)PLANTS_MALLOC(PLANTSMGR_LOCAL_BINDSHADERLOD0_MAXCMDSIZE*sizeof(u32), PLANTSMGR_CMD_DEF_ALIGN);
	Assert(gpShaderLOD0Cmd0[1]);

	gpShaderLOD1Cmd0[0] = (u32*)PLANTS_MALLOC(PLANTSMGR_LOCAL_BINDSHADERLOD1_MAXCMDSIZE*sizeof(u32), PLANTSMGR_CMD_DEF_ALIGN);
	Assert(gpShaderLOD1Cmd0[0]);
	gpShaderLOD1Cmd0[1] = (u32*)PLANTS_MALLOC(PLANTSMGR_LOCAL_BINDSHADERLOD1_MAXCMDSIZE*sizeof(u32), PLANTSMGR_CMD_DEF_ALIGN);
	Assert(gpShaderLOD1Cmd0[1]);

	gpShaderLOD2Cmd0[0] = (u32*)PLANTS_MALLOC(PLANTSMGR_LOCAL_BINDSHADERLOD2_MAXCMDSIZE*sizeof(u32), PLANTSMGR_CMD_DEF_ALIGN);
	Assert(gpShaderLOD2Cmd0[0]);
	gpShaderLOD2Cmd0[1] = (u32*)PLANTS_MALLOC(PLANTSMGR_LOCAL_BINDSHADERLOD2_MAXCMDSIZE*sizeof(u32), PLANTSMGR_CMD_DEF_ALIGN);
	Assert(gpShaderLOD2Cmd0[1]);

	gpShaderLOD0Offset[0] =PLANTS_GCM_OFFSET(gpShaderLOD0Cmd0[0]);
	Assert(gpShaderLOD0Offset[0]);
	gpShaderLOD0Offset[1] =PLANTS_GCM_OFFSET(gpShaderLOD0Cmd0[1]);
	Assert(gpShaderLOD0Offset[1]);

	gpShaderLOD1Offset[0] =PLANTS_GCM_OFFSET(gpShaderLOD1Cmd0[0]);
	Assert(gpShaderLOD1Offset[0]);
	gpShaderLOD1Offset[1] =PLANTS_GCM_OFFSET(gpShaderLOD1Cmd0[1]);
	Assert(gpShaderLOD1Offset[1]);

	gpShaderLOD2Offset[0] =PLANTS_GCM_OFFSET(gpShaderLOD2Cmd0[0]);
	Assert(gpShaderLOD2Offset[0]);
	gpShaderLOD2Offset[1] =PLANTS_GCM_OFFSET(gpShaderLOD2Cmd0[1]);
	Assert(gpShaderLOD2Offset[1]);

	gpPlantLOD2GeometryCmd0 = (u32*)PLANTS_MALLOC(PLANTSMGR_LOCAL_GEOMETRY_MAXCMDSIZE*sizeof(u32), PLANTSMGR_CMD_DEF_ALIGN);
	Assert(gpPlantLOD2GeometryCmd0);
	gpPlantLOD2GeometryOffset = PLANTS_GCM_OFFSET(gpPlantLOD2GeometryCmd0);
	Assert(gpPlantLOD2GeometryOffset);

	// allocate RSX labels:
	rsxLabel5Index		= gcm::RsxSemaphoreRegistrar::Allocate();		// index of RSX label1
	rsxLabel5Ptr		= cellGcmGetLabelAddress(rsxLabel5Index);		// ptr of the label in RSX memory
	Assert(rsxLabel5Ptr);
	ASSERT16(rsxLabel5Ptr); // enough to be 16 byte aligned for cellDmaSmallGet/Put()
	plantsDebugf1("RenderSPU: Label5Index=%d (0x%X), ptr=0x%p", rsxLabel5Index, rsxLabel5Index, rsxLabel5Ptr);

	*(rsxLabel5Ptr) = 0xFF000000|(((sm_BigHeapID-1)&0x1)<<8)|((rsxLabel5_CurrentTaskID-1)&0xFF);	// prevTaskID1
#endif //PSN_PLANTSMGR_SPU_RENDER...


#if PLANTS_USE_LOD_SETTINGS
	// Load the data in.
	PARSER.LoadObject("common:/data/GrassLODSettings", "xml", CGrassRenderer::ms_scalingParams_AllLods);
#if PLANTS_CAST_SHADOWS
	PARSER.LoadObject("common:/data/GrassShadowLODSettings", "xml", CGrassRenderer::ms_shadowEnvelope_AllLods);
#endif
	// Say we`re initialized.	
	ms_isInitialized = true;
	// Set up initial plant LOD.
	SetPlantLODToUse((CGrassRenderer_PlantLod)ms_LODToInitialiseWith);
#if __BANK
	// Update the one being edited.
	ms_LODBeingEdited = ms_LODToInitialiseWith;
	ms_LODNextToEdit = ms_LODToInitialiseWith;
#endif
#endif //PLANTS_USE_LOD_SETTINGS


#if PLANTS_CAST_SHADOWS
	ms_shdDeferredLOD0ShadowTechniqueID = ms_Shader->LookupTechnique("deferredLOD0_drawshadow", TRUE);
	ms_shdDeferredLOD1ShadowTechniqueID = ms_Shader->LookupTechnique("deferredLOD1_drawshadow", TRUE);
	ms_shdDeferredLOD2ShadowTechniqueID = ms_Shader->LookupTechnique("deferredLOD2_drawshadow", TRUE);
	ms_depthValueBias = grcEffect::LookupGlobalVar("depthValueBias0", TRUE);
	SetDepthBiasOff();
#endif //PLANTS_CAST_SHADOWS

#if GRASS_INSTANCING
	u32 i;
	for(i=0; i<CGRASS_RENDERER_LOD_BUCKET_SIZE; i++)
	{
		ms_instanceBucketsLOD0[i].Init(ms_Shader, ms_shdTextureID);
		ms_instanceBucketsLOD1[i].Init(ms_Shader, ms_shdTextureID);
		ms_instanceBucketsLOD2[i].Init(ms_Shader, ms_shdTextureID);

		ms_LOD0Bucket.SetBucket(i, &ms_instanceBucketsLOD0[i]);
		ms_LOD1Bucket.SetBucket(i, &ms_instanceBucketsLOD1[i]);
		ms_LOD2Bucket.SetBucket(i, &ms_instanceBucketsLOD2[i]);
	}
#endif // GRASS_INSTANCING 

	return(TRUE);
} // end of Initialise()...



//
//
//
//
grmShaderGroup* CGrassRenderer::GetGrassShaderGroup()
{
	return(ms_ShaderGroup);
}

//
//
//
//
grmShader*		CGrassRenderer::GetGrassShader()
{
	grmShader *pShader = ms_Shader;
	Assert(pShader);
	return(pShader);
}

grcEffectTechnique	CGrassRenderer::GetFakedGBufTechniqueID()
{
	return ms_shdBlitFakedGBufID;
}

grcEffectVar		CGrassRenderer::GetGBuf0TextureID()
{
	return ms_shdGBuf0TextureID;
}


//
//
//
//
void CGrassRenderer::Shutdown()
{
	FastAssert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

#define DELETE_PTR(PTR)					{ if(PTR) { delete(PTR); PTR=NULL; } }

	ms_Shader->SetVar(ms_shdTextureID, (grcTexture*)NULL);
	DELETE_PTR(ms_ShaderGroup);

#undef DELETE_PTR


#if PSN_PLANTSMGR_SPU_RENDER
	PPTriPlantBuffer.DestroyTextureBuffers();
	PPTriPlantBuffer.DestroyModelsBuffers();

#if PLANTSMGR_TRIPLANTTAB_IN_BLOCKS
	while( spuTriPlantBlockTabCount-- )
	{
		const u32 i = spuTriPlantBlockTabCount;
		PLANTS_FREE_STR( spuTriPlantBlockTab[i] );
		spuTriPlantBlockTab[i] = NULL;
	}
#endif

#if PLANTSMGR_BIG_HEAP_IN_BLOCKS
	while( sm_BigHeapBlockArrayCount[0]-- )
	{
		const u32 i = sm_BigHeapBlockArrayCount[0];
		PLANTS_FREE_STR( sm_BigHeapBlockArray[0][i].ea );
		sm_BigHeapBlockArray[0][i].ea		= NULL;
		sm_BigHeapBlockArray[0][i].offset	= 0;
	}

	while( sm_BigHeapBlockArrayCount[1]-- )
	{
		const u32 i = sm_BigHeapBlockArrayCount[1];
		PLANTS_FREE_STR( sm_BigHeapBlockArray[1][i].ea );
		sm_BigHeapBlockArray[1][i].ea		= NULL;
		sm_BigHeapBlockArray[1][i].offset	= 0;
	}
#else
	PLANTS_FREE_STR(sm_BigHeap[0]);
	PLANTS_FREE_STR(sm_BigHeap[1]);
	sm_BigHeap[0] = NULL;
	sm_BigHeap[1] = NULL;
	sm_BigHeapOffset[0] = 0;
	sm_BigHeapOffset[1] = 0;
#endif

	#if SPU_GCM_FIFO
		PLANTS_FREE(sm_StartupHeapPtr[0]);
		PLANTS_FREE(sm_StartupHeapPtr[1]);
		sm_StartupHeapPtr[0] = NULL;
		sm_StartupHeapPtr[1] = NULL;
		sm_StartupHeapOffset[0] = 0;	
		sm_StartupHeapOffset[1] = 0;	
	#endif

	PLANTS_FREE(gpShaderLOD0Cmd0[0]);
	gpShaderLOD0Cmd0[0]		= NULL;
	gpShaderLOD0Offset[0]	= 0;
	PLANTS_FREE(gpShaderLOD0Cmd0[1]);
	gpShaderLOD0Cmd0[1]		= NULL;
	gpShaderLOD0Offset[1]	= 0;

	PLANTS_FREE(gpShaderLOD1Cmd0[0]);
	gpShaderLOD1Cmd0[0]		= NULL;
	gpShaderLOD1Offset[0]	= 0;
	PLANTS_FREE(gpShaderLOD1Cmd0[1]);
	gpShaderLOD1Cmd0[1]		= NULL;
	gpShaderLOD1Offset[1]	= 0;

	PLANTS_FREE(gpShaderLOD2Cmd0[0]);
	gpShaderLOD2Cmd0[0]		= NULL;
	gpShaderLOD2Offset[0]	= 0;
	PLANTS_FREE(gpShaderLOD2Cmd0[1]);
	gpShaderLOD2Cmd0[1]		= NULL;
	gpShaderLOD2Offset[1]	= 0;


	PLANTS_FREE(gpPlantLOD2GeometryCmd0);
	gpPlantLOD2GeometryCmd0		= NULL;
	gpPlantLOD2GeometryOffset	= 0;

#endif //PSN_PLANTSMGR_SPU_RENDER...

#if GRASS_INSTANCING
	u32 i;

	for(i=0; i<CGRASS_RENDERER_LOD_BUCKET_SIZE; i++)
	{
		ms_instanceBucketsLOD0[i].ShutDown();
		ms_instanceBucketsLOD1[i].ShutDown();
		ms_instanceBucketsLOD2[i].ShutDown();
	}
#endif // GRASS_INSTANCING

}// end of Shutdown()...


//
//
// Update buffers allocated from streaming heap:
//
bool CGrassRenderer::UpdateStr()
{
	FastAssert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

bool bDoneAlloc	= false;	// nothing done yet
bool bDoneFree	= false;	// nothing done yet


#if PSN_PLANTSMGR_SPU_RENDER
	// make sure SPU render job (using the str buffers as I/O buffers) is not running:
	if(sm_GrassTaskHandle)
	{
		if(!sysTaskManager::Poll(sm_GrassTaskHandle))
		{
			plantsDebugf1("\nGrassRendererSPU::UpdateStr(): Waiting for SPU render job to finish.");
			//plantsAssertf(0, "GrassRendererSPU::UpdateStr(): Waiting for SPU render job to finish.");
			sysTaskManager::Wait(sm_GrassTaskHandle);
		}
		sm_GrassTaskHandle=NULL;
	}

#if PLANTSMGR_TRIPLANTTAB_IN_BLOCKS
	// 1. UpdateStr for spuTriPlantBlockTab: 
	if(1)
	{
		static dev_u32 dbgStrTriPlantExtra=16;
		const u32 numUsed		= sm_nNumGrassJobsAddedTotal + dbgStrTriPlantExtra;	// allow few more blocks as a safeguard buffer
		const u32 numAllocated	= spuTriPlantBlockTabCount;

		if(numUsed <= numAllocated)
		{	// too many allocated - free unwanted blocks:
			const u32 numToFree = numAllocated - numUsed;
			bDoneFree = bDoneFree || numToFree?true:false;	// true if any frees are done
			for(u32 i=0; i<numToFree; i++)
			{
				spuTriPlantBlockTabCount--;
				Assert(spuTriPlantBlockTabCount>=0);

				const u32 b = spuTriPlantBlockTabCount;
				PLANTS_FREE_STR( spuTriPlantBlockTab[b] );
				spuTriPlantBlockTab[b] = NULL;
			}
		}
		else
		{	// too few allocated - allocate more blocks:
			const u32 numToAlloc = numUsed - numAllocated;
			bDoneAlloc = bDoneAlloc || numToAlloc?true:false;	// true if any allocs are done
			for(u32 i=0; i<numToAlloc; i++)
			{
				const u32 b = spuTriPlantBlockTabCount;

				if(b >= PLANTSMGR_MAX_NUM_OF_RENDER_JOBS)
					break; // no more space!

				spuTriPlantBlockTab[b] = (PPTriPlant*)PLANTS_MALLOC_STR(PLANTSMGR_TRIPLANT_BLOCK_SIZE, 128);
				//Assert(spuTriPlantBlockTab[b]);
				//ASSERT128(spuTriPlantBlockTab[b]);
				if(!spuTriPlantBlockTab[b])
					break;	// no more str memory available!

				spuTriPlantBlockTabCount++;
				Assert(spuTriPlantBlockTabCount<=PLANTSMGR_MAX_NUM_OF_RENDER_JOBS);
			}
		}
	}// end of UpdateStr for spuTriPlantBlockTab...
#endif //PLANTSMGR_TRIPLANTTAB_IN_BLOCKS...

#if PLANTSMGR_BIG_HEAP_IN_BLOCKS
	// 2. UpdateStr for BigHeap: 
	if(1)
	{
		static dev_u32 dbgStrHeapBlockExtra=32;
		const u32 numUsed		=	rage::Max(sm_BigHeapConsumed[0],	sm_BigHeapConsumed[1]) +
									rage::Max(sm_BigHeapOverfilled[0],	sm_BigHeapOverfilled[1]) +
									dbgStrHeapBlockExtra;	// allow few more blocks as a safeguard buffer
		const u32 numAllocated	= rage::Max(sm_BigHeapBlockArrayCount[0], sm_BigHeapBlockArrayCount[1]);
	
		if(numUsed <= numAllocated)
		{	// too many blocks allocated - free some:
			const u32 numToFree = numAllocated - numUsed;
			bDoneFree = bDoneFree || numToFree?true:false;
			for(u32 i=0; i<numToFree; i++)
			{
				sm_BigHeapBlockArrayCount[0]--;
				sm_BigHeapBlockArrayCount[1]--;

				const s32 b0 = sm_BigHeapBlockArrayCount[0];
				Assert(b0 >= 0);
				const s32 b1 = sm_BigHeapBlockArrayCount[1];
				Assert(b1 >= 0);

				PLANTS_FREE_STR( sm_BigHeapBlockArray[0][b0].ea );
				sm_BigHeapBlockArray[0][b0].ea		= NULL;
				sm_BigHeapBlockArray[0][b0].offset	= 0;
				PLANTS_FREE_STR( sm_BigHeapBlockArray[1][b1].ea );
				sm_BigHeapBlockArray[1][b1].ea		= NULL;
				sm_BigHeapBlockArray[1][b1].offset	= 0;
			}
		}
		else
		{	// not enough blocks allocated - allocate some:
			const u32 numToAlloc = numUsed - numAllocated;
			bDoneAlloc = bDoneAlloc || numToAlloc?true:false;
			for(u32 i=0; i<numToAlloc; i++)
			{
				const u32 b0 = sm_BigHeapBlockArrayCount[0];
				const u32 b1 = sm_BigHeapBlockArrayCount[1];

				if( (b0 >= PLANTSMGR_BIG_HEAP_BLK_ARRAY_SIZE)	||
					(b1 >= PLANTSMGR_BIG_HEAP_BLK_ARRAY_SIZE)	)
					break; // no more space!

				sm_BigHeapBlockArray[0][b0].ea		= (u32*)PLANTS_MALLOC_STR(PLANTSMGR_LOCAL_HEAP_SIZE, 128);
				//Assert(sm_BigHeapBlockArray[0][b0].ea);
				//ASSERT128(sm_BigHeapBlockArray[0][b0].ea);
				if(!sm_BigHeapBlockArray[0][b0].ea)
				{
					break;	// no more str memory!
				}

				sm_BigHeapBlockArray[1][b1].ea		= (u32*)PLANTS_MALLOC_STR(PLANTSMGR_LOCAL_HEAP_SIZE, 128);
				//Assert(sm_BigHeapBlockArray[1][b1].ea);
				//ASSERT128(sm_BigHeapBlockArray[1][b1].ea);
				if(!sm_BigHeapBlockArray[1][b1].ea)
				{
					PLANTS_FREE_STR( sm_BigHeapBlockArray[0][b0].ea );
					sm_BigHeapBlockArray[0][b0].ea		= NULL;
					sm_BigHeapBlockArray[0][b0].offset	= 0;
					break;	// no more str memory!
				}

				sm_BigHeapBlockArray[0][b0].offset	= PLANTS_GCM_OFFSET(sm_BigHeapBlockArray[0][b0].ea);
				sm_BigHeapBlockArray[1][b1].offset	= PLANTS_GCM_OFFSET(sm_BigHeapBlockArray[1][b1].ea);

				sm_BigHeapBlockArrayCount[0]++;
				sm_BigHeapBlockArrayCount[1]++;
				Assert(sm_BigHeapBlockArrayCount[0] <= PLANTSMGR_BIG_HEAP_BLK_ARRAY_SIZE);
				Assert(sm_BigHeapBlockArrayCount[1] <= PLANTSMGR_BIG_HEAP_BLK_ARRAY_SIZE);
			}
		}
	}// end of UpdateStr for BigHeap...
#endif //PLANTSMGR_BIG_HEAP_IN_BLOCKS...

#endif // PSN_PLANTSMGR_SPU_RENDER...

	return(bDoneAlloc || bDoneFree);
}// end of CGrassRenderer::UpdateStr()...


//
//
//
//
void CGrassRenderer::AddTriPlant(PPTriPlant *pSrcPlant, u32 ePlantModelSet)
{
	PPTriPlantBuffer.ChangeCurrentPlantModelsSet(ePlantModelSet);

	PPTriPlant *pDstPlant = PPTriPlantBuffer.GetPPTriPlantPtr(1);

	sysMemCpy(pDstPlant, pSrcPlant, sizeof(PPTriPlant));;
	
	PPTriPlantBuffer.IncreaseBufferIndex(ePlantModelSet, 1);

}// end of CGrassRenderer::AddPlantCluster()...



//
//
//
//
void CGrassRenderer::FlushTriPlantBuffer()
{
	PPTriPlantBuffer.Flush();
}



//
//
//
//
bool CGrassRenderer::SetPlantModelsTab(u32 index, grassModel* plantModels)
{
	return(PPTriPlantBuffer.SetPlantModelsTab(index, plantModels));
}

#if !PSN_PLANTSMGR_SPU_RENDER

void CGrassRenderer::BeginUseNormalTechniques(spdTransposedPlaneSet8 &cullFrustum)
{
	ms_cullingFrustum[g_RenderThreadIndex] = cullFrustum;

#if GRASS_INSTANCING
	#if PLANTSMGR_MULTI_RENDER
		ms_LOD0Bucket.Reset(g_RenderThreadIndex, ms_shdDeferredLOD0TechniqueID);
		ms_LOD1Bucket.Reset(g_RenderThreadIndex, ms_shdDeferredLOD1TechniqueID);
		ms_LOD2Bucket.Reset(g_RenderThreadIndex, ms_shdDeferredLOD2TechniqueID);
	#else
		ms_LOD0Bucket.Reset(ms_shdDeferredLOD0TechniqueID);
		ms_LOD1Bucket.Reset(ms_shdDeferredLOD1TechniqueID);
		ms_LOD2Bucket.Reset(ms_shdDeferredLOD2TechniqueID);
	#endif
	#if __BANK
		if(GetDebugModeEnable())
		{
		#if PLANTSMGR_MULTI_RENDER
			ms_LOD0Bucket.Reset(g_RenderThreadIndex, ms_shdDeferredLOD0DbgTechniqueID);
			ms_LOD1Bucket.Reset(g_RenderThreadIndex, ms_shdDeferredLOD1DbgTechniqueID);
			ms_LOD2Bucket.Reset(g_RenderThreadIndex, ms_shdDeferredLOD2DbgTechniqueID);
		#else
			ms_LOD0Bucket.Reset(ms_shdDeferredLOD0DbgTechniqueID);
			ms_LOD1Bucket.Reset(ms_shdDeferredLOD1DbgTechniqueID);
			ms_LOD2Bucket.Reset(ms_shdDeferredLOD2DbgTechniqueID);
		#endif
		}
	#endif
#else // GRASS_INSTANCING
	ms_shdDeferredLOD0TechniqueID_ToUse = ms_shdDeferredLOD0TechniqueID;
	ms_shdDeferredLOD1TechniqueID_ToUse = ms_shdDeferredLOD1TechniqueID;
	ms_shdDeferredLOD2TechniqueID_ToUse = ms_shdDeferredLOD2TechniqueID;
	#if __BANK
		if(GetDebugModeEnable())
		{
			ms_shdDeferredLOD0TechniqueID_ToUse = ms_shdDeferredLOD0DbgTechniqueID;
			ms_shdDeferredLOD1TechniqueID_ToUse = ms_shdDeferredLOD1DbgTechniqueID;
			ms_shdDeferredLOD2TechniqueID_ToUse = ms_shdDeferredLOD2DbgTechniqueID;
		}
	#endif
#endif // GRASS_INSTANCING
}

void CGrassRenderer::EndUseNormalTechniques()
{
#if GRASS_INSTANCING
	#if PLANTSMGR_MULTI_RENDER
		ms_LOD0Bucket.Flush(g_RenderThreadIndex);
		ms_LOD1Bucket.Flush(g_RenderThreadIndex);
		ms_LOD2Bucket.Flush(g_RenderThreadIndex);
	#else
		ms_LOD0Bucket.Flush();
		ms_LOD1Bucket.Flush();
		ms_LOD2Bucket.Flush();
	#endif
#else // GRASS_INSTANCING
	ms_shdDeferredLOD0TechniqueID_ToUse = grcetNONE;
	ms_shdDeferredLOD1TechniqueID_ToUse = grcetNONE;
	ms_shdDeferredLOD2TechniqueID_ToUse = grcetNONE;
#endif // GRASS_INSTANCING
}

#endif //!__PS3


#if PLANTS_CAST_SHADOWS
#if !PSN_PLANTSMGR_SPU_RENDER

void CGrassRenderer::BeginUseShadowTechniques(spdTransposedPlaneSet8 &cullFrustum)
{
	SetDepthBiasOn();
	ms_cullingFrustum[g_RenderThreadIndex] = cullFrustum;

#if GRASS_INSTANCING
	#if PLANTSMGR_MULTI_RENDER
		ms_LOD0Bucket.Reset(g_RenderThreadIndex, ms_shdDeferredLOD0ShadowTechniqueID);
		ms_LOD1Bucket.Reset(g_RenderThreadIndex, ms_shdDeferredLOD1ShadowTechniqueID);
		ms_LOD2Bucket.Reset(g_RenderThreadIndex, ms_shdDeferredLOD2ShadowTechniqueID);
	#else
		ms_LOD0Bucket.Reset(ms_shdDeferredLOD0ShadowTechniqueID);
		ms_LOD1Bucket.Reset(ms_shdDeferredLOD1ShadowTechniqueID);
		ms_LOD2Bucket.Reset(ms_shdDeferredLOD2ShadowTechniqueID);
	#endif
#else // GRASS_INSTANCING
	ms_shdDeferredLOD0TechniqueID_ToUse = ms_shdDeferredLOD0ShadowTechniqueID;
	ms_shdDeferredLOD1TechniqueID_ToUse = ms_shdDeferredLOD1ShadowTechniqueID;
	ms_shdDeferredLOD2TechniqueID_ToUse = ms_shdDeferredLOD2ShadowTechniqueID;
#endif
}

void CGrassRenderer::EndUseShadowTechniques()
{
#if GRASS_INSTANCING
	#if PLANTSMGR_MULTI_RENDER
		ms_LOD0Bucket.Flush(g_RenderThreadIndex);
		ms_LOD1Bucket.Flush(g_RenderThreadIndex);
		ms_LOD2Bucket.Flush(g_RenderThreadIndex);
	#else
		ms_LOD0Bucket.Flush();
		ms_LOD1Bucket.Flush();
		ms_LOD2Bucket.Flush();
	#endif
#else // GRASS_INSTANCING
	ms_shdDeferredLOD0TechniqueID_ToUse = grcetNONE;
	ms_shdDeferredLOD1TechniqueID_ToUse = grcetNONE;
	ms_shdDeferredLOD2TechniqueID_ToUse = grcetNONE;
#endif // GRASS_INSTANCING
	SetDepthBiasOff();
}
#endif //!__PS3
#endif //PLANTS_CAST_SHADOWS

//
//
//
//
grassModel* CGrassRenderer::GetPlantModelsTab(u32 index)
{
	return(PPTriPlantBuffer.GetPlantModelsTab(index));
}


void CGrassRenderer::SetGeometryLOD2(grcVertexBuffer *vb, grcVertexDeclaration *decl)
{
	if(ms_plantLOD2VertexBuffer)
	{
		// destroy this
		delete ms_plantLOD2VertexBuffer;
		ms_plantLOD2VertexBuffer = NULL;
	}

	if(ms_plantLOD2VertexDecl)
	{
		// destroy this
		ms_plantLOD2VertexDecl->Release();
		ms_plantLOD2VertexDecl = NULL;
	}

	ms_plantLOD2VertexBuffer	= vb;
	ms_plantLOD2VertexDecl		= decl;
}


//
//
//
// updates global camera pos; should be called every frame;
//
void CGrassRenderer::SetGlobalCameraPos(const Vector3& camPos)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	const s32 idx = gPlantMgr.GetUpdateRTBufferID();
	ms_vecCameraPos[idx] = camPos;
}

const Vector3& CGrassRenderer::GetGlobalCameraPos()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	const s32 idx = gPlantMgr.GetRenderRTBufferID();
	return ms_vecCameraPos[idx];
}

void CGrassRenderer::SetGlobalCameraFront(const Vector3& camFront)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	const s32 idx = gPlantMgr.GetUpdateRTBufferID();
	ms_vecCameraFront[idx] = camFront;
}

const Vector3& CGrassRenderer::GetGlobalCameraFront()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	const s32 idx = gPlantMgr.GetRenderRTBufferID();
	return ms_vecCameraFront[idx];
}

void CGrassRenderer::SetGlobalCameraFppEnabled(bool enable)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	const s32 idx = gPlantMgr.GetUpdateRTBufferID();
	ms_cameraFppEnabled[idx] = enable;
}

bool CGrassRenderer::GetGlobalCameraFppEnabled()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	const s32 idx = gPlantMgr.GetRenderRTBufferID();
	return ms_cameraFppEnabled[idx];
}

void CGrassRenderer::SetGlobalCameraUnderwater(bool enable)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	const s32 idx = gPlantMgr.GetUpdateRTBufferID();
	ms_cameraUnderwater[idx] = enable;
}

bool CGrassRenderer::GetGlobalCameraUnderwater()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	const s32 idx = gPlantMgr.GetRenderRTBufferID();
	return ms_cameraUnderwater[idx];
}

void CGrassRenderer::SetGlobalForceHDGrassGeometry(bool enable)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	const s32 idx = gPlantMgr.GetUpdateRTBufferID();
	ms_forceHdGrassGeom[idx] = enable;
}

bool CGrassRenderer::GetGlobalForceHDGrassGeometry()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	const s32 idx = gPlantMgr.GetRenderRTBufferID();
	return ms_forceHdGrassGeom[idx];
}

void CGrassRenderer::SetGlobalPlayerPos(const Vector3& playerPos)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	const s32 idx = gPlantMgr.GetUpdateRTBufferID();
	ms_vecPlayerPos[idx] = playerPos;
}

const Vector3& CGrassRenderer::GetGlobalPlayerPos()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	const s32 idx = gPlantMgr.GetRenderRTBufferID();
	return ms_vecPlayerPos[idx];
}

void CGrassRenderer::SetGlobalPlayerCollisionRSqr(float radSqr)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	const s32 idx = gPlantMgr.GetUpdateRTBufferID();
	ms_fPlayerCollRSqr[idx] = radSqr;
}

float CGrassRenderer::GetGlobalPlayerCollisionRSqr()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	const s32 idx = gPlantMgr.GetRenderRTBufferID();
	return ms_fPlayerCollRSqr[idx];
}

void CGrassRenderer::SetGlobalCullFrustum(const spdTransposedPlaneSet8& frustum)
{
	FastAssert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	const s32 idx = gPlantMgr.GetUpdateRTBufferID();
	ms_cullFrustum[idx]	= frustum;
}

void CGrassRenderer::GetGlobalCullFrustum(spdTransposedPlaneSet8 *pFrustum)
{
	FastAssert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	const s32 idx = gPlantMgr.GetRenderRTBufferID();
	Assert(pFrustum);
	*pFrustum = ms_cullFrustum[idx];
}

void CGrassRenderer::SetGlobalInteriorCullAll(bool bCullAll)
{
	FastAssert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	const s32 idx = gPlantMgr.GetUpdateRTBufferID();
	ms_interiorCullAll[idx] = bCullAll;
}

bool CGrassRenderer::GetGlobalInteriorCullAll()
{
	FastAssert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	const s32 idx = gPlantMgr.GetRenderRTBufferID();
	return ms_interiorCullAll[idx];
}



void CGrassRenderer::SetGlobalVehCollisionParams(u32 n, bool bEnable, const Vector3& vecB, const Vector3& vecM, float radius, float groundZ)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	Assert(n<NUM_COL_VEH);

const s32 idx = gPlantMgr.GetUpdateRTBufferID();
//float4	vecVehCollB : vecVehCollB0 = float4(0,0,0,0);	// [ segment start point B.xyz				| 0				]
//float4	vecVehCollM : vecVehCollM0 = float4(0,0,0,0);	// [ segment vector M.xyz					| 1.0f/dot(M,M) ]
//float4	vecVehCollR : vecVehCollR0 = float4(0,0,0,0);	// [ R	      | R*R          | 1.0f/(R*R)   | worldPosZ		]

	for (u32 uThread = 0; uThread < NUMBER_OF_RENDER_THREADS; uThread++)
	{
		ms_bVehCollisionEnabled[idx][uThread][n] = bEnable;

		Vector4 &vehCollisionB = ms_vecVehCollisionB[idx][uThread][n]; 
		vehCollisionB.x = vecB.x;
		vehCollisionB.y = vecB.y;
		vehCollisionB.z = vecB.z;
		vehCollisionB.w = 0.0f;

		Vector2 vecM2;
		vecM2.x = vecM.x;
		vecM2.y = vecM.y;
		float dotMM = vecM2.Dot(vecM2);

		Vector4 &vehCollisionM = ms_vecVehCollisionM[idx][uThread][n];
		vehCollisionM.x = vecM.x;
		vehCollisionM.y = vecM.y;
		vehCollisionM.z = vecM.z;
		vehCollisionM.w = (dotMM>0.0f)? (1.0f/dotMM) : (0.0f);

		Vector4 &vehCollisionR = ms_vecVehCollisionR[idx][uThread][n];
		vehCollisionR.x = radius;
		vehCollisionR.y = radius * radius;
		vehCollisionR.z = (radius>0.0f)? (1.0f/(radius * radius)) : (0.0f);
		vehCollisionR.w = groundZ;
	}
}

void CGrassRenderer::GetGlobalVehCollisionParams(u32 n, bool *pbEnable, Vector4 *pVecB, Vector4 *pVecM, Vector4 *pVecR)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	Assert(n<NUM_COL_VEH);

	Assert(pbEnable);
	Assert(pVecB);
	Assert(pVecM);
	Assert(pVecR);

	const s32 idx = gPlantMgr.GetRenderRTBufferID();

	*pbEnable	= ms_bVehCollisionEnabled[idx][g_RenderThreadIndex][n];
	*pVecB		= ms_vecVehCollisionB[idx][g_RenderThreadIndex][n];
	*pVecM		= ms_vecVehCollisionM[idx][g_RenderThreadIndex][n];
	*pVecR		= ms_vecVehCollisionR[idx][g_RenderThreadIndex][n];
}



//
//
//
//
void CGrassRenderer::SetGlobalWindBending(const Vector2& bending)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	const s32 idx = gPlantMgr.GetUpdateRTBufferID();
	ms_windBending[idx] = bending;
}

// GetGlobalWindBendingUT() - version for UpdateThread
void CGrassRenderer::GetGlobalWindBendingUT(Vector2 &outBending)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	const s32 idx = gPlantMgr.GetUpdateRTBufferID();
	outBending = ms_windBending[idx];
}

// GetGlobalWindBending() - version for RenderThread
void CGrassRenderer::GetGlobalWindBending(Vector2 &outBending)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	const s32 idx = gPlantMgr.GetRenderRTBufferID();
	outBending = ms_windBending[idx];
}

void CGrassRenderer::SetFakeGrassNormal(const Vector3& grassNormal)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	const s32 idx = gPlantMgr.GetUpdateRTBufferID();
	ms_fakeGrassNormal[idx] = grassNormal;
}

const Vector3& CGrassRenderer::GetFakeGrassNormal()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	const s32 idx = gPlantMgr.GetRenderRTBufferID();
	return ms_fakeGrassNormal[idx];
}

#if __BANK
void CGrassRenderer::SetDebugModeEnable(bool enable)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	const s32 idx = gPlantMgr.GetUpdateRTBufferID();
	ms_enableDebugDraw[idx] = enable;
}

bool CGrassRenderer::GetDebugModeEnable()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	const s32 idx = gPlantMgr.GetRenderRTBufferID();
	return ms_enableDebugDraw[idx];
}
#endif //__BANK...

#if FURGRASS_TEST_V4
void CGrassRenderer::SetGlobalPlayerFeetPos(const Vector4& LFootPos, const Vector4& RFootPos)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	const s32 idx = gPlantMgr.GetUpdateRTBufferID();
	ms_vecPlayerLFootPos[idx] = LFootPos;
	ms_vecPlayerRFootPos[idx] = RFootPos;
}

const Vector4& CGrassRenderer::GetGlobalPlayerLFootPos()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	const s32 idx = gPlantMgr.GetRenderRTBufferID();
	return ms_vecPlayerLFootPos[idx];
}
const Vector4& CGrassRenderer::GetGlobalPlayerRFootPos()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	const s32 idx = gPlantMgr.GetRenderRTBufferID();
	return ms_vecPlayerRFootPos[idx];
}
#endif //FURGRASS_TEST_V4...


#if PLANTS_USE_LOD_SETTINGS

float CGrassRenderer::GetDistanceMultiplier()
{
	return ms_currentScalingParams.distanceMultiplier;
}


float CGrassRenderer::GetDensityMultiplier()
{
	return ms_currentScalingParams.densityMultiplier;
}


void CGrassRenderer::SetPlantLODToUse(CGrassRenderer_PlantLod plantLOD)
{
	if(ms_isInitialized)
	{
		ms_currentScalingParams = ms_scalingParams_AllLods.m_AllLods[plantLOD];
#if PLANTS_CAST_SHADOWS
		ms_currentShadowEnvelope = ms_shadowEnvelope_AllLods.m_AllLods[plantLOD];
#endif //PLANTS_CAST_SHADOWS
	}
	else
	{
		ms_LODToInitialiseWith = (int)plantLOD;
	}
}

#endif //PLANTS_USE_LOD_SETTINGS

#if PLANTS_USE_LOD_SETTINGS

bool CGrassRenderer::ms_isInitialized = false;
int CGrassRenderer::ms_LODToInitialiseWith = 0;

#if __BANK
int CGrassRenderer::ms_LODBeingEdited = 0;
int	CGrassRenderer::ms_LODNextToEdit = 0;

static const char *g_LODTypes[3] = 
{
	"Low",
	"Medium",
	"High",
};


void CGrassRenderer::OnComboBox_LODToEdit(void)
{
	// Record the ones being edited.
	ms_scalingParams_AllLods.m_AllLods[ms_LODBeingEdited] = ms_currentScalingParams;
#if PLANTS_CAST_SHADOWS
	ms_shadowEnvelope_AllLods.m_AllLods[ms_LODBeingEdited] = ms_currentShadowEnvelope;
#endif //PLANTS_CAST_SHADOWS

	// Set the next one to edit.
	SetPlantLODToUse((CGrassRenderer_PlantLod)ms_LODNextToEdit);
	ms_LODBeingEdited = ms_LODNextToEdit;
}


void CGrassRenderer::SaveParams(void)
{
	// Record the ones being edited.
	ms_scalingParams_AllLods.m_AllLods[ms_LODBeingEdited] = ms_currentScalingParams;
#if PLANTS_CAST_SHADOWS
	ms_shadowEnvelope_AllLods.m_AllLods[ms_LODBeingEdited] = ms_currentShadowEnvelope;
#endif //PLANTS_CAST_SHADOWS

	// Save the data out.
	PARSER.SaveObject("platform:/data/GrassLODSettings", "xml", &CGrassRenderer::ms_scalingParams_AllLods, parManager::XML);
#if PLANTS_CAST_SHADOWS
	PARSER.SaveObject("platform:/data/GrassShadowLODSettings", "xml", &CGrassRenderer::ms_shadowEnvelope_AllLods, parManager::XML);
#endif //PLANTS_CAST_SHADOWS
}


void CGrassRenderer::LoadParams(void)
{
	// Load the data in.
	PARSER.LoadObject("platform:/data/GrassLODSettings", "xml", CGrassRenderer::ms_scalingParams_AllLods);
#if PLANTS_CAST_SHADOWS
	PARSER.LoadObject("platform:/data/GrassShadowLODSettings", "xml", CGrassRenderer::ms_shadowEnvelope_AllLods);
#endif //PLANTS_CAST_SHADOWS

	// Update the one being edited.
	SetPlantLODToUse((CGrassRenderer_PlantLod)ms_LODBeingEdited);
}
#endif //__BANK

#endif //PLANTS_USE_LOD_SETTINGS

#if __BANK
void CGrassRenderer::InitWidgets(bkBank& bank)
{
#if PLANTS_USE_LOD_SETTINGS
	bank.PushGroup("LOD Settings");
		bank.AddCombo("LOD type", &ms_LODNextToEdit, 3, g_LODTypes, 0, datCallback(CFA(CGrassRenderer::OnComboBox_LODToEdit)));
		bank.AddSlider("Grass distance", &CGrassRenderer::ms_currentScalingParams.distanceMultiplier, 1.0f, 15.0f, 0.1f );
		bank.AddSlider("Grass density", &CGrassRenderer::ms_currentScalingParams.densityMultiplier, 1.0f, 250.0f, 0.1f );
		#if PLANTS_CAST_SHADOWS
			bank.AddToggle("Draw shadows", &CGrassRenderer::ms_drawShadows);
			bank.AddSlider("Shadow near", &CGrassRenderer::ms_currentShadowEnvelope.fadeNear, 1.0f, 1000.0f, 0.1f );
			bank.AddSlider("Shadow far", &CGrassRenderer::ms_currentShadowEnvelope.fadeFar, 1.0f, 1000.f, 0.1f );
		#endif //PLANTS_CAST_SHADOWS
		bank.AddButton("Save", CGrassRenderer::SaveParams);
		bank.AddButton("Load", CGrassRenderer::LoadParams);
	bank.PopGroup();
#endif //PLANTS_USE_LOD_SETTINGS
}
#endif //__BANK

#if PLANTS_CAST_SHADOWS

float g_Bias = 0.00005f;
float g_zBiasMax = 1.0f;

void CGrassRenderer::SetDepthBiasOn()
{
	float A = GetShadowFadeNearRadius()*GetShadowFadeNearRadius();
	float B = GetShadowFadeFarRadius()*GetShadowFadeFarRadius();
	float C1 = -1.0f/(B - A);
	float C2 = 1.0f + A/(B - A);

	Vec4V bias = Vec4V(g_Bias, g_zBiasMax, C1, C2);
	grcEffect::SetGlobalVar(ms_depthValueBias, bias);
}

void CGrassRenderer::SetDepthBiasOff()
{
	Vec4V Zero = Vec4V(V_ZERO_WONE);
	grcEffect::SetGlobalVar(ms_depthValueBias, Zero);
}


float CGrassRenderer::GetShadowFadeNearRadius()
{
	return ms_currentShadowEnvelope.fadeNear;
}


float CGrassRenderer::GetShadowFadeFarRadius()
{
	return ms_currentShadowEnvelope.fadeFar;
}

#endif //PLANTS_CAST_SHADOWS


#if PSN_PLANTSMGR_SPU_RENDER
//
//
//
//
static
void SetupContextData(CellGcmContextData *context, const uint32_t *addr, const uint32_t size, CellGcmContextCallback callback)
{
	context->begin		= (uint32_t*)addr;
	context->current	= (uint32_t*)addr;
	context->end		= (uint32_t*)addr + size/sizeof(uint32_t) - 1;
	context->callback	= callback;
}


static
int32_t CustomGcmReserveFailed(CellGcmContextData* context, uint32_t count)
{
#if __ASSERT
	Errorf("\nCustomGcmReserveFailed: Custom context 0x%p overfilled! CurrentPos=%d. MaxSize=%d. CountRequired=%d.",
				context,
				context->current - context->begin, context->end - context->begin, count);
	Assert(FALSE);
#else
	Quitf("CustomGcmReserveFailed: Custom context 0x%p overfilled! CurrentPos=%d. MaxSize=%d. CountRequired=%d.",
		context,
		context->current - context->begin, context->end - context->begin, count);
#endif
	return(CELL_OK);
}

//
// SetupGrassTextureForSPU():
// - generates small command buffer to set particular texture on SPU
// - most of this textrue stuff is stolen from grcFragmentProgram::SetParameter();
//
//
bool CGrassRenderer::SpuRecordGrassTexture(u32 texIndex, grcTexture *data)
{
const s32 _cmdBufSize = 128;
u32 _cmdBuf[_cmdBufSize]={0};	// fill with NOPs
CellGcmContext		con1;
#if __ASSERT
CellGcmContextData	*conData1 = (CellGcmContextData*)&con1;
#endif
	SetupContextData(&con1, _cmdBuf, _cmdBufSize*sizeof(u32), &CustomGcmReserveFailed);

	const u32 texUnit = ms_shdTextureTexUnit;

	if(!data)
	{
		// generate stuff to switch off texture:
		//con1.SetTextureControl(texUnit, CELL_GCM_FALSE, 0, 0, CELL_GCM_TEXTURE_MAX_ANISO_1);
		con1.SetNopCommand(PLANTSMGR_LOCAL_TEXTURE_MAXCMDSIZE-1);	// fill up with NOPs
	}
	else
	{
		const CellGcmTexture *gcmTex = reinterpret_cast<const CellGcmTexture*>(data->GetTexturePtr());

		con1.SetTexture(texUnit,gcmTex);

		const u8 anisotropy = CELL_GCM_TEXTURE_MAX_ANISO_1;
		union { int i; float f; } x;
		x.i = 0;	//states[grcessMIPMAPLODBIAS];
		int fixedBias = int(x.f * 256.0f) & 0x1fff;	// gcm doesn't mask this unless CELL_GCM_BITFIELD is set!

		// controls anisotropic filtering for texture sampler 1
		s32 minMipLevelIndex = static_cast<u32>(gcmTex->mipmap - 1);
		s32 maxMipLevelIndex = MIN(minMipLevelIndex, 0/*MAXMIPLEVEL*/) << 8;
		minMipLevelIndex = MIN(minMipLevelIndex, 12/*grcessMINMIPLEVEL]*/) << 8;

		con1.SetTextureControl(texUnit, CELL_GCM_TRUE, // disable / enable texture sampler
			maxMipLevelIndex, minMipLevelIndex, anisotropy); 

		con1.SetTextureFilter(texUnit, fixedBias,
				CELL_GCM_TEXTURE_LINEAR_LINEAR,	//remapMin[states[grcessMIPFILTER]][states[grcessMINFILTER]],
				CELL_GCM_TEXTURE_LINEAR,	//remapMag[states[grcessMAGFILTER]],
				CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX);	//remapConv[Max(states[grcessMINFILTER],states[grcessMAGFILTER])]));

		// texture address mode is wrap ...
		con1.SetTextureAddress(texUnit,
					/*grcessADDRESSU*/CELL_GCM_TEXTURE_WRAP,
					/*grcessADDRESSV*/CELL_GCM_TEXTURE_WRAP,
					/*grcessADDRESSW*/CELL_GCM_TEXTURE_WRAP,
					CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL,
					// specify the depth texture test method used when applying the shadow map
					// CELL_GCM_TEXTURE_ZFUNC_GREATER worked well in the past with a depth test of _LESS
					// switches on and off hardware comparison and hardware PCF filtering
					CELL_GCM_TEXTURE_ZFUNC_ALWAYS, 
					gcmTex->_padding & CELL_GCM_TEXTURE_GAMMA_MASK); // sRGB reverse conversion

		// Just to be safe - I'm not sure what version this function was actually added to the Cell SDK
		// "brilinear" optimization
		con1.SetTextureOptimization(texUnit, 8/*states[grcessTRILINEARTHRESHOLD]*/, CELL_GCM_TEXTURE_ISO_HIGH, CELL_GCM_TEXTURE_ANISO_HIGH);
		// texture border color
		con1.SetTextureBorderColor(texUnit, 0);
	}// if(data)...

	con1.SetReturnCommand();	// fillup to 20 commands...


#if __ASSERT
	const u32 cbSize = ((u8*)conData1->current) - ((u8*)conData1->begin);
	Assert((cbSize/4) == PLANTSMGR_LOCAL_TEXTURE_MAXCMDSIZE);
#endif
//	plantsDebugf1("\n SetupGrassTexture: CBsize=%d, cbSize4=%d", cbSize, cbSize/4);

	PPTriPlantBuffer.SetPlantTexturesCmd(0, texIndex, _cmdBuf);

	return(TRUE);
}// end of SpuRecordGrassTexture()....



#if 1
	static void DumpMemoryAsRSXDump(const char*,u32*,u32) { /*do nothing*/}
#else
//
// useful routine to print any given memory as RSX dump buffer:
//		[000] 00000330:00000207  00000334:00000000  00000338:000000ff  00000330:00000207
//		[004] 00000334:00000000  00000338:000000ff  00001d6c:00000440  00001d74:00000000
//		[008] 00001d7c:00000001  00001efc:00000021  00001f00:3d808081  00001f04:41287aa8
//		[00c] 00001f08:3f7eff00  00001f0c:3eea4f76  00001d6c:00000440  00001d74:35082214
//
static
void DumpMemoryAsRSXDump(const char *dumpName, u32 *startPtr, u32 sizeInWords)
{
u32 *endPtr = startPtr + sizeInWords;
#define GETP(PTR) (PTR<endPtr?(*(PTR)):(0x0000BACA))

	plantsDisplayf("\n xxxxx %s:Begin", dumpName);

const u32 countW = (sizeInWords+8)/8;
u32 *p=startPtr;

	u32 i=0;
	for(; i<(countW-1); i++)
	{
		plantsDisplayf("\n [%03x] %08x:%08x  %08x:%08x  %08x:%08x  %08x:%08x", i*4,
				p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]); 
		p += 8;
	}

	// last line:
	plantsDisplayf("\n [%03x] %08x:%08x  %08x:%08x  %08x:%08x  %08x:%08x", i*4,
				GETP(&p[0]), GETP(&p[1]), GETP(&p[2]), GETP(&p[3]), GETP(&p[4]), GETP(&p[5]), GETP(&p[6]), GETP(&p[7])); 

	plantsDisplayf("\n xxxxx %s:End", dumpName);
}// end of DumpMemoryAsRSXDump()...
#endif //#if 0...


#if __ASSERT
	namespace rage	{ extern u16 g_VertexShaderInputs; }
#endif

//
//
//
//
u32 CGrassRenderer::SpuRecordShaderBind(CellGcmContextData *conData1, grcEffectTechnique forcedTech, u32 *dstCmd, u32 maxCmdSize)
{
	conData1->current = conData1->begin;	// restart context

	// Bind technique:
	ASSERT_ONLY(s32 numPasses =) ms_Shader->RecordBeginDraw(grmShader::RMC_DRAW, /*restoreState*/FALSE, /*techOverride*/forcedTech);
	Assert(numPasses>0);
	const s32 pass=0;
	ms_Shader->RecordBind(pass);

	cellGcmSetReturnCommand(conData1);

	// at this point g_VertexShaderInputs should contain valid input mask:
	Assertf(g_VertexShaderInputs && g_VertexShaderInputs!=0x0001, "g_VertexShaderInputs is 0x%X. Possibly something have gone wrong with grass VS binding?",g_VertexShaderInputs);

	// fillup to extexted CB size in words
	const u32 cbSize4 = (((u8*)conData1->current) - ((u8*)conData1->begin))/4;
	if(cbSize4 < maxCmdSize)
	{	// fill CB with extra nops:
		s32 numNops = maxCmdSize - cbSize4;
		cellGcmSetNopCommand(conData1,numNops);
	}

#if __ASSERT
	{
		if((maxCmdSize-cbSize4) > 7)
			plantsDebugf1("\n SpuRecordShader: Bind: cbSize4=%d (maxCmdSize=%d): WastedSpace=%d.", cbSize4, maxCmdSize,maxCmdSize-cbSize4);
	}
	{
		const u32 cbSize = ((u8*)conData1->current) - ((u8*)conData1->begin);
		if((cbSize/4) != maxCmdSize)
			plantsWarningf("\n SpuRecordShader: Bind: cbSize=%d, cbSize4=%d, maxCmdSize4=%d", cbSize, cbSize/4, maxCmdSize);
		Assert((cbSize/4) == maxCmdSize);
	}
#endif

	sysMemCpy(dstCmd, conData1->begin, maxCmdSize*sizeof(u32));

	return(cbSize4);
}

//
//
//
//
u32 CGrassRenderer::SpuRecordShaderUnBind(CellGcmContextData *conData1)
{
	conData1->current = conData1->begin;

	// Unbind:
	GRCDEVICE.ClearStreamSource(0);		// this doesn't actually do anything with the context, so it can stay

	ms_Shader->RecordUnBind();
	ms_Shader->RecordEndDraw();

#if __ASSERT
	{
		const u32 cbSize = ((u8*)conData1->current) - ((u8*)conData1->begin);
		if((cbSize/4) != 0)
			plantsWarningf("\n SpuRecordShader: UnBind: cbSize=%d, cbSize4=%d, maxCmdSize=%d", cbSize, cbSize/4, 0);
//		Assert((cbSize/4) == 0);
	}
#endif

	return(1);
}

//
//
//
//
u32 CGrassRenderer::SpuRecordGeometry(CellGcmContextData *con, grmGeometry *pGeometry, u32 *dstCmd, u32 maxCmdSize)
{
	grcVertexBuffer* pVB		= pGeometry->GetVertexBuffer(true);
	grcVertexDeclaration* pVDecl= pGeometry->GetDecl();

	return SpuRecordGeometry(con, pVB, pVDecl, dstCmd, maxCmdSize);
}

u32 CGrassRenderer::SpuRecordGeometry(CellGcmContextData *conData1, grcVertexBuffer* pVertexBuffer, grcVertexDeclaration* pVertexDeclaration,
														u32 *dstCmd, u32 maxCmdSize)
{
conData1->current = conData1->begin;
	
	Assert(pVertexBuffer);
	Assert(pVertexDeclaration);

	GRCDEVICE.RecordSetVertexDeclaration(pVertexDeclaration);
	GRCDEVICE.SetStreamSource(0, *pVertexBuffer, 0, pVertexBuffer->GetVertexStride());

	cellGcmSetReturnCommand(conData1);

	// fillup to extexted CB size in words
	const u32 cbSize4 = (((u8*)conData1->current) - ((u8*)conData1->begin))/4;
	if(cbSize4 < maxCmdSize)
	{	// fill CB with extra nops:
		s32 numNops = maxCmdSize - cbSize4;
		cellGcmSetNopCommand(conData1,numNops);
	}

#if __ASSERT
	{
		if((maxCmdSize-cbSize4) > 7)
			plantsDebugf1("\n SpuRecordGeometry: cbSize4=%d (maxCmdSize=%d): WastedSpace=%d.", cbSize4, maxCmdSize,maxCmdSize-cbSize4);
	}
	{
		const u32 cbSize = ((u8*)conData1->current) - ((u8*)conData1->begin);
		if((cbSize/4) != maxCmdSize)
			plantsWarningf("\n SpuRecordGeometry: cbSize=%d, cbSize4=%d, maxCmdSize=%d", cbSize, cbSize/4, maxCmdSize);
		Assert((cbSize/4) == maxCmdSize);
	}
#endif

	sysMemCpy(dstCmd, conData1->begin, maxCmdSize*sizeof(u32));

	return(cbSize4);
}


//
//
//
//
bool CGrassRenderer::SpuRecordGeometries()
{
	const s32 _cmdBufSize = 1024;
	u32 _cmdBuf[_cmdBufSize]={0};	// fill with NOPs
	CellGcmContext		con1;

//#if __ASSERT
//	CellGcmContextData	*conData1 = (CellGcmContextData*)&con1;
//#endif
	SetupContextData(&con1, _cmdBuf, _cmdBufSize*sizeof(u32), &CustomGcmReserveFailed);

	CellGcmContextData *previousContext = GCM_CONTEXT;
	GCM_CONTEXT = &con1;

	// Bind LOD0 technique:
	SpuRecordShaderBind(&con1, ms_shdDeferredLOD0TechniqueID, gpShaderLOD0Cmd0[0], PLANTSMGR_LOCAL_BINDSHADERLOD0_MAXCMDSIZE);
	DumpMemoryAsRSXDump("BindShaderLOD0", gpShaderLOD0Cmd0[0],  PLANTSMGR_LOCAL_BINDSHADERLOD0_MAXCMDSIZE);
	{
		// 2. VertexDeclaration & SetStreamSource:
		// relies on g_VertexShaderInputs set in grcVertexProgram::Bind()
		for(s32 slotID=0; slotID<PPTRIPLANT_MODELS_TAB_SIZE; slotID++)
		{
			grassModel *pPlantModelsTab = PPTriPlantBuffer.GetPlantModels(slotID);
			if(pPlantModelsTab)
			{
				for(s32 i=0; i<CPLANT_SLOT_NUM_MODELS; i++)
				{
					// LOD0 geometries only:
					const s32 modelIdx = i;
					grassModel *pPlantModel = &pPlantModelsTab[modelIdx];

					grmGeometry *pGeometry = pPlantModel->m_pGeometry;
					Assert(pGeometry);
					grcIndexBufferGCM* pIndexBuffer	= (grcIndexBufferGCM*)pGeometry->GetIndexBuffer(true);
					Assert(pIndexBuffer);

					u16* ptr = pIndexBuffer->GetGCMBuffer();
					pPlantModel->m_IdxOffset		= pIndexBuffer->GetGCMOffset();
					// note: IsLocalPtr() may not always work at startup - depends on rage::g_LocalAddress:
					pPlantModel->m_IdxLocation		= gcm::IsLocalPtr(ptr)?CELL_GCM_LOCATION_LOCAL:CELL_GCM_LOCATION_MAIN;
					pPlantModel->m_IdxCount			= pIndexBuffer->GetIndexCount();
					pPlantModel->m_DrawMode			= CELL_GCM_PRIMITIVE_TRIANGLES;
					pPlantModel->m_GeometryCmdOffset= PPTriPlantBuffer.GetPlantModelsCmdOffset(slotID, modelIdx);
					u32 *destGeomCmds				= PPTriPlantBuffer.GetPlantModelsCmd(slotID, modelIdx);

					SpuRecordGeometry(&con1, pGeometry, destGeomCmds, PLANTSMGR_LOCAL_GEOMETRY_MAXCMDSIZE);
					DumpMemoryAsRSXDump("GeometryLOD0", destGeomCmds, PLANTSMGR_LOCAL_GEOMETRY_MAXCMDSIZE);
				}//for(s32 i=0; i<CPLANT_SLOT_NUM_MODELS; i++)...
			}
		} //for(s32 slotID=0; i<PPTRIPLANT_MODELS_TAB_SIZE; slotID++)...
	}
	// Unbind LOD0:
	SpuRecordShaderUnBind(&con1);


	// Bind LOD1 technique:
	SpuRecordShaderBind(&con1, ms_shdDeferredLOD1TechniqueID, gpShaderLOD1Cmd0[0], PLANTSMGR_LOCAL_BINDSHADERLOD1_MAXCMDSIZE);
	DumpMemoryAsRSXDump("BindShaderLOD1", gpShaderLOD1Cmd0[0],  PLANTSMGR_LOCAL_BINDSHADERLOD1_MAXCMDSIZE);
	{
		// 2. VertexDeclaration & SetStreamSource:
		// relies on g_VertexShaderInputs set in grcVertexProgram::Bind()
		for(s32 slotID=0; slotID<PPTRIPLANT_MODELS_TAB_SIZE; slotID++)
		{
			grassModel *pPlantModelsTab = PPTriPlantBuffer.GetPlantModels(slotID);
			if(pPlantModelsTab)
			{
				for(s32 i=0; i<CPLANT_SLOT_NUM_MODELS; i++)
				{
					// LOD1 geometries only:
					const s32 modelIdx = i+CPLANT_SLOT_NUM_MODELS;
					grassModel *pPlantModel = &pPlantModelsTab[modelIdx];

					grmGeometry *pGeometry = pPlantModel->m_pGeometry;
					Assert(pGeometry);
					grcIndexBufferGCM* pIndexBuffer	= (grcIndexBufferGCM*)pGeometry->GetIndexBuffer(true);
					Assert(pIndexBuffer);

					u16* ptr = pIndexBuffer->GetGCMBuffer();
					pPlantModel->m_IdxOffset	= pIndexBuffer->GetGCMOffset();
					// IsLocalPtr() may not always work at startup - depends on rage::g_LocalAddress:
					pPlantModel->m_IdxLocation			= gcm::IsLocalPtr(ptr)?CELL_GCM_LOCATION_LOCAL:CELL_GCM_LOCATION_MAIN;
					pPlantModel->m_IdxCount				= pIndexBuffer->GetIndexCount();
					pPlantModel->m_DrawMode				= CELL_GCM_PRIMITIVE_TRIANGLES;
					pPlantModel->m_GeometryCmdOffset	= PPTriPlantBuffer.GetPlantModelsCmdOffset(slotID, modelIdx);
					u32 *destGeomCmds					= PPTriPlantBuffer.GetPlantModelsCmd(slotID, modelIdx);

					SpuRecordGeometry(&con1, pGeometry, destGeomCmds, PLANTSMGR_LOCAL_GEOMETRY_MAXCMDSIZE);
				}//for(s32 i=0; i<CPLANT_SLOT_NUM_MODELS; i++)...
			}
		} //for(s32 slotID=0; i<PPTRIPLANT_MODELS_TAB_SIZE; slotID++)...
	}
	// Unbind LOD1
	SpuRecordShaderUnBind(&con1);




	// Bind LOD2 technique:
	SpuRecordShaderBind(&con1, ms_shdDeferredLOD2TechniqueID, gpShaderLOD2Cmd0[0], PLANTSMGR_LOCAL_BINDSHADERLOD2_MAXCMDSIZE);
	DumpMemoryAsRSXDump("BindShaderLOD2", gpShaderLOD2Cmd0[0],  PLANTSMGR_LOCAL_BINDSHADERLOD2_MAXCMDSIZE);
	{
		// there's only one LOD2 geometry to deal with:
		SpuRecordGeometry(&con1, ms_plantLOD2VertexBuffer, ms_plantLOD2VertexDecl, &gpPlantLOD2GeometryCmd0[0], PLANTSMGR_LOCAL_GEOMETRY_MAXCMDSIZE);
		//DumpMemoryAsRSXDump("LOD2 geometry", gpPlantLOD2GeometryCmd0, PLANTSMGR_LOCAL_GEOMETRY_MAXCMDSIZE);
	}
	// Unbind LOD2
	SpuRecordShaderUnBind(&con1);

	GCM_CONTEXT = previousContext;

	// reset internal Rage state:
	if(1)
	{
		//	CGrassRenderer::BindPlantShader(m_shdDeferredLOD0TechniqueID);
		//	CGrassRenderer::UnBindPlantShader();
		grcEffectTechnique forcedTech = ms_shdDeferredLOD0TechniqueID;
		ASSERT_ONLY(s32 numPasses =) ms_Shader->BeginDraw(grmShader::RMC_DRAW, /*restoreState*/FALSE, /*techOverride*/forcedTech);
		Assert(numPasses>0);
		const s32 pass=0;
		ms_Shader->Bind(pass);
		GRCDEVICE.ClearStreamSource(0);
		ms_Shader->UnBind();
		ms_Shader->EndDraw();
	}

	return(TRUE);
}// end of SpuRecordGeometries()...


//
//
//
//
bool CGrassRenderer::SpuRecordShaders(u32 bufID)
{
	const s32 _cmdBufSize = 1024;
	u32 _cmdBuf[_cmdBufSize]={0};	// fill with NOPs
	CellGcmContext		con1;

	SetupContextData(&con1, _cmdBuf, _cmdBufSize*sizeof(u32), &CustomGcmReserveFailed);

	CellGcmContextData *previousContext = GCM_CONTEXT;
	GCM_CONTEXT = &con1;


	grcEffectTechnique techLOD0ID = ms_shdDeferredLOD0TechniqueID;
	grcEffectTechnique techLOD1ID = ms_shdDeferredLOD1TechniqueID;
	grcEffectTechnique techLOD2ID = ms_shdDeferredLOD2TechniqueID;
#if __BANK
	if(GetDebugModeEnable())
	{
		techLOD0ID = ms_shdDeferredLOD0DbgTechniqueID;
		techLOD1ID = ms_shdDeferredLOD1DbgTechniqueID;
		techLOD2ID = ms_shdDeferredLOD2DbgTechniqueID;
	}
#endif
	
	// 1. Records LOD0 technique:
	SpuRecordShaderBind(&con1, techLOD0ID, gpShaderLOD0Cmd0[bufID], PLANTSMGR_LOCAL_BINDSHADERLOD0_MAXCMDSIZE);
//	DumpMemoryAsRSXDump("BindShaderLOD0", gpShaderLOD0Cmd0,  PLANTSMGR_LOCAL_BINDSHADERLOD0_MAXCMDSIZE);
	SpuRecordShaderUnBind(&con1);

	// 2. Record LOD1 technique:
	SpuRecordShaderBind(&con1, techLOD1ID, gpShaderLOD1Cmd0[bufID], PLANTSMGR_LOCAL_BINDSHADERLOD1_MAXCMDSIZE);
//	DumpMemoryAsRSXDump("BindShaderLOD1", gpShaderLOD1Cmd0,  PLANTSMGR_LOCAL_BINDSHADERLOD1_MAXCMDSIZE);
	SpuRecordShaderUnBind(&con1);

	// 3. Records LOD2 technique:
	SpuRecordShaderBind(&con1, techLOD2ID, gpShaderLOD2Cmd0[bufID], PLANTSMGR_LOCAL_BINDSHADERLOD2_MAXCMDSIZE);
//	DumpMemoryAsRSXDump("BindShaderLOD2", gpShaderLOD2Cmd0,  PLANTSMGR_LOCAL_BINDSHADERLOD2_MAXCMDSIZE);
	SpuRecordShaderUnBind(&con1);

	GCM_CONTEXT = previousContext;

	return(TRUE);
}// end of SpuRecordShaders()...
#endif //PSN_PLANTSMGR_SPU_RENDER...



#if PSN_PLANTSMGR_SPU_RENDER

static spuGrassParamsStructMaster	inSpuParamsMaster		ALIGNED(128);
static spuGrassParamsStructMaster	outSpuParamsMaster		ALIGNED(128);

//
//
//
//
bool CGrassRenderer::DrawTriPlantsSPU(PPTriPlant *pCurrTriPlant, const u32 numTriPlants, u32 UNUSED_PARAM(slotID))
{

		// enough space for new jobs data?
	#if PLANTSMGR_TRIPLANTTAB_IN_BLOCKS
		if(sm_nNumGrassJobsAdded >= spuTriPlantBlockTabCount)
	#else
		if(sm_nNumGrassJobsAdded >= PLANTSMGR_MAX_NUM_OF_RENDER_JOBS)
	#endif
		{
		#if PLANTSMGR_TRIPLANTTAB_IN_BLOCKS
			plantsWarningf("\nGrassRendererSPU: Max num of batches (%d) already running! Recycling some space to continue (taskID=0x%X).", spuTriPlantBlockTabCount, rsxLabel5_CurrentTaskID);
		#else
			// should it EVER happen during 1 frame?
			plantsAssertf(0, "GrassRendererSPU: Max num of batches (%d) already running! Recycling some space to continue (taskID=0x%X).", PLANTSMGR_MAX_NUM_OF_RENDER_JOBS, rsxLabel5_CurrentTaskID);
		#endif
	
			// launch and wait for some launched jobs to finish first:
			Assert(sm_GrassTaskHandle==NULL);
			CGrassRenderer::SpuFinalisePerRenderFrame();
			sysTaskManager::Wait(sm_GrassTaskHandle);
			sm_GrassTaskHandle		= NULL;
			sm_nNumGrassJobsAdded	= 0;

			// update str memory usage stats for BigHeap:
			SpuUpdateStrStats(&outSpuParamsMaster);
			
			GRCDEVICE.KickOffGpu();	// flush drawablespu to make RSX consume just issued command buffers
		}

		const u32 jobIndex = sm_nNumGrassJobsAdded;

		// copy data to aligned structures
#if PLANTSMGR_TRIPLANTTAB_IN_BLOCKS
		PPTriPlant *spuTriPlantPtr0		= spuTriPlantBlockTab[jobIndex];
#else
		PPTriPlant *spuTriPlantPtr0		= &spuTriPlantTab[jobIndex][0];
#endif
		sysMemCpy(spuTriPlantPtr0, pCurrTriPlant, sizeof(PPTriPlant)*numTriPlants);


		// setup data for this job:
		spuGrassParamsStruct *inSpuGrassStruct	= &inSpuGrassStructTab[jobIndex];
		ASSERT16(inSpuGrassStruct);
			
		inSpuGrassStruct->m_pTriPlant			= spuTriPlantPtr0;
	#if PLANTSMGR_TRIPLANTTAB_IN_BLOCKS
		ASSERT128(spuTriPlantPtr0);
	#else
		ASSERT16(spuTriPlantPtr0);
	#endif
		inSpuGrassStruct->m_numTriPlant			= numTriPlants;
		Assert(numTriPlants <= PPTRIPLANT_BUFFER_SIZE);
		Assert(numTriPlants < 65536);	// must fit into u16

		sm_nNumGrassJobsAdded++;		// # of jobs added this batch
		sm_nNumGrassJobsAddedTotal++;	// total # of jobs
		
		return(TRUE);
}


//
//
// launches master job to draw grass:
//
bool CGrassRenderer::SpuFinalisePerRenderFrame()
{
	// is there anything to do?
	if(!sm_nNumGrassJobsAdded)
		return(FALSE);

	Assert(sm_GrassTaskHandle==NULL);

	GRCDBG_PUSH("GrassSpuFinalisePerRenderFrame");

const Vector3	globalCameraPos(GetGlobalCameraPos());
const Vector3	globalPlayerPos(GetGlobalPlayerPos());


	ms_Shader->RecordSetVar(ms_shdCameraPosID, globalCameraPos);
	ms_Shader->RecordSetVar(ms_shdPlayerPosID, globalPlayerPos);

	for(u32 v=0; v<NUM_COL_VEH; v++)
	{
		bool bVehCollisionEnabled=FALSE;
		Vector4 vecVehCollisionB, vecVehCollisionM, vecVehCollisionR;
		GetGlobalVehCollisionParams(v, &bVehCollisionEnabled, &vecVehCollisionB, &vecVehCollisionM, &vecVehCollisionR);
	
		grcEffect::SetGlobalVar(ms_shdVehCollisionEnabledID[v],	bVehCollisionEnabled);
		ms_Shader->RecordSetVar(ms_shdVehCollisionBID[v],		vecVehCollisionB);
		ms_Shader->RecordSetVar(ms_shdVehCollisionMID[v],		vecVehCollisionM);
		ms_Shader->RecordSetVar(ms_shdVehCollisionRID[v],		vecVehCollisionR);
	}

	Vector4 fakeNormal;
	fakeNormal.SetVector3ClearW(GetFakeGrassNormal());
	fakeNormal.SetW(1.0f);
	ms_Shader->RecordSetVar(ms_shdFakedGrassNormal, fakeNormal);

#if __BANK
	extern bool gbPlantsGeometryCulling;
	dev_bool bGeomCullingTestEnabled = gbPlantsGeometryCulling;
	extern bool gbPlantsPremultiplyNdotL;
	dev_bool bPremultiplyNdotL = gbPlantsPremultiplyNdotL;
#endif


#if __BANK
	extern bool		gbPlantsFlashLOD0, gbPlantsFlashLOD1, gbPlantsFlashLOD2;
	extern bool		gbPlantsDisableLOD0, gbPlantsDisableLOD1, gbPlantsDisableLOD2;
	extern float	gbPlantsLOD0AlphaCloseDist, gbPlantsLOD0AlphaFarDist, gbPlantsLOD0FarDist;
	extern float	gbPlantsLOD1CloseDist, gbPlantsLOD1FarDist, gbPlantsLOD1Alpha0CloseDist, gbPlantsLOD1Alpha0FarDist, gbPlantsLOD1Alpha1CloseDist, gbPlantsLOD1Alpha1FarDist;
	extern float	gbPlantsLOD2CloseDist, gbPlantsLOD2FarDist, gbPlantsLOD2Alpha0CloseDist, gbPlantsLOD2Alpha0FarDist, gbPlantsLOD2Alpha1CloseDist, gbPlantsLOD2Alpha1FarDist;
	extern float	gbPlantsLOD2Alpha1CloseDistFar, gbPlantsLOD2Alpha1FarDistFar;
	const float	globalLOD0AlphaCloseDist	= gbPlantsLOD0AlphaCloseDist;
	float		globalLOD0AlphaFarDist		= gbPlantsLOD0AlphaFarDist;
	const float	globalLOD0FarDist			= gbPlantsLOD0FarDist;
	const float	globalLOD1CloseDist			= gbPlantsLOD1CloseDist;
	const float	globalLOD1FarDist			= gbPlantsLOD1FarDist;
	const float	globalLOD1Alpha0CloseDist	= gbPlantsLOD1Alpha0CloseDist;
	const float	globalLOD1Alpha0FarDist		= gbPlantsLOD1Alpha0FarDist;
	const float	globalLOD1Alpha1CloseDist	= gbPlantsLOD1Alpha1CloseDist;
	const float	globalLOD1Alpha1FarDist		= gbPlantsLOD1Alpha1FarDist;
	const float	globalLOD2CloseDist			= gbPlantsLOD2CloseDist;
	const float	globalLOD2FarDist			= gbPlantsLOD2FarDist;
	const float	globalLOD2Alpha0CloseDist	= gbPlantsLOD2Alpha0CloseDist;
	const float	globalLOD2Alpha0FarDist		= gbPlantsLOD2Alpha0FarDist;
	const float	globalLOD2Alpha1CloseDist	= gbPlantsLOD2Alpha1CloseDist;
	const float	globalLOD2Alpha1FarDist		= gbPlantsLOD2Alpha1FarDist;
	const float	globalLOD2Alpha1CloseDistFar= gbPlantsLOD2Alpha1CloseDistFar;
	const float	globalLOD2Alpha1FarDistFar	= gbPlantsLOD2Alpha1FarDistFar;
#else
	const float	globalLOD0AlphaCloseDist	= CPLANT_LOD0_ALPHA_CLOSE_DIST;
	float		globalLOD0AlphaFarDist		= CPLANT_LOD0_ALPHA_FAR_DIST;
	const float	globalLOD0FarDist			= CPLANT_LOD0_FAR_DIST;
	const float	globalLOD1CloseDist			= CPLANT_LOD1_CLOSE_DIST;
	const float	globalLOD1FarDist			= CPLANT_LOD1_FAR_DIST;
	const float	globalLOD1Alpha0CloseDist	= CPLANT_LOD1_ALPHA0_CLOSE_DIST;
	const float	globalLOD1Alpha0FarDist		= CPLANT_LOD1_ALPHA0_FAR_DIST;
	const float	globalLOD1Alpha1CloseDist	= CPLANT_LOD1_ALPHA1_CLOSE_DIST;
	const float	globalLOD1Alpha1FarDist		= CPLANT_LOD1_ALPHA1_FAR_DIST;
	const float	globalLOD2CloseDist			= CPLANT_LOD2_CLOSE_DIST;
	const float	globalLOD2FarDist			= CPLANT_LOD2_FAR_DIST;
	const float	globalLOD2Alpha0CloseDist	= CPLANT_LOD2_ALPHA0_CLOSE_DIST;
	const float	globalLOD2Alpha0FarDist		= CPLANT_LOD2_ALPHA0_FAR_DIST;
	const float	globalLOD2Alpha1CloseDist	= CPLANT_LOD2_ALPHA1_CLOSE_DIST;
	const float	globalLOD2Alpha1FarDist		= CPLANT_LOD2_ALPHA1_FAR_DIST;
	const float	globalLOD2Alpha1CloseDistFar= CPLANT_LOD2_ALPHA1_CLOSE_DIST_FAR;
	const float	globalLOD2Alpha1FarDistFar	= CPLANT_LOD2_ALPHA1_FAR_DIST_FAR;
#endif
	float	globalLOD0FarDist2		= globalLOD0FarDist*globalLOD0FarDist;
	float	globalLOD1CloseDist2	= globalLOD1CloseDist*globalLOD1CloseDist;
	float	globalLOD1FarDist2		= globalLOD1FarDist*globalLOD1FarDist;
	float	globalLOD2CloseDist2	= globalLOD2CloseDist*globalLOD2CloseDist;
	float	globalLOD2FarDist2		= globalLOD2FarDist*globalLOD2FarDist;

	if(CGrassRenderer::GetGlobalCameraFppEnabled() || CGrassRenderer::GetGlobalForceHDGrassGeometry())
	{	// special case: camera is in FPP mode - force LOD0 to be visible everywhere:
		globalLOD0AlphaFarDist		= globalLOD2Alpha1FarDistFar;

		globalLOD0FarDist2			= globalLOD2FarDist2;
		globalLOD1CloseDist2		=
		globalLOD1FarDist2			=
		globalLOD2CloseDist2		=
		globalLOD2FarDist2			= 999999.0f;
	}


	// LOD0 alpha, umTimer:
	{
		Vector4 vecAlphaFade4;	// [XY=alphaFade01 | Z=umTimer | ?]
#if 1
		// square distance approach:
		const float closeDist	=	globalLOD0AlphaCloseDist;
		const float farDist		=	globalLOD0AlphaFarDist; 
		const float alphaDenom	= 1.0f / (farDist*farDist - closeDist*closeDist);
		vecAlphaFade4.x = (farDist*farDist) * alphaDenom; // x = (f2)  / (f2-c2)
		vecAlphaFade4.y = (-1.0f) * alphaDenom;		 // y = (-1.0)/ (f2-c2) 
#else
		// linear distance approach:
		const float alphaDenom = 1.0f / (m_farDist - m_closeDist);
		vecAlphaFade4.x = (m_farDist) * alphaDenom; // x = (f2)  / (f2-c2)
		vecAlphaFade4.y = (-1.0f) * alphaDenom;		 // y = (-1.0)/ (f2-c2) 
#endif
		vecAlphaFade4.z = (float)(2.0*PI*(double(fwTimer::GetTimeInMilliseconds())/1000.0));
		vecAlphaFade4.w = 0.0f;
		ms_Shader->RecordSetVar(ms_shdFadeAlphaDistUmTimerID, vecAlphaFade4);
	}

	// LOD1 alpha:
	{
		Vector4 vecAlphaFade4;	// [XY=alpha0Fade01 | ZW=alpha1Fade01]

		const float closeDist0	= globalLOD1Alpha0CloseDist;
		const float farDist0	= globalLOD1Alpha0FarDist;
		const float alphaDenom0 = 1.0f / (farDist0*farDist0 - closeDist0*closeDist0);
		vecAlphaFade4.x = (farDist0*farDist0) * alphaDenom0;	// x = (f2)  / (f2-c2)
		vecAlphaFade4.y = (-1.0f) * alphaDenom0;				// y = (-1.0)/ (f2-c2) 

		const float closeDist1	=	globalLOD1Alpha1CloseDist;
		const float farDist1	=	globalLOD1Alpha1FarDist;
		const float alphaDenom1 = 1.0f / (farDist1*farDist1 - closeDist1*closeDist1);
		vecAlphaFade4.z = (farDist1*farDist1) * alphaDenom1;	// x = (f2)  / (f2-c2)
		vecAlphaFade4.w = (-1.0f) * alphaDenom1;			// y = (-1.0)/ (f2-c2) 

		ms_Shader->RecordSetVar(ms_shdFadeAlphaLOD1DistID, vecAlphaFade4);
	}

	// LOD2 alpha:
	{
		Vector4 vecAlphaFade4;	// [XY=alpha0Fade01 | ZW=alpha1Fade01]

		const float closeDist0	= globalLOD2Alpha0CloseDist;
		const float farDist0	= globalLOD2Alpha0FarDist;
		const float alphaDenom0 = 1.0f / (farDist0*farDist0 - closeDist0*closeDist0);
		vecAlphaFade4.x = (farDist0*farDist0) * alphaDenom0;	// x = (f2)  / (f2-c2)
		vecAlphaFade4.y = (-1.0f) * alphaDenom0;				// y = (-1.0)/ (f2-c2) 

		const float closeDist1	=	globalLOD2Alpha1CloseDist;
		const float farDist1	=	globalLOD2Alpha1FarDist;
		const float alphaDenom1 = 1.0f / (farDist1*farDist1 - closeDist1*closeDist1);
		vecAlphaFade4.z = (farDist1*farDist1) * alphaDenom1;	// x = (f2)  / (f2-c2)
		vecAlphaFade4.w = (-1.0f) * alphaDenom1;				// y = (-1.0)/ (f2-c2) 

		ms_Shader->RecordSetVar(ms_shdFadeAlphaLOD2DistID, vecAlphaFade4);


		Vector4 vecAlphaFadeFar4;	// [XY=alpha1FadeFar01 | 0 | 0]

		const float closeDistFar0	= globalLOD2Alpha1CloseDistFar;
		const float farDistFar0		= globalLOD2Alpha1FarDistFar;
		const float alphaDenomFar0	= 1.0f / (farDistFar0*farDistFar0 - closeDistFar0*closeDistFar0);
		vecAlphaFadeFar4.x = (farDistFar0*farDistFar0) * alphaDenomFar0;	// x = (f2)  / (f2-c2)
		vecAlphaFadeFar4.y = (-1.0f) * alphaDenomFar0;						// y = (-1.0)/ (f2-c2) 
		vecAlphaFadeFar4.z = 0.0f;
		vecAlphaFadeFar4.w = 0.0f;

		ms_Shader->RecordSetVar(ms_shdFadeAlphaLOD2DistFarID, vecAlphaFadeFar4);
	}



	PF_PUSH_TIMEBAR_DETAIL("PlantsMgr::Render::WaitForRSX");
	// make sure RSX is not executing our previous buffer:
	// (another way to check this is to set label after RSX has returned to main CB
	// and read it here, however this way seem quicker):
	if(1)
	{
		CellGcmControl volatile *control = cellGcmGetControlRegister();
		uint32_t get = control->get;
		u32 deadman=20;

#if PLANTSMGR_BIG_HEAP_IN_BLOCKS
		while( BigHeapBlockArrayContainsOffset(sm_BigHeapID, PLANTSMGR_LOCAL_HEAP_SIZE, get)
#else
		while(((get >= sm_BigHeapOffset[sm_BigHeapID])	&& (get < (sm_BigHeapOffset[sm_BigHeapID]+sm_BigHeapSize[sm_BigHeapID])))
#endif
#if SPU_GCM_FIFO						
			|| ((get >= sm_StartupHeapOffset[sm_BigHeapID])	&& (get < (sm_StartupHeapOffset[sm_BigHeapID]+STARTUP_HEAP_SIZE)))
#endif	
			)
		{
			sys_timer_usleep(30);
			get = control->get;
			if(!(--deadman))
			{
				deadman=20;
				plantsDebugf1("\n PlantsMgrSPU: Task %X has to wait - RSX is still executing previous command buffer!", rsxLabel5_CurrentTaskID);
			}
		}
	}

	// make sure RSX really executed prevPrev command buffer:
	if(1)
	{
		const u32 prevPrevTaskID= 0xFF000000|(((sm_BigHeapID  )&0x1)<<8)|	((rsxLabel5_CurrentTaskID-2)&0xFF);
		const u32 prevTaskID0	=											((rsxLabel5_CurrentTaskID-1)&0xFF);
		const u32 prevTaskID1	= 0xFF000000|(((sm_BigHeapID-1)&0x1)<<8)|	((rsxLabel5_CurrentTaskID-1)&0xFF);

		u32 label5 = *rsxLabel5Ptr;
		//plantsDebugf1("\n**Grass: label5=%X, prevTaskID=%X", label5, prevTaskID);

		u32 deadman=40;
		while((label5 != prevPrevTaskID) && (label5 != prevTaskID0) && (label5 != prevTaskID1))
		{
			sys_timer_usleep(30);
			if(!(--deadman))
			{
				deadman=40;
				plantsDebugf1("\nPPU: PlantsMgrSPU: PrevPrevJob %X still not executed by RSX [label5=%08X]!", prevPrevTaskID,label5);
			}
			label5 = *rsxLabel5Ptr;
		}
	}
	PF_POP_TIMEBAR_DETAIL(); //WaitForRSX()...
	
	GRCDBG_PUSH("RecordShaders");
	// record all shaders for SPU:
	CGrassRenderer::SpuRecordShaders(sm_BigHeapID);


	BindPlantShader(ms_shdDeferredLOD0TechniqueID);
	GRCDBG_POP();
	GRCDBG_PUSH("RenderBegin");
	{
		// set waiting marker for SPU:
#if SPU_GCM_FIFO
		// mStartupHeapPtr = small startup "bridge" command buffer from Rage's FIFO to plants CB and then to jump back:
		CellGcmContext		startupCon;
		CellGcmContextData*	startupConData = (CellGcmContextData*)&startupCon;
		SetupContextData(&startupCon, (u32*)sm_StartupHeapPtr[sm_BigHeapID], STARTUP_HEAP_SIZE, &CustomGcmReserveFailed);
		startupCon.SetWriteCommandLabel(rsxLabel5Index, rsxLabel5_CurrentTaskID);
#else //SPU_GCM_FIFO
		GCM_DEBUG(cellGcmSetWriteCommandLabel(GCM_CONTEXT, rsxLabel5Index, rsxLabel5_CurrentTaskID));
#endif //SPU_GCM_FIFO...

		//GRCDEVICE.KickOffGpu(); // set PUT pointer to end of current buffer
		//plantsDebugf1("\n PlantsRenderer: setLabel5=%X at %X(%X)", rsxLabel5_CurrentTaskID, u32(GCM_CONTEXT->current),gcm::MainOffset((u8*)(GCM_CONTEXT->current)));


	#if SPU_GCM_FIFO
		// JTS in startupCon (will be patched to jump to big heap):
		u32 offsetJTS0 = ((u8*)startupConData->current) - ((u8*)startupConData->begin);
		while( offsetJTS0 & 0x0000000f )	// fix pointer to be 16 byte aligned
		{
			startupCon.SetNopCommand(1);
			offsetJTS0 = ((u8*)startupConData->current) - ((u8*)startupConData->begin);
		}
		ASSERT16(offsetJTS0);

		u32 eaMainJTS = u32(sm_StartupHeapPtr[sm_BigHeapID]) + offsetJTS0;

		startupCon.SetJumpCommand(u32(sm_StartupHeapOffset[sm_BigHeapID]) + offsetJTS0);	// JTS
		startupCon.SetNopCommand(7);

		const u32 rsxReturnOffsetToMainCB = (u32)PLANTS_GCM_OFFSET((u8*)startupCon.current);
		startupCon.SetNopCommand(8);

		// "job executed" marker:
		startupCon.SetWriteCommandLabel(rsxLabel5Index, 0xFF000000|(sm_BigHeapID<<8)|rsxLabel5_CurrentTaskID);
		//plantsDebugf1("\n setting label5=%X",0xFF000000|(mBigHeapID<<8)|rsxLabel5_CurrentTaskID);
	#else
		Assert(0);
	#endif


#if	SPU_GCM_FIFO
		// jump from Rage's GCM FIFO to plants' startup FIFO and patch jump back:
		SPU_COMMAND(grcDevice__Jump, 0);
		// Local address of the jump target
		cmd->jumpLocalOffset = PLANTS_GCM_OFFSET(startupCon.begin);
		// PPU address of the return jump
		cmd->jumpBack		= startupCon.current;
		ASSERT16(startupCon.current);
		// plantsDebugf1("\n startupCon.current=%X",(u32)startupCon.current);
		if(0)
		{
			plantsDebugf1("\n grcDevice__Jump: jumpLocalAddr=%X, jumpBack=%p",		cmd->jumpLocalOffset, cmd->jumpBack);
			plantsDebugf1("\n (jumpLocalAddr: L=%X, M=%X) (jumpBack: L=%X, M=%X)",	(u32)PLANTS_GCM_OFFSET(startupCon.begin), (u32)startupCon.begin, (u32)PLANTS_GCM_OFFSET(startupCon.current), (u32)startupCon.current);
		}
#endif //SPU_GCM_FIFO...


		sysTaskParameters p;
		sysMemSet(&p,0x00,sizeof(p));

		// "master" job data:
		inSpuParamsMaster.m_nNumGrassJobsLaunched	= sm_nNumGrassJobsAdded;
		inSpuParamsMaster.m_inSpuGrassStructTab		= &inSpuGrassStructTab[0];
		
#if PLANTSMGR_BIG_HEAP_IN_BLOCKS
		inSpuParamsMaster.m_heapBlockArray			= sm_BigHeapBlockArray[sm_BigHeapID];
		inSpuParamsMaster.m_heapBlockArrayCount		= sm_BigHeapBlockArrayCount[sm_BigHeapID];
		Assert128(inSpuParamsMaster.m_heapBlockArray);
		Assert(inSpuParamsMaster.m_heapBlockArrayCount > 0);
#else
		inSpuParamsMaster.m_heapBegin				= (u32)sm_BigHeap[sm_BigHeapID];
		inSpuParamsMaster.m_heapBeginOffset			= sm_BigHeapOffset[sm_BigHeapID];
		inSpuParamsMaster.m_heapSize				= sm_BigHeapSize[sm_BigHeapID];
		AlignedAssertf(inSpuParamsMaster.m_heapSize/1024, PLANTSMGR_LOCAL_HEAP_SIZE/1024, "heapSize must be multiple of PLANTSMGR_LOCAL_HEAP_SIZE!");
		Assert(inSpuParamsMaster.m_heapSize >= PLANTSMGR_LOCAL_HEAP_SIZE);
#endif
		inSpuParamsMaster.m_heapID					= sm_BigHeapID;

		inSpuParamsMaster.m_ReturnOffsetToMainCB	= rsxReturnOffsetToMainCB;
		inSpuParamsMaster.m_eaMainJTS				= eaMainJTS;
		
		inSpuParamsMaster.m_rsxLabel5_CurrentTaskID	= rsxLabel5_CurrentTaskID;		// value of wait tag in Label4 for current job
		inSpuParamsMaster.m_DeviceFrameCounter		= GRCDEVICE.GetFrameCounter();


		inSpuParamsMaster.m_rsxLabel5Index		= rsxLabel5Index;
		inSpuParamsMaster.m_eaRsxLabel5			= (u32)rsxLabel5Ptr;


		const Vector3 vecCameraPos = CGrassRenderer::GetGlobalCameraPos();
		inSpuParamsMaster.m_vecCameraPos.SetVector3(vecCameraPos);
		inSpuParamsMaster.m_vecCameraPos.w			= 0.0f;
		GetGlobalWindBending(inSpuParamsMaster.m_windBending);
		   
#if CPLANT_PRE_MULTIPLY_LIGHTING
		inSpuParamsMaster.m_mainDirectionalLight_natAmbient		= Vector4(-Lights::GetRenderDirectionalLight().GetDirection());
		inSpuParamsMaster.m_mainDirectionalLight_natAmbient.w	= Lights::m_lightingGroup.GetNaturalAmbientBase().w;
#endif

		inSpuParamsMaster.m_LOD0FarDist2			= globalLOD0FarDist2;
		inSpuParamsMaster.m_LOD1CloseDist2			= globalLOD1CloseDist2;
		inSpuParamsMaster.m_LOD1FarDist2			= globalLOD1FarDist2;
		inSpuParamsMaster.m_LOD2CloseDist2			= globalLOD2CloseDist2;
		inSpuParamsMaster.m_LOD2FarDist2			= globalLOD2FarDist2;

		inSpuParamsMaster.m_BindShaderLOD0Offset	= gpShaderLOD0Offset[sm_BigHeapID];
		inSpuParamsMaster.m_BindShaderLOD1Offset	= gpShaderLOD1Offset[sm_BigHeapID];
		inSpuParamsMaster.m_BindShaderLOD2Offset	= gpShaderLOD2Offset[sm_BigHeapID];
		inSpuParamsMaster.m_GeometryLOD2Offset		= gpPlantLOD2GeometryOffset;

		inSpuParamsMaster.m_PlantTexturesOffsetTab	= PPTriPlantBuffer.GetPlantTexturesCmdOffset(0, 0);
		inSpuParamsMaster.m_PlantModelsTab			= PPTriPlantBuffer.GetPlantModelsTab(0);
		ASSERT128(inSpuParamsMaster.m_PlantModelsTab);

		ASSERT16(PLANTSMGR_LOCAL_SLOTSIZE_TEXTURE);
		ASSERT16(PLANTSMGR_LOCAL_SLOTSIZE_MODEL);

#if CPLANT_DYNAMIC_CULL_SPHERES
		const u32 id = gPlantMgr.GetRenderRTBufferID();
		inSpuParamsMaster.m_CullSphereArray		= &(gPlantMgr.GetCullSphereArray(id));
#endif

		inSpuParamsMaster.m_DecrementerTicks		= 0;
#if __BANK
		// statistics & debug:
		inSpuParamsMaster.m_bPlantsFlashLOD0		= gbPlantsFlashLOD0;
		inSpuParamsMaster.m_bPlantsFlashLOD1		= gbPlantsFlashLOD1;
		inSpuParamsMaster.m_bPlantsFlashLOD2		= gbPlantsFlashLOD2;
		inSpuParamsMaster.m_bPlantsDisableLOD0		= gbPlantsDisableLOD0;
		inSpuParamsMaster.m_bPlantsDisableLOD1		= gbPlantsDisableLOD1;
		inSpuParamsMaster.m_bPlantsDisableLOD2		= gbPlantsDisableLOD2;
		inSpuParamsMaster.m_bGeomCullingTestEnabled	= bGeomCullingTestEnabled;
		inSpuParamsMaster.m_bPremultiplyNdotL		= bPremultiplyNdotL;

		inSpuParamsMaster.m_LocTriPlantsLOD0DrawnPerSlot	= 0;
		inSpuParamsMaster.m_LocTriPlantsLOD1DrawnPerSlot	= 0;
		inSpuParamsMaster.m_LocTriPlantsLOD2DrawnPerSlot	= 0;
#endif //__BANK...


		// current culling frustum (current VP or from portal):
		GetGlobalCullFrustum( &inSpuParamsMaster.m_cullFrustum );

		ASSERT128(&inSpuParamsMaster);
		ASSERT128(&outSpuParamsMaster);
		ASSERT16(sizeof(inSpuParamsMaster));
		ASSERT16(sizeof(outSpuParamsMaster));


		p.Input.Data	= (void*)&inSpuParamsMaster;
		p.Input.Size	= sizeof(inSpuParamsMaster);
		p.Output.Data	= (void*)&outSpuParamsMaster;
		p.Output.Size	= sizeof(outSpuParamsMaster);
		p.Scratch.Size	= 28 * 1024;	// allocate 28KB for scratch
		p.SpuStackSize	= 16 * 1024;	// allocate 16KB for stack


		// run the category update job
		sys_lwsync();
		sm_GrassTaskHandle = sysTaskManager::Create(TASK_INTERFACE(PlantsGrassRendererSPU),p, sysTaskManager::SCHEDULER_GRAPHICS_OTHER);
		Assertf(sm_GrassTaskHandle, "PSN: Error creating SPU grass job!");
		//plantsDebugf1("\n GrassRendererSPU: launching job: (id=%X) (numjobs=%d) (gcm::current=%X(%X))", rsxLabel5_CurrentTaskID, sm_nNumGrassJobsAdded, u32(GCM_CONTEXT->current),gcm::MainOffset((u8*)GCM_CONTEXT->current) );


		(++sm_BigHeapID)&=0x01;

		(++rsxLabel5_CurrentTaskID)&=0xFF;
	}
	GRCDBG_POP();
	GRCDBG_PUSH("RenderEnd");
	UnBindPlantShader();

	InvalidateSpuGcmState(CachedStates.VertexFormats, -1);
	InvalidateSpuGcmState(CachedStates.SamplerState[ms_shdTextureTexUnit], -1);

	// make sure processing is finished
	if(0 && sm_GrassTaskHandle)
	{
		sysTaskManager::Wait(sm_GrassTaskHandle);
		sm_GrassTaskHandle=NULL;
		//plantsDebugf1("\n pad0=%d, pad1=%d, pad2=%d\n",outSpuGrassStruct.pad0,outSpuGrassStruct.pad1,outSpuGrassStruct.pad2);
	}

	GRCDBG_POP();
	GRCDBG_POP(); // "CGrassRenderer::SpuFinalisePerRenderFrame"...
	return(TRUE);
}// end of FinalizePerRenderFrame()...


static bool bInitialisedPerRenderFrameRunOnce = FALSE;

//
//
//
//
bool CGrassRenderer::SpuInitialisePerRenderFrame()
{
	// run once:
	if(!bInitialisedPerRenderFrameRunOnce)
	{
		bInitialisedPerRenderFrameRunOnce = TRUE;
		if(!CGrassRenderer::SpuRecordGeometries())
		{
			return(FALSE);
		}
	}


	if(sm_GrassTaskHandle)
	{
//		plantsDebugf1("\n GrassRendererSPU: Still waiting for master job to finish.");
		sysTaskManager::Wait(sm_GrassTaskHandle);
		sm_GrassTaskHandle=NULL;
	}
//	plantsDebugf1("\n GrassSPU: Execution ticks: %.2f", float(outSpuParamsMaster.m_DecrementerTicks)/1000000.0f);

	// reset num jobs launched:
	sm_nNumGrassJobsAdded		= 0;
	sm_nNumGrassJobsAddedTotal	= 0;

	// str memory usage stats for BigBuffer:
	SpuUpdateStrStats(&outSpuParamsMaster);

#if __BANK
	// rendering stats:
	extern u32 nLocTriPlantsLOD0Drawn;
	extern u32 nLocTriPlantsLOD1Drawn;
	extern u32 nLocTriPlantsLOD2Drawn;

	nLocTriPlantsLOD0Drawn = outSpuParamsMaster.m_LocTriPlantsLOD0DrawnPerSlot;
	nLocTriPlantsLOD1Drawn = outSpuParamsMaster.m_LocTriPlantsLOD1DrawnPerSlot;
	nLocTriPlantsLOD2Drawn = outSpuParamsMaster.m_LocTriPlantsLOD2DrawnPerSlot;
	outSpuParamsMaster.m_LocTriPlantsLOD0DrawnPerSlot=0;
	outSpuParamsMaster.m_LocTriPlantsLOD1DrawnPerSlot=0;
	outSpuParamsMaster.m_LocTriPlantsLOD2DrawnPerSlot=0;
#endif //__BANK...

	return(TRUE);
}// end of InitialisePerRenderFrame()...


//
//
// str memory usage stats for BigBuffer:
//
bool CGrassRenderer::SpuUpdateStrStats(spuGrassParamsStructMaster *outParams)
{
	FastAssert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	const u32 heapID				= outParams->m_heapID;
#if PLANTSMGR_BIG_HEAP_IN_BLOCKS
	sm_BigHeapAllocatedSize[heapID]	= outParams->m_heapBlockArrayCount;
#else
	sm_BigHeapAllocatedSize[heapID]	= outParams->m_heapSize;
#endif
	sm_BigHeapConsumed[heapID]		= outParams->m_AmountOfBigHeapConsumed;
	sm_BigHeapOverfilled[heapID]	= outParams->m_AmountOfBigHeapOverfilled;

	if(sm_BigHeapConsumed[heapID] > sm_BigHeapBiggestConsumed[heapID])
	{
		sm_BigHeapBiggestConsumed[heapID] = sm_BigHeapConsumed[heapID];
	}

	// reset mem usage stats:
	outParams->m_AmountOfBigHeapConsumed	= 0;
	outParams->m_AmountOfBigHeapOverfilled	= 0;

	return(TRUE);
}// end of SpuUpdateStrStats()...


#else //PSN_PLANTSMGR_SPU_RENDER
//
//
//
//
#if PLANTSMGR_MULTI_RENDER
	#if GRASS_INSTANCING==0
		#error "Multirender requires GRASS_INSTANCING!"
	#endif

static sysCriticalSectionToken	sm_csAddInstLod0Token[NUMBER_OF_RENDER_THREADS];				// AddInstanced access token during multi render
static sysCriticalSectionToken	sm_csAddInstLod1Token[NUMBER_OF_RENDER_THREADS];
static sysCriticalSectionToken	sm_csAddInstLod2Token[NUMBER_OF_RENDER_THREADS];

static CGrassRenderer::CParamsDrawTriPlantsMulti sm_paramsDrawMulti[NUMBER_OF_RENDER_THREADS];

//
//
//
//
bool CGrassRenderer::DrawTriPlantsMulti(const u8 rti, const bool bMainRT, PPTriPlant *triPlants, const u32 numTriPlants, grassModel* plantModelsTab,
										CParamsDrawTriPlantsMulti *pParamsDrawMulti)
{
#if __BANK
	extern bool		gbPlantsFlashLOD0, gbPlantsFlashLOD1, gbPlantsFlashLOD2;
	extern bool		gbPlantsDisableLOD0, gbPlantsDisableLOD1, gbPlantsDisableLOD2;
#endif // __BANK

mthRandom plantsRand;

	const float fLOD0FarDist2	= pParamsDrawMulti->m_LOD0FarDist2;
	const float fLOD1CloseDist2	= pParamsDrawMulti->m_LOD1CloseDist2;
	const float fLOD1FarDist2	= pParamsDrawMulti->m_LOD1FarDist2;
	const float fLOD2CloseDist2	= pParamsDrawMulti->m_LOD2CloseDist2;
	const float fLOD2FarDist2	= pParamsDrawMulti->m_LOD2FarDist2;

	const Vector3	globalCameraPos(pParamsDrawMulti->m_CameraPos);
	const Vector3	globalPlayerPos(pParamsDrawMulti->m_PlayerPos);

	const Vector2	globalWindBending(pParamsDrawMulti->m_WindBending);

	spdTransposedPlaneSet8 cullFrustum = ms_cullingFrustum[rti];

	#if __BANK
		extern bool gbPlantsGeometryCulling;
		dev_bool bGeomCullingTestEnabled	= gbPlantsGeometryCulling;
		extern bool gbPlantsPremultiplyNdotL;
		dev_bool bPremultiplyNdotL			= gbPlantsPremultiplyNdotL;
	#else
		dev_bool bGeomCullingTestEnabled	= TRUE;
		dev_bool bPremultiplyNdotL			= TRUE;
	#endif

	#if CPLANT_PRE_MULTIPLY_LIGHTING
		const Vector3 sunDir		= pParamsDrawMulti->m_SunDir;
		const float naturalAmbient	= pParamsDrawMulti->m_NaturalAmbient;
	#endif

instGrassDataDrawStruct	customInstDraw;
Matrix34				plantMatrix;
Matrix34				skewingMtx;
	 
		customInstDraw.Reset();

		for(u32 i=0; i<numTriPlants; i++) 
		{
			PPTriPlant *pCurrTriPlant = &triPlants[i];

			const float wind_bend_scale = pCurrTriPlant->V1.w;	// wind bending scale
			const float wind_bend_var	= pCurrTriPlant->V2.w;	// wind bending variation

			const bool bApplyGroundSkewing		= (pCurrTriPlant->skewAxisAngle.w.GetBinaryData()!=0); // technically this tests for +0 but not -0
			if(bApplyGroundSkewing)
			{
				const Vec4V unpacked_skewAxisAngle = Float16Vec4Unpack(&pCurrTriPlant->skewAxisAngle);
				skewingMtx.MakeRotateUnitAxis(RCC_VECTOR3(unpacked_skewAxisAngle), unpacked_skewAxisAngle.GetWf());
			}

			const bool bEnableCulling = pCurrTriPlant->flags.IsClear(PROCPLANT_CAMERADONOTCULL);

			float NdotL = 1.0f;
#if CPLANT_WRITE_GRASS_NORMAL || CPLANT_PRE_MULTIPLY_LIGHTING
			// set normal for whole pTriLoc:
			Vector4 groundNormal;
			groundNormal.SetX(float(pCurrTriPlant->loctri_normal[0])/127.0f);
			groundNormal.SetY(float(pCurrTriPlant->loctri_normal[1])/127.0f);
			groundNormal.SetZ(float(pCurrTriPlant->loctri_normal[2])/127.0f);
			groundNormal.SetW(0.0f);
		#if	CPLANT_PRE_MULTIPLY_LIGHTING
			if(bPremultiplyNdotL)
			{
				// For pre-multiplying lighting into the color channel
				Vector4 sunDir_Vec4(sunDir.x, sunDir.y, sunDir.z, 0.0f);
				NdotL = Saturate(groundNormal.Dot(sunDir_Vec4)) + naturalAmbient;
			}
		#endif
		#if CPLANT_WRITE_GRASS_NORMAL
			ms_Shader->SetVar(ms_shdTerrainNormal, groundNormal);
		#endif
#endif	// CPLANT_WRITE_GRASS_NORMAL || CPLANT_PRE_MULTIPLY_LIGHTING

			// want same values for plant positions each time...
			plantsRand.Reset(pCurrTriPlant->seed);

			// intensity support:  final_I = I + VarI*rand01()
			const s16 intensityVar = pCurrTriPlant->intensity_var;
			u16 intensity = pCurrTriPlant->intensity + u16(float(intensityVar)*plantsRand.GetRanged(0.0f, 1.0f));
			intensity = MIN(static_cast<u16>(intensity * NdotL), 255);

			Color32 plantColour32;
			plantColour32.SetAlpha(	pCurrTriPlant->color.GetAlpha());
			#define CALC_COLOR_INTENSITY(COLOR, I)			u8((u16(COLOR)*I) >> 8)
				plantColour32.SetRed(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetRed(),		intensity));
				plantColour32.SetGreen(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetGreen(),	intensity));
				plantColour32.SetBlue(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetBlue(),	intensity));
			#undef CALC_COLOR_INTENSITY

			grcTexture *pPlantTextureLOD0 = pCurrTriPlant->texture_ptr;
			Assert(pPlantTextureLOD0);
			grcTexture *pPlantTextureLOD1 = pCurrTriPlant->textureLOD1_ptr;
			Assert(pPlantTextureLOD1);

			grmGeometry *pGeometryLOD0		= plantModelsTab[pCurrTriPlant->model_id].m_pGeometry;
			const Vector4 boundSphereLOD0	= plantModelsTab[pCurrTriPlant->model_id].m_BoundingSphere; 
			grmGeometry *pGeometryLOD1		= plantModelsTab[pCurrTriPlant->model_id+CPLANT_SLOT_NUM_MODELS].m_pGeometry;
			const Vector4 boundSphereLOD1	= plantModelsTab[pCurrTriPlant->model_id+CPLANT_SLOT_NUM_MODELS].m_BoundingSphere; 
			const Vector2 GeometryDimLOD2	= plantModelsTab[pCurrTriPlant->model_id+CPLANT_SLOT_NUM_MODELS].m_dimensionLOD2;
			const Vector4 boundSphereLOD2(0,0,0,1.0f);

			// set global micro-movement params: [ scaleH | scaleV | freqH | freqV ]
			const Vec4V unpacked_um_params			= Float16Vec4Unpack(&pCurrTriPlant->um_param);
			// set global collision radius scale:
			const Vec2V unpacked_coll_params		= Float16Vec2Unpack(&pCurrTriPlant->coll_params);

			// LOD2 billboard dimensions:
			Vector4 dimLOD2;
			dimLOD2.x = GeometryDimLOD2.x;
			dimLOD2.y = GeometryDimLOD2.y;
			dimLOD2.z = pCurrTriPlant->flags.IsSet(PROCPLANT_LOD2FARFADE)? 1.0f : 0.0f;
			dimLOD2.w = 0.0f;
			u8 ambientScale = pCurrTriPlant->ambient_scl;

			Vector3 *v1 = (Vector3*)&pCurrTriPlant->V1;
			Vector3 *v2 = (Vector3*)&pCurrTriPlant->V2;
			Vector3 *v3 = (Vector3*)&pCurrTriPlant->V3;

			Color32 *c1 = &pCurrTriPlant->groundColorV1;
			Color32 *c2 = &pCurrTriPlant->groundColorV2;
			Color32 *c3 = &pCurrTriPlant->groundColorV3;

			// local per-vertex data: ground color(XYZ) + scaleZ/XYZ:
			Vector4 groundColorV1, groundColorV2, groundColorV3;
			Vector3 groundScaleV1, groundScaleV2, groundScaleV3;
			Vector4 groundColorf;
			Vector3 groundScalef;

			groundColorV1.x = c1->GetRedf();
			groundColorV1.y = c1->GetGreenf();
			groundColorV1.z = c1->GetBluef();
			groundColorV1.w = 1.0f;
			groundScaleV1.x = CPlantLocTri::pv8UnpackAndMapScaleXYZ(c1->GetAlpha());
			groundScaleV1.y = CPlantLocTri::pv8UnpackAndMapScaleZ(c1->GetAlpha());
			groundScaleV1.z = 0.0f;

			groundColorV2.x = c2->GetRedf();
			groundColorV2.y = c2->GetGreenf();
			groundColorV2.z = c2->GetBluef();
			groundColorV2.w = 1.0f;
			groundScaleV2.x = CPlantLocTri::pv8UnpackAndMapScaleXYZ(c2->GetAlpha());
			groundScaleV2.y = CPlantLocTri::pv8UnpackAndMapScaleZ(c2->GetAlpha());
			groundScaleV2.z = 0.0f;

			groundColorV3.x = c3->GetRedf();
			groundColorV3.y = c3->GetGreenf();
			groundColorV3.z = c3->GetBluef();
			groundColorV3.w = 1.0f;
			groundScaleV3.x = CPlantLocTri::pv8UnpackAndMapScaleXYZ(c3->GetAlpha());
			groundScaleV3.y = CPlantLocTri::pv8UnpackAndMapScaleZ(c3->GetAlpha());
			groundScaleV3.z = 0.0f;

			// unpack ground scale ranges:
			const Vec2V unpacked_range_params	= Float16Vec2Unpack(&pCurrTriPlant->scaleRangeXYZ_Z);
			const float fGroundScaleRangeXYZ	= unpacked_range_params.GetXf();
			const float fGroundScaleRangeZ		= unpacked_range_params.GetYf();

			// unpack ScaleXY, ScaleZ, ScaleVarXY, ScaleVarZ:
			const Vec4V unpacked_scale_params	= Float16Vec4Unpack(&pCurrTriPlant->packedScale);
			const Vector4 ScaleV4				= RCC_VECTOR4(unpacked_scale_params);

//		const bool bApplyGroundSkewingLod0	= bApplyGroundSkewing && pCurrTriPlant->flags.IsClear(PROCPLANT_NOGROUNDSKEW_LOD0);
//		const bool bApplyGroundSkewingLod1	= bApplyGroundSkewing && pCurrTriPlant->flags.IsClear(PROCPLANT_NOGROUNDSKEW_LOD1);
//		const bool bApplyGroundSkewingLod2	= bApplyGroundSkewing && pCurrTriPlant->flags.IsClear(PROCPLANT_NOGROUNDSKEW_LOD2);

		// want same values for plant positions each time...
		plantsRand.Reset(pCurrTriPlant->seed);

		customInstDraw.Reset();

		//
		// render a single triPlant full of the desired model
	#if PLANTS_USE_LOD_SETTINGS
		const u32 count = (u32)(float(pCurrTriPlant->num_plants) * GetDensityMultiplier());
	#else
		const u32 count = pCurrTriPlant->num_plants;
	#endif
		for(u32 j=0; j<count; j++)
		{
			// MainRT: only check for full LOD buckets bins and flush:
			if(bMainRT)
			{
				// MainRT: only check for full LOD buckets bins and flush:
				if(sm_csAddInstLod0Token[g_RenderThreadIndex].TryLock())
				{
					ms_LOD0Bucket.FlushAlmostFull(g_RenderThreadIndex);
					sm_csAddInstLod0Token[g_RenderThreadIndex].Unlock();
				}

				if(sm_csAddInstLod1Token[g_RenderThreadIndex].TryLock())
				{
					ms_LOD1Bucket.FlushAlmostFull(g_RenderThreadIndex);
					sm_csAddInstLod1Token[g_RenderThreadIndex].Unlock();
				}

				if(sm_csAddInstLod2Token[g_RenderThreadIndex].TryLock())
				{
					ms_LOD2Bucket.FlushAlmostFull(g_RenderThreadIndex);
					sm_csAddInstLod2Token[g_RenderThreadIndex].Unlock();
				}
			}

			const float s = plantsRand.GetRanged(0.0f, 1.0f);
			const float t = plantsRand.GetRanged(0.0f, 1.0f);

			Vector3 posPlant;
			PPTriPlant::GenPointInTriangle(&posPlant, v1, v2, v3, s, t);

			// calculate LOD0 distance:
		#if CPLANT_USE_COLLISION_2D_DIST
			Vector3 LodDistV3 = posPlant - globalCameraPos;
			Vector2 LodDistV(LodDistV3.x, LodDistV3.y);
		#else
			Vector3 LodDistV = posPlant - globalCameraPos;
		#endif
			const float fLodDist2 = LodDistV.Mag2();

			// interpolate ground color(XYZ):
			PPTriPlant::GenPointInTriangle(&groundColorf, &groundColorV1, &groundColorV2, &groundColorV3, s, t);

			// interpolate ground scaleXYZ/Z weights:
			PPTriPlant::GenPointInTriangle(&groundScalef, &groundScaleV1, &groundScaleV2, &groundScaleV3, s, t);
			const float groundScaleXYZ	= groundScalef.x;
			const float groundScaleZ	= groundScalef.y;

			// ---- apply various amounts of scaling to the matrix -----
			// calculate an x/y scaling value and apply to matrix
			float scaleXY	= ScaleV4.x;
			float scaleZ	= ScaleV4.y;

			// scale variation:
			const float scaleRand01 = plantsRand.GetRanged(0.0f, 1.0f);
			const float	scvariationXY	= ScaleV4.z;
			scaleXY		+= scvariationXY*scaleRand01;
			const float	scvariationZ	= ScaleV4.w;
			scaleZ		+= scvariationZ*scaleRand01;

			// apply ground scale variation:
			const float _grndScaleXYZ = (1.0f - fGroundScaleRangeXYZ)*groundScaleXYZ + fGroundScaleRangeXYZ;
			if(_grndScaleXYZ > 0.0f)
			{
				scaleXY *= _grndScaleXYZ;
				scaleZ  *= _grndScaleXYZ;
			}
				
			const float _grndScaleZ = (1.0f - fGroundScaleRangeZ)*groundScaleZ + fGroundScaleRangeZ; 
			if(_grndScaleZ > 0.0f)
			{
				scaleZ  *= _grndScaleZ;
			}

			//// ----- end of scaling stuff ------


			// choose bigger scale for bound frustum check:
			const float boundRadiusScale = (scaleXY > scaleZ)? (scaleXY) : (scaleZ);

			Vector4 sphereV4;
			sphereV4.Set(boundSphereLOD0);
			sphereV4.AddVector3XYZ(posPlant);
			sphereV4.w *= boundRadiusScale;
			spdSphere sphere(VECTOR4_TO_VEC4V(sphereV4));

			bool isCulled = bGeomCullingTestEnabled && bEnableCulling && (!cullFrustum.IntersectsOrContainsSphere(sphere));
			// skip current geometry if not visible:
			if(isCulled)
			{
				plantsRand.GetInt();
				plantsRand.GetInt();
				plantsRand.GetInt();
				plantsRand.GetInt();
				if(intensityVar)
					plantsRand.GetInt();

				continue;
			}

			plantMatrix.MakeRotateZ(plantsRand.GetRanged(0.0f, 1.0f)*2.0f*PI);// overwrites a,b,c
			plantMatrix.MakeTranslate(posPlant);									// overwrites d
			plantMatrix.Scale(scaleXY, scaleXY, scaleZ);	// scale in XY+Z

			if(bApplyGroundSkewing)	// TODO: implement PROCPLANT_NOGROUNDSKEW_LOD0/1/2 flags (not important on NG just yet)
			{
				plantMatrix.Dot3x3(skewingMtx);
			}

			// ---- muck about with the matrix to make atomic look blown by the wind --------
			// use here wind_bend_scale & wind_bend_var
			//	final_bend = [ 1.0 + (bend_var*rand(-1,1)) ] * [ global_wind * bend_scale ]
			const float variationBend = wind_bend_var;
			const float bending =	(1.0f + (variationBend*plantsRand.GetRanged(0.0f, 1.0f))) *
									(wind_bend_scale);
			plantMatrix.c.x = bending * globalWindBending.x * plantsRand.GetRanged(-1.0f, 1.0f);
			plantMatrix.c.y = bending * globalWindBending.y * plantsRand.GetRanged(-1.0f, 1.0f);
			// ----- end of wind stuff -----

			// color variation:
			if(intensityVar)
			{
				// intensity support:  final_I = I + VarI*rand01()
				u16 intensity = pCurrTriPlant->intensity + u16(float(intensityVar)*plantsRand.GetRanged(0.0f, 1.0f));
				intensity = MIN(static_cast<u16>(intensity * NdotL), 255);

				Color32 col32(plantColour32.GetColor());
			#define CALC_COLOR_INTENSITY(COLOR, I)			u8((u16(COLOR)*I) >> 8)
				col32.SetRed(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetRed(),		intensity));
				col32.SetGreen(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetGreen(),	intensity));
				col32.SetBlue(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetBlue(),	intensity));
			#undef CALC_COLOR_INTENSITY

				plantColour32 = col32;
			} // if(intensityVar)...

			customInstDraw.m_WorldMatrix.FromMatrix34(plantMatrix);
			customInstDraw.m_PlantColor32 = plantColour32;
			customInstDraw.m_GroundColorAmbient32.SetFromRGBA( RCC_VEC4V(groundColorf) );
			customInstDraw.m_GroundColorAmbient32.SetAlpha(ambientScale);

			// Do we render a LOD0 version ?
			if(fLodDist2 < fLOD0FarDist2)
			{
			#if __BANK
				if(pCurrTriPlant->flags.IsSet(PROCPLANT_LOD0) && !gbPlantsDisableLOD0 && (!gbPlantsFlashLOD0 || (GRCDEVICE.GetFrameCounter()&0x01)))
			#else
				if(pCurrTriPlant->flags.IsSet(PROCPLANT_LOD0))
			#endif
				{
					if(bMainRT)
					{
						sm_csAddInstLod0Token[rti].Lock();
							ms_LOD0Bucket.AddInstance(rti, true, pGeometryLOD0, pPlantTextureLOD0, &customInstDraw, unpacked_um_params, unpacked_coll_params);
						sm_csAddInstLod0Token[rti].Unlock();
						#if __BANK
							// rendering stats:
							extern u32 *pLocTriPlantsLOD0DrawnPerSlot;
							(*pLocTriPlantsLOD0DrawnPerSlot)++;
						#endif //__BANK
					}
					else
					{
						sm_csAddInstLod0Token[rti].Lock();
							bool bAdded = ms_LOD0Bucket.AddInstance(rti, false, pGeometryLOD0, pPlantTextureLOD0, &customInstDraw, unpacked_um_params, unpacked_coll_params);
						sm_csAddInstLod0Token[rti].Unlock();
						
						if(!bAdded)
						{
							Printf("\n LOD0 not added!");
						}
						#if __BANK
							// rendering stats:
							extern u32 *pLocTriPlantsLOD0DrawnPerSlot;
							(*pLocTriPlantsLOD0DrawnPerSlot)++;
						#endif //__BANK
					}
				}
			}

			// Do we render a LOD1 version ?
			if((fLodDist2 > fLOD1CloseDist2) && (fLodDist2 < fLOD1FarDist2))
			{
			#if __BANK
				if(pCurrTriPlant->flags.IsSet(PROCPLANT_LOD1) && !gbPlantsDisableLOD1 && (!gbPlantsFlashLOD1 || (GRCDEVICE.GetFrameCounter()&0x01)))
			#else
				if(pCurrTriPlant->flags.IsSet(PROCPLANT_LOD1))
			#endif
				{
					if(bMainRT)
					{
						sm_csAddInstLod1Token[rti].Lock();
							ms_LOD1Bucket.AddInstance(rti, true, pGeometryLOD1, pPlantTextureLOD1, &customInstDraw, unpacked_um_params, unpacked_coll_params);
						sm_csAddInstLod1Token[rti].Unlock();
						#if __BANK
							// rendering stats:
							extern u32 *pLocTriPlantsLOD1DrawnPerSlot;
							(*pLocTriPlantsLOD1DrawnPerSlot)++;
						#endif //__BANK
					}
					else
					{
						sm_csAddInstLod1Token[rti].Lock();
							bool bAdded = ms_LOD1Bucket.AddInstance(rti, false, pGeometryLOD1, pPlantTextureLOD1, &customInstDraw, unpacked_um_params, unpacked_coll_params);
						sm_csAddInstLod1Token[rti].Unlock();
					
						if(!bAdded)
						{
							Printf("\n LOD1 not added!");
						}
						#if __BANK
							// rendering stats:
							extern u32 *pLocTriPlantsLOD0DrawnPerSlot;
							(*pLocTriPlantsLOD0DrawnPerSlot)++;
						#endif //__BANK
					}
				}
			}

			// Do we render a LOD2 version ?
			if((fLodDist2 > fLOD2CloseDist2) && (fLodDist2 < fLOD2FarDist2))
			{
			#if __BANK
				if(pCurrTriPlant->flags.IsSet(PROCPLANT_LOD2) && !gbPlantsDisableLOD2 && (!gbPlantsFlashLOD2 || (GRCDEVICE.GetFrameCounter()&0x01)))
			#else
				if(pCurrTriPlant->flags.IsSet(PROCPLANT_LOD2))
			#endif
				{
					if(bMainRT)
					{
						sm_csAddInstLod2Token[rti].Lock();
							ms_LOD2Bucket.AddInstance(rti, true, NULL, pPlantTextureLOD1, &customInstDraw, VECTOR4_TO_VEC4V(dimLOD2), unpacked_coll_params);
						sm_csAddInstLod2Token[rti].Unlock();
						#if __BANK
							// rendering stats:
							extern u32 *pLocTriPlantsLOD2DrawnPerSlot;
							(*pLocTriPlantsLOD2DrawnPerSlot)++;
						#endif //__BANK
					}
					else
					{
						sm_csAddInstLod2Token[rti].Lock();
							bool bAdded = ms_LOD2Bucket.AddInstance(rti, false, NULL, pPlantTextureLOD1, &customInstDraw, VECTOR4_TO_VEC4V(dimLOD2), unpacked_coll_params);
						sm_csAddInstLod2Token[rti].Unlock();
						if(!bAdded)
						{
							Printf("\n LOD2 not added!");
						}
						#if __BANK
							// rendering stats:
							extern u32 *pLocTriPlantsLOD2DrawnPerSlot;
							(*pLocTriPlantsLOD2DrawnPerSlot)++;
						#endif //__BANK
					}
				}
			}
		}// for(u32 j=0; j<count; j++)...

		}// for(u32 i=0; i<numTriPlants; i++)...

		return(TRUE);
}// end of DrawTriPlantsMulti()...

static sysTaskHandle s_PlantRenderTaskHandle[NUMBER_OF_RENDER_THREADS][PLANTSMGR_MULTI_RENDER_TASKCOUNT] = {0};

//
//
//
//
static void RenderPlantSubTask(sysTaskParameters &params)
{
	const s32	taskID		= params.UserData[0].asInt;
	const u8	rti			= (u8)params.UserData[1].asUInt;	// render thread index

#if ENABLE_PIX_TAGGING
	const char* pixTagName[] = {	"PlantsMgrRenderTask0", "PlantsMgrRenderTask1", "PlantsMgrRenderTask2", "PlantsMgrRenderTask3",
									"PlantsMgrRenderTask4", "PlantsMgrRenderTask5", "PlantsMgrRenderTask6", "PlantsMgrRenderTask7"	};
	PIXBegin(1, pixTagName[taskID]);
#else
	PIXBegin(1, "PlantsRenderTask");
#endif

	PPTriPlant *triPlants										= (PPTriPlant*)params.UserData[2].asPtr;
	u32 numTriPlants											= params.UserData[3].asInt;
	grassModel *plantModelsTab									= (grassModel*)params.UserData[4].asPtr;
	CGrassRenderer::CParamsDrawTriPlantsMulti *pParamsDrawMulti	= (CGrassRenderer::CParamsDrawTriPlantsMulti*)params.UserData[5].asPtr;

	CGrassRenderer::DrawTriPlantsMulti(rti, false, triPlants, numTriPlants, plantModelsTab, pParamsDrawMulti);

	PIXEnd();
}

//
//
//
//
bool CGrassRenderer::DrawTriPlants(PPTriPlant *triPlants, const u32 numTriPlants, grassModel* plantModelsTab)
{
	Assert(triPlants);
	Assert(plantModelsTab);

	const Vector3	globalCameraPos(GetGlobalCameraPos());
	const Vector3	globalPlayerPos(GetGlobalPlayerPos());


		ms_Shader->SetVar(ms_shdCameraPosID, globalCameraPos);
		ms_Shader->SetVar(ms_shdPlayerPosID, globalPlayerPos);

		for(u32 v=0; v<NUM_COL_VEH; v++)
		{
		bool bVehCollisionEnabled=FALSE;
		Vector4 vecVehCollisionB, vecVehCollisionM, vecVehCollisionR;
			GetGlobalVehCollisionParams(v, &bVehCollisionEnabled, &vecVehCollisionB, &vecVehCollisionM, &vecVehCollisionR);

			grcEffect::SetGlobalVar(ms_shdVehCollisionEnabledID[v],	bVehCollisionEnabled);
			ms_Shader->SetVar(ms_shdVehCollisionBID[v],				vecVehCollisionB);
			ms_Shader->SetVar(ms_shdVehCollisionMID[v],				vecVehCollisionM);
			ms_Shader->SetVar(ms_shdVehCollisionRID[v],				vecVehCollisionR);
		}

		Vector4 fakeNormal;
		fakeNormal.SetVector3ClearW(GetFakeGrassNormal());
		fakeNormal.SetW(1.0f);
		ms_Shader->SetVar(ms_shdFakedGrassNormal, fakeNormal);




#if __BANK || PLANTS_USE_LOD_SETTINGS
		extern float	gbPlantsLOD0AlphaCloseDist, gbPlantsLOD0AlphaFarDist, gbPlantsLOD0FarDist;
		extern float	gbPlantsLOD1CloseDist, gbPlantsLOD1FarDist, gbPlantsLOD1Alpha0CloseDist, gbPlantsLOD1Alpha0FarDist, gbPlantsLOD1Alpha1CloseDist, gbPlantsLOD1Alpha1FarDist;
		extern float	gbPlantsLOD2CloseDist, gbPlantsLOD2FarDist, gbPlantsLOD2Alpha0CloseDist, gbPlantsLOD2Alpha0FarDist, gbPlantsLOD2Alpha1CloseDist, gbPlantsLOD2Alpha1FarDist;
		extern float	gbPlantsLOD2Alpha1CloseDistFar, gbPlantsLOD2Alpha1FarDistFar;

	#if PLANTS_USE_LOD_SETTINGS
		float grassDistanceMultiplier = GetDistanceMultiplier();
	#else
		float grassDistanceMultiplier = 1.0f;
	#endif

		const float	globalLOD0AlphaCloseDist	= grassDistanceMultiplier*gbPlantsLOD0AlphaCloseDist;
		float		globalLOD0AlphaFarDist		= grassDistanceMultiplier*gbPlantsLOD0AlphaFarDist;
		const float	globalLOD0FarDist			= grassDistanceMultiplier*gbPlantsLOD0FarDist;
		const float	globalLOD1CloseDist			= grassDistanceMultiplier*gbPlantsLOD1CloseDist;
		const float	globalLOD1FarDist			= grassDistanceMultiplier*gbPlantsLOD1FarDist;
		const float	globalLOD1Alpha0CloseDist	= grassDistanceMultiplier*gbPlantsLOD1Alpha0CloseDist;
		const float	globalLOD1Alpha0FarDist		= grassDistanceMultiplier*gbPlantsLOD1Alpha0FarDist;
		const float	globalLOD1Alpha1CloseDist	= grassDistanceMultiplier*gbPlantsLOD1Alpha1CloseDist;
		const float	globalLOD1Alpha1FarDist		= grassDistanceMultiplier*gbPlantsLOD1Alpha1FarDist;
		const float	globalLOD2CloseDist			= grassDistanceMultiplier*gbPlantsLOD2CloseDist;
		const float	globalLOD2FarDist			= grassDistanceMultiplier*gbPlantsLOD2FarDist;
		const float	globalLOD2Alpha0CloseDist	= grassDistanceMultiplier*gbPlantsLOD2Alpha0CloseDist;
		const float	globalLOD2Alpha0FarDist		= grassDistanceMultiplier*gbPlantsLOD2Alpha0FarDist;
		const float	globalLOD2Alpha1CloseDist	= grassDistanceMultiplier*gbPlantsLOD2Alpha1CloseDist;
		const float	globalLOD2Alpha1FarDist		= grassDistanceMultiplier*gbPlantsLOD2Alpha1FarDist;
		const float	globalLOD2Alpha1CloseDistFar= grassDistanceMultiplier*gbPlantsLOD2Alpha1CloseDistFar;
		const float	globalLOD2Alpha1FarDistFar	= grassDistanceMultiplier*gbPlantsLOD2Alpha1FarDistFar;
#else
		const float	globalLOD0AlphaCloseDist	= CPLANT_LOD0_ALPHA_CLOSE_DIST;
		float		globalLOD0AlphaFarDist		= CPLANT_LOD0_ALPHA_FAR_DIST;
		const float	globalLOD0FarDist			= CPLANT_LOD0_FAR_DIST;
		const float	globalLOD1CloseDist			= CPLANT_LOD1_CLOSE_DIST;
		const float	globalLOD1FarDist			= CPLANT_LOD1_FAR_DIST;
		const float	globalLOD1Alpha0CloseDist	= CPLANT_LOD1_ALPHA0_CLOSE_DIST;
		const float	globalLOD1Alpha0FarDist		= CPLANT_LOD1_ALPHA0_FAR_DIST;
		const float	globalLOD1Alpha1CloseDist	= CPLANT_LOD1_ALPHA1_CLOSE_DIST;
		const float	globalLOD1Alpha1FarDist		= CPLANT_LOD1_ALPHA1_FAR_DIST;
		const float	globalLOD2CloseDist			= CPLANT_LOD2_CLOSE_DIST;
		const float	globalLOD2FarDist			= CPLANT_LOD2_FAR_DIST;
		const float	globalLOD2Alpha0CloseDist	= CPLANT_LOD2_ALPHA0_CLOSE_DIST;
		const float	globalLOD2Alpha0FarDist		= CPLANT_LOD2_ALPHA0_FAR_DIST;
		const float	globalLOD2Alpha1CloseDist	= CPLANT_LOD2_ALPHA1_CLOSE_DIST;
		const float	globalLOD2Alpha1FarDist		= CPLANT_LOD2_ALPHA1_FAR_DIST;
		const float	globalLOD2Alpha1CloseDistFar= CPLANT_LOD2_ALPHA1_CLOSE_DIST_FAR;
		const float	globalLOD2Alpha1FarDistFar	= CPLANT_LOD2_ALPHA1_FAR_DIST_FAR;
#endif
		float globalLOD0FarDist2	= globalLOD0FarDist*globalLOD0FarDist;
		float globalLOD1CloseDist2	= globalLOD1CloseDist*globalLOD1CloseDist;
		float globalLOD1FarDist2	= globalLOD1FarDist*globalLOD1FarDist;
		float globalLOD2CloseDist2	= globalLOD2CloseDist*globalLOD2CloseDist;
		float globalLOD2FarDist2	= globalLOD2FarDist*globalLOD2FarDist;

		if(CGrassRenderer::GetGlobalCameraFppEnabled() || CGrassRenderer::GetGlobalForceHDGrassGeometry())
		{	// special case: camera is in FPP mode - force LOD0 to be visible everywhere:
			globalLOD0AlphaFarDist		= globalLOD2Alpha1FarDistFar;

			globalLOD0FarDist2			= globalLOD2FarDist2;
			globalLOD1CloseDist2		=
			globalLOD1FarDist2			=
			globalLOD2CloseDist2		=
			globalLOD2FarDist2			= 999999.0f;
		}


		// LOD0 alpha, umTimer:
		{
			Vector4 vecAlphaFade4;	// [XY=alphaFade01 | Z=umTimer | ?]
#if 1
			// square distance approach:
			const float closeDist	=	globalLOD0AlphaCloseDist;
			const float farDist		=	globalLOD0AlphaFarDist; 
			const float alphaDenom	= 1.0f / (farDist*farDist - closeDist*closeDist);
			vecAlphaFade4.x = (farDist*farDist) * alphaDenom; // x = (f2)  / (f2-c2)
			vecAlphaFade4.y = (-1.0f) * alphaDenom;		 // y = (-1.0)/ (f2-c2) 
#else
			// linear distance approach:
			const float alphaDenom = 1.0f / (m_farDist - m_closeDist);
			vecAlphaFade4.x = (m_farDist) * alphaDenom; // x = (f2)  / (f2-c2)
			vecAlphaFade4.y = (-1.0f) * alphaDenom;		 // y = (-1.0)/ (f2-c2) 
#endif
			vecAlphaFade4.z = (float)(2.0*PI*(double(fwTimer::GetTimeInMilliseconds())/1000.0));
			vecAlphaFade4.w = 0.0f;
			ms_Shader->SetVar(ms_shdFadeAlphaDistUmTimerID, vecAlphaFade4);
		}

		// LOD1 alpha:
		{
			Vector4 vecAlphaFade4;	// [XY=alpha0Fade01 | ZW=alpha1Fade01]
			
			const float closeDist0	= globalLOD1Alpha0CloseDist;
			const float farDist0	= globalLOD1Alpha0FarDist;
			const float alphaDenom0 = 1.0f / (farDist0*farDist0 - closeDist0*closeDist0);
			vecAlphaFade4.x = (farDist0*farDist0) * alphaDenom0;	// x = (f2)  / (f2-c2)
			vecAlphaFade4.y = (-1.0f) * alphaDenom0;				// y = (-1.0)/ (f2-c2) 
			
			const float closeDist1	=	globalLOD1Alpha1CloseDist;
			const float farDist1	=	globalLOD1Alpha1FarDist;
			const float alphaDenom1 = 1.0f / (farDist1*farDist1 - closeDist1*closeDist1);
			vecAlphaFade4.z = (farDist1*farDist1) * alphaDenom1;	// x = (f2)  / (f2-c2)
			vecAlphaFade4.w = (-1.0f) * alphaDenom1;			// y = (-1.0)/ (f2-c2) 
			
			ms_Shader->SetVar(ms_shdFadeAlphaLOD1DistID, vecAlphaFade4);
		}

		// LOD2 alpha:
		{
			Vector4 vecAlphaFade4;	// [XY=alpha0Fade01 | ZW=alpha1Fade01]

			const float closeDist0	= globalLOD2Alpha0CloseDist;
			const float farDist0	= globalLOD2Alpha0FarDist;
			const float alphaDenom0 = 1.0f / (farDist0*farDist0 - closeDist0*closeDist0);
			vecAlphaFade4.x = (farDist0*farDist0) * alphaDenom0; // x = (f2)  / (f2-c2)
			vecAlphaFade4.y = (-1.0f) * alphaDenom0;		 // y = (-1.0)/ (f2-c2) 

			const float closeDist1	=	globalLOD2Alpha1CloseDist;
			const float farDist1	=	globalLOD2Alpha1FarDist;
			const float alphaDenom1 = 1.0f / (farDist1*farDist1 - closeDist1*closeDist1);
			vecAlphaFade4.z = (farDist1*farDist1) * alphaDenom1; // x = (f2)  / (f2-c2)
			vecAlphaFade4.w = (-1.0f) * alphaDenom1;		 // y = (-1.0)/ (f2-c2) 

			ms_Shader->SetVar(ms_shdFadeAlphaLOD2DistID, vecAlphaFade4);

			Vector4 vecAlphaFadeFar4;	// [XY=alpha1FadeFar01 | 0 | 0]

			const float closeDistFar0	= globalLOD2Alpha1CloseDistFar;
			const float farDistFar0		= globalLOD2Alpha1FarDistFar;
			const float alphaDenomFar0	= 1.0f / (farDistFar0*farDistFar0 - closeDistFar0*closeDistFar0);
			vecAlphaFadeFar4.x = (farDistFar0*farDistFar0) * alphaDenomFar0;	// x = (f2)  / (f2-c2)
			vecAlphaFadeFar4.y = (-1.0f) * alphaDenomFar0;						// y = (-1.0)/ (f2-c2) 
			vecAlphaFadeFar4.z = 0.0f;
			vecAlphaFadeFar4.w = 0.0f;

			ms_Shader->SetVar(ms_shdFadeAlphaLOD2DistFarID, vecAlphaFadeFar4);
		}
		
		#if DEVICE_MSAA
			if (!GRCDEVICE.GetMSAA())
				ms_Shader->SetVar(ms_shdAlphaToCoverageScale, 1.0f);
			//else: leave artist-tuned value
		#endif


		CParamsDrawTriPlantsMulti *pDrawParams = &sm_paramsDrawMulti[g_RenderThreadIndex];
		pDrawParams->m_CameraPos		= GetGlobalCameraPos();
		pDrawParams->m_PlayerPos		= GetGlobalPlayerPos();
		pDrawParams->m_SunDir			= -Lights::GetRenderDirectionalLight().GetDirection();
		pDrawParams->m_NaturalAmbient	= Lights::m_lightingGroup.GetNaturalAmbientBase().w;
		Vector2	windBending;
		GetGlobalWindBending(windBending);
		pDrawParams->m_WindBending		= windBending;

		pDrawParams->m_LOD0FarDist2		= globalLOD0FarDist2;
		pDrawParams->m_LOD1CloseDist2	= globalLOD1CloseDist2;
		pDrawParams->m_LOD1FarDist2		= globalLOD1FarDist2;
		pDrawParams->m_LOD2CloseDist2	= globalLOD2CloseDist2;
		pDrawParams->m_LOD2FarDist2		= globalLOD2FarDist2;;

static dev_float subAmountPerc = 1.00f;	// 100% work to subtask
		s32 numTriPlantsSub		= s32(numTriPlants * subAmountPerc);
		s32 numTriPlantsMain	= numTriPlants - numTriPlantsSub;

		if(numTriPlantsSub > 0)	// any tris to share
		{
			sysTaskParameters params;

			const s32 numTriPerTask = numTriPlantsSub / PLANTSMGR_MULTI_RENDER_TASKCOUNT;

			PPTriPlant *startTri = &triPlants[numTriPlantsMain];
			s32 numTrisLeft = numTriPlantsSub;

			for(u32 t=0; t<PLANTSMGR_MULTI_RENDER_TASKCOUNT; t++)
			{
				Assertf(s_PlantRenderTaskHandle[g_RenderThreadIndex][t]==0, "PlantMgr: Render task %d already running!", t);

				params.UserData[0].asInt	= t;	// taskID
				params.UserData[1].asUInt	= g_RenderThreadIndex;
				params.UserData[2].asPtr	= startTri;

				if(t!=(PLANTSMGR_MULTI_RENDER_TASKCOUNT-1))
				{
					params.UserData[3].asInt	= numTriPerTask;
					startTri += numTriPerTask;
					numTrisLeft -= numTriPerTask;
				}
				else
				{
					Assert(numTrisLeft > 0);
					params.UserData[3].asInt	= numTrisLeft;
				}
				params.UserData[4].asPtr	= plantModelsTab;
				params.UserData[5].asPtr	= pDrawParams;
				s_PlantRenderTaskHandle[g_RenderThreadIndex][t] = rage::sysTaskManager::Create(TASK_INTERFACE(RenderPlantSubTask), params);
			}
		}

		// main thread:
		DrawTriPlantsMulti(g_RenderThreadIndex, /*this is mainRT*/true, &triPlants[0], numTriPlantsMain, plantModelsTab, pDrawParams);


		for(u32 t=0; t<PLANTSMGR_MULTI_RENDER_TASKCOUNT; t++)
		{
			if(s_PlantRenderTaskHandle[g_RenderThreadIndex][t])
			{
				do
				{
					// MainRT: only check for full LOD buckets bins and flush:
					if(sm_csAddInstLod0Token[g_RenderThreadIndex].TryLock())
					{
						ms_LOD0Bucket.FlushAlmostFull(g_RenderThreadIndex);
						sm_csAddInstLod0Token[g_RenderThreadIndex].Unlock();
					}

					if(sm_csAddInstLod1Token[g_RenderThreadIndex].TryLock())
					{
						ms_LOD1Bucket.FlushAlmostFull(g_RenderThreadIndex);
						sm_csAddInstLod1Token[g_RenderThreadIndex].Unlock();
					}

					if(sm_csAddInstLod2Token[g_RenderThreadIndex].TryLock())
					{
						ms_LOD2Bucket.FlushAlmostFull(g_RenderThreadIndex);
						sm_csAddInstLod2Token[g_RenderThreadIndex].Unlock();
					}
				}
				while( sysTaskManager::Poll(s_PlantRenderTaskHandle[g_RenderThreadIndex][t]) == false );
				
				//sysTaskManager::Wait(s_PlantRenderTaskHandle[g_RenderThreadIndex][t]);
				s_PlantRenderTaskHandle[g_RenderThreadIndex][t] = 0;
			}
		}
		
		return(TRUE);
}// end of CPPTriPlantBuffer::DrawTriPlants()...


#else //PLANTSMGR_MULTI_RENDER

#define NEW_STORE_OPT		(1)

bool CGrassRenderer::DrawTriPlants(PPTriPlant *triPlants, const u32 numTriPlants, grassModel* plantModelsTab)
{
	Assert(triPlants);
	Assert(plantModelsTab);

	const Vector3	globalCameraPos(GetGlobalCameraPos());
	const Vector3	globalPlayerPos(GetGlobalPlayerPos());

	Vector2			globalWindBending;
	GetGlobalWindBending(globalWindBending);

	spdTransposedPlaneSet8 cullFrustum = ms_cullingFrustum[g_RenderThreadIndex];

	#if CPLANT_DYNAMIC_CULL_SPHERES
		const u32 id = gPlantMgr.GetRenderRTBufferID();
		const CPlantMgr::DynCullSphereArray &cullSpheres = gPlantMgr.GetCullSphereArray(id);
	#endif

		ms_Shader->SetVar(ms_shdCameraPosID, globalCameraPos);
		ms_Shader->SetVar(ms_shdPlayerPosID, globalPlayerPos);

		for(u32 v=0; v<NUM_COL_VEH; v++)
		{
		bool bVehCollisionEnabled=FALSE;
		Vector4 vecVehCollisionB, vecVehCollisionM, vecVehCollisionR;
			GetGlobalVehCollisionParams(v, &bVehCollisionEnabled, &vecVehCollisionB, &vecVehCollisionM, &vecVehCollisionR);

			grcEffect::SetGlobalVar(ms_shdVehCollisionEnabledID[v],	bVehCollisionEnabled);
			ms_Shader->SetVar(ms_shdVehCollisionBID[v],				vecVehCollisionB);
			ms_Shader->SetVar(ms_shdVehCollisionMID[v],				vecVehCollisionM);
			ms_Shader->SetVar(ms_shdVehCollisionRID[v],				vecVehCollisionR);
		}

		Vector4 fakeNormal;
		fakeNormal.SetVector3ClearW(GetFakeGrassNormal());
		fakeNormal.SetW(1.0f);
		ms_Shader->SetVar(ms_shdFakedGrassNormal, fakeNormal);


#if __BANK
		extern bool gbPlantsGeometryCulling;
		dev_bool bGeomCullingTestEnabled	= gbPlantsGeometryCulling;
		extern bool gbPlantsPremultiplyNdotL;
		dev_bool bPremultiplyNdotL			= gbPlantsPremultiplyNdotL;
#else
		dev_bool bGeomCullingTestEnabled	= TRUE;
		dev_bool bPremultiplyNdotL			= TRUE;
#endif


#if __BANK
		extern bool		gbPlantsFlashLOD0, gbPlantsFlashLOD1, gbPlantsFlashLOD2;
		extern bool		gbPlantsDisableLOD0, gbPlantsDisableLOD1, gbPlantsDisableLOD2;
#endif // __BANK

#if __BANK || PLANTS_USE_LOD_SETTINGS
		extern float	gbPlantsLOD0AlphaCloseDist, gbPlantsLOD0AlphaFarDist, gbPlantsLOD0FarDist;
		extern float	gbPlantsLOD1CloseDist, gbPlantsLOD1FarDist, gbPlantsLOD1Alpha0CloseDist, gbPlantsLOD1Alpha0FarDist, gbPlantsLOD1Alpha1CloseDist, gbPlantsLOD1Alpha1FarDist;
		extern float	gbPlantsLOD2CloseDist, gbPlantsLOD2FarDist, gbPlantsLOD2Alpha0CloseDist, gbPlantsLOD2Alpha0FarDist, gbPlantsLOD2Alpha1CloseDist, gbPlantsLOD2Alpha1FarDist;
		extern float	gbPlantsLOD2Alpha1CloseDistFar, gbPlantsLOD2Alpha1FarDistFar;

	#if PLANTS_USE_LOD_SETTINGS
		float grassDistanceMultiplier = GetDistanceMultiplier();
	#else
		float grassDistanceMultiplier = 1.0f;
	#endif

		const float	globalLOD0AlphaCloseDist	= grassDistanceMultiplier*gbPlantsLOD0AlphaCloseDist;
		float		globalLOD0AlphaFarDist		= grassDistanceMultiplier*gbPlantsLOD0AlphaFarDist;
		const float	globalLOD0FarDist			= grassDistanceMultiplier*gbPlantsLOD0FarDist;
		const float	globalLOD1CloseDist			= grassDistanceMultiplier*gbPlantsLOD1CloseDist;
		const float	globalLOD1FarDist			= grassDistanceMultiplier*gbPlantsLOD1FarDist;
		const float	globalLOD1Alpha0CloseDist	= grassDistanceMultiplier*gbPlantsLOD1Alpha0CloseDist;
		const float	globalLOD1Alpha0FarDist		= grassDistanceMultiplier*gbPlantsLOD1Alpha0FarDist;
		const float	globalLOD1Alpha1CloseDist	= grassDistanceMultiplier*gbPlantsLOD1Alpha1CloseDist;
		const float	globalLOD1Alpha1FarDist		= grassDistanceMultiplier*gbPlantsLOD1Alpha1FarDist;
		const float	globalLOD2CloseDist			= grassDistanceMultiplier*gbPlantsLOD2CloseDist;
		const float	globalLOD2FarDist			= grassDistanceMultiplier*gbPlantsLOD2FarDist;
		const float	globalLOD2Alpha0CloseDist	= grassDistanceMultiplier*gbPlantsLOD2Alpha0CloseDist;
		const float	globalLOD2Alpha0FarDist		= grassDistanceMultiplier*gbPlantsLOD2Alpha0FarDist;
		const float	globalLOD2Alpha1CloseDist	= grassDistanceMultiplier*gbPlantsLOD2Alpha1CloseDist;
		const float	globalLOD2Alpha1FarDist		= grassDistanceMultiplier*gbPlantsLOD2Alpha1FarDist;
		const float	globalLOD2Alpha1CloseDistFar= grassDistanceMultiplier*gbPlantsLOD2Alpha1CloseDistFar;
		const float	globalLOD2Alpha1FarDistFar	= grassDistanceMultiplier*gbPlantsLOD2Alpha1FarDistFar;
#else
		const float	globalLOD0AlphaCloseDist	= CPLANT_LOD0_ALPHA_CLOSE_DIST;
		float		globalLOD0AlphaFarDist		= CPLANT_LOD0_ALPHA_FAR_DIST;
		const float	globalLOD0FarDist			= CPLANT_LOD0_FAR_DIST;
		const float	globalLOD1CloseDist			= CPLANT_LOD1_CLOSE_DIST;
		const float	globalLOD1FarDist			= CPLANT_LOD1_FAR_DIST;
		const float	globalLOD1Alpha0CloseDist	= CPLANT_LOD1_ALPHA0_CLOSE_DIST;
		const float	globalLOD1Alpha0FarDist		= CPLANT_LOD1_ALPHA0_FAR_DIST;
		const float	globalLOD1Alpha1CloseDist	= CPLANT_LOD1_ALPHA1_CLOSE_DIST;
		const float	globalLOD1Alpha1FarDist		= CPLANT_LOD1_ALPHA1_FAR_DIST;
		const float	globalLOD2CloseDist			= CPLANT_LOD2_CLOSE_DIST;
		const float	globalLOD2FarDist			= CPLANT_LOD2_FAR_DIST;
		const float	globalLOD2Alpha0CloseDist	= CPLANT_LOD2_ALPHA0_CLOSE_DIST;
		const float	globalLOD2Alpha0FarDist		= CPLANT_LOD2_ALPHA0_FAR_DIST;
		const float	globalLOD2Alpha1CloseDist	= CPLANT_LOD2_ALPHA1_CLOSE_DIST;
		const float	globalLOD2Alpha1FarDist		= CPLANT_LOD2_ALPHA1_FAR_DIST;
		const float	globalLOD2Alpha1CloseDistFar= CPLANT_LOD2_ALPHA1_CLOSE_DIST_FAR;
		const float	globalLOD2Alpha1FarDistFar	= CPLANT_LOD2_ALPHA1_FAR_DIST_FAR;
#endif
		float globalLOD0FarDist2	= globalLOD0FarDist*globalLOD0FarDist;
		float globalLOD1CloseDist2	= globalLOD1CloseDist*globalLOD1CloseDist;
		float globalLOD1FarDist2	= globalLOD1FarDist*globalLOD1FarDist;
		float globalLOD2CloseDist2	= globalLOD2CloseDist*globalLOD2CloseDist;
		float globalLOD2FarDist2	= globalLOD2FarDist*globalLOD2FarDist;

		if(CGrassRenderer::GetGlobalCameraFppEnabled() || CGrassRenderer::GetGlobalForceHDGrassGeometry())
		{	// special case: camera is in FPP mode - force LOD0 to be visible everywhere:
			globalLOD0AlphaFarDist		= globalLOD2Alpha1FarDistFar;

			globalLOD0FarDist2			= globalLOD2FarDist2;
			globalLOD1CloseDist2		=
			globalLOD1FarDist2			=
			globalLOD2CloseDist2		=
			globalLOD2FarDist2			= 999999.0f;
		}


		// LOD0 alpha, umTimer:
		{
			Vector4 vecAlphaFade4;	// [XY=alphaFade01 | Z=umTimer | ?]
#if 1
			// square distance approach:
			const float closeDist	=	globalLOD0AlphaCloseDist;
			const float farDist		=	globalLOD0AlphaFarDist; 
			const float alphaDenom	= 1.0f / (farDist*farDist - closeDist*closeDist);
			vecAlphaFade4.x = (farDist*farDist) * alphaDenom; // x = (f2)  / (f2-c2)
			vecAlphaFade4.y = (-1.0f) * alphaDenom;		 // y = (-1.0)/ (f2-c2) 
#else
			// linear distance approach:
			const float alphaDenom = 1.0f / (m_farDist - m_closeDist);
			vecAlphaFade4.x = (m_farDist) * alphaDenom; // x = (f2)  / (f2-c2)
			vecAlphaFade4.y = (-1.0f) * alphaDenom;		 // y = (-1.0)/ (f2-c2) 
#endif
			vecAlphaFade4.z = (float)(2.0*PI*(double(fwTimer::GetTimeInMilliseconds())/1000.0));
			vecAlphaFade4.w = 0.0f;
			ms_Shader->SetVar(ms_shdFadeAlphaDistUmTimerID, vecAlphaFade4);
		}

		// LOD1 alpha:
		{
			Vector4 vecAlphaFade4;	// [XY=alpha0Fade01 | ZW=alpha1Fade01]
			
			const float closeDist0	= globalLOD1Alpha0CloseDist;
			const float farDist0	= globalLOD1Alpha0FarDist;
			const float alphaDenom0 = 1.0f / (farDist0*farDist0 - closeDist0*closeDist0);
			vecAlphaFade4.x = (farDist0*farDist0) * alphaDenom0;	// x = (f2)  / (f2-c2)
			vecAlphaFade4.y = (-1.0f) * alphaDenom0;				// y = (-1.0)/ (f2-c2) 
			
			const float closeDist1	=	globalLOD1Alpha1CloseDist;
			const float farDist1	=	globalLOD1Alpha1FarDist;
			const float alphaDenom1 = 1.0f / (farDist1*farDist1 - closeDist1*closeDist1);
			vecAlphaFade4.z = (farDist1*farDist1) * alphaDenom1;	// x = (f2)  / (f2-c2)
			vecAlphaFade4.w = (-1.0f) * alphaDenom1;			// y = (-1.0)/ (f2-c2) 
			
			ms_Shader->SetVar(ms_shdFadeAlphaLOD1DistID, vecAlphaFade4);
		}

		// LOD2 alpha:
		{
			Vector4 vecAlphaFade4;	// [XY=alpha0Fade01 | ZW=alpha1Fade01]

			const float closeDist0	= globalLOD2Alpha0CloseDist;
			const float farDist0	= globalLOD2Alpha0FarDist;
			const float alphaDenom0 = 1.0f / (farDist0*farDist0 - closeDist0*closeDist0);
			vecAlphaFade4.x = (farDist0*farDist0) * alphaDenom0; // x = (f2)  / (f2-c2)
			vecAlphaFade4.y = (-1.0f) * alphaDenom0;		 // y = (-1.0)/ (f2-c2) 

			const float closeDist1	=	globalLOD2Alpha1CloseDist;
			const float farDist1	=	globalLOD2Alpha1FarDist;
			const float alphaDenom1 = 1.0f / (farDist1*farDist1 - closeDist1*closeDist1);
			vecAlphaFade4.z = (farDist1*farDist1) * alphaDenom1; // x = (f2)  / (f2-c2)
			vecAlphaFade4.w = (-1.0f) * alphaDenom1;		 // y = (-1.0)/ (f2-c2) 

			ms_Shader->SetVar(ms_shdFadeAlphaLOD2DistID, vecAlphaFade4);

			Vector4 vecAlphaFadeFar4;	// [XY=alpha1FadeFar01 | 0 | 0]

			const float closeDistFar0	= globalLOD2Alpha1CloseDistFar;
			const float farDistFar0		= globalLOD2Alpha1FarDistFar;
			const float alphaDenomFar0	= 1.0f / (farDistFar0*farDistFar0 - closeDistFar0*closeDistFar0);
			vecAlphaFadeFar4.x = (farDistFar0*farDistFar0) * alphaDenomFar0;	// x = (f2)  / (f2-c2)
			vecAlphaFadeFar4.y = (-1.0f) * alphaDenomFar0;						// y = (-1.0)/ (f2-c2) 
			vecAlphaFadeFar4.z = 0.0f;
			vecAlphaFadeFar4.w = 0.0f;

			ms_Shader->SetVar(ms_shdFadeAlphaLOD2DistFarID, vecAlphaFadeFar4);
		}
		
		#if DEVICE_MSAA
			if (!GRCDEVICE.GetMSAA())
				ms_Shader->SetVar(ms_shdAlphaToCoverageScale, 1.0f);
			//else: leave artist-tuned value
		#endif

#if CPLANT_PRE_MULTIPLY_LIGHTING
		const Vector3 sunDir		= -Lights::GetRenderDirectionalLight().GetDirection();
		const float naturalAmbient	= Lights::m_lightingGroup.GetNaturalAmbientBase().w;
#endif

instGrassDataDrawStruct	customInstDraw;
Matrix34				plantMatrix;
Matrix34				skewingMtx;
Vector4					plantColourf;
Color32					plantColour32;
	 
	#if !GRASS_INSTANCING
		customInstDraw.Reset();
	#endif

		for(u32 i=0; i<numTriPlants; i++) 
		{
			PPTriPlant *pCurrTriPlant = &triPlants[i];

			const float wind_bend_scale = pCurrTriPlant->V1.w;	// wind bending scale
			const float wind_bend_var	= pCurrTriPlant->V2.w;	// wind bending variation

			const bool bApplyGroundSkewing		= (pCurrTriPlant->skewAxisAngle.w.GetBinaryData()!=0); // technically this tests for +0 but not -0
			if(bApplyGroundSkewing)
			{
				const Vec4V unpacked_skewAxisAngle = Float16Vec4Unpack(&pCurrTriPlant->skewAxisAngle);
				skewingMtx.MakeRotateUnitAxis(RCC_VECTOR3(unpacked_skewAxisAngle), unpacked_skewAxisAngle.GetWf());
			}

			const bool bEnableCulling = pCurrTriPlant->flags.IsClear(PROCPLANT_CAMERADONOTCULL);

			float NdotL = 1.0f;
#if CPLANT_WRITE_GRASS_NORMAL || CPLANT_PRE_MULTIPLY_LIGHTING
			// set normal for whole pTriLoc:
			Vector4 groundNormal;
			groundNormal.SetX(float(pCurrTriPlant->loctri_normal[0])/127.0f);
			groundNormal.SetY(float(pCurrTriPlant->loctri_normal[1])/127.0f);
			groundNormal.SetZ(float(pCurrTriPlant->loctri_normal[2])/127.0f);
			groundNormal.SetW(0.0f);
		#if	CPLANT_PRE_MULTIPLY_LIGHTING
			if(bPremultiplyNdotL)
			{
				// For pre-multiplying lighting into the color channel
				Vector4 sunDir_Vec4(sunDir.x, sunDir.y, sunDir.z, 0.0f);
				NdotL = Saturate(groundNormal.Dot(sunDir_Vec4)) + naturalAmbient;
			}
		#endif
		#if CPLANT_WRITE_GRASS_NORMAL
			ms_Shader->SetVar(ms_shdTerrainNormal, groundNormal);
		#endif
#endif	// CPLANT_WRITE_GRASS_NORMAL || CPLANT_PRE_MULTIPLY_LIGHTING

			// want same values for plant positions each time...
			g_PlantsRendRand[g_RenderThreadIndex].Reset(pCurrTriPlant->seed);

			// intensity support:  final_I = I + VarI*rand01()
			const s16 intensityVar = pCurrTriPlant->intensity_var;
			u16 intensity = pCurrTriPlant->intensity + u16(float(intensityVar)*g_PlantsRendRand[g_RenderThreadIndex].GetRanged(0.0f, 1.0f));
			intensity = MIN(static_cast<u16>(intensity * NdotL), 255);

			Color32 col32;
			col32.SetAlpha(	pCurrTriPlant->color.GetAlpha());
			#define CALC_COLOR_INTENSITY(COLOR, I)			u8((u16(COLOR)*I) >> 8)
				col32.SetRed(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetRed(),		intensity));
				col32.SetGreen(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetGreen(),	intensity));
				col32.SetBlue(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetBlue(),	intensity));
			#undef CALC_COLOR_INTENSITY

			plantColour32  = col32;
			plantColourf.x = col32.GetRedf();
			plantColourf.y = col32.GetGreenf();
			plantColourf.z = col32.GetBluef();
			plantColourf.w = col32.GetAlphaf();


			grcTexture *pPlantTextureLOD0 = pCurrTriPlant->texture_ptr;
			Assert(pPlantTextureLOD0);
			grcTexture *pPlantTextureLOD1 = pCurrTriPlant->textureLOD1_ptr;
			Assert(pPlantTextureLOD1);

			grmGeometry *pGeometryLOD0		= plantModelsTab[pCurrTriPlant->model_id].m_pGeometry;
			const Vector4 boundSphereLOD0	= plantModelsTab[pCurrTriPlant->model_id].m_BoundingSphere; 
			grmGeometry *pGeometryLOD1		= plantModelsTab[pCurrTriPlant->model_id+CPLANT_SLOT_NUM_MODELS].m_pGeometry;
			const Vector4 boundSphereLOD1	= plantModelsTab[pCurrTriPlant->model_id+CPLANT_SLOT_NUM_MODELS].m_BoundingSphere; 
			const Vector2 GeometryDimLOD2	= plantModelsTab[pCurrTriPlant->model_id+CPLANT_SLOT_NUM_MODELS].m_dimensionLOD2;
			const Vector4 boundSphereLOD2(0,0,0,1.0f);


		#if !GRASS_INSTANCING
			// set color for whole pTriLoc:
			ms_Shader->SetVar(ms_shdPlantColorID,		plantColourf);
		#endif

			// set global micro-movement params: [ scaleH | scaleV | freqH | freqV ]
			const Vec4V unpacked_um_params			= Float16Vec4Unpack(&pCurrTriPlant->um_param);
		#if !GRASS_INSTANCING
			ms_Shader->SetVar(ms_shdUMovementParamsID,	unpacked_um_params);
		#endif
			// set global collision radius scale:
			const Vec2V unpacked_coll_params		= Float16Vec2Unpack(&pCurrTriPlant->coll_params);
		#if !GRASS_INSTANCING
			ms_Shader->SetVar(ms_shdCollParamsID,	unpacked_coll_params);
		#endif

			// LOD2 billboard dimensions:
			Vector4 dimLOD2;
			dimLOD2.x = GeometryDimLOD2.x;
			dimLOD2.y = GeometryDimLOD2.y;
			dimLOD2.z = pCurrTriPlant->flags.IsSet(PROCPLANT_LOD2FARFADE)? 1.0f : 0.0f;
			dimLOD2.w = 0.0f;
		#if !GRASS_INSTANCING
			ms_Shader->SetVar(ms_shdDimensionLOD2ID,	dimLOD2);
		#endif
			u8 ambientScale = pCurrTriPlant->ambient_scl;

			Vector3 *v1 = (Vector3*)&pCurrTriPlant->V1;
			Vector3 *v2 = (Vector3*)&pCurrTriPlant->V2;
			Vector3 *v3 = (Vector3*)&pCurrTriPlant->V3;

			Color32 *c1 = &pCurrTriPlant->groundColorV1;
			Color32 *c2 = &pCurrTriPlant->groundColorV2;
			Color32 *c3 = &pCurrTriPlant->groundColorV3;

			// local per-vertex data: ground color(XYZ) + scaleZ/XYZ:
			Vector4 groundColorV1, groundColorV2, groundColorV3;
			Vector3 groundScaleV1, groundScaleV2, groundScaleV3;
			Vector4 groundColorf;
			Vector3 groundScalef;

			groundColorV1.x = c1->GetRedf();
			groundColorV1.y = c1->GetGreenf();
			groundColorV1.z = c1->GetBluef();
			groundColorV1.w = 1.0f;
			groundScaleV1.x = CPlantLocTri::pv8UnpackAndMapScaleXYZ(c1->GetAlpha());
			groundScaleV1.y = CPlantLocTri::pv8UnpackAndMapScaleZ(c1->GetAlpha());
			groundScaleV1.z = 0.0f;

			groundColorV2.x = c2->GetRedf();
			groundColorV2.y = c2->GetGreenf();
			groundColorV2.z = c2->GetBluef();
			groundColorV2.w = 1.0f;
			groundScaleV2.x = CPlantLocTri::pv8UnpackAndMapScaleXYZ(c2->GetAlpha());
			groundScaleV2.y = CPlantLocTri::pv8UnpackAndMapScaleZ(c2->GetAlpha());
			groundScaleV2.z = 0.0f;

			groundColorV3.x = c3->GetRedf();
			groundColorV3.y = c3->GetGreenf();
			groundColorV3.z = c3->GetBluef();
			groundColorV3.w = 1.0f;
			groundScaleV3.x = CPlantLocTri::pv8UnpackAndMapScaleXYZ(c3->GetAlpha());
			groundScaleV3.y = CPlantLocTri::pv8UnpackAndMapScaleZ(c3->GetAlpha());
			groundScaleV3.z = 0.0f;

			// unpack ground scale ranges:
			const Vec2V unpacked_range_params	= Float16Vec2Unpack(&pCurrTriPlant->scaleRangeXYZ_Z);
			const float fGroundScaleRangeXYZ	= unpacked_range_params.GetXf();
			const float fGroundScaleRangeZ		= unpacked_range_params.GetYf();

			// unpack ScaleXY, ScaleZ, ScaleVarXY, ScaleVarZ:
			const Vec4V unpacked_scale_params	= Float16Vec4Unpack(&pCurrTriPlant->packedScale);
			const Vector4 ScaleV4				= RCC_VECTOR4(unpacked_scale_params);



	#if GRASS_INSTANCING
////////////////////////////////////////////////////////////////////////////////////////////////////

//		const bool bApplyGroundSkewingLod0	= bApplyGroundSkewing && pCurrTriPlant->flags.IsClear(PROCPLANT_NOGROUNDSKEW_LOD0);
//		const bool bApplyGroundSkewingLod1	= bApplyGroundSkewing && pCurrTriPlant->flags.IsClear(PROCPLANT_NOGROUNDSKEW_LOD1);
//		const bool bApplyGroundSkewingLod2	= bApplyGroundSkewing && pCurrTriPlant->flags.IsClear(PROCPLANT_NOGROUNDSKEW_LOD2);

		// want same values for plant positions each time...
		g_PlantsRendRand[g_RenderThreadIndex].Reset(pCurrTriPlant->seed);

		// find store buckets upfront:
#if NEW_STORE_OPT
		InstanceBucket *pLod0Bucket = ms_LOD0Bucket.FindBucket(pGeometryLOD0, pPlantTextureLOD0);
		InstanceBucket *pLod1Bucket = ms_LOD1Bucket.FindBucket(pGeometryLOD1, pPlantTextureLOD1);
		InstanceBucket *pLod2Bucket = ms_LOD2Bucket.FindBucket(NULL, pPlantTextureLOD1);
#endif

		//
		// render a single triPlant full of the desired model
	#if PLANTS_USE_LOD_SETTINGS
		const u32 count = (u32)(float(pCurrTriPlant->num_plants) * GetDensityMultiplier());
	#else
		const u32 count = pCurrTriPlant->num_plants;
	#endif
		for(u32 j=0; j<count; j++)
		{
			const float s = g_PlantsRendRand[g_RenderThreadIndex].GetRanged(0.0f, 1.0f);
			const float t = g_PlantsRendRand[g_RenderThreadIndex].GetRanged(0.0f, 1.0f);

			Vector3 posPlant;
			PPTriPlant::GenPointInTriangle(&posPlant, v1, v2, v3, s, t);

			// calculate LOD0 distance:
		#if CPLANT_USE_COLLISION_2D_DIST
			Vector3 LodDistV3 = posPlant - globalCameraPos;
			Vector2 LodDistV(LodDistV3.x, LodDistV3.y);
		#else
			Vector3 LodDistV = posPlant - globalCameraPos;
		#endif
			const float fLodDist2 = LodDistV.Mag2();

			// interpolate ground color(XYZ):
			PPTriPlant::GenPointInTriangle(&groundColorf, &groundColorV1, &groundColorV2, &groundColorV3, s, t);

			// interpolate ground scaleXYZ/Z weights:
			PPTriPlant::GenPointInTriangle(&groundScalef, &groundScaleV1, &groundScaleV2, &groundScaleV3, s, t);
			const float groundScaleXYZ	= groundScalef.x;
			const float groundScaleZ	= groundScalef.y;

			// ---- apply various amounts of scaling to the matrix -----
			// calculate an x/y scaling value and apply to matrix
			float scaleXY	= ScaleV4.x;
			float scaleZ	= ScaleV4.y;

			// scale variation:
			const float scaleRand01 = g_PlantsRendRand[g_RenderThreadIndex].GetRanged(0.0f, 1.0f);
			const float	scvariationXY	= ScaleV4.z;
			scaleXY		+= scvariationXY*scaleRand01;
			const float	scvariationZ	= ScaleV4.w;
			scaleZ		+= scvariationZ*scaleRand01;

			// apply ground scale variation:
			const float _grndScaleXYZ = (1.0f - fGroundScaleRangeXYZ)*groundScaleXYZ + fGroundScaleRangeXYZ;
			if(_grndScaleXYZ > 0.0f)
			{
				scaleXY *= _grndScaleXYZ;
				scaleZ  *= _grndScaleXYZ;
			}
				
			const float _grndScaleZ = (1.0f - fGroundScaleRangeZ)*groundScaleZ + fGroundScaleRangeZ; 
			if(_grndScaleZ > 0.0f)
			{
				scaleZ  *= _grndScaleZ;
			}

			//// ----- end of scaling stuff ------


			// choose bigger scale for bound frustum check:
			const float boundRadiusScale = (scaleXY > scaleZ)? (scaleXY) : (scaleZ);

			Vector4 sphereV4;
			sphereV4.Set(boundSphereLOD0);
			sphereV4.AddVector3XYZ(posPlant);
			sphereV4.w *= boundRadiusScale;
			spdSphere sphere(VECTOR4_TO_VEC4V(sphereV4));

			bool isCulled = bGeomCullingTestEnabled && bEnableCulling && (!cullFrustum.IntersectsOrContainsSphere(sphere));
			// skip current geometry if not visible:
			if(isCulled)
			{
				g_PlantsRendRand[g_RenderThreadIndex].GetInt();
				g_PlantsRendRand[g_RenderThreadIndex].GetInt();
				g_PlantsRendRand[g_RenderThreadIndex].GetInt();
				g_PlantsRendRand[g_RenderThreadIndex].GetInt();
				if(intensityVar)
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();

				continue;
			}

			plantMatrix.MakeRotateZ(g_PlantsRendRand[g_RenderThreadIndex].GetRanged(0.0f, 1.0f)*2.0f*PI);// overwrites a,b,c
			plantMatrix.MakeTranslate(posPlant);									// overwrites d
			plantMatrix.Scale(scaleXY, scaleXY, scaleZ);	// scale in XY+Z

			if(bApplyGroundSkewing)	// TODO: implement PROCPLANT_NOGROUNDSKEW_LOD0/1/2 flags (not important on NG just yet)
			{
				plantMatrix.Dot3x3(skewingMtx);
			}

			// ---- muck about with the matrix to make atomic look blown by the wind --------
			// use here wind_bend_scale & wind_bend_var
			//	final_bend = [ 1.0 + (bend_var*rand(-1,1)) ] * [ global_wind * bend_scale ]
			const float variationBend = wind_bend_var;
			const float bending =	(1.0f + (variationBend*g_PlantsRendRand[g_RenderThreadIndex].GetRanged(0.0f, 1.0f))) *
									(wind_bend_scale);
			plantMatrix.c.x = bending * globalWindBending.x * g_PlantsRendRand[g_RenderThreadIndex].GetRanged(-1.0f, 1.0f);
			plantMatrix.c.y = bending * globalWindBending.y * g_PlantsRendRand[g_RenderThreadIndex].GetRanged(-1.0f, 1.0f);
			// ----- end of wind stuff -----

			// color variation:
			if(intensityVar)
			{
				// intensity support:  final_I = I + VarI*rand01()
				u16 intensity = pCurrTriPlant->intensity + u16(float(intensityVar)*g_PlantsRendRand[g_RenderThreadIndex].GetRanged(0.0f, 1.0f));
				intensity = MIN(static_cast<u16>(intensity * NdotL), 255);

				Color32 col32(plantColour32.GetColor());
			#define CALC_COLOR_INTENSITY(COLOR, I)			u8((u16(COLOR)*I) >> 8)
				col32.SetRed(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetRed(),		intensity));
				col32.SetGreen(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetGreen(),	intensity));
				col32.SetBlue(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetBlue(),	intensity));
			#undef CALC_COLOR_INTENSITY

				plantColour32 = col32;
			} // if(intensityVar)...

			customInstDraw.m_WorldMatrix.FromMatrix34(plantMatrix);
			customInstDraw.m_PlantColor32 = plantColour32;
			customInstDraw.m_GroundColorAmbient32.SetFromRGBA( RCC_VEC4V(groundColorf) );
			customInstDraw.m_GroundColorAmbient32.SetAlpha(ambientScale);

			// Do we render a LOD0 version ?
			if(fLodDist2 < globalLOD0FarDist2)
			{
			#if __BANK
				if(pCurrTriPlant->flags.IsSet(PROCPLANT_LOD0) && !gbPlantsDisableLOD0 && (!gbPlantsFlashLOD0 || (GRCDEVICE.GetFrameCounter()&0x01)))
			#else
				if(pCurrTriPlant->flags.IsSet(PROCPLANT_LOD0))
			#endif
				{
			#if NEW_STORE_OPT
					pLod0Bucket->AddInstanceDirect(&customInstDraw, unpacked_um_params, unpacked_coll_params);
			#else
					ms_LOD0Bucket.AddInstance(pGeometryLOD0, pPlantTextureLOD0, &customInstDraw, unpacked_um_params, unpacked_coll_params);
			#endif
					#if __BANK
						// rendering stats:
						extern u32 *pLocTriPlantsLOD0DrawnPerSlot;
						(*pLocTriPlantsLOD0DrawnPerSlot)++;
					#endif //__BANK
				}
			}

			// Do we render a LOD1 version ?
			if((fLodDist2 > globalLOD1CloseDist2) && (fLodDist2 < globalLOD1FarDist2))
			{
			#if __BANK
				if(pCurrTriPlant->flags.IsSet(PROCPLANT_LOD1) && !gbPlantsDisableLOD1 && (!gbPlantsFlashLOD1 || (GRCDEVICE.GetFrameCounter()&0x01)))
			#else
				if(pCurrTriPlant->flags.IsSet(PROCPLANT_LOD1))
			#endif
				{
			#if NEW_STORE_OPT
					pLod1Bucket->AddInstanceDirect(&customInstDraw, unpacked_um_params, unpacked_coll_params);
			#else
					ms_LOD1Bucket.AddInstance(pGeometryLOD1, pPlantTextureLOD1, &customInstDraw, unpacked_um_params, unpacked_coll_params);
			#endif
					#if __BANK
						// rendering stats:
						extern u32 *pLocTriPlantsLOD1DrawnPerSlot;
						(*pLocTriPlantsLOD1DrawnPerSlot)++;
					#endif //__BANK
				}
			}

			// Do we render a LOD2 version ?
			if((fLodDist2 > globalLOD2CloseDist2) && (fLodDist2 < globalLOD2FarDist2))
			{
			#if __BANK
				if(pCurrTriPlant->flags.IsSet(PROCPLANT_LOD2) && !gbPlantsDisableLOD2 && (!gbPlantsFlashLOD2 || (GRCDEVICE.GetFrameCounter()&0x01)))
			#else
				if(pCurrTriPlant->flags.IsSet(PROCPLANT_LOD2))
			#endif
				{
			#if NEW_STORE_OPT
					pLod2Bucket->AddInstanceDirect(&customInstDraw, VECTOR4_TO_VEC4V(dimLOD2), unpacked_coll_params);
			#else
					ms_LOD2Bucket.AddInstance(NULL, pPlantTextureLOD1, &customInstDraw, VECTOR4_TO_VEC4V(dimLOD2), unpacked_coll_params);
			#endif
					#if __BANK
						// rendering stats:
						extern u32 *pLocTriPlantsLOD2DrawnPerSlot;
						(*pLocTriPlantsLOD2DrawnPerSlot)++;
					#endif //__BANK
				}
			}
		}// for(u32 j=0; j<count; j++)...

////////////////////////////////////////////////////////////////////////////////////////////////////
	#else //GRASS_INSTANCING
////////////////////////////////////////////////////////////////////////////////////////////////////

		const bool bApplyGroundSkewingLod0	= bApplyGroundSkewing && pCurrTriPlant->flags.IsClear(PROCPLANT_NOGROUNDSKEW_LOD0);
		const bool bApplyGroundSkewingLod1	= bApplyGroundSkewing && pCurrTriPlant->flags.IsClear(PROCPLANT_NOGROUNDSKEW_LOD1);
		const bool bApplyGroundSkewingLod2	= bApplyGroundSkewing && pCurrTriPlant->flags.IsClear(PROCPLANT_NOGROUNDSKEW_LOD2);

			///////////// Render LOD0: /////////////////////////////////////////////////////////////////////////
#if __BANK
		if(pCurrTriPlant->flags.IsSet(PROCPLANT_LOD0) && !gbPlantsDisableLOD0 && (!gbPlantsFlashLOD0 || (GRCDEVICE.GetFrameCounter()&0x01)))
#else
		if(pCurrTriPlant->flags.IsSet(PROCPLANT_LOD0))
#endif
		{
			// want same values for plant positions each time...
			g_PlantsRendRand[g_RenderThreadIndex].Reset(pCurrTriPlant->seed);

			ms_Shader->SetVar(ms_shdTextureID,		pPlantTextureLOD0);

			customInstDraw.Reset();

			// Setup the vertex and index buffers
			grcIndexBuffer* pIndexBuffer			= pGeometryLOD0->GetIndexBuffer(true);
			grcVertexBuffer* pVertexBuffer			= pGeometryLOD0->GetVertexBuffer(true);
			const u16 nPrimType						= pGeometryLOD0->GetPrimitiveType();
			const u32 nIdxCount						= pIndexBuffer->GetIndexCount();
			grcVertexDeclaration* vertexDeclaration	= pGeometryLOD0->GetDecl();

#if !(__D3D11 || RSG_ORBIS)
			bool bShaderCmdBufOut = false;
#endif // !(__D3D11 || RSG_ORBIS)
			//
			// render a single triPlant full of the desired model
			const u32 count = pCurrTriPlant->num_plants;
			for(u32 j=0; j<count; j++)
			{
				const float s = g_PlantsRendRand[g_RenderThreadIndex].GetRanged(0.0f, 1.0f);
				const float t = g_PlantsRendRand[g_RenderThreadIndex].GetRanged(0.0f, 1.0f);

				Vector3 posPlant;
				PPTriPlant::GenPointInTriangle(&posPlant, v1, v2, v3, s, t);

				// calculate LOD0 distance:
			#if CPLANT_USE_COLLISION_2D_DIST
				Vector3 LodDistV3 = posPlant - globalCameraPos;
				Vector2 LodDistV(LodDistV3.x, LodDistV3.y);
			#else
				Vector3 LodDistV = posPlant - globalCameraPos;
			#endif
				const float fLodDist2 = LodDistV.Mag2();
				const bool bLodVisible = (fLodDist2 < globalLOD0FarDist2);

				// early skip if current geometry not visible as LOD0:
				if(!bLodVisible)
				{
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					if(intensityVar)
						g_PlantsRendRand[g_RenderThreadIndex].GetInt();

					continue;
				}


				// interpolate ground color(XYZ):
				PPTriPlant::GenPointInTriangle(&groundColorf, &groundColorV1, &groundColorV2, &groundColorV3, s, t);

				// interpolate ground scaleXYZ/Z weights:
				PPTriPlant::GenPointInTriangle(&groundScalef, &groundScaleV1, &groundScaleV2, &groundScaleV3, s, t);
				const float groundScaleXYZ	= groundScalef.x;
				const float groundScaleZ	= groundScalef.y;

				// ---- apply various amounts of scaling to the matrix -----
				// calculate an x/y scaling value and apply to matrix
				float scaleXY	= ScaleV4.x;
				float scaleZ	= ScaleV4.y;

				// scale variation:
				const float scaleRand01 = g_PlantsRendRand[g_RenderThreadIndex].GetRanged(0.0f, 1.0f);
				const float	scvariationXY	= ScaleV4.z;
				scaleXY		+= scvariationXY*scaleRand01;
				const float	scvariationZ	= ScaleV4.w;
				scaleZ		+= scvariationZ*scaleRand01;

				// apply ground scale variation:
				const float _grndScaleXYZ = (1.0f - fGroundScaleRangeXYZ)*groundScaleXYZ + fGroundScaleRangeXYZ;
				if(_grndScaleXYZ > 0.0f)
				{
					scaleXY *= _grndScaleXYZ;
					scaleZ  *= _grndScaleXYZ;
				}
				
				const float _grndScaleZ = (1.0f - fGroundScaleRangeZ)*groundScaleZ + fGroundScaleRangeZ; 
				if(_grndScaleZ > 0.0f)
				{
					scaleZ  *= _grndScaleZ;
				}

				//// ----- end of scaling stuff ------


				// choose bigger scale for bound frustum check:
				const float boundRadiusScale = (scaleXY > scaleZ)? (scaleXY) : (scaleZ);

				Vector4 sphereV4;
				sphereV4.Set(boundSphereLOD0);
				sphereV4.AddVector3XYZ(posPlant);
				sphereV4.w *= boundRadiusScale;
				spdSphere sphere(VECTOR4_TO_VEC4V(sphereV4));

				// skip current geometry if not visible:
				bool isCulled = bGeomCullingTestEnabled && bEnableCulling && (!cullFrustum.IntersectsOrContainsSphere(sphere));

			#if CPLANT_DYNAMIC_CULL_SPHERES
				//Check dynamic cull spheres and cull if grass is in a cull sphere.
				CPlantMgr::DynCullSphereArray::const_iterator iter;
				CPlantMgr::DynCullSphereArray::const_iterator end = cullSpheres.end();
				for(iter = cullSpheres.begin(); iter != end && !isCulled; ++iter)
					isCulled |= iter->IntersectsSphere(sphere); //iter->ContainsSphere(sphere);
			#endif

			#if CPLANT_CLIP_EDGE_VERT
				//Skip current geometry if it intersects a clipping edge/vert:
				isCulled =	isCulled ||
							(pCurrTriPlant->m_ClipVert_0 && sphere.ContainsPoint(RCC_VEC3V(*v1))) ||
							(pCurrTriPlant->m_ClipVert_1 && sphere.ContainsPoint(RCC_VEC3V(*v2))) ||
							(pCurrTriPlant->m_ClipVert_2 && sphere.ContainsPoint(RCC_VEC3V(*v3))) ||
							(pCurrTriPlant->m_ClipEdge_01 && geomSpheres::TestSphereToSeg(sphere.GetCenter(), sphere.GetRadius(), RCC_VEC3V(*v1), RCC_VEC3V(*v2))) ||
							(pCurrTriPlant->m_ClipEdge_12 && geomSpheres::TestSphereToSeg(sphere.GetCenter(), sphere.GetRadius(), RCC_VEC3V(*v2), RCC_VEC3V(*v3))) ||
							(pCurrTriPlant->m_ClipEdge_20 && geomSpheres::TestSphereToSeg(sphere.GetCenter(), sphere.GetRadius(), RCC_VEC3V(*v3), RCC_VEC3V(*v1)));
			#endif
				
				if(isCulled)
				{
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					if(intensityVar)
						g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					
					continue;
				}

				plantMatrix.MakeRotateZ(g_PlantsRendRand[g_RenderThreadIndex].GetRanged(0.0f, 1.0f)*2.0f*PI);// overwrites a,b,c
				plantMatrix.MakeTranslate(posPlant);									// overwrites d
				plantMatrix.Scale(scaleXY, scaleXY, scaleZ);	// scale in XY+Z

				if(bApplyGroundSkewingLod0)
				{
					plantMatrix.Dot3x3(skewingMtx);
				}

				// ---- muck about with the matrix to make atomic look blown by the wind --------
				// use here wind_bend_scale & wind_bend_var
				//	final_bend = [ 1.0 + (bend_var*rand(-1,1)) ] * [ global_wind * bend_scale ]

				const float variationBend = wind_bend_var;
				const float bending =	(1.0f + (variationBend*g_PlantsRendRand[g_RenderThreadIndex].GetRanged(0.0f, 1.0f))) *
										(wind_bend_scale);
				plantMatrix.c.x = bending * globalWindBending.x * g_PlantsRendRand[g_RenderThreadIndex].GetRanged(-1.0f, 1.0f);
				plantMatrix.c.y = bending * globalWindBending.y * g_PlantsRendRand[g_RenderThreadIndex].GetRanged(-1.0f, 1.0f);
				// ----- end of wind stuff -----


				// color variation:
				if(intensityVar)
				{
					// intensity support:  final_I = I + VarI*rand01()
					u16 intensity = pCurrTriPlant->intensity + u16(float(intensityVar)*g_PlantsRendRand[g_RenderThreadIndex].GetRanged(0.0f, 1.0f));
					intensity = MIN(static_cast<u16>(intensity * NdotL), 255);

					Color32 col32;
					#define CALC_COLOR_INTENSITY(COLOR, I)			u8((u16(COLOR)*I) >> 8)
						col32.SetRed(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetRed(),		intensity));
						col32.SetGreen(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetGreen(),	intensity));
						col32.SetBlue(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetBlue(),	intensity));
					#undef CALC_COLOR_INTENSITY

					plantColourf.x = col32.GetRedf();
					plantColourf.y = col32.GetGreenf();
					plantColourf.z = col32.GetBluef();
				} // if(intensityVar)...


				customInstDraw.m_WorldMatrix.FromMatrix34(plantMatrix);
				customInstDraw.m_PlantColor.Set(plantColourf);
				customInstDraw.m_GroundColor.Set(groundColorf);

#if !(__D3D11 || RSG_ORBIS)
				if(!bShaderCmdBufOut)
				{
					bShaderCmdBufOut = true;
					// bind plant shader			
					BindPlantShader(ms_shdDeferredLOD0TechniqueID_ToUse);
					GRCDEVICE.RecordSetVertexDeclaration(vertexDeclaration);
					GRCDEVICE.SetIndices(*pIndexBuffer);
					GRCDEVICE.SetStreamSource(0, *pVertexBuffer, 0, pVertexBuffer->GetVertexStride());
				}

				const bool bOptimizedSetup = customInstDraw.UseOptimizedVsSetup(plantMatrix);
				if(bOptimizedSetup)
				{	// optimized setup: 3 registers:
					GRCDEVICE.SetVertexShaderConstant(GRASS_REG_TRANSFORM+3, customInstDraw.GetRegisterPointerOpt(), customInstDraw.GetRegisterCountOpt());
					GRCDEVICE.DrawIndexedPrimitive((grcDrawMode)nPrimType, 0, nIdxCount);
				}
				else
				{	// full setup: 6 registers:
					GRCDEVICE.SetVertexShaderConstant(GRASS_REG_TRANSFORM, customInstDraw.GetRegisterPointerFull(), customInstDraw.GetRegisterCountFull());
					GRCDEVICE.DrawIndexedPrimitive((grcDrawMode)nPrimType, 0, nIdxCount);
				}
#else	// !(__D3D11 || RSG_ORBIS)
				ms_Shader->SetVar( ms_grassRegTransform, customInstDraw.m_WorldMatrix );
				ms_Shader->SetVar( ms_grassRegPlantCol, customInstDraw.m_PlantColor );
				ms_Shader->SetVar( ms_grassRegGroundCol, customInstDraw.m_GroundColor );

				BindPlantShader(ms_shdDeferredLOD0TechniqueID_ToUse);

				GRCDEVICE.RecordSetVertexDeclaration(vertexDeclaration);
				GRCDEVICE.SetIndices(*pIndexBuffer);
				GRCDEVICE.SetStreamSource(0, *pVertexBuffer, 0, pVertexBuffer->GetVertexStride());
				GRCDEVICE.DrawIndexedPrimitive((grcDrawMode)nPrimType, 0, nIdxCount);

				UnBindPlantShader();
#endif // !(__D3D11 || RSG_ORBIS)


#if __BANK
				// rendering stats:
				extern u32 *pLocTriPlantsLOD0DrawnPerSlot;
				(*pLocTriPlantsLOD0DrawnPerSlot)++;
#endif //__BANK
			}// for(u32 j=0; j<count; j++)...

#if !(__D3D11 || RSG_ORBIS)
			if(bShaderCmdBufOut)
			{
				UnBindPlantShader();
			}
#endif // !(__D3D11 || RSG_ORBIS)
		}// render LOD0...
////////////////////////////////////////////////////////////////////////////////////////////////////

///////////// Render LOD1: /////////////////////////////////////////////////////////////////////////
#if __BANK
	if(pCurrTriPlant->flags.IsSet(PROCPLANT_LOD1) && !gbPlantsDisableLOD1 && (!gbPlantsFlashLOD1 || (GRCDEVICE.GetFrameCounter()&0x01)))
#else
	if(pCurrTriPlant->flags.IsSet(PROCPLANT_LOD1))
#endif
	{
			g_PlantsRendRand[g_RenderThreadIndex].Reset(pCurrTriPlant->seed);

			ms_Shader->SetVar(ms_shdTextureID,	pPlantTextureLOD1);

			customInstDraw.Reset();

			// Setup the vertex and index buffers
			grcIndexBuffer* pIndexBuffer			= pGeometryLOD1->GetIndexBuffer(true);
			grcVertexBuffer* pVertexBuffer			= pGeometryLOD1->GetVertexBuffer(true);
			const u16 nPrimType						= pGeometryLOD1->GetPrimitiveType();
			const u32 nIdxCount						= pIndexBuffer->GetIndexCount();
			grcVertexDeclaration* vertexDeclaration	= pGeometryLOD1->GetDecl();

#if !(__D3D11 || RSG_ORBIS)
			bool bShaderCmdBufOut = false;
#endif // !(__D3D11 || RSG_ORBIS)

			//
			// render a single triPlant full of the desired model
			const u32 count = pCurrTriPlant->num_plants;
			for(u32 j=0; j<count; j++)
			{
				const float s = g_PlantsRendRand[g_RenderThreadIndex].GetRanged(0.0f, 1.0f);
				const float t = g_PlantsRendRand[g_RenderThreadIndex].GetRanged(0.0f, 1.0f);
				
				Vector3 posPlant;
				PPTriPlant::GenPointInTriangle(&posPlant, v1, v2, v3, s, t);

				// calculate LOD1 distance:
			#if CPLANT_USE_COLLISION_2D_DIST
				Vector3 LodDistV3 = posPlant - globalCameraPos;
				Vector2 LodDistV(LodDistV3.x, LodDistV3.y);
			#else
				Vector3 LodDistV = posPlant - globalCameraPos;
			#endif
				const float fLodDist2 = LodDistV.Mag2();
				const bool bLodVisible = (fLodDist2 > globalLOD1CloseDist2)	&& (fLodDist2 < globalLOD1FarDist2);
	
				// early skip if current geometry not visible as LOD1:
				if(!bLodVisible)
				{
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					if(intensityVar)
						g_PlantsRendRand[g_RenderThreadIndex].GetInt();

					continue;
				}

				// interpolate ground color(XYZ):
				PPTriPlant::GenPointInTriangle(&groundColorf, &groundColorV1, &groundColorV2, &groundColorV3, s, t);

				// interpolate ground scaleXYZ/Z weights:
				PPTriPlant::GenPointInTriangle(&groundScalef, &groundScaleV1, &groundScaleV2, &groundScaleV3, s, t);
				const float groundScaleXYZ	= groundScalef.x;
				const float groundScaleZ	= groundScalef.y;

				// ---- apply various amounts of scaling to the matrix -----
				// calculate an x/y scaling value and apply to matrix
				float scaleXY	= ScaleV4.x;
				float scaleZ	= ScaleV4.y;

				// scale variation:
				const float scaleRand01 = g_PlantsRendRand[g_RenderThreadIndex].GetRanged(0.0f, 1.0f);
				const float	scvariationXY	= ScaleV4.z;
				scaleXY		+= scvariationXY*scaleRand01;
				const float	scvariationZ	= ScaleV4.w;
				scaleZ		+= scvariationZ*scaleRand01;

				// apply ground scale variation:
				const float _grndScaleXYZ = (1.0f - fGroundScaleRangeXYZ)*groundScaleXYZ + fGroundScaleRangeXYZ;
				if(_grndScaleXYZ > 0.0f)
				{
					scaleXY *= _grndScaleXYZ;
					scaleZ  *= _grndScaleXYZ;
				}
				
				const float _grndScaleZ = (1.0f - fGroundScaleRangeZ)*groundScaleZ + fGroundScaleRangeZ; 
				if(_grndScaleZ > 0.0f)
				{
					scaleZ  *= _grndScaleZ;
				}

				//// ----- end of scaling stuff ------

				// choose bigger scale for bound frustum check:
				const float boundRadiusScale = (scaleXY > scaleZ)? (scaleXY) : (scaleZ);

				Vector4 sphereV4;
				sphereV4.Set(boundSphereLOD1);
				sphereV4.AddVector3XYZ(posPlant);
				sphereV4.w *= boundRadiusScale;
				spdSphere sphere(VECTOR4_TO_VEC4V(sphereV4));

				// skip current geometry if not visible:
				bool isCulled = bGeomCullingTestEnabled && bEnableCulling && (!cullFrustum.IntersectsOrContainsSphere(sphere));

			#if CPLANT_DYNAMIC_CULL_SPHERES
				//Check dynamic cull spheres and cull if grass is in a cull sphere.
				CPlantMgr::DynCullSphereArray::const_iterator iter;
				CPlantMgr::DynCullSphereArray::const_iterator end = cullSpheres.end();
				for(iter = cullSpheres.begin(); iter != end && !isCulled; ++iter)
					isCulled |= iter->IntersectsSphere(sphere);
			#endif

			#if CPLANT_CLIP_EDGE_VERT
				//Skip current geometry if it intersects a clipping edge/vert:
				isCulled =	isCulled ||
							(pCurrTriPlant->m_ClipVert_0 && sphere.ContainsPoint(RCC_VEC3V(*v1))) ||
							(pCurrTriPlant->m_ClipVert_1 && sphere.ContainsPoint(RCC_VEC3V(*v2))) ||
							(pCurrTriPlant->m_ClipVert_2 && sphere.ContainsPoint(RCC_VEC3V(*v3))) ||
							(pCurrTriPlant->m_ClipEdge_01 && geomSpheres::TestSphereToSeg(sphere.GetCenter(), sphere.GetRadius(), RCC_VEC3V(*v1), RCC_VEC3V(*v2))) ||
							(pCurrTriPlant->m_ClipEdge_12 && geomSpheres::TestSphereToSeg(sphere.GetCenter(), sphere.GetRadius(), RCC_VEC3V(*v2), RCC_VEC3V(*v3))) ||
							(pCurrTriPlant->m_ClipEdge_20 && geomSpheres::TestSphereToSeg(sphere.GetCenter(), sphere.GetRadius(), RCC_VEC3V(*v3), RCC_VEC3V(*v1)));
			#endif
			
				if(isCulled)
				{
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					if(intensityVar)
						g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					
					continue;
				}

				plantMatrix.MakeRotateZ(g_PlantsRendRand[g_RenderThreadIndex].GetRanged(0.0f, 1.0f)*2.0f*PI);// overwrites a,b,c
				plantMatrix.MakeTranslate(posPlant);									// overwrites d
				plantMatrix.Scale(scaleXY, scaleXY, scaleZ);	// scale in XY+Z

				if(bApplyGroundSkewingLod1)
				{
					plantMatrix.Dot3x3(skewingMtx);
				}

				// ---- muck about with the matrix to make atomic look blown by the wind --------
				g_PlantsRendRand[g_RenderThreadIndex].GetInt();
				g_PlantsRendRand[g_RenderThreadIndex].GetInt();
				g_PlantsRendRand[g_RenderThreadIndex].GetInt();
				// ----- end of wind stuff -----

				// color variation:
				if(intensityVar)
				{
					// intensity support:  final_I = I + VarI*rand01()
					u16 intensity = pCurrTriPlant->intensity + u16(float(intensityVar)*g_PlantsRendRand[g_RenderThreadIndex].GetRanged(0.0f, 1.0f));
					intensity = MIN(static_cast<u16>(intensity * NdotL), 255);

					Color32 col32;
#define CALC_COLOR_INTENSITY(COLOR, I)			u8((u16(COLOR)*I) >> 8)
					col32.SetRed(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetRed(),		intensity));
					col32.SetGreen(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetGreen(),	intensity));
					col32.SetBlue(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetBlue(),	intensity));
#undef CALC_COLOR_INTENSITY

					plantColourf.x = col32.GetRedf();
					plantColourf.y = col32.GetGreenf();
					plantColourf.z = col32.GetBluef();
				} // if(intensityVar)...


				customInstDraw.m_WorldMatrix.FromMatrix34(plantMatrix);
				customInstDraw.m_PlantColor.Set(plantColourf);
				customInstDraw.m_GroundColor.Set(groundColorf);

#if !(__D3D11 || RSG_ORBIS)
				if(!bShaderCmdBufOut)
				{
					bShaderCmdBufOut = true;
					// Bind the grass shader
					BindPlantShader(ms_shdDeferredLOD1TechniqueID_ToUse);
					GRCDEVICE.RecordSetVertexDeclaration(vertexDeclaration);
					GRCDEVICE.SetIndices(*pIndexBuffer);
					GRCDEVICE.SetStreamSource(0, *pVertexBuffer, 0, pVertexBuffer->GetVertexStride());
				}

				const bool bOptimizedSetup = customInstDraw.UseOptimizedVsSetup(plantMatrix);
				if(bOptimizedSetup)
				{	// optimized setup: 3 registers:
					GRCDEVICE.SetVertexShaderConstant(GRASS_REG_TRANSFORM+3, customInstDraw.GetRegisterPointerOpt(), customInstDraw.GetRegisterCountOpt());
					GRCDEVICE.DrawIndexedPrimitive((grcDrawMode)nPrimType, 0, nIdxCount);
				}
				else
				{	// full setup: 6 registers:
					GRCDEVICE.SetVertexShaderConstant(GRASS_REG_TRANSFORM, customInstDraw.GetRegisterPointerFull(), customInstDraw.GetRegisterCountFull());
					GRCDEVICE.DrawIndexedPrimitive((grcDrawMode)nPrimType, 0, nIdxCount);
				}
#else	// !(__D3D11 || RSG_ORBIS)
				ms_Shader->SetVar( ms_grassRegTransform, customInstDraw.m_WorldMatrix );
				ms_Shader->SetVar( ms_grassRegPlantCol, customInstDraw.m_PlantColor );
				ms_Shader->SetVar( ms_grassRegGroundCol, customInstDraw.m_GroundColor );

				BindPlantShader(ms_shdDeferredLOD1TechniqueID_ToUse);

				GRCDEVICE.RecordSetVertexDeclaration(vertexDeclaration);
				GRCDEVICE.SetIndices(*pIndexBuffer);
				GRCDEVICE.SetStreamSource(0, *pVertexBuffer, 0, pVertexBuffer->GetVertexStride());
				GRCDEVICE.DrawIndexedPrimitive((grcDrawMode)nPrimType, 0, nIdxCount);

				UnBindPlantShader();
#endif // !(__D3D11 || RSG_ORBIS)

#if __BANK
				// rendering stats:
				extern u32 *pLocTriPlantsLOD1DrawnPerSlot;
				(*pLocTriPlantsLOD1DrawnPerSlot)++;
#endif //__BANK
			}// for(u32 j=0; j<count; j++)...
//////////////////////////////////////////////////////////////////
#if !(__D3D11 || RSG_ORBIS)
			if(bShaderCmdBufOut)
			{
				UnBindPlantShader();
			}
#endif // !(__D3D11 || RSG_ORBIS)
		}// renderLOD1...
////////////////////////////////////////////////////////////////////////////////////////////////////


///////////// Render LOD2: /////////////////////////////////////////////////////////////////////////
#if __BANK
		if(pCurrTriPlant->flags.IsSet(PROCPLANT_LOD2) && !gbPlantsDisableLOD2 && (!gbPlantsFlashLOD2 || (GRCDEVICE.GetFrameCounter()&0x01)))
#else
		if(pCurrTriPlant->flags.IsSet(PROCPLANT_LOD2))
#endif
		{
			g_PlantsRendRand[g_RenderThreadIndex].Reset(pCurrTriPlant->seed);

			ms_Shader->SetVar(ms_shdTextureID,	pPlantTextureLOD1);

			customInstDraw.Reset();
		
			// Setup the vertex and index buffers
			grcVertexBuffer* pVertexBuffer			= ms_plantLOD2VertexBuffer;
			grcVertexDeclaration* vertexDeclaration	= ms_plantLOD2VertexDecl;

#if !(__D3D11 || RSG_ORBIS)
			bool bShaderCmdBufOut = false;
#endif // !(__D3D11 || RSG_ORBIS)

			//
			// render a single triPlant full of the desired model
			const u32 count = pCurrTriPlant->num_plants;
			for(u32 j=0; j<count; j++)
			{
				const float s = g_PlantsRendRand[g_RenderThreadIndex].GetRanged(0.0f, 1.0f);
				const float t = g_PlantsRendRand[g_RenderThreadIndex].GetRanged(0.0f, 1.0f);
				
				Vector3 posPlant;
				PPTriPlant::GenPointInTriangle(&posPlant, v1, v2, v3, s, t);

				// calculate LOD2 distance:
			#if CPLANT_USE_COLLISION_2D_DIST
				Vector3 LodDistV3 = posPlant - globalCameraPos;
				Vector2 LodDistV(LodDistV3.x, LodDistV3.y);
			#else
				Vector3 LodDistV = posPlant - globalCameraPos;
			#endif
				const float fLodDist2 = LodDistV.Mag2();
				const bool bLodVisible = (fLodDist2 > globalLOD2CloseDist2)	&& (fLodDist2 < globalLOD2FarDist2);

				// early skip if current geometry not visible as LOD2:
				if(!bLodVisible)
				{
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					if(intensityVar)
						g_PlantsRendRand[g_RenderThreadIndex].GetInt();

					continue;
				}

				// interpolate ground color(XYZ):
				PPTriPlant::GenPointInTriangle(&groundColorf, &groundColorV1, &groundColorV2, &groundColorV3, s, t);

				// interpolate ground scaleXYZ/Z weights:
				PPTriPlant::GenPointInTriangle(&groundScalef, &groundScaleV1, &groundScaleV2, &groundScaleV3, s, t);
				const float groundScaleXYZ	= groundScalef.x;
				const float groundScaleZ	= groundScalef.y;

				// ---- apply various amounts of scaling to the matrix -----
				// calculate an x/y scaling value and apply to matrix
				float scaleXY	= ScaleV4.x;
				float scaleZ	= ScaleV4.y;

				// scale variation:
				const float scaleRand01 = g_PlantsRendRand[g_RenderThreadIndex].GetRanged(0.0f, 1.0f);
				const float	scvariationXY	= ScaleV4.z;
				scaleXY		+= scvariationXY*scaleRand01;
				const float	scvariationZ	= ScaleV4.w;
				scaleZ		+= scvariationZ*scaleRand01;

				// apply ground scale variation:
				const float _grndScaleXYZ = (1.0f - fGroundScaleRangeXYZ)*groundScaleXYZ + fGroundScaleRangeXYZ;
				if(_grndScaleXYZ > 0.0f)
				{
					scaleXY *= _grndScaleXYZ;
					scaleZ  *= _grndScaleXYZ;
				}
				
				const float _grndScaleZ = (1.0f - fGroundScaleRangeZ)*groundScaleZ + fGroundScaleRangeZ; 
				if(_grndScaleZ > 0.0f)
				{
					scaleZ  *= _grndScaleZ;
				}

				//// ----- end of scaling stuff ------


				// choose bigger scale for bound frustum check:
				//const float boundRadiusScale = (scaleXY > scaleZ)? (scaleXY) : (scaleZ);

				Vector4 sphereV4;
				sphereV4.Set(boundSphereLOD2);
				sphereV4.AddVector3XYZ(posPlant);
				//sphereV4.w *= boundRadiusScale;
				spdSphere sphere(VECTOR4_TO_VEC4V(sphereV4));

				// skip current geometry if not visible:
				bool isCulled = bGeomCullingTestEnabled && bEnableCulling && (!cullFrustum.IntersectsOrContainsSphere(sphere));

			#if CPLANT_DYNAMIC_CULL_SPHERES || CPLANT_CLIP_EDGE_VERT
				// choose bigger scale for bound frustum check:
				const float boundRadiusScale = (scaleXY > scaleZ)? (scaleXY) : (scaleZ);

				//The above sphere is not a tight enough bound for DynSphereCull/Poly Cull. Recompute a more accurate bounding sphere.
				float lod2MaxOffset = Max(GeometryDimLOD2.x, GeometryDimLOD2.y);
				Vec3V lod2halfOffset = Vec3V(lod2MaxOffset, lod2MaxOffset, lod2MaxOffset) * ScalarV(V_HALF);
				Vec3V lod2SphereCenter = sphere.GetCenter();// + lod2halfOffset;
				ScalarV lod2SphereRadius = lod2halfOffset.GetX() * LoadScalar32IntoScalarV(boundRadiusScale);
				spdSphere lod2Sphere(lod2SphereCenter, lod2SphereRadius);
			#endif
			
			#if CPLANT_DYNAMIC_CULL_SPHERES
				//Check dynamic cull spheres and cull if grass is in a cull sphere.
				CPlantMgr::DynCullSphereArray::const_iterator iter;
				CPlantMgr::DynCullSphereArray::const_iterator end = cullSpheres.end();
				for(iter = cullSpheres.begin(); iter != end && !isCulled; ++iter)
					isCulled |= iter->IntersectsSphere(lod2Sphere);
			#endif

			#if CPLANT_CLIP_EDGE_VERT
				//Skip current geometry if it intersects a clipping edge/vert:
				isCulled =	isCulled ||
							(pCurrTriPlant->m_ClipVert_0 && lod2Sphere.ContainsPoint(RCC_VEC3V(*v1))) ||
							(pCurrTriPlant->m_ClipVert_1 && lod2Sphere.ContainsPoint(RCC_VEC3V(*v2))) ||
							(pCurrTriPlant->m_ClipVert_2 && lod2Sphere.ContainsPoint(RCC_VEC3V(*v3))) ||
							(pCurrTriPlant->m_ClipEdge_01 && geomSpheres::TestSphereToSeg(lod2Sphere.GetCenter(), lod2Sphere.GetRadius(), RCC_VEC3V(*v1), RCC_VEC3V(*v2))) ||
							(pCurrTriPlant->m_ClipEdge_12 && geomSpheres::TestSphereToSeg(lod2Sphere.GetCenter(), lod2Sphere.GetRadius(), RCC_VEC3V(*v2), RCC_VEC3V(*v3))) ||
							(pCurrTriPlant->m_ClipEdge_20 && geomSpheres::TestSphereToSeg(lod2Sphere.GetCenter(), lod2Sphere.GetRadius(), RCC_VEC3V(*v3), RCC_VEC3V(*v1)));
			#endif

				if(isCulled)
				{
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					g_PlantsRendRand[g_RenderThreadIndex].GetInt();
					if(intensityVar)
						g_PlantsRendRand[g_RenderThreadIndex].GetInt();

					continue;
				}

//				plantMatrix.MakeRotateZ(g_PlantsRendRand.GetRanged(0.0f, 1.0f)*2.0f*PI);// overwrites a,b,c
				g_PlantsRendRand[g_RenderThreadIndex].GetInt();
				plantMatrix.Identity3x3();
				plantMatrix.MakeTranslate(posPlant);									// overwrites d
				plantMatrix.Scale(scaleXY, scaleXY, scaleZ);	// scale in XY+Z

				if(bApplyGroundSkewingLod2)
				{
					plantMatrix.Dot3x3(skewingMtx);
				}

				// ---- muck about with the matrix to make atomic look blown by the wind --------
				g_PlantsRendRand[g_RenderThreadIndex].GetInt();
				g_PlantsRendRand[g_RenderThreadIndex].GetInt();
				g_PlantsRendRand[g_RenderThreadIndex].GetInt();
				// ----- end of wind stuff -----

				// color variation:
				if(intensityVar)
				{
#if 1
					// intensity support:  final_I = I + VarI*rand01()
					u16 intensity = pCurrTriPlant->intensity + u16(float(intensityVar)*g_PlantsRendRand[g_RenderThreadIndex].GetRanged(0.0f, 1.0f));
					intensity = MIN(static_cast<u16>(intensity * NdotL), 255);

					Color32 col32;
#define CALC_COLOR_INTENSITY(COLOR, I)			u8((u16(COLOR)*I) >> 8)
					col32.SetRed(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetRed(),		intensity));
					col32.SetGreen(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetGreen(),	intensity));
					col32.SetBlue(	CALC_COLOR_INTENSITY(pCurrTriPlant->color.GetBlue(),	intensity));
#undef CALC_COLOR_INTENSITY

					plantColourf.x = col32.GetRedf();
					plantColourf.y = col32.GetGreenf();
					plantColourf.z = col32.GetBluef();
#else
					g_PlantsRendRand.GetInt();
#endif
				} // if(intensityVar)...

				customInstDraw.m_WorldMatrix.FromMatrix34(plantMatrix);
				customInstDraw.m_PlantColor.Set(plantColourf);
				customInstDraw.m_GroundColor.Set(groundColorf);

#if !(__D3D11 || RSG_ORBIS)
				if(!bShaderCmdBufOut)
				{
					bShaderCmdBufOut = true;
					// Bind the grass shader
					BindPlantShader(ms_shdDeferredLOD2TechniqueID_ToUse);
					GRCDEVICE.RecordSetVertexDeclaration(vertexDeclaration);
					GRCDEVICE.SetStreamSource(0, *pVertexBuffer, 0, pVertexBuffer->GetVertexStride());
				}

				const bool bOptimizedSetup = customInstDraw.UseOptimizedVsSetup(plantMatrix);
				const grcDrawMode lod2Draw = __WIN32PC? drawTriFan : drawQuads;
				if(bOptimizedSetup)
				{	// optimized setup: 3 registers:
					GRCDEVICE.SetVertexShaderConstant(GRASS_REG_TRANSFORM+3, customInstDraw.GetRegisterPointerOpt(), customInstDraw.GetRegisterCountOpt());
					GRCDEVICE.DrawPrimitive(lod2Draw, /*startVertex*/0, /*vertexCount*/4 );
				}
				else
				{	// full setup: 6 registers:
					GRCDEVICE.SetVertexShaderConstant(GRASS_REG_TRANSFORM, customInstDraw.GetRegisterPointerFull(), customInstDraw.GetRegisterCountFull());
					GRCDEVICE.DrawPrimitive(lod2Draw, /*startVertex*/0, /*vertexCount*/4 );
				}
#else
				ms_Shader->SetVar( ms_grassRegTransform, customInstDraw.m_WorldMatrix );
				ms_Shader->SetVar( ms_grassRegPlantCol, customInstDraw.m_PlantColor );
				ms_Shader->SetVar( ms_grassRegGroundCol, customInstDraw.m_GroundColor );
				BindPlantShader(ms_shdDeferredLOD2TechniqueID_ToUse);

				GRCDEVICE.RecordSetVertexDeclaration(vertexDeclaration);
				GRCDEVICE.SetStreamSource(0, *pVertexBuffer, 0, pVertexBuffer->GetVertexStride());
				GRCDEVICE.DrawPrimitive(drawTriStrip/*lod2Draw*/, /*startVertex*/0, /*vertexCount*/4 );

				UnBindPlantShader();
#endif // !(__D3D11 || RSG_ORBIS)

#if __BANK
				// rendering stats:
				extern u32 *pLocTriPlantsLOD2DrawnPerSlot;
				(*pLocTriPlantsLOD2DrawnPerSlot)++;
#endif //__BANK
			}// for(u32 j=0; j<count; j++)...
			//////////////////////////////////////////////////////////////////

#if !(__D3D11 || RSG_ORBIS)
			if(bShaderCmdBufOut)
			{
				UnBindPlantShader();
			}
#endif // !(__D3D11 || RSG_ORBIS)
			}// renderLOD2's...
////////////////////////////////////////////////////////////////////////////////////////////////////
	#endif // GRASS_INSTANCING
		}// for(u32 i=0; i<numTriPlants; i++)...

        return(TRUE);
}// end of CPPTriPlantBuffer::DrawTriPlants()...
#endif //PLANTSMGR_MULTI_RENDER...
#endif // PSN_PLANTSMGR_SPU_RENDER...

//
//
//
//
void CGrassRenderer::BindPlantShader(grcEffectTechnique forcedTech)
{
	// Bind the grass shader
	ASSERT_ONLY(s32 numPasses =) ms_Shader->BeginDraw(grmShader::RMC_DRAW, /*restoreState*/FALSE, /*techOverride*/forcedTech);
	Assert(numPasses>0);
	const s32 pass=0;
	ms_Shader->Bind(pass);
}

void CGrassRenderer::UnBindPlantShader()
{
	// Unbind the shader
	GRCDEVICE.ClearStreamSource(0);
	ms_Shader->UnBind();
	ms_Shader->EndDraw();
}

#if PSN_PLANTSMGR_SPU_RENDER && PLANTSMGR_BIG_HEAP_IN_BLOCKS
//
//
// returns true, if given offset is inside any of heap blocks
//
bool CGrassRenderer::BigHeapBlockArrayContainsOffset(u32 heapID, u32 blockSize, u32 offset)
{
	grassHeapBlock *heapArray = sm_BigHeapBlockArray[heapID];
	u32 count = sm_BigHeapBlockArrayCount[heapID];

	while( count-- )
	{
		if( (offset >= heapArray[count].offset) && (offset < heapArray[count].offset+blockSize) )
			return(true);
	}

	return(false);
}
#endif //PSN_PLANTSMGR_SPU_RENDER

///////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
//
//#pragma mark --- PPTriPlantBuffer stuff ---

//
//
//
//
CPPTriPlantBuffer::CPPTriPlantBuffer()
{
	int i;

	for(i=0; i<NUMBER_OF_RENDER_THREADS; i++)
	{
		this->m_PerThreadData[i].m_currentIndex = 0;
		this->m_PerThreadData[i].m_plantModelsSet = PPPLANTBUF_MODEL_SET0;
		//this->SetPlantModelsSet(PPPLANTBUF_MODEL_SET0);
	}
	
	for(s32 i=0; i<PPTRIPLANT_MODELS_TAB_SIZE; i++)
	{
		m_pPlantModelsTab[i] = NULL;
	}


}

//
//
//
//
CPPTriPlantBuffer::~CPPTriPlantBuffer()
{
}



//
//
// flush the buffer:
//
void CPPTriPlantBuffer::Flush()
{
	if(m_PerThreadData[g_RenderThreadIndex].m_currentIndex>0)
	{
#if !PSN_PLANTSMGR_SPU_RENDER
		grassModel*	plantModels = NULL;

		switch(this->m_PerThreadData[g_RenderThreadIndex].m_plantModelsSet)
		{
			case(PPPLANTBUF_MODEL_SET0):	plantModels = m_pPlantModelsTab[0];		break;
			case(PPPLANTBUF_MODEL_SET1):	plantModels = m_pPlantModelsTab[1];		break;
			case(PPPLANTBUF_MODEL_SET2):	plantModels = m_pPlantModelsTab[2];		break;
			case(PPPLANTBUF_MODEL_SET3):	plantModels = m_pPlantModelsTab[3];		break;
			default:						plantModels = NULL;						break;
		}
		Assertf(plantModels, "Unknown PlantModelsSet!");
#endif

		// save current seed:
		const u32 storedSeed = g_PlantsRendRand[g_RenderThreadIndex].GetSeed();


#if PSN_PLANTSMGR_SPU_RENDER
		{
			CGrassRenderer::DrawTriPlantsSPU(m_PerThreadData[g_RenderThreadIndex].m_Buffer, m_PerThreadData[g_RenderThreadIndex].m_currentIndex, m_PerThreadData[g_RenderThreadIndex].m_plantModelsSet);
			m_PerThreadData[g_RenderThreadIndex].m_currentIndex = 0;
		}
#else //PSN_PLANTSMGR_SPU_RENDER...
		{
			CGrassRenderer::DrawTriPlants(m_PerThreadData[g_RenderThreadIndex].m_Buffer, m_PerThreadData[g_RenderThreadIndex].m_currentIndex, plantModels);
			m_PerThreadData[g_RenderThreadIndex].m_currentIndex = 0;
		}
#endif //PSN_PLANTSMGR_SPU_RENDER...


		// restore randomness:
		g_PlantsRendRand[g_RenderThreadIndex].Reset(storedSeed);
	} // if(m_currentIndex>0)...

}// end of CPPTriPlantBuffer::Flush()...



//
//
//
//
PPTriPlant* CPPTriPlantBuffer::GetPPTriPlantPtr(s32 amountToAdd)
{
	if((this->m_PerThreadData[g_RenderThreadIndex].m_currentIndex+amountToAdd) > PPTRIPLANT_BUFFER_SIZE)
	{
		this->Flush();
	}

	return(&this->m_PerThreadData[g_RenderThreadIndex].m_Buffer[m_PerThreadData[g_RenderThreadIndex].m_currentIndex]);

}


//
//
//
//

void CPPTriPlantBuffer::ChangeCurrentPlantModelsSet(s32 newSet)
{
	// different modes of pipeline?
	if(this->GetPlantModelsSet() != newSet)
	{
		// flush contents of old pipeline mode:
		this->Flush();
		
		// set new pipeline mode:
		this->SetPlantModelsSet(newSet);
	}

}


//
//
//
//
void CPPTriPlantBuffer::IncreaseBufferIndex(s32 pipeMode, s32 amount)
{
	if(this->GetPlantModelsSet() != pipeMode)
	{
		// incompatible pipeline modes!
		Assertf(FALSE, "Incompatible pipeline modes!");
		return;
	}

	this->m_PerThreadData[g_RenderThreadIndex].m_currentIndex += amount;
	if(this->m_PerThreadData[g_RenderThreadIndex].m_currentIndex >= PPTRIPLANT_BUFFER_SIZE)
	{
		this->Flush();
	}
}



//
//
//
//
bool CPPTriPlantBuffer::SetPlantModelsTab(u32 index, grassModel* pPlantModels)
{
	if(index >= PPTRIPLANT_MODELS_TAB_SIZE)
		return(FALSE);
		
	this->m_pPlantModelsTab[index] = pPlantModels;

	return(TRUE);
}


//
//
//
//
grassModel* CPPTriPlantBuffer::GetPlantModelsTab(u32 index)
{
	if(index >= PPTRIPLANT_MODELS_TAB_SIZE)
		return(NULL);
	grassModel* plantModels = this->m_pPlantModelsTab[index];
	return(plantModels);
}


#if PSN_PLANTSMGR_SPU_RENDER
//
//
// internal buffers for storing texture command buffers:
//
bool	CPPTriPlantBuffer::SetPlantTexturesCmd(u32 slotID, u32 index, u32 *cmd)
{
	Assert(slotID < PPTRIPLANT_MODELS_TAB_SIZE);
	Assert(index < CPLANT_SLOT_NUM_TEXTURES*2);	// base textures + LOD textures

	u32 *dst0 = m_pPlantsTexturesCmdTab[slotID];
	u32 *dst = dst0 + index*PLANTSMGR_LOCAL_TEXTURE_MAXCMDSIZE;

	::sysMemCpy(dst, cmd, PLANTSMGR_LOCAL_TEXTURE_MAXCMDSIZE*sizeof(u32));

	return(TRUE);
}

u32*	CPPTriPlantBuffer::GetPlantTexturesCmd(u32 slotID, u32 index)
{
	u32 *dst0 = m_pPlantsTexturesCmdTab[slotID];
	u32 *dst = dst0 + index*PLANTSMGR_LOCAL_TEXTURE_MAXCMDSIZE;
	return(dst);
}

u32	CPPTriPlantBuffer::GetPlantTexturesCmdOffset(u32 slotID, u32 index)
{
	u32 offset0 = m_pPlantsTexturesCmdOffsetTab[slotID];
	u32 dst = offset0 + (index*PLANTSMGR_LOCAL_TEXTURE_MAXCMDSIZE)*sizeof(u32);
	
	return(dst);
}

//
//
//
//
bool CPPTriPlantBuffer::AllocateTextureBuffers()
{
	m_pPlantsTexturesCmd0		= (u32*)GtaMallocAlign(CPLANT_SLOT_NUM_TEXTURES*2*PLANTSMGR_LOCAL_TEXTURE_MAXCMDSIZE*sizeof(u32), PLANTSMGR_CMD_DEF_ALIGN);
	m_pPlantsTexturesCmdOffset0 = gcm::MainOffset(m_pPlantsTexturesCmd0);
	Assert(m_pPlantsTexturesCmd0);
	Assert(m_pPlantsTexturesCmdOffset0);

	m_pPlantsTexturesCmd1		= (u32*)GtaMallocAlign(CPLANT_SLOT_NUM_TEXTURES*2*PLANTSMGR_LOCAL_TEXTURE_MAXCMDSIZE*sizeof(u32), PLANTSMGR_CMD_DEF_ALIGN);
	m_pPlantsTexturesCmdOffset1	= gcm::MainOffset(m_pPlantsTexturesCmd1);
	Assert(m_pPlantsTexturesCmd1);
	Assert(m_pPlantsTexturesCmdOffset1);

	m_pPlantsTexturesCmd2		= (u32*)GtaMallocAlign(CPLANT_SLOT_NUM_TEXTURES*2*PLANTSMGR_LOCAL_TEXTURE_MAXCMDSIZE*sizeof(u32), PLANTSMGR_CMD_DEF_ALIGN);
	m_pPlantsTexturesCmdOffset2	= gcm::MainOffset(m_pPlantsTexturesCmd2);
	Assert(m_pPlantsTexturesCmd2);
	Assert(m_pPlantsTexturesCmdOffset2);
	
	m_pPlantsTexturesCmd3		= (u32*)GtaMallocAlign(CPLANT_SLOT_NUM_TEXTURES*2*PLANTSMGR_LOCAL_TEXTURE_MAXCMDSIZE*sizeof(u32), PLANTSMGR_CMD_DEF_ALIGN);
	m_pPlantsTexturesCmdOffset3	= gcm::MainOffset(m_pPlantsTexturesCmd3);
	Assert(m_pPlantsTexturesCmd3);
	Assert(m_pPlantsTexturesCmdOffset3);

	m_pPlantsTexturesCmdTab[0]			= m_pPlantsTexturesCmd0;
	m_pPlantsTexturesCmdTab[1]			= m_pPlantsTexturesCmd1;
	m_pPlantsTexturesCmdTab[2]			= m_pPlantsTexturesCmd2;
	m_pPlantsTexturesCmdTab[3]			= m_pPlantsTexturesCmd3;

	m_pPlantsTexturesCmdOffsetTab[0]	= m_pPlantsTexturesCmdOffset0;
	m_pPlantsTexturesCmdOffsetTab[1]	= m_pPlantsTexturesCmdOffset1;
	m_pPlantsTexturesCmdOffsetTab[2]	= m_pPlantsTexturesCmdOffset2;
	m_pPlantsTexturesCmdOffsetTab[3]	= m_pPlantsTexturesCmdOffset3;

	return(TRUE);
}

void CPPTriPlantBuffer::DestroyTextureBuffers()
{
	#define DELETE_PTR(P, OFFSET) {if(P) {GtaFreeAlign(P); P=NULL; OFFSET=0;}}
		DELETE_PTR(m_pPlantsTexturesCmd0, m_pPlantsTexturesCmdOffset0);
		DELETE_PTR(m_pPlantsTexturesCmd1, m_pPlantsTexturesCmdOffset1);
		DELETE_PTR(m_pPlantsTexturesCmd2, m_pPlantsTexturesCmdOffset2);
		DELETE_PTR(m_pPlantsTexturesCmd3, m_pPlantsTexturesCmdOffset3);
	#undef DELETE_PTR

	m_pPlantsTexturesCmdTab[0]			=
	m_pPlantsTexturesCmdTab[1]			=
	m_pPlantsTexturesCmdTab[2]			=
	m_pPlantsTexturesCmdTab[3]			= NULL;

	m_pPlantsTexturesCmdOffsetTab[0]	= 
	m_pPlantsTexturesCmdOffsetTab[1]	=
	m_pPlantsTexturesCmdOffsetTab[2]	=
	m_pPlantsTexturesCmdOffsetTab[3]	= 0;
}


//
//
// internal buffers for storing geometry command buffers:
//
bool	CPPTriPlantBuffer::SetPlantModelsCmd(u32 slotID, u32 index, u32 *cmd)
{
	Assert(slotID < PPTRIPLANT_MODELS_TAB_SIZE);
	Assert(index < CPLANT_SLOT_NUM_MODELS*2);	// base models + LOD models
	
	u32 *dst0 = m_pPlantsModelsCmdTab[slotID];
	u32 *dst  = dst0 + index*PLANTSMGR_LOCAL_GEOMETRY_MAXCMDSIZE;

	::sysMemCpy(dst, cmd, PLANTSMGR_LOCAL_GEOMETRY_MAXCMDSIZE*sizeof(u32));

	return(TRUE);
}

u32*	CPPTriPlantBuffer::GetPlantModelsCmd(u32 slotID, u32 index)
{
	u32 *dst0 = m_pPlantsModelsCmdTab[slotID];
	u32 *dst = dst0 + index*PLANTSMGR_LOCAL_GEOMETRY_MAXCMDSIZE;
	return(dst);
}

u32	CPPTriPlantBuffer::GetPlantModelsCmdOffset(u32 slotID, u32 index)
{
	u32 offset0 = m_pPlantsModelsCmdOffsetTab[slotID];
	u32 dst = offset0 + (index*PLANTSMGR_LOCAL_GEOMETRY_MAXCMDSIZE)*sizeof(u32);

	return(dst);
}

//
//
//
//
bool CPPTriPlantBuffer::AllocateModelsBuffers()
{
	m_pPlantsModelsCmd0			= (u32*)GtaMallocAlign(CPLANT_SLOT_NUM_MODELS*2*PLANTSMGR_LOCAL_GEOMETRY_MAXCMDSIZE*sizeof(u32), PLANTSMGR_CMD_DEF_ALIGN);
	m_pPlantsModelsCmdOffset0	= gcm::MainOffset(m_pPlantsModelsCmd0);
	Assert(m_pPlantsModelsCmd0);
	Assert(m_pPlantsModelsCmdOffset0);

	m_pPlantsModelsCmd1			= (u32*)GtaMallocAlign(CPLANT_SLOT_NUM_MODELS*2*PLANTSMGR_LOCAL_GEOMETRY_MAXCMDSIZE*sizeof(u32), PLANTSMGR_CMD_DEF_ALIGN);
	m_pPlantsModelsCmdOffset1	= gcm::MainOffset(m_pPlantsModelsCmd1);
	Assert(m_pPlantsModelsCmd1);
	Assert(m_pPlantsModelsCmdOffset1);

	m_pPlantsModelsCmd2			= (u32*)GtaMallocAlign(CPLANT_SLOT_NUM_MODELS*2*PLANTSMGR_LOCAL_GEOMETRY_MAXCMDSIZE*sizeof(u32), PLANTSMGR_CMD_DEF_ALIGN);
	m_pPlantsModelsCmdOffset2	= gcm::MainOffset(m_pPlantsModelsCmd2);
	Assert(m_pPlantsModelsCmd2);
	Assert(m_pPlantsModelsCmdOffset2);

	m_pPlantsModelsCmd3			= (u32*)GtaMallocAlign(CPLANT_SLOT_NUM_MODELS*2*PLANTSMGR_LOCAL_GEOMETRY_MAXCMDSIZE*sizeof(u32), PLANTSMGR_CMD_DEF_ALIGN);
	m_pPlantsModelsCmdOffset3	= gcm::MainOffset(m_pPlantsModelsCmd3);
	Assert(m_pPlantsModelsCmd3);
	Assert(m_pPlantsModelsCmdOffset3);

	m_pPlantsModelsCmdTab[0]		= m_pPlantsModelsCmd0;
	m_pPlantsModelsCmdTab[1]		= m_pPlantsModelsCmd1;
	m_pPlantsModelsCmdTab[2]		= m_pPlantsModelsCmd2;
	m_pPlantsModelsCmdTab[3]		= m_pPlantsModelsCmd3;

	m_pPlantsModelsCmdOffsetTab[0]	= m_pPlantsModelsCmdOffset0;
	m_pPlantsModelsCmdOffsetTab[1]	= m_pPlantsModelsCmdOffset1;
	m_pPlantsModelsCmdOffsetTab[2]	= m_pPlantsModelsCmdOffset2;
	m_pPlantsModelsCmdOffsetTab[3]	= m_pPlantsModelsCmdOffset3;

	return(TRUE);
}

void CPPTriPlantBuffer::DestroyModelsBuffers()
{
#define DELETE_PTR(P, OFFSET) {if(P) {GtaFreeAlign(P); P=NULL; OFFSET=0;}}
	DELETE_PTR(m_pPlantsModelsCmd0, m_pPlantsModelsCmdOffset0);
	DELETE_PTR(m_pPlantsModelsCmd1, m_pPlantsModelsCmdOffset1);
	DELETE_PTR(m_pPlantsModelsCmd2, m_pPlantsModelsCmdOffset2);
	DELETE_PTR(m_pPlantsModelsCmd3, m_pPlantsModelsCmdOffset3);
#undef DELETE_PTR

	m_pPlantsModelsCmdTab[0]		=
	m_pPlantsModelsCmdTab[1]		=
	m_pPlantsModelsCmdTab[2]		=
	m_pPlantsModelsCmdTab[3]		= NULL;

	m_pPlantsModelsCmdOffsetTab[0]	= 
	m_pPlantsModelsCmdOffsetTab[1]	=
	m_pPlantsModelsCmdOffsetTab[2]	=
	m_pPlantsModelsCmdOffsetTab[3]	= 0;
}

#endif //PSN_PLANTSMGR_SPU_RENDER...

///////////////////////////////////////////////////////////////////////////////////////////////////////

#if GRASS_INSTANCING

CGrassRenderer::InstanceBucket::InstanceBucket()
{
}


CGrassRenderer::InstanceBucket::~InstanceBucket()
{
}

#if PLANTSMGR_MULTI_RENDER
void CGrassRenderer::InstanceBucket::Init(grmShader *pShader, grcEffectVar textureVar)
{
	// Record shader details.
	m_pShader = pShader;
	m_textureVar = textureVar;

	for(s32 i=0; i<NUMBER_OF_RENDER_THREADS; i++)
	{
		m_PerThreadData0[i].m_isInstanceTextureLocked = false;
	}

	InitialiseTransport();
}


void CGrassRenderer::InstanceBucket::ShutDown()
{
	CleanUpTransport();
}


void CGrassRenderer::InstanceBucket::Reset(u8 rti, grcEffectTechnique techId)
{
	AssertMsg((m_PerThreadData0[rti].m_isInstanceTextureLocked == false), "CGrassRenderer::InstanceBucket::Reset()...Intance texture should not be locked.");
	m_PerThreadData0[rti].m_techId = techId;
	m_PerThreadData0[rti].m_noOfInstances = 0;
	m_PerThreadData0[rti].m_pCurrentGeometry = NULL;
	m_PerThreadData0[rti].m_pCurrentTexture = NULL;
}


bool CGrassRenderer::InstanceBucket::AddInstance(u8 rti, bool bMainRT, grmGeometry *pGeometry, grcTexture *pTexture, struct instGrassDataDrawStruct *pInstance, const Vec4V &umParams, const Vec2V &collParams)
{
	// Have we reached capacity or is the geometry/texture different ?
	if(IsFull(rti) || (pGeometry != m_PerThreadData0[rti].m_pCurrentGeometry) || (pTexture != m_PerThreadData0[rti].m_pCurrentTexture))
	{
		if(GetCount(rti)==0)
		{
			// Record texture/geometry.
			m_PerThreadData0[rti].m_pCurrentTexture = pTexture;
			m_PerThreadData0[rti].m_pCurrentGeometry = pGeometry;
		}
		else
		{
			if(bMainRT)
			{
				// Send all instances.
				Flush(rti);

				// Record texture/geometry.
				m_PerThreadData0[rti].m_pCurrentTexture = pTexture;
				m_PerThreadData0[rti].m_pCurrentGeometry = pGeometry;
			}
			else
			{
				return(false);	// subtask: instance not added
			}
		}
	}

	if(!m_PerThreadData0[rti].m_isInstanceTextureLocked)
	{
		//LockTransport(rti);
		m_PerThreadData0[rti].m_isInstanceTextureLocked = true;
	}

	Vector4 *pDest = GetDataPtr(rti);

	// Pack the instance data.
#if 1
	Vector4 C0 = pInstance->m_WorldMatrix.a;
	Vector4 C1 = pInstance->m_WorldMatrix.b;
	Vector4 C2 = pInstance->m_WorldMatrix.c;
	Vector4 C3 = pInstance->m_WorldMatrix.d;


	*((Color32*)(&C0.w)) = pInstance->m_PlantColor32;					// pack directly as u32
	*((Color32*)(&C1.w)) = pInstance->m_GroundColorAmbient32;			// pack directly as u32

	C2.w = collParams.GetXf();
	C3.w = collParams.GetYf();

	pDest[0] = C0;
	pDest[1] = C1;
	pDest[2] = C2;
	pDest[3] = C3;
	pDest[4] = VEC4V_TO_VECTOR4(umParams);
#else
	const Vec4V vXYZ	= Vec4V(V_ONE_WZERO);	// 1,1,1, 0
	const Vec4V vW		= Vec4V(V_ZERO_WONE);	// 0,0,0, 1

	Mat44V M = MATRIX44_TO_MAT44V(pInstance->m_WorldMatrix);

	Vector4 groundColor_Ambient = pInstance->m_GroundColor;
	groundColor_Ambient.w = float(ambient)/255.0f;

	Vec4V C0 = M.GetCol0()*vXYZ + ScalarV(PackColour(pInstance->m_PlantColor))*vW;
	Vec4V C1 = M.GetCol1()*vXYZ + ScalarV(PackColour(groundColor_Ambient))*vW;
	Vec4V C2 = M.GetCol2()*vXYZ + collParams.GetX()*vW;
	Vec4V C3 = M.GetCol3()*vXYZ + collParams.GetY()*vW;

	pDest[0] = VEC4V_TO_VECTOR4(C0);
	pDest[1] = VEC4V_TO_VECTOR4(C1);
	pDest[2] = VEC4V_TO_VECTOR4(C2);
	pDest[3] = VEC4V_TO_VECTOR4(C3);
	pDest[4] = VEC4V_TO_VECTOR4(umParams);
#endif

	// Increment the count.
	m_PerThreadData0[rti].m_noOfInstances++;

	return(true);	// succcess
}


void CGrassRenderer::InstanceBucket::Flush(u8 rti)
{
	if(m_PerThreadData0[rti].m_noOfInstances)
	{
		AssertMsg(m_PerThreadData0[rti].m_isInstanceTextureLocked, "CGrassRenderer::InstanceBucket::Flush(): Instance texture should be locked.");
		if (m_PerThreadData0[rti].m_isInstanceTextureLocked)
			UnlockTransport();
		m_PerThreadData0[rti].m_isInstanceTextureLocked = false;

		// Set the textures.
		m_pShader->SetVar(m_textureVar, m_PerThreadData0[rti].m_pCurrentTexture);
	#if GRASS_INSTANCING_TRANSPORT_TEXTURE
		BindTransport();
	#endif // GRASS_INSTANCING_TRANSPORT_TEXTURE

		// Bind the technique/shader.
		CGrassRenderer::BindPlantShader(m_PerThreadData0[rti].m_techId);

		// Set up the geometry etc.
		grcIndexBuffer* pIndexBuffer			= m_PerThreadData0[rti].m_pCurrentGeometry->GetIndexBuffer(true);
		grcVertexBuffer* pVertexBuffer			= m_PerThreadData0[rti].m_pCurrentGeometry->GetVertexBuffer(true);
		grcVertexDeclaration* vertexDeclaration	= m_PerThreadData0[rti].m_pCurrentGeometry->GetDecl();

		// Render the instances.
		GRCDEVICE.SetVertexDeclaration(vertexDeclaration);
		GRCDEVICE.SetIndices(*pIndexBuffer);
		GRCDEVICE.SetStreamSource(0, *pVertexBuffer, 0, pVertexBuffer->GetVertexStride());

	#if RSG_DURANGO || RSG_ORBIS
		GRCDEVICE.SetUpPriorToDraw((grcDrawMode)m_PerThreadData0[rti].m_pCurrentGeometry->GetPrimitiveType());
	#endif // RSG_DURANGO || RSG_ORBIS

	#if GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS
		BindTransport(rti);
	#endif // GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS

	#if __D3D11
		const bool alreadySetupPriorToDraw = RSG_DURANGO;
		GRCDEVICE.DrawInstancedIndexedPrimitive((grcDrawMode)m_PerThreadData0[rti].m_pCurrentGeometry->GetPrimitiveType(), m_PerThreadData0[rti].m_pCurrentGeometry->GetIndexBuffer(true)->GetIndexCount(), m_PerThreadData0[rti].m_noOfInstances, 0, 0, 0, alreadySetupPriorToDraw);
	#elif RSG_ORBIS
		gfxc.setNumInstances(m_PerThreadData0[rti].m_noOfInstances);
		if (GRCDEVICE.NotifyDrawCall(1))
			gfxc.drawIndex(pIndexBuffer->GetIndexCount(), pIndexBuffer->GetUnsafeReadPtr());
		gfxc.setNumInstances(1);
	#endif

		// Unbind.
		UnbindTransport();
		CGrassRenderer::UnBindPlantShader();

		m_PerThreadData0[rti].m_noOfInstances = 0;
	}
}
#else //PLANTSMGR_MULTI_RENDER...
void CGrassRenderer::InstanceBucket::Init(grmShader *pShader, grcEffectVar textureVar)
{
	int i;
	// Record shader details.
	m_pShader = pShader;
	m_textureVar = textureVar;

	for(i=0; i<NUMBER_OF_RENDER_THREADS; i++)
	{
		m_PerThreadData[i].m_isInstanceTextureLocked = false;
	}

	InitialiseTransport();
}


void CGrassRenderer::InstanceBucket::ShutDown()
{
	CleanUpTransport();
}


void CGrassRenderer::InstanceBucket::Reset(grcEffectTechnique techId)
{
	AssertMsg((m_PerThreadData[g_RenderThreadIndex].m_isInstanceTextureLocked == false), "CGrassRenderer::InstanceBucket::Reset()...Intance texture should not be locked.");
	m_PerThreadData[g_RenderThreadIndex].m_techId = techId;
	m_PerThreadData[g_RenderThreadIndex].m_noOfInstances = 0;
	m_PerThreadData[g_RenderThreadIndex].m_pCurrentGeometry = NULL;
	m_PerThreadData[g_RenderThreadIndex].m_pCurrentTexture = NULL;
}


void CGrassRenderer::InstanceBucket::AddInstance(grmGeometry *pGeometry, grcTexture *pTexture, struct instGrassDataDrawStruct *pInstance, const Vec4V &umParams, const Vec2V &collParams)
{
	// Have we reached capacity or is the geometry/texture different ?
	if(IsFull() || (pGeometry != m_PerThreadData[g_RenderThreadIndex].m_pCurrentGeometry) || (pTexture != m_PerThreadData[g_RenderThreadIndex].m_pCurrentTexture))
	{
		// Send all instances.
		Flush();

		// Record texture/geometry.
		m_PerThreadData[g_RenderThreadIndex].m_pCurrentTexture = pTexture;
		m_PerThreadData[g_RenderThreadIndex].m_pCurrentGeometry = pGeometry;
	}


	if(!m_PerThreadData[g_RenderThreadIndex].m_isInstanceTextureLocked)
	{
		if (LockTransport())
			m_PerThreadData[g_RenderThreadIndex].m_isInstanceTextureLocked = true;
	}

	Vector4 *pDest = GetDataPtr();

	// Pack the instance data.
#if 1
	Vector4 C0 = pInstance->m_WorldMatrix.a;
	Vector4 C1 = pInstance->m_WorldMatrix.b;
	Vector4 C2 = pInstance->m_WorldMatrix.c;
	Vector4 C3 = pInstance->m_WorldMatrix.d;


	*((Color32*)(&C0.w)) = pInstance->m_PlantColor32;					// pack directly as u32
	*((Color32*)(&C1.w)) = pInstance->m_GroundColorAmbient32;			// pack directly as u32

	C2.w = collParams.GetXf();
	C3.w = collParams.GetYf();

	pDest[0] = C0;
	pDest[1] = C1;
	pDest[2] = C2;
	pDest[3] = C3;
	pDest[4] = VEC4V_TO_VECTOR4(umParams);
#else
	const Vec4V vXYZ	= Vec4V(V_ONE_WZERO);	// 1,1,1, 0
	const Vec4V vW		= Vec4V(V_ZERO_WONE);	// 0,0,0, 1

	Mat44V M = MATRIX44_TO_MAT44V(pInstance->m_WorldMatrix);

	Vector4 groundColor_Ambient = pInstance->m_GroundColor;
	groundColor_Ambient.w = float(ambient)/255.0f;

	Vec4V C0 = M.GetCol0()*vXYZ + ScalarV(PackColour(pInstance->m_PlantColor))*vW;
	Vec4V C1 = M.GetCol1()*vXYZ + ScalarV(PackColour(groundColor_Ambient))*vW;
	Vec4V C2 = M.GetCol2()*vXYZ + collParams.GetX()*vW;
	Vec4V C3 = M.GetCol3()*vXYZ + collParams.GetY()*vW;

	pDest[0] = VEC4V_TO_VECTOR4(C0);
	pDest[1] = VEC4V_TO_VECTOR4(C1);
	pDest[2] = VEC4V_TO_VECTOR4(C2);
	pDest[3] = VEC4V_TO_VECTOR4(C3);
	pDest[4] = VEC4V_TO_VECTOR4(umParams);
#endif

	// Increment the count.
	m_PerThreadData[g_RenderThreadIndex].m_noOfInstances++;
}


void CGrassRenderer::InstanceBucket::Flush()
{
	if(m_PerThreadData[g_RenderThreadIndex].m_noOfInstances)
	{
		AssertMsg(m_PerThreadData[g_RenderThreadIndex].m_isInstanceTextureLocked, "CGrassRenderer::SendTriPlantInstances_Indexed()...Instance texture should be locked.");
		if (m_PerThreadData[g_RenderThreadIndex].m_isInstanceTextureLocked)
			UnlockTransport();
		m_PerThreadData[g_RenderThreadIndex].m_isInstanceTextureLocked = false;

		// Set the textures.
		m_pShader->SetVar(m_textureVar, m_PerThreadData[g_RenderThreadIndex].m_pCurrentTexture);
	#if GRASS_INSTANCING_TRANSPORT_TEXTURE
		BindTransport();
	#endif // GRASS_INSTANCING_TRANSPORT_TEXTURE

		// Bind the technique/shader.
		CGrassRenderer::BindPlantShader(m_PerThreadData[g_RenderThreadIndex].m_techId);

		// Set up the geometry etc.
		grcIndexBuffer* pIndexBuffer			= m_PerThreadData[g_RenderThreadIndex].m_pCurrentGeometry->GetIndexBuffer(true);
		grcVertexBuffer* pVertexBuffer			= m_PerThreadData[g_RenderThreadIndex].m_pCurrentGeometry->GetVertexBuffer(true);
		grcVertexDeclaration* vertexDeclaration	= m_PerThreadData[g_RenderThreadIndex].m_pCurrentGeometry->GetDecl();

		// Render the instances.
		GRCDEVICE.SetVertexDeclaration(vertexDeclaration);
		GRCDEVICE.SetIndices(*pIndexBuffer);
		GRCDEVICE.SetStreamSource(0, *pVertexBuffer, 0, pVertexBuffer->GetVertexStride());

	#if RSG_DURANGO || RSG_ORBIS
		GRCDEVICE.SetUpPriorToDraw((grcDrawMode)m_PerThreadData[g_RenderThreadIndex].m_pCurrentGeometry->GetPrimitiveType());
	#endif // RSG_DURANGO || RSG_ORBIS

	#if GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS
		BindTransport();
	#endif // GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS

	#if __D3D11
		const bool alreadySetupPriorToDraw = RSG_DURANGO;
		GRCDEVICE.DrawInstancedIndexedPrimitive((grcDrawMode)m_PerThreadData[g_RenderThreadIndex].m_pCurrentGeometry->GetPrimitiveType(), m_PerThreadData[g_RenderThreadIndex].m_pCurrentGeometry->GetIndexBuffer(true)->GetIndexCount(), m_PerThreadData[g_RenderThreadIndex].m_noOfInstances, 0, 0, 0, alreadySetupPriorToDraw);
	#elif RSG_ORBIS
		gfxc.setNumInstances(m_PerThreadData[g_RenderThreadIndex].m_noOfInstances);
		if (GRCDEVICE.NotifyDrawCall(1))
			gfxc.drawIndex(pIndexBuffer->GetIndexCount(), pIndexBuffer->GetUnsafeReadPtr());
		gfxc.setNumInstances(1);
	#endif

		// Unbind.
		UnbindTransport();
		CGrassRenderer::UnBindPlantShader();

		m_PerThreadData[g_RenderThreadIndex].m_noOfInstances = 0;
	}
	else
	{
	#if NEW_STORE_OPT
		// no of instances=0, but still locked because buckets are locked upfront:
		m_PerThreadData[g_RenderThreadIndex].m_isInstanceTextureLocked = false;
	#endif
	}
}
#endif//PLANTSMGR_MULTI_RENDER...

float CGrassRenderer::InstanceBucket::PackColour(Vector4 &c)
{
	Vector4 cStar = c*255.0f;
	cStar.RoundToNearestIntZero(); 
	return 256.0f*256.0f*256.0f*cStar.w + 256.0f*256.0f*cStar.x + 256.0f*cStar.y + cStar.z;
}


#if GRASS_INSTANCING_TRANSPORT_TEXTURE

void CGrassRenderer::InstanceBucket::InitialiseTransport()
{
	grcTextureFactory::TextureCreateParams TextureParams(grcTextureFactory::TextureCreateParams::VIDEO, 
		grcTextureFactory::TextureCreateParams::LINEAR,	grcsWrite /*| grcsRead*/, NULL, 
		grcTextureFactory::TextureCreateParams::NORMAL, 
		grcTextureFactory::TextureCreateParams::MSAA_NONE);

	m_pInstanceTexture = grcTextureFactory::GetInstance().Create((0x1 << GRASS_INSTANCING_TEXTURE_WIDTH_IN_INSTANCES_POWER_OF_2)*GRASS_INSTANCING_INSTANCE_SIZE, 
		GRASS_INSTANCING_TEXTURE_HEIGHT_IN_INSTANCES, grctfA32B32G32R32F, NULL,  1U /*numMips*/, &TextureParams);

	m_instanceTextureVar = ms_Shader->LookupVar("instanceTexture0", TRUE);
}


void CGrassRenderer::InstanceBucket::CleanUpTransport()
{
	if(m_pInstanceTexture)
	{
		m_pInstanceTexture->Release();
		m_pInstanceTexture = NULL;
	}
}


bool CGrassRenderer::InstanceBucket::LockTransport()
{
	PF_AUTO_PUSH_TIMEBAR_BUDGETED("Grass Instance->LockRect", 4.0f);
	// Lock the instance texture if needs be.
	return m_pInstanceTexture->LockRect(0, 0, m_instanceTextureLock, grcsWrite | grcsNoOverwrite);
}


void CGrassRenderer::InstanceBucket::UnlockTransport()
{
	m_pInstanceTexture->UnlockRect(m_instanceTextureLock);
}


Vector4 *CGrassRenderer::InstanceBucket::GetDataPtr()
{
	// "Index" into the instance texture.
	char *pTexels = (char *)m_instanceTextureLock.Base;
	u32 x = m_noOfInstances & ((0x1 << GRASS_INSTANCING_TEXTURE_WIDTH_IN_INSTANCES_POWER_OF_2) - 1);
	u32 y = m_noOfInstances >> GRASS_INSTANCING_TEXTURE_WIDTH_IN_INSTANCES_POWER_OF_2;
	Vector4 *pDest = (Vector4 *)&pTexels[y*m_instanceTextureLock.Pitch + x*sizeof(Vector4)*GRASS_INSTANCING_INSTANCE_SIZE];
	return pDest;
}


void CGrassRenderer::InstanceBucket::BindTransport()
{
	m_pShader->SetVar(m_instanceTextureVar, m_pInstanceTexture);
}


void CGrassRenderer::InstanceBucket::UnbindTransport()
{
}

#endif // GRASS_INSTANCING_TRANSPORT_TEXTURE

#if GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS

void CGrassRenderer::InstanceBucket::InitialiseTransport()
{
	grcEffectGlobalVar var = grcEffect::LookupGlobalVar("instanceData0");
	grcCBuffer *pBuff = grcEffect::GetGlobalParentCBuf(var);
	m_VertexShaderRegister = pBuff->GetRegister(rage::VS_TYPE);

	// Must be less than 4096 bytes.
	m_pCBuffer = rage_new rage::grcCBuffer(GRASS_INSTANCING_BUCKET_SIZE*GRASS_INSTANCING_INSTANCE_SIZE*sizeof(Vector4), true);

	// Set the registers/slots the constant buffer is bound to.
	u16 registers[NONE_TYPE] = { 0 };
	registers[VS_TYPE] = (u16)pBuff->GetRegister(rage::VS_TYPE);
	registers[PS_TYPE] = (u16)pBuff->GetRegister(rage::PS_TYPE);
	m_pCBuffer->SetRegisters(registers);

	for(int i=0; i<NUMBER_OF_RENDER_THREADS; i++)
	{
#if PLANTSMGR_MULTI_RENDER
		m_pLockBase0[i] = NULL;
		m_pLockBase0[i] = (Vector4*)rage_new char[m_pCBuffer->GetSize()];
#else
		m_pLockBase[i] = NULL;
		m_pLockBase[i] = (Vector4*)rage_new char[m_pCBuffer->GetSize()];
#endif
	}

}


void CGrassRenderer::InstanceBucket::CleanUpTransport()
{
	if(m_pCBuffer)
	{
		delete m_pCBuffer;
		m_pCBuffer = NULL;
	}

	for(int i=0; i<NUMBER_OF_RENDER_THREADS; i++)
	{
#if PLANTSMGR_MULTI_RENDER
		if(m_pLockBase0[i])
		{
			delete [] m_pLockBase0[i];
			m_pLockBase0[i] = NULL;
		}
#else
		if(m_pLockBase[i])
		{
			delete [] m_pLockBase[i];
			m_pLockBase[i] = NULL;
		}
#endif
	}
}


#if PLANTSMGR_MULTI_RENDER
void CGrassRenderer::InstanceBucket::BindTransport(u8 rti)
{
	// This is during an IsInDraw() session, so effect_gnm.cpp will bind the buffer.
	void *pDest = m_pCBuffer->BeginUpdate(m_PerThreadData0[rti].m_noOfInstances*GRASS_INSTANCING_INSTANCE_SIZE*sizeof(Vector4));
	sysMemCpy(pDest, m_pLockBase0[rti], m_PerThreadData0[rti].m_noOfInstances*GRASS_INSTANCING_INSTANCE_SIZE*sizeof(Vector4));
	m_pCBuffer->EndUpdate();

#if __D3D11
	GRCDEVICE.SetVertexShaderConstantBufferOverrides(m_VertexShaderRegister, &m_pCBuffer, 1);
#endif // __D3D11
}
#else //PLANTSMGR_MULTI_RENDER...
void CGrassRenderer::InstanceBucket::BindTransport()
{
	// This is during an IsInDraw() session, so effect_gnm.cpp will bind the buffer.
	void *pDest = m_pCBuffer->BeginUpdate(m_PerThreadData[g_RenderThreadIndex].m_noOfInstances*GRASS_INSTANCING_INSTANCE_SIZE*sizeof(Vector4));
	sysMemCpy(pDest, m_pLockBase[g_RenderThreadIndex], m_PerThreadData[g_RenderThreadIndex].m_noOfInstances*GRASS_INSTANCING_INSTANCE_SIZE*sizeof(Vector4));
	m_pCBuffer->EndUpdate();

#if RSG_PC && __D3D11
	GRCDEVICE.SetVertexShaderConstantBufferOverrides(m_VertexShaderRegister, &m_pCBuffer, 1);
#endif
}
#endif //PLANTSMGR_MULTI_RENDER...

#endif // GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS


///////////////////////////////////////////////////////////////////////////////////////////////////////


CGrassRenderer::InstanceBucket_LOD2::InstanceBucket_LOD2() : InstanceBucket()
{

}

CGrassRenderer::InstanceBucket_LOD2::~InstanceBucket_LOD2()
{

}

#if PLANTSMGR_MULTI_RENDER
void CGrassRenderer::InstanceBucket_LOD2::Flush(u8 rti)
{
	if(m_PerThreadData0[rti].m_noOfInstances)
	{
		// Unlock the instance texture.
		AssertMsg(m_PerThreadData0[rti].m_isInstanceTextureLocked, "CGrassRenderer::InstanceBucket_LOD2::Flush(): Instance texture should be locked.");
		if (m_PerThreadData0[rti].m_isInstanceTextureLocked)
			UnlockTransport();
		m_PerThreadData0[rti].m_isInstanceTextureLocked = false;

		// Set the textures.
		m_pShader->SetVar(m_textureVar, m_PerThreadData0[rti].m_pCurrentTexture);
	#if GRASS_INSTANCING_TRANSPORT_TEXTURE
		BindTransport();
	#endif // GRASS_INSTANCING_TRANSPORT_TEXTURE

		// Bind the technique/shader.
		CGrassRenderer::BindPlantShader(m_PerThreadData0[rti].m_techId);

		// Set up the geometry etc.
		grcVertexBuffer *pVertexBuffer			= ms_plantLOD2VertexBuffer;
		grcVertexDeclaration *vertexDeclaration	= ms_plantLOD2VertexDecl;

		// Render the instances.
		GRCDEVICE.SetVertexDeclaration(vertexDeclaration);
		GRCDEVICE.SetStreamSource(0, *pVertexBuffer, 0, pVertexBuffer->GetVertexStride());

	#if RSG_DURANGO || RSG_ORBIS
		GRCDEVICE.SetUpPriorToDraw(drawTriStrip);
	#endif // RSG_DURANGO || RSG_ORBIS

	#if GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS
		BindTransport(rti);
	#endif // GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS

	#if __D3D11
		const bool alreadySetupPriorToDraw = RSG_DURANGO;
		GRCDEVICE.DrawInstancedPrimitive(drawTriStrip, 4, m_PerThreadData0[rti].m_noOfInstances, 0, 0, alreadySetupPriorToDraw);
	#elif RSG_ORBIS
		gfxc.setNumInstances(m_PerThreadData0[rti].m_noOfInstances);
		if (GRCDEVICE.NotifyDrawCall(1))
			gfxc.drawIndexAuto(4);
		gfxc.setNumInstances(1);
	#endif

		// Unbind.
		UnbindTransport();
		CGrassRenderer::UnBindPlantShader();

		m_PerThreadData0[rti].m_noOfInstances = 0;
	}
}
#else //PLANTSMGR_MULTI_RENDER...
void CGrassRenderer::InstanceBucket_LOD2::Flush()
{
	if(m_PerThreadData[g_RenderThreadIndex].m_noOfInstances)
	{
		// Unlock the instance texture.
		AssertMsg(m_PerThreadData[g_RenderThreadIndex].m_isInstanceTextureLocked, "CGrassRenderer::SendTriPlantInstances_Indexed()...Instance texture should be locked.");
		if (m_PerThreadData[g_RenderThreadIndex].m_isInstanceTextureLocked)
			UnlockTransport();
		m_PerThreadData[g_RenderThreadIndex].m_isInstanceTextureLocked = false;

		// Set the textures.
		m_pShader->SetVar(m_textureVar, m_PerThreadData[g_RenderThreadIndex].m_pCurrentTexture);
	#if GRASS_INSTANCING_TRANSPORT_TEXTURE
		BindTransport();
	#endif // GRASS_INSTANCING_TRANSPORT_TEXTURE

		// Bind the technique/shader.
		CGrassRenderer::BindPlantShader(m_PerThreadData[g_RenderThreadIndex].m_techId);

		// Set up the geometry etc.
		grcVertexBuffer *pVertexBuffer			= ms_plantLOD2VertexBuffer;
		grcVertexDeclaration *vertexDeclaration	= ms_plantLOD2VertexDecl;

		// Render the instances.
		GRCDEVICE.SetVertexDeclaration(vertexDeclaration);
		GRCDEVICE.SetStreamSource(0, *pVertexBuffer, 0, pVertexBuffer->GetVertexStride());

	#if RSG_DURANGO || RSG_ORBIS
		GRCDEVICE.SetUpPriorToDraw(drawTriStrip);
	#endif // RSG_DURANGO || RSG_ORBIS

	#if GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS
		BindTransport();
	#endif // GRASS_INSTANCING_TRANSPORT_CONSTANT_BUFFERS

	#if __D3D11
		const bool alreadySetupPriorToDraw = RSG_DURANGO;
		GRCDEVICE.DrawInstancedPrimitive(drawTriStrip, 4, m_PerThreadData[g_RenderThreadIndex].m_noOfInstances, 0, 0, alreadySetupPriorToDraw);
	#elif RSG_ORBIS
		gfxc.setNumInstances(m_PerThreadData[g_RenderThreadIndex].m_noOfInstances);
		if (GRCDEVICE.NotifyDrawCall(1))
			gfxc.drawIndexAuto(4);
		gfxc.setNumInstances(1);
	#endif

		// Unbind.
		UnbindTransport();
		CGrassRenderer::UnBindPlantShader();

		m_PerThreadData[g_RenderThreadIndex].m_noOfInstances = 0;
	}
	else
	{
	#if NEW_STORE_OPT
		// no of instances=0, but still locked because buckets are locked upfront:
		m_PerThreadData[g_RenderThreadIndex].m_isInstanceTextureLocked = false;
	#endif
	}
}
#endif//PLANTSMGR_MULTI_RENDER...


#endif // GRASS_INSTANCING



