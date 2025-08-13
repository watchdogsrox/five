// Title	:	Renderer.cpp
// Author	:	Richard Jobling
// Started	:	10/05/99

#include "renderer/Renderer.h"

// Rage headers
#include "bank/bank.h" 
#include "bank/bkmgr.h"
#include "fragment/instance.h"
#include "fragment/type.h"
#include "grcore/debugdraw.h"
#include "grcore/device.h"
#include "grcore/effect_config.h"
#include "grcore/indexbuffer.h"
#include "grcore/vertexbuffer.h"
#include "grcore/wrapper_gcm.h"
#include "grmodel/shaderfx.h" 
#include "grmodel/geometry.h"
#include "phbound/boundcomposite.h"
#include "profile/cputrace.h"
#include "profile/timebars.h"
#include "system/cache.h"
#include "system/nelem.h"
#include "system/param.h"
#include "system/task.h"
#include "system/xtl.h"
#include "grprofile/pix.h"
#include "system/system.h"
#include "system/taskscheduler.h"
#include "vector/geometry.h"

// Framework headers
#include "fwanimation/directorcomponentmotiontree.h"
#include "fwmaths/angle.h"
#include "fwmaths/vector.h"
#include "fwpheffects/ropemanager.h"
#include "fwrenderer/renderlistbuilder.h"
#include "fwrenderer/renderlistgroup.h"
#include "fwscene/world/EntityDesc.h"
#include "fwscene/search/SearchVolumes.h"
#include "fwscene/scan/Scan.h"
#include "vfx/ptfx/ptfxdebug.h"

// Game headers
#include "audio/northaudioengine.h"
#include "camera/CamInterface.h"
#include "camera/base/BaseCamera.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/viewports/ViewportManager.h"
#include "core/game.h"
#include "debug/blockview.h"
#include "debug/debugglobals.h"
#include "debug/debugscene.h"
#include "debug/vectormap.h" 
#include "debug/Rendering/DebugRendering.h"
#include "debug/Rendering/DebugLighting.h"
#include "debug/Rendering/DebugLights.h"
#include "debug/debugdraw/debugvolume.h"
#include "debug/textureviewer/textureviewer.h"
#include "game/clock.h"
#include "frontend/GolfTrail.h"
#include "frontend/VideoEditor/VideoEditorInterface.h"
#include "modelinfo/MLOModelInfo.h"
#include "modelinfo/TimeModelInfo.h"
#include "modelinfo/VehicleModelInfo.h"
#include "objects/Object.h"
#include "peds/Ped.h"
#include "peds/pedpopulation.h"
#include "peds/rendering/PedHeadshotManager.h"
#include "physics/GtaInst.h"
#include "renderer/occlusion.h"
#include "renderer/clip_stat.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/DrawLists/drawlistMgr.h"
#include "renderer/Entities/EntityBatchDrawHandler.h"
#include "renderer/Lights/AsyncLightOcclusionMgr.h"
#include "renderer/lights/lights.h"
#include "renderer/lights/TiledLighting.h"
#include "renderer/MeshBlendManager.h"
#include "renderer/OcclusionQueries.h"
#include "renderer/PlantsMgr.h"
#include "renderer/PlantsGrassRenderer.h"
#include "renderer/PostProcessFX.h" 
#include "renderer/RenderListBuilder.h"
#include "renderer/renderListGroup.h" 
#include "renderer/RenderSettings.h"
#include "renderer/RenderTargetMgr.h"
#include "renderer/rendertargets.h"
#include "renderer/renderThread.h"
#include "renderer/Water.h"
#include "renderer/TreeImposters.h"
#include "renderer/SpuPm/SpuPmMgr.h"
#include "renderer/SSAO.h"
#include "renderer/Entities/InstancedEntityRenderer.h"
#include "renderer/ApplyDamage.h"
#include "renderer/RenderPhases/RenderPhaseDefLight.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseDebugOverlay.h"
#include "renderer/RenderPhases/RenderPhaseEntitySelect.h"
#include "renderer/RenderPhases/RenderPhaseFX.h"
#include "renderer/RenderPhases/RenderPhaseHeightMap.h"
#include "renderer/RenderPhases/RenderPhaseLensDistortion.h"
#include "renderer/RenderPhases/RenderPhaseMirrorReflection.h"
#include "renderer/RenderPhases/RenderPhaseParaboloidShadows.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/RenderPhases/RenderPhaseStd.h"
#include "renderer/RenderPhases/RenderPhaseWater.h"
#include "renderer/RenderPhases/RenderPhaseWaterReflection.h"
#include "renderer/shadows/shadows.h"
#include "renderer/UI3DDrawManager.h"
#include "renderer/Util/ShmooFile.h"
#include "scene/debug/PostScanDebug.h"
#include "scene/Entity.h"
#include "scene/EntityBatch.h"
#include "scene/portals/FrustumDebug.h"
#include "scene/portals/portal.h"
#include "scene/portals/portalDebugHelper.h"
#include "scene/portals/portalInst.h"
#include "scene/portals/portalTracker.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "scene/Scene.h"
#include "scene/worldpoints.h"
#include "scene/world/GameScan.h"
#include "scene/world/gameWorld.h"
#include "scene/world/WorldDebugHelper.h"
#include "shaders/ShaderLib.h"
#include "shaders/CustomShaderEffectTint.h"
#include "shaders/CustomShaderEffectTree.h"
#include "shaders/CustomShaderEffectAnimUV.h"
#include "shaders/CustomShaderEffectCable.h"
#include "shaders/CustomShaderEffectGrass.h"
#include "shaders/CustomShaderEffectMirror.h"
#include "Shaders/ShaderEdit.h"
#include "streaming/streaming.h"
#include "streaming/streamingengine.h"
#include "streaming/streamingmodule.h"
#include "timeCycle/TimeCycle.h"
#include "timeCycle/TimeCycleAsyncQueryMgr.h"
#include "vehicles/Automobile.h" //needed so we can tell some SPU code about automobiles
#include "vehicles/VehicleLightAsyncMgr.h"
#include "vehicles/vehiclepopulation.h"
#include "vfx/misc/DistantLights.h"
#include "vfx/misc/Puddles.h"
#include "vfx/misc/TerrainGrid.h"
#include "vfx/VisualEffects.h"
#include "vfx/misc/LinearPiecewiseFog.h"

#if RSG_PC
#include "renderer/computeshader.h"
#endif

#include "../shader_source/Lighting/Shadows/cascadeshadows_common.fxh"
#include "../shader_source/Terrain/terrain_cb_switches.fxh"

#if __PPU
#include "cell/atomic.h"
#include <cell/spurs/task.h>
#include <cell/spurs/event_flag.h>
#elif __XENON
#include "tracerecording.h"
#endif

// make sure these match
#if __BANK
CompileTimeAssert(rmDebugMode == _GTA5_rmDebugMode);
#endif

PARAM(lastgen,"render without new nextgen effects");
PARAM(forcetwopasstrees,"force two pass tree rendering");
PARAM(modelnamesincapture,"show model names in GPAD/PIX");
PARAM(dontwaitforbubblefences,"dont wait for bubble fences");
NOSTRIP_PARAM(setUAVNull,"Set unused UAV slots to NULL");

RENDER_OPTIMISATIONS();

#define	STRIP_ASSERTS	0

#if	STRIP_ASSERTS 
#undef ASSERT_ONLY
#undef Assert
#undef AssertMsgf
#undef Assertf
#undef AssertVerify
#undef DebugAssert


#define ASSERT_ONLY(x)
#define Assert(x)
#define AssertMsgf(x,args)
#define Assertf(x,msg)
#define AssertVerify(x) (x)
#define DebugAssert(x)


#undef	__DEV
#define	__DEV	0
#undef	__FINAL
#define	__FINAL	1
#undef	__BANK
#define	__BANK	0

#endif

#if !__FINAL
#define DEBUG_RENDERER
#endif

RAGE_DEFINE_CHANNEL(render)
RAGE_DEFINE_SUBCHANNEL(render, drawlist)

void DebugEntity(CEntity* UNUSED_PARAM(pEntity),const char* UNUSED_PARAM(fmt), ...) {}

#if __BANK
int				g_show_prerendered=false;
bool			gLastGenMode=false;
bool			gLastGenModeState = false;

bool			CRenderer::ms_isSnowing = false;

bool			CRenderer::ms_bPlantsAllEnabled			= true;
bool			CRenderer::ms_bPlantsDisplayDebugInfo	= false;
bool			CRenderer::ms_bPlantsAlphaOverdraw		= false;
#endif //__BANK

bool			g_multipass_list_gen=true;
bool			g_show_streaming_on_vmap=false;
bool			g_SortRenderLists=true;
bool			g_SortTreeList=true;
bool			g_debug_heightmap=false;

int				g_alternativeViewport=-1;
TweakBankBool	b_OnlySwapTimedWhenOffscreen=true;
float			g_HighPriorityYaw=10.0f; //in degrees
TweakBankBool	g_HighPrioritySphere=false;
float			g_ScreenYawForPriorityConsideration=5.0f; //in degrees
bool			g_UseTwoPassTreeNormalFlipping = true;
bool			g_DoubleSidedTreesCutoutOptimization = true;
bool			g_UseTwoPassTreeRendering = false;
bool			g_UseTwoPassTreeShadowRendering = false;
TweakBankBool	g_UseDelayedGbuffer2TileResolve = true;

#if __BANK
bool			g_HideDebug=false;
bool			g_DrawCloth=true;
bool			g_DrawPeds=true;
bool			g_DrawVehicles=true;
bool			g_EnableSkipRenderingShaders = false;
bool			g_SkipRenderingGrassFur = false;
bool			g_SkipRenderingTrees = false;

#if RSG_DURANGO
bool			g_DisplayFrameStatistics = true;
namespace rage {
extern DXGIX_FRAME_STATISTICS g_FrameStatistics[16];
}
#endif // RSG_DURANGO
#endif // __BANK

#if	__PPU
int				dma_planes_mem[MAX_OCCLUSION_PHASES][MAX_SPU_PLANES_MEM] ALIGNED(128);
short int		dma_planes_mem_count[MAX_OCCLUSION_PHASES] ALIGNED(16);
#endif

