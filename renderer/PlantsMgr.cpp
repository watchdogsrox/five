//
//
//	CPlantManager	- management of plants seeded (generated) around camera;
//					- global manager responsible for processing & drawing plants all over game map;
//
//
//	24/06/2005		- Andrzej:	- initial conversion to Rage from SA code;
//	14/07/2005		- Andrzej:	- port to Rage fully working;
//	05/10/2005		- Andrzej:	- support for collision quads (4 verts) added
//								  (so far only collision tris (3 verts) were supported);
//	18/10/2006		- Andrzej:	- HDR multiply added;
//	28/02/2007		- Andrzej:	- ColBoundCache: removed references to phBounds, everything is taken from CEntity now;
//	24/05/2007		- Andrzej:	- support for SPU renderer added;
//	10/10/2007		- Matt:		- support for SPU update added;
//	28/02/2008		- Andrzej:	- CPLANTMGR_ENABLED: if 0, then disables loading of models & textures and Render()
//								  Update() still works (as its results are required for procedural objects);
//	----------		- --------	----------------------------------------------------------------------------------------
//	08/07/2008		- Andrzej:	- PlantsMgr 2.0: code cleanup, refactor;
//	16/07/2008		- Andrzej:	- PlantsMgr 2.0: micro-movements support added (local micro-movements scales are encoded in vertex colors);
//	22/10/2008		- Andrzej:	- PlantsMgr 2.0: local per-vartex variation data support (ground color+scaleXY/Z) added;
//	28/10/2008		- Andrzej:	- PlantsMgr 2.0: LOD0 and LOD1 support added;
//	06/11/2008		- Andrzej:	- PlantsMgr 2.0: PSN: added two-pass rendering into GBuffer to allow to use hardware AlphaToMask() in GBuf0;
//	14/11/2008		- Andrzej:	- PlantsMgr 2.0: PSN: revitalised old SPU renderloop;
//	26/11/2008		- Andrzej:	- PlantsMgr 2.0: SpuRenderloop 2.0 and code cleanup (grassModelSPU4 removed, etc.);
//	16/12/2008		- Andrzej:	- PlantsMgr 2.0: ground slope skewing added;
//	04/02/2011		- Andrzej:	- PlantsMgr 2.0: SPU render buffers allocated from streaming heap;
//	10/02/2011		- Andrzej:	- Fur grass v4;
//	14/04/2011		- Andrzej:	- PlantsMgr 2.0: Update buffers allocated from streaming heap;
//	18/07/2011		- Andrzej:	- PlantsMgr 2.0: new layout of ground per-vertex Density/ScaleZ/ScaleXYZ;
//  ----------
//  22/10/2013		- Andrzej:	- PlantsMgr 3.0: MATERIAL type collision for NG;
//
//
//
//

// Rage headers:
#include "grcore/debugdraw.h"
#include "fwmaths/vector.h"
#include "physics/levelnew.h"
#include "physics/inst.h"
#include "phbound/bound.h"
#include "phbound/boundgeom.h"
#include "phbound/boundbox.h"
#include "grmodel/geometry.h"
#include "grcore/allocscope.h"
#include "grcore/indexbuffer.h"
#include "grcore/vertexbuffer.h"
#include "grcore/vertexbuffereditor.h"
#include "grmodel/modelfactory.h"
#include "pheffects/wind.h"
#include "profile/timebars.h"
#include "grprofile/pix.h"
#include "system/xtl.h"
#include "fwscene/scan/scan.h"
#include "fwscene/stores/txdstore.h"
#include "system/findsize.h"
#include "system/dependencyscheduler.h"
#include "streaming/packfilemanager.h"
#include "vector/geometry.h"

// gta headers:
#include "camera/caminterface.h"
#include "camera/viewports/ViewportManager.h"
#include "camera/base/BaseCamera.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/aim/FirstPersonAimCamera.h"
#include "camera/cinematic/CinematicDirector.h"
#include "core/game.h"
#include "scene/world/gameworld.h"
#include "physics/gtamaterialmanager.h"
#include "peds/PopCycle.h"		// GetCurrentZonePlantsMgrTxdIdx()
#include "physics/iterator.h"
#include "physics/physics.h"
#include "debug/debug.h"
#include "debug/TiledScreenCapture.h"
#include "peds/ped.h"		// findplayerped()...
#include "objects/procobjects.h"	
#include "game/weather.h"		// g_weather.wind()...
#include "game/ModelIndices.h"
#include "scene/ContinuityMgr.h"
#include "scene/InstancePriority.h"
#include "scene/refmacros.h"
#include "shaders/shaderlib.h"
#include "system/taskscheduler.h"
#include "renderer/deferred/deferredlighting.h"
#include "renderer/deferred/gbuffer.h"
#include "renderer/plantsgrassrenderer.h"
#include "renderer/Water.h"
#include "vehicles/vehicle.h"
#include "vehicles/Submarine.h"
#include "game/clock.h"
#include "renderer/lights/lights.h"
#include "renderer/sprite2d.h"
#include "renderer/gtadrawable.h"
#include "streaming/streamingrequest.h"
#include "renderer/PlantsMgr.h"
#include "system/SettingsManager.h"
#include "../shader_source/Vegetation/Grass/grass_regs.h"

#if __BANK
#include "audio/northaudioengine.h"
#endif

CPLANTLOCTRI_PV_MAP_ARRAYS		// CPlantLocTri::ms_pvMap/DensityScaleZ
#include "streaming/streaming.h"
#include "debug/DebugScene.h"

#if !__SPU
	RAGE_DEFINE_SUBCHANNEL(render, plants)
#endif

#if PSN_PLANTSMGR_SPU_RENDER
	#include "PlantsGrassRendererSPU.h"		// grassModelSPU
#endif //PSN_PLANTSMGR_SPU_RENDER...

#define PLANTS_TXD_STR				(__PS3 || __XENON)	// enable dynamic streaming of plant txds
#define LOCTRISTAB_STR				(1)					// enable dynamic allocation of work buffers from str memory

RENDER_OPTIMISATIONS();

#define PSN_ALPHATOMASK_PASS		(1 && __PPU && !CPLANT_WRITE_GRASS_NORMAL)
#define EXTRA_1_5_PASS				(0)			// experimental: does extra fullscreen passes to eliminate selfshadowing
#define XENON_FILL_PASS				(1 && __XENON)

#define USE_SSA_STIPPLE_ALPHA		(0)
#define WRITE_SELF_SHADOW_TERM		(1)

// RDR3: Temp Fix for a GCM fault. Seems grass rendering might leave GPU in bad state/cascaded shadows don't properly reset GPU state. This resets GPU state to avoid a crash.
#define PSN_CLEANUP_PASS			(HACK_RDR3 && __PPU && !PSN_ALPHATOMASK_PASS)

#define PLANT_DEFAULT_WIND_RESPONSE_AMOUNT		(0.750f)
#define PLANT_DEFAULT_WIND_RESPONSE_VEL_MIN		(1.500f)
#define PLANT_DEFAULT_WIND_RESPONSE_VEL_MAX		(22.000f)
#define PLANT_DEFAULT_WIND_RESPONSE_VEL_EXP		(0.0f)

static sysDependency	g_PlantMgrMemcpyDependency[CPLANT_LOC_TRIS_LIST_NUM];
static u32				g_PlantMgrMemcpyDependenciesRunning;

static BankFloat g_AvgPlantWindInfluence		= 0.970f;

#define OBJ_CREATION_SPEED_LIMIT				(25.0f) 

// per-vertex ground color (RGB) + density/scaleZ/scaleXYZ weights(A)
const Color32 DEFAULT_GROUND_COLOR	= Color32(111, 110, 92, CPLANT_PV_DEFAULT_D_SZ_SXYZ);

// CPLANT_BLIT_MIN_GBUFFER and CPLANT_STORE_LOCTRI_NORMAL may not be enabled at the same time
CompileTimeAssert( !(CPLANT_BLIT_MIN_GBUFFER && CPLANT_STORE_LOCTRI_NORMAL) );

PARAM(DisablePlantMgr, "Disable Plant Manager");

#if __BANK
	#include "file/asset.h"
	#include "bank/Bank.h"
	#include "bank/BkMgr.h"
	bool			gbDisplayCPlantMgrInfo		= FALSE;
	static bool		gbShowCPlantMgrPolys 		= FALSE;
	static bool		gbShowCPlantMgrPolysFar		= FALSE;
	static bool		gbShowCPlantMgrPolys_Plants	= TRUE;
	static bool		gbShowCPlantMgrPolys_ProcObj= TRUE;
	static bool		gbShowCPlantMgrProcMatType	= FALSE;
	static bool		gbShowCPlantMgrAmbScale		= FALSE;
	bool			gbShowAlphaOverdraw			= FALSE;
	static bool		gbShowCPlantMgrPolyNormals	= FALSE;
	static bool		gbShowCPlantMgrGroundColor	= FALSE;
	static u32		gbShowCPlantMgrGroundCSD	= 0;	// 0=color, 1=scaleXYZ, 2=scaleZ, 3=density

	static bool		gbForceDefaultGroundColor	= FALSE;
	static Color32	gbDefaultGroundColor		= DEFAULT_GROUND_COLOR;
	static bool		gbForceGroundColor			= FALSE;
	static bool		gbChangeGroundColorsAllAsOne= FALSE;
	static Color32	gbGroundColorV1(255,0,  0,  255);
	static Color32	gbGroundColorV2(0,  255,0,  255);
	static Color32	gbGroundColorV3(0,  0,  255,255);
	static u32		gbGroundScaleXYZ_V1=0x0f,	gbGroundScaleXYZ_V2=0x0f,	gbGroundScaleXYZ_V3=0x0f; 
	static u32		gbGroundScaleZ_V1=0x00,		gbGroundScaleZ_V2=0x00,		gbGroundScaleZ_V3=0x00; 
	static u32		gbGroundDensity_V1=0x00,	gbGroundDensity_V2=0x00,	gbGroundDensity_V3=0x00;
#if PSN_PLANTSMGR_SPU_UPDATE
	static bool		gbPrintUpdateSpuJobTimings	= FALSE;
#endif

	static void cbBankChangeGroundColorV1()
	{
		if(gbChangeGroundColorsAllAsOne)
		{
			gbGroundColorV2 = gbGroundColorV3 = gbGroundColorV1;
		}
	}

	static void cbBankChangeGroundColorV2()
	{
		if(gbChangeGroundColorsAllAsOne)
		{
			gbGroundColorV1 = gbGroundColorV3 = gbGroundColorV2;
		}
	}

	static void cbBankChangeGroundColorV3()
	{
		if(gbChangeGroundColorsAllAsOne)
		{
			gbGroundColorV1 = gbGroundColorV2 = gbGroundColorV3;
		}
	}


	bool	gbPlantsFlashLOD0	= FALSE;
	bool	gbPlantsFlashLOD1	= FALSE;
	bool	gbPlantsFlashLOD2	= FALSE;
	bool	gbPlantsDisableLOD0 = FALSE;
	bool	gbPlantsDisableLOD1 = FALSE;
	bool	gbPlantsDisableLOD2 = FALSE;

	// loctris far distance:
	float	gbPlantsLocTriFarDist				= CPLANT_TRILOC_FAR_DIST_INITVAL;
	float	CPlantMgr::ms_bkColTestRadius		= CPLANTMGR_COL_TEST_RADIUS_INITVAL;
	float	CPlantMgr::ms_bkTriLocFarDist		= CPLANT_TRILOC_FAR_DIST_INITVAL;
	float	CPlantMgr::ms_bkTriLocFarDistSqr	= CPLANT_TRILOC_FAR_DIST_INITVAL*CPLANT_TRILOC_FAR_DIST_INITVAL;
	float	CPlantMgr::ms_bkTriLocShortFarDist	= CPLANT_TRILOC_SHORT_FAR_DIST_INITVAL;
	float	CPlantMgr::ms_bkTriLocShortFarDistSqr=CPLANT_TRILOC_SHORT_FAR_DIST_INITVAL*CPLANT_TRILOC_SHORT_FAR_DIST_INITVAL;

	bool	CPlantMgr::ms_bkbWindResponseEnabled= true;
	float	CPlantMgr::ms_bkfWindResponseAmount	= PLANT_DEFAULT_WIND_RESPONSE_AMOUNT;
	float	CPlantMgr::ms_bkfWindResponseVelMin	= PLANT_DEFAULT_WIND_RESPONSE_VEL_MIN;
	float	CPlantMgr::ms_bkfWindResponseVelMax	= PLANT_DEFAULT_WIND_RESPONSE_VEL_MAX;
	float	CPlantMgr::ms_bkfWindResponseVelExp	= PLANT_DEFAULT_WIND_RESPONSE_VEL_EXP;
#endif // __BANK

Vector2 CPlantMgr::ms_AvgWindVector				= Vector2(0.0f, 0.0f);
bool	CPlantMgr::ms_bForceHDGrass				= false;

#if __BANK || PLANTS_USE_LOD_SETTINGS
	// LOD0:
	float	gbPlantsLOD0AlphaCloseDist		= CPLANT_LOD0_ALPHA_CLOSE_DIST;
	float	gbPlantsLOD0AlphaFarDist		= CPLANT_LOD0_ALPHA_FAR_DIST;
	float	gbPlantsLOD0FarDist				= CPLANT_LOD0_FAR_DIST;
	
	// LOD1:
	float	gbPlantsLOD1CloseDist			= CPLANT_LOD1_CLOSE_DIST;
	float	gbPlantsLOD1FarDist				= CPLANT_LOD1_FAR_DIST;
	float	gbPlantsLOD1Alpha0CloseDist		= CPLANT_LOD1_ALPHA0_CLOSE_DIST;
	float	gbPlantsLOD1Alpha0FarDist		= CPLANT_LOD1_ALPHA0_FAR_DIST;
	float	gbPlantsLOD1Alpha1CloseDist		= CPLANT_LOD1_ALPHA1_CLOSE_DIST;
	float	gbPlantsLOD1Alpha1FarDist		= CPLANT_LOD1_ALPHA1_FAR_DIST;

	// LOD2:
	float	gbPlantsLOD2CloseDist			= CPLANT_LOD2_CLOSE_DIST;
	float	gbPlantsLOD2FarDist				= CPLANT_LOD2_FAR_DIST;
	float	gbPlantsLOD2Alpha0CloseDist		= CPLANT_LOD2_ALPHA0_CLOSE_DIST;
	float	gbPlantsLOD2Alpha0FarDist		= CPLANT_LOD2_ALPHA0_FAR_DIST;
	float	gbPlantsLOD2Alpha1CloseDist		= CPLANT_LOD2_ALPHA1_CLOSE_DIST;
	float	gbPlantsLOD2Alpha1FarDist		= CPLANT_LOD2_ALPHA1_FAR_DIST;
	float	gbPlantsLOD2Alpha1CloseDistFar	= CPLANT_LOD2_ALPHA1_CLOSE_DIST_FAR;
	float	gbPlantsLOD2Alpha1FarDistFar	= CPLANT_LOD2_ALPHA1_FAR_DIST_FAR;

	float	gbPlantsGroundSlopeAngleMin		= CPLANT_GROUND_SLOPE_ANGLE_MIN;	
	bool	gbPlantsDarknessTimecycle		= TRUE;
	float	gbPlantsDarknessAmountAdd		= 0.0f;
	float	gbPlantsDarknessAmountPrint		= 0.0f;

	BANK_ONLY(static bool		gbDoubleSidedCulling	= FALSE);
	bool			gbPlantsGeometryCulling	= TRUE;
	bool			gbPlantsPremultiplyNdotL= TRUE;
	
	#if CPLANT_USE_OCCLUSION
		static bool		gbPlantsUseOcclusion	= TRUE;
		static float	gfPlantsOccCubeHeight	= 2.0f;
		static bool		gbPlantsOccRecalcAABBs	= FALSE;
		static s32		gnPlantsOccSkipTest		= 0;		// 0=test all, 1=test 50%, 2=test 33%, 3=test 25%
	#endif	

	BANK_ONLY(static bool bRenderInternalActive		= FALSE);
	#if FURGRASS_TEST_V4
	BANK_ONLY(static u32	 gnFurGrassNumPolys			= 0);		// debug stats only
	#endif

	void SetupPlantLods(CGrassRenderer_PlantLod plantLod);

#endif //__BANK...
#if FURGRASS_TEST_V4
	#define FURGRASS_USE_ALPHA_STIPPLE				(1)				// use alpha stipple for fading
	static bank_float	gfFurGrassMaxDist			= 30.0f;
	static bank_float	gfFurGrassAlphaFadeClose	= 10.0f;
	static bank_float	gfFurGrassAlphaFadeFar		= 25.0f;

	static bank_float	gfFurGrassFresnel			= 0.960f;
	static bank_float	gfFurGrassSpecFalloff		= 32.0f;
	static bank_float	gfFurGrassSpecInt			= 0.005f;
	static bank_float	gfFurGrassBumpiness			= 2.0000f;

	static bank_bool	gbFurGrassSeparateTextures		= true;
	static bank_s32		gnFurGrassSeparateTexturesTech	= 0;
	static bank_bool	gbEnableFurGrassMipmapLodBias	= false;
	static bank_float	gfFurGrassMipmapLodBias			= -1.0f;
	static bank_float	gfFurGrassUvScale			= 0.300f;
	static bank_float	gfFurGrassUvScale2			= 0.300f;
	static bank_float	gfFurGrassUvScale3			= 0.300f;
	static bank_float	gfFurGrassOffsetU			= 0.0f;
	static bank_float	gfFurGrassOffsetV			= 0.0f;
	static bank_u32		gnFurGrassUmScaleX			= 100;
	static bank_u32		gnFurGrassUmScaleY			= 100;
	static bank_u32		gnFurGrassFurStep			= 80;
	static bank_float	gfFurGrassZOffset			= 0.001f;
	static bank_u32		gnFurGrassNumLayers			= 8;
	static bank_u32		gnFurGrassAlphaClip[FURGRASS_MAX_LAYERS]	= {
													0,  10, 10, 15,
													20, 25, 30, 34
													};
	static bank_u8	gnFurGrassShadow[FURGRASS_MAX_LAYERS]	= {
													 90, 118, 148, 168,
													200, 220, 238, 255
													};
#endif //FURGRASS_TEST_V4...

	bool gbPlantMgrActive					= TRUE;

#if !__FINAL
	bool gbPlantMgrRenderActive				= TRUE;
#endif

#if CPLANT_CLIP_EDGE_VERT
	//Plant clipping
	static bank_bool gbPlantMgrClipEnable = false;	//Set this to true to enable edge/vert clipping. (Paying cost either way)
	static bank_bool gbPlantMgrEdgeClipEnable = true;
	static bank_bool gbPlantMgrVertClipEnable = true;
#endif //CPLANT_CLIP_EDGE_VERT...

#if CPLANT_DYNAMIC_CULL_SPHERES
//Dynamic Cull Spheres
static bank_bool gbPlantMgrDynCullSpheresEnable = true;
#if __BANK
	static bank_bool gbPlantMgrDCSDebugDraw = false;
	static bank_bool gbPlantMgrDCSDebugDrawSolid = false;
	static Color32 gPlantMgrDCSDebugDrawColor(0xFFFF0000);
	static Vec4V gPlantMgrDCSDbg_Sphere(0.0f, 0.0f, 0.0f, 2.0f); //(V_ZERO);
	static u32 gPlantMgrDCSDbg_Handle = 0;

	void PlantMgrDCSDbg_AddSphere()
	{
		u32 handle = gPlantMgr.AddDynamicCullSphere(spdSphere(gPlantMgrDCSDbg_Sphere));
		if(handle != PLANTSMGR_INVALID_DYNAMIC_CULL_SPHERE_INDEX)
			gPlantMgrDCSDbg_Handle = handle;
	}

	void PlantMgrDCSDbg_RemoveSphere()
	{
		gPlantMgr.RemoveDynamicCullSphere(gPlantMgrDCSDbg_Handle);
	}

	void PlantMgrDCSDbg_CenterSphereOnPlayerPed()
	{
		if(CPed *pPed = FindPlayerPed())
		{
			gPlantMgrDCSDbg_Sphere.SetXYZ(pPed->GetTransform().GetPosition());
		}
	}
#endif //__BANK...
#endif //CPLANT_DYNAMIC_CULL_SPHERES...


#if __BANK
	#define		PlantsMinGroundAngleSlope	( DtoR * gbPlantsGroundSlopeAngleMin)
#else
	const float PlantsMinGroundAngleSlope = ( DtoR * CPLANT_GROUND_SLOPE_ANGLE_MIN);
#endif



//
//
// slot name in TxdStore for CPlantMgr:
//
#define CPLANTMGR_TXDSTORE_SLOTNAME			("plantsmgr")
#define CPLANTMGR_MODELSTORE_SLOTNAME		("plantsmgr")

//
// location of CPlantMgr texture dictionary:
//
static const strStreamingObjectName	CPLANTMGR_TXD_PATH("platform:/textures/plantsmgr",0xA9628AAC);
#if PLANTS_TXD_STR
	static const strStreamingObjectName	CPLANTMGR1_TXD_PATH("platform:/textures/plantsmgr1",0xCE7C7227);
	static const strStreamingObjectName	CPLANTMGR2_TXD_PATH("platform:/textures/plantsmgr2",0x6A82AA35);
#endif
static const strStreamingObjectName CPLANTMGR_MODELS_PATH("platform:/models/plantsmgr",0xBA15DA67);




static fwTxd*			gpPlantMgrTextureDictionary = NULL;
static Dwd*				gpPlantMgrModelDictionary	= NULL;

#if PLANTS_TXD_STR
	static bool	gbPlantsMgrTxdActive = false;
#endif

#if __BANK
	static u32 nLocTriDrawn		=0;
	static u32 nLocTriTexSkipped=0;		// # of tris skipped because of not valid textures
	u32 nLocTriPlantsLOD0Drawn	=0;
	u32 nLocTriPlantsLOD1Drawn	=0;
	u32 nLocTriPlantsLOD2Drawn	=0;
	u32 *pLocTriPlantsLOD0DrawnPerSlot = &nLocTriPlantsLOD0Drawn;
	u32 *pLocTriPlantsLOD1DrawnPerSlot = &nLocTriPlantsLOD1Drawn;
	u32 *pLocTriPlantsLOD2DrawnPerSlot = &nLocTriPlantsLOD2Drawn;
	#if CPLANT_USE_OCCLUSION
		static u32 nLocTrisOccluded = 0;
	#endif
#endif //__BANK

#if __PS3
	CompileTimeAssert(sizeof(CPlantLocTri)==PSN_SIZEOF_CPLANTLOCTRI);
	//FindSize(CPlantLocTri);
#endif
//FindSize(phBoundGeometry);
//FindSize(CPlantColBoundEntry);
//FindSize(CPlantMgrBase0);
//FindSize(CPlantMgrBase);
//FindSize(CPlantInfo);
//FindSize(ProcObjData_t);
CompileTimeAssert(CPLANT_MAX_POLY_POINTS==3);	// only tris are supported

// private random generator for PlantsMgr (it's used during RenderThread)
// (g_DrawRand is used by stuff in UpdateThread and can't be shared)
mthRandom g_PlantsRendRand[NUMBER_OF_RENDER_THREADS];

//
//
//
//
CPlantMgr gPlantMgr;
#include "renderer/PlantsMgrUpdateCommon.h"

#if PLANTS_USE_LOD_SETTINGS
PARAM(grassandplants,"Detail level for the plants rendering (0=low, 1=medium, 2=high)");
#endif // __WIN32

#if PLANTSMGR_DATA_EDITOR
	CPlantMgrDataEditor gPlantMgrEditor;
#endif

//
//
//
//
bool PlantsMgrAsyncMemcpyJob(const sysDependency& dep)
{
	const int listID = dep.m_Params[0].m_AsInt;
	u32* pDependenciesRunning = static_cast<u32*>(dep.m_Params[1].m_AsPtr);

	gPlantMgr.RenderTabMemcpy(listID);

	sysInterlockedDecrement(pDependenciesRunning);

	return(true);
}

//
//
//
//
CPlantMgr::CPlantMgr()
{
	m_scanCode = 0;
	m_bShouldRunAsyncMemcpyJobThisFrame = false;
	m_bSuppressObjCreation = false;
	m_bSuppressObjCreationPermanently = false;
	m_bEnableAmbScaleScan = true;

#if FURGRASS_TEST_V4
	m_bFurgrassDoRender[0]		=
	m_bFurgrassDoRender[1]		= false;
	m_bFurgrassTrisRtPresent[0]	=
	m_bFurgrassTrisRtPresent[1]	= true;
#endif

	for(s32 i=0; i<CPLANT_LOC_TRIS_LIST_NUM; i++)
	{
		m_LocTrisTab[i]				=
		m_LocTrisRenderTab[0][i]	=
		m_LocTrisRenderTab[1][i]	= NULL;
	}

	sysMemSet(m_PlantTextureTab0,	0, sizeof(m_PlantTextureTab0));
	sysMemSet(m_PlantModelsTab0,	0, sizeof(m_PlantModelsTab0));

	m_RTbufferID			= 0;

	for(u32 i=0; i<CPLANT_CULLSPHERES_MAX; i++)
	{
		m_CullSphereEnabled0[i]	= false;
	}

#if PLANTSMGR_DATA_EDITOR
	m_bAllCollisionSelectable[0] = m_bAllCollisionSelectable[1] = false;
	m_bRegenTriCaches = false;
#endif
}// CPlantMgr::CPlantMgr()...

//
//
//
//
CPlantMgr::~CPlantMgr()
{
	// do nothing
}


//
//
//
//
static
grcTexture* LoadTexture(char *texname)
{
	grcTexture *pTexture = gpPlantMgrTextureDictionary->Lookup(texname);
	if(!pTexture)
	{
		plantsDisplayf("Cannot find plant texture '%s'!", texname);
		return(NULL);
	}

	pTexture->AddRef();

#if __ASSERT
	// check plant texture dimensions:
	const s32 texWidth = pTexture->GetWidth();
	const s32 texHeight= pTexture->GetHeight();

	if( (texWidth==8)	||	(texWidth==16)	||	(texWidth==32)	||	(texWidth==64)	||	(texWidth==128)	||	(texWidth==256)	||	(texWidth==512)	||
		(texHeight==8)	||	(texHeight==16)	||	(texHeight==32)	||	(texHeight==64)	||	(texHeight==128)||	(texHeight==256)||	(texHeight==512)
	)	
	{
		// do nothing - dimensions are OK
	}
	else
	{
		plantsAssertf(FALSE, "texture '%s': %dx%d\n", texname, texWidth, texHeight);
	}

	if((texWidth > 512) || (texHeight > 512))
		plantsAssertf(texWidth <= 512 && texHeight <= 512, "texture '%s': %dx%d\n", texname, texWidth, texHeight);
#endif //__ASSERT...

	return(pTexture);
}

//
//
// texture stuff:
//
bool CPlantMgr::InitialiseTextures()
{
#if PLANTS_TXD_STR
	return(true);	// do nothing
#else //PLANTS_TXD_STR

	strLocalIndex txdSlot = strLocalIndex(g_TxdStore.FindSlot(CPLANTMGR_TXD_PATH));
	CStreaming::LoadObject(txdSlot, g_TxdStore.GetStreamingModuleId());
	g_TxdStore.AddRef(txdSlot, REF_OTHER);
	gpPlantMgrTextureDictionary = g_TxdStore.Get(txdSlot);
	Assert(gpPlantMgrTextureDictionary);

	for(s32 i=0; i<CPLANT_SLOT_NUM_TEXTURES; i++)
	{
		char txname[128];
		sprintf(txname, "txgrass%02d", i);				// "txgrass00"
		m_PlantTextureTab0[0+i]							= LoadTexture(txname);

		sprintf(txname, "txgrass%02d_lod1", i);			// "txgrass00_lod1"
		m_PlantTextureTab0[CPLANT_SLOT_NUM_TEXTURES+i]	= LoadTexture(txname);
	}

#if	PSN_PLANTSMGR_SPU_RENDER
	// setup texture commands for SPU (all base and LOD textures):
	for(s32 i=0; i<(CPLANT_SLOT_NUM_TEXTURES*2); i++)
	{
		CGrassRenderer::SpuRecordGrassTexture(i, m_PlantTextureTab0[i]);
	}
#endif
	return(true);	
#endif //!PLANTS_TXD_STR...
}// end of InitialiseTextures()...

#if PLANTS_TXD_STR
static strRequest	gpPlantMgrTxdStrReq[2];

#define PLANTS_TXD_MAX					(8)
static
const strStreamingObjectName* sm_PlantsMgrTxdPaths[PLANTS_TXD_MAX] =	{
		NULL,						// 0 = none
		&CPLANTMGR1_TXD_PATH,		// 1 = countryside
		&CPLANTMGR2_TXD_PATH,		// 2 = ocean
		NULL,						// 3 = n/a
		NULL,						// 4 = n/a
		NULL,						// 5 = n/a
		NULL,						// 6 = n/a
		&CPLANTMGR_TXD_PATH			// 7 = default plantsmgr
		};
#endif //PLANTS_TXD_STR...

//
//
//
//
bool CPlantMgr::UpdateStrTextures()
{
#if PLANTS_TXD_STR

enum // simple txd state machine:
{
	PLANTS_TXD_LOADED = 0,
	PLANTS_TXD_REQUESTED
};

static u8 currentState		= PLANTS_TXD_LOADED;
static u8 currentTxd		= 0;
static u8 requestedTxd		= 0;
static bool currentStrReq	= false;	// 0 or 1


	switch(currentState)
	{
		case(PLANTS_TXD_REQUESTED):
		{
			Assert(gpPlantMgrTxdStrReq[currentStrReq].IsValid());
			if(gpPlantMgrTxdStrReq[currentStrReq].IsValid())
			{
				if(gpPlantMgrTxdStrReq[currentStrReq].HasLoaded())
				{	// finished loading?

					// unload old txd:
					if(gpPlantMgrTextureDictionary)
					{
						const strStreamingObjectName *txdPathName = sm_PlantsMgrTxdPaths[ currentTxd ];
						plantsDisplayf("Unloading '%s'.", txdPathName->GetCStr());
						ShutdownTextures();
						gpPlantMgrTextureDictionary = NULL;
						g_TxdStore.RemoveRef(g_TxdStore.FindSlot(*txdPathName), REF_OTHER);
						gpPlantMgrTxdStrReq[!currentStrReq].Release();
					}

					const strStreamingObjectName *txdPathName = sm_PlantsMgrTxdPaths[ requestedTxd ];
					gpPlantMgrTxdStrReq[currentStrReq].ClearRequiredFlags(STRFLAG_DONTDELETE);
					CStreaming::SetDoNotDefrag((s32)gpPlantMgrTxdStrReq[currentStrReq].GetRequestId(), (s32)gpPlantMgrTxdStrReq[currentStrReq].GetModuleId());

					plantsDisplayf("Loaded '%s'!", txdPathName->GetCStr());
					const s32 txdSlot = g_TxdStore.FindSlot(*txdPathName);
					g_TxdStore.AddRef(txdSlot, REF_OTHER);
					Assert(gpPlantMgrTextureDictionary==NULL);
					gpPlantMgrTextureDictionary = g_TxdStore.Get(txdSlot);
					Assert(gpPlantMgrTextureDictionary);

					for(s32 i=0; i<CPLANT_SLOT_NUM_TEXTURES; i++)
					{
						char txname[128];
						sprintf(txname, "txgrass%02d", i);				// "txgrass00"
						m_PlantTextureTab0[0+i]							= LoadTexture(txname);

						sprintf(txname, "txgrass%02d_lod1", i);			// "txgrass00_lod1"
						m_PlantTextureTab0[CPLANT_SLOT_NUM_TEXTURES+i]	= LoadTexture(txname);
					}

				#if	PSN_PLANTSMGR_SPU_RENDER
					// setup texture commands for SPU (all base and LOD textures):
					for(s32 i=0; i<(CPLANT_SLOT_NUM_TEXTURES*2); i++)
					{
						CGrassRenderer::SpuRecordGrassTexture(i, m_PlantTextureTab0[i]);
					}
				#endif

					currentTxd		= requestedTxd;
					currentStrReq	= !currentStrReq;
					currentState	= PLANTS_TXD_LOADED;
				} // HasLoaded()...
			}// IsValid()...
		}
		break; //PLANTS_TXD_REQUESTED...
		
		case(PLANTS_TXD_LOADED):
		{
			u8 wantedTxd = CPopCycle::GetCurrentZonePlantsMgrTxdIdx();
			Assert(wantedTxd < PLANTS_TXD_MAX);
			if(wantedTxd >= PLANTS_TXD_MAX)
			{
				wantedTxd = PLANTS_TXD_MAX-1;
			}

			if(currentTxd != wantedTxd)
			{
				// request new txd:
				Assert(gpPlantMgrTxdStrReq[currentStrReq].IsValid()==false);
				if(!gpPlantMgrTxdStrReq[currentStrReq].IsValid())
				{
					const strStreamingObjectName *wantedTxdPathName = sm_PlantsMgrTxdPaths[ wantedTxd ];
					if(wantedTxdPathName)
					{
						plantsDisplayf("Requesting '%s'.", wantedTxdPathName->GetCStr());
						const s32 txdSlot = g_TxdStore.FindSlot(*wantedTxdPathName);
						// STRFLAG_DONTDELETE - Never delete the object, until CanDelete is called
						// STRFLAG_FORCE_LOAD - Keep the request alive until the object has been loaded
						gpPlantMgrTxdStrReq[currentStrReq].Request(txdSlot, g_TxdStore.GetStreamingModuleId(), STRFLAG_FORCE_LOAD|STRFLAG_DONTDELETE);
				
						requestedTxd	= wantedTxd;
						currentState	= PLANTS_TXD_REQUESTED;
					}
					else
					{
						// mark as already loaded (requested txd is NULL)

						// unload old txd:
						if(gpPlantMgrTextureDictionary)
						{
							const strStreamingObjectName *txdPathName = sm_PlantsMgrTxdPaths[ currentTxd ];
							plantsDisplayf("Unloading '%s'.", txdPathName->GetCStr());
							ShutdownTextures();
							gpPlantMgrTextureDictionary = NULL;
							g_TxdStore.RemoveRef(g_TxdStore.FindSlot(*txdPathName), REF_OTHER);
							gpPlantMgrTxdStrReq[!currentStrReq].Release();
						}
						
						currentStrReq	= !currentStrReq;
						currentTxd		= wantedTxd;
						currentState	= PLANTS_TXD_LOADED;
					}
				}
			}//if(currentTxd != wantedTxd)...
		}
		break; // PLANTS_TXD_LOADED...

		default:
			Assertf(0, "Invalid plantsmgr txd loading state!");
			break;
	};

	(void)gbPlantsMgrTxdActive;
#if 0
	const strStreamingObjectName *txdPathName = &CPLANTMGR1_TXD_PATH; 
	if(gbPlantsMgrTxdActive)
	{
		if(!gpPlantMgrTextureDictionary)
		{
			if(gpPlantMgrTxdStrReq.IsValid())
			{
				if(gpPlantMgrTxdStrReq.HasLoaded())
				{	// finished loading?
					gpPlantMgrTxdStrReq.ClearRequiredFlags(STRFLAG_DONTDELETE);
					plantsDisplayf("Loaded '%s'!", txdPathName->GetCStr());
					const s32 txdSlot = g_TxdStore.FindSlot(*txdPathName);
					g_TxdStore.AddRef(txdSlot, REF_OTHER);
					gpPlantMgrTextureDictionary = g_TxdStore.Get(txdSlot);
					Assert(gpPlantMgrTextureDictionary);

					for(s32 i=0; i<CPLANT_SLOT_NUM_TEXTURES; i++)
					{
						char txname[128];
						sprintf(txname, "txgrass%02d", i);				// "txgrass00"
						m_PlantTextureTab0[0+i]							= LoadTexture(txname);

						sprintf(txname, "txgrass%02d_lod1", i);			// "txgrass00_lod1"
						m_PlantTextureTab0[CPLANT_SLOT_NUM_TEXTURES+i]	= LoadTexture(txname);
					}

				#if	PSN_PLANTSMGR_SPU_RENDER
					// setup texture commands for SPU (all base and LOD textures):
					for(s32 i=0; i<(CPLANT_SLOT_NUM_TEXTURES*2); i++)
					{
						CGrassRenderer::SpuRecordGrassTexture(i, m_PlantTextureTab0[i]);
					}
				#endif
				} // HasLoaded()...
			}
			else
			{
				plantsDisplayf("Requesting '%s'.", txdPathName->GetCStr());
				const s32 txdSlot = g_TxdStore.FindSlot(*txdPathName);
				// STRFLAG_DONTDELETE - Never delete the object, until CanDelete is called
				// STRFLAG_FORCE_LOAD - Keep the request alive until the object has been loaded
				gpPlantMgrTxdStrReq.Request(txdSlot, g_TxdStore.GetStreamingModuleId(), STRFLAG_FORCE_LOAD|STRFLAG_DONTDELETE);
			}
		}//if(!gpPlantMgrTextureDictionary)...
	}
	else
	{
		if(gpPlantMgrTextureDictionary)
		{
			plantsDisplayf("Unloading '%s'.", txdPathName->GetCStr());
			ShutdownTextures();

			gpPlantMgrTextureDictionary = NULL;
			g_TxdStore.RemoveRef(g_TxdStore.FindSlot(*txdPathName), REF_OTHER);
			gpPlantMgrTxdStrReq.Release();
		}
	}
#endif //#if 0...
#endif	//PLANTS_TXD_STR...
	return(true);	
}// end of UpdateStrTextures()...

//
//
// destroy allocated textures:
//
bool CPlantMgr::ShutdownTextures()
{
#define DESTROY_TEXTURE(TEXTURE)		{ if(TEXTURE) {TEXTURE->Release(); TEXTURE=NULL;} }

	for(s32 i=0; i<CPLANT_SLOT_NUM_TEXTURES*2; i++)
	{
		DESTROY_TEXTURE(m_PlantTextureTab0[i]);
	}
#undef DESTROY_TEXTURE


#if	PSN_PLANTSMGR_SPU_RENDER
	for(s32 i=0; i<CPLANT_SLOT_NUM_TEXTURES*2; i++)
	{
		CGrassRenderer::SpuRecordGrassTexture(i, m_PlantTextureTab0[i]);
	}
#endif

#if PLANTS_TXD_STR
	// do nothing
#else
	g_TxdStore.RemoveRef(strLocalIndex(g_TxdStore.FindSlot(CPLANTMGR_TXD_PATH)), REF_OTHER);
	gpPlantMgrTextureDictionary = NULL;
#endif
	return(true);
}
	
//
//
//
//
bool CPlantMgr::LoadPlantModel(char *fname, grassModel *pPlantModel)
{
	gtaDrawable *pDrawable = static_cast<gtaDrawable*>(gpPlantMgrModelDictionary->Lookup(fname));
	Assertf(pDrawable, "Cannot find plant model!");

	//	const s32 oldShaderCount = oldShaderGroup->GetCount();

	// create new shadergroup with one shader:
	grmShaderGroup *pNewShaderGroup =	//CGrassRenderer::GetGrassShaderGroup();
										grmShaderFactory::GetInstance().CreateGroup();
	Assert(pNewShaderGroup);

	grmShader	*pGrassShader = CGrassRenderer::GetGrassShader();
	Assert(pGrassShader);
	pNewShaderGroup->InitCount(1);		//oldShaderCount);
	pNewShaderGroup->Add(pGrassShader);

	pDrawable->SetShaderGroup(pNewShaderGroup);

	rmcLodGroup *pLodGroup	= &pDrawable->GetLodGroup();
	Assert(pLodGroup);
	rmcLod		*pLod		= &pLodGroup->GetLod(LOD_HIGH);
	Assert(pLod);
	grmModel	*pModel		= pLod->GetModel(0);
	Assert(pModel);

	const s32 geomCount	= pModel->GetGeometryCount();
	Assert(geomCount > 0);
	
	// setup model shader indices:
	for(s32 i=0; i<geomCount; i++)
	{
		pModel->SetShaderIndex(i, 0);

#if __ASSERT
		// verify boundSphere radius:
		Assert(pModel->GetAABBs());
		Vector4 boundSphere = VEC4V_TO_VECTOR4(pModel->GetGeometryAABB(i).GetBoundingSphereV4());
		if(boundSphere.w < 0.01f)
			plantsDisplayf("'%s' boundSphere=(%.3f, %.2f, %.3f, %.3f) usesBoundingBoxes=%d\n", fname, boundSphere.x, boundSphere.y, boundSphere.z, boundSphere.w, (pModel->GetModelCullback() != NULL));
		Assert(boundSphere.w >= 0.01f);

		// verify vertex stream format available in this geometry:
		grmGeometry *pGeometry = &pModel->GetGeometry(i);
		Assert(pGeometry);
		Assert(pGeometry->GetType() != grmGeometry::GEOMETRYEDGE);
		const grcFvf *pFvf = pGeometry->GetFvf();
		Assert(pFvf);

		plantsAssertf(pFvf->GetPosChannel(),						"Geometry '%s' is missing POSITION channel!", fname);
		if			(!pFvf->GetPosChannel())		plantsWarningf(	"Geometry '%s' is missing POSITION channel!", fname);
		plantsAssertf(pFvf->GetDiffuseChannel(),					"Geometry '%s' is missing COLOR0 channel!", fname);
		if			(!pFvf->GetDiffuseChannel())	plantsWarningf(	"Geometry '%s' is missing COLOR0 channel!", fname);
		plantsAssertf(pFvf->GetSpecularChannel(),					"Geometry '%s' is missing COLOR1 channel!", fname);
		if			(!pFvf->GetSpecularChannel())	plantsWarningf(	"Geometry '%s' is missing COLOR1 channel!", fname);
		plantsAssertf(pFvf->GetTextureChannel(0),					"Geometry '%s' is missing TEXCOORD0 channel!", fname);
		if			(!pFvf->GetTextureChannel(0))	plantsWarningf(	"Geometry '%s' is missing TEXCOORD0 channel!", fname);
#endif //__ASSERT
	}

	// return plantModel:
	pPlantModel->m_pDrawable = pDrawable;
	Assert(pPlantModel->m_pDrawable);
	pPlantModel->m_pGeometry = &pModel->GetGeometry(0);
	Assert(pPlantModel->m_pGeometry);
	pPlantModel->m_BoundingSphere = VEC4V_TO_VECTOR4(pModel->GetModelAABB().GetBoundingSphereV4());
	Assert(pPlantModel->m_BoundingSphere.w > 0.0f);
	return(TRUE);
}// end of LoadPlantModel()...


//
//
//
//
void CPlantMgr::UnloadPlantModel(grassModel *model)
{
	gtaDrawable *pDrawable = model->m_pDrawable;
	if (pDrawable)
	{
		// Get shader group
		grmShaderGroup &shadergroup = pDrawable->GetShaderGroup();
		// Remove reference to the shaders : they're owned by the grassRenderer, and probably
		// already deleted by now. (only one by now).
		for(int i=0;i<shadergroup.GetCount();i++)
		{
			shadergroup.Remove(shadergroup.GetShaderPtr(i),false);
		}
		
		//pDrawable->SetShaderGroup(NULL);
		delete pDrawable;
		pDrawable = NULL;
	}

	model->m_pDrawable = NULL;
	model->m_pGeometry = NULL;
	model->m_BoundingSphere.Set(0);
	return;

}// end of UnloadPlantModel()...


PS3_ONLY(namespace rage { extern u32 g_AllowVertexBufferVramLocks; })
//
//
//
//
bool CPlantMgr::LoadPlantModelLOD2(char *fname, grassModel	*pPlantModel)
{

	plantsAssert(pPlantModel);

	gtaDrawable *pDrawable = static_cast<gtaDrawable*>(gpPlantMgrModelDictionary->Lookup(fname));
	plantsAssertf(pDrawable, "Cannot find plant model!");

	rmcLodGroup *pLodGroup	= &pDrawable->GetLodGroup();
	plantsAssert(pLodGroup);
	rmcLod		*pLod		= &pLodGroup->GetLod(LOD_HIGH);
	plantsAssert(pLod);
	grmModel	*pModel		= pLod->GetModel(0);
	plantsAssert(pModel);

#if __ASSERT
	plantsAssertf(pModel->GetGeometryCount()==1, "LOD2: Only simple billboard geometries allowed");
#endif

	grmGeometry *pGeometry = &pModel->GetGeometry(0);
	plantsAssert(pGeometry);
	grcVertexBuffer *pVB	= pGeometry->GetVertexBuffer();
	plantsAssert(pVB);
	
	PS3_ONLY(++g_AllowVertexBufferVramLocks);	// PS3: attempting to edit VBs in VRAM

	grcVertexBufferEditor vbEditor(pVB,true,true);
#if __WIN32PC
	if (!vbEditor.isValid())
	{
		pPlantModel->m_dimensionLOD2.x = 0.000001f;
		pPlantModel->m_dimensionLOD2.y = 0.000001f;
		return false;
	}
#endif // __WIN32PC

#if __ASSERT
	grcIndexBuffer *pIB		= pGeometry->GetIndexBuffer();
	plantsAssert(pIB);

	const s32 indexCount	= pIB->GetIndexCount();
	const s32 primCount	= pGeometry->GetPrimitiveCount();
	plantsAssert(indexCount == 6);
	plantsAssert(primCount*3 == indexCount);	// make sure it's tri list (drawTris);

#if 0
	const u16* pIndices	= pIB->LockRO();
	Printf("\n indexCount=%d, primCount=%d", indexCount, primCount);
	for(s32 i=0; i<indexCount; i++)
	{
		const s32 idx = pIndices[i];
		Vector3	pos		= vbEditor.GetPosition(idx);
		Printf("\n %d: index=%d: xyz=%.2f %.2f %.2f", i, idx, pos.x, pos.y, pos.z);
	}
#endif //#if 0...
#endif //__ASSERT...


	Vector3 v0 = vbEditor.GetPosition(0);
	Vector3 v1 = vbEditor.GetPosition(1);
	Vector3 v2 = vbEditor.GetPosition(2);
	// Vector3 v3 = vbEditor.GetPosition(3);

#if 0
	// old geom LOD2 v1
	indexCount=6, primCount=2
	0: 0: xyz=-1.58 0.00 0.00
	1: 1: xyz=-1.58 0.00 1.53
	2: 2: xyz= 1.58 0.00 1.53
	3: 2: xyz= 1.58 0.00 1.53
	4: 3: xyz= 1.58 0.00 0.00
	5: 0: xyz=-1.58 0.00 0.00

	0: 0: xyz=-1.58 0.00 0.00
	1: 1: xyz=-1.58 0.00 1.53
	2: 2: xyz= 1.58 0.00 1.53
	4: 3: xyz= 1.58 0.00 0.00

	// new geom LOD2 v2
	0: index=0: xyz=-0.77 0.00 0.00
	1: index=1: xyz= 0.77 0.00 0.00
	2: index=2: xyz= 0.77 0.00 14.46
	3: index=2: xyz= 0.77 0.00 14.46
	4: index=3: xyz=-0.77 0.00 14.46
	5: index=0: xyz=-0.77 0.00 0.00

	// new geom LOD2 v3
	0: index=0: xyz=0.00 -1.58 0.00
	1: index=1: xyz=0.00  1.58 0.00
	2: index=2: xyz=0.00  1.58 1.53
	3: index=2: xyz=0.00  1.58 1.53
	4: index=3: xyz=0.00 -1.58 1.53
	5: index=0: xyz=0.00 -1.58 0.00
#endif //#if 0...

	// v1: first try to find dimentions:
	float width = rage::Abs(v0.x) + rage::Abs(v2.x);
	float height= rage::Abs(v0.z) + rage::Abs(v1.z);

	// v2: second try (try to pick different verts to calculate dimensions):
	if((width <= 0.0f)	|| (width <= 0.0f))
	{
		width = rage::Abs(v0.x) + rage::Abs(v1.x);
		height= rage::Abs(v0.z) + rage::Abs(v2.z);
	}

	// v3: third try (try to pick different verts to calculate dimensions):
	if((width <= 0.0f)	|| (width <= 0.0f))
	{
		width = rage::Abs(v0.y) + rage::Abs(v1.y);
		height= rage::Abs(v0.z) + rage::Abs(v2.z);
	}

	Assert(width  > 0.0f);
	Assert(height > 0.0f);

#if __ASSERT
	plantsDebugf1("LOD2 '%s': detected billboard W=%.2f and H=%.2f.", fname, width, height);
#endif

	PS3_ONLY(--g_AllowVertexBufferVramLocks);	// PS3: finished with editing VBs in VRAM

	// return results:
	pPlantModel->m_dimensionLOD2.x = width;
	pPlantModel->m_dimensionLOD2.y = height;

	return(TRUE);
}// end of LoadPlantModelLOD2()...

//
//
// Create LOD2 vertexbuffer and declaration:
//
bool CPlantMgr::CreateGeometryLOD2()
{

#if GRASS_LOD2_BATCHING
	grcVertexElement elements[1+6] =
	{
		// stream#0:
		grcVertexElement( 0, grcVertexElement::grcvetTexture, 0/*TEXCOORD0*/, grcFvf::GetDataSizeFromType(grcFvf::grcdsFloat2), grcFvf::grcdsFloat2 , grcFvf::grcsfmModulo, 4 ),
		// stream#1:
		grcVertexElement( 1, grcVertexElement::grcvetTexture, 1/*TEXCOORD1*/, grcFvf::GetDataSizeFromType(grcFvf::grcdsFloat4), grcFvf::grcdsFloat4 , grcFvf::grcsfmDivide, 4 ),
		grcVertexElement( 1, grcVertexElement::grcvetTexture, 2/*TEXCOORD2*/, grcFvf::GetDataSizeFromType(grcFvf::grcdsFloat4), grcFvf::grcdsFloat4 , grcFvf::grcsfmDivide, 4 ),
		grcVertexElement( 1, grcVertexElement::grcvetTexture, 3/*TEXCOORD3*/, grcFvf::GetDataSizeFromType(grcFvf::grcdsFloat4), grcFvf::grcdsFloat4 , grcFvf::grcsfmDivide, 4 ),
		grcVertexElement( 1, grcVertexElement::grcvetTexture, 4/*TEXCOORD4*/, grcFvf::GetDataSizeFromType(grcFvf::grcdsFloat4), grcFvf::grcdsFloat4 , grcFvf::grcsfmDivide, 4 ),
		grcVertexElement( 1, grcVertexElement::grcvetTexture, 5/*TEXCOORD5*/, grcFvf::GetDataSizeFromType(grcFvf::grcdsFloat4), grcFvf::grcdsFloat4 , grcFvf::grcsfmDivide, 4 ),
		grcVertexElement( 1, grcVertexElement::grcvetTangent, 0/*TEXCOORD6*/, grcFvf::GetDataSizeFromType(grcFvf::grcdsFloat4), grcFvf::grcdsFloat4 , grcFvf::grcsfmDivide, 4 )
	};
#else
	grcVertexElement elements[1] =
	{
	#if (RSG_PC || RSG_DURANGO || RSG_ORBIS)
		// I can't see any reason why the LOD code even needs a divider, it always renders only one quad.  But let's not risk breaking other builds.
		grcVertexElement( 0, grcVertexElement::grcvetTexture, 0, grcFvf::GetDataSizeFromType(grcFvf::grcdsFloat2), grcFvf::grcdsFloat2 , grcFvf::grcsfmDivide, 0 )
	#else
		grcVertexElement( 0, grcVertexElement::grcvetTexture, 0, grcFvf::GetDataSizeFromType(grcFvf::grcdsFloat2), grcFvf::grcdsFloat2 , grcFvf::grcsfmModulo, 4 )
	#endif
	};
#endif // GRASS_LOD2_BATCHING


#if !(__D3D11 || RSG_ORBIS)
	// Verts for quads or triangle fans
	static ALIGNAS(16) Vector2 quadVerts[4] = 
	{
		Vector2(1.0f, 0.0f), 
		Vector2(0.0f, 0.0f), 
		Vector2(0.0f, 1.0f),
		Vector2(1.0f, 1.0f) 
	};
#else
	// Verts for a triangle strip
	static ALIGNAS(16) Vector2 quadVerts[4] = 
	{
		Vector2(1.0f, 0.0f), 
		Vector2(0.0f, 0.0f), 
		Vector2(1.0f, 1.0f),
		Vector2(0.0f, 1.0f)
	};
#endif // !(__D3D11 || RSG_ORBIS)

	// set up quad vertex buffer for instancing.
	const s32 repeatAmount = 4;
	const s32 channel = 0;

	grcFvf		fvf0;
	fvf0.SetTextureChannel( channel, true, grcFvf::grcdsFloat2);
	fvf0.SetPosChannel( false );

	const bool bReadWrite = !(__D3D11 && RSG_PC);
	const bool bDynamic = false;
#if RSG_PC
	void *const preAllocatedMemory = quadVerts;
#else
	void *const preAllocatedMemory = NULL;
#endif
	grcVertexBuffer* vbuf = grcVertexBuffer::Create(repeatAmount, fvf0, bReadWrite, bDynamic, preAllocatedMemory);
	Assert(vbuf);

	PS3_ONLY(++g_AllowVertexBufferVramLocks);	// PS3: attempting to edit VBs in VRAM
#if !(__D3D11 && RSG_PC)
	{
		grcVertexBufferEditor editor(vbuf);
		for(s32 j=0; j<repeatAmount; j++ )
		{
			s32 index = j;
			editor.SetUV(index, channel, quadVerts[index], false );
		}
	}
	vbuf->MakeReadOnly();
#endif // !(__D3D11 && RSG_PC)
	PS3_ONLY(--g_AllowVertexBufferVramLocks);	// PS3: finished with editing VBs in VRAM

	grcVertexDeclaration	*decl = GRCDEVICE.CreateVertexDeclaration( elements, NELEM(elements) );
	Assert(decl);

	CGrassRenderer::SetGeometryLOD2(vbuf, decl);

	return(TRUE);
}// end of CreateGeometryLOD2()...



static grcRasterizerStateHandle		hGrassAlphaToMaskRS_pass1			= grcStateBlock::RS_Invalid;
static grcDepthStencilStateHandle	hGrassAlphaToMaskDSS_pass1			= grcStateBlock::DSS_Invalid;
static grcBlendStateHandle			hGrassAlphaToMaskBS_pass1			= grcStateBlock::BS_Invalid;
static grcBlendStateHandle			hGrassAlphaToMaskBS_pass1_end		= grcStateBlock::BS_Invalid;
static grcDepthStencilStateHandle	hGrassAlphaToMaskDSS_pass1_end		= grcStateBlock::DSS_Invalid;
#if EXTRA_1_5_PASS
	static grcBlendStateHandle			hGrassAlphaToMaskBS_pass1_5a	= grcStateBlock::BS_Invalid;
	static grcDepthStencilStateHandle	hGrassAlphaToMaskDSS_pass1_5a	= grcStateBlock::DSS_Invalid;
	static grcBlendStateHandle			hGrassAlphaToMaskBS_pass1_5b	= grcStateBlock::BS_Invalid;
	static grcDepthStencilStateHandle	hGrassAlphaToMaskDSS_pass1_5b	= grcStateBlock::DSS_Invalid;
	static grcDepthStencilStateHandle	hGrassAlphaToMaskDSS_pass1_5c	= grcStateBlock::DSS_Invalid;
	static grcDepthStencilStateHandle	hGrassAlphaToMaskDSS_pass1_5_end= grcStateBlock::DSS_Invalid;
	static grcBlendStateHandle			hGrassAlphaToMaskBS_pass1_5_end = grcStateBlock::BS_Invalid;
#endif
#if XENON_FILL_PASS
	static grcBlendStateHandle			hGbuffFillPassBS				= grcStateBlock::BS_Invalid;
	static grcBlendStateHandle			hGbuffFillPassBS_end			= grcStateBlock::BS_Invalid;
	static grcDepthStencilStateHandle	hGbuffFillPassDSS				= grcStateBlock::DSS_Invalid;
	static grcDepthStencilStateHandle	hGbuffFillPassRestoreHiStDSS	= grcStateBlock::DSS_Invalid;
	static grcDepthStencilStateHandle	hGbuffFillPassDSS_end			= grcStateBlock::DSS_Invalid;
#endif
#if PSN_ALPHATOMASK_PASS
	static grcBlendStateHandle			hGrassAlphaToMaskBS_pass2		= grcStateBlock::BS_Invalid;
	static grcDepthStencilStateHandle	hGrassAlphaToMaskDSS_pass2		= grcStateBlock::DSS_Invalid;
	static grcRasterizerStateHandle		hGrassAlphaToMaskRS_pass2		= grcStateBlock::RS_Invalid;
	static grcDepthStencilStateHandle	hGrassAlphaToMaskDSS_pass2_end	= grcStateBlock::DSS_Invalid;
	static grcRasterizerStateHandle		hGrassAlphaToMaskRS_pass2_end	= grcStateBlock::RS_Invalid;
	static grcBlendStateHandle			hGrassAlphaToMaskBS_pass2_end	= grcStateBlock::BS_Invalid;
#endif

#if PLANTS_CAST_SHADOWS
	static grcRasterizerStateHandle		hGrassShadowRS 					= grcStateBlock::RS_Invalid;
	static grcRasterizerStateHandle		hGrassShadowRS_end				= grcStateBlock::RS_Invalid;
#endif //PLANTS_CAST_SHADOWS

static bool InitialiseGrassRenderStateBlocks()
{
#if __BANK
	const bool sm_bEnableCulling		= gbDoubleSidedCulling;
#else
	static dev_bool sm_bEnableCulling	= FALSE;
#endif

	if(hGrassAlphaToMaskRS_pass1 == grcStateBlock::RS_Invalid)
	{
		grcRasterizerStateDesc rasDesc;
		rasDesc.FillMode	= grcRSV::FILL_SOLID;
		rasDesc.CullMode	= sm_bEnableCulling? grcRSV::CULL_BACK : grcRSV::CULL_NONE;
		
		hGrassAlphaToMaskRS_pass1	= grcStateBlock::CreateRasterizerState(rasDesc);
		Assert(hGrassAlphaToMaskRS_pass1 != grcStateBlock::RS_Invalid);
	}

	if(hGrassAlphaToMaskDSS_pass1 == grcStateBlock::DSS_Invalid)
	{
		grcDepthStencilStateDesc dsDesc;
		dsDesc.DepthEnable					= TRUE;
		dsDesc.DepthWriteMask				= TRUE;
		dsDesc.DepthFunc					= rage::FixupDepthDirection(grcRSV::CMP_LESS);
		// enable stencil:
		dsDesc.StencilEnable				= TRUE;
		dsDesc.StencilReadMask 				= DEFERRED_MATERIAL_INTERIOR_VEH;
	#if EXTRA_1_5_PASS
		dsDesc.StencilWriteMask				= 0xf0;		// do not overwrite original materialID (bits: 3-0)
	#else
		dsDesc.StencilWriteMask				= 0xff;
	#endif
		dsDesc.FrontFace.StencilFunc		= 
		dsDesc.BackFace.StencilFunc			= grcRSV::CMP_EQUAL;
		dsDesc.FrontFace.StencilPassOp		=
		dsDesc.BackFace.StencilPassOp		= grcRSV::STENCILOP_REPLACE;
		dsDesc.FrontFace.StencilFailOp		=
		dsDesc.BackFace.StencilFailOp		= grcRSV::STENCILOP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp	=
		dsDesc.BackFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;

		hGrassAlphaToMaskDSS_pass1		= grcStateBlock::CreateDepthStencilState(dsDesc);
		Assert(hGrassAlphaToMaskDSS_pass1 != grcStateBlock::DSS_Invalid);
	}

#if USE_SSA_STIPPLE_ALPHA
	//RDR3 uses SSA style stipple to eliminate pixels for grass alpha, so these states need to be changed.
	static dev_bool debugEnableAlphaToMask1		= FALSE;
	static dev_bool debugUseSolidAlphaToMask1	= TRUE;
#else
	static dev_bool debugEnableAlphaToMask1		= TRUE;
	static dev_bool debugUseSolidAlphaToMask1	= FALSE;
#endif
	if(hGrassAlphaToMaskBS_pass1 == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.BlendRTDesc[0].BlendEnable			= FALSE;
		// Standard srcAlpha, 1-srcAlpha blend
		bsDesc.BlendRTDesc[0].BlendOp				= grcRSV::BLENDOP_ADD;
		bsDesc.BlendRTDesc[0].SrcBlend				= grcRSV::BLEND_SRCALPHA;
		bsDesc.BlendRTDesc[0].DestBlend				= grcRSV::BLEND_INVSRCALPHA;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_RGB;

		#if PSN_ALPHATOMASK_PASS
			bsDesc.IndependentBlendEnable					= FALSE;
		#else
			bsDesc.IndependentBlendEnable					= TRUE;
			bsDesc.BlendRTDesc[1].RenderTargetWriteMask		= grcRSV::COLORWRITEENABLE_ALL;
			#if WRITE_SELF_SHADOW_TERM
				bsDesc.BlendRTDesc[2].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
			#else
				bsDesc.BlendRTDesc[2].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_RGB;		// respect selfshadow term from underlying ground
			#endif
			bsDesc.BlendRTDesc[3].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
		#endif //PSN_ALPHATOMASK_PASS...

		bsDesc.AlphaToCoverageEnable				= debugEnableAlphaToMask1;
		bsDesc.AlphaToMaskOffsets					= debugUseSolidAlphaToMask1 ? grcRSV::ALPHATOMASKOFFSETS_SOLID : grcRSV::ALPHATOMASKOFFSETS_DITHERED;
		
		hGrassAlphaToMaskBS_pass1 = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hGrassAlphaToMaskBS_pass1 != grcStateBlock::BS_Invalid);
	}

	if(hGrassAlphaToMaskBS_pass1_end == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.AlphaToCoverageEnable	= FALSE;
		
		hGrassAlphaToMaskBS_pass1_end = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hGrassAlphaToMaskBS_pass1_end != grcStateBlock::BS_Invalid);
	}

	if(hGrassAlphaToMaskDSS_pass1_end == grcStateBlock::DSS_Invalid)
	{
		grcDepthStencilStateDesc dsDesc;
		dsDesc.DepthEnable					= TRUE;
		dsDesc.DepthWriteMask				= TRUE;
		dsDesc.DepthFunc					= rage::FixupDepthDirection(grcRSV::CMP_LESS);
		// enable stencil:
		dsDesc.StencilEnable				= TRUE;
		dsDesc.StencilReadMask				= 0xff;
		dsDesc.StencilWriteMask				= 0xff;
		dsDesc.FrontFace.StencilFunc		= grcRSV::CMP_ALWAYS;
		dsDesc.FrontFace.StencilPassOp		= grcRSV::STENCILOP_REPLACE;
		dsDesc.FrontFace.StencilFailOp		= grcRSV::STENCILOP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;

		hGrassAlphaToMaskDSS_pass1_end		= grcStateBlock::CreateDepthStencilState(dsDesc);
		Assert(hGrassAlphaToMaskDSS_pass1_end != grcStateBlock::DSS_Invalid);
	}

#if EXTRA_1_5_PASS
	if(hGrassAlphaToMaskBS_pass1_5a == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
	#if __XENON
		bsDesc.IndependentBlendEnable				= TRUE;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_NONE;
		bsDesc.BlendRTDesc[1].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_NONE;
		bsDesc.BlendRTDesc[2].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALPHA;	// write only to MRT2.a=selfshadow
		bsDesc.BlendRTDesc[3].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_NONE;
	#else // __XENON
		bsDesc.IndependentBlendEnable				= FALSE;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALPHA;	// write only to MRT2.a=selfshadow
	#endif // __XENON
		hGrassAlphaToMaskBS_pass1_5a = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hGrassAlphaToMaskBS_pass1_5a != grcStateBlock::BS_Invalid);
	}

	if(hGrassAlphaToMaskDSS_pass1_5a == grcStateBlock::DSS_Invalid)
	{
		grcDepthStencilStateDesc dsDesc;
		dsDesc.DepthEnable					= FALSE;
		dsDesc.DepthWriteMask				= FALSE;
		dsDesc.StencilEnable				= TRUE;
		dsDesc.FrontFace.StencilFunc		= grcRSV::CMP_NOTEQUAL;
		dsDesc.FrontFace.StencilPassOp		= grcRSV::STENCILOP_KEEP;
		dsDesc.FrontFace.StencilFailOp		= grcRSV::STENCILOP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;
		dsDesc.StencilReadMask				= 0x1f;						// read bits 4,3..0
		dsDesc.StencilWriteMask				= 0x00;
		hGrassAlphaToMaskDSS_pass1_5a		= grcStateBlock::CreateDepthStencilState(dsDesc);
		Assert(hGrassAlphaToMaskDSS_pass1_5a != grcStateBlock::DSS_Invalid);
	}

	if(hGrassAlphaToMaskBS_pass1_5b == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.IndependentBlendEnable				= FALSE;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_NONE;
		hGrassAlphaToMaskBS_pass1_5b = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hGrassAlphaToMaskBS_pass1_5b != grcStateBlock::BS_Invalid);
	}

	if(hGrassAlphaToMaskDSS_pass1_5b == grcStateBlock::DSS_Invalid)
	{
		grcDepthStencilStateDesc dsDesc;
		dsDesc.DepthEnable					= FALSE;
		dsDesc.DepthWriteMask				= FALSE;
		
		dsDesc.StencilEnable				= TRUE;
		dsDesc.FrontFace.StencilFunc		= grcRSV::CMP_EQUAL;
		dsDesc.FrontFace.StencilPassOp		= grcRSV::STENCILOP_REPLACE;
		dsDesc.FrontFace.StencilFailOp		= grcRSV::STENCILOP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;
		dsDesc.StencilReadMask				= 0x10;						// read special bit only
		dsDesc.StencilWriteMask				= 0x0f;						// overwrite original material with 0x03

		hGrassAlphaToMaskDSS_pass1_5b		= grcStateBlock::CreateDepthStencilState(dsDesc);
		Assert(hGrassAlphaToMaskDSS_pass1_5b != grcStateBlock::DSS_Invalid);
	}

	if(hGrassAlphaToMaskDSS_pass1_5c == grcStateBlock::DSS_Invalid)
	{
		grcDepthStencilStateDesc dsDesc;
		dsDesc.DepthEnable					= FALSE;
		dsDesc.DepthWriteMask				= FALSE;
		
		dsDesc.StencilEnable				= TRUE;
		dsDesc.FrontFace.StencilFunc		= grcRSV::CMP_EQUAL;
		dsDesc.FrontFace.StencilPassOp		= grcRSV::STENCILOP_ZERO;
		dsDesc.FrontFace.StencilFailOp		= grcRSV::STENCILOP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;
		dsDesc.StencilReadMask				= 0x1f;						// read full material
		dsDesc.StencilWriteMask				= 0x10;						// clear special bit only

		hGrassAlphaToMaskDSS_pass1_5c		= grcStateBlock::CreateDepthStencilState(dsDesc);
		Assert(hGrassAlphaToMaskDSS_pass1_5c != grcStateBlock::DSS_Invalid);
	}

	if(hGrassAlphaToMaskDSS_pass1_5_end == grcStateBlock::DSS_Invalid)
	{
		grcDepthStencilStateDesc dsDesc;
		dsDesc.DepthEnable					= TRUE;
		dsDesc.DepthWriteMask				= TRUE;
	
		dsDesc.StencilEnable				= FALSE;
		dsDesc.FrontFace.StencilFunc		= grcRSV::CMP_ALWAYS;

		hGrassAlphaToMaskDSS_pass1_5_end		= grcStateBlock::CreateDepthStencilState(dsDesc);
		Assert(hGrassAlphaToMaskDSS_pass1_5_end != grcStateBlock::DSS_Invalid);
	}

	if(hGrassAlphaToMaskBS_pass1_5_end == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.IndependentBlendEnable				= FALSE;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
		hGrassAlphaToMaskBS_pass1_5_end = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hGrassAlphaToMaskBS_pass1_5_end != grcStateBlock::BS_Invalid);
	}
#endif //EXTRA_1_5_PASS...

#if XENON_FILL_PASS
	if(hGbuffFillPassRestoreHiStDSS == grcStateBlock::DSS_Invalid)
	{
		grcDepthStencilStateDesc dsDesc;
		dsDesc.DepthEnable					= FALSE;
		dsDesc.DepthWriteMask				= FALSE;

		dsDesc.StencilEnable				= TRUE;
		dsDesc.FrontFace.StencilFunc		= grcRSV::CMP_EQUAL;
		dsDesc.FrontFace.StencilPassOp		= grcRSV::STENCILOP_KEEP;
		dsDesc.FrontFace.StencilFailOp		= grcRSV::STENCILOP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;
		dsDesc.StencilReadMask				= 0x7;						// read bits 2..0

		hGbuffFillPassRestoreHiStDSS		= grcStateBlock::CreateDepthStencilState(dsDesc);
		Assert(hGbuffFillPassRestoreHiStDSS != grcStateBlock::DSS_Invalid);
	}

	if(hGbuffFillPassDSS_end == grcStateBlock::DSS_Invalid)
	{
		grcDepthStencilStateDesc dsDesc;
		dsDesc.DepthEnable					= TRUE;
		dsDesc.DepthWriteMask				= TRUE;

		dsDesc.StencilEnable				= FALSE;
		dsDesc.FrontFace.StencilFunc		= grcRSV::CMP_ALWAYS;

		hGbuffFillPassDSS_end				= grcStateBlock::CreateDepthStencilState(dsDesc);
		Assert(hGbuffFillPassDSS_end != grcStateBlock::DSS_Invalid);
	}

	if(hGbuffFillPassBS == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.IndependentBlendEnable				= TRUE;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_NONE;
		bsDesc.BlendRTDesc[1].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
		bsDesc.BlendRTDesc[2].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;	
		bsDesc.BlendRTDesc[3].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
		hGbuffFillPassBS							= grcStateBlock::CreateBlendState(bsDesc);
		Assert(hGbuffFillPassBS != grcStateBlock::BS_Invalid);
	}

	if(hGbuffFillPassDSS == grcStateBlock::DSS_Invalid)
	{
		grcDepthStencilStateDesc dsDesc;
		dsDesc.DepthEnable					= FALSE;
		dsDesc.DepthWriteMask				= FALSE;
		dsDesc.StencilEnable				= TRUE;
		dsDesc.FrontFace.StencilFunc		= grcRSV::CMP_EQUAL;
		dsDesc.FrontFace.StencilPassOp		= grcRSV::STENCILOP_ZERO;
		dsDesc.FrontFace.StencilFailOp		= grcRSV::STENCILOP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;
		dsDesc.StencilReadMask				= 0xFF;	  
		dsDesc.StencilWriteMask				= 0x10;
		hGbuffFillPassDSS					= grcStateBlock::CreateDepthStencilState(dsDesc);
		Assert(hGbuffFillPassDSS != grcStateBlock::DSS_Invalid);
	}

	if(hGbuffFillPassBS_end == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.IndependentBlendEnable				= FALSE;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
		hGbuffFillPassBS_end						= grcStateBlock::CreateBlendState(bsDesc);
		Assert(hGbuffFillPassBS_end != grcStateBlock::BS_Invalid);
	}
#endif //XENON_FILL_PASS...

#if PSN_ALPHATOMASK_PASS
	if(hGrassAlphaToMaskBS_pass2 == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.IndependentBlendEnable				= TRUE;
		#if CPLANT_STORE_LOCTRI_NORMAL
			bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;	// need to overwrite decompressed color
			bsDesc.BlendRTDesc[1].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
			#if WRITE_SELF_SHADOW_TERM
				bsDesc.BlendRTDesc[2].BlendEnable = TRUE;
				bsDesc.BlendRTDesc[2].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
			#else
				bsDesc.BlendRTDesc[2].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_RGB;		// respect selfshadow term from underlying ground
			#endif
				bsDesc.BlendRTDesc[3].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
		#elif CPLANT_BLIT_MIN_GBUFFER
			// Everything shifts down by 1 slot
			bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
			#if WRITE_SELF_SHADOW_TERM
				bsDesc.BlendRTDesc[1].BlendEnable = TRUE;
				bsDesc.BlendRTDesc[1].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
			#else
				bsDesc.BlendRTDesc[1].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_RGB;		// respect selfshadow term from underlying ground
			#endif
			bsDesc.BlendRTDesc[2].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
			bsDesc.BlendRTDesc[3].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
		#else
			bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_NONE;
			bsDesc.BlendRTDesc[1].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
			#if WRITE_SELF_SHADOW_TERM
				bsDesc.BlendRTDesc[2].BlendEnable = TRUE;
				bsDesc.BlendRTDesc[2].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
			#else
				bsDesc.BlendRTDesc[2].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_RGB;		// respect selfshadow term from underlying ground
			#endif
			bsDesc.BlendRTDesc[3].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
		#endif
		
		hGrassAlphaToMaskBS_pass2 = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hGrassAlphaToMaskBS_pass2 != grcStateBlock::BS_Invalid);
	}
		
	if(hGrassAlphaToMaskDSS_pass2 == grcStateBlock::DSS_Invalid)
	{
		grcDepthStencilStateDesc dsDesc;
		dsDesc.DepthEnable					= FALSE;
		dsDesc.DepthWriteMask				= FALSE;
		
		dsDesc.StencilEnable				= TRUE;
		dsDesc.FrontFace.StencilFunc		= grcRSV::CMP_EQUAL;
		dsDesc.FrontFace.StencilPassOp		= grcRSV::STENCILOP_ZERO;	// reset special bit
		dsDesc.FrontFace.StencilFailOp		= grcRSV::STENCILOP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;
		dsDesc.StencilReadMask				= 0x17;						// read bits 4,2..0
		dsDesc.StencilWriteMask				= 0x10;						// write only to bit 4

		hGrassAlphaToMaskDSS_pass2		= grcStateBlock::CreateDepthStencilState(dsDesc);
		Assert(hGrassAlphaToMaskDSS_pass2 != grcStateBlock::DSS_Invalid);
	}

	if(hGrassAlphaToMaskRS_pass2 == grcStateBlock::RS_Invalid)
	{
		grcRasterizerStateDesc rasDesc;
		rasDesc.CullMode	= grcRSV::CULL_NONE;
		hGrassAlphaToMaskRS_pass2	= grcStateBlock::CreateRasterizerState(rasDesc);
		Assert(hGrassAlphaToMaskRS_pass2 != grcStateBlock::RS_Invalid);
	}

	if(hGrassAlphaToMaskDSS_pass2_end == grcStateBlock::DSS_Invalid)
	{
		grcDepthStencilStateDesc dsDesc;
		dsDesc.DepthEnable					= TRUE;
		dsDesc.DepthWriteMask				= TRUE;
	
		dsDesc.StencilEnable				= FALSE;
		dsDesc.FrontFace.StencilFunc		= grcRSV::CMP_ALWAYS;

		hGrassAlphaToMaskDSS_pass2_end		= grcStateBlock::CreateDepthStencilState(dsDesc);
		Assert(hGrassAlphaToMaskDSS_pass2_end != grcStateBlock::DSS_Invalid);
	}

	if(hGrassAlphaToMaskRS_pass2_end == grcStateBlock::RS_Invalid)
	{
		grcRasterizerStateDesc rasDesc;
		rasDesc.CullMode	= grcRSV::CULL_BACK;
		hGrassAlphaToMaskRS_pass2_end	= grcStateBlock::CreateRasterizerState(rasDesc);
		Assert(hGrassAlphaToMaskRS_pass2_end != grcStateBlock::RS_Invalid);
	}

	if(hGrassAlphaToMaskBS_pass2_end == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.IndependentBlendEnable				= FALSE;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
		hGrassAlphaToMaskBS_pass2_end = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hGrassAlphaToMaskBS_pass2_end != grcStateBlock::BS_Invalid);
	}
#endif //PSN_ALPHATOMASK_PASS...

#if PLANTS_CAST_SHADOWS
	if(hGrassShadowRS == grcStateBlock::RS_Invalid)
	{
		grcRasterizerStateDesc rasDesc;
		rasDesc.CullMode = grcRSV::CULL_NONE;

		hGrassShadowRS = grcStateBlock::CreateRasterizerState(rasDesc);
		Assert(hGrassShadowRS != grcStateBlock::RS_Invalid);
	}

	if(hGrassShadowRS_end == grcStateBlock::RS_Invalid)
	{
		grcRasterizerStateDesc rasDesc;
		rasDesc.CullMode = grcRSV::CULL_BACK;

		hGrassShadowRS_end = grcStateBlock::CreateRasterizerState(rasDesc);
		Assert(hGrassShadowRS_end != grcStateBlock::RS_Invalid);
	}
#endif //PLANTS_CAST_SHADOWS

	return(TRUE);

}// end of InitialiseGrassRenderStateBlocks()...



//
//
//
//
void CPlantMgr::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
	    if(!gPlantMgr.InitialiseInternal())
        {
            Assertf(FALSE, "Failed to initialise CPlantMgr!");
        }
    }
}


bool CPlantMgr::InitialiseInternal()
{
		USE_MEMBUCKET(MEMBUCKET_RENDER);

		// PLANTS_MALLOC_STR requirement:
		Assertf(_IsPowerOfTwo(g_rscVirtualLeafSize), "g_rscVirtualLeafSize is NOT power-of-2!");

#if LOCTRISTAB_STR
		// do nothing
#else
		AllocateStrLocTriTabs();
#endif

		const u32 flags = 0;
		for(int i=0; i<CPLANT_LOC_TRIS_LIST_NUM; i++)
		{
			g_PlantMgrMemcpyDependency[i].Init( PlantsMgrAsyncMemcpyJob, 0, flags );
			g_PlantMgrMemcpyDependency[i].m_Priority = sysDependency::kPriorityLow;
		}

		if(!ReloadConfig())
		{
			return(FALSE);
		}

		if(!CGrassRenderer::Initialise())
		{
			return(FALSE);
		}

		if(!InitialiseGrassRenderStateBlocks())
		{
			return(FALSE);
		}

		// texture stuff:
		InitialiseTextures();

		//
		// model stuff:
		//
		strLocalIndex modelSlot = g_DwdStore.FindSlot(CPLANTMGR_MODELS_PATH);
		CStreaming::LoadObject(modelSlot, g_DwdStore.GetStreamingModuleId());
		g_DwdStore.AddRef(modelSlot, REF_OTHER);   
		gpPlantMgrModelDictionary = g_DwdStore.Get(modelSlot);
		Assert(gpPlantMgrModelDictionary);

		for(s32 i=0; i<CPLANT_SLOT_NUM_MODELS; i++)
		{
			char modelname[128];
			sprintf(modelname, "grass%02d", i);			// "grass00"
			LoadPlantModel(modelname,			&m_PlantModelsTab0[0+i]						);

			sprintf(modelname, "grass%02d_lod1", i);	// "grass00_lod1"
			LoadPlantModel(modelname,			&m_PlantModelsTab0[CPLANT_SLOT_NUM_MODELS+i]);

			sprintf(modelname, "grass%02d_lod2", i);	// "grass00_lod2"
			LoadPlantModelLOD2(modelname,		&m_PlantModelsTab0[CPLANT_SLOT_NUM_MODELS+i]);
		}

		// setup geometry buffers for GrassRenderer:
		CGrassRenderer::SetPlantModelsTab(PPPLANTBUF_MODEL_SET0,	m_PlantModelsTab0);
		CGrassRenderer::SetPlantModelsTab(PPPLANTBUF_MODEL_SET1,	NULL);
		CGrassRenderer::SetPlantModelsTab(PPPLANTBUF_MODEL_SET2,	NULL);
		CGrassRenderer::SetPlantModelsTab(PPPLANTBUF_MODEL_SET3,	NULL);

		if(!CreateGeometryLOD2())
		{
			return(FALSE);
		}

#if FURGRASS_TEST_V4
		if(!FurGrassLODInitialise())
			return(FALSE);
#endif

#if PLANTS_USE_LOD_SETTINGS
	// Default to high detail for now.
	CGrassRenderer_PlantLod plantLod = PLANTLOD_HIGH;

#if __BANK
	if (PARAM_grassandplants.Get())
	{
		u32 grassAndPlantsDetail=0;
		PARAM_grassandplants.Get(grassAndPlantsDetail);
		plantLod = (CGrassRenderer_PlantLod)grassAndPlantsDetail;
	}
#endif // __BANK

	SetupPlantLods(plantLod);
#endif //PLANTS_USE_LOD_SETTINGS

#if PLANTS_CAST_SHADOWS
	InitShadowCandidates();
#endif //PLANTS_CAST_SHADOWS
		return(TRUE);
}// end of CPlantMgr::Initialise()...

//
//
//
//
bool CPlantMgr::AllocateStrLocTriTabs()
{
	const u32 nLocTrisTabSize = sizeof(CPlantLocTriArray);

bool bSuccess=true;
s32  numAllocated=0;
	// allocate update buffers from streaming heap:
	for(s32 i=0; i<CPLANT_LOC_TRIS_LIST_NUM; i++)
	{
		Assert(m_LocTrisTab[i]==NULL);
		m_LocTrisTab[i] = (CPlantLocTriArray*)PLANTS_MALLOC_STR(nLocTrisTabSize, 128);
		if(m_LocTrisTab[i])
		{
			numAllocated++;
			plantsDisplayf("LocTris[]: Allocated %d/12 chunk of 64K.", numAllocated);
		}
		else
		{
			bSuccess = false;
		}
	
		Assert(m_LocTrisRenderTab[0][i]==NULL);
		m_LocTrisRenderTab[0][i] = (CPlantLocTriArray*)PLANTS_MALLOC_STR(nLocTrisTabSize, 128);
		if(m_LocTrisRenderTab[0][i])
		{
			numAllocated++;
			plantsDisplayf("LocTris[]: Allocated %d/12 chunk of 64K.", numAllocated);
		}
		else
		{
			bSuccess = false;
		}

		Assert(m_LocTrisRenderTab[1][i]==NULL);
		m_LocTrisRenderTab[1][i] = (CPlantLocTriArray*)PLANTS_MALLOC_STR(nLocTrisTabSize, 128);
		if(m_LocTrisRenderTab[1][i])
		{
			numAllocated++;
			plantsDisplayf("LocTris[]: Allocated %d/12 chunk of 64K.", numAllocated);
		}
		else
		{
			bSuccess = false;
		}
	}

	if(!bSuccess)
	{	// allocations failed
		plantsDisplayf("Allocation of LocTris[] from str memory failed (done %d out of 12)!", numAllocated);
		FreeStrLocTriTabs();	// free whatever was allocated
		return(false);
	}

	plantsDisplayf("Allocation of LocTris[] from str memory successful.");
	for(s32 i=0; i<CPLANT_LOC_TRIS_LIST_NUM; i++)
	{
		Assert(m_LocTrisTab[i]);
		Assert16(m_LocTrisTab[i]);
		sysMemSet(m_LocTrisTab[i], 0x00, nLocTrisTabSize);
	#if LOCTRISTAB_STR
		m_LocTrisTab[i]->Init( (u16)i );
	#endif

		Assert(m_LocTrisRenderTab[0][i]);
		Assert16(m_LocTrisRenderTab[0][i]);
		sysMemSet(m_LocTrisRenderTab[0][i], 0x00, nLocTrisTabSize);

		Assert(m_LocTrisRenderTab[1][i]);
		Assert16(m_LocTrisRenderTab[1][i]);
		sysMemSet(m_LocTrisRenderTab[1][i], 0x00, nLocTrisTabSize);
	}

	return(true);
}

//
//
//
//
bool CPlantMgr::FreeStrLocTriTabs()
{
#if LOCTRISTAB_STR
	// clear whole BoundsCache and Procedural Objects (via CPlantLocTri::Release()):
    u16 entry = gPlantMgr.m_ColEntCache.m_CloseListHead;
    while(entry)
    {
	    CPlantColBoundEntry *pEntry = &gPlantMgr.m_ColEntCache[entry];
	    entry = pEntry->m_NextEntry;
	    pEntry->ReleaseEntry();
    }
#endif

	for(s32 i=0; i<CPLANT_LOC_TRIS_LIST_NUM; i++)
	{
		if(m_LocTrisTab[i])
		{
			PLANTS_FREE_STR(m_LocTrisTab[i]);
			m_LocTrisTab[i] = NULL;
		}

		if(m_LocTrisRenderTab[0][i])
		{
			PLANTS_FREE_STR(m_LocTrisRenderTab[0][i]);
			m_LocTrisRenderTab[0][i] = NULL;
		}

		if(m_LocTrisRenderTab[1][i])
		{
			PLANTS_FREE_STR(m_LocTrisRenderTab[1][i]);
			m_LocTrisRenderTab[1][i] = NULL;
		}
	}

	plantsDisplayf("Freeing LocTris[] from str memory.");

	return(true);
}

//
//
//
//
bool CPlantMgr::UpdateStrLocTriTabs()
{
#if LOCTRISTAB_STR

enum // simple txd state machine:
{
	PLANTS_STR_FREED = 0,
	PLANTS_STR_ALLOCATING,
	PLANTS_STR_ALLOCATED
};

		// want LocTrisTabs[] allocated whenever txd!=0:
		const bool bWantAllocated = (CPopCycle::GetCurrentZonePlantsMgrTxdIdx() != 0);

const s32 COUNTDOWN_INIT= 400;	// wait 400 frames before state change
static s32 countdown	= COUNTDOWN_INIT;

static u8  currentState	= PLANTS_STR_FREED;

	switch(currentState)
	{
		case(PLANTS_STR_FREED):
		{
			if(bWantAllocated)
			{
				currentState = PLANTS_STR_ALLOCATING;
			}
		}
		break; //PLANTS_STR_FREED...
		
		case(PLANTS_STR_ALLOCATING):
		{
			if(AllocateStrLocTriTabs())
			{
				currentState = PLANTS_STR_ALLOCATED;
			}
			else
			{	// failed to allocate and don't want allocated anymore?
				if(bWantAllocated==false)
				{
					currentState = PLANTS_STR_FREED;
				}
			}
		}
		break; // PLANTS_STR_ALLOCATING...

		case(PLANTS_STR_ALLOCATED):
		{
			if(bWantAllocated)
			{
				countdown = COUNTDOWN_INIT;
			}
			else
			{
				if((--countdown)==0)
				{
					countdown = COUNTDOWN_INIT;
					FreeStrLocTriTabs();
					currentState = PLANTS_STR_FREED;
				}
			}
		}
		break; // PLANTS_STR_ALLOCATED...

		default:
			Assertf(0, "Invalid plant alloc str state!");
			break;
	};
#endif

	return(true);
}// end of UpdateStrLocTrisTabs()...

#if PLANTS_USE_LOD_SETTINGS
void SetupPlantLods(CGrassRenderer_PlantLod plantLod)
{
	CGrassRenderer::SetPlantLODToUse(plantLod);
}
#endif //PLANTS_USE_LOD_SETTINGS

//
//
// checks consistency of plant definitions in procedural meta:
// - textures;
//
bool CPlantMgr::CheckProceduralMeta()
{
#if PLANTS_TXD_STR
	return(true);
#else
	const u32 count = g_procInfo.m_plantInfos.GetCount();

	for(u32 i=0; i<count; i++)
	{
		CPlantInfo *pPlantInfo = &g_procInfo.m_plantInfos[i];
#if !__FINAL
		const char* name = g_procInfo.m_plantInfos[i].m_Tag.GetCStr();
#elif RSG_PC || !__NO_OUTPUT
		const char* name = "";
#endif
		if(!m_PlantTextureTab0[pPlantInfo->m_TextureId])
		{
			Quitf(ERR_GFX_CHECK_PROC_1,"\n PlantsMgr: %s trying to use empty texture from slot %d!", name, pPlantInfo->m_TextureId);
			return false;
		}

		if(!m_PlantTextureTab0[pPlantInfo->m_TextureId+CPLANT_SLOT_NUM_TEXTURES])
		{
			Quitf(ERR_GFX_CHECK_PROC_2,"\n PlantsMgr: %s trying to use empty LOD texture from slot %d!", name, pPlantInfo->m_TextureId);
			return false;
		}
	}

	return true;
#endif //PLANTS_TXD_STR...
}// end of CheckProceduralMeta()...

#if __BANK
static bool gbProceduralMetaChanged = FALSE;

void CPlantMgr::MarkReloadConfig()
{
	gbProceduralMetaChanged = true;
}

void ReloadProceduralMeta()
{
	FastAssert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	if(gbProceduralMetaChanged)
	{
		gbProceduralMetaChanged = false;

		// wait for any active renderThread stuff to finish:
		while(bRenderInternalActive)
		{
			sysIpcSleep(10);
		}

		const bool prevPlantMgrActive = gbPlantMgrActive;
		gbPlantMgrActive = FALSE;
		{
			gPlantMgr.ReloadConfig();
		}
		gbPlantMgrActive = prevPlantMgrActive;
	}
}

void ReloadProceduralDotDatCB()
{
	Printf("\n Reloading 'procedural.dat' (%d)...", GRCDEVICE.GetFrameCounter());
	// wait for any active renderThread stuff to finish:
	while(bRenderInternalActive)
	{
		sysIpcSleep(10);
	}

	const bool prevPlantMgrActive = gbPlantMgrActive;
	gbPlantMgrActive = FALSE;
	{
		// now reload procedural.dat data:
		g_procObjMan.ReloadData();
		gPlantMgr.ReloadConfig();
	}
	gbPlantMgrActive = prevPlantMgrActive;
}

static void ToggleCullingCB()
{
	// wait for any active renderThread stuff to finish:
	while(bRenderInternalActive)
	{
		sysIpcSleep(10);
	}

	const bool prevPlantMgrActive = gbPlantMgrActive;
	gbPlantMgrActive = FALSE;
	{
		// invalidate rasterizer state:
		grcStateBlock::DestroyRasterizerState(hGrassAlphaToMaskRS_pass1);
		hGrassAlphaToMaskRS_pass1 = grcStateBlock::RS_Invalid;
		// recreate rasterizer state:
		InitialiseGrassRenderStateBlocks();
	}
	gbPlantMgrActive = prevPlantMgrActive;
}

//
//
// debug widgets:
//
bool CPlantMgr::InitWidgets(bkBank& bank)
{
//	bkBank& bank = *BANKMGR.FindBank("Renderer");
	bank.PushGroup("PlantsMgr", false);
		// debug widgets:
		bank.AddToggle("CPlantMgr active",				&gbPlantMgrActive);
		bank.AddToggle("CPlantMgrRender active",		&gbPlantMgrRenderActive);
		bank.AddToggle("Display Debug Info",			&gbDisplayCPlantMgrInfo);
	#if PSN_PLANTSMGR_SPU_UPDATE
		bank.AddToggle("Print UpdateSpu job timings",	&gbPrintUpdateSpuJobTimings);
	#endif
		bank.PushGroup("Show LocTri Polys");
			bank.AddToggle("All Polys",					&gbShowCPlantMgrPolys);
			bank.AddToggle("Far Polys",					&gbShowCPlantMgrPolysFar);
			bank.AddToggle("Show Plant",				&gbShowCPlantMgrPolys_Plants);
			bank.AddToggle("Show ProcObj",				&gbShowCPlantMgrPolys_ProcObj);
		bank.PopGroup();
		bank.AddToggle("Show Proc Material Types",		&gbShowCPlantMgrProcMatType);
		bank.AddToggle("Show Ambient Scales",			&gbShowCPlantMgrAmbScale);
		bank.AddToggle("Show Alpha Overdraw",			&gbShowAlphaOverdraw);
		bank.AddToggle("Show LocTri Normals",			&gbShowCPlantMgrPolyNormals);
		bank.AddToggle("LOD0: Flash Instances",			&gbPlantsFlashLOD0);
		bank.AddToggle("LOD1: Flash Instances",			&gbPlantsFlashLOD1);
		bank.AddToggle("LOD2: Flash Instances",			&gbPlantsFlashLOD2);
		bank.AddToggle("LOD0: Disable Render",			&gbPlantsDisableLOD0);
		bank.AddToggle("LOD1: Disable Render",			&gbPlantsDisableLOD1);
		bank.AddToggle("LOD2: Disable Render",			&gbPlantsDisableLOD2);

		CGrassRenderer::InitWidgets(bank);

		bank.PushGroup("Wind Movement", false);
		{
			bank.AddToggle("Response Enabled",		&ms_bkbWindResponseEnabled);
			bank.AddSlider("Response Amount",		&ms_bkfWindResponseAmount,		0.0f, 50.0f, 1.0f/256.0f);
			bank.AddSlider("Response Vel Min",		&ms_bkfWindResponseVelMin,		0.0f, 64.0f, 1.0f/256.0f);
			bank.AddSlider("Response Vel Max",		&ms_bkfWindResponseVelMax,		0.0f, 64.0f, 1.0f/256.0f);
			bank.AddSlider("Response Vel Exp",		&ms_bkfWindResponseVelExp,		-4.0f, 4.0f, 1.0f/256.0f);
			bank.AddSlider("Average Wind Influence",&g_AvgPlantWindInfluence,		0.0f,  1.0f, 0.001f);
		}
		bank.PopGroup();

	#if CPLANT_CLIP_EDGE_VERT
		bank.PushGroup("Plant Edge/Vert Clipping", false);
		{
			bank.AddToggle("Enable plant/poly intersection clipping",		&gbPlantMgrClipEnable);
			bank.AddTitle("");
			bank.AddToggle("Enable poly edge intersection clipping",		&gbPlantMgrEdgeClipEnable);
			bank.AddToggle("Enable poly vert intersection clipping",		&gbPlantMgrVertClipEnable);
		}
		bank.PopGroup();
	#endif //CPLANT_CLIP_EDGE_VERT...

	#if CPLANT_DYNAMIC_CULL_SPHERES
		bank.PushGroup("Dynamic Cull Spheres", false);
		{
			bank.AddToggle("Enable Dynamic Cull Spheres",					&gbPlantMgrDynCullSpheresEnable);
			bank.PushGroup("--- Debugging ---", false);
			{
				bank.AddToggle("Debug Draw Cull Spheres", &gbPlantMgrDCSDebugDraw);
				bank.AddSeparator("");
				bank.AddVector("Sphere XYZ & Radius", &gPlantMgrDCSDbg_Sphere, -FLT_MAX / 1000.0f, FLT_MAX / 1000.0f, 1.0f);
				bank.AddSlider("Cull Sphere Handle", &gPlantMgrDCSDbg_Handle, 0, gPlantMgr.m_DynCullSpheres.size() - 1, 1);
				bank.AddButton("Add Sphere", PlantMgrDCSDbg_AddSphere, "Add sphere above to the Dynamic Cull Sphere List. Successful adds update the handle.");
				bank.AddButton("Remove Sphere", PlantMgrDCSDbg_RemoveSphere, "Remove dynamic cull sphere at the handle above.");
				bank.AddSeparator("");
				bank.AddButton("Center Sphere around Player", PlantMgrDCSDbg_CenterSphereOnPlayerPed, "Sets the center of the dbg sphere to the player ped's position. Useful shortcut function.");
				bank.PushGroup("Debug Draw Options", false);
				{
					bank.AddToggle("Draw Cull Spheres as solid", &gbPlantMgrDCSDebugDrawSolid);
					bank.AddColor("Debug Cull Sphere Color", &gPlantMgrDCSDebugDrawColor);
				}
				bank.PopGroup();
			}
			bank.PopGroup();
			bank.PushGroup("Cull Sphere Array", false);
			{
				for(DynCullSphereSourceArray::size_type i = 0; i < gPlantMgr.m_DynCullSpheres.size(); ++i)
				{
					char nameBuff[256];
					formatf(nameBuff, "Cull Sphere %d", i);
					bank.AddVector(nameBuff, &(gPlantMgr.m_DynCullSpheres[i].GetV4Ref()), -FLT_MAX / 1000.0f, FLT_MAX / 1000.0f, 0.1f);
				}
			}
			bank.PopGroup();
		}
		bank.PopGroup();
	#endif //CPLANT_DYNAMIC_CULL_SPHERES...
		
	#if PLANTSMGR_DATA_EDITOR
		gPlantMgrEditor.InitWidgets(bank, "Data Edit");
	#endif

		bank.AddSlider("Tris Far Distance:",					&gbPlantsLocTriFarDist,		CPLANT_TRILOC_FAR_DIST_INITVAL, CPLANT_TRILOC_FAR_DIST_INITVAL*2, 0.25f);

		bank.PushGroup("LOD Fading Control", false);
			bank.AddSlider("LOD0: Alpha Close Distance:",		&gbPlantsLOD0AlphaCloseDist,0.0f, CPLANT_TRILOC_FAR_DIST*2, 0.25f);
			bank.AddSlider("LOD0: Alpha Far Distance:",			&gbPlantsLOD0AlphaFarDist,	0.0f, CPLANT_TRILOC_FAR_DIST*2, 0.25f);
			bank.AddSlider("LOD0: Far Distance:",				&gbPlantsLOD0FarDist,		0.0f, CPLANT_TRILOC_FAR_DIST*2, 0.25f);

			bank.AddSlider("LOD1: Close Distance:",				&gbPlantsLOD1CloseDist,		0.0f, CPLANT_TRILOC_FAR_DIST*2, 0.25f);
			bank.AddSlider("LOD1: Far Distance:",				&gbPlantsLOD1FarDist,		0.0f, CPLANT_TRILOC_FAR_DIST*2, 0.25f);
			bank.AddSlider("LOD1: Close Alpha Close Distance:",	&gbPlantsLOD1Alpha0CloseDist,0.0f,CPLANT_TRILOC_FAR_DIST*2, 0.25f);
			bank.AddSlider("LOD1: Close Alpha Far Distance:",	&gbPlantsLOD1Alpha0FarDist,	0.0f, CPLANT_TRILOC_FAR_DIST*2, 0.25f);
			bank.AddSlider("LOD1: Far Alpha Close Distance:",	&gbPlantsLOD1Alpha1CloseDist,0.0f,CPLANT_TRILOC_FAR_DIST*2, 0.25f);
			bank.AddSlider("LOD1: Far Alpha Far Distance:",		&gbPlantsLOD1Alpha1FarDist,	0.0f, CPLANT_TRILOC_FAR_DIST*2, 0.25f);
			
			bank.AddSlider("LOD2: Close Distance:",				&gbPlantsLOD2CloseDist,		0.0f, CPLANT_TRILOC_FAR_DIST*2, 0.25f);
			bank.AddSlider("LOD2: Far Distance:",				&gbPlantsLOD2FarDist,		0.0f, CPLANT_TRILOC_FAR_DIST*2, 0.25f);
			bank.AddSlider("LOD2: Close Alpha Close Distance:",	&gbPlantsLOD2Alpha0CloseDist,0.0f,CPLANT_TRILOC_FAR_DIST*2, 0.25f);
			bank.AddSlider("LOD2: Close Alpha Far Distance:",	&gbPlantsLOD2Alpha0FarDist,	0.0f, CPLANT_TRILOC_FAR_DIST*2, 0.25f);
			bank.AddSlider("LOD2: Far Alpha Close Distance:",	&gbPlantsLOD2Alpha1CloseDist,0.0f,CPLANT_TRILOC_FAR_DIST*2, 0.25f);
			bank.AddSlider("LOD2: Far Alpha Far Distance:",		&gbPlantsLOD2Alpha1FarDist,	0.0f, CPLANT_TRILOC_FAR_DIST*2, 0.25f);
			bank.AddSlider("LOD2: Far Alpha Close Distance Far:",&gbPlantsLOD2Alpha1CloseDistFar,0.0f,CPLANT_TRILOC_FAR_DIST*2, 0.25f);
			bank.AddSlider("LOD2: Far Alpha Far Distance Far:",	 &gbPlantsLOD2Alpha1FarDistFar,  0.0f,CPLANT_TRILOC_FAR_DIST*2, 0.25f);
		bank.PopGroup();

		bank.PushGroup("Per-vertex Local Variation Data", false);	
			bank.AddToggle("Show per-vertex Ground Color/Scale/Density:",	&gbShowCPlantMgrGroundColor);
			bank.AddSlider("(0=color, 1=scaleXYZ, 2=scaleZ, 3=density",		&gbShowCPlantMgrGroundCSD, 0, 3, 1);
			bank.AddToggle("Force Default Ground Color",		&gbForceDefaultGroundColor);
			bank.AddColor( "Default Ground Color",				&gbDefaultGroundColor);
			bank.AddToggle("Force per-vertex Ground Colors",	&gbForceGroundColor);
			bank.AddToggle("Change all 3 Ground Colors at once",&gbChangeGroundColorsAllAsOne);
			bank.AddColor( "Ground Color at vertex#0",			&gbGroundColorV1, datCallback(cbBankChangeGroundColorV1));
			bank.AddColor( "Ground Color at vertex#1",			&gbGroundColorV2, datCallback(cbBankChangeGroundColorV2));
			bank.AddColor( "Ground Color at vertex#2",			&gbGroundColorV3, datCallback(cbBankChangeGroundColorV3));
			bank.AddSlider("Ground ScaleXYZ weight at vertex#0 (maps to <0; 1>)",			&gbGroundScaleXYZ_V1,	0, 15, 1);
			bank.AddSlider("Ground ScaleXYZ weight at vertex#1 (maps to <0; 1>)",			&gbGroundScaleXYZ_V2,	0, 15, 1);
			bank.AddSlider("Ground ScaleXYZ weight at vertex#2 (maps to <0; 1>)",			&gbGroundScaleXYZ_V3,	0, 15, 1);
			bank.AddSlider("Ground ScaleZ weight at vertex#0 (maps to [1, 0.6, 0.3, 0])",	&gbGroundScaleZ_V1,		0, 3, 1);
			bank.AddSlider("Ground ScaleZ weight at vertex#1 (maps to [1, 0.6, 0.3, 0])",	&gbGroundScaleZ_V2,		0, 3, 1);
			bank.AddSlider("Ground ScaleZ weight at vertex#2 (maps to [1, 0.6, 0.3, 0])",	&gbGroundScaleZ_V3,		0, 3, 1);
			bank.AddSlider("Ground Density weight at vertex#0 (maps to [1, 0.6, 0.3, 0])",	&gbGroundDensity_V1,	0, 3, 1);
			bank.AddSlider("Ground Density weight at vertex#1 (maps to [1, 0.6, 0.3, 0])",	&gbGroundDensity_V2,	0, 3, 1);
			bank.AddSlider("Ground Density weight at vertex#2 (maps to [1, 0.6, 0.3, 0])",	&gbGroundDensity_V3,	0, 3, 1);
		bank.PopGroup();
		
		bank.AddSlider("Skewing: Ground slope min angle (deg):",	&gbPlantsGroundSlopeAngleMin,	1.0f, 90.0, 0.5f);
		bank.AddToggle("Darkness: use timecycle (6-7am, 5-6pm)",	&gbPlantsDarknessTimecycle);
		bank.AddSlider("Darkness amount to add:",					&gbPlantsDarknessAmountAdd,		-1.0f,1.0,	0.01f);
#if CPLANT_PRE_MULTIPLY_LIGHTING
		bank.AddToggle("Premultiply NdotL + ambient",				&gbPlantsPremultiplyNdotL);
#endif
#if PLANTS_TXD_STR
		bank.AddToggle("PlantsMgrTxd str request",					&gbPlantsMgrTxdActive);
#endif
		bank.AddToggle("Geometry frustum culling",					&gbPlantsGeometryCulling);
#if CPLANT_USE_OCCLUSION
		bank.PushGroup("Occlusion", false);
			bank.AddToggle("Use occlusion",							&gbPlantsUseOcclusion);
			bank.AddSlider("Tri cube box height",					&gfPlantsOccCubeHeight,			0.0f, 5.0f, 0.1f);
			bank.AddToggle("Recalc AABBs",							&gbPlantsOccRecalcAABBs);
			bank.AddSlider("Test skip (0=all, 1=50%, 2=33%, 3=25%)",&gnPlantsOccSkipTest,			0, 3, 1);
		bank.PopGroup();
#endif
		bank.AddToggle("Double-sided culling",						&gbDoubleSidedCulling,			ToggleCullingCB);
		bank.AddButton("Reload \"procedural.dat\" file",			ReloadProceduralDotDatCB,		"Reloads \"procedural.dat\" and resets CPlantMgr.");

#if FURGRASS_TEST_V4
		bank.PushGroup("Fur grass", false);
			bank.AddSlider("Draw distance:",						&gfFurGrassMaxDist,				0.0f, 75.0f, 0.25f);
			bank.AddSlider("Alpha fade close:",						&gfFurGrassAlphaFadeClose,		0.0f, 75.0f, 0.25f);
			bank.AddSlider("Alpha fade far:",						&gfFurGrassAlphaFadeFar,		0.0f, 75.0f, 0.25f);

			bank.AddToggle("Unique texture per every layer",					&gbFurGrassSeparateTextures);
			//bank.AddSlider("Fur height combo tech (0=combo4tex, 1=combo2tex):",	&gnFurGrassSeparateTexturesTech,	0, 1, 1);
			bank.AddSlider("# of fur layers:",						&gnFurGrassNumLayers,			1, FURGRASS_MAX_LAYERS, 1);
			bank.PushGroup("Alpha clip", true);
				bank.AddSlider("Layer #7 (top-most):",				&gnFurGrassAlphaClip[7],		0, 255, 1);
				bank.AddSlider("Layer #6:",							&gnFurGrassAlphaClip[6],		0, 255, 1);
				bank.AddSlider("Layer #5:",							&gnFurGrassAlphaClip[5],		0, 255, 1);
				bank.AddSlider("Layer #4:",							&gnFurGrassAlphaClip[4],		0, 255, 1);
				bank.AddSlider("Layer #3:",							&gnFurGrassAlphaClip[3],		0, 255, 1);
				bank.AddSlider("Layer #2:",							&gnFurGrassAlphaClip[2],		0, 255, 1);
				bank.AddSlider("Layer #1:",							&gnFurGrassAlphaClip[1],		0, 255, 1);
				bank.AddSlider("Layer #0 (bottom-most):",			&gnFurGrassAlphaClip[0],		0, 255, 1);
			bank.PopGroup();
			bank.PushGroup("Grass 'darkening'", true);
				bank.AddSlider("Layer #7 (top-most):",				&gnFurGrassShadow[7],		0, 255, 1);
				bank.AddSlider("Layer #6:",							&gnFurGrassShadow[6],		0, 255, 1);
				bank.AddSlider("Layer #5:",							&gnFurGrassShadow[5],		0, 255, 1);
				bank.AddSlider("Layer #4:",							&gnFurGrassShadow[4],		0, 255, 1);
				bank.AddSlider("Layer #3:",							&gnFurGrassShadow[3],		0, 255, 1);
				bank.AddSlider("Layer #2:",							&gnFurGrassShadow[2],		0, 255, 1);
				bank.AddSlider("Layer #1:",							&gnFurGrassShadow[1],		0, 255, 1);
				bank.AddSlider("Layer #0 (bottom-most):",			&gnFurGrassShadow[0],		0, 255, 1);
			bank.PopGroup();
			bank.AddSlider("Fur Z offset:",							&gfFurGrassZOffset,				0.0f, 0.2f, 0.001f);
			bank.AddSlider("Fur step (divided by 10,000):",			&gnFurGrassFurStep,				0, 1000, 1);
			bank.AddSlider("UV scale (diffuse):",					&gfFurGrassUvScale,				0.0f, 16.0f, 0.001f);
			bank.AddSlider("UV scale (layer mask):",				&gfFurGrassUvScale2,			0.0f, 16.0f, 0.001f);
			bank.AddSlider("UV scale (normal):",					&gfFurGrassUvScale3,			0.0f, 16.0f, 0.001f);
			bank.AddSlider("Offset U:",								&gfFurGrassOffsetU,				0.0f, 1.0f, 0.001f);
			bank.AddSlider("Offset V:",								&gfFurGrassOffsetV,				0.0f, 1.0f, 0.001f);
			bank.AddSlider("Um scale X (divided by 100,000):",		&gnFurGrassUmScaleX,			0, 10000, 1);
			bank.AddSlider("Um scale Y (divided by 100,000):",		&gnFurGrassUmScaleY,			0, 10000, 1);

			bank.AddSlider("Fresnel:",								&gfFurGrassFresnel,				0.0f, 1.0f, 0.01f);
			bank.AddSlider("Spec Falloff:",							&gfFurGrassSpecFalloff,			0.0f, 512.0f, 0.1f);
			bank.AddSlider("Spec Intensity:",						&gfFurGrassSpecInt,				0.0f, 1.0f, 0.01f);
			bank.AddSlider("Bumpiness (divided by 1000):",			&gfFurGrassBumpiness,			0.0f, 10.0f, 0.01f);
			bank.AddToggle("Enable Mip Adjust:",					&gbEnableFurGrassMipmapLodBias);
			bank.AddSlider("Mip Adjust:",							&gfFurGrassMipmapLodBias,		-8.0f,8.0f, 0.125f);
		bank.PopGroup();
#endif
	bank.PopGroup();

	return(TRUE);
}
#endif //__BANK...


//
//
//
//
void CPlantColBoundEntryArray::Init()
{
	m_CloseListHead		= 0;	// NULL
	m_UnusedListHead	= 1;	// address of first valid element of m_ColEntCache

	for(u16 i=1; i<(CPLANT_COL_ENTITY_CACHE_SIZE+1); i++)
	{
		CPlantColBoundEntry *pEntry = &(*this)[i];
		pEntry->m__pEntity		= NULL;
		pEntry->m_bMaterialBound= false;
		pEntry->m_nBParentIndex	= -1;
		pEntry->m_nBChildIndex	= -1;
		pEntry->m_LocTriArray	= NULL;
		pEntry->m_nNumTris		= 0;
		pEntry->m_BoundMatProps.Kill();
#if PLANTSMGR_MULTI_UPDATE
		pEntry->m_processedTris.Kill();
#endif

		if(i==1)
			pEntry->m_PrevEntry = 0;
		else
			pEntry->m_PrevEntry = GetIdx(pEntry-1);
		
		if(i==CPLANT_COL_ENTITY_CACHE_SIZE)
			pEntry->m_NextEntry = 0;
		else
			pEntry->m_NextEntry = GetIdx(pEntry+1);
	}
}// end of CPlantColBoundEntryArray::Init()...


//
//
//
//
void CPlantLocTriArray::Init(u16 listID)
{
	m_listID = listID;

	m_CloseListHead		= 0;	// NULL
	m_UnusedListHead	= 1;	// address of first valid entry in m_LocTrisTab[]

	m_bRequireAmbScale	= false;

	for(u16 i=1; i<(CPLANT_MAX_CACHE_LOC_TRIS_NUM+1); i++)
	{
		CPlantLocTri *pLocTri	= &(*this)[i];

		pLocTri->m_V1			= Vector4(0,0,0,0);
		pLocTri->m_V2			= Vector4(0,0,0,0);
		pLocTri->m_V3			= Vector4(0,0,0,0);
		pLocTri->m_nSurfaceType	= 0;
		pLocTri->m_skewAxisAngle.w.SetFloat16_Zero();
		pLocTri->m_skewAxisAngle.x.SetFloat16_Zero();
		pLocTri->m_skewAxisAngle.y.SetFloat16_Zero();
		pLocTri->m_skewAxisAngle.z.SetFloat16_Zero();
#if CPLANT_STORE_LOCTRI_NORMAL || CPLANT_PRE_MULTIPLY_LIGHTING
		pLocTri->m_normal[0] = 
		pLocTri->m_normal[1] = 
		pLocTri->m_normal[2] = 0;
#endif

		if(i==1)
			pLocTri->m_PrevTri = 0;
		else
			pLocTri->m_PrevTri = this->GetIdx(pLocTri-1);
		
		if(i==CPLANT_MAX_CACHE_LOC_TRIS_NUM)
			pLocTri->m_NextTri = 0;
		else
			pLocTri->m_NextTri = this->GetIdx(pLocTri+1);
	}
}// end of CPlantLocTriArray::Init()...

//
//
// loads surface properties, initializes internal CPlantMgr lists:
//
bool CPlantMgr::ReloadConfig()
{
#if LOCTRISTAB_STR
	if(m_LocTrisTab[0])
#endif
	for(u16 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)
	{
		m_LocTrisTab[listID]->Init( listID );
	}

	m_ColEntCache.Init();

#if !__FINAL
	if (PARAM_DisablePlantMgr.Get())
		gbPlantMgrActive = false;
#endif

#if RSG_PC
	if(CSettingsManager::GetInstance().IsLowQualitySystem())
	{
		// AMD min spec video card + 4-core CPU. Extremely render thread 
		// bound in this case because of single-threaded driver, we cut 
		// plants manager (procedural grass + procedural trash, etc.) in 
		// this case.
		gbPlantMgrActive = false;
	}
#endif

	return(TRUE);

}// end of CPlantMgr::ReloadConfig()...

//
//
//
//
void CPlantMgr::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
        gPlantMgr.ShutdownInternal();
    }
    else if(shutdownMode == SHUTDOWN_SESSION)
    {
	    // clear whole BoundsCache:
	    u16 entry = gPlantMgr.m_ColEntCache.m_CloseListHead;
	    while(entry)
	    {
		    CPlantColBoundEntry *pEntry = &gPlantMgr.m_ColEntCache[entry];
		    entry = pEntry->m_NextEntry;
		    pEntry->ReleaseEntry();
	    }
    }
}

void CPlantMgr::ShutdownInternal()
{
	CPlantMgr::Shutdown(SHUTDOWN_WITH_MAP_LOADED);
	CGrassRenderer::Shutdown();

	FreeStrLocTriTabs();

	ShutdownTextures();

    for(s32 i=0; i<CPLANT_SLOT_NUM_MODELS*2; i++)
    {
        UnloadPlantModel(&m_PlantModelsTab0[i]);
    }

	// destroy allocated drawables:
	g_DwdStore.RemoveRef(g_DwdStore.FindSlot(CPLANTMGR_MODELS_PATH), REF_OTHER);
	gpPlantMgrModelDictionary = NULL;

}// end of CPlantMgr::Shutdown()...






#if !PSN_PLANTSMGR_SPU_UPDATE
//
// name:		CPlantMgr::UpdateTask
// description:	Task to do most of the plant mgr update
//
void CPlantMgr::UpdateTask(u32 listID, s32 iTriProcessSkipMask)
{
CPlantLocTriArray& triTab = *gPlantMgr.m_LocTrisTab[listID];

#if FURGRASS_TEST_V4
	u32 *pFurgrassTagPresent = &m_nFurgrassTagPresentTab[listID];
#else
	u32 *pFurgrassTagPresent = NULL;
#endif

	//
	// update LocTris in CEntities in _ColEntityCache:
	//
	UpdateAllLocTris(triTab, m_CameraPos, iTriProcessSkipMask, pFurgrassTagPresent);

}

void CPlantMgr::RenderTabMemcpy(u32 listID)
{
	Assert(listID < CPLANT_LOC_TRIS_LIST_NUM);

	CPlantLocTriArray& triTab = *gPlantMgr.m_LocTrisTab[listID];
	if(triTab.m_CloseListHead)
	{
		sysMemCpy(m_LocTrisRenderTab[GetUpdateRTBufferID()][listID], &triTab, sizeof(CPlantLocTriArray));
	}
	else
	{	// final list is empty after update - just erase target RT close list (it will be recopied next time src update list becomes non-empty):
		m_LocTrisRenderTab[GetUpdateRTBufferID()][listID]->m_CloseListHead = 0;
	}
}

#if PLANTSMGR_MULTI_UPDATE
	static void UpdatePlantMgrSubTask(sysTaskParameters &params)
	{
// 		sysThreadType::AddCurrentThreadType(sysThreadType::THREAD_TYPE_UPDATE);

		const u32 listID = params.UserData[0].asInt;
		Assert(listID < CPLANT_LOC_TRIS_LIST_NUM);
		const s32 iTriProcessSkipMask = params.UserData[1].asInt;

	#if ENABLE_PIX_TAGGING
		const char* pixTagName[] = {	"PlantsMgrUpdateTask0", "PlantsMgrUpdateTask1", "PlantsMgrUpdateTask2", "PlantsMgrUpdateTask3",
										"PlantsMgrUpdateTask4", "PlantsMgrUpdateTask5", "PlantsMgrUpdateTask6", "PlantsMgrUpdateTask7"	};
		PIXBegin(1, pixTagName[listID]);
	#else
		PIXBegin(1, "PlantsMgrUpdateTask");
	#endif


		gPlantMgr.UpdateTask(listID, iTriProcessSkipMask);

		PIXEnd();
	}
#else //PLANTSMGR_MULTI_UPDATE
	static void UpdatePlantMgrTask(sysTaskParameters &params)
	{
		PIXBegin(1, "PlantsMgrUpdateTask");

		const s32 iTriProcessSkipMask = params.UserData[1].asInt;

		for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)
		{
			gPlantMgr.UpdateTask(listID, iTriProcessSkipMask);
		}

		#if FURGRASS_TEST_V4
			// store fur grass pickup info for RT:
			gPlantMgr.FurGrassStoreRenderInfo(gPlantMgr.m_furGrassPickupRenderInfo[gPlantMgr.GetUpdateRTBufferID()], 0);
		#endif

		PIXEnd();
	}
#endif //PLANTSMGR_MULTI_UPDATE...
#endif //!PSN_PLANTSMGR_SPU_UPDATE... 

//
//
//
//
#if PLANTSMGR_MULTI_UPDATE
	static sysTaskHandle							s_PlantMgrTaskHandle[PLANTSMGR_MULTI_UPDATE_TASKCOUNT] = {0};
#else
	static sysTaskHandle							s_PlantMgrTaskHandle = 0;
#endif

#if PSN_PLANTSMGR_SPU_UPDATE
	DECLARE_TASK_INTERFACE(PlantsMgrUpdateSPU);
	static ProcObjectCreationInfo					s_SpuProcObjectCreationInfo[512*3+384]	;
	static ProcTriRemovalInfo						s_SpuProcTriRemovalInfo[512*3]			;
	static spuPlantsMgrUpdateStruct::CResultSize	s_PlantMgrResultSize;
#endif //PSN_PLANTSMGR_SPU_UPDATE...

//
//
//
//
bool CPlantMgr::UpdateBegin()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	USE_MEMBUCKET(MEMBUCKET_RENDER);

	if(!gbPlantMgrActive)	// skip updating, if varconsole switch is off
		return(FALSE);

#if LOCTRISTAB_STR
	if(m_LocTrisTab[0] == NULL)
	{	// loc tris tabs not allocated, so skip:
		return(TRUE);
	}
#endif

#if __BANK
	if (TiledScreenCapture::IsEnabled())
		return(FALSE);
	ReloadProceduralMeta();
#endif
#if __BANK
	// recalculate editable loctri far distances as required:
	ms_bkColTestRadius			= gbPlantsLocTriFarDist - 5.0f;
	ms_bkTriLocFarDist			= gbPlantsLocTriFarDist;
	ms_bkTriLocFarDistSqr		= ms_bkTriLocFarDist*ms_bkTriLocFarDist;
	ms_bkTriLocShortFarDist		= ms_bkTriLocFarDist - CPLANT_INCREASE_DIST;
	ms_bkTriLocShortFarDistSqr	= ms_bkTriLocShortFarDist*ms_bkTriLocShortFarDist;
#endif
#if __BANK
	CGrassRenderer::SetDebugModeEnable(gbShowAlphaOverdraw);
#endif


	CPlantMgr::AdvanceCurrentScanCode();

    const Vector3& cameraPos	= camInterface::GetPos();
	const Vec3V	   cameraPosV	= RCC_VEC3V(cameraPos);
    const Vector3& cameraFront	= camInterface::GetFront();

#if PLANTSMGR_DATA_EDITOR
	if(m_bRegenTriCaches)
	{
		m_bRegenTriCaches = false;
		CPlantMgr::PreUpdateOnceForNewCameraPos(cameraPos+Vector3(10000,10000,10000));
	}
#endif

	m_CameraPos = cameraPos;
	CGrassRenderer::SetGlobalCameraPos(cameraPos);
	CGrassRenderer::SetGlobalCameraFront(cameraFront);
	
	// is camera in first person perspective mode?
	bool bCameraFppEnabled = false;
	if(camInterface::GetGameplayDirectorSafe())
	{
		const camGameplayDirector& GameplayDirector = camInterface::GetGameplayDirector(); 
		const camFirstPersonAimCamera* pAimCamera = GameplayDirector.GetFirstPersonAimCamera(); 
		if(pAimCamera && camInterface::IsDominantRenderedCamera(*pAimCamera))
		{
			bCameraFppEnabled = true;
		}
	}
	CGrassRenderer::SetGlobalCameraFppEnabled(bCameraFppEnabled);
	
	CGrassRenderer::SetGlobalCameraUnderwater(Water::IsCameraUnderwater());

	CGrassRenderer::SetGlobalForceHDGrassGeometry(GetForceHDGrass());

	// get current player pos:
	const Vector3 vecPlayerPos = CGameWorld::FindLocalPlayerCoors();
	CGrassRenderer::SetGlobalPlayerPos(vecPlayerPos);

	CGrassRenderer::SetGlobalPlayerCollisionRSqr(1.0f);
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if(pPlayerPed)
	{
		CVehicle *pPlayerVeh = pPlayerPed->GetVehiclePedInside();
		if(pPlayerVeh)
		{
			const VehicleType vehType = pPlayerVeh->GetVehicleType();
			if(vehType==VEHICLE_TYPE_SUBMARINE)
			{
				CGrassRenderer::SetGlobalPlayerCollisionRSqr(3.2f*3.2f);	// BS#1494465: hack: override player collision for submersible
			}

			if(vehType==VEHICLE_TYPE_SUBMARINECAR)
			{
				CSubmarineCar *pSubmarineCar = (CSubmarineCar*)pPlayerVeh;
				if(pSubmarineCar->IsInSubmarineMode())
				{
					CGrassRenderer::SetGlobalPlayerCollisionRSqr(1.4f*1.4f);	// BS#1778095: hack: override player collision for underwater stromberg
				}
			}
		}

		if(pPlayerPed->GetPedType() == PEDTYPE_ANIMAL)
		{
			static dev_float peyoteScale = 0.01f;
			CGrassRenderer::SetGlobalPlayerCollisionRSqr(peyoteScale*peyoteScale);
		}
	}


spdTransposedPlaneSet8	cullFrustum;
#if NV_SUPPORT
	cullFrustum.SetStereo( *gVpMan.GetUpdateGameGrcViewport() );	
#else
	cullFrustum.Set( *gVpMan.GetUpdateGameGrcViewport() );
#endif

	// Interior portal:
bool bInteriorCullAll = false;

static dev_bool	bEnableInteriorTest = true;
	if(bEnableInteriorTest && g_SceneToGBufferPhaseNew && g_SceneToGBufferPhaseNew->GetPortalVisTracker() && g_SceneToGBufferPhaseNew->GetPortalVisTracker()->GetInteriorLocation().IsValid())
	{
		// the camera is in interior:
		// is the exterior visible at all with the current camera?
		bInteriorCullAll = !fwScan::GetScanResults().GetIsExteriorVisibleInGbuf();

		if(bInteriorCullAll==false)
		{
			// the part of the screen the exterior is visible in:
			fwScreenQuad quad = fwScan::GetScanResults().GetExteriorGbufScreenQuad();
			// calculate new 3D frustum to cull proc plants through portals:
			quad.GenerateFrustum(g_SceneToGBufferPhaseNew->GetGrcViewport().GetCullCompositeMtx(), cullFrustum);
		}
	}

	CGrassRenderer::SetGlobalCullFrustum(cullFrustum);			// current viewport's or interior portal's culling frustum
	CGrassRenderer::SetGlobalInteriorCullAll(bInteriorCullAll);


	bool bCameraVehicleFPV = false;	// camera in First Person View in vehicle?
#if FPS_MODE_SUPPORTED
	bCameraVehicleFPV = camInterface::GetCinematicDirector().IsRenderingAnyInVehicleFirstPersonCinematicCamera();
#endif

	// find 4 nearest vehicles to camera:
CVehicle*	pVehicles[CGrassRenderer::NUM_COL_VEH]		= {NULL,		NULL,		NULL,		NULL		};
float		fVehicleDist[CGrassRenderer::NUM_COL_VEH]	= {999999.0f,	999999.0f,	999999.0f,	999999.0f	};	// vehicle dist sqr to camera

	// Grab the spatial array.
	const CSpatialArray& vehSpatialArray = CVehicle::GetSpatialArray();
	const int sMaxResults = 16;
	static dev_float fMaxDistance = 40.0f; 
	CSpatialArray::FindResult result[sMaxResults];
	const int iFound = vehSpatialArray.FindInSphere(cameraPosV, fMaxDistance, &result[0], sMaxResults);

	int iEmptySlot = 0;
	//Iterate over the results.
	for(int iVeh=0; iVeh<iFound; iVeh++)
	{
		CVehicle* pVehicle = CVehicle::GetVehicleFromSpatialArrayNode(result[iVeh].m_Node);
		Assert(pVehicle);

		const VehicleType vehType = pVehicle->GetVehicleType();

		// disable vehicle collision for vehicles with 0 wheels (e.g. proptrailer):
		pVehicle = (!pVehicle || pVehicle->GetNumWheels()==0)? NULL : pVehicle;

		// disable vehicle collision for helis and planes:
		pVehicle = ((vehType==VEHICLE_TYPE_HELI) || (vehType==VEHICLE_TYPE_PLANE) || (vehType==VEHICLE_TYPE_BLIMP))?	NULL : pVehicle;
		pVehicle = ((vehType==VEHICLE_TYPE_BOAT) || (vehType==VEHICLE_TYPE_SUBMARINE))?									NULL : pVehicle;
		pVehicle = (vehType==VEHICLE_TYPE_BICYCLE)?																		NULL : pVehicle;

		// BS#1778095: special case for stromberg - collide only when NOT in submarine mode:
		if(pVehicle && vehType==VEHICLE_TYPE_SUBMARINECAR)
		{
			CSubmarineCar *pSubmarineCar = (CSubmarineCar*)pVehicle;
			pVehicle = pSubmarineCar->IsInSubmarineMode()? NULL : pVehicle;

			if(pVehicle)
			{
				// is stromberg attached to another vehicle (e.g. cargoplane - BS#1786565)?:
				const fwAttachmentEntityExtension *vehExt = pVehicle->GetAttachmentExtension();
				if(vehExt)
				{
					pVehicle = vehExt->GetIsAttached()? NULL : pVehicle;
				}
			}
		}

		if(pVehicle)
		{	// [HACK GTAV] BS#1105797: disable grass collision for dump:
			// [HACK GTAV] BS#1945307: disable grass collision for monster;
			// [HACK GTAV] BS#1998362: disable grass collision for trailers;
			const u32 uModelNameHash = pVehicle->GetBaseModelInfo()->GetModelNameHash();
			pVehicle = (uModelNameHash==MID_DUMP)?			NULL : pVehicle;
			pVehicle = (uModelNameHash==MID_MONSTER)?		NULL : pVehicle;
			pVehicle = (uModelNameHash==MID_TRAILERS)?		NULL : pVehicle;
			pVehicle = (uModelNameHash==MID_TRAILERS2)?		NULL : pVehicle;
			pVehicle = (uModelNameHash==MID_TRAILERS3)?		NULL : pVehicle;
			pVehicle = (uModelNameHash==MID_TRAILERLOGS)?	NULL : pVehicle;
		}

		if(pVehicle)
		{
			const float distSqr = result[iVeh].m_DistanceSq;

			if(iEmptySlot < CGrassRenderer::NUM_COL_VEH)
			{
				fVehicleDist[iEmptySlot]= distSqr;
				pVehicles[iEmptySlot]	= pVehicle;
				iEmptySlot++;
				continue;
			}

			// find vehicle's index which is most far away from the camera:
			u32		fari	= 0;
			float	farDist	= fVehicleDist[0];
			for(u32 i=1; i<CGrassRenderer::NUM_COL_VEH; i++)
			{
				if( fVehicleDist[i] > farDist )
				{
					farDist = fVehicleDist[i];
					fari	= i;
				}
			}
			Assert(fari < CGrassRenderer::NUM_COL_VEH);

			// check against current vehicle:
			// dist2 smaller than furthest found?
			if( distSqr < farDist )
			{
				fVehicleDist[fari]	= distSqr;
				pVehicles[fari]		= pVehicle;	
			}
		}
	}// (int iVeh=0; iVeh<iFound; iVeh++)...

	for(u32 v=0; v<CGrassRenderer::NUM_COL_VEH; v++)
	{
		CVehicle *pVehicle = pVehicles[v];
		
		if(bCameraVehicleFPV)
		{
			// BS#2057861: FPV in vehicle - always enable grass collision
		}
		else
		{
			if(pVehicle && (pVehicle->IsUpsideDown() || pVehicle->IsOnItsSide()))
			{
				pVehicle = NULL;	// no grass collision if vehicle is not firmly on the ground
			}

			// BS#2509157: check if lowrider hydraulics car is bouncing:
			bool bIsCarHydraulicsBouncing = false;
			if(pVehicle && pVehicle->InheritsFromAutomobile())
			{
				CAutomobile *pCar = (CAutomobile*)pVehicle;
				if(pCar && (pVehicle->m_nVehicleFlags.bPlayerModifiedHydraulics || pCar->m_nAutomobileFlags.bHydraulicsBounceRaising || pCar->m_nAutomobileFlags.bHydraulicsBounceLanding || pCar->m_nAutomobileFlags.bHydraulicsJump))
				{
					bIsCarHydraulicsBouncing = true;
				}
			}

			const u32 modelNameHash = pVehicle? pVehicle->GetBaseModelInfo()->GetModelNameHash() : 0;

			// BS#4036767 - Deluxo - Changing from driving to flying/floating affects grass collision
			bool bIsHooveringDeluxo = false;
			if(pVehicle && modelNameHash==MID_DELUXO)
			{
				bIsHooveringDeluxo = pVehicle->HoveringCloseToGround();
			}

			if(bIsCarHydraulicsBouncing)
			{
				// do nothing - vehicle is bouncing via hydraulics
			}
			else if(pVehicle && modelNameHash==MID_BLAZER5)
			{
				// do nothing - special case for blazer5
			}
			else if(bIsHooveringDeluxo)
			{
				// do nothing - deluxo is hoovering close to the ground
			}
			else if(pVehicle && (pVehicle->GetNumContactWheels() < pVehicle->GetNumWheels()))
			{
				pVehicle = NULL;	// no grass collision if vehicle is not firmly on the ground
			}

			// B*2491123: disable collision for attached vehicles:
			if(pVehicle && pVehicle->GetIsAttached())
			{
				CPhysical* pAttachParent = (CPhysical*)pVehicle->GetAttachParent();
				if(pAttachParent)
				{
					pVehicle = NULL;	// no grass collision for attached vehicles
				}
			}
		}

		if(pVehicle)
		{

			// grab unmodified (e.g. by opened cardoors, etc.) vehicle's bbox:
			const Vector3 bboxmin = pVehicle->GetBaseModelInfo()->GetBoundingBoxMin(); //pVehicle->GetBoundingBoxMin();
			const Vector3 bboxmax = pVehicle->GetBaseModelInfo()->GetBoundingBoxMax(); //pVehicle->GetBoundingBoxMax();
	//		Printf("\n cars bboxmin: %.2f, %.2f, %.2f", bboxmin.x, bboxmin.y, bboxmin.z);
	//		Printf("\n cars bboxmax: %.2f, %.2f, %.2f", bboxmax.x, bboxmax.y, bboxmax.z);
			
	//		Vector3 bboxminW = pVehicle->TransformIntoWorldSpace(bboxmin);
	//		Vector3 bboxmaxW = pVehicle->TransformIntoWorldSpace(bboxmax);
	//		Printf("\n cars bboxminW: %.2f, %.2f, %.2f", bboxminW.x, bboxminW.y, bboxminW.z);
	//		Printf("\n cars bboxmaxW: %.2f, %.2f, %.2f", bboxmaxW.x, bboxmaxW.y, bboxmaxW.z);
		
	//		example:
	//		cars bboxmin0:   -1.07,   -2.99,  -0.83
	//		cars bboxmax0:    1.07,    2.53,   0.76
	//		cars bboxmin: -1232.78, -970.34, 398.32
	//		cars bboxmax: -1230.58, -964.85, 399.89

			// two points: on the front and back of the car:
			float middleX = bboxmin.x + (bboxmax.x - bboxmin.x)*0.5f;
			float middleZ = bboxmin.z + (bboxmax.z - bboxmin.z)*0.5f;

		static dev_float radiusPerc = 1.05f;	// how much make bounding shape "shorter" to fit better around vehicle

			const u32 vehNameHash = pVehicle->GetBaseModelInfo()->GetModelNameHash();

			// front/rear Y shift overrides:
			float frontShiftY = 0.0f;
			float rearShiftY  = 0.0f;
			float sideShift	  = 0.0f;

			// BS#1777745: bodhi2: magic shift grass collision is not producing "halo" at the rear of vehicle:
			if(vehNameHash == MID_BODHI2)
			{
				rearShiftY = -0.7f;
			}

			// BS#1918403: Bulldozer flattens grass without being touched when bucket is up;
			if(vehNameHash == MID_BULLDOZER)
			{
				rearShiftY = -0.8f;

				// loop through gadgets until we found the digger arm
				for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
				{
					CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);
					if(pVehicleGadget->GetType() == VGT_DIGGER_ARM)
					{
						CVehicleGadgetDiggerArm *pDiggerArm = static_cast<CVehicleGadgetDiggerArm*>(pVehicleGadget);
						Assert(pDiggerArm);

						if(pDiggerArm)
						{
							const float fArmPosRatio = pDiggerArm->GetJointPositionRatio();

							static dev_float minPosRatio = 0.00f;
							static dev_float maxPosRatio = 0.36f;
							float fArmLerpRatio = Clamp((fArmPosRatio-minPosRatio)/(maxPosRatio-minPosRatio), 0.0f, 1.0f);
							frontShiftY = Lerp(fArmLerpRatio, -0.25f, 1.9f);
						}
					}
				}
			}// if(vehNameHash == MID_BULLDOZER)...


			// BS#1992186: granger side adjustments;
			if (vehNameHash == MID_GRANGER)
			{
				sideShift	= 0.42f;
			}
			else if(vehNameHash == MID_GRANGER2)
			{
				sideShift	= 0.50f;
			}
			// BS#1992186: riot side adjustments;
			else if (vehNameHash == MID_RIOT)
			{
				sideShift	= 0.40f;
			}
			// BS#1925880: zentorno side adjustments;
			else if (vehNameHash == MID_ZENTORNO)
			{
				sideShift	= 0.40f;
			}
			// BS#2278312: rhino front/rear adjustments:
			else if (vehNameHash == MID_RHINO)
			{
				frontShiftY	= 2.6f;
				rearShiftY	= -0.3f;
			}
			// BS#2333618: T20 side adjustments:
			else if (vehNameHash == MID_T20)
			{
				sideShift	= 0.45f;
			}
			// BS#3120202: Large Area around Phantom2 is cleared when driving off-road:
			else if (vehNameHash == MID_PHANTOM2)
			{
				frontShiftY	= 3.0f;
				sideShift	= 0.75f;
			}
			// BS#3334666 - GP1 - Large area cleared around vehicle
			else if (vehNameHash == MID_INFERNUS2)
			{
				sideShift	= 0.40f;
			}
			// BS#3334666 - GP1 - Large area cleared around vehicle
			else if (vehNameHash == MID_GP1)
			{
				sideShift	= 0.80f;
			}
			// BS#3477152 - XA-21 - Large area cleared around the vehicle
			else if (vehNameHash == MID_XA21)
			{
				sideShift	= 0.70f;
			}
			// BS#3888877 - Visione - Excessive collision of vegetation
			else if (vehNameHash == MID_VISIONE)
			{
				sideShift	= 0.50f;
			}
			// BS#4028275 - Riot2 - Excess grass & vegeatation clearance
			else if (vehNameHash == MID_RIOT2)
			{
				sideShift	= 0.6f;
				rearShiftY	= -0.4f;
			}
			// BS#4068209 - Barrage - Too much foliage cleared behind the vehicle
			else if (vehNameHash == MID_BARRAGE)
			{
				frontShiftY	= 0.4f;
				rearShiftY	= -1.0f;
				sideShift	= 0.3f;
			}
			// BS#4176577 - Chernobog - Area for flattening grass is too big
			else if (vehNameHash == MID_CHERNOBOG)
			{
				frontShiftY = 1.0f;
				rearShiftY	= -0.5f;
				sideShift	= 0.6f;
			}
			// BS#4194918 - Khanjali - Grass flattening area is larger than vehicle
			else if (vehNameHash == MID_KHANJALI)
			{
				frontShiftY = 1.0f;
				rearShiftY	= -0.4f;
				sideShift	= 0.3f;
			}
			// BS#198885 - Autarch - Grass collision bounds:
			else if (vehNameHash == MID_AUTARCH)
			{
				sideShift	= 0.5f;
				rearShiftY	= -0.2f;
			}
			// BS#4623613 - Excessive clearing of foliage around Mule4:
			else if (vehNameHash == MID_MULE4)
			{
				frontShiftY	= 0.5f;
				rearShiftY	= -0.4f;
				sideShift	= 0.85f;
			}
			// BS#4623697 - Pounder2 - Excessive clearing of foliage;
			else if (vehNameHash == MID_POUNDER2)
			{
				frontShiftY = 0.5f;
				sideShift	= 0.8f;
			}
			// BS#4702207 - pbus2 has too large area that flattens grass
			else if (vehNameHash == MID_PBUS2)
			{
				frontShiftY	= 1.0f;
				rearShiftY	= -3.1f;
				sideShift	= 1.3f;
			}
			// BS#5459805 - Monster3 - Foliage grass flatten area is very large on this vehicle.
			else if (vehNameHash == MID_MONSTER3 || vehNameHash == MID_MONSTER4 || vehNameHash == MID_MONSTER5)
			{
				frontShiftY	= 3.2f;
				rearShiftY	= -0.5f;
				sideShift	= 0.5f;
			}
			// BS#5388815 - Bruiser - Vehicle has a large foliage flatten area
			else if (vehNameHash == MID_BRUISER || vehNameHash == MID_BRUISER2 || vehNameHash == MID_BRUISER3)
			{
				frontShiftY = 1.2f;
				rearShiftY	= -1.0f;
				sideShift	= 0.7f;
			}
			// BS#5354204 - Scarab - Excessive foliage suppression
			else if (vehNameHash == MID_SCARAB || vehNameHash == MID_SCARAB2 || vehNameHash == MID_SCARAB3)
			{
				frontShiftY = 1.4f;
				rearShiftY	= -0.3f;
				sideShift	= 0.4f;
			}
			// BS#5350080 - Deveste - Vegetation cleared quite far
			else if (vehNameHash == MID_DEVESTE)
			{
				frontShiftY = -0.05f;
				rearShiftY	= 0.0f;
				sideShift	= 0.4f;
			}
			// BS#5415156 - ZR380 - Area that flattens grass is too big
			else if (vehNameHash == MID_ZR380 || vehNameHash == MID_ZR3802 || vehNameHash == MID_ZR3803)
			{
				frontShiftY = 0.6f;
				rearShiftY	= -0.2f;
				sideShift	= 0.35f;
			}
			// BS#5423756 - Cerberus - Vehicle has a large foliage flatten area
			else if (vehNameHash == MID_CERBERUS || vehNameHash == MID_CERBERUS2 || vehNameHash == MID_CERBERUS3)
			{
				frontShiftY	= 1.6f;
				rearShiftY	= -1.0f;
				sideShift	= 0.6f;
			}
			// BS#5459616 - Slamvan4 -  Foliage suppression too large
			else if (vehNameHash == MID_SLAMVAN4 || vehNameHash == MID_SLAMVAN5 || vehNameHash == MID_SLAMVAN6)
			{
				frontShiftY	= 0.35f;
				rearShiftY	= -0.75f;
				sideShift	= 0.1f;
			}
			// BS#5476478 - Dominator5 - excess foliage collision
			else if (vehNameHash == MID_DOMINATOR3)
			{
				rearShiftY	= -0.2f;
				sideShift	= 0.2f;
			}
			else if (vehNameHash == MID_DOMINATOR4 || vehNameHash == MID_DOMINATOR5 || vehNameHash == MID_DOMINATOR6)
			{
				frontShiftY	= 0.85f;
				rearShiftY	= 0.1f;
				sideShift	= 0.25f;
			}
			// BS#5457801 - Vehicles with Scoop Mods have large foliage flatten around it
			else if (vehNameHash == MID_BRUTUS || vehNameHash == MID_BRUTUS2 || vehNameHash == MID_BRUTUS3)
			{
				frontShiftY	= 1.3f;
				rearShiftY	= -0.5f;
				sideShift	= 0.3f;
			}
			else if (vehNameHash == MID_IMPALER2 || vehNameHash == MID_IMPALER3 || vehNameHash == MID_IMPALER4)
			{
				frontShiftY = 0.7f;
				rearShiftY	= -0.1f;
				sideShift	= 0.2f;
			}
			else if (vehNameHash == MID_IMPERATOR || vehNameHash == MID_IMPERATOR3)
			{
				frontShiftY	= 0.7f;
				rearShiftY	= -0.35f;
				sideShift	= 0.3f;
			}
			else if (vehNameHash == MID_IMPERATOR2)
			{
				frontShiftY = 0.4f;
				rearShiftY	= -0.35f;
				sideShift	= 0.3f;
			}
			else if (vehNameHash == MID_ISSI4 || vehNameHash == MID_ISSI5 || vehNameHash == MID_ISSI6)
			{
				frontShiftY	= 0.8f;
				rearShiftY	= -0.1f;
				sideShift	= 0.3f;
			}
			else if (vehNameHash == MID_DEATHBIKE || vehNameHash == MID_DEATHBIKE2 || vehNameHash == MID_DEATHBIKE3)
			{
				sideShift	= 0.4f;
			}
			else if (vehNameHash == MID_EMERUS)
			{
				rearShiftY	= -0.3f;
				sideShift	= 0.15f;
			}
			else if (vehNameHash == MID_NEO)
			{
				sideShift	= 0.3f;
			}
			else if (vehNameHash == MID_KRIEGER)
			{
				rearShiftY	= -0.2f;
				sideShift	= 0.1f;
			}
			else if (vehNameHash == MID_S80)
			{
				frontShiftY	= -0.1f;
				rearShiftY	= -0.2f;
			}
			else if (vehNameHash == MID_IMORGON)
			{
				frontShiftY	= -0.05f;
				rearShiftY	= -0.15f;
				sideShift	= 0.1f;
			}
			else if (vehNameHash == MID_FORMULA)
			{
				frontShiftY = -0.3f;
				sideShift	= 0.2f;
			}
			else if (vehNameHash == MID_REBLA)
			{
				sideShift	= 0.1f;
			}
			else if(vehNameHash == MID_POLBUFFALO)
			{
				frontShiftY	= -0.08f;
				rearShiftY	= 0.05f;
				sideShift	= 0.05f;
			}
			else if(vehNameHash == MID_YOUGA3)
			{
				frontShiftY	= 0.3f;
				rearShiftY	= 0.1f;
				sideShift	= 0.1f;
			}
			else if(vehNameHash == MID_YOUGA4)
			{
				frontShiftY	= 0.3f;
				rearShiftY	= -0.2f;
				sideShift	= 0.4f;
			}
			else if(vehNameHash == MID_OPENWHEEL1)
			{
				frontShiftY	= -0.05f;
				rearShiftY	= -0.2f;
				sideShift	= 0.2f;
			}
			else if(vehNameHash == MID_VERUS)
			{
				frontShiftY	= 0.15f;
				rearShiftY	= -0.3f;
				sideShift	= 0.1f;
			}
			else if(vehNameHash == MID_IGNUS)
			{
				rearShiftY	= -0.2f;
				sideShift	= 0.2f;
			}
			else if(vehNameHash == MID_DEITY)
			{
				sideShift	= 0.15f;
			}
			else if(vehNameHash == MID_ZENO)
			{
				sideShift	= 0.75f;
			}
			else if(vehNameHash == MID_COMET7)
			{
				sideShift	= 0.15f;
			}
			else if(vehNameHash == MID_BALLER7)
			{
				sideShift	= 0.15f;
			}
			else if(vehNameHash == MID_PATRIOT3)
			{
				sideShift	= 0.3f;
			}
			else if(vehNameHash == MID_TORERO2)
			{
				rearShiftY	= -0.1f;
				sideShift	= 0.4f;
			}
			else if(vehNameHash == MID_CORSITA)
			{
				frontShiftY	= 0.1f;
				rearShiftY	= -0.2f;
				sideShift	= 1.05f;
			}
			else if(vehNameHash == MID_KANJOSJ)
			{
				frontShiftY	= 0.2f;
				sideShift	= 0.2f;
			}
			else if(vehNameHash == MID_WEEVIL2)
			{
				frontShiftY	= 0.3f;
				rearShiftY	= -0.5f;
				sideShift	= 0.4f;
			}

		static dev_bool bOverrideFrontShiftY=false;
			if(bOverrideFrontShiftY)
			{
				static dev_float frontShiftYOverride = 1.9f;
				frontShiftY = frontShiftYOverride;
			}

		static dev_bool bOverrideRearShiftY=false;
			if(bOverrideRearShiftY)
			{
				static dev_float rearShiftYOverride = -0.7f;
				rearShiftY = rearShiftYOverride;
			}

		static dev_bool bOverrideSideShift=false;
			if(bOverrideSideShift)
			{
				static dev_float sideShiftOverride = 0.2f;
				sideShift = sideShiftOverride;
			}

			float   Radius = (bboxmax.x - bboxmin.x)*0.5f - sideShift;	// radius
			Vector3 ptb(middleX, bboxmax.y - Radius*radiusPerc - frontShiftY, middleZ);
			Vector3 pta(middleX, bboxmin.y + Radius*radiusPerc - rearShiftY, middleZ);

			// first point on segment:
			Vector3 ptB = pVehicle->TransformIntoWorldSpace(ptb);
			// last point on segment:
			Vector3 ptA = pVehicle->TransformIntoWorldSpace(pta);
			// segment vector:
			Vector3 vecM = ptA - ptB;

	//		Printf("\n ptB:  %.2f, %.2f, %2.f", ptB.x, ptB.y, ptB.z);
	//		Printf("\n ptA:  %.2f, %.2f, %2.f", ptA.x, ptA.y, ptA.z);
	//		Printf("\n vecM: %.2f, %.2f, %2.f; radius=%.2f", vecM.x, vecM.y, vecM.z, Radius);

			// detect groundZ pos (from wheel hit pos):	
			Vector3 wheelHitPos(0,0,100000);
			const s32 numWheels = pVehicle->GetNumWheels();
			for(s32 i=0; i<numWheels; i++)
			{
				if(pVehicle->GetWheel(i))
				{
					// select wheelHitPos with lowest Z (to allow for steep slopes - see BS#978552, BS#1725071):
					Vector3 wheelPos = pVehicle->GetWheel(i)->GetHitPos();
					if(wheelPos.z < wheelHitPos.z)
					{
						wheelHitPos = wheelPos;
					}
				}
			}
			
			static dev_bool bOverrideZ = FALSE;
			static float overrideZ = 398.44f; // test for testbed
			float groundZ = bOverrideZ? overrideZ : wheelHitPos.z;

			static dev_float tweakRadiusScale = 1.0f;		// v1 collision
			//static dev_float tweakRadiusScale = 1.25f;	// v2 collision
			CGrassRenderer::SetGlobalVehCollisionParams(v, TRUE, ptB, vecM, Radius*tweakRadiusScale, groundZ);
		}
		else
		{
			CGrassRenderer::SetGlobalVehCollisionParams(v, FALSE, Vector3(0,0,0), Vector3(0,0,0), 0, 0);
		}
	}//	for(u32 v=0; v<CGrassRenderer::NUM_COL_VEH; v++)...


#if FURGRASS_TEST_V4
	if(pPlayerPed)
	{
		const s32 leftFootBoneIdx  = pPlayerPed->GetBoneIndexFromBoneTag(BONETAG_L_FOOT);
		const s32 rightFootBoneIdx = pPlayerPed->GetBoneIndexFromBoneTag(BONETAG_R_FOOT);

		if (leftFootBoneIdx != -1 && rightFootBoneIdx != -1)
		{
			Matrix34 leftFootMat;
			Matrix34 rightFootMat;
			pPlayerPed->GetGlobalMtx(leftFootBoneIdx, leftFootMat);
			pPlayerPed->GetGlobalMtx(rightFootBoneIdx, rightFootMat);

			// move positions forward, so fur collision circle better encircles both feet:
			const Vector3 vecForward(1,0,0);
			Vector3 LForward, RForward;
			leftFootMat.Transform3x3(vecForward,  LForward);
			rightFootMat.Transform3x3(vecForward, RForward);
static dev_float scaleLFwdX = 0.1f;
static dev_float scaleLFwdY = 0.1f;
static dev_float scaleLFwdZ = 0.0f;
			const Vector3 vecScaleFwd(scaleLFwdX, scaleLFwdY, scaleLFwdZ);

			Vector4 lFootPos(leftFootMat.d	+ LForward*vecScaleFwd);
			Vector4 rFootPos(rightFootMat.d	+ RForward*vecScaleFwd);
			lFootPos.w = rFootPos.w = 0.0f;
			CGrassRenderer::SetGlobalPlayerFeetPos(lFootPos, rFootPos);
		}
	}
#endif //FURGRASS_TEST_V4...

	Vector2 bending;
	CPlantMgr::CalculateWindBending(bending);
	bending.x = rage::Abs(bending.x);
	bending.y = rage::Abs(bending.y);
	CGrassRenderer::SetGlobalWindBending(bending);

#if __BANK
	const float amount = gbPlantsDarknessAmountAdd;
#else
	const float amount = 0.0f;
#endif
	CPlantMgr::CalculateFakeGrassNormal(amount);


	//
	// update ColEntityCache:
	//
	static u8 nUpdateEntCache = 0;

	//
	// do not update this stuff too often:
	//
	if( !((++nUpdateEntCache)&(CPLANT_COL_ENTITY_UPDATE_CACHE-1)) )
	{
		// full update of entry cache (FULL update):
		_ColBoundCache_Update(m_CameraPos);
	}
	else
	{
		// quick update:
		_ColBoundCache_Update(m_CameraPos, TRUE);
	}


#if PSN_PLANTSMGR_SPU_UPDATE
	// launch SPU job:
	const u32 outputSize	= 0;
	const u32 scratchSize	= 0;
	const u32 plantsBaseSize= (SPU_SIZEOF_CPLANTMGRBASE+1024)/1024; // 68KB
#if CPLANT_DYNAMIC_CULL_SPHERES || CPLANT_CLIP_EDGE_VERT
	const u32 stackSize		= (86+plantsBaseSize)*1024;
#else
	const u32 stackSize		= (89+plantsBaseSize)*1024;
#endif
	sysTaskContext c(TASK_INTERFACE(PlantsMgrUpdateSPU), outputSize, scratchSize, stackSize);
	c.SetInputOutput();
	c.AddInput(&g_procInfo, sizeof(g_procInfo));	// 27.64KB
	c.AddInput(g_procInfo.m_procObjInfos.GetElements(), sizeof(CProcObjInfo) * g_procInfo.m_procObjInfos.GetCount());
	c.AddInput(g_procInfo.m_plantInfos.GetElements(), sizeof(CPlantInfo) * g_procInfo.m_plantInfos.GetCount());
	spuPlantsMgrUpdateStruct& structUpdate = *c.AllocUserDataAs<spuPlantsMgrUpdateStruct>(); // 0.08KB
	structUpdate.m_pPlantsMgr			= this;
	structUpdate.m_camPos				= cameraPos;
	structUpdate.m_GroundSlopeAngleMin	= PlantsMinGroundAngleSlope;
	structUpdate.m_bCullSphereEnabled0	= m_CullSphereEnabled[0];
	structUpdate.m_bCullSphereEnabled1	= m_CullSphereEnabled[1];
	structUpdate.m_cullSphere[0]		= m_CullSphere[0];
	structUpdate.m_cullSphere[1]		= m_CullSphere[1];
#if __BANK
		structUpdate.m_gbForceDefaultGroundColor		= gbForceDefaultGroundColor;
		structUpdate.m_gbDefaultGroundColor				= gbDefaultGroundColor;
		structUpdate.m_bkTriLocFarDistSqr				= ms_bkTriLocFarDistSqr;
		structUpdate.m_bkTriLocShortFarDistSqr			= ms_bkTriLocShortFarDistSqr;
		// g_procObjMan flags:
		structUpdate.m_ignoreMinDist					= g_procObjMan.m_ignoreMinDist;
		structUpdate.m_forceOneObjPerTri				= g_procObjMan.m_forceOneObjPerTri;
		structUpdate.m_ignoreSeeding					= g_procObjMan.m_ignoreSeeding;
		structUpdate.m_enableSeeding					= g_procObjMan.m_enableSeeding;
		structUpdate.m_disableCollisionObjects			= g_procObjMan.m_disableCollisionObjects;
		structUpdate.m_printSpuJobTimings				= gbPrintUpdateSpuJobTimings;
		#if PLANTSMGR_DATA_EDITOR
			structUpdate.m_AllCollisionSelectable		= GetAllCollisionSelectableUT();
		#endif
	#endif
	structUpdate.m_IsNetworkGameInProgress				= NetworkInterface::IsGameInProgress();
	
	structUpdate.m_addBufSize			= 256;
	c.AddOutput(structUpdate.m_addBufSize * sizeof(ProcObjectCreationInfo));	// 8KB
	
	structUpdate.m_removeBufSize		= 256;
	c.AddOutput(structUpdate.m_removeBufSize * sizeof(ProcTriRemovalInfo));		// 2KB

	c.AddOutput(sizeof(spuPlantsMgrUpdateStruct::CResultSize));					// 0.02KB
	
	structUpdate.m_maxAdd				= NELEM(s_SpuProcObjectCreationInfo);
	structUpdate.m_maxRemove			= NELEM(s_SpuProcTriRemovalInfo);
	structUpdate.m_pAddList				= &s_SpuProcObjectCreationInfo[0];
	structUpdate.m_pRemoveList			= &s_SpuProcTriRemovalInfo[0];
	structUpdate.m_pResultSize			= &s_PlantMgrResultSize;
	structUpdate.m_iTriProcessSkipMask	= fwTimer::GetSystemFrameCount() & (CPLANT_ENTRY_TRILOC_PROCESS_UPDATE-1);

	const u32 bufferID					= GetUpdateRTBufferID();
	for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)
	{
		structUpdate.m_LocTrisTab[listID]		= m_LocTrisTab[listID];
		structUpdate.m_LocTrisRenderTab[listID]	= m_LocTrisRenderTab[bufferID][listID];
	}
	
	#if FURGRASS_TEST_V4
		structUpdate.m_furGrassPickupRenderInfo = m_furGrassPickupRenderInfo[bufferID];
	#endif
	
	Assertf(s_PlantMgrTaskHandle==0, "PlantMgr: SPU Update task already running!");
	s_PlantMgrTaskHandle = c.Start(sysTaskManager::SCHEDULER_DEFAULT_SHORT);

#else //PSN_PLANTSMGR_SPU_UPDATE...

	static u8 nLocTriSkipCounter=0;
	const s32 iTriProcessSkipMask = (nLocTriSkipCounter++)&(CPLANT_ENTRY_TRILOC_PROCESS_UPDATE-1);

	// Stop creating proc objects when the player is traveling over a certain velocity
	m_bSuppressObjCreation = bool(g_ContinuityMgr.GetPlayerVelocity() >= OBJ_CREATION_SPEED_LIMIT);
	m_bSuppressObjCreation |= m_bSuppressObjCreationPermanently;
#if RSG_PC
	if(CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShaderQuality == CSettings::Low)
	{
		m_bSuppressObjCreation |= true;
	}
#endif

#if FURGRASS_TEST_V4
	for(u32 i=0; i<CPLANT_LOC_TRIS_LIST_NUM; i++)
	{
		m_nFurgrassTagPresentTab[i] = false;
	}
#endif

	// add update plantmgr task
	#if PLANTSMGR_MULTI_UPDATE
		sysTaskParameters params;
		for(u32 t=0; t<PLANTSMGR_MULTI_UPDATE_TASKCOUNT; t++)
		{
			Assertf(s_PlantMgrTaskHandle[t]==0, "PlantMgr: Update task %d already running!", t);
			params.UserData[0].asInt = t;	// 0-7
			params.UserData[1].asInt = iTriProcessSkipMask;
			s_PlantMgrTaskHandle[t] = rage::sysTaskManager::Create(TASK_INTERFACE(UpdatePlantMgrSubTask), params);
		}
	#else //PLANTSMGR_MULTI_UPDATE
		sysTaskParameters params;
		params.UserData[1].asInt = iTriProcessSkipMask;
		Assertf(s_PlantMgrTaskHandle==0, "PlantMgr: Update task already running!");
		s_PlantMgrTaskHandle = rage::sysTaskManager::Create(TASK_INTERFACE(UpdatePlantMgrTask), params);
	#endif //PLANTSMGR_MULTI_UPDATE...

#endif //PSN_PLANTSMGR_SPU_UPDATE...

	// we ran the update tasks this frame, so we also want to run the memcpy jobs
	m_bShouldRunAsyncMemcpyJobThisFrame = true;

	return(TRUE);
}// end of Update()...


//
//
//
//
void CPlantMgr::UpdateEnd()
{
	if(!gbPlantMgrActive)	// skip updating, if varconsole switch is off
		return;

#if __BANK
	if (TiledScreenCapture::IsEnabled())
		return;
#endif

#if LOCTRISTAB_STR
	if(m_LocTrisTab[0] == NULL)
	{	// loc tris tabs not allocated, so skip:
		return;
	}
#endif

#if CPLANT_DYNAMIC_CULL_SPHERES
	CopyCullSpheresToUpdateBuffer();
#endif

#if PLANTSMGR_MULTI_UPDATE
	for(u32 t=0; t<PLANTSMGR_MULTI_UPDATE_TASKCOUNT; t++)
	{
		if(s_PlantMgrTaskHandle[t])
		{
			sysTaskManager::Wait( s_PlantMgrTaskHandle[t] );
			s_PlantMgrTaskHandle[t] = 0;
		}
	}
	#if FURGRASS_TEST_V4
		// store fur grass pickup info for RT:
		gPlantMgr.FurGrassStoreRenderInfo(gPlantMgr.m_furGrassPickupRenderInfo[gPlantMgr.GetUpdateRTBufferID()], 0);
	#endif
#else //PLANTSMGR_MULTI_UPDATE
	if(s_PlantMgrTaskHandle)
	{
		sysTaskManager::Wait(s_PlantMgrTaskHandle);
		s_PlantMgrTaskHandle = 0;
	}
#endif //PLANTSMGR_MULTI_UPDATE...


#if FURGRASS_TEST_V4
	const u32 bufferIdx = GetUpdateRTBufferID();
	// establish wheter to render fur grass or not:
	m_bFurgrassDoRender[bufferIdx] = false;
	for(u32 i=0; i<CPLANT_LOC_TRIS_LIST_NUM; i++)
	{
		m_bFurgrassDoRender[bufferIdx] |= bool(m_nFurgrassTagPresentTab[i]!=0); // results from subupdate tasks
	}

	m_bFurgrassDoRender[bufferIdx] |= GetFurgrassTrisRtPresent();	// + feedback from Furgrass::Render()
#endif


	#if PLANTSMGR_DATA_EDITOR
		gPlantMgrEditor.UpdateSafe();
	#endif


#if PSN_PLANTSMGR_SPU_UPDATE
	// CProcObjectMan::ProcessAddAndRemoveLists():
	// process SPU generated lists of objects to add/remove
const ProcTriRemovalInfo* pRemove		= s_SpuProcTriRemovalInfo;
const ProcTriRemovalInfo* pRemoveEnd	= pRemove + s_PlantMgrResultSize.m_numRemove;
	for(; pRemove != pRemoveEnd; pRemove++)
	{
		const CTriHashIdx16 hashIdx = u16(u32(pRemove->pLocTri));
		const u16 listID	= hashIdx.GetListID();
		const u16 idx		= hashIdx.GetIdx();
		CPlantLocTriArray& triTab = *gPlantMgr.m_LocTrisTab[listID];
		CPlantLocTri *pRemoveLocTri = &triTab[idx]; 

		g_procObjMan.ProcessTriangleRemoved(pRemoveLocTri, pRemove->procTagId);
	}// for(; pRemove != pRemoveEnd; pRemove++)...

const ProcObjectCreationInfo* pAdd		= s_SpuProcObjectCreationInfo;
const ProcObjectCreationInfo* pAddEnd	= pAdd + s_PlantMgrResultSize.m_numAdd;
	for(; pAdd != pAddEnd; pAdd++)
	{		
		CProcObjInfo* pProcObjInfo = (CProcObjInfo*)(pAdd->pos.uw + (u8*)&g_procInfo.m_procObjInfos[0]);

		const CTriHashIdx16 hashIdx = (u16)pAdd->normal.uw;
		const u16 listID	= hashIdx.GetListID();
		const u16 idx		= hashIdx.GetIdx();
		CPlantLocTriArray& triTab	= *gPlantMgr.m_LocTrisTab[listID];
		CPlantLocTri* pLocTri		= &triTab[idx];

		if(pLocTri->m_pParentEntity)
		{
			CEntityItem* pEntityItem = g_procObjMan.AddObject(pAdd->pos, pAdd->normal, 
											pProcObjInfo, &pProcObjInfo->m_EntityList,
											const_cast<CEntity*>(pLocTri->m_pParentEntity), pLocTri->GetSeed());
			if (pEntityItem)
			{
				pEntityItem->m_pParentTri = pLocTri;
			#if __BANK
				pEntityItem->triVert1[0] = pLocTri->m_V1.x;
				pEntityItem->triVert1[1] = pLocTri->m_V1.y;
				pEntityItem->triVert1[2] = pLocTri->m_V1.z;

				pEntityItem->triVert2[0] = pLocTri->m_V2.x;
				pEntityItem->triVert2[1] = pLocTri->m_V2.y;
				pEntityItem->triVert2[2] = pLocTri->m_V2.z;

				pEntityItem->triVert3[0] = pLocTri->m_V3.x;
				pEntityItem->triVert3[1] = pLocTri->m_V3.y;
				pEntityItem->triVert3[2] = pLocTri->m_V3.z;
			#endif // __BANK
			}
			else
			{
			#if __BANK
				g_procObjMan.m_bkNumProcObjsSkipped++;
			#endif
			}
		}
	}// for(; pAdd != pAddEnd; pAdd++)...

	#if __BANK
		g_procObjMan.m_bkNumProcObjsToAdd		= s_PlantMgrResultSize.m_numAdd;
		g_procObjMan.m_bkNumProcObjsToAddSize	= NELEM(s_SpuProcObjectCreationInfo);
		g_procObjMan.m_bkNumTrisToRemove		= s_PlantMgrResultSize.m_numRemove;
		g_procObjMan.m_bkNumTrisToRemoveSize	= NELEM(s_SpuProcTriRemovalInfo);
	#endif

	#if __DEV && 0
		static u32 maxAdd = 0;
		if (s_PlantMgrResultSize.m_numAdd > maxAdd)
		{
			maxAdd = s_PlantMgrResultSize.m_numAdd;
			Displayf("PlantsMgrUpdateSPU: most added=%d.", maxAdd);
		}
		static u32 maxRemove = 0;
		if (s_PlantMgrResultSize.m_numRemove > maxRemove)
		{
			maxRemove = s_PlantMgrResultSize.m_numRemove;
			Displayf("PlantsMgrUpdateSPU: most removed=%d.", maxRemove);
		}
	#endif //__DEV...
#endif //PSN_PLANTSMGR_SPU_UPDATE...


const fwInteriorLocation interiorLocation;

	// recalculate ambient scale from TC:
	if(IsAmbScaleScanEnabled())
	{
		for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)
		{
			CPlantLocTriArray& triTab = *m_LocTrisTab[listID];
			if(triTab.m_bRequireAmbScale)
			{
				triTab.m_bRequireAmbScale = false;

				u16 loctri = triTab.m_CloseListHead;
				while(loctri)
				{
					CPlantLocTri *pLocTri = &triTab[loctri];

					// sample ambient TC:
					if(pLocTri->m_bRequireAmbScale)
					{
						pLocTri->m_bRequireAmbScale = false;

						const Vector3 center = pLocTri->GetCenter();
						const Vec3V centerV3 = RCC_VEC3V(center);

						const Vec2V vAmbientScale = g_timeCycle.CalcAmbientScale(centerV3, interiorLocation);

						Assertf(vAmbientScale.GetXf() >= 0.0f && vAmbientScale.GetXf() <= 1.0f,"Dynamic Natural Ambient is out of range (%f, should be between 0.0f and 1.0f)",vAmbientScale.GetXf());
						Assertf(vAmbientScale.GetYf() >= 0.0f && vAmbientScale.GetYf() <= 1.0f,"Dynamic Artificial Ambient is out of range (%f, should be between 0.0f and 1.0f)",vAmbientScale.GetYf());
						const Vec2V vScaledAmbientScale = vAmbientScale * ScalarV(255.0f);
						pLocTri->m_nAmbientScale[0] = u8(vScaledAmbientScale.GetXf());	// natural ambient
						pLocTri->m_nAmbientScale[1] = u8(vScaledAmbientScale.GetYf());	// artificial ambient
					}

					loctri = pLocTri->m_NextTri;
				}//while(pLocTri)...
			}
		}//for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)...
	}//if(IsAmbScaleScanEnabled())...

}// end of CPlantMgr::EndUpdate()...

//
//
//
//
void CPlantMgr::AsyncMemcpyBegin()
{
	if(m_bShouldRunAsyncMemcpyJobThisFrame)
	{
		g_PlantMgrMemcpyDependenciesRunning = CPLANT_LOC_TRIS_LIST_NUM;
		
		for(int i=0; i<CPLANT_LOC_TRIS_LIST_NUM; i++)
		{
			g_PlantMgrMemcpyDependency[i].m_Params[0].m_AsInt = i;
			g_PlantMgrMemcpyDependency[i].m_Params[1].m_AsPtr = &g_PlantMgrMemcpyDependenciesRunning;
			sysDependencyScheduler::Insert(&g_PlantMgrMemcpyDependency[i]);
		}
	}

	m_bShouldRunAsyncMemcpyJobThisFrame = false;
}

void CPlantMgr::AsyncMemcpyEnd()
{
	PF_PUSH_TIMEBAR_IDLE("CPlantMgr AsyncMemcpyEnd");

	while(true)
	{
		volatile u32 *pDependenciesRunning = &g_PlantMgrMemcpyDependenciesRunning;
		if(*pDependenciesRunning == 0)
		{
			break;
		}
		
		sysIpcYield(PRIO_NORMAL);
	}

	PF_POP_TIMEBAR();
}

//
//
//
//
#if CPLANT_USE_OCCLUSION

static spdAABB	sm_loctriAABB[CPLANT_LOC_TRIS_LIST_NUM][CPLANT_MAX_CACHE_LOC_TRIS_NUM];

//
//
//
//
void CPlantMgr::UpdateOcclusion()
{
#if __BANK
	nLocTrisOccluded=0;

	if(!gbPlantsUseOcclusion)
		return;

	if(gbPlantsOccRecalcAABBs)
	{
		gbPlantsOccRecalcAABBs = false;
	
		for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)
		{
			CPlantLocTriArray& triTab = *gPlantMgr.m_LocTrisTab[listID];

			u16 loctri = triTab.m_CloseListHead;
			while(loctri)
			{
				CPlantLocTri *pLocTri = &triTab[loctri];

				pLocTri->m_bNeedsAABB = true;

				loctri = pLocTri->m_NextTri;
			}//while(pLocTri)...
		}
	}
#endif//__BANK...


	extern rage::fwScanBaseInfo	g_scanBaseInfo;
	rage::fwScanBaseInfo *scanBaseInfo = &g_scanBaseInfo;

	const Vec4V *pHiZBuffer = COcclusion::GetHiZBuffer(SCAN_OCCLUSION_STORAGE_PRIMARY);
	Assert(pHiZBuffer);

	const float fTriCubeHeight = BANK_SWITCH(gfPlantsOccCubeHeight,2.0f);
	const s32 nSkipTest = BANK_SWITCH(gnPlantsOccSkipTest,0);

	s32 skipTest = nSkipTest;
	for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)
	{
		CPlantLocTriArray& triTab = *gPlantMgr.m_LocTrisTab[listID];
		spdAABB	*tabAabb = sm_loctriAABB[listID];

		u16 loctri = triTab.m_CloseListHead;
		while(loctri)
		{
			CPlantLocTri *pLocTri = &triTab[loctri];

			spdAABB *aabb = &tabAabb[loctri-1];

			if(pLocTri->m_bNeedsAABB)
			{
				pLocTri->m_bNeedsAABB = false;

				// calculate AABB for the loc tri:
				aabb->Invalidate();

				// construct AABB for the loctri: base + base offseted by 2.0m up:
				Vector3 V1 = pLocTri->GetV1();
				Vector3 V2 = pLocTri->GetV2();
				Vector3 V3 = pLocTri->GetV3();
				aabb->GrowPoint(RCC_VEC3V(V1));
				aabb->GrowPoint(RCC_VEC3V(V2));
				aabb->GrowPoint(RCC_VEC3V(V3));

				V1.z += fTriCubeHeight;
				V2.z += fTriCubeHeight;
				V3.z += fTriCubeHeight;
				aabb->GrowPoint(RCC_VEC3V(V1));
				aabb->GrowPoint(RCC_VEC3V(V2));
				aabb->GrowPoint(RCC_VEC3V(V3));
			}

		#if __BANK
			if(skipTest)
			{
				skipTest--;
				pLocTri->m_bOccluded = false;
			}
			else
			{
				skipTest = nSkipTest;
				ScalarV area = rstTestAABBExactEdgeList( scanBaseInfo->m_transform[SCAN_OCCLUSION_STORAGE_PRIMARY],
								ScalarVFromF32(scanBaseInfo->m_depthRescale[SCAN_OCCLUSION_STORAGE_PRIMARY]),
								pHiZBuffer, RCC_VEC3V(m_CameraPos), aabb->GetMin(), aabb->GetMax(),
								ScalarV(V_ZERO), scanBaseInfo->m_minMaxBounds[SCAN_OCCLUSION_STORAGE_PRIMARY],
								scanBaseInfo->m_minZ[SCAN_OCCLUSION_STORAGE_PRIMARY]
								DEV_ONLY(, &(scanBaseInfo->m_TrivialAcceptActiveFrustumCount[SCAN_OCCLUSION_STORAGE_PRIMARY]), &(scanBaseInfo->m_TrivialAcceptMinZCount[SCAN_OCCLUSION_STORAGE_PRIMARY]), &(scanBaseInfo->m_TrivialAcceptVisiblePixelCount[SCAN_OCCLUSION_STORAGE_PRIMARY]))
							#if __BANK
								, THRESHOLD_PIXELS_SIMPLE_TEST
								, /*useTrivialAcceptTest*/true, /*useTrivialAcceptVisiblePixelTest*/true
							#endif
								);
				pLocTri->m_bOccluded = IsGreaterThanAll(area, ScalarV(V_ZERO))==0;
			}
		#else
				ScalarV area = rstTestAABBExactEdgeList( scanBaseInfo->m_transform[SCAN_OCCLUSION_STORAGE_PRIMARY],
								ScalarVFromF32(scanBaseInfo->m_depthRescale[SCAN_OCCLUSION_STORAGE_PRIMARY]),
								pHiZBuffer, RCC_VEC3V(m_CameraPos), aabb->GetMin(), aabb->GetMax(),
								ScalarV(V_ZERO), scanBaseInfo->m_minMaxBounds[SCAN_OCCLUSION_STORAGE_PRIMARY],
								scanBaseInfo->m_minZ[SCAN_OCCLUSION_STORAGE_PRIMARY]
								DEV_ONLY(, &(scanBaseInfo->m_TrivialAcceptActiveFrustumCount[SCAN_OCCLUSION_STORAGE_PRIMARY]), &(scanBaseInfo->m_TrivialAcceptMinZCount[SCAN_OCCLUSION_STORAGE_PRIMARY]), &(scanBaseInfo->m_TrivialAcceptVisiblePixelCount[SCAN_OCCLUSION_STORAGE_PRIMARY]))
							#if __BANK
								, THRESHOLD_PIXELS_SIMPLE_TEST
								, /*useTrivialAcceptTest*/true, /*useTrivialAcceptVisiblePixelTest*/true
							#endif
								);
				pLocTri->m_bOccluded = IsGreaterThanAll(area, ScalarV(V_ZERO))==0;
		#endif
	
			#if __BANK
				nLocTrisOccluded += u32(pLocTri->m_bOccluded);
			#endif

			loctri = pLocTri->m_NextTri;
		}//while(pLocTri)...
	}

}
#endif // CPLANT_USE_OCCLUSION...

//
//
// Update streaming buffers:
//
void CPlantMgr::UpdateStr()
{
	FastAssert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	CGrassRenderer::UpdateStr();

#if PLANTS_TXD_STR
	UpdateStrTextures();
#endif

#if LOCTRISTAB_STR
	UpdateStrLocTriTabs();
#endif

}// end of CPlantMgr::UpdateStr()...

// wrapper for interfacing the plants manager with the game skeleton code
void CPlantMgrWrapper::UpdateBegin()
{
    gPlantMgr.UpdateBegin();
}

void CPlantMgrWrapper::UpdateEnd()
{
    gPlantMgr.UpdateEnd();
}

#if CPLANT_USE_OCCLUSION
void CPlantMgrWrapper::UpdateOcclusion()
{
    gPlantMgr.UpdateOcclusion();
}
#endif

void CPlantMgrWrapper::UpdateSafeMode()
{
    gPlantMgr.UpdateStr();
	gPlantMgr.SwapRTBuffer();	// swap buffer for update task


#if PLANTSMGR_DATA_EDITOR
	// pass over signals from Editor to gPlantMgr:
	gPlantMgr.SetAllCollisionSelectable(gPlantMgrEditor.IsAllCollisionSelectable()); 
	if(gPlantMgrEditor.GetRegenTriCache())
	{
		gPlantMgrEditor.SetRegenTriCache(false);
		gPlantMgr.ForceRegenTriCaches(true);
	}
#endif
}

//
// this function helps to avoid ugly visual "rebuilding" of plants while
// camera is teleported into new remote location;
//
// to be called ONCE before teleporting camera / player to new far position:
//
bool CPlantMgr::PreUpdateOnceForNewCameraPos(const Vector3& newCameraPos)
{
#if __BANK
	if(audNorthAudioEngine::GetOcclusionManager()->GetIsOcclusionBuildEnabled() && audNorthAudioEngine::GetOcclusionManager()->GetIsBuildingOcclusionPaths())
	{
		return true;
	}
#endif
	return gPlantMgr.PreUpdateOnceForNewCameraPosInternal(newCameraPos);
}

bool CPlantMgr::PreUpdateOnceForNewCameraPosInternal(const Vector3& newCameraPos)
{
#if LOCTRISTAB_STR
	if(m_LocTrisTab[0]==NULL)
	{
		return(TRUE);
	}
#endif

	g_procObjMan.Shutdown();
	g_procObjMan.Init();

	CPlantMgr::AdvanceCurrentScanCode();
	
	CGrassRenderer::SetGlobalCameraPos(newCameraPos);

	Vector2 bending;
	CPlantMgr::CalculateWindBending(bending);
	CGrassRenderer::SetGlobalWindBending(bending);

	m_bSuppressObjCreation = false;
	m_bSuppressObjCreation |= m_bSuppressObjCreationPermanently;


	//
	// empty + update ColEntityCache:
	//
	// force updating all EntityCache:
	_ColBoundCache_Update(newCameraPos);
	_ColBoundCache_Update(newCameraPos);
	
	//
	// update ALL LocTris in CEntities in _ColEntityCache:
	//
#if PSN_PLANTSMGR_SPU_UPDATE
	for(u32 update=0; update<2; update++)
	{
		// launch SPU job:
		const u32 outputSize	= 0;
		const u32 scratchSize	= 0;
		const u32 plantsBaseSize= (SPU_SIZEOF_CPLANTMGRBASE+1024)/1024; // 68KB
	#if CPLANT_DYNAMIC_CULL_SPHERES || CPLANT_CLIP_EDGE_VERT
		const u32 stackSize		= (86+plantsBaseSize)*1024;
	#else
		const u32 stackSize		= (89+plantsBaseSize)*1024;
	#endif
		sysTaskContext c(TASK_INTERFACE(PlantsMgrUpdateSPU), outputSize, scratchSize, stackSize);
		c.SetInputOutput();
		c.AddInput(&g_procInfo, sizeof(g_procInfo));	// 27.64KB
		c.AddInput(g_procInfo.m_procObjInfos.GetElements(), sizeof(CProcObjInfo) * g_procInfo.m_procObjInfos.GetCount());
		c.AddInput(g_procInfo.m_plantInfos.GetElements(), sizeof(CPlantInfo) * g_procInfo.m_plantInfos.GetCount());
		
		spuPlantsMgrUpdateStruct& structUpdate = *c.AllocUserDataAs<spuPlantsMgrUpdateStruct>(); // 0.08KB
		structUpdate.m_pPlantsMgr			= this;
		structUpdate.m_camPos				= newCameraPos;
		structUpdate.m_GroundSlopeAngleMin	= PlantsMinGroundAngleSlope;
		structUpdate.m_bCullSphereEnabled0	= m_CullSphereEnabled[0];
		structUpdate.m_bCullSphereEnabled1	= m_CullSphereEnabled[1];
		structUpdate.m_cullSphere[0]		= m_CullSphere[0];
		structUpdate.m_cullSphere[1]		= m_CullSphere[1];
		#if __BANK
			structUpdate.m_gbForceDefaultGroundColor		= gbForceDefaultGroundColor;
			structUpdate.m_gbDefaultGroundColor				= gbDefaultGroundColor;
			// g_procObjMan flags:
			structUpdate.m_ignoreMinDist					= g_procObjMan.m_ignoreMinDist;
			structUpdate.m_forceOneObjPerTri				= g_procObjMan.m_forceOneObjPerTri;
			structUpdate.m_ignoreSeeding					= g_procObjMan.m_ignoreSeeding;
			structUpdate.m_enableSeeding					= g_procObjMan.m_enableSeeding;
			structUpdate.m_disableCollisionObjects			= g_procObjMan.m_disableCollisionObjects;
			structUpdate.m_printSpuJobTimings				= gbPrintUpdateSpuJobTimings;
			#if PLANTSMGR_DATA_EDITOR
				structUpdate.m_AllCollisionSelectable		= GetAllCollisionSelectableUT();
			#endif
		#endif
		structUpdate.m_IsNetworkGameInProgress				= NetworkInterface::IsGameInProgress();
		
		structUpdate.m_addBufSize			= 256;
		c.AddOutput(structUpdate.m_addBufSize * sizeof(ProcObjectCreationInfo));	// 8KB
		
		structUpdate.m_removeBufSize		= 256;
		c.AddOutput(structUpdate.m_removeBufSize * sizeof(ProcTriRemovalInfo));		// 2KB

		c.AddOutput(sizeof(spuPlantsMgrUpdateStruct::CResultSize));					// 0.02KB
		
		structUpdate.m_maxAdd				= NELEM(s_SpuProcObjectCreationInfo);
		structUpdate.m_maxRemove			= NELEM(s_SpuProcTriRemovalInfo);
		structUpdate.m_pAddList				= &s_SpuProcObjectCreationInfo[0];
		structUpdate.m_pRemoveList			= &s_SpuProcTriRemovalInfo[0];
		structUpdate.m_pResultSize			= &s_PlantMgrResultSize;
		structUpdate.m_iTriProcessSkipMask	= CPLANT_ENTRY_TRILOC_PROCESS_ALWAYS;

		const u32 bufferID					= GetUpdateRTBufferID();
		for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)
		{
			structUpdate.m_LocTrisTab[listID]		= m_LocTrisTab[listID];
			structUpdate.m_LocTrisRenderTab[listID]	= m_LocTrisRenderTab[bufferID][listID];
		}
		
		#if FURGRASS_TEST_V4
			structUpdate.m_furGrassPickupRenderInfo = m_furGrassPickupRenderInfo[bufferID];
		#endif

		if(s_PlantMgrTaskHandle)
		{
			sysTaskManager::Wait(s_PlantMgrTaskHandle);
			s_PlantMgrTaskHandle = 0;
		}
		Assertf(s_PlantMgrTaskHandle==0, "PlantMgr: SPU Update task already running!");
		s_PlantMgrTaskHandle = c.Start(sysTaskManager::SCHEDULER_DEFAULT_SHORT);
		
		UpdateEnd();
	}
#else //PSN_PLANTSMGR_SPU_UPDATE...

	#if FURGRASS_TEST_V4
		for(u32 i=0; i<CPLANT_LOC_TRIS_LIST_NUM; i++)
		{
			m_nFurgrassTagPresentTab[i] = false;
		}
	#endif

	for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)
	{
	#if FURGRASS_TEST_V4
		u32 *pFurgrassTagPresent = &m_nFurgrassTagPresentTab[listID];
	#else
		u32 *pFurgrassTagPresent = NULL;
	#endif
		UpdateAllLocTris(*m_LocTrisTab[listID], newCameraPos, CPLANT_ENTRY_TRILOC_PROCESS_ALWAYS, pFurgrassTagPresent);
		UpdateAllLocTris(*m_LocTrisTab[listID], newCameraPos, CPLANT_ENTRY_TRILOC_PROCESS_ALWAYS, pFurgrassTagPresent);
	}

	#if FURGRASS_TEST_V4
		// establish wheter to render fur grass or not:
		const u32 bufferIdx = GetUpdateRTBufferID();
		m_bFurgrassDoRender[bufferIdx] = false;
		for(u32 i=0; i<CPLANT_LOC_TRIS_LIST_NUM; i++)
		{
			m_bFurgrassDoRender[bufferIdx] |= bool(m_nFurgrassTagPresentTab[i]!=0);
		}

		m_bFurgrassDoRender[bufferIdx] |= GetFurgrassTrisRtPresent();	// + feedback from Furgrass::Render()
	#endif

#endif //PSN_PLANTSMGR_SPU_UPDATE...

	return(TRUE);
}


//
//
//
//
bool CPlantMgr::_ColBoundCache_Update(const Vector3& cameraPos, bool bQuickUpdate)
{

const u16 nCurrentScanCode = CPlantMgr::GetCurrentScanCode();

	if(bQuickUpdate)
	{
		// Quick Update:
		if(m_ColEntCache.m_CloseListHead)
		{
			u16 entry = m_ColEntCache.m_CloseListHead;
			while(entry)
			{
				CPlantColBoundEntry *pEntry = &m_ColEntCache[entry];
				entry = pEntry->m_NextEntry;
				// not valid pointer to parent (pEntity or bound in static bounds store)?
				if(!pEntry->IsParentValid())
				{
					pEntry->ReleaseEntry();
				}
				else
				{
				#if PLANTSMGR_MULTI_UPDATE
					// reset processed tris cache:
					pEntry->m_processedTris.Reset();
				#endif
				}
			}
		}
	}
	else // if(bQuickUpdate)...
	{
		// Full update: 

		// Use this option to get all objects matching the req'd state
	#if PLANTS_USE_LOD_SETTINGS
		float fColTestRadius = CPLANTMGR_COL_TEST_RADIUS*CGrassRenderer::GetDistanceMultiplier();
	#else
		float fColTestRadius = CPLANTMGR_COL_TEST_RADIUS;
	#endif
	#if PLANTSMGR_DATA_EDITOR
		if(GetAllCollisionSelectableUT())
		{
			fColTestRadius *= 0.333f;	// use 1/3 of range when editing wuth "select all" mode
		}
	#endif

#if 0	// WEAPON based collision is disabled
		phLevelNew * phLevel = CPhysics::GetLevel();
		Assert(phLevel);

#if ENABLE_PHYSICS_LOCK
		phIterator iterator(phIterator::PHITERATORLOCKTYPE_READLOCK);
#else	// ENABLE_PHYSICS_LOCK
		phIterator iterator;
#endif	// ENABLE_PHYSICS_LOCK

		iterator.InitCull_Sphere(cameraPos, fColTestRadius);			// ::InitCull_All();

		iterator.SetStateIncludeFlags(phLevelNew::STATE_FLAG_FIXED);// | phLevelNew::STATE_FLAG_ACTIVE | phLevelNew::STATE_FLAG_INACTIVE);

		u16 iIndexInLevel = phLevel->GetFirstCulledObject(iterator);
		while(iIndexInLevel != phInst::INVALID_INDEX)
		{
			phInst *pInstance = phLevel->GetInstance(iIndexInLevel);
			if(pInstance)
			{
				phArchetype *pArchetype = pInstance->GetArchetype();
				if(pArchetype)
				{
					phBound *pBound = pArchetype->GetBound();
					Assert(pBound);

					if(pInstance->GetArchetype()->GetTypeFlags()&ArchetypeFlags::GTA_MAP_TYPE_WEAPON)
					{
						CEntity* pEntity = CPhysics::GetEntityFromInst(pInstance);
						Assert(pEntity);
						_ColBoundCache_ProcessBound(pBound, pEntity, -1, -1, nCurrentScanCode);
					}
				}
			}//if(pInstance)...

			iIndexInLevel = phLevel->GetNextCulledObject(iterator);
		}// while(iIndexInLevel != phInst::INVALID_INDEX)...
#endif //#if 0...

		// MATERIAL-type collision:
		if(true)
		{
			//g_StaticBoundsStore
			Vector4 sphereV4(cameraPos);
			sphereV4.w = fColTestRadius;
			spdSphere searchVol(VECTOR4_TO_VEC4V(sphereV4));

			// first search for static bounds files that are asset type MATERIAL
			atArray<u32> slotIndices;
			g_StaticBoundsStore.GetBoxStreamer().GetIntersectingAABB(spdAABB(searchVol), fwBoxStreamerAsset::FLAG_STATICBOUNDS_MATERIAL, slotIndices, false);

			// run over slots for any ones which are loaded
			for(s32 i=0; i<slotIndices.GetCount(); i++)
			{
				strLocalIndex index = strLocalIndex((s32)slotIndices[i]);
				if((index!=-1) && (g_StaticBoundsStore.GetPtr(index)!=NULL))
				{
					fwBoundDef* pDef = g_StaticBoundsStore.GetSlot(index);
					if (pDef && pDef->m_pObject && pDef->m_pObject->GetType() == phBound::COMPOSITE)
					{
						phBoundComposite* pComposite = (phBoundComposite*)(pDef->m_pObject);
						const s32 numBounds = pComposite->GetNumBounds();
						for(s32 i=0; i<numBounds; i++)
						{
							phBound* pBound = pComposite->GetBound(i);
							if(pBound)
							{
								_ColBoundCache_ProcessBound(pBound, NULL, index.Get(), i, nCurrentScanCode);
							}
						}
					}
				}
			}
		}

		// go through BoundCache and remove Entrys with no current scancode
		//  (probably they are too far from iterator):
		if(m_ColEntCache.m_CloseListHead)
		{
			u16 entry = m_ColEntCache.m_CloseListHead;
			while(entry)
			{
				CPlantColBoundEntry *pEntry = &m_ColEntCache[entry];
				entry = pEntry->m_NextEntry;

				if(	(pEntry->m_nScancode != nCurrentScanCode)	||
					// not valid pointer to parent (entity or bound in static bounds store)?
					(!pEntry->IsParentValid())					)
				{
					pEntry->ReleaseEntry();
				}
				else
				{
				#if PLANTSMGR_MULTI_UPDATE
					// reset processed tris cache:
					pEntry->m_processedTris.Reset();
				#endif
				}
			}
		}

	} // Full update...


	return(TRUE);

}// end of _ColBoundCache_Update()...


//
//
//
//
bool CPlantMgr::_ColBoundCache_ProcessBound(phBound *pBound, CEntity *pEntity, s32 parentIdx, s32 childIdx, const u16 nCurrentScanCode)
{
	if(pBound->GetType() == phBound::BVH || pBound->GetType() == phBound::GEOMETRY)
	{
		phBoundGeometry *pBoundGeom = (phBoundGeometry*)pBound;

	#if PLANTSMGR_DATA_EDITOR
		if(CPlantMgr::IsBoundGeomPlantFriendly(pBoundGeom) || GetAllCollisionSelectableUT())	// allow all bounds regardless of their materials in "select all" mode
	#else
		if(CPlantMgr::IsBoundGeomPlantFriendly(pBoundGeom))
	#endif
		{
			CPlantColBoundEntry *pEntry = _ColBoundCache_FindInCache(pEntity, parentIdx, childIdx);
			if(!pEntry)
			{
				pEntry = _ColBoundCache_Add(pEntity, parentIdx, childIdx);
			}

			if(pEntry)
			{
				pEntry->m_nScancode = nCurrentScanCode;		// update scanecode to indicate this bound is colliding with iterator
			}
		}
	}
	
	return(TRUE);
}

//
//
// checks if given Bound is already in BoundCache:
//
CPlantColBoundEntry*	CPlantMgr::_ColBoundCache_FindInCache(CEntity *pEntity, s32 parentIdx, s32 childIdx)
{
	(void)parentIdx;	// shut up the compiler about unused params
	(void)childIdx;

	if(m_ColEntCache.m_CloseListHead)
	{
		u16 entry = m_ColEntCache.m_CloseListHead;
		while(entry)
		{
			CPlantColBoundEntry *pEntry = &m_ColEntCache[entry];

			if(pEntry->IsMatBound())
			{
				if((pEntry->m_nBParentIndex == parentIdx) && (pEntry->m_nBChildIndex == childIdx))
				{
					return(pEntry);
				}
			}
			else
			{
				if(pEntry->m__pEntity == pEntity)
				{
					return(pEntry);
				}
			}

			entry = pEntry->m_NextEntry;
		}
	}
		
	return(NULL);
}

//
//
// adds Bound to BoundCache:
//
CPlantColBoundEntry* CPlantMgr::_ColBoundCache_Add(CEntity* pEntity, s32 parentIdx, s32 childIdx, bool bCheckCacheFirst)
{
CPlantColBoundEntry *pEntry=NULL;
	
	if(bCheckCacheFirst)
	{
		pEntry = _ColBoundCache_FindInCache(pEntity, parentIdx, childIdx);
		if(pEntry)
		{
			// seems that this CEntity if already in the cache:
			return(pEntry);
		}
	}
	

	if(m_ColEntCache.m_UnusedListHead)
	{
		pEntry = &m_ColEntCache[m_ColEntCache.m_UnusedListHead];
		if(pEntry->AddEntry(pEntity, parentIdx, childIdx))
		{
			return(pEntry);
		}
	}

	return(NULL);
}


//
//
// removes Bound from BoundCache:
//
void CPlantMgr::_ColBoundCache_Remove(CEntity *pEntity, s32 parentIdx, s32 childIdx)
{
	CPlantColBoundEntry *pEntry = _ColBoundCache_FindInCache(pEntity, parentIdx, childIdx);
	if(pEntry)
	{
		pEntry->ReleaseEntry();
	}
}


//
//
//
//
bool CPlantMgr::IsBoundGeomPlantFriendly(phBoundGeometry *pBound)
{
	Assert(pBound);
	
	const s32 numMaterials = pBound->GetNumMaterials();
	if(numMaterials <= 0)
	{
		return(FALSE);
	}

	// check all materials in given bound if they're "plant friendly":
	for(s32 i=0; i<numMaterials; i++)
	{
		s32 procTagId = PGTAMATERIALMGR->UnpackProcId(pBound->GetMaterialId(i));
		if (procTagId>0)
		{
			if (g_procInfo.CreatesPlants(procTagId) || g_procInfo.CreatesProcObjects(procTagId))
			{
				return(TRUE);
			}
		}
	}

	return(FALSE);
}// end of IsBoundGeomPlantFriendly()...

//
//
//
//
void CPlantMgr::SetForceHDGrass(bool enable)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	ms_bForceHDGrass = enable;
}

//
//
// top amount of "darkness" for given hours:
//
static float tabFakeGrassRotAmountPerHour[24] =
{
	0.0f,	// 0hrs
	0.0f,	// 1hrs
	0.0f,	// 2hrs
	0.0f,	// 3hrs
	0.0f,	// 4hrs
	0.0f,	// 5hrs
	0.5f,	// 6hrs
	0.4f,	// 7hrs
	0.4f,	// 8hrs
	0.4f,	// 9hrs
	0.2f,	//10hrs
	0.2f,	//11hrs
	0.2f,	//12hrs
	0.2f,	//13hrs
	0.2f,	//14hrs
	0.2f,	//15hrs
	0.2f,	//16hrs
	0.2f,	//17hrs
	0.4f,	//18hrs
	0.4f,	//19hrs
	0.4f,	//20hrs
	0.4f,	//21hrs
	0.0f,	//22hrs
	0.0f	//23hrs
};

//
//
// calculates faked grass normal (taken from main SunDirection);
// rotates it accordingly to make overall grass darker;
// 
// rot=0: no rotation -> full lighting amount
// rot=1: full rotation -> no rotation amount
//
void CPlantMgr::CalculateFakeGrassNormal(float rotationAmountAddDbg)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));


float rotationAmount = rotationAmountAddDbg;

#if __BANK
	// use simplified timecycle below?
	if(gbPlantsDarknessTimecycle)
#else
	if(1)
#endif
	{
		const s32 hour	= CClock::GetHour();
		const s32 minutes = CClock::GetMinute();
		s32 hourNext		= hour+1;
		if(hourNext > 23)
			hourNext = 0;

		const float rotAmount0		= tabFakeGrassRotAmountPerHour[hour];
		const float rotAmount1		= tabFakeGrassRotAmountPerHour[hourNext];
		const float rotAmountAdd	= rotAmount0 + (rotAmount1-rotAmount0) * (minutes/60.0f);

		rotationAmount += rotAmountAdd;
	}// use darkness timecycle...


	rotationAmount = rage::Max(rotationAmount, 0.0f);
	rotationAmount = rage::Min(rotationAmount, 1.0f);

#if __BANK
	gbPlantsDarknessAmountPrint = rotationAmount;
#endif



Vector3 fakedNormal;

	Vector3 lightSunDir = Lights::GetUpdateDirectionalLight().GetDirection();
	if(lightSunDir == Vector3(0,0,-1))
	{	// detect wrong vector direction case:
		lightSunDir = Vector3(1,1,1);
	}

static dev_bool bFakedNormalEnabled=FALSE;
	if(bFakedNormalEnabled)
	{
		Vector3 rotAxis = CrossProduct(lightSunDir, Vector3(0,0,1));
		rotAxis.Normalize();

		float rotation = 90.0f * rotationAmount;

		Matrix34 rotMtx;
		rotMtx.Identity();
		rotMtx.MakeRotateUnitAxis(rotAxis,( DtoR * rotation));

		Vector3 newLightSunDir;
		rotMtx.Transform3x3(lightSunDir, newLightSunDir);
		fakedNormal = -newLightSunDir;
	}
	else
	{
		fakedNormal = -lightSunDir;
	}

static dev_bool	bUseForcedFakeNormal= TRUE;
	if(bUseForcedFakeNormal)
	{
		Vector3 fakeNormal3;
		static dev_float	fakeNormalX		= 0.0f;
		static dev_float	fakeNormalY		= 0.0f;
		static dev_float	fakeNormalZ		= 1.0f;
		fakeNormal3.SetX(fakeNormalX);
		fakeNormal3.SetY(fakeNormalY);
		fakeNormal3.SetZ(fakeNormalZ);
		fakeNormal3.Normalize();
		fakedNormal = fakeNormal3;
	}

	CGrassRenderer::SetFakeGrassNormal(fakedNormal);
}// end of CalculateFakeGrassNormal()...



//
//
//
//
void CPlantMgr::CalculateWindBending(Vector2 &outBending)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	outBending.Set(0.0f, 0.0f);

	//Here we use the wind velocity in XY Plane to determine how much bend we want (includes direction
#if __BANK
	if (ms_bkbWindResponseEnabled && (ms_bkfWindResponseAmount > 0.0f))
#endif
	{
		Vector3 wind(0.0f, 0.0f, 0.0f);
		Vector2 wind2d(0.0f, 0.0f);

		//Using global velocity as we dont have access to each patches position here. 
		// Plus it would be more expensive to calculate bending for each group.
		WIND.GetGlobalVelocity(WIND_TYPE_AIR, RC_VEC3V(wind));		
		wind.GetVector2XY(wind2d);
		const float windVel = wind2d.Mag();

		const float windVelMin = BANK_SWITCH(ms_bkfWindResponseVelMin, PLANT_DEFAULT_WIND_RESPONSE_VEL_MIN);
		const float windVelMax = BANK_SWITCH(ms_bkfWindResponseVelMax, PLANT_DEFAULT_WIND_RESPONSE_VEL_MAX);
		const float windVelExp = BANK_SWITCH(ms_bkfWindResponseVelExp, PLANT_DEFAULT_WIND_RESPONSE_VEL_EXP);

		//Calculate wind response in [0.0, 1.0f]
		float windResponse = Clamp<float>((windVel - windVelMin)/(windVelMax - windVelMin), 0.0f, 1.0f);

		if (windVelExp != 0.0f)
		{
			windResponse = powf(windResponse, powf(2.0f, windVelExp));
		}

		//Scale wind response
		const float currentStrength = windResponse*BANK_SWITCH(ms_bkfWindResponseAmount, PLANT_DEFAULT_WIND_RESPONSE_AMOUNT);

		//re-scale bending vector based on calculated strength
		wind2d.NormalizeSafe(Vector2(1.0f, 0.0));
		wind2d *= currentStrength;		

		// smoothening bend movement to avoid jerks
		ms_AvgWindVector = ms_AvgWindVector*g_AvgPlantWindInfluence  + wind2d*(1.0f - g_AvgPlantWindInfluence);
		
		outBending = ms_AvgWindVector;
	}
}


//
//
//
// IsInsideCurrentViewFrustum?
//
static
__forceinline bool IsLocTriVisibleByCamera(const spdTransposedPlaneSet8& cullFrustum, CPlantLocTri *pLocTri)
{
	if(pLocTri->m_bCameraDontCull)
	{
		return(true);	// do not cull when requested
	}

	// is sphere containing triangle visible?
	Vector4 sphereV4( pLocTri->GetCenter() );
	sphereV4.w = pLocTri->GetSphereRadius();
	spdSphere sphere(VECTOR4_TO_VEC4V(sphereV4));

	return cullFrustum.IntersectsOrContainsSphere(sphere);
}

static
inline bool IsLocTriVisibleUnderwater(CPlantLocTri *pLocTri, bool bIsCameraUnderwater)
{
	if(pLocTri->m_bUnderwater)
	{
		return bIsCameraUnderwater? true : false;	// proc types flagged as underwater are visible only underwater
	}
	else
	{
		return(true);
	}
}

#if PLANTS_CAST_SHADOWS
static bool IsLocTriAShadowCandidate(CPlantLocTri *pLocTri)
{
	const float R1PlusR2 = pLocTri->GetSphereRadius() + CGrassRenderer::GetShadowFadeFarRadius();
	const Vector3 A = pLocTri->GetCenter() - CGrassRenderer::GetGlobalCameraPos();

	if(Dot(A, A) > R1PlusR2*R1PlusR2)
	{
		return(false);
	}
	
	return(true);
}
#endif //PLANTS_CAST_SHADOWS

#if PSN_ALPHATOMASK_PASS || EXTRA_1_5_PASS
	const u32 DEFERRED_MATERIAL_GRASS = DEFERRED_MATERIAL_TREE+DEFERRED_MATERIAL_SPECIALBIT; //0x03+0x10
	CompileTimeAssert(DEFERRED_MATERIAL_GRASS==0x13);
#elif XENON_FILL_PASS
	const u32 DEFERRED_MATERIAL_GRASS = DEFERRED_MATERIAL_TREE+DEFERRED_MATERIAL_SPECIALBIT; //0x03+0x10
	CompileTimeAssert(DEFERRED_MATERIAL_GRASS==0x13);
#else
	const u32 DEFERRED_MATERIAL_GRASS = DEFERRED_MATERIAL_TREE;
#endif
//
//
// PSN render pass#1:
// draw only to color0 , depth and stencil mask
//
static void BeginGrassAlphaToMaskPass1()
{
	grcStateBlock::SetRasterizerState(hGrassAlphaToMaskRS_pass1);
	grcStateBlock::SetDepthStencilState(hGrassAlphaToMaskDSS_pass1, DEFERRED_MATERIAL_GRASS);
	grcStateBlock::SetBlendState(hGrassAlphaToMaskBS_pass1);

#if __XENON
	static dev_bool debugHiZEnable = TRUE;
	GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HIZENABLE,		debugHiZEnable?D3DHIZ_AUTOMATIC:D3DHIZ_DISABLE);
	GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HIZWRITEENABLE,D3DHIZ_AUTOMATIC);
#endif


#if PSN_ALPHATOMASK_PASS
	// render only to color0+depth/stencil:
	GBuffer::UnlockTargets();
	GBuffer::LockSingleTarget(GBUFFER_RT_0, TRUE);	// ps3: calls BlockAlphaToMask() under the hood, so must be called before stateblocks
#endif
}

static void EndGrassAlphaToMaskPass1()
{
#if PSN_ALPHATOMASK_PASS
	GBuffer::UnlockSingleTarget(GBUFFER_RT_0);
	GBuffer::LockTargets();
#endif

#if __XENON
	GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HIZENABLE,		D3DHIZ_AUTOMATIC );
	GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HIZWRITEENABLE,D3DHIZ_AUTOMATIC );
#endif // __XENON

	grcStateBlock::SetBlendState(hGrassAlphaToMaskBS_pass1_end);
	grcStateBlock::SetDepthStencilState(hGrassAlphaToMaskDSS_pass1_end, DEFERRED_MATERIAL_DEFAULT);
}


#if EXTRA_1_5_PASS
// clears MRT2.a (selfshadow term) for anything but default material:
static void RenderGrassAlphaToMaskPass1_5()
{
#if __PS3 || (RSG_PC || RSG_DURANGO || RSG_ORBIS)
	GBuffer::UnlockTargets();
	GBuffer::LockSingleTarget(GBUFFER_RT_2, TRUE);
#endif

	if(1)	// clear selfshadowing in MRT2.a
	{
		grcStateBlock::SetBlendState(hGrassAlphaToMaskBS_pass1_5a);

		const u32 stencilRef1 = DEFERRED_MATERIAL_SPECIALBIT+DEFERRED_MATERIAL_TERRAIN;
		CompileTimeAssert(stencilRef1==0x14);
		grcStateBlock::SetDepthStencilState(hGrassAlphaToMaskDSS_pass1_5a, stencilRef1);	// anything greater than default material+grass

	#if __XENON
		// MRT write technique:
		grmShader *shader = CGrassRenderer::GetGrassShader();
		Assert(shader);
		grcEffectTechnique tech = CGrassRenderer::GetFakedGBufTechniqueID();
		Assert(tech);
		shader->TWODBlit(	-1.0f,1.0f, 1.0f,-1.0f, 0.0f,
							0.0f,0.0f, 1.0f,1.0f, Color32(0,0,0,0), tech);
	#else
		// single RT technique:
		CSprite2d::DrawRect(0, 0, 1, 1, 0, Color32(0,0,0,0));
	#endif
	}

	if(1)	// write full grass matID=0x03
	{
		grcStateBlock::SetBlendState(hGrassAlphaToMaskBS_pass1_5b);

		const u32 stencilRef1 = DEFERRED_MATERIAL_GRASS;
		grcStateBlock::SetDepthStencilState(hGrassAlphaToMaskDSS_pass1_5b, stencilRef1);	// anything greater than default material+grass

		CSprite2d::DrawRect(0, 0, 1, 1, 0, Color32(0,0,0,0));
	}

#if __XENON	
	if(1)	// clear special bit - restore proper matID (PS3 does this in fakeGBuf pass):
	{
		const u32 stencilRef1 = DEFERRED_MATERIAL_GRASS;
		grcStateBlock::SetDepthStencilState(hGrassAlphaToMaskDSS_pass1_5c, stencilRef1);	// anything greater than default material+grass

		CSprite2d::DrawRect(0, 0, 1, 1, 0, Color32(0,0,0,0));
	}
#endif

	// cleanup:
	const u32 stencilRef2 = DEFERRED_MATERIAL_DEFAULT;

	grcStateBlock::SetDepthStencilState(hGrassAlphaToMaskDSS_pass1_5_end, stencilRef2);
	grcStateBlock::SetBlendState(hGrassAlphaToMaskBS_pass1_5_end);

#if __PS3 || (RSG_PC || RSG_DURANGO || RSG_ORBIS)
	GBuffer::UnlockSingleTarget(GBUFFER_RT_2);
	GBuffer::LockTargets();
#endif
}
#endif //EXTRA_1_5_PASS...

#if XENON_FILL_PASS

static void RestoreHiStencilForFoliagePass(const u32 stencilRef, bool useEqual = true);
//
//
// fillup pass for Xenon:
//
static void XenonFillPass()
{
	RestoreHiStencilForFoliagePass(DEFERRED_MATERIAL_GRASS);

	if(1)	// clear selfshadowing in MRT2.a
	{
		grcStateBlock::SetBlendState(hGbuffFillPassBS);

		const u32 stencilRef1 = DEFERRED_MATERIAL_GRASS;
		grcStateBlock::SetDepthStencilState(hGbuffFillPassDSS, stencilRef1);	// anything greater than default material+grass

		// MRT write technique:
		grmShader *shader = CGrassRenderer::GetGrassShader();
		Assert(shader);
		grcEffectTechnique tech = CGrassRenderer::GetFakedGBufTechniqueID();
		Assert(tech);
		shader->TWODBlit(	-1.0f,1.0f, 1.0f,-1.0f, 0.0f,
							0.0f,0.0f, 1.0f,1.0f, Color32(0,0,0,0), tech);
	}

	// cleanup:
	const u32 stencilRef2 = DEFERRED_MATERIAL_DEFAULT;
	GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILENABLE, FALSE );
	GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILFUNC, D3DHSCMP_EQUAL );
	GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILWRITEENABLE, FALSE );
	grcStateBlock::SetDepthStencilState(hGbuffFillPassDSS_end, stencilRef2);

	grcStateBlock::SetBlendState(hGbuffFillPassBS_end);
}

//
//
//
//
static void RestoreHiStencilForFoliagePass(const u32 stencilRef, bool useEqual)
{
	// Can't change render targets b/c of predicated tiling. So this custom version doesn't try to change render targets before refresh.
	// That makes it slightly slower, but makes the fullscreen foliage pass faster overall.
	PF_PUSH_TIMEBAR_DETAIL("Mark HiStencil");

	grcStateBlock::SetDepthStencilState(hGbuffFillPassRestoreHiStDSS, stencilRef);

	GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILENABLE, FALSE);
	GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILWRITEENABLE, TRUE);
	GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILFUNC, useEqual ? D3DHSCMP_NOTEQUAL : D3DHSCMP_EQUAL); //Reversing useEqual, just like in GBuffer.cpp. This matches PS3 functionality.
	GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILREF, stencilRef);

	CSprite2d restoreHiZ;
	restoreHiZ.BeginCustomList(CSprite2d::CS_RESTORE_HIZ, NULL);
	grcBeginQuads(1);
		grcDrawQuadf(0,0,1,1,0,0,0,1,1,Color32(255,255,255,255));
	grcEndQuads();
	restoreHiZ.EndCustomList();

	GRCDEVICE.GetCurrent()->FlushHiZStencil(D3DFHZS_SYNCHRONOUS);

	// Should be ready to use now.
	GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILENABLE, TRUE);
	GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILWRITEENABLE, FALSE);

	PF_POP_TIMEBAR_DETAIL();
}
#endif //XENON_FILL_PASS...

#if PSN_ALPHATOMASK_PASS
//
// PSN render pass#2:
// fullscreen blit to color1 (faked normals) and color2 (spec params)
//
static void RenderGrassAlphaToMaskPass2(float fNearZ, float fFarZ)
{
#if CPLANT_BLIT_MIN_GBUFFER
	GBuffer::UnlockTargets();
	const grcRenderTarget* pRendertargets[grcmrtColorCount] = { GBuffer::GetTarget(GBUFFER_RT_1), GBuffer::GetTarget(GBUFFER_RT_2), GBuffer::GetTarget(GBUFFER_RT_3), NULL };
	grcTextureFactory::GetInstance().LockMRT(pRendertargets, GBuffer::GetDepthTarget());
#endif

	grcStateBlock::SetBlendState(hGrassAlphaToMaskBS_pass2);
	const u32 stencilRef1 = DEFERRED_MATERIAL_GRASS;
	grcStateBlock::SetDepthStencilState(hGrassAlphaToMaskDSS_pass2, stencilRef1);
//	grcStateBlock::SetRasterizerState(hGrassAlphaToMaskRS_pass2);
	
	grmShader *shader = CGrassRenderer::GetGrassShader();
	Assert(shader);
	grcEffectTechnique tech = CGrassRenderer::GetFakedGBufTechniqueID();
	Assert(tech);
#if CPLANT_STORE_LOCTRI_NORMAL
	shader->SetVar(CGrassRenderer::GetGBuf0TextureID(), GBuffer::GetTarget(GBUFFER_RT_0));
#endif

	grcDevice::SetDepthBoundsTestEnable(true);
	CRenderer::SetDepthBoundsFromRange(grcViewport::GetCurrent()->GetNearClip(), grcViewport::GetCurrent()->GetFarClip(), fNearZ, fFarZ);

	shader->TWODBlit(	-1.0f,1.0f, 1.0f,-1.0f, 0.0f,
						0.0f,0.0f, 1.0f,1.0f, Color32(255,255,255,255), tech);

	grcDevice::SetDepthBoundsTestEnable(false);

#if CPLANT_BLIT_MIN_GBUFFER
	grcTextureFactory::GetInstance().UnlockMRT();
	GBuffer::LockTargets();
#endif

	// cleanup:
	const u32 stencilRef2 = DEFERRED_MATERIAL_DEFAULT;
	grcStateBlock::SetDepthStencilState(hGrassAlphaToMaskDSS_pass2_end, stencilRef2);
//	grcStateBlock::SetRasterizerState(hGrassAlphaToMaskRS_pass2_end);
	grcStateBlock::SetBlendState(hGrassAlphaToMaskBS_pass2_end);
}
#elif PSN_CLEANUP_PASS
static void RenderGrassAlphaToMaskPass2()
{
	grmShader *shader = CGrassRenderer::GetGrassShader();
	Assert(shader);
	grcEffectTechnique tech = CGrassRenderer::GetFakedGBufTechniqueID();
	Assert(tech);
#if CPLANT_STORE_LOCTRI_NORMAL
	shader->SetVar(CGrassRenderer::GetGBuf0TextureID(), GBuffer::GetTarget(GBUFFER_RT_0));
#endif

	const Vector2 topLeft (-2.0f, -1.01f);
	const Vector2 botRight(-1.01f, -2.0f);

	shader->TWODBlit(	topLeft.x, topLeft.y, botRight.x, botRight.y, 0.0f,
						0.0f,0.0f, 1.0f,1.0f, Color32(255,255,255,255), tech);
}
#endif //PSN_ALPHATOMASK_PASS...

//
//
//
//
bool CPlantMgr::Render()
{
	return gPlantMgr.RenderInternal();
}

bool CPlantMgr::RenderInternal()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	if(!gbPlantMgrActive)	// skip updating, if varconsole switch is off
		return(FALSE);

#if __BANK
	if (TiledScreenCapture::IsEnabled())
		return(FALSE);
	bRenderInternalActive = TRUE;	// RenderInternal() active
#endif

#if __BANK
	CPlantMgr::RenderDebugStuff();
#endif

#if LOCTRISTAB_STR
	if(m_LocTrisTab[0] == NULL)
	{	// loc tris tabs not allocated, so skip:
	#if __BANK
		bRenderInternalActive = FALSE;	// RenderInternal() inactive
	#endif
		return(TRUE);
	}
#endif

	if(CGrassRenderer::GetGlobalInteriorCullAll())
	{
	#if __BANK
		bRenderInternalActive = FALSE;	// RenderInternal() inactive
	#endif
		return(TRUE);	// camera is in interior and exterior is not visible through a portal
	}

	PIXBegin(0, "PlantsMgrRender");
	PF_PUSH_TIMEBAR_DETAIL("PlantsMgrRender");
	GRCDBG_PUSH("RenderBegin");

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);

	// setup render state:
	grcBindTexture(NULL);
	grcWorldIdentity();

	// set the hdr value
	const float hdrVal = Lights::GetRenderDirectionalLight().GetIntensity();
	CShaderLib::SetGlobalEmissiveScale(hdrVal);

	DeferredLighting::PrepareCutoutPass(false,true,false);

	GRCDBG_POP();
	GRCDBG_PUSH("BeginGrassAlphaToMaskPass1");
	BeginGrassAlphaToMaskPass1();

	GRCDBG_POP();
	GRCDBG_PUSH("SpuInitialisePerRenderFrame");
	CGrassRenderer::SpuInitialisePerRenderFrame();

	GRCDBG_POP();
	GRCDBG_PUSH("RenderActive");

	/////////////////////////////////////////////////////////////////////////////////////////////////

	const s32 oldStoredSeed	= g_PlantsRendRand[g_RenderThreadIndex].GetSeed();
	const u32 bufferID		= GetRenderRTBufferID();

	spdTransposedPlaneSet8 cullFrustum;
	CGrassRenderer::GetGlobalCullFrustum(&cullFrustum);

	bool bPlantsRendered = RenderClosedLists(bufferID, cullFrustum);

#if !PSN_ALPHATOMASK_PASS && !PSN_CLEANUP_PASS
	(void)bPlantsRendered;
#endif

	/////////////////////////////////////////////////////////////////////////////////////////////////

	GRCDBG_POP();
	GRCDBG_PUSH("SpuFinalPerRndrFrme");
	CGrassRenderer::SpuFinalisePerRenderFrame();

	// Restore original state.
	GRCDBG_POP();
	GRCDBG_PUSH("EndGrassAlphaToMaskPass1");
	EndGrassAlphaToMaskPass1();

	DeferredLighting::FinishCutoutPass();

#if EXTRA_1_5_PASS
	GRCDBG_POP();
	GRCDBG_PUSH("RenderGrassAlphaToMaskPass1_5");
	if(bPlantsRendered)
	{
		RenderGrassAlphaToMaskPass1_5();
	}
#endif

#if XENON_FILL_PASS
	GRCDBG_POP();
	GRCDBG_PUSH("360 Fill Pass");
	if(bPlantsRendered)
	{
		XenonFillPass();
	}
#endif

#if PSN_ALPHATOMASK_PASS || PSN_CLEANUP_PASS
	if(bPlantsRendered)
	{
		GRCDBG_POP();
		GRCDBG_PUSH("RenderGrassAlphaToMaskPass2");
		RenderGrassAlphaToMaskPass2(0.0f, CPLANT_TRILOC_FAR_DIST); // We could use a tighter near & far bounds if we got that info from the PlantsMgr.
	}
#endif



	GRCDBG_POP();
	GRCDBG_PUSH("RenderEnd");
	CShaderLib::SetGlobalEmissiveScale(1.0f);
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);

	GRCDBG_POP();
	PF_POP_TIMEBAR_DETAIL();
	PIXEnd();
	
	g_PlantsRendRand[g_RenderThreadIndex].Reset(oldStoredSeed);
#if __BANK
	bRenderInternalActive = FALSE;	// RenderInternal() inactive
#endif
	return(TRUE);
}// end of RenderInternal()...


static dev_bool sm_bLodDistanceCheck = true;
//
//
//
//
bool CPlantMgr::RenderClosedLists(const u32 bufferID, spdTransposedPlaneSet8 &cullFrustum)
{
	PF_PUSH_TIMEBAR_DETAIL("RenderClosedLists");

	bool bPlantsRendered = false;

#if PLANTS_CAST_SHADOWS
	ResetShadowCandidates();
#endif

#if !__PS3
	CGrassRenderer::BeginUseNormalTechniques(cullFrustum);
#endif

const bool  bIsCameraUnderwater				= CGrassRenderer::GetGlobalCameraUnderwater();
const float fGlobaPlayerCollisionRadiusSqr	= CGrassRenderer::GetGlobalPlayerCollisionRSqr();

#if __BANK || PLANTS_USE_LOD_SETTINGS
	#if PLANTS_USE_LOD_SETTINGS
		float grassDistanceMultiplier = CGrassRenderer::GetDistanceMultiplier();
	#else
		float grassDistanceMultiplier = 1.0f;
	#endif
	const float	globalLOD0FarDist		= grassDistanceMultiplier*gbPlantsLOD0FarDist;
	const float	globalLOD1CloseDist		= grassDistanceMultiplier*gbPlantsLOD1CloseDist;
	const float	globalLOD1FarDist		= grassDistanceMultiplier*gbPlantsLOD1FarDist;
	const float	globalLOD2CloseDist		= grassDistanceMultiplier*gbPlantsLOD2CloseDist;
	const float	globalLOD2FarDist		= grassDistanceMultiplier*gbPlantsLOD2FarDist;
#else
	const float	globalLOD0FarDist		= CPLANT_LOD0_FAR_DIST;
	const float	globalLOD1CloseDist		= CPLANT_LOD1_CLOSE_DIST;
	const float	globalLOD1FarDist		= CPLANT_LOD1_FAR_DIST;
	const float	globalLOD2CloseDist		= CPLANT_LOD2_CLOSE_DIST;
	const float	globalLOD2FarDist		= CPLANT_LOD2_FAR_DIST;
#endif
	const float globalLOD0FarDist2		= globalLOD0FarDist*globalLOD0FarDist;
	const float globalLOD1CloseDist2	= globalLOD1CloseDist*globalLOD1CloseDist;
	const float globalLOD1FarDist2		= globalLOD1FarDist*globalLOD1FarDist;
	const float globalLOD2CloseDist2	= globalLOD2CloseDist*globalLOD2CloseDist;
	const float globalLOD2FarDist2		= globalLOD2FarDist*globalLOD2FarDist;
		
	//
	// render all close lists:
	//
#if !__FINAL
	if(gbPlantMgrRenderActive)
#endif
		for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)
		{
			CPlantLocTriArray& triRenderTab = *m_LocTrisRenderTab[bufferID][listID];
			u16 loctri = triRenderTab.m_CloseListHead;

#if __BANK
			// rendering stats:
			pLocTriPlantsLOD0DrawnPerSlot	= &nLocTriPlantsLOD0Drawn;
			pLocTriPlantsLOD1DrawnPerSlot	= &nLocTriPlantsLOD1Drawn;
			pLocTriPlantsLOD2DrawnPerSlot	= &nLocTriPlantsLOD2Drawn;
#endif

			while(loctri)
			{
				CPlantLocTri *pLocTri = &triRenderTab[loctri];

				bool bCreatesPlants = pLocTri->m_bCreatesPlants;
			#if PLANTSMGR_DATA_EDITOR
				// special case: edit mode allows all collision to be selected, possibly invalid tris are in the list:
				if(GetAllCollisionSelectable())
				{
					const s32 procTagId = PGTAMATERIALMGR->UnpackProcId(pLocTri->m_nSurfaceType);
					bCreatesPlants &= g_procInfo.CreatesPlants(procTagId);
				}
			#endif

			#if CPLANT_USE_OCCLUSION
				BANK_ONLY( if(gbPlantsUseOcclusion) )
				{
					// skip occluded tris:
					bCreatesPlants &= (!pLocTri->m_bOccluded);
				}
			#endif

				// draw only if it creates plants
				if(bCreatesPlants)	
				{
				#if PLANTS_CAST_SHADOWS
					if(IsLocTriAShadowCandidate(pLocTri))
					{
						AddShadowCandidate(pLocTri);
					}
				#endif //PLANTS_CAST_SHADOWS

					// check if pLocTri inside current View Frustum:
					if(IsLocTriVisibleByCamera(cullFrustum, pLocTri) && IsLocTriVisibleUnderwater(pLocTri, bIsCameraUnderwater))
					{
					#if CPLANT_USE_COLLISION_2D_DIST
						Vector3 vertDistV0a(pLocTri->GetV1() - CGrassRenderer::GetGlobalCameraPos());
						Vector2 vertDistV0(vertDistV0a.x, vertDistV0a.y);
						Vector3 vertDistV1a(pLocTri->GetV2() - CGrassRenderer::GetGlobalCameraPos());
						Vector2 vertDistV1(vertDistV1a.x, vertDistV1a.y);
						Vector3 vertDistV2a(pLocTri->GetV3() - CGrassRenderer::GetGlobalCameraPos());
						Vector2 vertDistV2(vertDistV2a.x, vertDistV2a.y);
					#else
						Vector3 vertDistV0(pLocTri->GetV1() - CGrassRenderer::GetGlobalCameraPos());
						Vector3 vertDistV1(pLocTri->GetV2() - CGrassRenderer::GetGlobalCameraPos());
						Vector3 vertDistV2(pLocTri->GetV3() - CGrassRenderer::GetGlobalCameraPos());
					#endif
						const float fVert0Dist2 = vertDistV0.Mag2();
						const float fVert1Dist2 = vertDistV1.Mag2();
						const float fVert2Dist2 = vertDistV2.Mag2();

#if __DEV || SECTOR_TOOLS_EXPORT
						pLocTri->m_nMaxNumPlants = 0;
#endif
						s32 procTagId = PGTAMATERIALMGR->UnpackProcId(pLocTri->m_nSurfaceType);
						s32 plantInfoIndex = g_procInfo.m_procTagTable[procTagId].plantIndex;

#if __ASSERT || __BANK
						// help assert to catch evil BS#1010542:
						if(plantInfoIndex == -1)
						{
							Assertf(0, "Helper for BS#1010542: Please call Andrzej when this triggers: pLocTri=0x%p, listID=%d, procTagId=%d, surfaceType=0x%" I64FMT "x.", pLocTri, listID, procTagId, pLocTri->m_nSurfaceType);  
							Printf("Helper for BS#1010542: Please call Andrzej when this triggers: pLocTri=0x%p, listID=%d, procTagId=%d, surfaceType=0x%" I64FMT "x.", pLocTri, listID, procTagId, pLocTri->m_nSurfaceType);  
							TrapEQ(0,0);
						}
#endif

						//PF_PUSH_TIMEBAR_DETAIL("m_plantInfos");
						atHashValue currentProcTag = g_procInfo.m_plantInfos[plantInfoIndex].m_Tag;
						for( ; plantInfoIndex < g_procInfo.m_plantInfos.GetCount() && g_procInfo.m_plantInfos[plantInfoIndex].m_Tag==currentProcTag ; plantInfoIndex++)
						{
							CPlantInfo *pPlantInfo = &g_procInfo.m_plantInfos[plantInfoIndex];

							PPTriPlant triPlant;
							triPlant.V1				=	pLocTri->m_V1;
							triPlant.V2				=	pLocTri->m_V2;
							triPlant.V3				=	pLocTri->m_V3;
							Assert(pPlantInfo->m_ModelId < 255);	// must fit into u8
							triPlant.model_id		=	(u8)pPlantInfo->m_ModelId;

						#if CPLANT_CLIP_EDGE_VERT
							triPlant.m_ClipEdge_01		=	pLocTri->m_ClipEdge_01	&& gbPlantMgrClipEnable	&& gbPlantMgrEdgeClipEnable;
							triPlant.m_ClipEdge_12		=	pLocTri->m_ClipEdge_12	&& gbPlantMgrClipEnable	&& gbPlantMgrEdgeClipEnable;
							triPlant.m_ClipEdge_20		=	pLocTri->m_ClipEdge_20	&& gbPlantMgrClipEnable	&& gbPlantMgrEdgeClipEnable;
							triPlant.m_ClipVert_0		=	pLocTri->m_ClipVert_0	&& gbPlantMgrClipEnable && gbPlantMgrVertClipEnable;
							triPlant.m_ClipVert_1		=	pLocTri->m_ClipVert_1	&& gbPlantMgrClipEnable && gbPlantMgrVertClipEnable;
							triPlant.m_ClipVert_2		=	pLocTri->m_ClipVert_2	&& gbPlantMgrClipEnable && gbPlantMgrVertClipEnable;
						#endif

							g_PlantsRendRand[g_RenderThreadIndex].Reset(pLocTri->GetSeed());

#if __BANK
							// forced ground color:
							if(gbForceGroundColor)
							{
								const u8 gbGroundDensityM = u8(float(gbGroundDensity_V1+gbGroundDensity_V2+gbGroundDensity_V3)/3.0f + 0.5f);

								triPlant.groundColorV1 = gbGroundColorV1;
								triPlant.groundColorV1.SetAlpha(CPlantLocTri::pv8PackDensityScaleZScaleXYZ(gbGroundDensityM, (u8)gbGroundScaleZ_V1, (u8)gbGroundScaleXYZ_V1));

								triPlant.groundColorV2 = gbGroundColorV2;
								triPlant.groundColorV2.SetAlpha(CPlantLocTri::pv8PackDensityScaleZScaleXYZ(gbGroundDensityM, (u8)gbGroundScaleZ_V2, (u8)gbGroundScaleXYZ_V2));

								triPlant.groundColorV3 = gbGroundColorV3;
								triPlant.groundColorV3.SetAlpha(CPlantLocTri::pv8PackDensityScaleZScaleXYZ(gbGroundDensityM, (u8)gbGroundScaleZ_V3, (u8)gbGroundScaleXYZ_V3));
							}
							else
#endif //__BANK
							{
								triPlant.groundColorV1 = pLocTri->m_GroundColorV1;
								triPlant.groundColorV2 = pLocTri->m_GroundColorV2;
								triPlant.groundColorV3 = pLocTri->m_GroundColorV3;
							}

							//
							// max number of plants to generate for this TriLoc
							// this is connected with triangle area + surface type:
							//

							// grab medium pv density weight for tri:
							const float pvDensityWeight = CPlantLocTri::pv8UnpackAndMapDensity(triPlant.groundColorV1.GetAlpha());
							float fDensity = pPlantInfo->m_Density.GetFloat32_FromFloat16() - pvDensityWeight*pPlantInfo->m_DensityRange.GetFloat32_FromFloat16();
							fDensity = fDensity>=0.0f? fDensity : 0.0f;	// density must be positive

							float numPlants = pLocTri->GetTriArea() * fDensity;
							s32 nMaxNumPlants = (s32)numPlants;
							float remainder = numPlants - nMaxNumPlants;
							if (g_PlantsRendRand[g_RenderThreadIndex].GetRanged(0.0f, 1.0f)<remainder)
							{
								nMaxNumPlants++;
							}		

							if (nMaxNumPlants==0)
							{
								continue;
							}

#if __DEV || SECTOR_TOOLS_EXPORT
							pLocTri->m_nMaxNumPlants += nMaxNumPlants;
#endif				
							triPlant.num_plants		=	(u16)nMaxNumPlants;

							// skip the tri - no valid textures to render:
							if( (!m_PlantTextureTab0[pPlantInfo->m_TextureId])							||
								(!m_PlantTextureTab0[pPlantInfo->m_TextureId+CPLANT_SLOT_NUM_TEXTURES])	)
							{
							#if __BANK
								nLocTriTexSkipped++;
							#endif
								continue;
							}
#if PSN_PLANTSMGR_SPU_RENDER
							triPlant.texture_id		=	pPlantInfo->m_TextureId;							// no of texture to use
							triPlant.textureLOD1_id	=	pPlantInfo->m_TextureId+CPLANT_SLOT_NUM_TEXTURES;	// LOD1 texture
#else
							triPlant.texture_ptr	=	m_PlantTextureTab0[pPlantInfo->m_TextureId];							// texture ptr to use
							triPlant.textureLOD1_ptr=	m_PlantTextureTab0[pPlantInfo->m_TextureId+CPLANT_SLOT_NUM_TEXTURES];	// textureLOD1 ptr to use
#endif

							triPlant.color			=	pPlantInfo->m_Color;

							triPlant.intensity		=	pPlantInfo->m_Intensity;
							triPlant.intensity_var	=	pPlantInfo->m_IntensityVar;

							triPlant.seed			=	pLocTri->GetSeed() + (plantInfoIndex*0x33);	// g_PlantsRendRand.GetRanged(0.0f, 1.0f)

							triPlant.packedScale.x	= pPlantInfo->m_ScaleXY;			// scale in XY
							triPlant.packedScale.y	= pPlantInfo->m_ScaleZ;			// scale in Z
							triPlant.packedScale.z	= pPlantInfo->m_ScaleVariationXY;		// scale xyz variation
							triPlant.packedScale.w	= pPlantInfo->m_ScaleVariationZ;

							triPlant.scaleRangeXYZ_Z.x = pPlantInfo->m_ScaleRangeXYZ;
							triPlant.scaleRangeXYZ_Z.y = pPlantInfo->m_ScaleRangeZ;

							triPlant.um_param.x		= pPlantInfo->m_MicroMovementsScaleH;		// micro-movements: global horizontal scale
							triPlant.um_param.y		= pPlantInfo->m_MicroMovementsScaleV;		// micro-movements: global vertical scale
							triPlant.um_param.z		= pPlantInfo->m_MicroMovementsFreqH;		// micro-movements: global horizontal frequency
							triPlant.um_param.w		= pPlantInfo->m_MicroMovementsFreqV;		// micro-movements: global vertical frequency

							triPlant.V1.w = pPlantInfo->m_WindBendScale.GetFloat32_FromFloat16();		// wind bending scale
							triPlant.V2.w = pPlantInfo->m_WindBendVariation.GetFloat32_FromFloat16();	// wind bending variation

							static dev_float fPlayerDefaultCollRadiusSqr = 1.0f*1.0f;	//0.75f;	// radius of influence
							const float collisionRadiusSqr = pPlantInfo->GetCollisionRadiusSqr() * fGlobaPlayerCollisionRadiusSqr * fPlayerDefaultCollRadiusSqr;
							// x = collision radius sqr:
							triPlant.coll_params.x.SetFloat16_FromFloat32(collisionRadiusSqr);
							// y = inv collision radius sqr:
							triPlant.coll_params.y.SetFloat16_FromFloat32(1.0f/collisionRadiusSqr);

							triPlant.flags		= pPlantInfo->m_Flags;
							if(pLocTri->m_bDrawFarTri)
							{
								triPlant.flags.Set(PROCPLANT_LOD2FARFADE);	// set "far far fade" flag
							}

							if(triPlant.flags.IsClear(PROCPLANT_LOD0) && triPlant.flags.IsClear(PROCPLANT_LOD1) && triPlant.flags.IsClear(PROCPLANT_LOD2))
							{
								continue;	// if no visible LODs for procedural grass then skip this tri
							}

							if(sm_bLodDistanceCheck)
							{
								// distance check vs available LODs:
								if(pPlantInfo->m_Flags.IsClear(PROCPLANT_LOD0))
								{
									if(	(fVert0Dist2 < globalLOD0FarDist2) &&
										(fVert1Dist2 < globalLOD0FarDist2) &&
										(fVert2Dist2 < globalLOD0FarDist2) )
									{
										bool bSkipLOD0 = true;
										// does tri overlap with LOD1?
										if(pPlantInfo->m_Flags.IsSet(PROCPLANT_LOD1))
										{
											if(	((fVert0Dist2 >= globalLOD1CloseDist2) && (fVert0Dist2 <= globalLOD1FarDist2)) ||
												((fVert1Dist2 >= globalLOD1CloseDist2) && (fVert1Dist2 <= globalLOD1FarDist2)) ||
												((fVert2Dist2 >= globalLOD1CloseDist2) && (fVert2Dist2 <= globalLOD1FarDist2)) )
											{
												bSkipLOD0 = false;
											}
										}

										if(bSkipLOD0)
										{
											continue;	// tri falls into LOD0, but flag is cleared
										}
									}
								}// LOD0...

								if(pPlantInfo->m_Flags.IsClear(PROCPLANT_LOD1))
								{	
									if(	((fVert0Dist2 > globalLOD1CloseDist2) && (fVert0Dist2 < globalLOD1FarDist2)) &&
										((fVert1Dist2 > globalLOD1CloseDist2) && (fVert1Dist2 < globalLOD1FarDist2)) &&
										((fVert2Dist2 > globalLOD1CloseDist2) && (fVert2Dist2 < globalLOD1FarDist2)) )
									{
										bool bSkipLOD1 = true;

										// does tri overlap with LOD0?
										if(pPlantInfo->m_Flags.IsSet(PROCPLANT_LOD0))
										{
											if(	(fVert0Dist2 <= globalLOD0FarDist2) ||
												(fVert1Dist2 <= globalLOD0FarDist2) ||
												(fVert2Dist2 <= globalLOD0FarDist2) )
											{
												bSkipLOD1 = false;
											}
										}

										// does tri overlap with LOD2?
										if(pPlantInfo->m_Flags.IsSet(PROCPLANT_LOD2))
										{
											if(	((fVert0Dist2 >= globalLOD2CloseDist2) && (fVert0Dist2 <= globalLOD2FarDist2)) ||
												((fVert1Dist2 >= globalLOD2CloseDist2) && (fVert1Dist2 <= globalLOD2FarDist2)) ||
												((fVert2Dist2 >= globalLOD2CloseDist2) && (fVert2Dist2 <= globalLOD2FarDist2)) )
											{
												bSkipLOD1 = false;
											}
										}

										if(bSkipLOD1)
										{
											continue;	// tri falls into LOD1, but flag is cleared
										}
									}
								}// LOD1...

								if(pPlantInfo->m_Flags.IsClear(PROCPLANT_LOD2))
								{	
									if(	((fVert0Dist2 > globalLOD2CloseDist2) && (fVert0Dist2 < globalLOD2FarDist2)) &&
										((fVert1Dist2 > globalLOD2CloseDist2) && (fVert1Dist2 < globalLOD2FarDist2)) &&
										((fVert2Dist2 > globalLOD2CloseDist2) && (fVert2Dist2 < globalLOD2FarDist2)) )
									{
										bool bSkipLOD2 = true;

										// does tri overlap with LOD1?
										if(pPlantInfo->m_Flags.IsSet(PROCPLANT_LOD1))
										{
											if(	((fVert0Dist2 >= globalLOD1CloseDist2) && (fVert0Dist2 <= globalLOD1FarDist2)) ||
												((fVert1Dist2 >= globalLOD1CloseDist2) && (fVert1Dist2 <= globalLOD1FarDist2)) ||
												((fVert2Dist2 >= globalLOD1CloseDist2) && (fVert2Dist2 <= globalLOD1FarDist2)) )
											{
												bSkipLOD2 = false;
											}
										}

										if(bSkipLOD2)
										{
											continue;	// tri falls into LOD2, but flag is clered
										}
									}
								}// LOD2...
							}//LodDistanceCheck...

							// skewing info:
							triPlant.skewAxisAngle	= pLocTri->m_skewAxisAngle;

							// lame: m_normal should be u32:
							triPlant.loctri_normal[0]	= pLocTri->m_normal[0];
							triPlant.loctri_normal[1]	= pLocTri->m_normal[1];
							triPlant.loctri_normal[2]	= pLocTri->m_normal[2];

							triPlant.ambient_scl		= pLocTri->m_nAmbientScale[0];

							//PF_PUSH_TIMEBAR_DETAIL("AddTriPlant");
							CGrassRenderer::AddTriPlant(&triPlant, 0);
							//PF_POP_TIMEBAR_DETAIL();
#if __BANK
							nLocTriDrawn++;
#endif
							bPlantsRendered = true;
						}//	for( ; (g_procInfo.m_plantInfos[plantInfoIndex].m_procTagId==procTagId) && (plantInfoIndex < g_procInfo.m_numPlantInfos); plantInfoIndex++)...
						//PF_POP_TIMEBAR_DETAIL();
					}//	if(IsLocTriVisibleByCamera(pLocTri))...
				} // if(!pLocTri->m_createsPlants)...

				loctri = pLocTri->m_NextTri;
			}//while(pLocTri)...

			PF_PUSH_TIMEBAR_DETAIL("FlushTriPlantBuffer");
			CGrassRenderer::FlushTriPlantBuffer();
		#if PLANTS_CAST_SHADOWS
			AddShadowCandidate(NULL);
		#endif
			PF_POP_TIMEBAR_DETAIL();
		}//for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)...

#if !__PS3
		CGrassRenderer::EndUseNormalTechniques();
#endif

		PF_POP_TIMEBAR_DETAIL();
		return bPlantsRendered;
}


//
//
//
//
bool CPlantMgr::RenderDecal()
{
	return gPlantMgr.RenderDecalInternal();
}

bool CPlantMgr::RenderDecalInternal()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	if(!gbPlantMgrActive)
		return(FALSE);

#if __BANK
	if (TiledScreenCapture::IsEnabled())
		return(FALSE);
	bRenderInternalActive = TRUE;	// RenderInternal() active
#endif

#if LOCTRISTAB_STR
	if(m_LocTrisTab[0] == NULL)
	{	// loc tris tabs not allocated, so skip:
	#if __BANK
		bRenderInternalActive = FALSE;	// RenderInternal() inactive
	#endif
		return(TRUE);
	}
#endif

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);


#if FURGRASS_TEST_V4
	PIXBegin(0, "Furgrass::Render");
	PF_PUSH_TIMEBAR_DETAIL("Furgrass::Render");

	if(DoFurgrassRender())
	{
		m_bFurgrassTrisRtPresent[GetRenderRTBufferID()] =
			FurGrassLODRender(CGrassRenderer::GetGlobalCameraPos(), CGrassRenderer::GetGlobalCameraFront(), CGrassRenderer::GetGlobalPlayerPos());
	}

	PF_POP_TIMEBAR_DETAIL();
	PIXEnd();
#endif

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);

#if __BANK
	bRenderInternalActive = FALSE;	// RenderInternal() inactive
#endif
	return(TRUE);
}// end of RenderDecalInternal()...


#if PLANTS_CAST_SHADOWS

bool CPlantMgr::ShadowRender()
{
#if !__FINAL
	if(gbPlantMgrRenderActive)
	{
#endif
	#if __BANK
		if(CGrassRenderer::ms_drawShadows)
		{
	#endif
			bool ret = gPlantMgr.ShadowRenderInternal();
			return ret;
	#if __BANK
		}
		else
		{
			return true;
		}
	#endif
#if !__FINAL
	}
	else
	{
		return true;
	}
#endif
}


bool CPlantMgr::ShadowRenderInternal()
{
	PIXBegin(0, "PlantsMgrRenderShadow");
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	grcWorldIdentity();
	const s32 oldStoredSeed	= g_PlantsRendRand[g_RenderThreadIndex].GetSeed();

	spdTransposedPlaneSet8 cullFrustum;
	cullFrustum.Set(*grcViewport::GetCurrent());

	grcStateBlock::SetRasterizerState(hGrassShadowRS);
	RenderShadowCandidates(cullFrustum);
	grcStateBlock::SetRasterizerState(hGrassShadowRS_end);

	g_PlantsRendRand[g_RenderThreadIndex].Reset(oldStoredSeed);

	PIXEnd();
	return true;
}


void CPlantMgr::InitShadowCandidates()
{
	for(u32 i=0; i<2; i++)
	{
		m_noOfShadowCandidates[i] = 0;
	}
}


void CPlantMgr::ResetShadowCandidates()
{
#if __BANK
	static u8 printWarn=0;
	if(m_noOfShadowCandidates[GetUpdateRTBufferID()] == CPLANT_LOC_TRI_SHADOW_CANDIDATES_SIZE && (!((printWarn++)&0x7f)))
	{
		plantsWarningf("CPlantMgr::ResetShadowCandidates(): Need to up the shadow candidate buffer!");
	}
#endif

	m_noOfShadowCandidates[GetUpdateRTBufferID()] = 0;
}


void CPlantMgr::AddShadowCandidate(CPlantLocTri *pLocTri)
{
	if(m_noOfShadowCandidates[GetUpdateRTBufferID()] < CPLANT_LOC_TRI_SHADOW_CANDIDATES_SIZE)
	{
		m_ShadowCandidates[GetUpdateRTBufferID()][ m_noOfShadowCandidates[GetUpdateRTBufferID()]++ ] = pLocTri;
	}
}


void CPlantMgr::RenderShadowCandidates(spdTransposedPlaneSet8 &cullFrustum)
{
const bool bIsCameraUnderwater				= CGrassRenderer::GetGlobalCameraUnderwater();
const float fGlobaPlayerCollisionRadiusSqr	= CGrassRenderer::GetGlobalPlayerCollisionRSqr();


	const u32 shadowCandidatesCount = m_noOfShadowCandidates[GetRenderRTBufferID()];
	if(shadowCandidatesCount == 0)
	{
		return;
	}

u32 noOfLocTris = 0;

	CGrassRenderer::BeginUseShadowTechniques(cullFrustum);

	for(u32 candidateIdx=0; candidateIdx<shadowCandidatesCount; candidateIdx++)
	{
		CPlantLocTri *pLocTri = m_ShadowCandidates[GetRenderRTBufferID()][candidateIdx];

		if(pLocTri == NULL)
		{
			if(noOfLocTris != 0)
			{
				CGrassRenderer::FlushTriPlantBuffer();
				noOfLocTris = 0;
			}
			continue;
		}

		// Check if pLocTri inside current View Frustum:
		if(IsLocTriVisibleByCamera(cullFrustum, pLocTri) && IsLocTriVisibleUnderwater(pLocTri, bIsCameraUnderwater))
		{
			s32 procTagId = PGTAMATERIALMGR->UnpackProcId(pLocTri->m_nSurfaceType);
			s32 plantInfoIndex = g_procInfo.m_procTagTable[procTagId].plantIndex;

#if (RSG_PC || RSG_DURANGO || RSG_ORBIS)
			if (plantInfoIndex < 0)
			{
				continue;
			}
#endif	//	(RSG_PC || RSG_DURANGO || RSG_ORBIS)

			atHashValue currentProcTag = g_procInfo.m_plantInfos[plantInfoIndex].m_Tag;

			for( ; plantInfoIndex < g_procInfo.m_plantInfos.GetCount() && g_procInfo.m_plantInfos[plantInfoIndex].m_Tag==currentProcTag ; plantInfoIndex++)
			{
				CPlantInfo *pPlantInfo = &g_procInfo.m_plantInfos[plantInfoIndex];

				if(pPlantInfo->m_Flags.IsSet(PROCPLANT_NOSHADOW))
				{
					continue;
				}

				PPTriPlant triPlant;
				triPlant.V1				=	pLocTri->m_V1;
				triPlant.V2				=	pLocTri->m_V2;
				triPlant.V3				=	pLocTri->m_V3;
				Assert(pPlantInfo->m_ModelId < 255);	// must fit into u8
				triPlant.model_id		=	(u8)pPlantInfo->m_ModelId;

				g_PlantsRendRand[g_RenderThreadIndex].Reset(pLocTri->GetSeed());

				triPlant.groundColorV1 = pLocTri->m_GroundColorV1;
				triPlant.groundColorV2 = pLocTri->m_GroundColorV2;
				triPlant.groundColorV3 = pLocTri->m_GroundColorV3;

				//
				// max number of plants to generate for this TriLoc
				// this is connected with triangle area + surface type:
				//

				// grab medium pv density weight for tri:
				const float pvDensityWeight = CPlantLocTri::pv8UnpackAndMapDensity(triPlant.groundColorV1.GetAlpha());
				float fDensity = pPlantInfo->m_Density.GetFloat32_FromFloat16() - pvDensityWeight*pPlantInfo->m_DensityRange.GetFloat32_FromFloat16();
				fDensity = fDensity>=0.0f? fDensity : 0.0f;	// density must be positive

				float numPlants = pLocTri->GetTriArea() * fDensity;
				s32 nMaxNumPlants = (s32)numPlants;
				float remainder = numPlants - nMaxNumPlants;
				if (g_PlantsRendRand[g_RenderThreadIndex].GetRanged(0.0f, 1.0f)<remainder)
				{
					nMaxNumPlants++;
				}		

				if (nMaxNumPlants==0)
				{
					continue;
				}

				triPlant.num_plants		=	(u16)nMaxNumPlants;

				Assert(m_PlantTextureTab0[pPlantInfo->m_TextureId]);
				Assert(m_PlantTextureTab0[pPlantInfo->m_TextureId+CPLANT_SLOT_NUM_TEXTURES]);
				triPlant.texture_ptr	=	m_PlantTextureTab0[pPlantInfo->m_TextureId];							// texture ptr to use
				triPlant.textureLOD1_ptr=	m_PlantTextureTab0[pPlantInfo->m_TextureId+CPLANT_SLOT_NUM_TEXTURES];	// textureLOD1 ptr to use

				triPlant.color			=	pPlantInfo->m_Color;
				triPlant.intensity		=	pPlantInfo->m_Intensity;
				triPlant.intensity_var	=	pPlantInfo->m_IntensityVar;

				triPlant.seed			=	pLocTri->GetSeed() + (plantInfoIndex*0x33);	// g_PlantsRendRand.GetRanged(0.0f, 1.0f)

				triPlant.packedScale.x	= pPlantInfo->m_ScaleXY;			// scale in XY
				triPlant.packedScale.y	= pPlantInfo->m_ScaleZ;			// scale in Z
				triPlant.packedScale.z	= pPlantInfo->m_ScaleVariationXY;		// scale xyz variation
				triPlant.packedScale.w	= pPlantInfo->m_ScaleVariationZ;

				triPlant.scaleRangeXYZ_Z.x = pPlantInfo->m_ScaleRangeXYZ;
				triPlant.scaleRangeXYZ_Z.y = pPlantInfo->m_ScaleRangeZ;

				triPlant.um_param.x		= pPlantInfo->m_MicroMovementsScaleH;		// micro-movements: global horizontal scale
				triPlant.um_param.y		= pPlantInfo->m_MicroMovementsScaleV;		// micro-movements: global vertical scale
				triPlant.um_param.z		= pPlantInfo->m_MicroMovementsFreqH;		// micro-movements: global horizontal frequency
				triPlant.um_param.w		= pPlantInfo->m_MicroMovementsFreqV;		// micro-movements: global vertical frequency

				triPlant.V1.w = pPlantInfo->m_WindBendScale.GetFloat32_FromFloat16();		// wind bending scale
				triPlant.V2.w = pPlantInfo->m_WindBendVariation.GetFloat32_FromFloat16();	// wind bending variation

				static dev_float fPlayerDefaultCollRadiusSqr = 1.0f*1.0f;	//0.75f;	// radius of influence
				const float collisionRadiusSqr = pPlantInfo->GetCollisionRadiusSqr() * fGlobaPlayerCollisionRadiusSqr * fPlayerDefaultCollRadiusSqr;
				// x = collision radius sqr:
				triPlant.coll_params.x.SetFloat16_FromFloat32(collisionRadiusSqr);
				// y = inv collision radius sqr:
				triPlant.coll_params.y.SetFloat16_FromFloat32(1.0f/collisionRadiusSqr);

				triPlant.flags		= pPlantInfo->m_Flags;
				if(pLocTri->m_bDrawFarTri)
				{
					triPlant.flags.Set(PROCPLANT_LOD2FARFADE);	// set "far far fade" flag
				}

				if(triPlant.flags.IsClear(PROCPLANT_LOD0) && triPlant.flags.IsClear(PROCPLANT_LOD1) && triPlant.flags.IsClear(PROCPLANT_LOD2))
				{
					continue;	// if no visible LODs for procedural grass then skip this tri
				}

				// skewing info:
				triPlant.skewAxisAngle	= pLocTri->m_skewAxisAngle;

				// lame: m_normal should be u32:
				triPlant.loctri_normal[0]	= pLocTri->m_normal[0];
				triPlant.loctri_normal[1]	= pLocTri->m_normal[1];
				triPlant.loctri_normal[2]	= pLocTri->m_normal[2];

				CGrassRenderer::AddTriPlant(&triPlant, 0);
				noOfLocTris++;
			}
		}
	}

	if(noOfLocTris != 0)
	{
		CGrassRenderer::FlushTriPlantBuffer();
	}

	CGrassRenderer::EndUseShadowTechniques();
}
#endif //PLANTS_CAST_SHADOWS

//#pragma mark -
//#pragma mark --- CPlantColEntEntry stuff ---

//
//
//
//
CPlantColBoundEntry*	CPlantColBoundEntry::AddEntry(CEntity *pEntity, s32 parentIdx, s32 childIdx)
{
	Assert(gPlantMgr.m_ColEntCache.m_UnusedListHead);

	this->m__pEntity = pEntity;	// this assigment must be first, otherwise all Get...() methods below won't work...
	this->m_bMaterialBound	= false; 
	this->m_nBParentIndex	= -1;
	this->m_nBChildIndex	= -1;
	#if __ASSERT	// can't have both systems valid at the same time
		if(pEntity)
		{
			Assert(parentIdx==-1);
			Assert(childIdx	==-1);
		}
		else
		{
			Assert(parentIdx!=-1);
			Assert(childIdx	!=-1);
		}
	#endif

	if((parentIdx!=-1) && (childIdx!=-1))
	{
		Assert(pEntity==NULL);
		this->m_bMaterialBound	= true;
		this->m_nBParentIndex	= parentIdx;
		this->m_nBChildIndex	= childIdx;
	}

	phBoundGeometry* pBound = this->GetBound();
	Assert(pBound); // should it ever happen?
	if(!pBound)
	{
		return(NULL);	// nothing added...
	}

	const Matrix34 *pSrcBoundMat = this->GetBoundMatrix();
	if(pSrcBoundMat)
	{
		this->m_BoundMat.Set(*pSrcBoundMat);
		// check whether BoundMat is an identity matrix (so no transformations are required for LocTris):
		this->m_bBoundMatIdentity = (!pSrcBoundMat->IsNotEqual(M34_IDENTITY));
	}
	else
	{	// should it ever happen?
		// idea: maybe flag this situation and get matrix later?
		//Assertf(FALSE, "Bound matrix not available!");
		this->m_BoundMat.Set(M34_IDENTITY);
		this->m_bBoundMatIdentity = TRUE;
	}


	if(pBound->GetNumPolygons() > 0)
	{
		const u32 numTris	= pBound->GetNumPolygons();
		Assert(numTris < 65536);	// must fit into u16
	#if !__FINAL
		if(numTris >= 65536)
		{
			Displayf("\n Invalid amount of polygons in bound: %d", numTris);
			return(NULL);
		}
	#endif
		this->m_nNumTris 	= (u16)numTris;
		this->m_LocTriArray	= rage_aligned_new(16) CTriHashIdx16[numTris];
		Assert(this->m_LocTriArray);
		if(!this->m_LocTriArray)
		{
			return(NULL);
		}
		Assert16(this->m_LocTriArray);	// must be dma'able by PlantsMgrUpdateSpu

		sysMemSet(this->m_LocTriArray, 0x00, numTris*sizeof(CTriHashIdx16));

		u32 netSkipCount=0;
		// initialize material props for all polys of the bound:
		this->m_BoundMatProps.Init(numTris*2);
		for(u32 i=0; i<numTris; i++)
		{
			// check surface type of polygon:
			const s32 boundMaterialID	= pBound->GetPolygonMaterialIndex(i);
			const phMaterialMgr::Id	actualMaterialID = pBound->GetMaterialId(boundMaterialID);

			s32 procTagId = PGTAMATERIALMGR->UnpackProcId(actualMaterialID);
			bool bCreatesPlants  = g_procInfo.CreatesPlants(procTagId);
			bool bCreatesObjects = g_procInfo.CreatesProcObjects(procTagId);

#if 0	// this check is now done during export
			const phPolygon& poly = pBound->GetPolygon(i);

			// measure sides of triangles in current bound (potential pLocTris)
			// and reject them if they are too small / too thin:
			const phPolygon::Index vIndex0 = poly.GetVertexIndex(0);
			const phPolygon::Index vIndex1 = poly.GetVertexIndex(1);
			const phPolygon::Index vIndex2 = poly.GetVertexIndex(2);
			const Vector3 tv0 = pBound->GetVertex(vIndex0);
			const Vector3 tv1 = pBound->GetVertex(vIndex1);
			const Vector3 tv2 = pBound->GetVertex(vIndex2);

			// calculate lengths of triangle's sides:
			const Vector3 side0 = tv0 - tv1;
			const Vector3 side1 = tv1 - tv2;
			const Vector3 side2 = tv2 - tv0;
			const float sideLenSqr0 = side0.Mag2();
			const float sideLenSqr1 = side1.Mag2();
			const float sideLenSqr2 = side2.Mag2();

const float TRILOC_MINIMUM_SIDE_LEN = 2.5f;

			if( (sideLenSqr0 < TRILOC_MINIMUM_SIDE_LEN*TRILOC_MINIMUM_SIDE_LEN) ||
				(sideLenSqr1 < TRILOC_MINIMUM_SIDE_LEN*TRILOC_MINIMUM_SIDE_LEN) ||
				(sideLenSqr2 < TRILOC_MINIMUM_SIDE_LEN*TRILOC_MINIMUM_SIDE_LEN)	)
			{	// triangle is too small, don't allow procedural stuff to be created on it:
				bCreatesObjects = FALSE;
				bCreatesPlants	= FALSE;
			}
#endif //#if 0...

			if(NetworkInterface::IsGameInProgress())
			{
				if(bCreatesPlants)
				{
					if((netSkipCount&0x03)==3)
					{
						bCreatesObjects	=
						bCreatesPlants	= false;
					}
					netSkipCount++;
				}
			}

			this->m_BoundMatProps.Set(i*2+0, bCreatesPlants); 
			this->m_BoundMatProps.Set(i*2+1, bCreatesObjects);
		}
		
#if PLANTSMGR_MULTI_UPDATE
		this->m_processedTris.Init(numTris);
#endif

		gPlantMgr.MoveColEntToList(&gPlantMgr.m_ColEntCache.m_UnusedListHead, &gPlantMgr.m_ColEntCache.m_CloseListHead, this);


#if FURGRASS_TEST_V4
		// Furgrass: pick up textures and settings from underlying map object:

		this->m_furInfo.m_bValid = FALSE;	// mark as invalid for a start
		if(!IsMatBound() && pEntity && pEntity->IsArchetypeSet())
		{
			CBaseModelInfo* pMI = pEntity->GetBaseModelInfo();
			Assert(pMI);

			rmcDrawable *pDrawable = pMI->GetDrawable();
			if(pDrawable)
			{
				grmShaderGroup *pShaderGroup = &pDrawable->GetShaderGroup();
				Assert(pShaderGroup);
				grmShader *pShader = pShaderGroup->LookupShader("grass_fur_lod");
				if(pShader)
				{
					// DiffuseTexFur:
					grcEffectVar varDiffTex = pShader->LookupVar("DiffuseTexFur", false);
					if(varDiffTex)
					{
						pShader->GetVar(varDiffTex, this->m_furInfo.m_furGrassDiffTex);
					}
					else
					{
						this->m_furInfo.m_furGrassDiffTex = NULL;
					}
					
					// BumpTexFur:
					grcEffectVar varBumpTex = pShader->LookupVar("BumpTexFur", false);
					if(varBumpTex)
					{
						pShader->GetVar(varBumpTex, this->m_furInfo.m_furGrassNormalTex);
					}
					else
					{
						this->m_furInfo.m_furGrassNormalTex = NULL;
					}

					// ComboHeightTexFur:
					grcEffectVar varComboHeighTex0 = pShader->LookupVar("ComboHeightTexFur", false);
					if(varComboHeighTex0)
					{
						pShader->GetVar(varComboHeighTex0, this->m_furInfo.m_furGrassComboH4Tex[0]);
					}
					else
					{
						this->m_furInfo.m_furGrassComboH4Tex[0] = NULL;
					}

					// ComboHeightTexFur2:
					grcEffectVar varComboHeighTex2 = pShader->LookupVar("ComboHeightTexFur2", false);
					if(varComboHeighTex2)
					{
						pShader->GetVar(varComboHeighTex2, this->m_furInfo.m_furGrassComboH4Tex[1]);
					}
					else
					{
						this->m_furInfo.m_furGrassComboH4Tex[1] = NULL;
					}

					// ComboHeightTexFur3:
					grcEffectVar varComboHeighTex3 = pShader->LookupVar("ComboHeightTexFur3", false);
					if(varComboHeighTex3)
					{
						pShader->GetVar(varComboHeighTex3, this->m_furInfo.m_furGrassComboH4Tex[2]);
					}
					else
					{
						this->m_furInfo.m_furGrassComboH4Tex[2] = NULL;
					}

					// ComboHeightTexFur4:
					grcEffectVar varComboHeighTex4 = pShader->LookupVar("ComboHeightTexFur4", false);
					if(varComboHeighTex4)
					{
						pShader->GetVar(varComboHeighTex4, this->m_furInfo.m_furGrassComboH4Tex[3]);
					}
					else
					{
						this->m_furInfo.m_furGrassComboH4Tex[3] = NULL;
					}

					this->m_furInfo.m_bValid = TRUE;	// mark as valid
				}// if(pShader)...
			}// if(pDrawable)...
		}// if(pEntity->GetModelIndex() != INVALID_MI)...
#endif //FURGRASS_TEST_V4...

		return(this);
	}

	return(NULL);
}


//
//
//
//
void CPlantColBoundEntry::ReleaseEntry()
{
	if(this->m_LocTriArray)
	{
		// release LocTris first:
		const u32 count = this->m_nNumTris;
		for(u32 i=0; i<count; i++)
		{
			const CTriHashIdx16 hashIdx = m_LocTriArray[i];
			const u16 listID = hashIdx.GetListID();
			const u16 tri	 = hashIdx.GetIdx();
			if(tri)
			{
				CPlantLocTriArray& triTab = *gPlantMgr.m_LocTrisTab[listID];
				CPlantLocTri *pTri = &triTab[tri];
				pTri->Release(triTab);
			}
		}
	
		delete[] this->m_LocTriArray;
		this->m_LocTriArray		= NULL;
		this->m_BoundMatProps.Kill();
#if PLANTSMGR_MULTI_UPDATE
		this->m_processedTris.Kill();
#endif

		this->m_nNumTris		= 0;
		#if FURGRASS_TEST_V4
			this->m_furInfo.m_bValid= FALSE;
		#endif
	}

	if(IsMatBound())
	{
		Assert(this->m__pEntity==NULL);
		this->m_nBParentIndex	=-1;
		this->m_nBChildIndex	=-1;
	}
	else
	{
		Assert(this->m_nBParentIndex==-1);
		Assert(this->m_nBChildIndex==-1);
		this->m__pEntity		= NULL;
	}

	gPlantMgr.MoveColEntToList(&gPlantMgr.m_ColEntCache.m_CloseListHead, &gPlantMgr.m_ColEntCache.m_UnusedListHead, this);
}



//#pragma mark -
//#pragma mark --- MoveToList stuff ---


#if CPLANT_DYNAMIC_CULL_SPHERES
//
//
//
//
u32 CPlantMgr::AddDynamicCullSphere(const spdSphere &s)
{
	u32 handle = PLANTSMGR_INVALID_DYNAMIC_CULL_SPHERE_INDEX;

	//Don't add an invalid sphere.
	if(s.GetRadiusf() <= 0.0f)
	{
		Assertf(s.GetRadiusf() > 0.0f, "WARNING: Call to add a dynamic culling sphere for grass ignored. An invalid sphere was passed in:\t\tCenter = {%f, %f, %f}\tRadius = %f",
				VEC3V_ARGS(s.GetCenter()), s.GetRadiusf()	);
		return handle;
	}

	//Search for an empty slot in the cull sphere array to add the current sphere.
	for(DynCullSphereSourceArray::size_type i = 0; i < m_DynCullSpheres.size(); ++i)
	{
		if(m_DynCullSpheres[i].GetRadiusf() == 0.0f)
		{
			m_DynCullSpheres[i] = s;
			handle = i;
			break;
		}
	}

	Assertf(	handle != PLANTSMGR_INVALID_DYNAMIC_CULL_SPHERE_INDEX,
				"WARNING: Grass dynamic cull sphere array is full! Ignoring call to add new cull sphere:\t\tCenter = {%f, %f, %f}\tRadius = %f",
				VEC3V_ARGS(s.GetCenter()), s.GetRadiusf()	);

	return handle;
}


void CPlantMgr::RemoveDynamicCullSphere(u32 handle)
{
	if(Verifyf(	handle < m_DynCullSpheres.size(), "WARNING: Invalid index/handle passed into RemoveDynamicCullSphere()!\t\tMax Index = %d\thandle = %d %s",
				m_DynCullSpheres.size(), handle, (handle == PLANTSMGR_INVALID_DYNAMIC_CULL_SPHERE_INDEX ? "(PLANTSMGR_INVALID_DYNAMIC_CULL_SPHERE_INDEX)" : "")	))
	{
		m_DynCullSpheres[handle].SetV4(Vec4V(V_ZERO));	//Same as default spdSphere constructor
	}
}

void CPlantMgr::ResetDynamicCullSpheres()
{
	DynCullSphereSourceArray::iterator iter;
	DynCullSphereSourceArray::iterator end = m_DynCullSpheres.end();
	for(iter = m_DynCullSpheres.begin(); iter != end; ++iter)
	{
		iter->SetV4(Vec4V(V_ZERO));
	}
}


void CPlantMgr::CopyCullSpheresToUpdateBuffer()
{
	const u32 id = gPlantMgr.GetUpdateRTBufferID();
	CPlantMgr::DynCullSphereArray &dest = m_DynCullSpheresBuffer[id];
	dest.Reset();

	//Loop through source array and add only valid spheres
	DynCullSphereSourceArray::const_iterator iter;
	DynCullSphereSourceArray::const_iterator end = m_DynCullSpheres.end();
	for(iter = m_DynCullSpheres.begin(); iter != end; ++iter)
	{
		if(iter->GetRadiusf() > 0.0f && gbPlantMgrDynCullSpheresEnable)
			dest.Push(*iter);
	}
}
#endif //CPLANT_DYNAMIC_CULL_SPHERES...


//
//
//
//
CPlantColBoundEntry* CPlantMgr::MoveColEntToList(u16* ppCurrentList, u16* ppNewList, CPlantColBoundEntry *pEntry)
{
	Assertf(*ppCurrentList, "CPlantMgr::MoveColEntToList(): m_CurrentList==NULL!");

	// First - Cut out of old list
	if(pEntry->m_PrevEntry==0)
	{	// if at head of old list
		*ppCurrentList = pEntry->m_NextEntry;
		if(*ppCurrentList)
			m_ColEntCache[*ppCurrentList].m_PrevEntry = 0;
	}
	else if(pEntry->m_NextEntry==0)
	{	// if at tail of old list
		m_ColEntCache[pEntry->m_PrevEntry].m_NextEntry = 0;
	}
	else
	{	// else if in middle of old list
		m_ColEntCache[pEntry->m_NextEntry].m_PrevEntry = pEntry->m_PrevEntry;
		m_ColEntCache[pEntry->m_PrevEntry].m_NextEntry = pEntry->m_NextEntry;
	}

	// Second - Insert at start of new list
	pEntry->m_NextEntry	= *ppNewList;
	pEntry->m_PrevEntry	= 0;
	*ppNewList = m_ColEntCache.GetIdx(pEntry);	// get hashed index of pEntry
	
	if(pEntry->m_NextEntry)
		m_ColEntCache[pEntry->m_NextEntry].m_PrevEntry = *ppNewList;

	return(pEntry);
}// end of CPlantMgr::MoveColEntToList()...


////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// DEBUG STUFF:		
// This debug render function render to gbuffers after main grass render
//
////////////////////////////////////////////////////////////////////////////////////////////////////////
#if __BANK
bool CPlantMgr::RenderDebugStuff()
{
	DbgPrintCPlantMgrInfo(nLocTriDrawn, nLocTriTexSkipped, nLocTriPlantsLOD0Drawn, nLocTriPlantsLOD1Drawn, nLocTriPlantsLOD2Drawn);

#if __BANK
	// reset statistics
	nLocTriDrawn				=
	nLocTriTexSkipped			=
	nLocTriPlantsLOD0Drawn		=
	nLocTriPlantsLOD1Drawn		=
	nLocTriPlantsLOD2Drawn		= 0;	// reset statistics
#endif

#if LOCTRISTAB_STR
	if(m_LocTrisTab[0] == NULL)
	{
		return(FALSE);
	}
#endif

#if PLANTSMGR_DATA_EDITOR
	gPlantMgrEditor.Update();
#endif

#if __BANK
	static dev_float dbgHdrVal = 1.0f;
	CShaderLib::SetGlobalEmissiveScale(dbgHdrVal);

	// debug render of all plant polygons:
	if(gbShowCPlantMgrPolys || gbShowCPlantMgrPolysFar)
	{
		atArray<Vector3>		dbgTriangleVertices;
		atArray<Color32>		dbgTriangleColors;

		dbgTriangleVertices.Reset();
		dbgTriangleVertices.Reserve(64);
		dbgTriangleColors.Reset();
		dbgTriangleColors.Reserve(64);

		Color32 testColor0(255, 255, 255, 255);
		Color32 testColor1(255, 0,   0,   255);
		Color32 testColor2(0,   0,   255, 255);

		//
		// render all close lists:
		//
		for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)
		{
			CPlantLocTriArray& triTab = *m_LocTrisTab[listID];
			u16 loctri = triTab.m_CloseListHead;
			while(loctri)
			{
				CPlantLocTri *pLocTri = &triTab[loctri];
				Vector3 pVert0 = pLocTri->GetV1();
				Vector3 pVert1 = pLocTri->GetV2();
				Vector3 pVert2 = pLocTri->GetV3();
				
				bool bAddPoly = true;
				if(gbShowCPlantMgrPolysFar && !gbShowCPlantMgrPolys)
				{
					bAddPoly = pLocTri->m_bDrawFarTri;
				}

			#if CPLANT_USE_OCCLUSION
				if(gbPlantsUseOcclusion)
				{
					bAddPoly = (!pLocTri->m_bOccluded);	// skip occluded polys
				}
			#endif

				bool bAddPoly2 = false;
				if(gbShowCPlantMgrPolys_Plants)
				{
					if(pLocTri->m_bCreatesPlants)
					{
						bAddPoly2 = true;
					}
				}
				
				if(gbShowCPlantMgrPolys_ProcObj)
				{
					if(pLocTri->m_bCreatesObjects)
					{
						bAddPoly2 = true;
					}
				}

				if(bAddPoly && bAddPoly2)
				{
					dbgTriangleVertices.PushAndGrow(pVert0);
					dbgTriangleColors.PushAndGrow(testColor0);
					dbgTriangleVertices.PushAndGrow(pVert1);
					dbgTriangleColors.PushAndGrow(testColor1);
					dbgTriangleVertices.PushAndGrow(pVert2);
					dbgTriangleColors.PushAndGrow(testColor2);
				}
				loctri = pLocTri->m_NextTri;
			}//while(pLocTri)...

		}//for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)...

		// setup render state:
		{
			grcWorldIdentity();
			grcBindTexture(NULL);

			static grcRasterizerStateHandle hRasterizerState = grcStateBlock::RS_Invalid;
			if(hRasterizerState == grcStateBlock::RS_Invalid)
			{
				grcRasterizerStateDesc rasDesc;
				rasDesc.CullMode	= grcRSV::CULL_NONE;
				rasDesc.FillMode	= grcRSV::FILL_SOLID;
				
				hRasterizerState	= grcStateBlock::CreateRasterizerState(rasDesc);
				Assert(hRasterizerState != grcStateBlock::RS_Invalid);
			}
			grcStateBlock::SetRasterizerState(hRasterizerState);

			static grcBlendStateHandle hBlendState = grcStateBlock::BS_Invalid;
			if(hBlendState == grcStateBlock::BS_Invalid)
			{
				grcBlendStateDesc bsDesc;
				bsDesc.IndependentBlendEnable		= FALSE;
				bsDesc.BlendRTDesc[0].BlendEnable	= FALSE;
				#if __XENON
					// REL-1-11: im rendering supports only 1 MRT
					bsDesc.IndependentBlendEnable		= TRUE;
					bsDesc.BlendRTDesc[0].BlendEnable	= FALSE;
					bsDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
					bsDesc.BlendRTDesc[1].BlendEnable	= FALSE;
					bsDesc.BlendRTDesc[1].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
					bsDesc.BlendRTDesc[2].BlendEnable	= FALSE;
					bsDesc.BlendRTDesc[2].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
					bsDesc.BlendRTDesc[3].BlendEnable	= FALSE;
					bsDesc.BlendRTDesc[3].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
				#endif
				
				hBlendState = grcStateBlock::CreateBlendState(bsDesc);
				Assert(hBlendState != grcStateBlock::BS_Invalid);
			}
			grcStateBlock::SetBlendState(hBlendState);

			static grcDepthStencilStateHandle hDepthStencilState = grcStateBlock::DSS_Invalid;
			if(hDepthStencilState == grcStateBlock::DSS_Invalid)
			{
				grcDepthStencilStateDesc dsDesc;
				dsDesc.DepthEnable					= TRUE;
				dsDesc.DepthWriteMask				= TRUE;
				dsDesc.DepthFunc					= rage::FixupDepthDirection(grcRSV::CMP_LESS);

				hDepthStencilState		= grcStateBlock::CreateDepthStencilState(dsDesc);
				Assert(hDepthStencilState != grcStateBlock::DSS_Invalid);
			}
			grcStateBlock::SetDepthStencilState(hDepthStencilState);

			#if __PS3 || (RSG_PC || RSG_DURANGO || RSG_ORBIS)				// REL-1-11: im rendering supports only 1 MRT:
				GBuffer::UnlockTargets();
				GBuffer::LockSingleTarget(GBUFFER_RT_0, TRUE);
			#endif

			DbgDrawTriangleArrays(dbgTriangleVertices, dbgTriangleColors);
			
			#if __PS3 || (RSG_PC || RSG_DURANGO || RSG_ORBIS)				// REL-1-11: im rendering supports only 1 MRT:
				GBuffer::UnlockSingleTarget(GBUFFER_RT_0);
				GBuffer::LockTargets();
			#endif

			grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
			grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
			grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
		}

	}// if(gbShowCPlantMgrPolys)...

	// debug render of all plant polygons:
	if(gbShowCPlantMgrPolyNormals)
	{
		atArray<Vector3>		dbgLineVertices;
		atArray<Color32>		dbgLineColors;

		Color32 testColor0(255, 255, 255, 255);
//		Color32 testColor1(255, 0,   0,   255);
//		Color32 testColor2(0,   0,   255, 255);

		dbgLineVertices.Reset();
		dbgLineVertices.Reserve(64);
		dbgLineColors.Reset();
		dbgLineColors.Reserve(64);

		//
		// render all close lists:
		//
		for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)
		{
			CPlantLocTriArray& triTab = *m_LocTrisTab[listID];
			u16 loctri = triTab.m_CloseListHead;
			while(loctri)
			{
				CPlantLocTri *pLocTri = &triTab[loctri];
				Vector3 pVert0 = pLocTri->GetV1();
				//Vector3 pVert1 = pLocTri->GetV2();
				//Vector3 pVert2 = pLocTri->GetV3();
				
				Vector3 polyNormal(pLocTri->m_normal[0]/127.0f, pLocTri->m_normal[1]/127.0f, pLocTri->m_normal[2]/127.0f);
				
				dbgLineVertices.PushAndGrow(pVert0);
				dbgLineColors.PushAndGrow(testColor0);
				dbgLineVertices.PushAndGrow(pVert0+polyNormal*10.0f);
				dbgLineColors.PushAndGrow(testColor0);

				loctri = pLocTri->m_NextTri;
			}//while(pLocTri)...

		}//for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)...

		// setup render state:
		{
			grcWorldIdentity();
			grcBindTexture(NULL);

			static grcRasterizerStateHandle hRasterizerState = grcStateBlock::RS_Invalid;
			if(hRasterizerState == grcStateBlock::RS_Invalid)
			{
				grcRasterizerStateDesc rasDesc;
				rasDesc.CullMode	= grcRSV::CULL_NONE;
				rasDesc.FillMode	= grcRSV::FILL_SOLID;
				
				hRasterizerState	= grcStateBlock::CreateRasterizerState(rasDesc);
				Assert(hRasterizerState != grcStateBlock::RS_Invalid);
			}
			grcStateBlock::SetRasterizerState(hRasterizerState);

			static grcBlendStateHandle hBlendState = grcStateBlock::BS_Invalid;
			if(hBlendState == grcStateBlock::BS_Invalid)
			{
				grcBlendStateDesc bsDesc;
				bsDesc.IndependentBlendEnable		= FALSE;
				bsDesc.BlendRTDesc[0].BlendEnable	= FALSE;
				#if __XENON
					// REL-1-11: im rendering supports only 1 MRT
					bsDesc.IndependentBlendEnable		= TRUE;
					bsDesc.BlendRTDesc[0].BlendEnable	= FALSE;
					bsDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
					bsDesc.BlendRTDesc[1].BlendEnable	= FALSE;
					bsDesc.BlendRTDesc[1].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
					bsDesc.BlendRTDesc[2].BlendEnable	= FALSE;
					bsDesc.BlendRTDesc[2].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
					bsDesc.BlendRTDesc[3].BlendEnable	= FALSE;
					bsDesc.BlendRTDesc[3].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
				#endif
				
				hBlendState = grcStateBlock::CreateBlendState(bsDesc);
				Assert(hBlendState != grcStateBlock::BS_Invalid);
			}
			grcStateBlock::SetBlendState(hBlendState);

			static grcDepthStencilStateHandle hDepthStencilState = grcStateBlock::DSS_Invalid;
			if(hDepthStencilState == grcStateBlock::DSS_Invalid)
			{
				grcDepthStencilStateDesc dsDesc;
				dsDesc.DepthEnable					= TRUE;
				dsDesc.DepthWriteMask				= TRUE;
				dsDesc.DepthFunc					= rage::FixupDepthDirection(grcRSV::CMP_LESS);
				
				hDepthStencilState		= grcStateBlock::CreateDepthStencilState(dsDesc);
				Assert(hDepthStencilState != grcStateBlock::DSS_Invalid);
			}
			grcStateBlock::SetDepthStencilState(hDepthStencilState);

			#if __PS3 || (RSG_PC || RSG_DURANGO || RSG_ORBIS)				// REL-1-11: im rendering supports only 1 MRT:
				GBuffer::UnlockTargets();
				GBuffer::LockSingleTarget(GBUFFER_RT_0, TRUE);
			#endif

			DbgDrawLineArrays(dbgLineVertices, dbgLineColors);
			
			#if __PS3 || (RSG_PC || RSG_DURANGO || RSG_ORBIS)				// REL-1-11: im rendering supports only 1 MRT:
				GBuffer::UnlockSingleTarget(GBUFFER_RT_0);
				GBuffer::LockTargets();
			#endif

			grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
			grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
			grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
		}

	}// if(gbShowCPlantMgrPolyNormals)...

	// debug render of all plant polygons:
	if(gbShowCPlantMgrGroundColor)
	{
		atArray<Vector3>		dbgTriangleVertices;
		atArray<Color32>		dbgTriangleColors;

		dbgTriangleVertices.Reset();
		dbgTriangleVertices.Reserve(128);
		dbgTriangleColors.Reset();
		dbgTriangleColors.Reserve(128);

		//
		// render all close lists:
		//
		for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)
		{
			CPlantLocTriArray& triTab = *m_LocTrisTab[listID];
			u16 loctri = triTab.m_CloseListHead;
			while(loctri)
			{
				CPlantLocTri *pLocTri = &triTab[loctri];

				Vector3 pVert0 = pLocTri->GetV1();
				Vector3 pVert1 = pLocTri->GetV2();
				Vector3 pVert2 = pLocTri->GetV3();

				Color32 colorV1, colorV2, colorV3; 
				if(gbForceGroundColor)
				{	// assign debug colors:
					colorV1 = gbGroundColorV1;
					colorV2 = gbGroundColorV2;
					colorV3 = gbGroundColorV3;
					
					// ... and debug densities & scales:
					colorV1.SetAlpha(CPlantLocTri::pv8PackDensityScaleZScaleXYZ((u8)gbGroundDensity_V1, (u8)gbGroundScaleZ_V1, (u8)gbGroundScaleXYZ_V1));
					colorV2.SetAlpha(CPlantLocTri::pv8PackDensityScaleZScaleXYZ((u8)gbGroundDensity_V2, (u8)gbGroundScaleZ_V2, (u8)gbGroundScaleXYZ_V2));
					colorV3.SetAlpha(CPlantLocTri::pv8PackDensityScaleZScaleXYZ((u8)gbGroundDensity_V3, (u8)gbGroundScaleZ_V3, (u8)gbGroundScaleXYZ_V3));
				}
				else
				{
					colorV1	= pLocTri->m_GroundColorV1;
					colorV2 = pLocTri->m_GroundColorV2;
					colorV3 = pLocTri->m_GroundColorV3;
				}

				switch(gbShowCPlantMgrGroundCSD)
				{
					case(0):	// color
					{
						colorV1.SetAlpha(255);
						colorV2.SetAlpha(255);
						colorV3.SetAlpha(255);
					}
					break;
					
					case(1):	// scaleXYZ
					{
						const float scale1 = CPlantLocTri::pv8UnpackAndMapScaleXYZ(colorV1.GetAlpha());
						const float scale2 = CPlantLocTri::pv8UnpackAndMapScaleXYZ(colorV2.GetAlpha());
						const float scale3 = CPlantLocTri::pv8UnpackAndMapScaleXYZ(colorV3.GetAlpha());
						const u8 cScale1 = u8(rage::Abs(scale1)*255.0f);
						const u8 cScale2 = u8(rage::Abs(scale2)*255.0f);
						const u8 cScale3 = u8(rage::Abs(scale3)*255.0f);
						
						if(scale1 >= 0.0f)
						{	// white: positive mapping <8; 15> -> <0.0666; 1>:
							colorV1.SetBytes(cScale1, cScale1, cScale1, 255);
						}
						else
						{	// red: negative mapping <0; 7> -> <-1; -0.0666>:
							colorV1.SetBytes(cScale1, 0, 0, 255);
						}
						
						if(scale2 >= 0.0f)
						{	// white: positive mapping <8; 15> -> <0.0666; 1>:
							colorV2.SetBytes(cScale2, cScale2, cScale2, 255);
						}
						else
						{	// red: negative mapping <0; 7> -> <-1; -0.0666>:
							colorV2.SetBytes(cScale2, 0, 0, 255);
						}

						if(scale3 >= 0.0f)
						{	// white: positive mapping <8; 15> -> <0.0666; 1>:
							colorV3.SetBytes(cScale3, cScale3, cScale3, 255);
						}
						else
						{	// red: negative mapping <0; 7> -> <-1; -0.0666>:
							colorV3.SetBytes(cScale3, 0, 0, 255);
						}
					}
					break;
					
					case(2):	// scaleZ
					{
						const float scale1 = CPlantLocTri::pv8UnpackScaleZ(colorV1.GetAlpha());
						const float scale2 = CPlantLocTri::pv8UnpackScaleZ(colorV2.GetAlpha());
						const float scale3 = CPlantLocTri::pv8UnpackScaleZ(colorV3.GetAlpha());
						const u8 cScale1 = u8((rage::Abs(scale1)/3.0f)*255.0f);
						const u8 cScale2 = u8((rage::Abs(scale2)/3.0f)*255.0f);
						const u8 cScale3 = u8((rage::Abs(scale3)/3.0f)*255.0f);
						
						colorV1.SetBytes(cScale1, cScale1, cScale1, 255);
						colorV2.SetBytes(cScale2, cScale2, cScale2, 255);
						colorV3.SetBytes(cScale3, cScale3, cScale3, 255);
					}
					break;

					case(3):	// density
					{
						const float density1 = CPlantLocTri::pv8UnpackDensity(colorV1.GetAlpha());
						const float density2 = CPlantLocTri::pv8UnpackDensity(colorV2.GetAlpha());
						const float density3 = CPlantLocTri::pv8UnpackDensity(colorV3.GetAlpha());
						const u8 bDensity1 = u8((rage::Abs(density1)/3.0f)*255.0f);
						const u8 bDensity2 = u8((rage::Abs(density2)/3.0f)*255.0f);
						const u8 bDensity3 = u8((rage::Abs(density3)/3.0f)*255.0f);
						
						colorV1.SetBytes(bDensity1, bDensity1, bDensity1, 255);
						colorV2.SetBytes(bDensity2, bDensity2, bDensity2, 255);
						colorV3.SetBytes(bDensity3, bDensity3, bDensity3, 255);
					}
					break;
					
					default:
						Assert(0);	// invalid state
						break;
				}

				dbgTriangleVertices.PushAndGrow(pVert0);
				dbgTriangleColors.PushAndGrow(colorV1);
				dbgTriangleVertices.PushAndGrow(pVert1);
				dbgTriangleColors.PushAndGrow(colorV2);
				dbgTriangleVertices.PushAndGrow(pVert2);
				dbgTriangleColors.PushAndGrow(colorV3);

				loctri = pLocTri->m_NextTri;
			}//while(pLocTri)...
		}//for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)...

		// setup render state:
		{
			grcWorldIdentity();
			grcBindTexture(NULL);

			static grcRasterizerStateHandle hRasterizerState = grcStateBlock::RS_Invalid;
			if(hRasterizerState == grcStateBlock::RS_Invalid)
			{
				grcRasterizerStateDesc rasDesc;
				rasDesc.CullMode	= grcRSV::CULL_NONE;
				rasDesc.FillMode	= grcRSV::FILL_SOLID;
				
				hRasterizerState	= grcStateBlock::CreateRasterizerState(rasDesc);
				Assert(hRasterizerState != grcStateBlock::RS_Invalid);
			}
			grcStateBlock::SetRasterizerState(hRasterizerState);

			static grcBlendStateHandle hBlendState = grcStateBlock::BS_Invalid;
			if(hBlendState == grcStateBlock::BS_Invalid)
			{
				grcBlendStateDesc bsDesc;
				bsDesc.IndependentBlendEnable		= FALSE;
				bsDesc.BlendRTDesc[0].BlendEnable	= FALSE;
				#if __XENON
					// REL-1-11: im rendering supports only 1 MRT
					bsDesc.IndependentBlendEnable		= TRUE;
					bsDesc.BlendRTDesc[0].BlendEnable	= FALSE;
					bsDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
					bsDesc.BlendRTDesc[1].BlendEnable	= FALSE;
					bsDesc.BlendRTDesc[1].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
					bsDesc.BlendRTDesc[2].BlendEnable	= FALSE;
					bsDesc.BlendRTDesc[2].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
					bsDesc.BlendRTDesc[3].BlendEnable	= FALSE;
					bsDesc.BlendRTDesc[3].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
				#endif
				
				hBlendState = grcStateBlock::CreateBlendState(bsDesc);
				Assert(hBlendState != grcStateBlock::BS_Invalid);
			}
			grcStateBlock::SetBlendState(hBlendState);

			static grcDepthStencilStateHandle hDepthStencilState = grcStateBlock::DSS_Invalid;
			if(hDepthStencilState == grcStateBlock::DSS_Invalid)
			{
				grcDepthStencilStateDesc dsDesc;
				dsDesc.DepthEnable					= TRUE;
				dsDesc.DepthWriteMask				= TRUE;
				dsDesc.DepthFunc					= rage::FixupDepthDirection(grcRSV::CMP_LESS);
				
				hDepthStencilState		= grcStateBlock::CreateDepthStencilState(dsDesc);
				Assert(hDepthStencilState != grcStateBlock::DSS_Invalid);
			}
			grcStateBlock::SetDepthStencilState(hDepthStencilState);

			#if __PS3 || (RSG_PC || RSG_DURANGO || RSG_ORBIS)			// REL-1-11: im rendering supports only 1 MRT:
				GBuffer::UnlockTargets();
				GBuffer::LockSingleTarget(GBUFFER_RT_0, TRUE);
			#endif

			DbgDrawTriangleArrays(dbgTriangleVertices, dbgTriangleColors);

			#if __PS3 || (RSG_PC || RSG_DURANGO || RSG_ORBIS)			// REL-1-11: im rendering supports only 1 MRT:
				GBuffer::UnlockSingleTarget(GBUFFER_RT_0);
				GBuffer::LockTargets();
			#endif

			grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
			grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
			grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
		}

	}// if(gbShowCPlantMgrPolys)...


	// print extracted & mapped CPV ScaleXYZ /  ScaleZ / Density:
	if(gbShowCPlantMgrGroundColor && (gbShowCPlantMgrGroundCSD > 0))
	{
		grcWorldIdentity();
		grcBindTexture(NULL);

		static grcRasterizerStateHandle hRasterizerState = grcStateBlock::RS_Invalid;
		if(hRasterizerState == grcStateBlock::RS_Invalid)
		{
			grcRasterizerStateDesc rasDesc;
			rasDesc.CullMode	= grcRSV::CULL_NONE;
			rasDesc.FillMode	= grcRSV::FILL_SOLID;
			
			hRasterizerState	= grcStateBlock::CreateRasterizerState(rasDesc);
			Assert(hRasterizerState != grcStateBlock::RS_Invalid);
		}
		grcStateBlock::SetRasterizerState(hRasterizerState);

		static grcBlendStateHandle hBlendState = grcStateBlock::BS_Invalid;
		if(hBlendState == grcStateBlock::BS_Invalid)
		{
			grcBlendStateDesc bsDesc;
			bsDesc.IndependentBlendEnable		= FALSE;
			bsDesc.BlendRTDesc[0].BlendEnable	= FALSE;
			#if __XENON
				// REL-1-11: im rendering supports only 1 MRT
				bsDesc.IndependentBlendEnable		= TRUE;
				bsDesc.BlendRTDesc[0].BlendEnable	= FALSE;
				bsDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
				bsDesc.BlendRTDesc[1].BlendEnable	= FALSE;
				bsDesc.BlendRTDesc[1].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
				bsDesc.BlendRTDesc[2].BlendEnable	= FALSE;
				bsDesc.BlendRTDesc[2].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
				bsDesc.BlendRTDesc[3].BlendEnable	= FALSE;
				bsDesc.BlendRTDesc[3].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
			#endif
			
			hBlendState = grcStateBlock::CreateBlendState(bsDesc);
			Assert(hBlendState != grcStateBlock::BS_Invalid);
		}
		grcStateBlock::SetBlendState(hBlendState);

		static grcDepthStencilStateHandle hDepthStencilState = grcStateBlock::DSS_Invalid;
		if(hDepthStencilState == grcStateBlock::DSS_Invalid)
		{
			grcDepthStencilStateDesc dsDesc;
			dsDesc.DepthEnable					= TRUE;
			dsDesc.DepthWriteMask				= TRUE;
			dsDesc.DepthFunc					= rage::FixupDepthDirection(grcRSV::CMP_LESS);
			
			hDepthStencilState		= grcStateBlock::CreateDepthStencilState(dsDesc);
			Assert(hDepthStencilState != grcStateBlock::DSS_Invalid);
		}
		grcStateBlock::SetDepthStencilState(hDepthStencilState);

		#if __PS3 || (RSG_PC || RSG_DURANGO || RSG_ORBIS)				// REL-1-11: im rendering supports only 1 MRT:
			GBuffer::UnlockTargets();
			GBuffer::LockSingleTarget(GBUFFER_RT_0, TRUE);
		#endif
		#if __XENON
			// 360: reset screen vp, so grcDrawLabelf() renders correctly:
			const u32 oldDeviceWidth = GRCDEVICE.GetWidth();
			const u32 oldDeviceHeight= GRCDEVICE.GetHeight();
			GRCDEVICE.SetSize(1280, 720);

			grcViewport *pScreenVP = (grcViewport*)grcViewport::GetDefaultScreen();
			grcViewport oldScreenVP = *pScreenVP;
			pScreenVP->ResetWindow();
			pScreenVP->Screen();
		#endif // __XENON...

		grcColor( Color32(255,255,255,255) );

		//
		// render all close lists:
		//
		for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)
		{
			CPlantLocTriArray& triTab = *m_LocTrisTab[listID];
			u16 loctri = triTab.m_CloseListHead;
			while(loctri)
			{
				CPlantLocTri *pLocTri = &triTab[loctri];

				Color32 colorV1 = pLocTri->m_GroundColorV1;
				Color32 colorV2 = pLocTri->m_GroundColorV2;
				Color32 colorV3 = pLocTri->m_GroundColorV3;

				float printVal[3]={0, 0, 0};
				switch(gbShowCPlantMgrGroundCSD)
				{
					case(0):	// color
						break;
					
					case(1):	// scaleXYZ
					{
						printVal[0] = CPlantLocTri::pv8UnpackAndMapScaleXYZ(colorV1.GetAlpha());
						printVal[1] = CPlantLocTri::pv8UnpackAndMapScaleXYZ(colorV2.GetAlpha());
						printVal[2] = CPlantLocTri::pv8UnpackAndMapScaleXYZ(colorV3.GetAlpha());
					}
					break;
					
					case(2):	// scaleZ
					{
						printVal[0] = CPlantLocTri::pv8UnpackAndMapScaleZ(colorV1.GetAlpha());
						printVal[1] = CPlantLocTri::pv8UnpackAndMapScaleZ(colorV2.GetAlpha());
						printVal[2] = CPlantLocTri::pv8UnpackAndMapScaleZ(colorV3.GetAlpha());
					}
					break;

					case(3):	// density
					{
						printVal[0] = CPlantLocTri::pv8UnpackAndMapDensity(colorV1.GetAlpha());
						printVal[1] = CPlantLocTri::pv8UnpackAndMapDensity(colorV2.GetAlpha());
						printVal[2] = CPlantLocTri::pv8UnpackAndMapDensity(colorV3.GetAlpha());
					}
					break;
					
					default:
						Assert(0);	// invalid state
						break;
				}

				Vector3 v0 = pLocTri->GetV1();
				Vector3 v1 = pLocTri->GetV2();
				Vector3 v2 = pLocTri->GetV3();
				
				// move towards middle of tri:
				static BankFloat bShiftVal = 0.2f;
				const Vector3 center = pLocTri->GetCenter();
				v0 += (center - v0) * bShiftVal;
				v1 += (center - v1) * bShiftVal;
				v2 += (center - v2) * bShiftVal;
				
				v0.z += 0.5f;
				v1.z += 0.5f;
				v2.z += 0.5f;

				grcDrawLabelf(v0, "%.3f", printVal[0]);
				grcDrawLabelf(v1, "%.3f", printVal[1]);
				grcDrawLabelf(v2, "%.3f", printVal[2]);

				loctri = pLocTri->m_NextTri;
			}//while(pLocTri)...
		}//for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)...

		#if __PS3 || (RSG_PC || RSG_DURANGO || RSG_ORBIS)				// REL-1-11: im rendering supports only 1 MRT:
			GBuffer::UnlockSingleTarget(GBUFFER_RT_0);
			GBuffer::LockTargets();
		#endif
		#if __XENON
			GRCDEVICE.SetSize(oldDeviceWidth, oldDeviceHeight);
			*pScreenVP = oldScreenVP;
		#endif

		grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
		grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
		grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
	}// ifif(gbShowCPlantMgrGroundColor && (gbShowCPlantMgrGroundCSD > 0))...
#endif //__BANK...


	// debug render of all proc mat type:
	if(gbShowCPlantMgrProcMatType)
	{
		grcWorldIdentity();
		grcBindTexture(NULL);

		static grcRasterizerStateHandle hRasterizerState = grcStateBlock::RS_Invalid;
		if(hRasterizerState == grcStateBlock::RS_Invalid)
		{
			grcRasterizerStateDesc rasDesc;
			rasDesc.CullMode	= grcRSV::CULL_NONE;
			rasDesc.FillMode	= grcRSV::FILL_SOLID;
			
			hRasterizerState	= grcStateBlock::CreateRasterizerState(rasDesc);
			Assert(hRasterizerState != grcStateBlock::RS_Invalid);
		}
		grcStateBlock::SetRasterizerState(hRasterizerState);

		static grcBlendStateHandle hBlendState = grcStateBlock::BS_Invalid;
		if(hBlendState == grcStateBlock::BS_Invalid)
		{
			grcBlendStateDesc bsDesc;
			bsDesc.IndependentBlendEnable		= FALSE;
			bsDesc.BlendRTDesc[0].BlendEnable	= FALSE;
			#if __XENON
				// REL-1-11: im rendering supports only 1 MRT
				bsDesc.IndependentBlendEnable		= TRUE;
				bsDesc.BlendRTDesc[0].BlendEnable	= FALSE;
				bsDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
				bsDesc.BlendRTDesc[1].BlendEnable	= FALSE;
				bsDesc.BlendRTDesc[1].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
				bsDesc.BlendRTDesc[2].BlendEnable	= FALSE;
				bsDesc.BlendRTDesc[2].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
				bsDesc.BlendRTDesc[3].BlendEnable	= FALSE;
				bsDesc.BlendRTDesc[3].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
			#endif
			
			hBlendState = grcStateBlock::CreateBlendState(bsDesc);
			Assert(hBlendState != grcStateBlock::BS_Invalid);
		}
		grcStateBlock::SetBlendState(hBlendState);

		static grcDepthStencilStateHandle hDepthStencilState = grcStateBlock::DSS_Invalid;
		if(hDepthStencilState == grcStateBlock::DSS_Invalid)
		{
			grcDepthStencilStateDesc dsDesc;
			dsDesc.DepthEnable					= TRUE;
			dsDesc.DepthWriteMask				= TRUE;
			dsDesc.DepthFunc					= rage::FixupDepthDirection(grcRSV::CMP_LESS);
			
			hDepthStencilState		= grcStateBlock::CreateDepthStencilState(dsDesc);
			Assert(hDepthStencilState != grcStateBlock::DSS_Invalid);
		}
		grcStateBlock::SetDepthStencilState(hDepthStencilState);

		#if __PS3 || (RSG_PC || RSG_DURANGO || RSG_ORBIS)				// REL-1-11: im rendering supports only 1 MRT:
			GBuffer::UnlockTargets();
			GBuffer::LockSingleTarget(GBUFFER_RT_0, TRUE);
		#endif
		#if __XENON
			// 360: reset screen vp, so grcDrawLabelf() renders correctly:
			const u32 oldDeviceWidth = GRCDEVICE.GetWidth();
			const u32 oldDeviceHeight= GRCDEVICE.GetHeight();
			GRCDEVICE.SetSize(1280, 720);

			grcViewport *pScreenVP = (grcViewport*)grcViewport::GetDefaultScreen();
			grcViewport oldScreenVP = *pScreenVP;
			pScreenVP->ResetWindow();
			pScreenVP->Screen();
		#endif // __XENON...

		grcColor( Color32(255,255,255,255) );

		for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)
		{
			CPlantLocTriArray& triTab = *m_LocTrisTab[listID];
			u16 loctri = triTab.m_CloseListHead;
			while(loctri)
			{
				CPlantLocTri *pLocTri = &triTab[loctri];
				Vector3 pTriCenter = pLocTri->GetCenter();
				pTriCenter.z += 0.5f;

				s32 procTagId = PGTAMATERIALMGR->UnpackProcId(pLocTri->m_nSurfaceType);
				s32 procInfoIdx	= g_procInfo.m_procTagTable[procTagId].procObjIndex;
				s32 plantInfoIdx= g_procInfo.m_procTagTable[procTagId].plantIndex;
				const char* procName	= procInfoIdx>-1?  g_procInfo.m_procObjInfos[procInfoIdx].m_Tag.GetCStr() : "none";
				const char* plantName	= plantInfoIdx>-1? g_procInfo.m_plantInfos[plantInfoIdx].m_Tag.GetCStr()  : "none";

				grcDrawLabelf(pTriCenter, "%s [%s / %s]", (const char*)g_procInfo.m_procTagTable[procTagId].GetName(), procName, plantName);

				loctri = pLocTri->m_NextTri;
			}//while(pLocTri)...

		}//for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)...

		#if __PS3 || (RSG_PC || RSG_DURANGO || RSG_ORBIS)				// REL-1-11: im rendering supports only 1 MRT:
			GBuffer::UnlockSingleTarget(GBUFFER_RT_0);
			GBuffer::LockTargets();
		#endif
		#if __XENON
			GRCDEVICE.SetSize(oldDeviceWidth, oldDeviceHeight);
			*pScreenVP = oldScreenVP;
		#endif

		grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
		grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
		grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
	}// if(gbShowCPlantMgrProcMatType)...

	// debug render of all proc mat type:
	if(gbShowCPlantMgrAmbScale)
	{
		grcWorldIdentity();
		grcBindTexture(NULL);

		static grcRasterizerStateHandle hRasterizerState = grcStateBlock::RS_Invalid;
		if(hRasterizerState == grcStateBlock::RS_Invalid)
		{
			grcRasterizerStateDesc rasDesc;
			rasDesc.CullMode	= grcRSV::CULL_NONE;
			rasDesc.FillMode	= grcRSV::FILL_SOLID;
			
			hRasterizerState	= grcStateBlock::CreateRasterizerState(rasDesc);
			Assert(hRasterizerState != grcStateBlock::RS_Invalid);
		}
		grcStateBlock::SetRasterizerState(hRasterizerState);

		static grcBlendStateHandle hBlendState = grcStateBlock::BS_Invalid;
		if(hBlendState == grcStateBlock::BS_Invalid)
		{
			grcBlendStateDesc bsDesc;
			bsDesc.IndependentBlendEnable		= FALSE;
			bsDesc.BlendRTDesc[0].BlendEnable	= FALSE;
			#if __XENON
				// REL-1-11: im rendering supports only 1 MRT
				bsDesc.IndependentBlendEnable		= TRUE;
				bsDesc.BlendRTDesc[0].BlendEnable	= FALSE;
				bsDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
				bsDesc.BlendRTDesc[1].BlendEnable	= FALSE;
				bsDesc.BlendRTDesc[1].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
				bsDesc.BlendRTDesc[2].BlendEnable	= FALSE;
				bsDesc.BlendRTDesc[2].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
				bsDesc.BlendRTDesc[3].BlendEnable	= FALSE;
				bsDesc.BlendRTDesc[3].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
			#endif
			
			hBlendState = grcStateBlock::CreateBlendState(bsDesc);
			Assert(hBlendState != grcStateBlock::BS_Invalid);
		}
		grcStateBlock::SetBlendState(hBlendState);

		static grcDepthStencilStateHandle hDepthStencilState = grcStateBlock::DSS_Invalid;
		if(hDepthStencilState == grcStateBlock::DSS_Invalid)
		{
			grcDepthStencilStateDesc dsDesc;
			dsDesc.DepthEnable					= TRUE;
			dsDesc.DepthWriteMask				= TRUE;
			dsDesc.DepthFunc					= rage::FixupDepthDirection(grcRSV::CMP_LESS);
			
			hDepthStencilState		= grcStateBlock::CreateDepthStencilState(dsDesc);
			Assert(hDepthStencilState != grcStateBlock::DSS_Invalid);
		}
		grcStateBlock::SetDepthStencilState(hDepthStencilState);

		#if __PS3 || (RSG_PC || RSG_DURANGO || RSG_ORBIS)				// REL-1-11: im rendering supports only 1 MRT:
			GBuffer::UnlockTargets();
			GBuffer::LockSingleTarget(GBUFFER_RT_0, TRUE);
		#endif
		#if __XENON
			// 360: reset screen vp, so grcDrawLabelf() renders correctly:
			const u32 oldDeviceWidth = GRCDEVICE.GetWidth();
			const u32 oldDeviceHeight= GRCDEVICE.GetHeight();
			GRCDEVICE.SetSize(1280, 720);

			grcViewport *pScreenVP = (grcViewport*)grcViewport::GetDefaultScreen();
			grcViewport oldScreenVP = *pScreenVP;
			pScreenVP->ResetWindow();
			pScreenVP->Screen();
		#endif // __XENON...

		grcColor( Color32(255,255,255,255) );

		for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)
		{
			CPlantLocTriArray& triTab = *m_LocTrisTab[listID];
			u16 loctri = triTab.m_CloseListHead;
			while(loctri)
			{
				CPlantLocTri *pLocTri = &triTab[loctri];
				Vector3 pTriCenter = pLocTri->GetCenter();
				pTriCenter.z += 0.5f;

				grcDrawLabelf(pTriCenter, "%d:%d", pLocTri->m_nAmbientScale[0],pLocTri->m_nAmbientScale[1]);

				loctri = pLocTri->m_NextTri;
			}//while(pLocTri)...

		}//for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)...

		#if __PS3 || (RSG_PC || RSG_DURANGO || RSG_ORBIS)				// REL-1-11: im rendering supports only 1 MRT:
			GBuffer::UnlockSingleTarget(GBUFFER_RT_0);
			GBuffer::LockTargets();
		#endif
		#if __XENON
			GRCDEVICE.SetSize(oldDeviceWidth, oldDeviceHeight);
			*pScreenVP = oldScreenVP;
		#endif

		grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
		grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
		grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
	}// if(gbShowCPlantMgrAmbScale)...

	return(TRUE);

}// end of RenderDebugStuff()...


//
//
//
// This debug render function render during debug render phase near end of frame
//
bool CPlantMgr::RenderDebugStuffForward()
{
#if CPLANT_DYNAMIC_CULL_SPHERES
	if(gbPlantMgrDCSDebugDraw)
	{
		const u32 id = gPlantMgr.GetRenderRTBufferID();
		const CPlantMgr::DynCullSphereArray &cullSpheres = gPlantMgr.GetCullSphereArray(id);

		CPlantMgr::DynCullSphereArray::const_iterator iter;
		CPlantMgr::DynCullSphereArray::const_iterator end = cullSpheres.end();
		for(iter = cullSpheres.begin(); iter != end; ++iter)
			grcDebugDraw::Sphere(iter->GetCenter(), iter->GetRadiusf(), gPlantMgrDCSDebugDrawColor, gbPlantMgrDCSDebugDrawSolid);
	}// if(gbPlantMgrDCSDebugDraw)...
#endif //CPLANT_DYNAMIC_CULL_SPHERES...

	return(TRUE);
}

bool CPlantMgr::RenderDebugForward()
{
	return gPlantMgr.RenderDebugStuffForward();
}
#endif //__BANK...



//
//
//
//
bool CPlantMgr::DbgPrintCPlantMgrInfo(u32 BANK_ONLY(nNumLocTrisDrawn), u32 BANK_ONLY(nNumLocTrisTexSkipped), u32 BANK_ONLY(nNumPlantsLOD0Drawn),  u32 BANK_ONLY(nNumPlantsLOD1Drawn), u32 BANK_ONLY(nNumPlantsLOD2Drawn))
{
#if __BANK
/////////////////////////////////////////////////////////////////////////////////////////////////////////

	// print some debug/stats info:
	if(gbDisplayCPlantMgrInfo)
	{
		u32 nNumCachedBounds=0, nNumCachedCollBounds=0, nNumCachedMatBounds=0;
		DbgCountCachedBounds(&nNumCachedBounds, &nNumCachedCollBounds, &nNumCachedMatBounds);

		u32 nNumPlantLocTris=0, nNumPlants=0;
		DbgCountLocTrisAndPlants(&nNumPlantLocTris, &nNumPlants, true, false, false);
		u32 nNumObjLocTris=0;
		DbgCountLocTrisAndPlants(&nNumObjLocTris, NULL, false, true, false);
		u32 nNumFreeLocTris=0;
		DbgCountLocTrisAndPlants(&nNumFreeLocTris, NULL, false, false, true);

		char buf[256];
		const s32 StartX = 8;
		const s32 StartY = 26;

		const bool bPrevDisplayDebugText = grcDebugDraw::GetDisplayDebugText();
		grcDebugDraw::SetDisplayDebugText(TRUE);

		::sprintf(buf, "CPlantMgr::Info:");
		grcDebugDraw::PrintToScreenCoors(buf, StartX+0, StartY+0);
		::sprintf(buf, "----------------");
		grcDebugDraw::PrintToScreenCoors(buf, StartX+0, StartY+1);

		::sprintf(buf, "Bounds: All=%d (collision: %d, material: %d)", nNumCachedBounds, nNumCachedCollBounds, nNumCachedMatBounds);
		grcDebugDraw::PrintToScreenCoors(buf, StartX+0, StartY+2);

		::sprintf(buf, "LocTris[0]: All=%d (drawn=%d, txskipped=%d)", nNumPlantLocTris, nNumLocTrisDrawn, nNumLocTrisTexSkipped);
		grcDebugDraw::PrintToScreenCoors(buf, StartX+0, StartY+3);
		::sprintf(buf, "LocTris[0]: Plants=%d (LOD0: dr=%d (%.2f%%), LOD1: dr=%d (%.2f%%), LOD2: dr=%d (%.2f%%))", nNumPlants, nNumPlantsLOD0Drawn, 100.0f*float(nNumPlantsLOD0Drawn)/float(nNumPlants+1), nNumPlantsLOD1Drawn, 100.0f*float(nNumPlantsLOD1Drawn)/float(nNumPlants+1), nNumPlantsLOD2Drawn, 100.0f*float(nNumPlantsLOD2Drawn)/float(nNumPlants+1));
		grcDebugDraw::PrintToScreenCoors(buf, StartX+0, StartY+4);
		::sprintf(buf, "LocTris[1]: All=%d", nNumObjLocTris);
		grcDebugDraw::PrintToScreenCoors(buf, StartX+0, StartY+5);
		::sprintf(buf, "LocTris free: All=%d", nNumFreeLocTris);
		grcDebugDraw::PrintToScreenCoors(buf, StartX+0, StartY+6);
	#if LOCTRISTAB_STR
		::sprintf(buf, "LocTris str allocated: %d/12",	m_LocTrisTab[0]? 12 : 0);
		grcDebugDraw::PrintToScreenCoors(buf, StartX+0, StartY+7);
	#endif
		
	#if CPLANT_USE_OCCLUSION
		if(gbPlantsUseOcclusion)
		{
			::sprintf(buf, "LocTris occluded: %d", nLocTrisOccluded);
			grcDebugDraw::PrintToScreenCoors(buf, StartX+0, StartY+8);
		}
	#endif

		#if FURGRASS_TEST_V4
			::sprintf(buf, "Fur grass polys: %d", gnFurGrassNumPolys);
			grcDebugDraw::PrintToScreenCoors(buf, StartX+0, StartY+10);
		#endif
		::sprintf(buf, "TriLocFarDist: %.1fm", rage::Sqrtf(CPLANT_TRILOC_FAR_DIST_SQR));
		grcDebugDraw::PrintToScreenCoors(buf, StartX+0, StartY+11);
		::sprintf(buf, "AlphaClose/FarDist: %.1fm/%.1fm", CPLANT_LOD0_ALPHA_CLOSE_DIST, CPLANT_LOD0_ALPHA_FAR_DIST);
		grcDebugDraw::PrintToScreenCoors(buf, StartX+0, StartY+12);

		::sprintf(buf, "Darkness amount: %.3f", gbPlantsDarknessAmountPrint);
		grcDebugDraw::PrintToScreenCoors(buf, StartX+0, StartY+13);

	#if PSN_PLANTSMGR_SPU_RENDER
		::sprintf(buf, "SPU jobs: %d (input buffer: used: %dKB/allocated: %dKB).", CGrassRenderer::SpuGetAmountOfJobs(), CGrassRenderer::SpuGetAmountOfJobs()*PLANTSMGR_TRIPLANT_BLOCK_SIZE/1024, CGrassRenderer::SpuGetAmountOfAllocatedJobs()*PLANTSMGR_TRIPLANT_BLOCK_SIZE/1024);
		grcDebugDraw::PrintToScreenCoors(buf, StartX+0, StartY+14);

		const u32 bigHeapSize			= CGrassRenderer::SpuGetBigHeapAllocatedSize()*PLANTSMGR_LOCAL_HEAP_SIZE;
		const u32 bigHeapConsumed		= CGrassRenderer::SpuGetBigHeapConsumed()*PLANTSMGR_LOCAL_HEAP_SIZE;
		const u32 bigHeapBiggestConsumed= CGrassRenderer::SpuGetBigHeapBiggestConsumed()*PLANTSMGR_LOCAL_HEAP_SIZE;
		const u32 bigHeapOverfilled		= CGrassRenderer::SpuGetBigHeapOverfilled()*PLANTSMGR_LOCAL_HEAP_SIZE;
		::sprintf(buf, "SPU BigBuffer: used: %dKB/allocated: %dKB (%.2f%%), biggest used: %dKB, overfilled: %dKB", bigHeapConsumed/1024, bigHeapSize/1024, float(bigHeapConsumed)/float(bigHeapSize)*100.0f, bigHeapBiggestConsumed/1024, bigHeapOverfilled/1024);
		grcDebugDraw::PrintToScreenCoors(buf, StartX+0, StartY+13);
	#endif

	#if PLANTS_TXD_STR
		::sprintf(buf, "Current Zone: %s, Txd: %d",	CPopCycle::sm_nCurrentZoneName.TryGetCStr(), CPopCycle::GetCurrentZonePlantsMgrTxdIdx());
		grcDebugDraw::PrintToScreenCoors(buf, StartX+0, StartY+16);
	#endif

		grcDebugDraw::SetDisplayDebugText(bPrevDisplayDebugText);
	}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif //__BANK...

	return(TRUE);
}




//
//
//
//
bool CPlantMgr::DbgCountCachedBounds(u32* BANK_ONLY(pCountAll), u32* BANK_ONLY(pCountColl), u32* BANK_ONLY(pCountMat))
{
#if __BANK
u32 nCountEnt = 0;
u32 nCountColl= 0;	// (old) collision type bounds
u32 nCountMat = 0;	// (new) material type bounds

	u16 entry = m_ColEntCache.m_CloseListHead;
	while(entry)
	{
		CPlantColBoundEntry *pEntry = &m_ColEntCache[entry];
		if(pEntry->GetBound())
		{
			nCountEnt++;
			
			if(pEntry->IsMatBound())
			{
				nCountMat++;
			}
			else
			{
				nCountColl++;
			}
		}

		entry = pEntry->m_NextEntry;
	}
	
	if(pCountAll)
	{
		*pCountAll = nCountEnt;
	}

	if(pCountColl)
	{
		*pCountColl = nCountColl;
	}

	if(pCountMat)
	{
		*pCountMat = nCountMat;
	}
#endif//__BANK
	return(TRUE);
}// end of CPlantMgr::DbgCountCachedEntities()...


//
//
//
//
bool CPlantMgr::DbgCountLocTrisAndPlants(u32* BANK_ONLY(pCountLocTris) , u32* BANK_ONLY(pCountPlants), bool BANK_ONLY(bCountCreatesPlants), bool BANK_ONLY(bCountCreatesObjects), bool BANK_ONLY(bCountFree))
{
#if __BANK
	Assert(bCountCreatesPlants || bCountCreatesObjects || bCountFree);	// at least one count option must be selected

u32	nCountTris		= 0,
	nCountPlants	= 0;


const u32 bufferID = GetUpdateRTBufferID()&0x01;	// read previous buffer for stats; __BANK: make its range to be safe (as UpdateThread sometimes collides here with SwapRTBuffer())

#if LOCTRISTAB_STR
	if(m_LocTrisRenderTab[bufferID][0])
#endif
	for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)
	{
		CPlantLocTriArray& triRenderTab = *m_LocTrisRenderTab[bufferID][listID];
		u16 locTri = triRenderTab.m_CloseListHead;
		while(locTri)
		{
			CPlantLocTri *pLocTri = &triRenderTab[locTri];

		#if CPLANT_USE_OCCLUSION
			if( !(gbPlantsUseOcclusion && pLocTri->m_bOccluded) )
		#endif
			{
				if(	(pLocTri->m_bCreatesPlants	==bCountCreatesPlants)	||
					(pLocTri->m_bCreatesObjects	==bCountCreatesObjects)	)
				{
					nCountTris++;
					nCountPlants += pLocTri->m_nMaxNumPlants;
				}
			}

			locTri = pLocTri->m_NextTri;
		}//while(pLocTri)...
	}

	if(bCountFree)
	{
		nCountTris = 0;
		#if LOCTRISTAB_STR
			if(m_LocTrisRenderTab[bufferID][0])
		#endif
			for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)
			{
				CPlantLocTriArray& triRenderTab = *m_LocTrisRenderTab[bufferID][listID];
				u16 locTri = triRenderTab.m_UnusedListHead;
				while(locTri)
				{
					CPlantLocTri *pLocTri = &triRenderTab[locTri];
					nCountTris++;
					
					locTri = pLocTri->m_NextTri;
				}//while(pLocTri)...
			}
	}// if(bCountFree)...

	if(pCountLocTris)
	{
		*pCountLocTris = nCountTris;
	}
	
	if(pCountPlants)
	{
		*pCountPlants = nCountPlants;
	}
#endif// __BANK...

	return(TRUE);
}// end of CPlantMgr::DbgCountLocTrix()...




//
//
//
//
void CPlantMgr::DbgDrawTriangleArrays(atArray<Vector3>& BANK_ONLY(triangleVertices) , atArray<Color32>& BANK_ONLY(triangleColors) )
{
#if __BANK

//const	u32 nNumVerts= triangleVertices.size();
	const	u32 nNumTris	= triangleVertices.GetCount() / 3;
	Assert(triangleVertices.GetCount() == triangleColors.GetCount());

#define Z_OFFSET		(0.25f)

	s32 numTrisLeftToDraw = nNumTris;
	s32 actualVertIndex = 0;

	while(numTrisLeftToDraw > 0)
	{
		s32 trisToDraw = numTrisLeftToDraw;

		if((numTrisLeftToDraw*3) > grcBeginMax)
		{
			trisToDraw			=  grcBeginMax/3;
			numTrisLeftToDraw	-= grcBeginMax/3;
		}
		else
		{
			trisToDraw			 = numTrisLeftToDraw;
			numTrisLeftToDraw	-= trisToDraw;
		}
		
		grcNormal3f(0.0f, 0.0f, 1.0f);
		grcTexCoord2f(0.0f, 0.0f);

		grcBegin(drawTris, trisToDraw*3);
		for(s32 v=0; v<trisToDraw; v++)
		{
			Color32	*pCol0	= &triangleColors	[actualVertIndex];
			Vector3 *pVert0 = &triangleVertices	[actualVertIndex];
			actualVertIndex++;

			Color32	*pCol1	= &triangleColors	[actualVertIndex];
			Vector3 *pVert1 = &triangleVertices	[actualVertIndex];
			actualVertIndex++;
			
			Color32	*pCol2	= &triangleColors	[actualVertIndex];
			Vector3 *pVert2 = &triangleVertices	[actualVertIndex];
			actualVertIndex++;

			//grcTexCoord2f(0.0f, 0.0f);
			grcColor(*pCol0);
			grcVertex3f(pVert0->x, pVert0->y, pVert0->z + Z_OFFSET);
			//grcTexCoord2f(1.0f, 0.0f);
			grcColor(*pCol1);
			grcVertex3f(pVert1->x, pVert1->y, pVert1->z + Z_OFFSET);
			//grcTexCoord2f(0.0f, 1.0f);
			grcColor(*pCol2);
			grcVertex3f(pVert2->x, pVert2->y, pVert2->z + Z_OFFSET);
		}
		grcEnd();
	}
#endif //__BANK...

#if PLANTSMGR_DATA_EDITOR
	gPlantMgrEditor.Render();
#endif
} // end of DbgDrawTriangleArrays()...

//
//
//
//
void CPlantMgr::DbgDrawLineArrays(atArray<Vector3>& BANK_ONLY(lineVertices) , atArray<Color32>& BANK_ONLY(lineColors) )
{
#if __BANK

	const	u32 nNumLines	= lineVertices.GetCount() / 2;
	Assert(lineVertices.GetCount() == lineColors.GetCount());

#define Z_OFFSET		(0.25f)

	s32 numLinesLeftToDraw = nNumLines;
	s32 actualVertIndex = 0;

	while(numLinesLeftToDraw > 0)
	{
		s32 linesToDraw = numLinesLeftToDraw;

		if((numLinesLeftToDraw*2) > grcBeginMax)
		{
			linesToDraw			=  grcBeginMax/2;
			numLinesLeftToDraw	-= grcBeginMax/2;
		}
		else
		{
			linesToDraw			 = numLinesLeftToDraw;
			numLinesLeftToDraw	-= linesToDraw;
		}
		
		grcNormal3f(0.0f, 0.0f, 1.0f);
		grcTexCoord2f(0.0f, 0.0f);

		grcBegin(drawLines, linesToDraw*2);
		for(s32 v=0; v<linesToDraw; v++)
		{
			Color32	*pCol0	= &lineColors	[actualVertIndex];
			Vector3 *pVert0 = &lineVertices	[actualVertIndex];
			actualVertIndex++;

			Color32	*pCol1	= &lineColors	[actualVertIndex];
			Vector3 *pVert1 = &lineVertices	[actualVertIndex];
			actualVertIndex++;
			
			//grcTexCoord2f(0.0f, 0.0f);
			grcColor(*pCol0);
			grcVertex3f(pVert0->x, pVert0->y, pVert0->z + Z_OFFSET);
			//grcTexCoord2f(1.0f, 0.0f);
			grcColor(*pCol1);
			grcVertex3f(pVert1->x, pVert1->y, pVert1->z + Z_OFFSET);
		}
		grcEnd();
	}

#endif //__BANK...
} // end of DbgDrawTriangleArrays()...


#if SECTOR_TOOLS_EXPORT
void CPlantMgr::DbgGetCPlantMgrInfo( u32* nNumLocTrisDrawn, u32* nNumPlantsDrawn )
{
	*nNumLocTrisDrawn = 0;
	*nNumPlantsDrawn = 0;

u32 nNumCachedBounds=0;
	DbgCountCachedBounds(&nNumCachedBounds, NULL, NULL);

u32 nNumLocTris=0, nNumPlants=0;
	DbgCountLocTrisAndPlants(&nNumLocTris, &nNumPlants, true, true, false);

	*nNumLocTrisDrawn	+= nNumLocTris;
	*nNumPlantsDrawn	+= nNumPlants;
}

//
// name: DbgStatsCountCachedBounds
// desc:
//
void CPlantMgr::DbgStatsCountCachedBounds( u32* pCountAll )
{
u32 nCountEnt=0;

	u16 entry = m_ColEntCache.m_CloseListHead;
	while(entry)
	{
		CPlantColBoundEntry *pEntry = &m_ColEntCache[entry];
		Assert(pEntry->GetBound());

		nCountEnt++;

		entry = pEntry->m_NextEntry;
	}

	if(pCountAll)
	{
		*pCountAll = nCountEnt;
	}
}// end of CPlantMgr::DbgCountCachedEntities()...


//
// name: DbgStatsCountLocTrisAndPlants
// desc:
//
void CPlantMgr::DbgStatsCountLocTrisAndPlants( u32* pCountLocTris, u32* pCountPlants )
{
s32 nCountTris = 0;
s32 nCountPlants = 0;

	for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)
	{
		CPlantLocTriArray& triTab = *m_LocTrisTab[listID];
		u16 locTri = triTab.m_CloseListHead;
		while(locTri)
		{
			CPlantLocTri *pLocTri = &triTab[locTri];
			nCountTris++;
			nCountPlants += pLocTri->m_nMaxNumPlants;
			locTri = pLocTri->m_NextTri;
		}
	}

	if(pCountLocTris)
	{
		*pCountLocTris = nCountTris;
	}

	if(pCountPlants)
	{
		*pCountPlants = nCountPlants;
	}
}
#endif // SECTOR_TOOLS_EXPORT


//////////////////////////////////////////////////////////////////////////////////////////////
#if FURGRASS_TEST_V4
static grmShader			*gFurGrassLODShader=NULL;
static grcEffectTechnique	gFurGrassLODComboTechniqueID=grcetNONE;
static grcEffectTechnique	gFurGrassDitheredTechniqueID=grcetNONE;

static grcEffectVar			varID_FurFresnel=grcevNONE;
static grcEffectVar			varID_FurSpecFalloff=grcevNONE;
static grcEffectVar			varID_FurSpecInt=grcevNONE;
static grcEffectVar			varID_FurBumpiness=grcevNONE;
static grcEffectVar			varID_FurLODDiffuseTex=grcevNONE, varID_FurLODComboHeightTex=grcevNONE, varID_FurLODNormalTex=grcevNONE, varID_FurLODStippleTex=grcevNONE;
static grcEffectVar			varID_FurLODComboHeightTexMask=grcevNONE;
static grcEffectVar			varID_FurDitherAlphaFadeParams=grcevNONE;
static grcEffectVar			varID_FurLayerParams=grcevNONE;
static grcEffectVar			varID_FurLayerParams2=grcevNONE;
static grcEffectVar			varID_FurLayerParams3=grcevNONE;
static grcEffectVar			varID_FurPlayerLFeetPos=grcevNONE;
static grcEffectVar			varID_FurPlayerRFeetPos=grcevNONE;

static grcTexture*			gFurGrassLODComboDiffTex=NULL;
//static grcTexture*		gFurGrassLODComboH2Tex[2]={NULL};
//static Color32			gFurGrassLODComboH2TexMask[8] = {
//							Color32( Vector4(1,0,0,0) ),
//							Color32( Vector4(0,1,0,0) ),
//							Color32( Vector4(0,0,1,0) ),
//							Color32( Vector4(0,0,0,1) ),
//							Color32( Vector4(1,0,0,0) ),
//							Color32( Vector4(0,1,0,0) ),
//							Color32( Vector4(0,0,1,0) ),
//							Color32( Vector4(0,0,0,1) )
//							};
static grcTexture*			gFurGrassLODComboH4Tex[4]={NULL};
static Color32				gFurGrassLODComboH4TexMask[8] = {
							Color32( Vector4(0,1,0,0) ),
							Color32( Vector4(0,0,0,1) ),
							Color32( Vector4(0,1,0,0) ),
							Color32( Vector4(0,0,0,1) ),
							Color32( Vector4(0,1,0,0) ),
							Color32( Vector4(0,0,0,1) ),
							Color32( Vector4(0,1,0,0) ),
							Color32( Vector4(0,0,0,1) )
							};
static grcTexture*			gFurGrassLODTexN=NULL;
static grcTexture*			gFurGrassLODStippleTex=NULL;

static grcRasterizerStateHandle		hFurGrassRS_pass1		= grcStateBlock::RS_Invalid;
static grcBlendStateHandle			hFurGrassBS_pass1		= grcStateBlock::BS_Invalid;
static grcDepthStencilStateHandle	hFurGrassDSS_pass1		= grcStateBlock::DSS_Invalid;
static grcBlendStateHandle			hFurGrassBS_pass1_end	= grcStateBlock::BS_Invalid;

static grcDepthStencilStateHandle	hFurGrassDSS_pass2a		= grcStateBlock::DSS_Invalid;
static grcDepthStencilStateHandle	hFurGrassDSS_pass2b		= grcStateBlock::DSS_Invalid;
static grcDepthStencilStateHandle	hFurGrassDSS_pass2		= grcStateBlock::DSS_Invalid;

static grcBlendStateHandle			hFurGrassBS_pass3		= grcStateBlock::BS_Invalid;
static grcDepthStencilStateHandle	hFurGrassDSS_pass3		= grcStateBlock::DSS_Invalid;
static grcBlendStateHandle			hFurGrassBS_pass3_end	= grcStateBlock::BS_Invalid;

//
//
//
//
static grcTexture* LoadFurGrassLODTexture(const char *texname)
{
	grcTexture *pTexture = gpPlantMgrTextureDictionary->Lookup(texname);
	Assertf(pTexture, "Cannot find fur grass texture '%s'!", texname);
	pTexture->AddRef();
	return(pTexture);
}

static grmShader* LoadGrassFurLODShader(const char *shaderName)
{
grmShader *pShader=NULL;

	pShader = grmShaderFactory::GetInstance().Create();
	Assert(pShader);

	if(!pShader->Load(shaderName))
	{
		Assertf(FALSE, "Error loading '%s' shader!", shaderName);
		return(NULL);
	}

	return(pShader);
}

bool CPlantMgr::FurGrassLODInitialise()
{
	// load fur grass shader and dics:
	gFurGrassLODShader = LoadGrassFurLODShader("grass_fur_lod");
	Assert(gFurGrassLODShader);
	gFurGrassLODComboTechniqueID = gFurGrassLODShader->LookupTechnique("deferred_furgrasstriCombo", TRUE);
	Assert(gFurGrassLODComboTechniqueID);
	gFurGrassDitheredTechniqueID = gFurGrassLODShader->LookupTechnique("furgrasstri_dithered", TRUE);
	Assert(gFurGrassDitheredTechniqueID);

	varID_FurFresnel = gFurGrassLODShader->LookupVar("Fresnel", TRUE);
	Assert(varID_FurFresnel);
	varID_FurSpecFalloff = gFurGrassLODShader->LookupVar("Specular", TRUE);
	Assert(varID_FurSpecFalloff);
	varID_FurSpecInt = gFurGrassLODShader->LookupVar("SpecularColor", TRUE);
	Assert(varID_FurSpecInt);
	varID_FurBumpiness = gFurGrassLODShader->LookupVar("Bumpiness", TRUE);
	Assert(varID_FurBumpiness);

	varID_FurLODDiffuseTex = gFurGrassLODShader->LookupVar("DiffuseTexFur", TRUE);
	Assert(varID_FurLODDiffuseTex);
	varID_FurLODComboHeightTex = gFurGrassLODShader->LookupVar("ComboHeightTexFur", TRUE);
	Assert(varID_FurLODComboHeightTex);
	varID_FurLODComboHeightTexMask = gFurGrassLODShader->LookupVar("ComboHeightTexMask", TRUE);
	Assert(varID_FurLODComboHeightTexMask);

	varID_FurLODNormalTex = gFurGrassLODShader->LookupVar("BumpTexFur");	// this texture is optional

#if FURGRASS_USE_ALPHA_STIPPLE
	varID_FurLODStippleTex = gFurGrassLODShader->LookupVar("StippleTexture", TRUE);
	Assert(varID_FurLODStippleTex);
#endif
	varID_FurDitherAlphaFadeParams = gFurGrassLODShader->LookupVar("furDitherAlphaFadeParams",TRUE);
	Assert(varID_FurDitherAlphaFadeParams);
	varID_FurLayerParams = gFurGrassLODShader->LookupVar("furLayerParams",TRUE);
	Assert(varID_FurLayerParams);
	varID_FurLayerParams2 = gFurGrassLODShader->LookupVar("furLayerParams2",TRUE);
	Assert(varID_FurLayerParams2);
	varID_FurLayerParams3 = gFurGrassLODShader->LookupVar("furLayerParams3",TRUE);
	Assert(varID_FurLayerParams3);
	varID_FurPlayerLFeetPos = gFurGrassLODShader->LookupVar("playerLFootPos",TRUE);
	Assert(varID_FurPlayerLFeetPos);
	varID_FurPlayerRFeetPos = gFurGrassLODShader->LookupVar("playerRFootPos",TRUE);
	Assert(varID_FurPlayerRFeetPos);

	gFurGrassLODComboDiffTex = LoadFurGrassLODTexture("fur_grass_d_0");
	Assert(gFurGrassLODComboDiffTex);

//	gFurGrassLODComboH2Tex[0] = LoadFurGrassLODTexture("fur_grass_rgba2_0");
//	Assert(gFurGrassLODComboH2Tex[0]);
//	gFurGrassLODComboH2Tex[1] = LoadFurGrassLODTexture("fur_grass_rgba2_1");
//	Assert(gFurGrassLODComboH2Tex[1]);

	gFurGrassLODComboH4Tex[0] = LoadFurGrassLODTexture("fur_grass_rgba4_0");
	Assert(gFurGrassLODComboH4Tex[0]);
	gFurGrassLODComboH4Tex[1] = LoadFurGrassLODTexture("fur_grass_rgba4_1");
	Assert(gFurGrassLODComboH4Tex[1]);
	gFurGrassLODComboH4Tex[2] = LoadFurGrassLODTexture("fur_grass_rgba4_2");
	Assert(gFurGrassLODComboH4Tex[2]);
	gFurGrassLODComboH4Tex[3] = LoadFurGrassLODTexture("fur_grass_rgba4_3");
	Assert(gFurGrassLODComboH4Tex[3]);

	gFurGrassLODTexN = LoadFurGrassLODTexture("fur_grass_n");
	Assert(gFurGrassLODTexN);
#if FURGRASS_USE_ALPHA_STIPPLE
	gFurGrassLODStippleTex = LoadFurGrassLODTexture("txstipple");
	Assert(gFurGrassLODStippleTex);
#endif

	// initialize renderstate blocks:
const u32	rsCullMode		= grcRSV::CULL_BACK;
static dev_bool bDepthWrite	= true;
const bool	rsDepthWrite	= bDepthWrite;
const bool	rsDepthTest		= TRUE;

	// pass1:
	if(hFurGrassRS_pass1 == grcStateBlock::RS_Invalid)
	{
		grcRasterizerStateDesc rasDesc;
		rasDesc.CullMode	= rsCullMode;
		
		hFurGrassRS_pass1	= grcStateBlock::CreateRasterizerState(rasDesc);
		Assert(hFurGrassRS_pass1 != grcStateBlock::RS_Invalid);
	}

	if(hFurGrassBS_pass1 == grcStateBlock::BS_Invalid)
	{
	static dev_bool debugEnableAlphaToMask1		= true;
	static dev_bool debugUseSolidAlphaToMask1	= false;
		grcBlendStateDesc bsDesc;
		bsDesc.AlphaToCoverageEnable		= debugEnableAlphaToMask1;
		bsDesc.AlphaToMaskOffsets			= debugUseSolidAlphaToMask1 ? grcRSV::ALPHATOMASKOFFSETS_SOLID : grcRSV::ALPHATOMASKOFFSETS_DITHERED;

		bsDesc.IndependentBlendEnable		= FALSE;
		bsDesc.BlendRTDesc[0].BlendEnable	= FALSE;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
		
		hFurGrassBS_pass1 = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hFurGrassBS_pass1 != grcStateBlock::BS_Invalid);
	}

	if(hFurGrassDSS_pass1 == grcStateBlock::DSS_Invalid)
	{
		grcDepthStencilStateDesc dsDesc;
		dsDesc.DepthEnable					= rsDepthTest;
		dsDesc.DepthWriteMask				= rsDepthWrite;
		dsDesc.DepthFunc					= rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);

		// stencil state:
		dsDesc.StencilEnable				= TRUE;
		dsDesc.StencilReadMask				= 0xff;
		dsDesc.StencilWriteMask				= 0x10;			// write only to 5th bit;
		dsDesc.FrontFace.StencilFunc		= 
		dsDesc.BackFace.StencilFunc			= grcRSV::CMP_ALWAYS;
		dsDesc.FrontFace.StencilPassOp		= 
		dsDesc.BackFace.StencilPassOp		= grcRSV::STENCILOP_REPLACE;
		dsDesc.FrontFace.StencilFailOp		= 
		dsDesc.BackFace.StencilFailOp		= grcRSV::STENCILOP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp	= 
		dsDesc.BackFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;

		hFurGrassDSS_pass1		= grcStateBlock::CreateDepthStencilState(dsDesc);
		Assert(hFurGrassDSS_pass1 != grcStateBlock::DSS_Invalid);
	}

	if(hFurGrassBS_pass1_end == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.AlphaToCoverageEnable		= FALSE;

		bsDesc.IndependentBlendEnable		= TRUE;
		bsDesc.BlendRTDesc[0].BlendEnable	= FALSE;
		// SSA: don't overwrite GBuffer0.alpha
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RED | grcRSV::COLORWRITEENABLE_GREEN | grcRSV::COLORWRITEENABLE_BLUE;
		bsDesc.BlendRTDesc[1].BlendEnable	= FALSE;
		bsDesc.BlendRTDesc[1].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
		bsDesc.BlendRTDesc[2].BlendEnable	= FALSE;
		bsDesc.BlendRTDesc[2].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
		bsDesc.BlendRTDesc[3].BlendEnable	= FALSE;
		bsDesc.BlendRTDesc[3].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
		
		hFurGrassBS_pass1_end = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hFurGrassBS_pass1_end != grcStateBlock::BS_Invalid);
	}

	// pass2:
	if(hFurGrassDSS_pass2a == grcStateBlock::DSS_Invalid)
	{
		grcDepthStencilStateDesc dsDesc;
		dsDesc.DepthEnable					= rsDepthTest;
		dsDesc.DepthWriteMask				= false;
		dsDesc.DepthFunc					= rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		
		// stencil state:
		dsDesc.StencilEnable				= TRUE;
		dsDesc.StencilReadMask				= 0x10;						// read 5th bit
		dsDesc.StencilWriteMask				= 0x00;						// do not touch stencil
		dsDesc.FrontFace.StencilFunc		=
		dsDesc.BackFace.StencilFunc			= grcRSV::CMP_EQUAL;
		dsDesc.FrontFace.StencilPassOp		= 
		dsDesc.BackFace.StencilPassOp		= grcRSV::STENCILOP_ZERO;	// reset special bit
		dsDesc.FrontFace.StencilFailOp		=
		dsDesc.BackFace.StencilFailOp		= grcRSV::STENCILOP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp	=
		dsDesc.BackFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;

		hFurGrassDSS_pass2a		= grcStateBlock::CreateDepthStencilState(dsDesc);
		Assert(hFurGrassDSS_pass2a != grcStateBlock::DSS_Invalid);
	}
	
	if(hFurGrassDSS_pass2b == grcStateBlock::DSS_Invalid)
	{
		grcDepthStencilStateDesc dsDesc;
		dsDesc.DepthEnable					= rsDepthTest;
		dsDesc.DepthWriteMask				= false;
		dsDesc.DepthFunc					= rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		
		// stencil state:
		dsDesc.StencilEnable				= TRUE;
		dsDesc.StencilReadMask				= 0x10;						// read 5th bit
		dsDesc.StencilWriteMask				= 0x10;						// write only to 5th bit;
		dsDesc.FrontFace.StencilFunc		= 
		dsDesc.BackFace.StencilFunc			= grcRSV::CMP_EQUAL;
		dsDesc.FrontFace.StencilPassOp		= 
		dsDesc.BackFace.StencilPassOp		= grcRSV::STENCILOP_ZERO;	// reset special bit
		dsDesc.FrontFace.StencilFailOp		= 
		dsDesc.BackFace.StencilFailOp		= grcRSV::STENCILOP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp	= 
		dsDesc.BackFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;

		hFurGrassDSS_pass2b		= grcStateBlock::CreateDepthStencilState(dsDesc);
		Assert(hFurGrassDSS_pass2b != grcStateBlock::DSS_Invalid);
	}

	if(hFurGrassDSS_pass2 == grcStateBlock::DSS_Invalid)
	{
		grcDepthStencilStateDesc dsDesc;
		dsDesc.DepthEnable					= rsDepthTest;
		dsDesc.DepthWriteMask				= false;//rsDepthWrite;
		dsDesc.DepthFunc					= rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		
		// stencil state:
		dsDesc.StencilEnable				= FALSE;
		dsDesc.FrontFace.StencilFunc		= 
		dsDesc.BackFace.StencilFunc			= grcRSV::CMP_ALWAYS;
		dsDesc.FrontFace.StencilPassOp		= 
		dsDesc.BackFace.StencilPassOp		= grcRSV::STENCILOP_KEEP;
		dsDesc.FrontFace.StencilFailOp		= 
		dsDesc.BackFace.StencilFailOp		= grcRSV::STENCILOP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp	= 
		dsDesc.BackFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;

		hFurGrassDSS_pass2		= grcStateBlock::CreateDepthStencilState(dsDesc);
		Assert(hFurGrassDSS_pass2 != grcStateBlock::DSS_Invalid);
	}

	// pass3:
	if(hFurGrassBS_pass3 == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;

		bsDesc.IndependentBlendEnable		= FALSE;
		bsDesc.BlendRTDesc[0].BlendEnable	= FALSE;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
		
		hFurGrassBS_pass3 = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hFurGrassBS_pass3 != grcStateBlock::BS_Invalid);
	}

	if(hFurGrassDSS_pass3 == grcStateBlock::DSS_Invalid)
	{
		grcDepthStencilStateDesc dsDesc;
		dsDesc.DepthEnable					= rsDepthTest;
		dsDesc.DepthWriteMask				= rsDepthWrite;
		dsDesc.DepthFunc					= rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		
		// stencil state:
		dsDesc.StencilEnable				= TRUE;
		dsDesc.StencilReadMask				= 0x10;						// read 5th bit
		dsDesc.StencilWriteMask				= 0x10;						// write only to 5th bit;
		dsDesc.FrontFace.StencilFunc		= 
		dsDesc.BackFace.StencilFunc			= grcRSV::CMP_EQUAL;
		dsDesc.FrontFace.StencilPassOp		= 
		dsDesc.BackFace.StencilPassOp		= grcRSV::STENCILOP_ZERO;	// reset special bit
		dsDesc.FrontFace.StencilFailOp		= 
		dsDesc.BackFace.StencilFailOp		= grcRSV::STENCILOP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp	= 
		dsDesc.BackFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;

		hFurGrassDSS_pass3		= grcStateBlock::CreateDepthStencilState(dsDesc);
		Assert(hFurGrassDSS_pass3 != grcStateBlock::DSS_Invalid);
	}

	if(hFurGrassBS_pass3_end == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;

		bsDesc.IndependentBlendEnable		= FALSE;
		bsDesc.BlendRTDesc[0].BlendEnable	= FALSE;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
		
		hFurGrassBS_pass3_end = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hFurGrassBS_pass3_end != grcStateBlock::BS_Invalid);
	}

	return(TRUE);
}

//
//
//
//
static
void furDrawTriangleArrays(atArray<Vector3> &triVerts)
{
const float zOffset = gfFurGrassZOffset;

const u32 nNumTris	= triVerts.GetCount() / 3;

s32 numTrisLeftToDraw = nNumTris;
u32 actualVertIdx = 0;

	while(numTrisLeftToDraw > 0)
	{
		s32 trisToDraw = numTrisLeftToDraw;

		if((numTrisLeftToDraw*3) > grcBeginMax)
		{
			trisToDraw			=  grcBeginMax/3;
			numTrisLeftToDraw	-= grcBeginMax/3;
		}
		else
		{
			trisToDraw			 = numTrisLeftToDraw;
			numTrisLeftToDraw	-= trisToDraw;
		}
		
		grcNormal3f(0.0f, 0.0f, 1.0f);
		grcTexCoord2f(0.0f, 0.0f);
		grcColor( Color32(255, 255, 255, 255) );
		Color32 colV0, colV1, colV2;

		grcBegin(drawTris, trisToDraw*3);
		for(s32 v=0; v<trisToDraw; v++)
		{
			Vector3 *pVert0 = &triVerts[ actualVertIdx ];
			colV0.Set((u32)pVert0->uw);
			actualVertIdx++;

			Vector3 *pVert1 = &triVerts[ actualVertIdx ];
			colV1.Set((u32)pVert1->uw);
			actualVertIdx++;

			Vector3 *pVert2 = &triVerts[ actualVertIdx ];
			colV2.Set((u32)pVert2->uw);
			actualVertIdx++;

static dev_float uvMultiplier = 1.0f;

Vector2 uvOffset0, uvOffset1, uvOffset2;
			uvOffset0.x = rage::Abs(pVert0->x) *uvMultiplier;	//rage::Abs( rage::Modf( pVert0->x, &int_modf) );
			uvOffset0.y = rage::Abs(pVert0->y) *uvMultiplier;	//rage::Abs( rage::Modf( pVert0->y, &int_modf) );

			uvOffset1.x = rage::Abs(pVert1->x)*uvMultiplier;	//rage::Abs( rage::Modf( pVert1->x, &int_modf) );
			uvOffset1.y = rage::Abs(pVert1->y)*uvMultiplier;	//rage::Abs( rage::Modf( pVert1->y, &int_modf) );

			uvOffset2.x = rage::Abs(pVert2->x)*uvMultiplier;	//rage::Abs( rage::Modf( pVert1->x, &int_modf) );
			uvOffset2.y = rage::Abs(pVert2->y)*uvMultiplier;	//rage::Abs( rage::Modf( pVert1->y, &int_modf) );


			grcColor(colV0);
			grcTexCoord2f(uvOffset0.x, uvOffset0.y);
			grcVertex3f(pVert0->x, pVert0->y, pVert0->z + zOffset);

			grcColor(colV1);
			grcTexCoord2f(uvOffset1.x, uvOffset1.y);
			grcVertex3f(pVert1->x, pVert1->y, pVert1->z + zOffset);

			grcColor(colV2);
			grcTexCoord2f(uvOffset2.x, uvOffset2.y);
			grcVertex3f(pVert2->x, pVert2->y, pVert2->z + zOffset);
		}
		grcEnd();
	}

} // end of furDrawTriangleArrays()...


//
//
//
//
static
void furDrawTriangleBatch(atArray<Vector3> &triVerts, const u32 nNumTris, atArray<u8> &triBatchIdx, const u8 batchID)
{
const float zOffset = gfFurGrassZOffset;

u32 actualTriIdx=0;

s32 numTrisLeftToDraw = nNumTris;

	while(numTrisLeftToDraw > 0)
	{
		s32 trisToDraw = numTrisLeftToDraw;

		if((numTrisLeftToDraw*3) > grcBeginMax)
		{
			trisToDraw			=  grcBeginMax/3;
			numTrisLeftToDraw	-= grcBeginMax/3;
		}
		else
		{
			trisToDraw			 = numTrisLeftToDraw;
			numTrisLeftToDraw	-= trisToDraw;
		}
		
		grcNormal3f(0.0f, 0.0f, 1.0f);
		grcTexCoord2f(0.0f, 0.0f);
		grcColor( Color32(255, 255, 255, 255) );
		Color32 colV0, colV1, colV2;

		grcBegin(drawTris, trisToDraw*3);
		for(s32 v=0; v<trisToDraw; v++)
		{
			while(triBatchIdx[actualTriIdx++] != batchID)	// skip tri if not belonging to current batch
				continue;

			u32 actualVertIdx = (actualTriIdx-1)*3;

			Vector3 *pVert0 = &triVerts[ actualVertIdx ];
			colV0.Set((u32)pVert0->uw);
			actualVertIdx++;

			Vector3 *pVert1 = &triVerts[ actualVertIdx ];
			colV1.Set((u32)pVert1->uw);
			actualVertIdx++;

			Vector3 *pVert2 = &triVerts[ actualVertIdx ];
			colV2.Set((u32)pVert2->uw);
			actualVertIdx++;

static dev_float uvMultiplier = 1.0f;

Vector2 uvOffset0, uvOffset1, uvOffset2;
			uvOffset0.x = rage::Abs(pVert0->x) *uvMultiplier;	//rage::Abs( rage::Modf( pVert0->x, &int_modf) );
			uvOffset0.y = rage::Abs(pVert0->y) *uvMultiplier;	//rage::Abs( rage::Modf( pVert0->y, &int_modf) );

			uvOffset1.x = rage::Abs(pVert1->x)*uvMultiplier;	//rage::Abs( rage::Modf( pVert1->x, &int_modf) );
			uvOffset1.y = rage::Abs(pVert1->y)*uvMultiplier;	//rage::Abs( rage::Modf( pVert1->y, &int_modf) );

			uvOffset2.x = rage::Abs(pVert2->x)*uvMultiplier;	//rage::Abs( rage::Modf( pVert1->x, &int_modf) );
			uvOffset2.y = rage::Abs(pVert2->y)*uvMultiplier;	//rage::Abs( rage::Modf( pVert1->y, &int_modf) );


			grcColor(colV0);
			grcTexCoord2f(uvOffset0.x, uvOffset0.y);
			grcVertex3f(pVert0->x, pVert0->y, pVert0->z + zOffset);

			grcColor(colV1);
			grcTexCoord2f(uvOffset1.x, uvOffset1.y);
			grcVertex3f(pVert1->x, pVert1->y, pVert1->z + zOffset);

			grcColor(colV2);
			grcTexCoord2f(uvOffset2.x, uvOffset2.y);
			grcVertex3f(pVert2->x, pVert2->y, pVert2->z + zOffset);
		}
		grcEnd();
	}

} // end of furDrawTriangleBatch()...

//
//
//
//
bool CPlantMgr::FurGrassLODRender(const Vector3& cameraPos, const Vector3& UNUSED_PARAM(camFront), const Vector3& UNUSED_PARAM(playerPos))
{
	Assert(gFurGrassLODShader);
	Assert(gFurGrassLODComboTechniqueID);

#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	const u32 cFurMaxNumTris = CPLANT_MAX_CACHE_LOC_TRIS_NUM*CPLANT_LOC_TRIS_LIST_NUM/4;
#else
	const u32 cFurMaxNumTris = CPLANT_MAX_CACHE_LOC_TRIS_NUM*CPLANT_LOC_TRIS_LIST_NUM/2;
#endif

const bool bIsCameraUnderwater = CGrassRenderer::GetGlobalCameraUnderwater();

atUserArray<Vector3>	tabFurTriVerts( Alloca(Vector3,cFurMaxNumTris*3), cFurMaxNumTris*3 );
atUserArray<u8>			tabFurTriVertsColEntIdx( Alloca(u8,cFurMaxNumTris), cFurMaxNumTris );

u16 tabCountColEntTri[CPLANT_COL_ENTITY_CACHE_SIZE];	// counts all tris belonging to ColEntities
	sysMemSet(tabCountColEntTri, 0x00, sizeof(tabCountColEntTri));

const float fFurGrassMaxDist2 = gfFurGrassMaxDist*gfFurGrassMaxDist;

const u32 bufferID	= GetRenderRTBufferID();

	// find default batch (first batch with "invalid" pickup info),
	// so other "invalid" batches can be gathered and rendered together
	// (avoiding seams from batching - collision entity meshes can be split during export):
u8 defaultBatchIdx=255;
	for(u32 batch=0; batch<CPLANT_COL_ENTITY_CACHE_SIZE; batch++)
	{
		if(!m_furGrassPickupRenderInfo[bufferID][batch].m_bValid)
		{
			defaultBatchIdx = (u8)batch;
			break;
		}
	}
	Assertf(defaultBatchIdx < CPLANT_COL_ENTITY_CACHE_SIZE, "Error finding default batch!");

spdTransposedPlaneSet8 cullFrustum;
	CGrassRenderer::GetGlobalCullFrustum(&cullFrustum);


bool bFurGrassTagsPresent = false;

	//
	// render all close lists:
	//
	for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM && (tabFurTriVerts.GetCapacity()!=tabFurTriVerts.GetCount()); listID++)
	{
		CPlantLocTriArray& triRenderTab = *m_LocTrisRenderTab[bufferID][listID];
		u16 loctri = triRenderTab.m_CloseListHead;
		while(loctri)
		{
			CPlantLocTri *pLocTri = &triRenderTab[loctri];

			// check if pLocTri inside current View Frustum and creates plants:
			if(pLocTri->m_bCreatesPlants)
			{
				s32 procTagId = PGTAMATERIALMGR->UnpackProcId(pLocTri->m_nSurfaceType);
				s32 plantInfoIndex = g_procInfo.m_procTagTable[procTagId].plantIndex;
				CPlantInfo *pPlantInfo = &g_procInfo.m_plantInfos[plantInfoIndex];

				if(pPlantInfo->m_Flags.IsSet(PROCPLANT_FURGRASS))
				{
					bFurGrassTagsPresent = true;
					if(IsLocTriVisibleByCamera(cullFrustum, pLocTri) && IsLocTriVisibleUnderwater(pLocTri, bIsCameraUnderwater))
					{
						Vector3 Vert0 = pLocTri->GetV1();
						Vector3 Vert1 = pLocTri->GetV2();
						Vector3 Vert2 = pLocTri->GetV3();
						const Vector3 Center= pLocTri->GetCenter();

						Color32 colV0, colV1, colV2;
					#if __BANK
						if(gbForceGroundColor)
						{
							colV0 = gbGroundColorV1;	colV0.SetAlpha(CPLANT_PV_DEFAULT_D_SZ_SXYZ);
							colV1 = gbGroundColorV2;	colV1.SetAlpha(CPLANT_PV_DEFAULT_D_SZ_SXYZ);
							colV2 = gbGroundColorV3;	colV2.SetAlpha(CPLANT_PV_DEFAULT_D_SZ_SXYZ);
						}
						else
					#endif //__BANK
						{
							colV0 = pLocTri->m_GroundColorV1;
							colV1 = pLocTri->m_GroundColorV2;
							colV2 = pLocTri->m_GroundColorV3;
						}

						// TODO: PPU: vectorise properly
					#if CPLANT_USE_COLLISION_2D_DIST
						const Vector3 _colDistV0 = cameraPos - Vert0;
						const Vector3 _colDistV1 = cameraPos - Vert1;
						const Vector3 _colDistV2 = cameraPos - Vert2;
						const Vector3 _colDistV3 = cameraPos - Center;
						const Vector2 colDistV0(_colDistV0.x, _colDistV0.y);
						const Vector2 colDistV1(_colDistV1.x, _colDistV1.y);
						const Vector2 colDistV2(_colDistV2.x, _colDistV2.y);
						const Vector2 colDistV3(_colDistV3.x, _colDistV3.y);
					#else
						const Vector3 colDistV0 = cameraPos - Vert0;
						const Vector3 colDistV1 = cameraPos - Vert1;
						const Vector3 colDistV2 = cameraPos - Vert2;
						const Vector3 colDistV3 = cameraPos - Center;
					#endif
						const float colDistSqr0 = colDistV0.Mag2();
						const float colDistSqr1 = colDistV1.Mag2();
						const float colDistSqr2 = colDistV2.Mag2();
						const float colDistSqr3 = colDistV3.Mag2();
							
						// calc middle points distances (distance between camera and tri edges' middle points):
					#if CPLANT_USE_COLLISION_2D_DIST
						const Vector2 _colDistMV01( (colDistV0+colDistV1) * 0.5f);	// V0-1
						const Vector2 _colDistMV12( (colDistV1+colDistV2) * 0.5f);	// V1-2
						const Vector2 _colDistMV20( (colDistV2+colDistV0) * 0.5f);	// V2-0
						const Vector2 colDistMV01(_colDistMV01.x, _colDistMV01.y);
						const Vector2 colDistMV12(_colDistMV12.x, _colDistMV12.y);
						const Vector2 colDistMV20(_colDistMV20.x, _colDistMV20.y);
					#else
						const Vector3 colDistMV01( (colDistV0+colDistV1) * 0.5f );	// V0-1
						const Vector3 colDistMV12( (colDistV1+colDistV2) * 0.5f );	// V1-2
						const Vector3 colDistMV20( (colDistV2+colDistV0) * 0.5f );	// V2-0
					#endif
						const float colDistSqr4 = colDistMV01.Mag2();
						const float colDistSqr5 = colDistMV12.Mag2();
						const float colDistSqr6 = colDistMV20.Mag2();

						if(	(colDistSqr0 < fFurGrassMaxDist2)	||	
							(colDistSqr1 < fFurGrassMaxDist2)	||	
							(colDistSqr2 < fFurGrassMaxDist2)	||	
							(colDistSqr3 < fFurGrassMaxDist2)	||	
							(colDistSqr4 < fFurGrassMaxDist2)	||	
							(colDistSqr5 < fFurGrassMaxDist2)	||	
							(colDistSqr6 < fFurGrassMaxDist2)	)
						{
							Vert0.uw = colV0.GetColor();
							Vert1.uw = colV1.GetColor();
							Vert2.uw = colV2.GetColor();
							tabFurTriVerts.Push(Vert0);
							tabFurTriVerts.Push(Vert1);
							tabFurTriVerts.Push(Vert2);

							// HACK: translate ColEntIdx into index for this furtri:
							u8 idx = pLocTri->m_nColEntIdx-1;
							FastAssert(idx < CPLANT_COL_ENTITY_CACHE_SIZE);
						
							// pickup settings are invalid? - add this tri to "default" batch:
							if(!m_furGrassPickupRenderInfo[bufferID][idx].m_bValid)
							{
								idx = defaultBatchIdx;
							}

							tabFurTriVertsColEntIdx.Push(idx);

							// count furtri as belonging to given ColEnt:
							tabCountColEntTri[idx]++;
						
							// array fully filled up?
							if(tabFurTriVerts.GetCapacity()==tabFurTriVerts.GetCount())
							{
								break;
							}
						}
					}// if(IsLocTriVisibleByCamera(cullFrustum, pLocTri) && IsLocTriVisibleUnderwater(pLocTri, bIsCameraUnderwater)))...
				}// if(LODMASK_FUR)...
			}// if(pLocTri->m_bCreatesPlants && IsLocTriVisibleByCamera(pLocTri))...
			
			loctri = pLocTri->m_NextTri;
		}//while(pLocTri)...
	}//for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)...
	#if __BANK
		gnFurGrassNumPolys = tabFurTriVerts.GetCount()/3;	// debug stats
	#endif

	if(!tabFurTriVerts.GetCount())
	{
		return(bFurGrassTagsPresent);	// nothing to render
	}


	DeferredLighting::PrepareCutoutPass(true, true, false);

	grmShader *shader = gFurGrassLODShader;

Matrix34 mtx;
	mtx.Identity();
	grcViewport::SetCurrentWorldMtx(RCC_MAT34V(mtx));


bool bRestoreSamplers = false;
grcSamplerStateHandle	sshFurLODDiffuseTex			= grcStateBlock::SS_Invalid,
						sshFurLODComboHeightTex		= grcStateBlock::SS_Invalid,
						sshFurLODNormalTex			= grcStateBlock::SS_Invalid;
	if(gbEnableFurGrassMipmapLodBias)
	{
		grcSamplerStateHandle h;
		grcSamplerStateDesc d;

		// quantize with accuracy of SamplerStateSet:
		const float fFurGrassMipmapLodBias = float((s32(int(gfFurGrassMipmapLodBias*256.0f)&0x1FFF)<<19)>>19) * (1.0f / 256.0f);		

		h = gFurGrassLODShader->GetSamplerState(varID_FurLODDiffuseTex);
		grcStateBlock::GetSamplerStateDesc(h,d);
		d.MipLodBias = fFurGrassMipmapLodBias;
		sshFurLODDiffuseTex = grcStateBlock::CreateSamplerState(d);
		Assert(sshFurLODDiffuseTex != grcStateBlock::SS_Invalid);
		gFurGrassLODShader->PushSamplerState(varID_FurLODDiffuseTex, sshFurLODDiffuseTex);

		h = gFurGrassLODShader->GetSamplerState(varID_FurLODComboHeightTex);
		grcStateBlock::GetSamplerStateDesc(h,d);
		d.MipLodBias = fFurGrassMipmapLodBias;
		sshFurLODComboHeightTex = grcStateBlock::CreateSamplerState(d); 
		Assert(sshFurLODComboHeightTex != grcStateBlock::SS_Invalid);
		gFurGrassLODShader->PushSamplerState(varID_FurLODComboHeightTex, sshFurLODComboHeightTex);

		if(varID_FurLODNormalTex)
		{
			h = gFurGrassLODShader->GetSamplerState(varID_FurLODNormalTex);
			grcStateBlock::GetSamplerStateDesc(h,d);
			d.MipLodBias = fFurGrassMipmapLodBias;
			sshFurLODNormalTex = grcStateBlock::CreateSamplerState(d); 
			Assert(sshFurLODNormalTex != grcStateBlock::SS_Invalid);
			gFurGrassLODShader->PushSamplerState(varID_FurLODNormalTex, sshFurLODNormalTex);
		}
	
		bRestoreSamplers = true;
	}


	shader->SetVar(varID_FurFresnel,	gfFurGrassFresnel);
	shader->SetVar(varID_FurSpecFalloff,gfFurGrassSpecFalloff);
	shader->SetVar(varID_FurSpecInt,	gfFurGrassSpecInt);
	shader->SetVar(varID_FurBumpiness,	gfFurGrassBumpiness * 0.001f);

	shader->SetVar(varID_FurLODDiffuseTex, gFurGrassLODComboDiffTex);

	if(varID_FurLODNormalTex)
		shader->SetVar(varID_FurLODNormalTex,  gFurGrassLODTexN);

#if FURGRASS_USE_ALPHA_STIPPLE
	shader->SetVar(varID_FurLODStippleTex, gFurGrassLODStippleTex);
#endif

Vector4 alphaFadeParams;
	const float invAlphaFade = 1.0f / (gfFurGrassAlphaFadeFar*gfFurGrassAlphaFadeFar - gfFurGrassAlphaFadeClose*gfFurGrassAlphaFadeClose);
	alphaFadeParams.x = gfFurGrassAlphaFadeFar*gfFurGrassAlphaFadeFar	* invAlphaFade;		// f2  / (f2-c2)
	alphaFadeParams.y = -1.0f											* invAlphaFade;		//-1.0 / (f2-c2)
	alphaFadeParams.z = 0.0f;
	alphaFadeParams.w = 0.0f;
	shader->SetVar(varID_FurDitherAlphaFadeParams, alphaFadeParams);

Vector4 furLayerParams;
	furLayerParams.x = 1.0f;								// layerShd
	furLayerParams.y = float(gnFurGrassFurStep)/10000.0f;	// furStep
	furLayerParams.z = 1.0f / 255.0f;						// alphaClip
	furLayerParams.w = 0.0f;

Vector4 furLayerParams2;
	furLayerParams2.x = gfFurGrassOffsetU;					// offset U
	furLayerParams2.y = gfFurGrassOffsetV;					// offset V
	furLayerParams2.z = float(gnFurGrassUmScaleX)/100000.0f;// um scale X
	furLayerParams2.w = float(gnFurGrassUmScaleY)/100000.0f;// um scale Y
	shader->SetVar(varID_FurLayerParams2, furLayerParams2);

Vector4 furLayerParams3;
	furLayerParams3.x = gfFurGrassUvScale;					// UV scale
	furLayerParams3.y = gfFurGrassUvScale2;					// UV scale 2
	furLayerParams3.z = gfFurGrassUvScale3;					// UV scale 3
	furLayerParams3.w = 0.0f;
	shader->SetVar(varID_FurLayerParams3, furLayerParams3);

	shader->SetVar(varID_FurPlayerLFeetPos, CGrassRenderer::GetGlobalPlayerLFootPos());
	shader->SetVar(varID_FurPlayerRFeetPos, CGrassRenderer::GetGlobalPlayerRFootPos());


	const u32 DEFERRED_MATERIAL_FURGRASS = DEFERRED_MATERIAL_SPECIALBIT; //0x10
	CompileTimeAssert(DEFERRED_MATERIAL_FURGRASS==0x10);

static dev_bool bEnableDitherPass=!FURGRASS_USE_ALPHA_STIPPLE;
	// 1. draw dithered alphafade mask into stencil:
	if(bEnableDitherPass)
	{
		// render only to depth/stencil:
		#if __PS3 || (RSG_PC || RSG_DURANGO || RSG_ORBIS)
			GBuffer::UnlockTargets();
			GBuffer::LockSingleTarget(GBUFFER_RT_0, TRUE);
		#endif

		grcStateBlock::SetRasterizerState(	hFurGrassRS_pass1	);
	static dev_u64 sampleMask = ~0ULL;
		grcStateBlock::SetBlendState(		hFurGrassBS_pass1, ~0U,sampleMask);
		CShaderLib::SetAlphaTestRef(0.f);
	const u32 materialID = DEFERRED_MATERIAL_FURGRASS;
		grcStateBlock::SetDepthStencilState(hFurGrassDSS_pass1, materialID);

		// draw dithered:
		grcEffectTechnique forcedTech = gFurGrassDitheredTechniqueID;
		ASSERT_ONLY(const u32 numPasses=) shader->BeginDraw(grmShader::RMC_DRAW, false, forcedTech);
		Assert(numPasses>0);
		shader->Bind(0);
		{
			furDrawTriangleArrays(tabFurTriVerts);
		}
		shader->UnBind();
		shader->EndDraw();

		#if __PS3 || (RSG_PC || RSG_DURANGO || RSG_ORBIS)
			GBuffer::UnlockSingleTarget(GBUFFER_RT_0);
			GBuffer::LockTargets();
		#endif
	}

	grcStateBlock::SetBlendState(hFurGrassBS_pass1_end);


	// 2. main furgrass draw:
	if(1)
	{
		grcEffectTechnique forcedTech = gFurGrassLODComboTechniqueID;
		ASSERT_ONLY(const u32 numShaderPasses=) shader->BeginDraw(grmShader::RMC_DRAW, false, forcedTech);
		Assert(numShaderPasses>0);
		Assert(numShaderPasses==FURGRASS_MAX_LAYERS);
		Assert(gnFurGrassNumLayers <= numShaderPasses);
		const u32 numPasses = gnFurGrassNumLayers;

		// draw in batches - one batch for every ColEntity:
		for(u32 batch=0; batch<CPLANT_COL_ENTITY_CACHE_SIZE; batch++)
		{
			if(!tabCountColEntTri[batch])
				continue;	// skip if nothing to draw for this batch

			CPlantColBoundEntryFurGrassInfo *pFurGrassInfo = &m_furGrassPickupRenderInfo[bufferID][batch];

			grcTexture *pDiffuseTex = (pFurGrassInfo->m_bValid && pFurGrassInfo->m_furGrassDiffTex)?
											pFurGrassInfo->m_furGrassDiffTex : gFurGrassLODComboDiffTex;
			shader->SetVar(varID_FurLODDiffuseTex, pDiffuseTex);

			if(varID_FurLODNormalTex)
			{
				grcTexture *pNormalTex = (pFurGrassInfo->m_bValid && pFurGrassInfo->m_furGrassNormalTex)?
											pFurGrassInfo->m_furGrassNormalTex : gFurGrassLODTexN;
				shader->SetVar(varID_FurLODNormalTex, pNormalTex);
			}

			for(u32 pass=0; pass<numPasses; pass++)
			{
				if(bEnableDitherPass)
				{
					if(pass < (numPasses-1))
					{	// earlier passes: do not touch stencil
						const u32 stencilRef = DEFERRED_MATERIAL_FURGRASS;
						grcStateBlock::SetDepthStencilState(	hFurGrassDSS_pass2a, stencilRef	);
					}
					else
					{	// last pass: clear stencil:
						const u32 stencilRef = DEFERRED_MATERIAL_FURGRASS;
						grcStateBlock::SetDepthStencilState(	hFurGrassDSS_pass2b, stencilRef	);
					}
				}
				else
				{
					grcStateBlock::SetDepthStencilState(hFurGrassDSS_pass2, 0);
				}

				// fur params settable for every layer:
				Assert(pass < FURGRASS_MAX_LAYERS);
				furLayerParams.x = float(gnFurGrassShadow[pass])	/ 255.0f;
				furLayerParams.z = float(gnFurGrassAlphaClip[pass]) / 255.0f;
				shader->SetVar(varID_FurLayerParams, furLayerParams);

				// different texture for every pass:
				if(gbFurGrassSeparateTextures)
				{
					switch(gnFurGrassSeparateTexturesTech)
					{
						case(0):	// 4 separate combo height GA textures
						{
							const u32 texIdx = pass>>1;
							grcTexture *pComboHTex = (pFurGrassInfo->m_bValid && pFurGrassInfo->m_furGrassComboH4Tex[texIdx])?
														pFurGrassInfo->m_furGrassComboH4Tex[texIdx] :
														gFurGrassLODComboH4Tex[texIdx];
							shader->SetVar(varID_FurLODComboHeightTex,		pComboHTex);
							shader->SetVar(varID_FurLODComboHeightTexMask,	gFurGrassLODComboH4TexMask[ pass ]);
						}
						break;

						//case(1):	// 2 separate combo height RGBA textures
						//	shader->SetVar(varID_FurLODComboHeightTex,		gFurGrassLODComboH2Tex[ pass>3?1:0 ]);
						//	shader->SetVar(varID_FurLODComboHeightTexMask,	gFurGrassLODComboH2TexMask[ pass ]);
						//	break;
					}
				}

				shader->Bind(pass);
				{
					furDrawTriangleBatch(tabFurTriVerts, tabCountColEntTri[batch], tabFurTriVertsColEntIdx, (u8)batch);
				}
				shader->UnBind();
			}//for(u32 pass=0; pass<numPasses; pass++)...
		}//for(u32 batch=0; batch<CPLANT_COL_ENTITY_CACHE_SIZE; batch++)

		shader->EndDraw();
	}

	// 3. clear stencil
	if(bEnableDitherPass)
	{
		#if __PS3 || (RSG_PC || RSG_DURANGO || RSG_ORBIS)
			GBuffer::UnlockTargets();
			GBuffer::LockSingleTarget(GBUFFER_RT_0, TRUE);
		#endif

		grcStateBlock::SetBlendState(			hFurGrassBS_pass3	);
	const u32 stencilRef = DEFERRED_MATERIAL_FURGRASS;
		grcStateBlock::SetDepthStencilState(	hFurGrassDSS_pass3, stencilRef	);

		grcEffectTechnique forcedTech = gFurGrassDitheredTechniqueID;
		ASSERT_ONLY(const u32 numPasses=) shader->BeginDraw(grmShader::RMC_DRAW, false, forcedTech);
		Assert(numPasses>0);
		shader->Bind(0);
		{
			furDrawTriangleArrays(tabFurTriVerts);
		}
		shader->UnBind();
		shader->EndDraw();
		

		#if __PS3 || (RSG_PC || RSG_DURANGO || RSG_ORBIS)
			GBuffer::UnlockSingleTarget(GBUFFER_RT_0);
			GBuffer::LockTargets();
		#endif
	}

	grcStateBlock::SetBlendState(hFurGrassBS_pass3_end);

	// cleanup:
	if(bRestoreSamplers)
	{
		gFurGrassLODShader->PopSamplerState(varID_FurLODDiffuseTex);
		grcStateBlock::DestroySamplerState(sshFurLODDiffuseTex); 

		gFurGrassLODShader->PopSamplerState(varID_FurLODComboHeightTex);
		grcStateBlock::DestroySamplerState(sshFurLODComboHeightTex); 

		if(varID_FurLODNormalTex)
		{
			gFurGrassLODShader->PopSamplerState(varID_FurLODNormalTex);
			grcStateBlock::DestroySamplerState(sshFurLODNormalTex); 
		}
	}

	DeferredLighting::FinishCutoutPass();

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert, DEFERRED_MATERIAL_DEFAULT);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);

	return(bFurGrassTagsPresent);
}// end of CPlantMgr::FurGrassLODRender()...
#endif //FURGRASS_TEST_V4...


#if PLANTSMGR_DATA_EDITOR
//////////////////////////////////////////////////////////////////////////
// CPlantMgrDataEditor

//
//
//
//
CPlantMgrDataEditor::CPlantMgrDataEditor()
{
	m_selectedEntries.Reset();
	m_selectedEntries.Reserve(64);
	m_bAllowPicking = false;
	m_bShowPlantPolys = true;
	m_bEdAllCollisionSelectable = false;
	m_bEdRegenTriCache = false;
	m_bSphereSearchPending = false;
	m_bClearSelectedEntries = false;
	m_bUpdateCurEditEntry = false;
	m_bSaveCurEditEntry = false;
	m_bSetProcTypeToAll = false;
	m_bMaxUiSphereSearchFromFilePending = false;
	m_bMaxUiWritingSelectionToFile = false;
	m_bMaxUiSavingChangesToBatch = false;
	m_searchEntriesFound = 0;
	m_editEntryIdx = -1;
	m_searchRadius = 0.05f;
	m_triPositions.Reset();
	m_triPositions.Reserve(4);

	ResetEditableEntry();

	m_tagNamesCombo = NULL;
}

bool CPlantMgrDataEditor::InitWidgets(bkBank& bank, const char *bankName)
{
	bank.PushGroup(bankName, false);
		bank.PushGroup("Search Options", false);
			bank.AddToggle("Allow Picking",				&m_bAllowPicking,				datCallback(cbBankToggleAllowPicking));
			bank.AddToggle("Show Plant Polys",			&m_bShowPlantPolys,				datCallback(cbBankToggleShowPlantPolys));
			bank.AddToggle("All Collision Selectable",	&m_bEdAllCollisionSelectable,	datCallback(cbBankToggleSelectAllCollision));
			bank.AddSeparator();
			bank.AddTitle("Search Using Sphere Volume");
			bank.AddVector("Sphere Center",		&m_searchPos,			-10000.0f, 10000.0f, 0.001f);
			bank.AddButton("Copy Player's Position to Sphere Center",	datCallback(cbBankButtonCopyPlayerPosToSphereCenter));
			bank.AddSlider("Sphere Radius",		&m_searchRadius,		0.01f, 50.0f, 0.001f);
			bank.AddButton("Search",									datCallback(cbBankButtonSearchSphere));
			bank.AddSeparator();
			bank.AddText("Entries found:",		&m_searchEntriesFound,	true);
			bank.AddSeparator();
			bank.AddButton("Clear Selected Entries",					datCallback(cbBankButtonClearSelEntries));
		bank.PopGroup();

		bank.PushGroup("Edit Options", false);
			bank.AddText("Entry Index:", &m_editEntryIdx, false,		datCallback(cbBankTextEditEntryIdxChanged));
			AddEditableDataToWidget(bank);
		bank.PopGroup();
	bank.PopGroup();

	return true;
}

void CPlantMgrDataEditor::ResetEditableEntry()
{
	m_editableEntry.m_nSurfaceType = 0;

	for (int i = 0; i < 3; i++) 
	{
		m_editableEntry.m_V[i].Zero();
		m_editableEntry.m_GroundColorV[i].Set(0,0,0,0);
		m_editableEntry.m_GroundDensity[i] =0;
		m_editableEntry.m_GroundScaleZ[i] = 0;
		m_editableEntry.m_GroundScaleXYZ[i] = 0;
	}
}

void CPlantMgrDataEditor::AddEditableDataToWidget(bkBank& bank)
{
	bank.AddVector("Vertex#0", &m_editableEntry.m_V[0], -5000.0f, 5000.0f, 0.001f);
	bank.AddVector("Vertex#1", &m_editableEntry.m_V[1], -5000.0f, 5000.0f, 0.001f);
	bank.AddVector("Vertex#2", &m_editableEntry.m_V[2], -5000.0f, 5000.0f, 0.001f);
	bank.AddSeparator();

	m_editableEntry.m_nSurfaceType = 0;

	bank.AddSlider("Type", &m_editableEntry.m_nSurfaceType, 0, MAX_PROCEDURAL_TAGS, 1);

	if(m_tagNames.GetCount()==0)
	{
		m_tagNames.Resize(MAX_PROCEDURAL_TAGS+1);
		for(s32 i=0; i<=MAX_PROCEDURAL_TAGS; i++)
		{
			m_tagNames[i] = "n/a";
		}
	}

	if(!m_tagNamesCombo)
	{
		// create tag names only once - only for _LiveEdit_ group:
		m_tagNamesCombo = bank.AddCombo("Type name", &m_editableEntry.m_nSurfaceType, MAX_PROCEDURAL_TAGS, &m_tagNames[0], datCallback(cbBankButtonTypeNameChanged));
		Assert(m_tagNamesCombo);
	}

	bank.AddSeparator();
	bank.AddColor("Ground Color at vertex#0", &m_editableEntry.m_GroundColorV[0]);
	bank.AddColor("Ground Color at vertex#1", &m_editableEntry.m_GroundColorV[1]);
	bank.AddColor("Ground Color at vertex#2", &m_editableEntry.m_GroundColorV[2]);
	bank.AddSeparator();
	bank.AddSlider("Ground Density weight at vertex#0 (maps to [1, 0.6, 0.3, 0])", &m_editableEntry.m_GroundDensity[0], 0, 3, 1);
	bank.AddSlider("Ground Density weight at vertex#1 (maps to [1, 0.6, 0.3, 0])", &m_editableEntry.m_GroundDensity[1], 0, 3, 1);
	bank.AddSlider("Ground Density weight at vertex#2 (maps to [1, 0.6, 0.3, 0])", &m_editableEntry.m_GroundDensity[2], 0, 3, 1);
	bank.AddSeparator();
	bank.AddSlider("Ground ScaleZ weight at vertex#0 (maps to [1, 0.6, 0.3, 0])", &m_editableEntry.m_GroundScaleZ[0], 0, 3, 1);
	bank.AddSlider("Ground ScaleZ weight at vertex#1 (maps to [1, 0.6, 0.3, 0])", &m_editableEntry.m_GroundScaleZ[1], 0, 3, 1);
	bank.AddSlider("Ground ScaleZ weight at vertex#2 (maps to [1, 0.6, 0.3, 0])", &m_editableEntry.m_GroundScaleZ[2], 0, 3, 1);
	bank.AddSeparator();
	bank.AddSlider("Ground ScaleXYZ weight at vertex#0 (maps to <0; 1>)", &m_editableEntry.m_GroundScaleXYZ[0], 0,  15, 1);
	bank.AddSlider("Ground ScaleXYZ weight at vertex#1 (maps to <0; 1>)", &m_editableEntry.m_GroundScaleXYZ[1], 0,  15, 1);
	bank.AddSlider("Ground ScaleXYZ weight at vertex#2 (maps to <0; 1>)", &m_editableEntry.m_GroundScaleXYZ[2], 0,  15, 1);
	bank.AddSeparator();
	bank.AddButton("Override proc type for all active tris", datCallback(cbBankButtonSetProcTypeToAll));
	bank.AddSeparator();
	bank.AddButton("Save changes to current entry", datCallback(cbBankButtonSaveCurEntry));
	bank.AddSeparator();
	bank.AddButton("Save changes to batch", datCallback(cbBankButtonSaveBatch));
	bank.AddSeparator();
	bank.AddButton("Read input from file", datCallback(cbBankToggleFindClosestEntriesFromFile));
	bank.AddSeparator();
	bank.AddButton("Write selection to file", datCallback(cbBankToggleWriteSelectionToFile));
	bank.PushGroup("Max UI", false);		// flags for MaxUI<->Game communication only
		bank.AddToggle("Reading input from file",	&m_bMaxUiSphereSearchFromFilePending);
		bank.AddToggle("Writing selection to file",	&m_bMaxUiWritingSelectionToFile);
		bank.AddToggle("Saving changes to batch",	&m_bMaxUiSavingChangesToBatch);
	bank.PopGroup();
}

void CPlantMgrDataEditor::Update()
{
	if (m_bClearSelectedEntries)
	{
		Reset();
		m_bClearSelectedEntries = false;
	}

	// manual in-game picking
	if (m_bAllowPicking) 
	{
		if(ioMouse::GetButtons() & ioMouse::MOUSE_LEFT)		// single select mode
		{
			Vector3 mouseStart, mouseEnd;
			CDebugScene::GetMousePointing(mouseStart, mouseEnd, false);
			if(FindClosestEntry(mouseStart, mouseEnd, false)==1)
			{
				// when only 1 tri picked via mouse, then automatically select it in the interface:
				m_editEntryIdx = 0;
				cbBankTextEditEntryIdxChanged();
			}
		}
		else if(ioMouse::GetButtons() & ioMouse::MOUSE_RIGHT)	// multiselect mode
		{
			Vector3 mouseStart, mouseEnd;
			CDebugScene::GetMousePointing(mouseStart, mouseEnd, false);
			if(FindClosestEntry(mouseStart, mouseEnd, true)==1)
			{
				// when only 1 tri picked via mouse, then automatically select it in the interface:
				m_editEntryIdx = 0;
				cbBankTextEditEntryIdxChanged();
			}
		}
	}

	// default spherical volume search
	if (m_bSphereSearchPending) 
	{
		Reset();
		FindClosestEntries(m_searchPos, m_searchRadius);
		m_bSphereSearchPending = false;
	} 
	else if (m_bMaxUiSphereSearchFromFilePending)
	{
		for (int i = 0; i < m_triPositions.GetCount(); i++)
		{
			FindClosestEntries(m_triPositions[i], m_searchRadius, true);
		}

		Reset(true);
		m_triPositions.clear();
		m_bMaxUiSphereSearchFromFilePending = false;
	}
}

void CPlantMgrDataEditor::UpdateSafe()
{
	if (m_bUpdateCurEditEntry) 
	{
		UpdateEditableEntry();
		m_bUpdateCurEditEntry = false;
	}

	if (m_bMaxUiSavingChangesToBatch)
	{
		for (int i = 0; i < m_selectedEntries.GetCount(); i++) 
		{
			CPlantLocTri* pLocTri = m_selectedEntries[i];
			SaveEditableEntryTo(pLocTri);
		}

		m_bMaxUiSavingChangesToBatch = false;			// signal to MaxUI we're done
		Reset();
	}
	else if (m_bSaveCurEditEntry)
	{
		m_bSaveCurEditEntry = false;

		if (m_editEntryIdx < 0 || m_editEntryIdx >= m_selectedEntries.GetCount())
		{
			ResetEditableEntry();
			return;
		}

		CPlantLocTri* pLocTri = m_selectedEntries[m_editEntryIdx];
		SaveEditableEntryTo(pLocTri);
	}
	else if (m_bSetProcTypeToAll)
	{
		m_bSetProcTypeToAll = false;

		if(g_procInfo.CreatesPlants(m_editableEntry.m_nSurfaceType))	// apply only valid proc tags
		{
			for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)
			{
				CPlantLocTriArray& triTab = *gPlantMgr.m_LocTrisTab[listID];
				u16 loctri = triTab.m_CloseListHead;
				while(loctri)
				{
					CPlantLocTri *pLocTri = &triTab[loctri];

					pLocTri->m_nSurfaceType = PGTAMATERIALMGR->PackProcId(pLocTri->m_nSurfaceType, u8(m_editableEntry.m_nSurfaceType));

					pLocTri->m_bCreatesPlants  = true;
					pLocTri->m_bCreatesObjects = false;	// avoid creating procObjects
					pLocTri->m_bCreatedObjects = false;

					loctri = pLocTri->m_NextTri;
				}
			}//for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)...
		}
	}
}

void CPlantMgrDataEditor::Render()
{
	for (int i = 0; i < m_selectedEntries.GetCount(); i++) 
	{
		const CPlantLocTri* pLocTri = m_selectedEntries[i];
		if (pLocTri == NULL) 
		{
			continue;
		}

		const Color32 col(255, 255, 255, 128);
		Vector3 v0 = pLocTri->GetV1();
		v0.z += Z_OFFSET*2.0f;
		Vector3 v1 = pLocTri->GetV2();
		v1.z += Z_OFFSET*2.0f;
		Vector3 v2 = pLocTri->GetV3();
		v2.z += Z_OFFSET*2.0f;

		grcBegin(drawTris, 3);
			grcColor(col);
			grcVertex3f(v0.x, v0.y, v0.z);
			grcColor(col);
			grcVertex3f(v1.x, v1.y, v1.z);
			grcColor(col);
			grcVertex3f(v2.x, v2.y, v2.z);
		grcEnd();

	}
}

void CPlantMgrDataEditor::Reset(bool bKeepSelection)
{
	if (bKeepSelection == false)
	{
		m_selectedEntries.clear();
		m_triPositions.clear();
	}
	m_searchEntriesFound = 0;
	m_editEntryIdx = -1;
	m_bUpdateCurEditEntry = true;
	m_bSaveCurEditEntry = false;
}

int CPlantMgrDataEditor::FindClosestEntry(const Vector3& p0, const Vector3& p1, bool bMultiSelect)
{
	for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)
	{
		CPlantLocTriArray& triTab = *gPlantMgr.m_LocTrisTab[listID];
		u16 loctri = triTab.m_CloseListHead;
		while(loctri)
		{
			CPlantLocTri *pLocTri = &(triTab[loctri]);
			Vector3 pVert0 = pLocTri->GetV1();
			Vector3 pVert1 = pLocTri->GetV2();
			Vector3 pVert2 = pLocTri->GetV3();

			bool bIntersects = geomSegments::TestSegmentToTriangle(p0, p1, pVert0, pVert1, pVert2);
			if (bIntersects)
			{
				if(bMultiSelect)
				{
					// add tri to m_selectedEntries only if not picked already:
					bool bFound = false;
					if(m_selectedEntries.GetCount()>0)
					{
						for(int i=0; i<m_selectedEntries.GetCount(); i++)
						{
							if(m_selectedEntries[i] == pLocTri)
							{
								bFound = true;
								break;
							}
						}
					}
					
					if(!bFound)
					{
						Reset(true);
						m_selectedEntries.PushAndGrow(pLocTri);
					}
				}
				else
				{
					Reset(false);
					m_selectedEntries.PushAndGrow(pLocTri);
				}
				
				break;
			}

			loctri = pLocTri->m_NextTri;
		}//while(pLocTri)...
	}//for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)...

	m_searchEntriesFound = m_selectedEntries.GetCount();
	return m_searchEntriesFound;
}

bool CPlantMgrDataEditor::FindClosestEntries(const Vector3& p0, float radius, bool bKeepSelection)
{
	bool bFoundAny = false;
	Reset(bKeepSelection);
	for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)
	{
		CPlantLocTriArray& triTab= *gPlantMgr.m_LocTrisTab[listID];
		u16 loctri = triTab.m_CloseListHead;
		while(loctri)
		{
			CPlantLocTri *pLocTri = &(triTab[loctri]);
			Vector3 tri[3];
			tri[0] = pLocTri->GetV1();
			tri[1] = pLocTri->GetV2();
			tri[2] = pLocTri->GetV3();

			Vector3 triNormal;
			triNormal.Cross(tri[1]-tri[0], tri[2]-tri[0]);
			triNormal.Normalize();

			bool bIntersects = 	geomSpheres::TestSphereToTriangle (Vec4V(RCC_VEC3V(p0), ScalarV(radius)), (const Vec3V*)&tri[0], RCC_VEC3V(triNormal));

			if (bIntersects)
			{
				m_selectedEntries.PushAndGrow(pLocTri);
				bFoundAny = true;
			}

			loctri = pLocTri->m_NextTri;
		}//while(pLocTri)...
	}//for(u32 listID=0; listID<CPLANT_LOC_TRIS_LIST_NUM; listID++)...

	m_searchEntriesFound = m_selectedEntries.GetCount();
	return bFoundAny;
}

void CPlantMgrDataEditor::UpdateEditableEntry()
{
	if (m_editEntryIdx < 0 || m_editEntryIdx >= m_selectedEntries.GetCount())
	{
		ResetEditableEntry();
		return;
	}

	CPlantLocTri* pLocTri = m_selectedEntries[m_editEntryIdx];

	if (pLocTri)
	{

		m_editableEntry.m_V[0] = pLocTri->GetV1();
		m_editableEntry.m_V[1] = pLocTri->GetV2();
		m_editableEntry.m_V[2] = pLocTri->GetV3();

		m_editableEntry.m_nSurfaceType = PGTAMATERIALMGR->UnpackProcId(pLocTri->m_nSurfaceType);
		m_editableEntry.m_GroundColorV[0] = pLocTri->m_GroundColorV1;
		m_editableEntry.m_GroundColorV[1] = pLocTri->m_GroundColorV2;
		m_editableEntry.m_GroundColorV[2] = pLocTri->m_GroundColorV3;

		m_editableEntry.m_GroundDensity[0] = (u8)CPlantLocTri::pv8UnpackDensity(pLocTri->m_GroundColorV1.GetAlpha());
		m_editableEntry.m_GroundDensity[1] = (u8)CPlantLocTri::pv8UnpackDensity(pLocTri->m_GroundColorV2.GetAlpha());
		m_editableEntry.m_GroundDensity[2] = (u8)CPlantLocTri::pv8UnpackDensity(pLocTri->m_GroundColorV3.GetAlpha());

		m_editableEntry.m_GroundScaleZ[0] = (u8)CPlantLocTri::pv8UnpackScaleZ(pLocTri->m_GroundColorV1.GetAlpha());
		m_editableEntry.m_GroundScaleZ[1] = (u8)CPlantLocTri::pv8UnpackScaleZ(pLocTri->m_GroundColorV2.GetAlpha());
		m_editableEntry.m_GroundScaleZ[2] =	(u8)CPlantLocTri::pv8UnpackScaleZ(pLocTri->m_GroundColorV3.GetAlpha());

		m_editableEntry.m_GroundScaleXYZ[0] = (u8)CPlantLocTri::pv8UnpackScaleXYZ(pLocTri->m_GroundColorV1.GetAlpha());
		m_editableEntry.m_GroundScaleXYZ[1] = (u8)CPlantLocTri::pv8UnpackScaleXYZ(pLocTri->m_GroundColorV2.GetAlpha());
		m_editableEntry.m_GroundScaleXYZ[2] = (u8)CPlantLocTri::pv8UnpackScaleXYZ(pLocTri->m_GroundColorV3.GetAlpha());
	}
}


void CPlantMgrDataEditor::SaveEditableEntryTo(CPlantLocTri* pLocTri)
{
	if (pLocTri == NULL)
	{
		return;		
	}

	if(g_procInfo.CreatesPlants(m_editableEntry.m_nSurfaceType))	// apply only valid proc tags
	{
		pLocTri->m_nSurfaceType		= PGTAMATERIALMGR->PackProcId(pLocTri->m_nSurfaceType, u8(m_editableEntry.m_nSurfaceType));
		
		pLocTri->m_bCreatesPlants	= true;
		pLocTri->m_bCreatesObjects	= false;	// avoid creating procObjects
		pLocTri->m_bCreatedObjects	= false;
	}

	pLocTri->m_GroundColorV1 = m_editableEntry.m_GroundColorV[0];
	pLocTri->m_GroundColorV2 = m_editableEntry.m_GroundColorV[1];
	pLocTri->m_GroundColorV3 = m_editableEntry.m_GroundColorV[2];


	u8 packedAlpha = CPlantLocTri::pv8PackDensityScaleZScaleXYZ((u8)m_editableEntry.m_GroundDensity[0], 
																(u8)m_editableEntry.m_GroundScaleZ[0], 
																(u8)m_editableEntry.m_GroundScaleXYZ[0]);
	pLocTri->m_GroundColorV1.SetAlpha(packedAlpha);


	packedAlpha = CPlantLocTri::pv8PackDensityScaleZScaleXYZ((u8)m_editableEntry.m_GroundDensity[1], 
															(u8)m_editableEntry.m_GroundScaleZ[1], 
															(u8)m_editableEntry.m_GroundScaleXYZ[1]);
	pLocTri->m_GroundColorV2.SetAlpha(packedAlpha);


	packedAlpha = CPlantLocTri::pv8PackDensityScaleZScaleXYZ((u8)m_editableEntry.m_GroundDensity[2], 
															(u8)m_editableEntry.m_GroundScaleZ[2], 
															(u8)m_editableEntry.m_GroundScaleXYZ[2]);
	pLocTri->m_GroundColorV3.SetAlpha(packedAlpha);

}

// tools cannot currently handle the bkData widget, which would be ideal
// for sending this data, so instead we rely on exchanging data via a 
// binary file ("x:/procdata") containing the positions of the triangles 
// that the tool wants to applies changes to.
//
// if the reading operation is successful, CPlantMgrDataEditor will try
// matching the received positions with triangles in the cache.
// 
// the tool at the other end can then save changes to whatever selection
// was found
bool CPlantMgrDataEditor::ReadInputDataFile()
{
	const char*  INPUT_TRI_POS_FILE = "x:/procdata";
	const char*	 INPUT_TRI_POS_EXT = "bin";

	// temporary debug code to test this locally
#if 0
	static bool bCreateTestfile = true;
	if (bCreateTestfile)
	{
		bCreateTestfile = false;
		fiStream* pOutFile = ASSET.Create(INPUT_TRI_POS_FILE,INPUT_TRI_POS_EXT);

		if (pOutFile == NULL)
		{
			return false;
		}

		const int cNumElems = 16*3;
		float test[cNumElems] = {
			-187.704f,1053.67f,37.267f,
			-187.037f,1054.34f,37.2651f,
			-187.704f,1055.67f,37.4712f,
			-187.037f,1056.34f,37.4905f,
			-187.704f,1057.67f,37.5569f,
			-187.037f,1058.34f,37.6333f,
			-187.704f,1059.67f,37.6802f,
			-187.037f,1060.34f,37.8504f,
			-187.704f,1061.67f,37.9711f,
			-187.037f,1062.34f,38.1309f,
			-187.704f,1063.67f,38.217f,
			-187.037f,1064.34f,38.2152f,
			-187.704f,1065.67f,38.184f,
			-187.037f,1066.34f,38.0744f,
			-187.704f,1067.67f,38.062f,
			-187.037f,1068.34f,37.9975f };
		u32 written = 0;
		written = pOutFile->WriteInt(&cNumElems, 1);
		written = pOutFile->WriteFloat(&test[0], cNumElems);

		Assert (written == cNumElems);

		pOutFile->Close();
	}
#endif

	fiStream* pFile = ASSET.Open(INPUT_TRI_POS_FILE,INPUT_TRI_POS_EXT);

	if (pFile == NULL)
	{
		return false;
	}

	int numElems = -1;
	int count = pFile->ReadInt(&numElems, 1);

	if (count == 0 || numElems < 0 || numElems > 1500 || numElems%3 != 0)
	{
		pFile->Close();
		return false;
	}

	m_triPositions.clear();


	float pos[3] = {0.0f,0.0f,0.0f};
	while (pFile->ReadFloat(&pos[0], 3) == 3)
	{		
		m_triPositions.PushAndGrow(Vector3(pos[0],pos[1],pos[2]));
	}

	pFile->Close();
	pFile = NULL;

	return true;

}

//
//
//
//
bool CPlantMgrDataEditor::WriteOutputDataFile()
{
	const char*  OUTPUT_TRI_DATA_FILE		= "x:/procdataOut";
	const char*	 OUTPUT_TRI_DATA_FILE_EXT	= "bin";

	fiStream* pOutFile = ASSET.Create(OUTPUT_TRI_DATA_FILE, OUTPUT_TRI_DATA_FILE_EXT);
	if (pOutFile == NULL)
	{
		return false;
	}

	const int nNumTris = m_selectedEntries.GetCount();
	u32 written = 0;
	written = pOutFile->WriteInt(&nNumTris, 1);
	Assert(written == 1);

	if(nNumTris>0)
	{
		// total: 36+9+3+3+3+2 = 56 bytes per tri:
		for (int i=0; i<nNumTris; i++) 
		{
			CPlantLocTri* pLocTri = m_selectedEntries[i];

			// 3x position (3*float (XYZ) = 9 floats/36 bytes):
			written = pOutFile->WriteFloat((const float*)&pLocTri->m_V1.x, 3);
			Assert(written == 3);
			written = pOutFile->WriteFloat((const float*)&pLocTri->m_V2.x, 3);
			Assert(written == 3);
			written = pOutFile->WriteFloat((const float*)&pLocTri->m_V3.x, 3);
			Assert(written == 3);

			// 3x RGB ground color data (3*3 bytes = 9 bytes):
			u8 rgb1[3], rgb2[3], rgb3[3];
			rgb1[0] = pLocTri->m_GroundColorV1.GetRed();
			rgb1[1] = pLocTri->m_GroundColorV1.GetGreen();
			rgb1[2] = pLocTri->m_GroundColorV1.GetBlue();
			rgb2[0] = pLocTri->m_GroundColorV2.GetRed();
			rgb2[1] = pLocTri->m_GroundColorV2.GetGreen();
			rgb2[2] = pLocTri->m_GroundColorV2.GetBlue();
			rgb3[0] = pLocTri->m_GroundColorV3.GetRed();
			rgb3[1] = pLocTri->m_GroundColorV3.GetGreen();
			rgb3[2] = pLocTri->m_GroundColorV3.GetBlue();
			written = pOutFile->WriteByte((const char*)&rgb1[0], 3);
			Assert(written == 3);
			written = pOutFile->WriteByte((const char*)&rgb2[0], 3);
			Assert(written == 3);
			written = pOutFile->WriteByte((const char*)&rgb3[0], 3);
			Assert(written == 3);

			// 3x ScaleXYZ data [0-15] (3 bytes):
			u8 scaleXYZ[3];
			scaleXYZ[0] = CPlantLocTri::pv8UnpackScaleXYZ(pLocTri->m_GroundColorV1.GetAlpha());
			scaleXYZ[1] = CPlantLocTri::pv8UnpackScaleXYZ(pLocTri->m_GroundColorV2.GetAlpha());
			scaleXYZ[2] = CPlantLocTri::pv8UnpackScaleXYZ(pLocTri->m_GroundColorV3.GetAlpha());
			written = pOutFile->WriteByte((const char*)&scaleXYZ[0], 3);
			Assert(written == 3);

			// 3x ScaleZ data [0-3] (3 bytes):
			u8 scaleZ[3];
			scaleZ[0] = CPlantLocTri::pv8UnpackScaleZ(pLocTri->m_GroundColorV1.GetAlpha());
			scaleZ[1] = CPlantLocTri::pv8UnpackScaleZ(pLocTri->m_GroundColorV2.GetAlpha());
			scaleZ[2] = CPlantLocTri::pv8UnpackScaleZ(pLocTri->m_GroundColorV3.GetAlpha());
			written = pOutFile->WriteByte((const char*)&scaleZ[0], 3);
			Assert(written == 3);

			// 3x Density data [0-3] (3 bytes):
			u8 density[3];
			density[0] = CPlantLocTri::pv8UnpackDensity(pLocTri->m_GroundColorV1.GetAlpha());
			density[1] = CPlantLocTri::pv8UnpackDensity(pLocTri->m_GroundColorV2.GetAlpha());
			density[2] = CPlantLocTri::pv8UnpackDensity(pLocTri->m_GroundColorV3.GetAlpha());
			written = pOutFile->WriteByte((const char*)&density[0], 3);
			Assert(written == 3);

			// control data (0xBEEF) (2 bytes):
			u8 controlData[2];
			controlData[0] = 0xBE;
			controlData[1] = 0xEF;
			written = pOutFile->WriteByte((const char*)&controlData[0], 2);
			Assert(written == 2);
		}
	}

	pOutFile->Close();
	pOutFile = NULL;

	return true;
}

void CPlantMgrDataEditor::cbBankToggleAllowPicking()
{
	gbShowCPlantMgrPolys = gPlantMgrEditor.m_bAllowPicking && gPlantMgrEditor.m_bShowPlantPolys;

	if(gPlantMgrEditor.m_bAllowPicking)
	{
		for(s32 i=0; i<MAX_PROCEDURAL_TAGS; i++)
		{
			gPlantMgrEditor.m_tagNames[i] = (const char*)g_procInfo.m_procTagTable[i].GetName();
			if(gPlantMgrEditor.m_tagNames[i][0]==0)
			{
				gPlantMgrEditor.m_tagNames[i] = "n/a";
			}
			
			gPlantMgrEditor.m_tagNamesCombo->SetString(i, gPlantMgrEditor.m_tagNames[i]);
		}
	}
	else
	{
		if(gPlantMgrEditor.m_bEdAllCollisionSelectable)
		{
			gPlantMgrEditor.m_bEdAllCollisionSelectable = false;
			cbBankToggleSelectAllCollision();
		}
	}
}

void CPlantMgrDataEditor::cbBankToggleShowPlantPolys()
{
	gbShowCPlantMgrPolys = gPlantMgrEditor.m_bAllowPicking && gPlantMgrEditor.m_bShowPlantPolys;
}

void CPlantMgrDataEditor::cbBankToggleSelectAllCollision()
{
	gPlantMgrEditor.m_bEdRegenTriCache = true;
}

void CPlantMgrDataEditor::cbBankButtonCopyPlayerPosToSphereCenter()
{
	gPlantMgrEditor.m_searchPos = CGameWorld::FindLocalPlayerCoors();
}

void CPlantMgrDataEditor::cbBankButtonSearchSphere()
{
	gPlantMgrEditor.m_bSphereSearchPending = true;
}

void CPlantMgrDataEditor::cbBankButtonClearSelEntries()
{
	gPlantMgrEditor.m_bClearSelectedEntries = true;
}

void CPlantMgrDataEditor::cbBankTextEditEntryIdxChanged()
{
	gPlantMgrEditor.m_bUpdateCurEditEntry = true;
}

void CPlantMgrDataEditor::cbBankButtonTypeNameChanged()
{
#if 0
	s32& type = gPlantMgrEditor.m_editableEntry.m_nSurfaceType;

	if(type < PROCEDURAL_TAG_PLANTS_BEGIN)
		type = PROCEDURAL_TAG_PLANTS_BEGIN;
	else if(type > MAX_PROCEDURAL_TAGS)
		type = MAX_PROCEDURAL_TAGS;
#endif
}

void CPlantMgrDataEditor::cbBankButtonSetProcTypeToAll()
{
	gPlantMgrEditor.m_bSetProcTypeToAll = true;
}

void CPlantMgrDataEditor::cbBankButtonSaveCurEntry()
{
	gPlantMgrEditor.m_bSaveCurEditEntry = true;
}

void CPlantMgrDataEditor::cbBankButtonSaveBatch()
{ 
	gPlantMgrEditor.m_bMaxUiSavingChangesToBatch = true;
}

void CPlantMgrDataEditor::cbBankToggleFindClosestEntriesFromFile()
{
	if (gPlantMgrEditor.m_bMaxUiSphereSearchFromFilePending == false)
	{
		gPlantMgrEditor.m_bMaxUiSphereSearchFromFilePending = gPlantMgrEditor.ReadInputDataFile();
	}
}

void CPlantMgrDataEditor::cbBankToggleWriteSelectionToFile()
{
	if (gPlantMgrEditor.m_bMaxUiWritingSelectionToFile == false)
	{
		gPlantMgrEditor.m_bMaxUiWritingSelectionToFile = true;
		gPlantMgrEditor.WriteOutputDataFile();
		gPlantMgrEditor.m_bMaxUiWritingSelectionToFile = false;
	}
}
#endif // PLANTSMGR_DATA_EDITOR...