#if __DEV

namespace DebugStreaming 
{
	struct  DebugStreamingItem
	{
		CEntity *m_pEntity;
		s16	m_Counter;
		s16	m_Dist;	
		char	*m_String;
	};

	static const int MAX_DEBUG_STREAMING_ARRAY=256;

	static	DebugStreamingItem m_Array[MAX_DEBUG_STREAMING_ARRAY];
	static	int		m_Next=0;

	void	AddEntity(CEntity *pEntity,s16 counter,float dist,char *pstring=NULL)
	{
		if(pstring==NULL)
		{
			for(int i=0;i<MAX_DEBUG_STREAMING_ARRAY;i++)
			{
				if(m_Array[i].m_pEntity==pEntity)
				{
					m_Array[i].m_Counter=counter;
					return;
				}
			}
		}

		m_Array[m_Next].m_pEntity=pEntity;
		m_Array[m_Next].m_Dist=(s16)(dist*10.0f);
		m_Array[m_Next].m_Counter=counter;
		m_Array[m_Next].m_String=pstring;
		m_Next++;
		m_Next%=MAX_DEBUG_STREAMING_ARRAY;
	}

	void	DrawAll(void)
	{
		for(int i=0;i<MAX_DEBUG_STREAMING_ARRAY;i++)
		{
			if(m_Array[i].m_Counter>0)
			{
				--m_Array[i].m_Counter;

				char str[256];
				Vector3 pos= VEC3V_TO_VECTOR3(m_Array[i].m_pEntity->GetTransform().GetPosition());
				float ldist=((float)m_Array[i].m_Dist)*0.1f;
				float	fade=1.0f;//Min(((float)m_Array[i].m_Counter+10.0f)/20.0f,1.0f);

				Color32	fadecol=Color32(1.0f,1.0f,1.0f,fade);
				if(m_Array[i].m_String)
				{
					grcDebugDraw::Text(pos+Vector3(0.f,0.f,0.6f),fadecol,m_Array[i].m_String);
					grcDebugDraw::AddDebugOutput(fadecol,m_Array[i].m_String);

				}
				else
				{
					sprintf(str,"%s  d %3.1f/%3u",m_Array[i].m_pEntity->GetModelName(),ldist,m_Array[i].m_pEntity->GetLodDistance());
					grcDebugDraw::Text(pos+Vector3(0.f,0.f,0.6f),fadecol,str);
				}

			}
		}
	}
}
void	AddEntity(CEntity *pEntity,s16 counter,float dist,char *pstring)
{
	DebugStreaming::AddEntity(pEntity,counter,dist,pstring);
}
#endif


//
//
// 360 GPU GPR allocations
#if __XENON
BankInt32 CGPRCounts::ms_DefaultPsRegs			= 64;
BankInt32 CGPRCounts::ms_CascadedGenPsRegs		= 32; // Haven't really found a place where it matters what we set this to!
BankInt32 CGPRCounts::ms_CascadedRevealPsRegs	= 96; // Was 64, setting to 112 saves 0.01ms
BankInt32 CGPRCounts::ms_EnvReflectGenPsRegs	= 92; // Was 64, 110 saves ~0.11ms for parab, 92 saves 0.03ms for cubemaps
BankInt32 CGPRCounts::ms_EnvReflectBlurPsRegs	= 96;
BankInt32 CGPRCounts::ms_GBufferPsRegs			= 64;
BankInt32 CGPRCounts::ms_WaterFogPrepassRegs	= 112;
BankInt32 CGPRCounts::ms_WaterCompositePsRegs	= 112;
BankInt32 CGPRCounts::ms_DefLightingPsRegs		= 112; // Was 92, 112 saves 0.192ms in directional+foliage+skin 
BankInt32 CGPRCounts::ms_DepthFXPsRegs			= 112;
BankInt32 CGPRCounts::ms_PostFXPsRegs			= 112;

#endif // __XENON

//
//
// CRenderer static variables

Vector3 CRenderer::ms_PrevCameraPosition;
Vector3 CRenderer::ms_ThisCameraPosition;
float   CRenderer::ms_PreStreamDistance=RENDER_PRESTREAMDIST;
float   CRenderer::ms_PrevCameraVelocity=0.0f;
int		CRenderer::m_SceneToGBufferBits=0;
int		CRenderer::m_MirrorBits=0;
int		CRenderer::m_ShadowPhases=0;
#if __BANK
int		CRenderer::m_DebugPhases=0;
#endif // __BANK
u32		CRenderer::ms_canRenderAlpha=0;
u32		CRenderer::ms_canRenderWater=0;
u32		CRenderer::ms_canRenderScreenDoor=0;
u32		CRenderer::ms_canRenderDecal=0;
u32		CRenderer::ms_canPreRender=0;
float	CRenderer::ms_fCameraHeading;
float	CRenderer::ms_fFarClipPlane;
bool	CRenderer::ms_bInTheSky = false;
#if !__FINAL
bool	CRenderer::sm_bRenderCollisionGeometry = false;
#endif

#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES
int CRenderer::ms_WheelTessellationFactor = 1;
int CRenderer::ms_TreeTessellationFactor = 1;
float CRenderer::ms_TreeTessellation_PNTriangle_K = 0.0f;

float CRenderer::ms_LinearNear = 1.0f;
float CRenderer::ms_LinearFar = 10.0f;
// 0.1% of 1280 screen is about 1-2 pixels.

float CRenderer::ms_ScreenSpaceErrorInPixels = 1.0f;
int CRenderer::ms_MaxTessellation = 16;
float CRenderer::ms_FrustumEpsilon = 0.1f; // Todo - Use height scalar
float CRenderer::ms_BackFaceEpsilon = 0.725f;
bool CRenderer::ms_CullEnable = true;


# if !__FINAL
// Remember to change the default in common.fxh as well
bool CRenderer::ms_DisablePOM = false;
bool CRenderer::m_POMDistanceFade = true;
bool CRenderer::ms_VisPOMDistanceFade = false;
bool CRenderer::ms_VisPOMVertexHeight= false;

float CRenderer::ms_POMNear = 0.0f;
float CRenderer::ms_POMFar = 20.0f;
float CRenderer::ms_POMForwardOffset = 8.0f;

float CRenderer::ms_POMMinSamp = 3.0f;
float CRenderer::ms_POMMaxSamp = 27.0f;

grcEffectGlobalVar CRenderer::ms_POMValuesVar = grcegvNONE;
grcEffectGlobalVar CRenderer::ms_POMValues2Var = grcegvNONE;
# endif // !__FINAL
//---------------------------------------

float CRenderer::ms_GlobalHeightScale = 1.0f;

CRenderer::TessellationVars CRenderer::ms_TessellationVars;

#if !__FINAL || RSG_PC
grcEffectGlobalVar CRenderer::ms_POMFlagsVar = grcegvNONE;
#endif

grcEffectGlobalVar CRenderer::ms_GlobalHeightScaleVar;
#endif //RAGE_SUPPORT_TESSELLATION_TECHNIQUES

# if !__FINAL
// Remember to change the default in common.fxh as well
float CRenderer::ms_TerrainTintBlendNear = 70.0f;
float CRenderer::ms_TerrainTintBlendFar = 90.0f;

grcEffectGlobalVar CRenderer::ms_TerrainTintBlendValuesVar;
# endif // !__FINAL

// --- Global variables ---------------------------------------------------------------------------------------

#if DEBUG_DRAW && __BANK
static u32 gAddEntityToRenderListDebugModel = 0;
static u32 gSetupMapEntityVisibilityDebugModel = 0;
#endif // DEBUG_DRAW && __BANK

#if __BANK

#ifdef DEBUG_RENDERER
// model indices to debug
static char gAddEntityToRenderListDebugModelName[STORE_NAME_LENGTH] = "";
static char gSetupMapEntityVisibilityDebugModelName[STORE_NAME_LENGTH] = "";
#endif // DEBUG_RENDERER
#endif // __BANK

#if __BANK
extern  bool g_AddModelGcmMarkers;
#endif

// --- CRenderer --------------------------------------------------------------------------------------------

#if __XENON
int CRenderer::sm_backbufferColorExpBiasHDR=0;
int CRenderer::sm_backbufferColorExpBiasLDR=0;
#endif

bool CRenderer::sm_disableArtificialLights		= false;
bool CRenderer::sm_disableArtificialVehLights	= true;	// BS#6497322: if false, then do not disable vehicle lights during the EMP

//
// name:		CRenderer::PreLoadingScreensInit
// description:	Yellow logo support ;)
//
void CRenderer::PreLoadingScreensInit()
{
	USE_MEMBUCKET(MEMBUCKET_RENDER);
	CRenderTargets::PreAllocateRenderTargetMemPool();
	CRenderTargets::AllocateRenderTargets();
}

//
// name:		CRenderer::Init
// description:	Initialise renderer structures
//
void CRenderer::Init(unsigned initMode)
{
	USE_MEMBUCKET(MEMBUCKET_RENDER);

	#if __BANK
		g_AddModelGcmMarkers = PARAM_modelnamesincapture.Get();
	#endif

	#if __BANK
	if ( PARAM_lastgen.Get() )
	{
		gLastGenModeState=true;
	}
	else
	{
		gLastGenModeState=false;
	}

#if RSG_PC
	if (PARAM_setUAVNull.Get())
	{
		grcEffect::sm_SetUAVNull = true;
	}
#endif // RSG_PC

	if ( PARAM_forcetwopasstrees.Get() )
	{
		g_UseTwoPassTreeRendering=true;
	}
	#endif

    if(initMode == INIT_CORE)
    {
		InstancedRenderManager::Instantiate();

		DeferredLighting::Init();
		#if TILED_LIGHTING_ENABLED
			CTiledLighting::Init();
		#endif

		#if __BANK
			CShaderEdit::Init(initMode);
			CDebugTextureViewerInterface::Init(initMode);
			CRenderPhaseDebugOverlayInterface::InitShaders();
			DebugRenderingMgr::Init();
		#endif // __BANK

		CRenderPhaseCascadeShadowsInterface::Init_(initMode);
		CRenderPhaseCascadeShadowsInterface::InitShaders();
		CRenderPhaseDrawSceneInterface::Init(initMode);

#if __PS3
		// This has to happen after cascaded shadows on the PS3
		CRenderTargets::GetDepthBufferQuarter()->AllocateMemoryFromPool();
		CRenderTargets::SetDepthBufferQuarterAsColor(((grcRenderTargetGCM*)CRenderTargets::GetDepthBufferQuarter())->CreatePrePatchedTarget(false));
	
		CRenderTargets::GetOffscreenDepthBuffer()->AllocateMemoryFromPool();
		CRenderTargets::SetOffscreenDepthBufferAsColor(((grcRenderTargetGCM*)CRenderTargets::GetOffscreenDepthBuffer())->CreatePrePatchedTarget(false));
		
#if USE_DEPTHBUFFER_ZCULL_SHARING
		// Force the offscreen depth buffer to share zcull memory with the main back buffer depth. If this fails, performance for volumetric lighting will be very bad (+5ms)
		grcRenderTargetGCM* pOffscreenDepth = static_cast<grcRenderTargetGCM*>( CRenderTargets::GetOffscreenDepthBuffer() );
		if ( !pOffscreenDepth->ForceZCullSharing(static_cast<grcRenderTargetGCM*>(CRenderTargets::GetDepthBuffer())) )
		{
			Assertf(false,"Unable to force zcull sharing for OffscreenDepth, this will destroy volumetric lighting performance!");
		}
#endif // USE_DEPTHBUFFER_ZCULL_SHARING
#endif // __PS3

		CParaboloidShadow::Init();
	    CParaboloidShadow::InitShaders("common:/shaders");

		SSAO::Init(initMode);
#if LINEAR_PIECEWISE_FOG
		LinearPiecewiseFog::Init();
#endif // LINEAR_PIECEWISE_FOG
#if TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
		CCustomShaderEffectTree::Init();
#endif // TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS

//		ENTITYSELECT_ONLY(CRenderPhaseEntitySelect::InitializeRenderingResources());

		g_RenderPhaseManager::Instantiate(40);

		CGolfTrailInterface::Init();

		CRenderPhaseReflection::InitClass();
	    CRenderPhaseMirrorReflection::InitClass();
		CRenderPhaseWaterReflection::InitClass();
	    RenderPhaseSeeThrough::InitClass();

		CGtaRenderListGroup::Init(INIT_CORE);

		CGameScan::Init();
		
		gDrawListMgr->SetDefaultSubBucketMask(GenerateSubBucketMask(RB_MODEL_DEFAULT));

#if __D3D11
		gCSGameManager.Init();
#endif

		InstancedRendererConfig::Init();

		GPU_VEHICLE_DAMAGE_ONLY(CVehicleDeformation::TexCacheInit(initMode);)
		GPU_VEHICLE_DAMAGE_ONLY(CApplyDamage::Init();)
    }
	else if(initMode == INIT_SESSION)
	{
		RenderPhaseSeeThrough::InitSession();
	}

	CGrassBatch::Init(initMode);
}

#if __BANK
void CRenderer::ToggleRenderCollisionGeometry(void)
{
	if(sm_bRenderCollisionGeometry)
	{
		sm_bRenderCollisionGeometry = false;
	}
	else
	{
		sm_bRenderCollisionGeometry = true;
	}
}
#endif

//
// name:		ShutdownLevel
// description:	Shutdown renderer module for this level
void CRenderer::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
		DeferredLighting::Shutdown();
#if TILED_LIGHTING_ENABLED
		CTiledLighting::Shutdown();
#endif
		SSAO::Shutdown(shutdownMode);

		ENTITYSELECT_ONLY(CRenderPhaseEntitySelect::ReleaseRenderingResources());

		CGolfTrailInterface::Shutdown();
#if __BANK
		CShaderEdit::Shutdown(shutdownMode);
		CDebugTextureViewerInterface::Shutdown(shutdownMode);
		DebugRenderingMgr::Shutdown();
#endif
        PostFX::Shutdown(shutdownMode);
		CRenderPhaseCascadeShadowsInterface::Terminate();
	    CParaboloidShadow::Terminate();

	    gRenderListGroup.Shutdown(SHUTDOWN_CORE);

#if __XENON || __PPU
	    CRenderTargets::FreeRenderTargetMemPool();
#endif

		g_RenderPhaseManager::Destroy();

#if __D3D11
		gCSGameManager.Shutdown();
#endif
		OcclusionQueries::Shutdown();

		InstancedRenderManager::Destroy();

		CGrassBatch::Shutdown();

		GPU_VEHICLE_DAMAGE_ONLY(CVehicleDeformation::TexCacheShutdown(shutdownMode);)
		GPU_VEHICLE_DAMAGE_ONLY(CApplyDamage::ShutDown();)
	}
    else if(shutdownMode == SHUTDOWN_SESSION)
    {
	    ClearRenderLists();

		PostFX::Shutdown(shutdownMode);
    }
}

#if __BANK 

#define TOO_MANY_SHADERS 20

int PrintAndCountShaders(fwRenderListDesc& renderList, RenderPassId id)
{
	int numAlphaShaders = 0;

	// Count standard entity shaders
	for(s32 i = 0; i < renderList.GetNumberOfEntities(id); i++)
	{
		rmcDrawable* pDrawable = ((CEntity *) renderList.GetEntity(i, id))->GetDrawable();
		if(pDrawable && pDrawable->GetLodGroup().ContainsLod(LOD_HIGH))
		{
			s32 num = pDrawable->GetLodGroup().GetLodModel0(LOD_HIGH).GetGeometryCount();
			if(num > TOO_MANY_SHADERS)
			{
				grcDebugDraw::AddDebugOutput("%s has %d shaders\n", 
					renderList.GetEntity(i, id)->GetModelName(), num);
			}
			numAlphaShaders += num;
		}
	}

	return numAlphaShaders;
}
//
// name:		CRenderer::PrintNumShadersRendered
// description:	
void CRenderer::PrintNumShadersRendered(s32 list, const char *DEV_ONLY(phaseName))
{
#define TOO_MANY_SHADERS 20
	s32 num;
	s32 numShaders = 0;
	s32 numLodShaders = 0;
	s32 numAlphaShaders = 0;
	fwRenderListDesc& renderList = gRenderListGroup.GetRenderListForPhase(list);

	// Count standard entity shaders
	for(s32 i = 0; i < renderList.GetNumberOfEntities(RPASS_VISIBLE); i++)
	{
		rmcDrawable* pDrawable = ((CEntity *) renderList.GetEntity(i, RPASS_VISIBLE))->GetDrawable();
		if(pDrawable && pDrawable->GetLodGroup().ContainsLod(LOD_HIGH))
		{
			num = pDrawable->GetLodGroup().GetLodModel0(LOD_HIGH).GetGeometryCount();
			if(num > TOO_MANY_SHADERS)
			{
				grcDebugDraw::AddDebugOutput("%s has %d shaders\n", 
					renderList.GetEntity(i, RPASS_VISIBLE)->GetModelName(), num);
			}
			numShaders += num;
		}
	}
	// Count lod entity shaders
	for(s32 i = 0; i < renderList.GetNumberOfEntities(RPASS_LOD); i++)
	{
		rmcDrawable* pDrawable = ((CEntity *) renderList.GetEntity(i, RPASS_LOD))->GetDrawable();
		if(pDrawable && pDrawable->GetLodGroup().ContainsLod(LOD_HIGH))
		{
			num = pDrawable->GetLodGroup().GetLodModel0(LOD_HIGH).GetGeometryCount();
			if(num > TOO_MANY_SHADERS)
			{
				grcDebugDraw::AddDebugOutput("%s has %d shaders\n", 
					renderList.GetEntity(i, RPASS_LOD)->GetModelName(), num);
			}
			numLodShaders += num;
		}
	}

	//decal too?
	numAlphaShaders += PrintAndCountShaders(renderList, RPASS_ALPHA);
	numAlphaShaders += PrintAndCountShaders(renderList, RPASS_WATER);
	numAlphaShaders += PrintAndCountShaders(renderList, RPASS_FADING);

#if __XENON
	numAlphaShaders += PrintAndCountShaders(renderList, RPASS_TREE);
#endif //__XENON

#if __DEV
	grcDebugDraw::AddDebugOutput("%s %d: shaders %d, lods %d, alpha %d, inst %d, trees %d", 
		phaseName, list, 
		numShaders, numLodShaders, numAlphaShaders, 0,0);//numInstanceShaders, numTreeShaders);
#endif //__DEV
}

#if RSG_DURANGO


void CRenderer::PrintFrameStatistic()
{
	if( g_DisplayFrameStatistics == false )
		return;

	const int statsStartX = 190;
	const int statsStartY = 20;	
	
	int y = statsStartY;

	int queuedFrames = 0;
	u64 presentToQueue = 0;
	u64 queueToGpuDone = 0;
	u64 queueToDisplay = 0;
	u64 GpuDoneToDisplay = 0;

	for(int i=0;i<16;i++)
	{
		DXGIX_FRAME_STATISTICS *frameStats = &g_FrameStatistics[i];
		if( frameStats->VSyncCount == 0 )
		{
			queuedFrames++;
		}
		else
		{
			presentToQueue += (frameStats->CPUTimeAddedToQueue - frameStats->CPUTimePresentCalled);
			queueToGpuDone += (frameStats->CPUTimeFrameComplete - frameStats->CPUTimeAddedToQueue);
			queueToDisplay += (frameStats->CPUTimeFlip - frameStats->CPUTimeAddedToQueue);
			GpuDoneToDisplay += (frameStats->CPUTimeFlip - frameStats->CPUTimeFrameComplete);

		}
	}

	char debugText[512];
	float t2ms = sysTimer::GetTicksToMilliseconds();

	sprintf(debugText,"Queued Frames : %d",queuedFrames);
	grcDebugDraw::PrintToScreenCoors(debugText,statsStartX,y);
	y++;

	sprintf(debugText,"MS P2Q : %f",float(presentToQueue/16)*t2ms);
	grcDebugDraw::PrintToScreenCoors(debugText,statsStartX,y);
	y++;

	sprintf(debugText,"MS Q2G : %f",float(queueToGpuDone/16)*t2ms);
	grcDebugDraw::PrintToScreenCoors(debugText,statsStartX,y);
	y++;
			 
	sprintf(debugText,"MS Q2D : %f",float(queueToDisplay/16)*t2ms);
	grcDebugDraw::PrintToScreenCoors(debugText,statsStartX,y);
	y++;

	sprintf(debugText,"MS G2D %f",float(GpuDoneToDisplay/16)*t2ms);
	grcDebugDraw::PrintToScreenCoors(debugText,statsStartX,y);
	y++;

#if 0 // enable to see the matrix
	for(int i=0;i<16;i++)
	{
		sprintf(debugText,"%u %u %u %u %u %u %u %u %u %u",
		g_FrameStatistics[i].CPUTimePresentCalled,
		g_FrameStatistics[i].CPUTimeAddedToQueue,
		g_FrameStatistics[i].QueueLengthAddedToQueue,
		g_FrameStatistics[i].CPUTimeFrameComplete,
		g_FrameStatistics[i].GPUTimeFrameComplete,
		g_FrameStatistics[i].CPUTimeFlip,
		g_FrameStatistics[i].GPUTimeFlip,
		g_FrameStatistics[i].VSyncCount,
		g_FrameStatistics[i].VSyncCPUTime,
		g_FrameStatistics[i].VSyncGPUTime);

		grcDebugDraw::PrintToScreenCoors(debugText,statsStartX,y);
		y++;
	}
	y++;
#endif

}
#endif // RSG_DURANGO

#endif // __BANK

RAGETRACE_DECL(CRenderer_ConstructRenderList);
extern bool	g_render_lock;
//
// name:		ConstructRenderList
// description:	construct list of objects to render

// TODO: These need to be moved to RenderPhaseDefLight.cpp once the old render phase code disappears
/*dev_float*/	float MINIMUM_DISTANCE_FOR_PRESTREAM=5.0f;
/*dev_float*/	float MAX_DISTANCE_FOR_PRESTREAM=(80.0f-MINIMUM_DISTANCE_FOR_PRESTREAM);
/*dev_float*/	float MAX_SPEED_FOR_PRESTREAM=48.0f; // meters per second


void CRenderer::ConstructRenderListNew(u32 renderFlags, CRenderPhaseScanned *renderPhase)
{
	Assert(renderPhase->IsScanRenderPhase());
	Assert(renderPhase->HasEntityListIndex() && renderPhase->GetEntityListIndex()<MAX_NUM_RENDER_PHASES);
	
	if(g_render_lock)
		return;

	RAGETRACE_SCOPE(CRenderer_ConstructRenderList);

	u32 canRenderScreenDoor=0;
	u32 canRenderAlpha=0;
	u32 canRenderWater=0;
	u32 canRenderDecal=0;
	u32 canPreRender=0;

	s32 list = renderPhase->GetEntityListIndex();
	u32 listBitMask = 1 << list;

	if(renderFlags & RENDER_SETTING_RENDER_ALPHA_POLYS)
	{
		canRenderAlpha		|= listBitMask;
	}
	if(renderFlags & RENDER_SETTING_RENDER_WATER)
	{
		canRenderWater		|= listBitMask;
	}
	if(renderFlags & RENDER_SETTING_RENDER_CUTOUT_POLYS)
	{
		canRenderScreenDoor	|= listBitMask;
	}
	if(renderFlags & RENDER_SETTING_RENDER_DECAL_POLYS)
	{
		canRenderDecal		|= listBitMask;
	}
	if(renderFlags & RENDER_SETTING_ALLOW_PRERENDER)
	{
		canPreRender		|= listBitMask;
	}

	if (CDrawListMgr::IsShadowDrawList(renderPhase->GetDrawListType()))
	{
		CRenderer::OrInShadowPhases(listBitMask);
	}
	else if (CDrawListMgr::IsGBufDrawList(renderPhase->GetDrawListType()))
	{
		CRenderer::SetSceneToGBufferListBits(listBitMask);
	}
	else if (CDrawListMgr::IsMirrorReflectionDrawList(renderPhase->GetDrawListType()))
	{
		CRenderer::SetMirrorListBits(listBitMask);
	}
#if __BANK
	else if (CDrawListMgr::IsDebugDrawList(renderPhase->GetDrawListType()))
	{
		CRenderer::OrInDebugPhases(listBitMask);
	}
#endif // __BANK

	CRenderer::OrCanRenderFlags(canRenderAlpha,canRenderWater,canRenderScreenDoor,canRenderDecal,canPreRender);

	ms_bInTheSky = false;

	gRenderListGroup.GetRenderListForPhase(list).InitRenderListBuild();

//	TODO Assert(pRenderPhase->GetPhaseId()<MAX_PHASE_FLAGS);

	renderPhase->ConstructRenderList();
}


//////////////////////////////////////////////////////////////////////
// FUNCTION: PreRender
// PURPOSE:  Does the stuff that needs to be done for each visible entity
//           outside of the rendering loop.
//////////////////////////////////////////////////////////////////////
void CRenderer::PreRender()
{	
	PF_AUTO_PUSH_DETAIL("PreRender");

	CParaboloidShadow::PreRender();

	SSAO::Update();

	UI3DDRAWMANAGER.Update();

#if __BANK

#if RSG_DURANGO
	CRenderer::PrintFrameStatistic();
#endif // RSG_DURANGO

	CRenderTargets::UpdateBank(false);

#if __DEV
	CDebugScene::PreRender();
#endif // __DEV

#endif //__BANK

#if defined(GTA_REPLAY) && GTA_REPLAY
	CVideoEditorInterface::UpdatePreRender();
#endif // defined(GTA_REPLAY) && GTA_REPLAY

}

void CRenderer::PreRenderEntities()
{
	PF_AUTO_PUSH_TIMEBAR_BUDGETED("PreRenderEntities",2.0f);

	PF_PUSH_TIMEBAR("CVehicle : CalcSirenPriorities");
	CVehicle::CalcSirenPriorities();
	PF_POP_TIMEBAR();
	
	PF_PUSH_TIMEBAR("CWeaponComponentFlashLight : CalcFlashlightPriorites");
	CWeaponComponentFlashLight::CalcFlashlightPriorities();
	PF_POP_TIMEBAR();

	PF_PUSH_TIMEBAR("Wait : PreRenderEntities");
	CTaskScheduler::WaitOnResults();
	PF_POP_TIMEBAR();

	PF_PUSH_TIMEBAR( "PreRender2" );

	// Kick off the second part of the plant mgr update
	gPlantMgr.AsyncMemcpyBegin();

	// This needs to be called before PreRender2, which will start kicking off vehicle light async jobs
	CVehicleLightAsyncMgr::SetupDataForAsyncJobs();

	fwEntityPreRenderList	&preRender2List = gRenderListGroup.GetPreRender2List();
	for (int i = 0; i < preRender2List.GetCount(); ++i)
	{
		CDynamicEntity		*pEntity = static_cast< CDynamicEntity* >( preRender2List.GetEntity(i) );
		Assert( pEntity && pEntity->GetIsDynamic() );

		// Visibility scanning is completely done by now, so GetIsVisibleInSomeViewportThisFrame() 
		// should return the correct visibility for the main viewport for this dynamic entity this frame.
		const bool bIsEntityVisibleInMainViewport = pEntity->GetIsVisibleInSomeViewportThisFrame();
		pEntity->PreRender2(bIsEntityVisibleInMainViewport);
		
		BANK_ONLY( CPostScanDebug::GetNumModelsPreRendered2Ref() += 1; )
	}
	PF_POP_TIMEBAR();

	// Now that we're done with PreRender2, kick off a final job to handle any remaining vehicles that were queued
	CVehicleLightAsyncMgr::KickJobForRemainingVehicles();
	
	// NEED to process 3DUI and headshot peds here so they get registered with the damage system (they don't seem to be in the preRender2List)
#if __BANK
	UI3DDRAWMANAGER.UpdateDebug();
#endif
	PedHeadshotManager::GetInstance().PreRender();
	UI3DDrawManager::GetInstance().PreRender();

	CAsyncLightOcclusionMgr::StartAsyncProcessing();

	PF_PUSH_TIMEBAR("GameWorld Process After PreRender");
	gVpMan.PushViewport(gVpMan.GetGameViewport());
	{
		// Process post-pre-render activities
		CGameWorld::ProcessAfterPreRender();
	}
	gVpMan.PopViewport();
	PF_POP_TIMEBAR();

	// NOTE: character cloth should be here because animation expressions are waited on inside pre-render ( i.e. cloth update has to be after expressions have finished )
	if( !fwTimer::IsGamePaused() )
	{
		CPhysics::RopeSimUpdate( fwTimer::GetTimeStep(), ROPE_UPDATE_LATE );
	}

	CPhysics::GetClothManager()->SwapBuffers();
	CPhysics::VerletSimUpdate( clothManager::Character );

	// Timecycle async query needs to finish before Visual Effects Update.
	CTimeCycleAsyncQueryMgr::WaitForAsyncProcessingToFinish();

	PF_PUSH_TIMEBAR("VisualEffects Update After PreRender");
	CVisualEffects::UpdateAfterPreRender();
	PF_POP_TIMEBAR();

	CAsyncLightOcclusionMgr::ProcessResults();
	
	CVehicleLightAsyncMgr::ProcessResults();

	CParaboloidShadow::UpdateAfterPreRender(); // this needs to be after all moving lights are added.

	g_SceneStreamerMgr.GetStreamingLists().WaitForSort();

	//Sorting lights after paraboloid shadows update
	PF_PUSH_TIMEBAR("Process Lights Visibility/Sort");
	Lights::ProcessVisibilitySortAsync();
	PF_POP_TIMEBAR();

	CLODLights::WaitForProcessVisibilityDependency();
	g_distantLights.WaitForProcessVisibilityDependency();
	gPlantMgr.AsyncMemcpyEnd();
}

RAGETRACE_DECL(CRenderer_PreRender);

bool g_debugNonEdgeShadows	=true;
bool g_debugEdgeShadows		=true;

extern	bool g_render_lock;
//
// name:		CRenderer::ClearRenderLists
// description:	Clear all the render lists 
void CRenderer::ClearRenderLists()
{
	if(g_render_lock)
		return;

	gRenderListGroup.Clear();
	CRenderer::SetSceneToGBufferListBits(0);
	CRenderer::SetMirrorListBits(0);
	CRenderer::SetShadowPhases(0);
#if __BANK
	CRenderer::SetDebugPhases(0);
#endif // __BANK
}


int CRenderer::GetRenderListVisibleCount(int list)
{
	fwRenderListDesc& renderList = gRenderListGroup.GetRenderListForPhase(list);
	return renderList.GetNumberOfEntities(RPASS_VISIBLE)+renderList.GetNumberOfEntities(RPASS_LOD);
}

#if __PS3
float CRenderer::SetDepthBoundsFromRange(float nearClip, float farClip, float nearZ, float farZ, bool excludeSky/*=true*/)
{
	Assert(nearZ<=farZ);
	Assert(nearClip<=farClip);
	const float linearMin = Clamp<float>(nearZ, nearClip, farClip);
	const float linearMax = Clamp<float>(farZ, nearClip, farClip);

	const float Q = farClip/(farClip - nearClip);

	const float sampleMin = (Q/linearMin)*(linearMin - nearClip);
	const float sampleMax = (Q/linearMax)*(linearMax - nearClip);

	if (excludeSky && (farClip == farZ))
	{
		// set it to the furthest possible bound before the sky

		// The furthest depth value that can be represented in fixed
		// point lones is 0xfffff8 (corresponds to znear 0xffff).
		// We want the depth bounds to therefore be the next closest
		// value representable in zcull, 0xfffe, which is depth
		// 0xfffff0.
		//
		// So that sounds great in theory... but that logic leads to
		// the float 0x3f7ffff0, which does not cull the sky.  By
		// pure trial and error, the largest float that will cause
		// the sky to get culled is 0x3f7ffeff.  Don't know where
		// that values comes from, but it works.
		//
		grcDevice::SetDepthBounds(sampleMin, 0x1.fffdfep-1);
	}
	else
	{
		grcDevice::SetDepthBounds(sampleMin, sampleMax);
	}

	return sampleMin; // return nearZ in depth sample space
}

#endif
#if __BANK

static void SetAddEntityToRenderListDebugModel()
{
	fwModelId  modelId;
	CModelInfo::GetBaseModelInfoFromName(gAddEntityToRenderListDebugModelName, &modelId);
	gAddEntityToRenderListDebugModel = modelId.GetModelIndex();
}

static void SetSetupMapEntityVisibilityDebugModel()
{
	fwModelId  modelId;
	CModelInfo::GetBaseModelInfoFromName(gSetupMapEntityVisibilityDebugModelName, &modelId);
	gSetupMapEntityVisibilityDebugModel = modelId.GetModelIndex();
}

static grcEffectGlobalVar g_AmbDirBiasVar = (grcEffectGlobalVar)0;
static Vec4f g_AmbDirBias(0.0f,0.0f,1.0f,0.5f);
void SetGlobalAmbDirBiasCB()
{
	grcEffect::SetGlobalVar(g_AmbDirBiasVar, g_AmbDirBias);
}

extern bool PostFX::g_lightRaysAboveWaterLine;
extern bool g_bRenderExteriorReflection;

static bool		g_prevDensityMultiplierSaved = false;
static float	g_prevOverrideVehDensityMultiplier;
static float	g_prevOverridePedDensityMultiplier;

void SetLastGenMode()
{
	if ( (gLastGenMode != gLastGenModeState) )
	{
		if (gLastGenModeState == true)
		{
			SSAO::Enable(false);
			PostFX::g_lightRaysAboveWaterLine = false;
			CWorldDebugHelper::SetEarlyRejectExtraDistCull(true);
			CWorldDebugHelper::SetEarlyRejectMaxDist(1000);
			CWorldDebugHelper::SetEarlyRejectMinDist(0);		

			g_bRenderExteriorReflection = false;

			extern CRenderSettings g_widgetRenderSettings;
			g_widgetRenderSettings.SetLODScale(0.65f);
			g_distantLights.SetRenderingEnabled(false);
		}
		else
		{
			SSAO::Enable(true);
			PostFX::g_lightRaysAboveWaterLine = true;
			CWorldDebugHelper::SetEarlyRejectExtraDistCull(false);

			g_bRenderExteriorReflection = true;

			extern CRenderSettings g_widgetRenderSettings;
			g_widgetRenderSettings.SetLODScale(1.0);
			g_distantLights.SetRenderingEnabled(true);
		}

		if (gLastGenModeState )
		{
			g_prevOverrideVehDensityMultiplier = CVehiclePopulation::GetDebugOverrideMultiplier();
			g_prevOverridePedDensityMultiplier = CPedPopulation::GetDebugOverrideMultiplier();
			g_prevDensityMultiplierSaved = true;

			PostFX::SetFXAA(false);
			ptfxDebug::SetDisablePtFx(true);

			CVehiclePopulation::SetDebugOverrideMultiplier( 0.4f );
			CPedPopulation::SetDebugOverrideMultiplier( 0.5f );
		}
		else
		{
#if !(__D3D11 || RSG_ORBIS) && 0
			PostFX::SetFXAA(!GRCDEVICE.GetMSAA());	// Re-enable only if we're not using MSAA
#endif // !(__D3D11 || RSG_ORBIS) 
			ptfxDebug::SetDisablePtFx(false);

			if ( g_prevDensityMultiplierSaved )
			{
				CVehiclePopulation::SetDebugOverrideMultiplier( g_prevOverrideVehDensityMultiplier );
				CPedPopulation::SetDebugOverrideMultiplier( g_prevOverridePedDensityMultiplier );
				g_prevDensityMultiplierSaved = false;
			}
		}

		gLastGenMode = gLastGenModeState;
	}
}

PARAM(rag_renderer,"rag_renderer");

#if DEVICE_EQAA
namespace rage {
#if RSG_ORBIS
	extern bool s_CopyByCompute;
	extern bool s_AllowGPUClear, s_AllowFastClearStencil;
	extern bool s_UseClearDbControl;
#endif
	namespace eqaa {
		extern int	s_ResolveHighQuality;
		extern bool	s_ResolveHighQualityS1;
	}
}
#endif //DEVICE_EQAA

void CRenderer::InitWidgets()
{
	// Create the renderer bank
	bkBank*		pBank = &BANKMGR.CreateBank("Renderer", 0, 0, false); 
	if(renderVerifyf(pBank, "Failed to create renderer bank"))
	{
		if (PARAM_rag_renderer.Get())
		{
			AddWidgets();
			PostFX::AddWidgetsOnDemand();
		}
		else
		{
			pBank->AddButton("Create Renderer widgets", datCallback(CFA1(CRenderer::AddWidgets), pBank));
		}
	}

	DebugLights::InitWidgets();
	DebugLighting::AddWidgets();
	CPortalDebugHelper::InitInteriorWidgets();

	ShmooHandling::AddWidgets(*pBank);

#if RSG_ORBIS
	{
		bkBank *pDebug = BANKMGR.FindBank("Debug");
		pDebug->PushGroup("Orbis computing");
			pDebug->AddToggle("Use CS RT copy", &rage::s_CopyByCompute,
				NullCallback, "Use compute shaders to copy render targets");
			pDebug->AddToggle("Allow GPU clear", &rage::s_AllowGPUClear,
				NullCallback, "Use compute shaders to clear buffers");
			pDebug->AddToggle("Use CS clear (stencil)", &rage::s_AllowFastClearStencil,
				NullCallback, "Use compute shaders to clear raw stencil data");
			pDebug->AddToggle("Use DbRenderControl", &rage::s_UseClearDbControl,
				NullCallback, "Use another fast-clear path for depth & stencil");
		pDebug->PopGroup();
	}
#endif //RSG_ORBIS
#if DEVICE_EQAA
	{
		bkBank *pDebug = BANKMGR.FindBank("Debug");
		pDebug->PushGroup("EQAA");
			pDebug->AddSlider("HQ resolve Mode", &rage::eqaa::s_ResolveHighQuality, -1, 1, 1);
			pDebug->AddToggle("Force HQ resolve for S/1", &rage::eqaa::s_ResolveHighQualityS1);
	#if AA_SAMPLE_DEBUG
			eqaa::Debug &ed = eqaa::GetDebug();
			pDebug->PushGroup("Custom Pattern");
				pDebug->AddToggle("Use Deprecated Mode", &ed.useDeprecatedPattern,
					NullCallback, "Use the previously written hand-made pattern for comparison.");
				pDebug->AddSlider("Radius", &ed.patternRadius, 0.f, 1.f, 0.1f);
				pDebug->AddSlider("Angle Offset", &ed.patternOffset, 0.f, 360.f, 1.f);
			pDebug->PopGroup();
			pDebug->PushGroup("Resolve Params");
				pDebug->AddSlider("Side Weight", &ed.resolveSideWeight, 0.f, 0.5f, 0.01f);
				pDebug->AddSlider("Depth Threshold", &ed.resolveDepthThreshold, 0.f, 1.f, 0.001f);
				pDebug->AddToggle("Skip Blending Edges", &ed.resolveSkipEdges);
			pDebug->PopGroup();
	#endif //AA_SAMPLE_DEBUG
	#if RSG_ORBIS
			eqaa::Control &ct = eqaa::GetControl();
			pDebug->PushGroup("Depth Control");
				pDebug->AddToggle("Incoherent Eqaa Reads", &ct.incoherentEqaaReads,
					NullCallback, "Enables the coherency check for abutting triangles that share anchor samples, but not detail samples.");
				pDebug->AddToggle("Static Anchor Associations", &ct.staticAnchorAssociations,
					NullCallback, "Forces replicated destination data to always come from the statically associated anchor sample.");
				pDebug->AddToggle("Interpolate Compressed Z", &ct.interpolateCompressedZ,
					NullCallback, "Allows unanchored samples to interpolate a unique Z from compressed Z Planes.");
				pDebug->AddToggle("High Quality Tile Intersections", &ct.highQualityTileIntersections,
					NullCallback, "Enables high-quality tile intersections.");
			pDebug->PopGroup();
	#endif //RSG_ORBIS
		pDebug->PopGroup();
	}
#endif //DEVICE_EQAA

	bkBank* pOptBank = BANKMGR.FindBank("Optimization");
	pOptBank->AddToggle("Draw Cloth",			&g_DrawCloth );
	pOptBank->AddToggle("Draw Peds",			&g_DrawPeds );
	pOptBank->AddToggle("Draw Vehicles",		&g_DrawVehicles );
	pOptBank->PushGroup("Skip Rendering Shaders");
		pOptBank->AddToggle("Enable",					&g_EnableSkipRenderingShaders);
		pOptBank->AddToggle("Disable fur grass shaders",&g_SkipRenderingGrassFur);
		pOptBank->AddToggle("Disable tree shaders",		&g_SkipRenderingTrees);
	pOptBank->PopGroup();

	CCustomShaderEffectCable::InitOptimisationWidgets(*pOptBank);
}

//		
// name:		CRenderer::AddWidgets
// description:	Add bank widgets for renderer class
extern	bank_bool g_show_water_clip;
extern	bank_bool g_cache_entities;
extern	bool g_prototype_batch;
namespace FPSPedDraw {
	extern BankBool sEnable3rdPersonSkel;
}

void CRenderer::AddWidgets()
{
	// destroy first widget which is the create button
	bkWidget* pWidget = BANKMGR.FindWidget("Renderer/Create Renderer widgets");
	if(pWidget)
	{
		pWidget->Destroy();
	}

	bkBank* pRendererBank = BANKMGR.FindBank("Renderer");
	Assert(pRendererBank);
	bkBank& bank = *pRendererBank;

	bank.AddToggle("LASTGEN", &gLastGenModeState);
	bank.AddToggle("Enable Snow", &ms_isSnowing);
	bank.PushGroup("Plants");
		bank.AddToggle("All Plants Enabled",	&ms_bPlantsAllEnabled,			PlantsToggleCB);
		bank.AddToggle("Display Debug Info",	&ms_bPlantsDisplayDebugInfo,	PlantsDisplayDebugInfoCB);
		bank.AddToggle("Alpha Overdraw",		&ms_bPlantsAlphaOverdraw,		PlantsAlphaOverDrawCB);
	bank.PopGroup();
	bank.AddToggle("Show clip water quads against frustum", &g_show_water_clip);
    bank.AddToggle("Cache entity draw data", &g_cache_entities);
	bank.AddToggle("Batch entity draw", &g_prototype_batch);
	bank.AddToggle("Add model names to GPAD/PIX captures", &g_AddModelGcmMarkers);

	CCustomShaderEffectAnimUV::InitWidgets(bank);
	CCustomShaderEffectCable::InitWidgets(bank);
	CCustomShaderEffectMirror::InitWidgets(bank);
#if __DEV
	extern void InitBypassWidgets(bkBank& bk);
	InitBypassWidgets(bank);
#endif // __DEV
#if __BANK
	extern void InitRenderWidgets(bkBank& bk);
	InitRenderWidgets(bank);
#endif

#if RSG_DURANGO
	bank.AddToggle("Display Frame Statistics", &g_DisplayFrameStatistics);
#endif // RSG_DURANGO
	bank.AddToggle("Disable Artificial Lights",&sm_disableArtificialLights);
	bank.AddSlider("DAL Emissive Scale",&CShaderLib::ms_overridesEmissiveScale,0.0f,1.0f,0.1f);
	bank.AddSlider("DAL Emissive Mult",&CShaderLib::ms_overridesEmissiveMult,0.0f,1.0f,0.1f);
	bank.AddSlider("DAL Art Int Amb Down",&CShaderLib::ms_overridesArtIntAmbDown,0.0f,255.0f,0.1f);
	bank.AddSlider("DAL Art Int Amb Base",&CShaderLib::ms_overridesArtIntAmbBase,0.0f,255.0f,0.1f);
	bank.AddSlider("DAL Art Ext Amb Down",&CShaderLib::ms_overridesArtExtAmbDown,0.0f,255.0f,0.1f);
	bank.AddSlider("DAL Art Ext Amb Base",&CShaderLib::ms_overridesArtExtAmbBase,0.0f,255.0f,0.1f);
	bank.AddSlider("DAL Art Amb Scale",&CShaderLib::ms_overridesArtAmbScale,0.0f,1.0f,0.1f);

#if GCM_HUD
	bank.PushGroup("Gcm Hud", false);
	{
		s_bGcmHudEnabled = grcCellGcmHudIsEnabled;
		bank.AddToggle("Enable Gcm Hud *BUGGED*", &s_bGcmHudEnabled, datCallback(CFA(ToggleGcmHud)), "Enable/Disable Gcm Hud");
		bank.AddToggle("Enable render phase tags", &g_bGcmHudRenderPhaseTags, NullCB, "Enable/Disable RenderPhase tagging");
	}
	bank.PopGroup();
#endif

	bank.PushGroup("Streaming High Priority settings", false);
	{
		bank.AddToggle("Show streaming on Vmap", &g_show_streaming_on_vmap,NullCB);
		bank.AddSlider("High Priority angle degrees", &g_HighPriorityYaw, 0.0f, 90.f, 1.0f ,NullCB,"Inner Cone around camera view vector considered to be high priority for streaming if an entities bounding sphere overlaps it");
		bank.AddToggle("Show High Priority Spheres", &g_HighPrioritySphere,NullCB," Show the bounding sphere for high priority objects");
		bank.AddSlider("Subtended Angle For High Priority Consideration", &g_ScreenYawForPriorityConsideration,0.0f,90.0f,0.5f,NullCB," Angle subtended by object (ie decreases with distance) required to be considered for HP");
	}
	bank.PopGroup();

	bank.PushGroup("Optimisations WIP", false);
	{
		bank.AddSlider("Show pre rendered", &g_show_prerendered,0,3,1,NullCB,"");
		bank.AddToggle("Hide Debug Objects", &g_HideDebug);
		bank.AddToggle("Use multipass List Processing", &g_multipass_list_gen);
		bank.AddToggle("Only Swap Timed objects When Offscreen", &b_OnlySwapTimedWhenOffscreen);
		bank.AddToggle("Lock Render List and Pause Game", &g_render_lock);
		bank.AddSlider("renderPhase to use for main render", &g_alternativeViewport, -1, (int)40, 1 );

		bank.AddToggle("Sort Lod and Visible Lists", &g_SortRenderLists);
		bank.AddToggle("Sort Tree List", &g_SortTreeList);
		bank.AddToggle("Use GTAIV two pass tree rendering ", &g_UseTwoPassTreeRendering );
		bank.AddToggle("Use GTAIV two pass tree normal flipping ", &g_UseTwoPassTreeNormalFlipping );
		bank.AddToggle("Use Single SetDoubleSided Call for trees cutout", &g_DoubleSidedTreesCutoutOptimization);

		bank.AddToggle("Use two pass shadow tree rendering ", &g_UseTwoPassTreeShadowRendering );
		bank.AddToggle("Use delayed Gbuffer2 tile resolve ", &g_UseDelayedGbuffer2TileResolve );
	}
	bank.PopGroup();

	InstancedRendererConfig::AddWidgets(bank);
	CEntityBatch::AddWidgets(bank);

#if __XENON
	bank.PushGroup("GPR Allocations", false);
	{
		bank.AddSlider("DefaultPsRegs",			&CGPRCounts::ms_DefaultPsRegs,			16, 112, 1);
		bank.AddSlider("CascadedGenPsRegs",		&CGPRCounts::ms_CascadedGenPsRegs,		16, 112, 1);
		bank.AddSlider("CascadedRevealPsRegs",	&CGPRCounts::ms_CascadedRevealPsRegs,	16, 112, 1);
		bank.AddSlider("EnvReflectGenPsRegs",	&CGPRCounts::ms_EnvReflectGenPsRegs,	16, 112, 1);
		bank.AddSlider("EnvReflectBlurPsRegs",	&CGPRCounts::ms_EnvReflectBlurPsRegs,	16, 112, 1);
		bank.AddSlider("GBufferPsRegs",			&CGPRCounts::ms_GBufferPsRegs,			16, 112, 1);
		bank.AddSlider("DefLightingPsRegs",		&CGPRCounts::ms_DefLightingPsRegs,		16, 112, 1);
		bank.AddSlider("WaterCompositePsRegs",	&CGPRCounts::ms_WaterCompositePsRegs,	16, 112, 1);
		bank.AddSlider("DepthFXPsRegs",			&CGPRCounts::ms_DepthFXPsRegs,			16, 112, 1);
		bank.AddSlider("PostFXPsRegs",			&CGPRCounts::ms_PostFXPsRegs,			16, 112, 1);
	}
	bank.PopGroup();
#endif // __XENON

	gVpMan.SetupRenderPhaseWidgets(bank);

	bank.PushGroup("Debug Info", false);
	{
		bank.AddButton("Collision geometry", datCallback(CFA(ToggleRenderCollisionGeometry)));
	}
	bank.PopGroup();
#ifdef DEBUG_RENDERER
	const u32 maxModelInfos = CModelInfo::GetMaxModelInfos();

	bank.PushGroup("Debug RenderList", false);
	{
		bank.AddText("AddEntityToRenderList", &gAddEntityToRenderListDebugModelName[0], 
			sizeof(gAddEntityToRenderListDebugModelName), false, &SetAddEntityToRenderListDebugModel);
		bank.AddSlider("AddEntityToRenderList", &gAddEntityToRenderListDebugModel, 0, maxModelInfos, 1);
		bank.AddText("SetupMapEntityVisibility", &gSetupMapEntityVisibilityDebugModelName[0], 
			sizeof(gSetupMapEntityVisibilityDebugModelName), false, &SetSetupMapEntityVisibilityDebugModel);
		bank.AddSlider("SetupMapEntityVisibility", &gSetupMapEntityVisibilityDebugModel, 0, maxModelInfos, 1);

		bank.AddToggle("Count models rendered", &CPostScanDebug::GetCountModelsRenderedRef() );
		bank.AddToggle("Count shaders rendered", &CPostScanDebug::GetCountShadersRenderedRef() );
	}
	bank.PopGroup();
#endif // DEBUG_RENDERER

	CFrustumDebug::AddWidgets(bank);

	CPlantMgr::InitWidgets(bank);

#if SPUPMMGR_ENABLED
	CSpuPmMgr::InitWidgets(bank);
#endif // SPUPMMGR_ENABLED

	DebugRenderingMgr::AddWidgets(&bank);
	SSAO::AddWidgets(bank);
	Lights::AddWidgets(bank);

	gDebugVolumeGlobals.AddWidgets(bank);
	gShadows.AddWidgets(bank);

	GOLFGREENGRID.AddWidgets(&bank);
	UI3DDRAWMANAGER.AddWidgets(&bank);

	PostFX::AddWidgets(bank);
	
	OcclusionQueries::AddWidgets(bank);

	gDrawListMgr->SetupWidgets(bank);
#if __XENON
	bank.AddSlider("HDR Color Exp Bias", &CRenderer::sm_backbufferColorExpBiasHDR, -4, 4, 1 );
	bank.AddSlider("LDR Color Exp Bias", &CRenderer::sm_backbufferColorExpBiasLDR, -4, 4, 1 );
#endif

#if __DEV
	CDebugScene::AddModelViewerBankWidgets(bank);
#endif // __DEV

	CRenderListBuilder::AddWidgets(bank);

	bkBank *pBank = BANKMGR.FindBank("Optimization");
	pBank->AddToggle("count models rendered", &CPostScanDebug::GetCountModelsRenderedRef() );
	pBank->AddToggle("count shaders rendered", &CPostScanDebug::GetCountShadersRenderedRef() );
	pBank->AddButton("collision geometry", datCallback(CFA(ToggleRenderCollisionGeometry)));

	CRenderTargets::AddWidgets(bank);

	COcclusion::InitWidgets();		
	CPortalDebugHelper::InitWidgets();			
	Water::InitWidgets();			 
	PuddlePassSingleton::InstanceRef().InitWidgets();
#if USE_TREE_IMPOSTERS
	CTreeImposters::InitWidgets();	
#endif // USE_TREE_IMPOSTERS
#if CSE_TREE_EDITABLEVALUES
	CCustomShaderEffectTree::InitWidgets(bank);
#endif

#if CSE_TINT_EDITABLEVALUES
	CCustomShaderEffectTint::InitWidgets(bank);
#endif

	g_ShaderEdit::InstanceRef().AddRendererWidgets();
	CRenderPhaseDebugOverlayInterface::AddWidgets();

	CRenderPhaseLensDistortion::InitWidgets();

	MESHBLENDMANAGER.AddWidgets(bank);

#if __D3D11 || RSG_ORBIS
	bank.PushGroup("Shader Model 5");
# if RAGE_SUPPORT_TESSELLATION_TECHNIQUES
	bank.PushGroup("POM / Displacement", false);

	bank.AddToggle("Disable POM", &ms_DisablePOM);
	bank.AddToggle("POM Distance Fade", &m_POMDistanceFade);
	bank.AddToggle("Visualise POM Distance Fade Settings", &ms_VisPOMDistanceFade);
	bank.AddToggle("Visualise POM Vertex Weights", &ms_VisPOMVertexHeight);
	bank.AddSlider("Near Fade", &ms_POMNear, 0.0f, 60.0f, 0.5f);
	bank.AddSlider("Far Fade", &ms_POMFar, 0.0f, 100.0f, 0.5f);
	bank.AddSlider("Forward Fade Offset", &ms_POMForwardOffset, 0.0f, 20.0f, 0.5f);
	bank.AddSlider("Min Sample", &ms_POMMinSamp, 0.0f, 20.0f, 1.0f);
	bank.AddSlider("Max Sample", &ms_POMMaxSamp, 0.0f, 50.0f, 1.0f);
	bank.AddSlider("Global Height Scale", &ms_GlobalHeightScale, 0.0f, 2.0f, 0.01f);
	bank.PopGroup();

	bank.PushGroup("Terrain Tint Blend", false);
	bank.AddSlider("Blend Start", &ms_TerrainTintBlendNear, 0.0f, 100.0f, 1.0f);
	bank.AddSlider("Blend finish", &ms_TerrainTintBlendFar, 10.0f, 200.0f, 1.0f);
	bank.PopGroup();

	bank.PushGroup("Tessellation", false);
	bank.AddSlider("Wheel Tessellation", &ms_WheelTessellationFactor, 1, 10, 1 );
	bank.AddSlider("Tree Tessellation", &ms_TreeTessellationFactor, 1, 10, 1 );
	bank.AddSlider("Tree K Factor", &ms_TreeTessellation_PNTriangle_K, 0.0f, 1.0f, 0.05f );

	bank.AddSlider("Tessellation - Near", &ms_LinearNear, 0.01f, 200.0f, 0.25f );
	bank.AddSlider("Tessellation - Far", &ms_LinearFar, 0.01f, 200.0f, 0.25f );
	bank.AddSlider("Tessellation - Allowed error in pixels", &ms_ScreenSpaceErrorInPixels, 0.001f, 100.0f, 0.001f );
	bank.AddSlider("Tessellation - Max", &ms_MaxTessellation, 1, 16, 1);

	bank.AddSlider("Tessellation - Frustum Epsilon", &ms_FrustumEpsilon, -1000.00f, 1000.0f, 0.001f );
	bank.AddSlider("Tessellation - BackFace Cull Epsilon", &ms_BackFaceEpsilon, -1.00f, 1.0f, 0.001f );
	bank.AddToggle("Tessellation - Cull", &ms_CullEnable);
	bank.PopGroup();
# endif // RAGE_SUPPORT_TESSELLATION_TECHNIQUES
	
# if TRACK_CONSTANT_BUFFER_CHANGES
	bank.AddToggle("Track ConstantBuffer Usage", &grcEffect::sm_TrackConstantBufferUsage);
	bank.AddToggle("Reset counters after each frame", &grcEffect::sm_TrackConstantBufferResetFrame);
	bank.AddToggle("Print ConstantBuffer Usage (requires track on)", &grcEffect::sm_PrintConstantBufferUsage);
	bank.AddToggle("Only print changed constant buffers", &grcEffect::sm_PrintConstantBufferUsageOnlyChanged);
# endif // TRACK_CONSTANT_BUFFER_CHANGES

#if RSG_PC
	bank.AddToggle("SetUAV to NULL rather than NULL Buffer", &grcEffect::sm_SetUAVNull);
#endif // RSG_PC

	bank.PopGroup();
#endif //__D3D11 || RSG_ORBIS

#if TERRAIN_TESSELLATION_SUPPORT
	CRenderPhaseDeferredLighting_SceneToGBuffer::AddTerrainWidgets(bank);
#endif //TERRAIN_TESSELLATION_SUPPORT

#if CASCADE_SHADOWS_DO_SOFT_FILTERING
	CRenderPhaseCascadeShadowsInterface::InitWidgets_SoftShadows(bank);
#endif

#if LINEAR_PIECEWISE_FOG
	LinearPiecewiseFog::AddWidgets(bank);
#endif // LINEAR_PIECEWISE_FOG

	bank.AddToggle("Render FPS Ped with 3rd person IK in non-gbuffer phases", &FPSPedDraw::sEnable3rdPersonSkel);
}
#endif //__BANK


RAGETRACE_DECL(CRenderer_WaitForSPU);

int	g_stream_model=-100;

bool	dump_stats=false;
void	CRenderer::IssueAllScanTasksNew()
{
	if(g_render_lock)
		return;

	if (!g_SceneToGBufferPhaseNew)
		return;

	PF_START_TIMEBAR_BUDGETED("Issued scan tasks",1.0f);

	Assert(g_SceneToGBufferPhaseNew);
	CRenderer::SetSceneToGBufferListBits(1<<g_SceneToGBufferPhaseNew->GetEntityListIndex());
	CRenderer::SetMirrorListBits(1<<CRenderPhaseMirrorReflectionInterface::GetRenderPhase()->GetEntityListIndex());
	CRenderer::SetShadowPhases(0);
#if __BANK
	CRenderer::SetDebugPhases(0);
#endif // __BANK

	int phaseCount = RENDERPHASEMGR.GetRenderPhaseCount();

	for(int x=0; x<phaseCount; x++)
	{
		fwRenderPhase &pRenderPhase = RENDERPHASEMGR.GetRenderPhase(x);

		if (CDrawListMgr::IsShadowDrawList(pRenderPhase.GetDrawListType()))
		{
			Assert(pRenderPhase.HasEntityListIndex());
			CRenderer::OrInShadowPhases(1<<pRenderPhase.GetEntityListIndex());
		}
#if __BANK
		else if (CDrawListMgr::IsDebugDrawList(pRenderPhase.GetDrawListType()))
		{
			if (pRenderPhase.HasEntityListIndex())
			{
				CRenderer::OrInDebugPhases(1<<pRenderPhase.GetEntityListIndex());
			}
		}
#endif // __BANK
	}

	CGameScan::PerformScan();
}


//////////////////////////////////////////////////////////////////////////



float CRenderer::ms_bestScannedVisiblePortalArea=0.0f;
grcViewport CRenderer::ms_bestScannedVisiblePortalViewport;
s32 CRenderer::ms_bestScannedVisiblePortalIntProxyID=-1;
s32 CRenderer::ms_bestScannedVisiblePortalMainTCModifier=-1;
s32 CRenderer::ms_bestScannedVisiblePortalSecondaryTCModifier=-1;

bank_float ScannedVisibilePortalNearClip = 0.01f;
bank_float ScannedVisibilePortalFarClip = 500.0f;

const grcViewport& CRenderer::FixupScannedVisiblePortalViewport(const grcViewport& viewport)
{
	ms_bestScannedVisiblePortalViewport=viewport;

	// use fixed parameters, it's save us from the tc use far clip to modify far clip to use far clip to modify far clip to use far clip to modify far clip to use far clip to...
	ms_bestScannedVisiblePortalViewport.SetNearFarClip(ScannedVisibilePortalNearClip,ScannedVisibilePortalFarClip); // Bank this, bitch

	return ms_bestScannedVisiblePortalViewport;
}

void CRenderer::ResetScannedVisiblePortal()
{
	ms_bestScannedVisiblePortalArea=0.0f;
	ms_bestScannedVisiblePortalIntProxyID=-1;
}


void CRenderer::UpdateScannedVisiblePortal(CPortalInst* portalInst)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	CInteriorInst* pIntInst;
	s32 portalIdx0;
	portalInst->GetOwnerData(pIntInst, portalIdx0);

	Assert(pIntInst);

	u32 portalIdx = pIntInst->GetPortalIdxInRoom(0, portalIdx0);
	const fwPortalCorners& portalCorners=pIntInst->GetPortal(portalIdx);
	spdAABB boundBox;
	portalCorners.CalcBoundingBox(boundBox);
	
	bool visible = COcclusion::IsBoxVisibleFast(boundBox);

	if( visible )
	{	
		float area = portalCorners.CalcScreenSurface(ms_bestScannedVisiblePortalViewport);

		if (area>ms_bestScannedVisiblePortalArea)
		{
			ms_bestScannedVisiblePortalArea=area;
			ms_bestScannedVisiblePortalIntProxyID = CInteriorProxy::GetPool()->GetJustIndex(pIntInst->GetProxy());
			
			CMloModelInfo* modelInfo = pIntInst->GetMloModelInfo();
			unsigned int roomId1,roomId2;
			CPortalFlags flags;
			
			modelInfo->GetPortalData(portalIdx,roomId1,roomId2,flags);
			int roomId = Max(roomId1,roomId2);

			pIntInst->GetTimeCycleIndicesForRoom(roomId, ms_bestScannedVisiblePortalMainTCModifier, ms_bestScannedVisiblePortalSecondaryTCModifier);
		}

		if( portalInst->GetUseLightBleed() )
		{
			const Vec3V camPos = VECTOR3_TO_VEC3V(camInterface::GetPos());
			const ScalarV portalDist = portalCorners.CalcSimpleDistanceToPointSquared(camPos);

			const Vec3V extent = (portalCorners.GetCornerV(0) - portalCorners.GetCornerV(2))*ScalarV(V_HALF);
			const ScalarV extentMagSquared = Max(ScalarV(V_FIFTEEN)*ScalarV(V_FIFTEEN), MagSquared(extent));
			
			if(IsLessThanAll(portalDist, extentMagSquared))
			{
				CMloModelInfo* modelInfo = pIntInst->GetMloModelInfo();
				unsigned int roomId1,roomId2;
				int timeCycleIndex, secondaryTimeCycleIndex;
				CPortalFlags flags;
				
				modelInfo->GetPortalData(portalIdx,roomId1,roomId2,flags);
				int roomId = Max(roomId1,roomId2);

				pIntInst->GetTimeCycleIndicesForRoom(roomId, timeCycleIndex, secondaryTimeCycleIndex);
				if( timeCycleIndex != -1 )
				{
					float doorOcclusion = flags.GetAllowClosing() ? CPortal::GetDoorOcclusion(pIntInst, portalIdx) : 1.0f;
					g_timeCycle.AddLocalPortal(portalCorners,area,doorOcclusion,timeCycleIndex,secondaryTimeCycleIndex);
				}
			}
		}
	}
}

bool CRenderer::TwoPassTreeRenderingEnabled()
{
	return g_UseTwoPassTreeRendering;
}

bool CRenderer::TwoPassTreeNormalFlipping()
{
	return g_UseTwoPassTreeNormalFlipping;
}

bool CRenderer::DoubleSidedTreesCutoutOptimization()
{
	return g_DoubleSidedTreesCutoutOptimization;
}
bool CRenderer::TwoPassTreeShadowRenderingEnabled()
{
	return g_UseTwoPassTreeShadowRendering;
}

bool CRenderer::UseDelayedGbuffer2TileResolve()
{
	//Only supported in 720p scene buffers
	bool allow = VideoResManager::GetSceneWidth() == 1280 && VideoResManager::GetSceneHeight() == 720;
	return g_UseDelayedGbuffer2TileResolve && allow;
}

#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES
void CRenderer::SetTessellationQuality(int quality)
{
	ms_ScreenSpaceErrorInPixels = quality==0 ? 0.f : quality==1? 2.f : 1.f;
	ms_MaxTessellation = 2<<quality;
}

bool CRenderer::IsTessellationEnabled()
{
	return ms_ScreenSpaceErrorInPixels != 0;
}

void CRenderer::SetTessellationVars(const grcViewport &viewport, const float aspect)
{
	//Vec4V TessellationGlobal1((float)ms_WheelTessellationFactor, (float)ms_TreeTessellationFactor, (float)ms_TreeTessellation_PNTriangle_K, 0.0f);
	//DLC_SET_GLOBAL_VAR(m_TessellationVar, TessellationGlobal1);
	ms_TessellationVars.Set(&viewport, ms_LinearNear, ms_LinearFar, ms_MaxTessellation, ms_ScreenSpaceErrorInPixels, aspect, ms_FrustumEpsilon, ms_BackFaceEpsilon, ms_CullEnable);

# if !__FINAL
	SetDisplacementGlobals();
# endif // !__FINAL

#if !__FINAL || RSG_PC
	//	Setup POM Flags
	if(ms_POMFlagsVar == grcegvNONE)
	{
		ms_POMFlagsVar = grcEffect::LookupGlobalVar("POMFlags", !RSG_PC);
	}

	if(ms_POMFlagsVar != grcegvNONE)
	{
		const bool pomDisabled = CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShaderQuality >= CSettings::High ? false : true;

#if !__FINAL
		//	if rag, force using its settings
		DLC_SET_GLOBAL_VAR(ms_POMFlagsVar, Vec4V(ms_DisablePOM ? 1.0f : pomDisabled ?  1.0f : 0.0f,
			ms_VisPOMVertexHeight ? 1.0f : 0.0f, 
			m_POMDistanceFade ? 1.0f : 0.0f,
			ms_VisPOMDistanceFade ? 1.0f : 0.0f));
#else
		DLC_SET_GLOBAL_VAR(ms_POMFlagsVar, Vec4V(pomDisabled ?  1.0f : 0.0f, 0.0f, 0.0f, 0.0f));
#endif
	}
#endif
}

# if !__FINAL
void CRenderer::SetDisplacementGlobals()
{
	//POM vars
	if(ms_POMValuesVar == grcegvNONE)
	{
		ms_POMValuesVar = grcEffect::LookupGlobalVar("POMValues", !RSG_PC);
	}

	if(ms_POMValuesVar != grcegvNONE)
	{
		DLC_SET_GLOBAL_VAR(ms_POMValuesVar, Vec4V(ms_POMNear, ms_POMFar, ms_POMMinSamp, ms_POMMaxSamp>=ms_POMMinSamp ? ms_POMMaxSamp : ms_POMMinSamp));
	}

	if(ms_POMValues2Var == grcegvNONE)
	{
		ms_POMValues2Var = grcEffect::LookupGlobalVar("POMValues2", !RSG_PC);
	}

	if(ms_POMValues2Var != grcegvNONE)
	{
		DLC_SET_GLOBAL_VAR(ms_POMValues2Var, Vec4V(ms_POMForwardOffset, 0.0f, 0.0f, 0.0f));
	}

	//general displacement vars
	if(ms_GlobalHeightScaleVar == grcegvNONE)
	{
		ms_GlobalHeightScaleVar = grcEffect::LookupGlobalVar("globalHeightScale", !RSG_PC);
	}

	if(ms_GlobalHeightScaleVar != grcegvNONE)
	{
		DLC_SET_GLOBAL_VAR(ms_GlobalHeightScaleVar, ms_GlobalHeightScale);
	}
}
# endif // !__FINAL
#endif //RAGE_SUPPORT_TESSELLATION_TECHNIQUES

# if !__FINAL
void CRenderer::SetTerrainTintGlobals()
{
	if(ms_TerrainTintBlendValuesVar == grcegvNONE)
	{
		ms_TerrainTintBlendValuesVar = grcEffect::LookupGlobalVar("TerrainTintBlendValues");
	}

	if(ms_TerrainTintBlendValuesVar != grcegvNONE)
	{
		DLC_SET_GLOBAL_VAR(ms_TerrainTintBlendValuesVar, Vec4V(ms_TerrainTintBlendNear, ms_TerrainTintBlendFar, 0.0f, 0.0f));
	}
}
# endif // !__FINAL

#if __BANK
void CRenderer::PlantsToggleCB()
{
	// Proc grass:
	gbPlantMgrActive = ms_bPlantsAllEnabled;

	// IP grass:
	CEntityBatchDrawHandler::SetGrassBatchEnabled(ms_bPlantsAllEnabled);
}

namespace EBStatic
{
	extern BankBool sPerfDisplayUsageInfo;
}

void CRenderer::PlantsDisplayDebugInfoCB()
{
	// Proc grass:
	extern bool gbDisplayCPlantMgrInfo;
	gbDisplayCPlantMgrInfo = ms_bPlantsDisplayDebugInfo;

	// IP grass:
	grcDebugDraw::SetDisplayDebugText(true);
	EBStatic::sPerfDisplayUsageInfo = ms_bPlantsDisplayDebugInfo;
}

void CRenderer::PlantsAlphaOverDrawCB()
{
	// Proc grass:
	extern bool	gbShowAlphaOverdraw;
	gbShowAlphaOverdraw = ms_bPlantsAlphaOverdraw;

	// IP grass:
	CCustomShaderEffectGrass::SetDbgAlphaOverdraw(ms_bPlantsAlphaOverdraw);
}
#endif //__BANK...


