//
// name:		viewportMgr.h
// description:	Class controlling and containing the viewports
//

// C++ hdrs
#include "stdio.h"

// Rage headers
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/combo.h"

#include "grcore/debugdraw.h"
#include "grcore/device.h"
#include "grcore/viewport.h"
#include "grcore/font.h"
#include "grcore/texture.h"
#include "grcore/texturegcm.h"
#include "grcore/texturexenon.h"
#include "grcore/im.h"
#include "grcore/wrapper_gcm.h"
#include "profile/timebars.h"
#include "grprofile/pix.h"
#include "system/param.h"
#include "system/xtl.h"
#include "vector/color32.h"
#include "grmodel/geometry.h"
#include "gpuptfx/ptxgpubase.h"
#include "portcullis/portcullis.h"
#if RSG_PC
#include "renderer/computeshader.h"
#endif

// Framework headers
#include "fwmaths/random.h"
#include "fwsys/timer.h"
#include "fwscene/world/WorldMgr.h"
#include "fwscene/search/Search.h"

// Core headers
#include "scene/EntityIterator.h"
#include "camera/CamInterface.h"
#include "camera/helpers/Frame.h"
#include "camera/viewports/ViewportManager.h"
#include "camera/viewports/Viewport.h"
#include "camera/scripted/ScriptDirector.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "Debug/Debug.h"
#include "frontend/HudTools.h"
#include "Frontend/Credits.h"
#include "frontend/MobilePhone.h"
#include "renderer/DrawLists/drawListMgr.h"
#include "renderer/Mirrors.h"
#include "renderer/occlusion.h"
#include "renderer/renderListGroup.h"
#include "renderer/Deferred/DeferredConfig.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseDebugNY.h"
#include "renderer/RenderPhases/RenderPhaseDebugOverlay.h"
#include "renderer/RenderPhases/RenderPhaseDefLight.h"
#include "renderer/RenderPhases/RenderPhaseEntitySelect.h"
#include "renderer/RenderPhases/RenderPhaseFX.h"
#include "renderer/RenderPhases/RenderPhaseHeightMap.h"
#include "renderer/RenderPhases/RenderPhaseLensDistortion.h"
#include "renderer/RenderPhases/RenderPhaseMirrorReflection.h"
#include "renderer/RenderPhases/RenderPhaseParaboloidShadows.h"
#include "renderer/RenderPhases/RenderPhasePedDamage.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/RenderPhases/RenderPhaseStd.h"
#include "renderer/RenderPhases/RenderPhaseStdNY.h"
#include "renderer/RenderPhases/RenderPhaseTreeImposters.h"
#include "renderer/RenderPhases/RenderPhaseWater.h"
#include "renderer/RenderPhases/RenderPhaseWaterReflection.h"
#include "renderer/RenderTargetMgr.h"
#include "renderer/RenderTargets.h"
#include "renderer/renderThread.h"
#include "renderer/Renderer.h"
#include "renderer/PostScan.h"
#include "renderer/Shadows/ParaboloidShadows.h"
#include "renderer/Water.h"
#include "scene/lod/LodMgr.h"
#include "scene/portals/Portal.h"
#include "shaders/shaderlib.h"
#include "streaming/streaming.h"
#include "streaming/streamingrequestlist.h"
#include "streaming/streamingvisualize.h"
#include "system/performancetimer.h"
#include "system/SettingsManager.h"
#include "system/taskscheduler.h"
#include "system/usetuner.h"

#if __D3D11
#include "grcore/texturefactory_d3d11.h"
#endif

#if __PPU

#if GCM_HUD
extern bool g_bGcmHudRenderPhaseTags;
#endif // GCM_HUD

#endif // __PPU

CAMERA_OPTIMISATIONS()

CRenderPhaseScanned *g_SceneToGBufferPhaseNew;
CRenderPhaseEntitySelect *g_RenderPhaseEntitySelectNew;
CRenderPhaseCascadeShadows *g_CascadeShadowsNew;
CRenderPhaseHeight *g_RenderPhaseHeightNew;
CRenderPhaseDeferredLighting_LightsToScreen *g_DefLighting_LightsToScreen;
CRenderPhaseScanned *g_ReflectionMapPhase;
CRenderPhaseMirrorReflection *g_MirrorReflectionPhase;
RenderPhaseSeeThrough *g_SeeThroughPhase;

// GTA hdrs

#if !__FINAL
PARAM(nowater, "Disable water code");
#if __D3D11
XPARAM(DX11Use8BitTargets);
#endif //__D3D11
//XPARAM(RT);
#endif // !__FINAL

#if RSG_PC
PARAM(UseDefaultResReflection, "Use default size reflection resolution");
#endif

#if RSG_ORBIS
PARAM(UseVarReflection, "Use High resolution size reflection resolution");
#endif

// --- Structure/Class Definitions ----------------------------------------------
// Custom draw list commands
class dlCmdRenderPhasesDrawInit : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_RenderPhasesDrawInit,
	};

	s32 GetCommandSize()						{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase & /*cmd*/)	{ CViewportManager::RenderPhasesDrawInit(); }
};


//************************************************************
CViewportManager gVpMan; // The main and only viewport manager
//************************************************************

#if RSG_PC
float CViewportManager::sm_HalfEyeSeparation = -1.0f;
#endif

#if __BANK

CRenderSettings g_widgetRenderSettings;

#endif // __BANK

//------------------------------------------
// name:		Init
// description: Initialise viewport class, construct Rage viewports
void CViewportManager::Init()
{
	Reset();

	ResetViewportStack();

	SetDefaultGlobalRenderSettings();
	
	ResetRenderPhaseLists();

	m_pGameViewport			= NULL;
	m_pPrimaryOrthoViewport	= NULL;
	m_gameViewportIndex		= -1;

#if DEBUG_DRAW
	grcDebugDraw::SetCullingViewport(NULL);
#endif // DEBUG_DRAW
}

//----------------------------------------------------------------
// These settings can exclude render settings over the entire game
void CViewportManager::SetDefaultGlobalRenderSettings()
{
}

//------------------------------------------------------------
// Empty the list of pointers to phases.
void CViewportManager::ResetRenderPhaseLists()
{
}

//------------------------------------------
// name:		Shutdown
// description:	Delete viewports
void CViewportManager::Shutdown()
{
	ShutdownRenderPhases();

	Reset();
}

void CViewportManager::ShutdownRenderPhases()
{
	g_SceneToGBufferPhaseNew = NULL;
	g_RenderPhaseEntitySelectNew = NULL;
	g_DefLighting_LightsToScreen = NULL;
	g_ReflectionMapPhase = NULL;

	CRenderPhaseWaterReflectionInterface::ResetViewport();

	// Delete all the render phases
	int count = RENDERPHASEMGR.GetRenderPhaseCount();

	for (int x=0; x<count; x++)
	{
		delete &RENDERPHASEMGR.GetRenderPhase(x);
	}

	RENDERPHASEMGR.ResetRenderPhaseList();

	// Unlink the destroyed render phases from the phase list.
	gRenderTargetMgr.ClearRenderPhasePointersNew();
#if SORT_RENDERLISTS_ON_SPU
	gRenderListGroup.WaitForSortEntityListJob();
#endif	//SORT_RENDERLISTS_ON_SPU
	gRenderListGroup.ClearRenderPhasePointersNew();
	gRenderListGroup.ResetRegisteredRenderPhases();
}

void CViewportManager::RecomputeViewports()
{
	for(s32 i=0;i<m_viewports.GetCount();i++)
	{
		m_viewports[i]->ResetWindow();
		m_viewports[i]->UpdateAspectRatio();
	}
}

#if RSG_PC
void CViewportManager::DeviceLost()
{
}

void CViewportManager::DeviceReset()
{
	if (!g_RenderPhaseManager::IsInstantiated()) 
	{
		return;
	}

	bool bReCreateScreenRederPhaseList = g_SceneToGBufferPhaseNew != NULL;

	if (g_SceneToGBufferPhaseNew)
	{
		((CRenderPhaseDeferredLighting_SceneToGBuffer*) g_SceneToGBufferPhaseNew)->StorePortalVisTrackerForReset();
	}

	grcViewport::ShutdownClass();
	gVpMan.ShutdownRenderPhases();

	grcViewport::InitClass();
	gVpMan.RecomputeViewports();
	if (bReCreateScreenRederPhaseList) gVpMan.CreateFinalScreenRenderPhaseList();

	bool bPause = CPauseMenu::GetPauseRenderPhasesStatus();
	if (bPause) CPauseMenu::RePauseRenderPhases();

/*
	Need to be checked by Grant on crossfire
	if(CPauseMenu::GetPauseRenderPhasesStatus())
	{
		CPauseMenu::ResetPauseRenderPhasesStatus();
		CPauseMenu::ClearGPUCountdownToPauseStatus();
	}
*/
}
#endif

//----------------------------
//
void CViewportManager::Reset()
{
	for(s32 i=0;i<m_viewports.GetCount();i++) 
		delete m_viewports[i];
	m_viewports.Reset();

	m_sortedViewportIndicies.Reset();

	m_bWidescreenBorders = false;
	m_bDrawWidescreenBorders = false;
	m_timeWideScreenBordersCommandSet = -1;
	m_timeScrollOnWideScreenBorders = 500;
}

//-----------------
// Init the Session
void CViewportManager::InitSession()
{
	D3D11_ONLY(GRCDEVICE.LockContext();)

	for(s32 i = 0; i<m_viewports.GetCount(); i++)
		m_viewports[i]->InitSession();

	ResetViewportStack();

	SetWidescreenBorders(false);  // start the session with the widescreen borders switched OFF
	D3D11_ONLY(GRCDEVICE.UnlockContext();)
}

//---------------------
// Shutdown the Session
void CViewportManager::ShutdownSession()
{
	for(s32 i = 0; i<m_viewports.GetCount(); i++)
		m_viewports[i]->ShutdownSession();

	gRenderListGroup.Clear();
	
	ResetViewportStack();
}

//---------------------
// Clear the stack out
void CViewportManager::ResetViewportStack()
{
	if (m_stackSize==0) // quick fix! I know its silly.
	{
		m_stackSize = 0;
		for (s32 i=0;i<MAX_VIEWPORT_STACK_SIZE;i++)
			m_stackViewports[i] = NULL;
	}
}

//---------------------------------------------------------------------
// Set a viewport to be the new render destination & maintain the stack
void CViewportManager::PushViewport(CViewport *pViewport)
{
	Assert(m_stackSize<MAX_VIEWPORT_STACK_SIZE); // DW - if this asserts then you have to check the game for BALANCED push and pops, there has simply been a bad render function called somehow... OR it could possibly be that we are really deep in a complicated renderer call, but unlikely!..increasing the stack size is not an answer ever!
	
	m_stackViewports[m_stackSize] = pViewport;
	m_stackSize++;
}

//---------------------------------------------------------------------------------------
// Take a viewport off the stack, the previous set viewport becomes the new render target
void CViewportManager::PopViewport()
{
	m_stackSize--;
	Assert(m_stackSize>=0); // DW - if this asserts then you have to check the game for BALANCED push and pops, there has simply been a bad render fucntion called somehow.
	m_stackViewports[m_stackSize] = NULL; // null for super safety here ( v. important ! ) 
}

//=======================
// MULTI_VIEWPORT SUPPORT
//=======================

//----------------------------------------------------
// Qsort function
int QSortViewportCompareFunc(const s32* pA, const s32* pB)
{
	return gVpMan.GetViewport(*pB)->GetPriority() - gVpMan.GetViewport(*pA)->GetPriority();
}

//-------------------------------------------------------------------------------------------------------
// Loops over all viewports and computes a list of viewport indexs which should be rendered in this order
// The sorted list can be accessed through special methods in this class, otherwise the access to viewports
// is not guaranteed to be sorted.
s32 CViewportManager::ComputeRenderOrder()
{
	m_sortedViewportIndicies.ResetCount();

	for (s32 i=0;i<m_viewports.GetCount();i++)
		if (m_viewports[i]->IsActive() && m_viewports[i]->IsRenderable())
			m_sortedViewportIndicies.PushAndGrow(i);

	m_sortedViewportIndicies.QSort(0,-1,QSortViewportCompareFunc);

	return m_sortedViewportIndicies.GetCount();
}


//----------------------------------------------------------
// Process all our viewports - i.e. let them fade, move etc.
void CViewportManager::Process()
{
	// Process all active viewports
	for (s32 i=0;i<m_viewports.GetCount();i++)
		if (m_viewports[i]->IsActive())
			m_viewports[i]->Process();

	// Now that they have processed we cache their desired order of compositing onto screen.
	ComputeRenderOrder();

	CGtaOldLoadingScreen::SetLoadingScreenIfRequired();
}

//---------------------------------------------------------------------
// Computes a large matrix & viewport encompassing all active viewports
// For now it ONLY chooses the biggest visible area window on the screen
// If nothing is onscreen the viewport is not updated, and hopes that things will return to normal shortly..
grcViewport* CViewportManager::CalcViewportForFragManager()
{
	s32 numFound = 0;
	s32 biggestWindow = 0;
	s32 biggestAreaOfWindow = -1;

	for (s32 vp=0; vp < m_viewports.GetCount(); vp++)		// iterate over all the viewports
	{
		CViewport *pVp = GetViewport(vp);
		if (pVp && pVp->IsActive() && pVp->IsGame() )
		{					
			CViewportGame *pVpScene = static_cast<CViewportGame*>(pVp);
			numFound++;

			s32 area =	pVpScene->GetGrcViewport().GetWindow().GetWidth() * 
						pVpScene->GetGrcViewport().GetWindow().GetHeight();
			if (area>biggestAreaOfWindow)
			{
				biggestWindow = vp;
				biggestAreaOfWindow = area;
			}
		}
	}

	if (numFound!=0) 
	{
		if ( m_viewports[biggestWindow]->GetOnScreenStatus() == VIEWPORT_ONSCREEN_CODE_WHOLLY_OFFSCREEN)
		{
			// dont want to update the frustum
			return &m_fragManagerViewport;
		}

		m_fragManagerViewport = m_viewports[biggestWindow]->GetGrcViewport(); // can be tried later
		return &m_fragManagerViewport;
	}
	else // nothing visible... don't update the fragmanager viewport and matrix
	{
		return &m_fragManagerViewport;
	}
}

//******************************************************************************
//******************************************************************************
//******************************************************************************
//******************************************************************************
// RENDER PHASE SUPPORT
//******************************************************************************
//******************************************************************************
//******************************************************************************
//******************************************************************************

#define WATER_REFLECTION_RES_BASELINE 128

#if RSG_PC || RSG_DURANGO || RSG_ORBIS
int CViewportManager::ComputeReflectionResolution(int width, int height, int reflectionQuality) {
	int reflectionSizeX = ((reflectionQuality > 0) ? Max(1, width / (1 << (3 - Min(3, reflectionQuality)))) : 32);
	int reflectionSizeY = ((reflectionQuality > 0) ? Max(1, height / (1 << (3 - Min(3, reflectionQuality)))) : 32);
	u32 dimension = (u32) rage::Sqrtf((float)(reflectionSizeX * reflectionSizeY));
	u32 iMipLevels = 0;
	while (dimension)
	{
		dimension >>= 1;
		iMipLevels++;
	}
	iMipLevels--;

	return 1 << iMipLevels;
}
#endif

//----------------------------------------------------------------
// Was in Process() but had to migrate after camSys.update
// Decide whether to create renderphases for this viewport... when it is activated it creates them
void CViewportManager::ProcessRenderPhaseCreationAndDestruction()
{
	for (s32 i=0;i<m_viewports.GetCount();i++)
	{
		CViewport *pVp = m_viewports[i];

		pVp->SetActiveLastFrame(pVp->IsActive());
	}
}

#if __DEV && __XENON
PARAM(refdebugnoheap_360,"refdebugnoheap_360"); // allocate reflection target separately, so it can be viewed from rag
#endif // __DEV && __XENON

#if __XENON && !__NO_OUTPUT && 0
void SpewMemCheck(const char* spewWords){
	MEMORYSTATUS ms;
	GlobalMemoryStatus(&ms);
	size_t remain = ms.dwAvailPhys >> 10;
	Displayf("[CViewportManager] '%s'(%uk remaining)",spewWords,remain);
}
#else  //__XENON && !__NO_OUTPUT
void SpewMemCheck(const char*) {}
#endif //__XENON && !__NO_OUTPUT

#if RSG_PC
void CViewportManager::ComputeVendorMSAA(grcTextureFactory::CreateParams& params)
{
	//	create a MSAA rendertarget and keep MSAA for depth
	DXGI_FORMAT format = static_cast<DXGI_FORMAT>(grcTextureFactoryDX11::ConvertToD3DFormat((grcTextureFormat)params.Format));
	UINT numQualityLevels;

	switch(GRCDEVICE.GetManufacturer())
	{
	case NVIDIA:
		GRCDEVICE.GetCurrent()->CheckMultisampleQualityLevels(format, 4, &numQualityLevels);

		//	Use CSSA if possible
		if(params.Multisample == grcDevice::MSAAModeEnum::MSAA_4 && numQualityLevels >= 8)
		{
			//	CSSA 8x
			params.Multisample = 4;
			params.MultisampleQuality = 8;
		}
		else if(params.Multisample == grcDevice::MSAAModeEnum::MSAA_8 && numQualityLevels >= 16)
		{
			//	CSS 16x
			params.Multisample = 4;
			params.MultisampleQuality = 16;
		}
		break;

	case ATI:
		GRCDEVICE.GetCurrent()->CheckMultisampleQualityLevels(format, params.Multisample, &numQualityLevels);

		// try to use EQAA
		if(numQualityLevels >= params.Multisample*2)
		{
			//	Use EQAA 2x
			params.Multisample = params.Multisample;
			params.MultisampleQuality = (u8)(params.Multisample*2);
		}
		break;
	default:
		// keep MSAAx2
		break;
	}
}
#endif

//------------------------------------------------------------------
// Look across all viewports active and build a list of renderphases
// that will be performed to render all the entire screen
void CViewportManager::CreateFinalScreenRenderPhaseList()
{
	SpewMemCheck("Entry point");

	// DW - its at this stage we decide whether a viewports phases need to be created. ( based upon activation and deactivation of a viewport ).. perhaps might be better at viewport creation / deststruction ?
	ProcessRenderPhaseCreationAndDestruction();

#if RSG_PC
// 	static bool firstTime = true;
// 	if (firstTime)
// 	{
// 		CGame::InitCallbacks();
// 		firstTime = false;
// 	}
#endif

	if (!g_SceneToGBufferPhaseNew)
	{
		// Here is the master list of all render phases.
		CViewport *orthoViewport = GetPrimaryOrthoViewport();
		CViewport *gameViewport = GetGameViewport();

		Assert(orthoViewport);
		Assert(gameViewport);

		// 3D Viewport pre-render
		RENDERPHASEMGR.RegisterRenderPhase(*rage_new CRenderPhasePreRenderViewport(gameViewport));

		#if ENTITYSELECT_ENABLED_BUILD
			// Entity Selection: Special render phase used to select an entity
			g_RenderPhaseEntitySelectNew = rage_new CRenderPhaseEntitySelect(gameViewport);
			CRenderPhaseEntitySelect::InitializeRenderingResources();
			RENDERPHASEMGR.RegisterRenderPhase(*g_RenderPhaseEntitySelectNew);
		#endif // ENTITYSELECT_ENABLED_BUILD

		#if USE_TREE_IMPOSTERS
			// Tree imposters.
			RENDERPHASEMGR.RegisterRenderPhase(*rage_new CRenderPhaseTreeImposters(gameViewport));
		#endif // USE_TREE_IMPOSTERS

		// Script 2D update. TODO: Verify. Really 2D? Yes, 2D, but the result is in a texture applied to world geom.
		CRenderPhase *phaseNew = rage_new CRenderPhaseScript2d(NULL);
		Assert(phaseNew);
		gRenderTargetMgr.SetRenderPhase(CRenderTargetMgr::RTI_Scripted,phaseNew);
		RENDERPHASEMGR.RegisterRenderPhase(*phaseNew);

		CRenderPhase *phaseNewMovieMesh = rage_new CRenderPhaseScript2d(NULL);
		Assert(phaseNewMovieMesh);
		gRenderTargetMgr.SetRenderPhase(CRenderTargetMgr::RTI_MovieMesh,phaseNewMovieMesh);
		RENDERPHASEMGR.RegisterRenderPhase(*phaseNewMovieMesh);

		RENDERPHASEMGR.RegisterRenderPhase(*rage_new CRenderPhaseCloudGeneration(gameViewport));

		// Reflection map
		{
			grcTextureFactory::CreateParams params;
			params.UseFloat		= true;
			params.HasParent	= true; 
			params.Parent		= NULL;

			int reflectionResolution = REFLECTION_TARGET_HEIGHT;

			#if __XENON
				params.IsResolvable = true;
				params.UseFloat     = false;
				params.IsSRGB       = false;
				params.Format		= grctfA8R8G8B8;

				grcRenderTarget *pColourTarget = NULL;
			  #if __DEV
				if (PARAM_refdebugnoheap_360.Get())
				{
					params.Multisample	= 1; 
					params.MipLevels	= 4;
					pColourTarget = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "REFLECTION_MAP_COLOUR", grcrtPermanent, REFLECTION_TARGET_WIDTH, REFLECTION_TARGET_HEIGHT, 32, &params);
				}
				else
			  #endif // __DEV
				{
					params.Multisample	= 1; 
					params.MipLevels	= 4;
					params.IsResolvable = true;
					pColourTarget = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GENERAL, "REFLECTION_MAP_COLOUR", grcrtPermanent, REFLECTION_TARGET_WIDTH, REFLECTION_TARGET_HEIGHT, 32, &params, kReflectionHeap);
					pColourTarget->AllocateMemoryFromPool();
				}

			#elif __PPU
				const s32 tbpp			= 32;
				params.Format			= grctfA8R8G8B8;
				params.Multisample		= 0;
				params.MipLevels		= 4;		// min 32x32
				params.IsSRGB			= true;

				CompileTimeAssert(REFLECTION_TARGET_WIDTH == 512);	// PSN: size of RT in memory pool

				grcRenderTarget *pColourTarget		= CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P2048_TILED_LOCAL_COMPMODE_C32_2X1, "REFLECTION_MAP_COLOUR",	grcrtPermanent, REFLECTION_TARGET_WIDTH, REFLECTION_TARGET_HEIGHT, tbpp, &params, 0);
				pColourTarget->AllocateMemoryFromPool();

			#else // (RSG_PC || RSG_DURANGO || RSG_ORBIS)

				#if !(__D3D11 || RSG_ORBIS) // 64 bpp
					const int tbpp=64;
					params.Format = grctfA16B16G16R16F;
				#else // 32 bpp
						const s32 tbpp			= 32;
						params.Format			= grctfR11G11B10F;
						params.IsSRGB			= false;
				#endif

				params.Multisample	= grcDevice::MSAA_None;
				params.MipLevels	= REFLECTION_TARGET_MIPCOUNT;
				//params.UseAsUAV = true;

				grcRenderTarget *pColourTarget = CRenderPhaseReflection::GetRenderTargetColor();
				#if RSG_PC
					grcRenderTarget *pColourTargetCopy = CRenderPhaseReflection::GetRenderTargetColorCopy();
				#endif

			  #if RSG_PC
				if ( !PARAM_UseDefaultResReflection.Get())
			  #elif RSG_ORBIS
				if (PARAM_UseVarReflection.Get())
			  #else
				if (false)
			  #endif
				{
					#if RSG_PC || RSG_ORBIS
						const Settings& settings = CSettingsManager::GetInstance().GetSettings();
						reflectionResolution = ComputeReflectionResolution(GRCDEVICE.GetWidth(), GRCDEVICE.GetHeight(), settings.m_graphics.m_ReflectionQuality);
					#else
						reflectionResolution = ComputeReflectionResolution(GRCDEVICE.GetWidth(), GRCDEVICE.GetHeight(), 3);
					#endif

					//We still need to mip down to the same resolution as console so adjust increase the number of mip levels depending on our resolution.
					u32 mipResolution = reflectionResolution;
					while( mipResolution > REFLECTION_TARGET_HEIGHT)
					{
						mipResolution = mipResolution >>1;
						params.MipLevels++;
					}

					pColourTarget	= CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "REFLECTION_MAP_COLOUR", grcrtPermanent, reflectionResolution * 2, reflectionResolution, tbpp, &params WIN32PC_ONLY(, 0, pColourTarget));
					#if RSG_PC
						pColourTargetCopy	= CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "REFLECTION_MAP_COLOUR COPY", grcrtPermanent, reflectionResolution * 2, reflectionResolution, tbpp, &params WIN32PC_ONLY(, 0, pColourTargetCopy));
					#endif
					//params.StereoRTMode = grcDevice::AUTO;
				}
				else
				{
					pColourTarget	= CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "REFLECTION_MAP_COLOUR", grcrtPermanent, REFLECTION_TARGET_WIDTH, REFLECTION_TARGET_HEIGHT, tbpp, &params WIN32PC_ONLY(, 0, pColourTarget));
// this corrupts gbuf0 when reflection phase is placed after cascade shadow
#if RSG_DURANGO
					params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_REFLECTION_MAP;
#endif
					#if RSG_PC
						pColourTargetCopy	= CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "REFLECTION_MAP_COLOUR COPY", grcrtPermanent, REFLECTION_TARGET_WIDTH, REFLECTION_TARGET_HEIGHT, tbpp, &params WIN32PC_ONLY(, 0, pColourTargetCopy));
					#endif
				}

			#endif // (RSG_PC || RSG_DURANGO || RSG_ORBIS)

			g_ReflectionMapPhase = rage_new CRenderPhaseReflection(gameViewport);
			// moving after shadow phase to add shadow to reflection map
			RENDERPHASEMGR.RegisterRenderPhase(*g_ReflectionMapPhase);

			CRenderPhaseReflection::SetRenderTargetColor(pColourTarget);
			CRenderPhaseReflection::CreateCubeMapRenderTarget(reflectionResolution);
			#if RSG_PC
				CRenderPhaseReflection::SetRenderTargetColorCopy(pColourTargetCopy);
			#endif
		}

		// Ped Damage (after Reflection so we can reuse the memory)
		RENDERPHASEMGR.RegisterRenderPhase(*rage_new CRenderPhasePedDamageUpdate(gameViewport));

		// W A T E R - surface
		CRenderPhaseWaterSurface *pWaterSurfacePhaseNew;
		{
			pWaterSurfacePhaseNew = rage_new CRenderPhaseWaterSurface(gameViewport);
			RENDERPHASEMGR.RegisterRenderPhase(*pWaterSurfacePhaseNew);
		}

		// The main GBUF.
		g_SceneToGBufferPhaseNew=rage_new CRenderPhaseDeferredLighting_SceneToGBuffer(gameViewport);
		RENDERPHASEMGR.RegisterRenderPhase(*g_SceneToGBufferPhaseNew);

		#if SPUPMMGR_PS3
			// Kick off SPU SSAO:
			CRenderPhaseDeferredLighting_SSAOSPU* DefLighting_SSAOSPU = rage_new CRenderPhaseDeferredLighting_SSAOSPU(gameViewport);
			RENDERPHASEMGR.RegisterRenderPhase(*DefLighting_SSAOSPU);
		#endif

		// Paraboloid shadows
		for (int i=0;i<MAX_ACTIVE_PARABOLOID_SHADOWS;i++)
		{
			CRenderPhaseParaboloidShadow *paraboloidShadowNew = rage_new CRenderPhaseParaboloidShadow(gameViewport, i);
			RENDERPHASEMGR.RegisterRenderPhase(*paraboloidShadowNew);
		}

		// Directional shadows
		g_CascadeShadowsNew = rage_new CRenderPhaseCascadeShadows(gameViewport);
		RENDERPHASEMGR.RegisterRenderPhase(*g_CascadeShadowsNew);

		// R A I N  H E I G H T  M A P / H E I G H T  M A P
		{
			grcTextureFactory::CreateParams params;
			params.HasParent = true; 
			params.Parent = NULL; 
			params.Multisample = grcDevice::MSAA_None;
			params.UseFloat = true;
			params.UseHierZ = true;
			params.Format = grctfD32FS8;
			grcRenderTarget *pDepthTarget	= CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "HEIGHT_MAP_DEPTH",		RSG_ORBIS ? grcrtShadowMap : grcrtPermanent,	HGHT_TARGET_SIZE, HGHT_TARGET_SIZE, 32, &params);

			CRenderPhaseHeight::Params	phaseParams("raincollision", 
				false, 
				DL_RENDERPHASE_RAIN_COLLISION_MAP );

			g_RenderPhaseHeightNew = rage_new CRenderPhaseHeight(gameViewport, phaseParams);
			RENDERPHASEMGR.RegisterRenderPhase(*g_RenderPhaseHeightNew);

			CRenderPhaseHeight::SetPtFxGPUCollisionMap(g_RenderPhaseHeightNew);
			g_RenderPhaseHeightNew->SetRenderTarget( pDepthTarget );
		}

		#if __PPU
			RENDERPHASEMGR.RegisterRenderPhase(*rage_new CRenderPhaseRainUpdate(gameViewport));
		#endif // __PPU

		//WATER REFLECTION
		{
			CRenderPhaseWaterReflection *waterReflectionNew = rage_new CRenderPhaseWaterReflection(gameViewport);
			RENDERPHASEMGR.RegisterRenderPhase(*waterReflectionNew);

			const int resX = CMirrors::GetResolutionX();
			const int resY = CMirrors::GetResolutionY();

			grcTextureFactory::CreateParams params;

#if RSG_PC
			params.StereoRTMode = grcDevice::MONO;
#endif
			params.Format    = grctfR11G11B10F;
			params.MipLevels = 5;
			grcRenderTarget *pColourTarget = CRenderPhaseWaterReflection::GetRenderTargetColor();
			pColourTarget = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "WATER_REFLECTION_COLOUR",	grcrtPermanent,	  resX, resY, 32, &params WIN32PC_ONLY(, 0, pColourTarget));

#if RSG_PC
			grcRenderTarget *pMSAAColourTarget = CRenderPhaseWaterReflection::GetRenderTargetColor();
			const Settings& settings = CSettingsManager::GetInstance().GetSettings();

			if((settings.m_graphics.m_ReflectionMSAA > 1) && GRCDEVICE.GetDxFeatureLevel() >= 1010)
			{
				//	create a MSAA rendertarget and keep MSAA for depth
				params.Multisample = settings.m_graphics.m_ReflectionMSAA;
				params.MipLevels = 1;

				ComputeVendorMSAA(params);

				pMSAAColourTarget = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "WATER_REFLECTION_COLOUR_MSAA",	grcrtPermanent,	  resX, resY, 32, &params WIN32PC_ONLY(, 0, pMSAAColourTarget));
			}
#endif

			params.Format    = grctfD32FS8;
			params.MipLevels = 1;
			grcRenderTarget *pDepthTarget;
#if RSG_PC
			pDepthTarget = CRenderPhaseWaterReflection::GetDepth();
#endif // RSG_PC
			pDepthTarget = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "WATER_REFLECTION_DEPTH",	grcrtDepthBuffer, resX, resY, 40, &params WIN32PC_ONLY(, 0, pDepthTarget));

#if __D3D11
			grcRenderTarget *pDepthTargetReadOnly       = NULL;
			grcRenderTarget *pDepthTargetCopy           = NULL;
			grcRenderTarget *pDepthTargetCopyOrReadOnly = NULL;
			
			grcTextureFactoryDX11& textureFactory11 = static_cast<grcTextureFactoryDX11&>(grcTextureFactoryDX11::GetInstance()); 
			if (GRCDEVICE.IsReadOnlyDepthAllowed())
			{
				pDepthTargetCopyOrReadOnly = pDepthTargetReadOnly = CRenderPhaseWaterReflection::GetDepthReadOnly();
				pDepthTargetCopyOrReadOnly = pDepthTargetReadOnly = textureFactory11.CreateRenderTarget("WATER_REFLECTION_DEPTH_READONLY", pDepthTarget->GetTexturePtr(), params.Format, DepthStencilReadOnly, pDepthTargetReadOnly, &params);
				waterReflectionNew->SetRenderTargetDepthReadOnly(pDepthTargetReadOnly);
			}
			else
			{
				pDepthTargetCopyOrReadOnly = pDepthTargetCopy = CRenderPhaseWaterReflection::GetDepthCopy();
				pDepthTargetCopyOrReadOnly = pDepthTargetCopy = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "WATER_REFLECTION_DEPTH_COPY",	grcrtDepthBuffer,	resX, resY, 32, &params WIN32PC_ONLY(, 0, pDepthTargetCopyOrReadOnly));
				waterReflectionNew->SetRenderTargetDepthCopy(pDepthTargetCopy);
			}

			CMirrors::SetMirrorWaterDepthTarget_CopyOrReadOnly(pDepthTargetCopyOrReadOnly);
#endif // __D3D11

			CMirrors::SetMirrorWaterRenderTarget(pColourTarget);
			CMirrors::SetMirrorWaterDepthTarget(pDepthTarget);

			waterReflectionNew->SetRenderTargetColor(pColourTarget);
			waterReflectionNew->SetRenderTargetDepth(pDepthTarget);
			Water::SetReflectionRT(pColourTarget);

#if RSG_PC
			CMirrors::SetMirrorWaterMSAARenderTarget(pMSAAColourTarget);
			waterReflectionNew->SetMSAARenderTargetColor(pMSAAColourTarget);
			Water::SetMSAAReflectionRT(pMSAAColourTarget);
#endif
		}
		//=================================================================================================================

		SpewMemCheck("Mid point");

		#if !__PPU
			RENDERPHASEMGR.RegisterRenderPhase(*rage_new CRenderPhaseRainUpdate(gameViewport));
		#endif // !__PPU

		// TODO: Really game viewport?
		RENDERPHASEMGR.RegisterRenderPhase(*rage_new CRenderPhasePreRenderViewport(gameViewport));

		// SCENE RENDER
		// Lights pass.
		g_DefLighting_LightsToScreen = rage_new CRenderPhaseDeferredLighting_LightsToScreen(gameViewport);
		RENDERPHASEMGR.RegisterRenderPhase(*g_DefLighting_LightsToScreen);
		g_DefLighting_LightsToScreen->SetGBufferRenderPhase(g_SceneToGBufferPhaseNew);

		// Mirror reflection
		g_MirrorReflectionPhase = rage_new CRenderPhaseMirrorReflection(gameViewport);
		RENDERPHASEMGR.RegisterRenderPhase(*g_MirrorReflectionPhase);

		// SeeThrough FX
		{
			grcTextureFactory::CreateParams params;
			params.UseFloat = true;
			params.HasParent = true; 
			params.Parent = NULL; 
			params.IsSRGB = false;
			params.Multisample = grcDevice::MSAA_None;		

			params.Format = grctfR11G11B10F;
			grcRenderTarget *pColorTarget	= CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE,"FX_SEETHROUGH_COLOR", grcrtPermanent, SEETHROUGH_LAYER_WIDTH, SEETHROUGH_LAYER_HEIGHT, 32, &params);
			grcRenderTarget *pBlurTarget	= CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE,"FX_SEETHROUGH_BLUR", grcrtPermanent, SEETHROUGH_LAYER_WIDTH, SEETHROUGH_LAYER_HEIGHT, 32, &params);
			grcRenderTarget *pDepthTarget	= CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE,"FX_SEETHROUGH_DEPTH", grcrtDepthBuffer, SEETHROUGH_LAYER_WIDTH, SEETHROUGH_LAYER_HEIGHT, 32);

			g_SeeThroughPhase = rage_new RenderPhaseSeeThrough(gameViewport);
			RENDERPHASEMGR.RegisterRenderPhase(*g_SeeThroughPhase);

			RenderPhaseSeeThrough::SetRenderTargetColor(pColorTarget);
			RenderPhaseSeeThrough::SetRenderTargetBlur(pBlurTarget);
			RenderPhaseSeeThrough::SetRenderTargetDepth(pDepthTarget);
		}

		// Alpha pass.
		CRenderPhaseDrawScene* pAlphaPhaseNew = rage_new CRenderPhaseDrawScene(gameViewport, g_SceneToGBufferPhaseNew);
		RENDERPHASEMGR.RegisterRenderPhase(*pAlphaPhaseNew);
		pAlphaPhaseNew->SetWaterSurfaceRenderPhase(pWaterSurfacePhaseNew);
		Water::SetWaterRenderPhase(pAlphaPhaseNew);
		pAlphaPhaseNew->SetEntityListIndex(g_SceneToGBufferPhaseNew->GetEntityListIndex());

		#if __BANK
			// Debug overlay
			CRenderPhaseDebugOverlay* pDebugOverlayNew = rage_new CRenderPhaseDebugOverlay(gameViewport);
			RENDERPHASEMGR.RegisterRenderPhase(*pDebugOverlayNew);
			pDebugOverlayNew->SetEntityListIndex(g_SceneToGBufferPhaseNew->GetEntityListIndex());
		#endif // __BANK

		#if DEBUG_DISPLAY_BAR
			RENDERPHASEMGR.RegisterRenderPhase(*rage_new CRenderPhaseDebug3d(gameViewport));
		#endif // DEBUG_DISPLAY_BAR

		//================================ UI Layer =======================================
		RENDERPHASEMGR.RegisterRenderPhase(*rage_new CRenderPhaseHud(orthoViewport));

			//Phone Screen
			CRenderPhase *phonePhaseNew = rage_new CRenderPhasePhoneScreen(orthoViewport);
			Assert(phonePhaseNew);
			gRenderTargetMgr.SetRenderPhase(CRenderTargetMgr::RTI_PhoneScreen,phonePhaseNew);
			RENDERPHASEMGR.RegisterRenderPhase(*phonePhaseNew);
			//Phone Model
			RENDERPHASEMGR.RegisterRenderPhase(*rage_new CRenderPhasePhoneModel(gameViewport));

		//leveraging this empty render phase
		RENDERPHASEMGR.RegisterRenderPhase(*rage_new CRenderPhaseFrontEnd(orthoViewport));
		//==================================================================================

		#if DEBUG_DISPLAY_BAR
			RENDERPHASEMGR.RegisterRenderPhase(*rage_new CRenderPhaseDebug2d(orthoViewport));
		#endif // DEBUG_DISPLAY_BAR
		RENDERPHASEMGR.RegisterRenderPhase(*rage_new CRenderPhaseTimeBars(orthoViewport));

		//==============================Lens Distortion======================================
		RENDERPHASEMGR.RegisterRenderPhase(*rage_new CRenderPhaseLensDistortion(orthoViewport));

		#if !__FINAL
			RENDERPHASEMGR.RegisterRenderPhase(*rage_new CRenderPhaseScreenShot(orthoViewport));
		#endif // !__FINAL

		STRVIS_REGISTER_VISFLAG(1 << g_SceneToGBufferPhaseNew->GetEntityListIndex(), false, "GBUF");
		STRVIS_REGISTER_VISFLAG(VIS_FLAG_OPTION_INTERIOR, true, "INT");
		STRVIS_REGISTER_VISFLAG(VIS_FLAG_STREAM_ENTITY, true, "STRM");
		STRVIS_REGISTER_VISFLAG(VIS_FLAG_ONLY_STREAM_ENTITY, true, "ONLY_STRM");
	}

	RENDERPHASEMGR.UpdateViewports();

	SpewMemCheck("End point");
}



#if __BANK

//------------------------------------------------
// Destroy and recreate render phases so that 
// phase type rendering settings will be picked up.
void CViewportManager::RecreateRenderPhases()
{
	RENDERPHASEMGR.UpdateViewports();
}


void CViewportManager::CopyWidgetSettingsToPhaseCB()
{
	fwRenderPhase::SetRenderSettingsOverrideMask(g_widgetRenderSettings.GetFlags());
}

//-----------------------------------------------
// Widgets enabling easy debugging of this system
void CViewportManager::SetupRenderPhaseWidgets(bkBank& bank)
{
#if (1 == __BANK) && (1 == __XENON)
	CSystem::SetupRingBufferWidgets(bank);  
#endif // (1 == __BANK) && (1 == __XENON)
	bank.PushGroup("Display Settings( RenderPhases )", false);
	
#if __DEV //this stuff just doen't work in bank release
	bank.AddToggle("Print RenderPhaseList",&m_bPrintPhaseList);

	bank.PushGroup("Phase Render Settings", false);
	for (s32 j=0;j<RENDER_SETTINGS_MAX;j++)
	{
		bank.AddToggle(CRenderSettings::GetSettingsFlagString(j),g_widgetRenderSettings.GetFlagsPtr(),(1<<j), CopyWidgetSettingsToPhaseCB);
	}
	bank.PopGroup();
#endif //__DEV
	bank.PopGroup();
}
#endif // __BANK




#define	EXTRA_TIMING	0

bool	g_render_lock = false;
//--------------------------------------------------------------
// Executes the final renderphases pointer list at buildlist time
void CViewportManager::BuildRenderLists()
{
	USE_MEMBUCKET(MEMBUCKET_RENDER);

#if __BANK
	if(g_render_lock)
	{
		if(!fwTimer::IsGamePaused())
			fwTimer::StartUserPause(); //ensure game is paused
		if (!CStreaming::IsStreamingPaused())
			CStreaming::SetStreamingPaused(true);
extern	void	ShowCamFrustum(void);
		ShowCamFrustum();

		return;
	}
#endif

	PF_PUSH_TIMEBAR_BUDGETED("Pre Process BuildRenderList",1.0f);
#if	EXTRA_TIMING
	sysPerformanceTimer	time("BuildRenderList");
	time.Start();
#endif



	SN_START_MARKER(1, "Build Render List");  

	CRenderer::SetCanRenderFlags(0,0,0,0,0);//canRenderAlpha,canRenderWater,canRenderScreenDoor,canRenderDecal); //damn these arent set yet will have to set them as we go along
	//CRenderPhase *pDelayedRenderPhases[2]={NULL,NULL};

	CLODLightManager::ResetLODLightsBucketVisibility();

	gStreamingRequestList.EnableRecording(true);

	RENDERPHASEMGR.BuildRenderList();
	
	PF_POP_TIMEBAR(); // Pre process BRL
	// JW : don't know what it is supposed to be waiting on here. try running without it...
	//PF_START_TIMEBAR_DETAIL("WaitOnResults : BuildRenderList");
	//CTaskScheduler::WaitOnResults();

	//DLC((true) dlCmdNewDrawList(DL_UPDATE));
	// for quad tree, go through all the phases and vis tag all quads, also build the load balanced lists of quads needed for issuing the scan tasks
	gWorldMgr.PostScan();
	//DLC((true) dlCmdEndDrawList()); //DL_UPDATE


	// eventually replace this CRenderer call with some other class - CPreScan?
	CRenderer::IssueAllScanTasksNew();
	gPostScan.BuildRenderListsNew();		// construct renderlists from the results of the scan tasks

#if	EXTRA_TIMING
	static	int	count=0;
	count++;
	if(count>200 && (count&0x1)==0)
	{

		time.Stop();
		float	timems=time.GetTimeMS();
		static	float	total=0.0f;
		static	float	tmin=99.0f;
		static	float	tmax=0.0f;
		static	int	innercount=0;
		innercount++;
		total+=timems;
		if(timems>tmax)
			tmax=timems;
		if(timems<tmin)
			tmin=timems;
		if((count&0xf)==0)
			printf(" BuildRenderList %f average %f min %f max %f    count %d\n",timems,total/(float)innercount,tmin,tmax,innercount/16);

	}
#endif
	SN_STOP_MARKER(1);

	gStreamingRequestList.EnableRecording(false);
}

#if RSG_PC && __DEBUG
const float kEpsilon = 0.000001f;
bool ApproximatelyEqual(float _a, float _b) 
{ 
	float d = abs(_a - _b);
	return d < kEpsilon; 
}

bool ApproximatelyEqual(const Vector3& _a, const Vector3& _b) 
{
	Vector3 _c = _a - _b;
	float d = _c.Mag();
	return d < kEpsilon; 
}
#endif

void CViewportManager::RenderPhasesDrawInit(void)
{
	// PF_AUTO_PUSH_DETAIL("CViewportManager::RenderPhasesDrawInit");

	// Make sure that all rendering of viewports begins with a clean
	// default render state.

	grcBindTexture(NULL);

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
	
	Lights::SetupDirectionalAndAmbientGlobals();

	CShaderLib::SetGlobalAlpha(1.0f);
	CShaderLib::SetAlphaRefConsts();

#if DEVICE_MSAA
	CShaderLib::SetMSAAParams( GRCDEVICE.GetMSAA(),
# if DEVICE_EQAA
		GRCDEVICE.IsEQAA() );
# else
		false );
# endif // DEVICE_EQAA
#endif // DEVICE_MSAA

#if RSG_ORBIS || __D3D11
	GRCDEVICE.ResetClipPlanes();
#endif
#if RSG_ORBIS && __DEV
	GRCDEVICE.ResetDrawCallCount();
#endif

#if RSG_PC
	if (GRCDEVICE.CanUseStereo() && GRCDEVICE.IsStereoEnabled())
	{
		Vector3 vLeft(0,0,0);
		Vector3 vRight(0,0,0);

		CViewport* pGameViewport = gVpMan.GetGameViewport();
		if (pGameViewport)
		{
			const grcViewport &vp = pGameViewport->GetGrcViewport();

			Matrix44 ViewProj;
			ViewProj.Dot(RCC_MATRIX44(vp.GetProjection()),RCC_MATRIX44(vp.GetViewMtx()));

			Matrix44 InvCam44(RCC_MATRIX44(vp.GetCameraMtx44()));
			InvCam44.Inverse();

			Matrix44 InvViewProj;
			InvViewProj.Inverse(ViewProj);

			Matrix44 I;
			I.Dot(InvViewProj,ViewProj);

			Vector4 RoundTripCamPosHom(0,0,1,0);
			Vector4 RoundTripCamPosHomOut;
			InvViewProj.Transform(RoundTripCamPosHom,RoundTripCamPosHomOut);
			RoundTripCamPosHomOut /= RoundTripCamPosHomOut.GetW();

#if __DEBUG
			Assert(ApproximatelyEqual(RCC_MATRIX44(vp.GetCameraMtx44()).d.GetVector3(),RoundTripCamPosHomOut.GetVector3()));
#endif 

			float fEyeSep = GRCDEVICE.GetEyeSeparation();
			float fSep = GRCDEVICE.GetSeparationPercentage(false);
			float fConv = GRCDEVICE.GetConvergenceDistance();

			Vector4 ClipSpaceCamOffset_Left(fEyeSep * fSep * 0.01f * -fConv, 0.0f,0.0f,0.0f);
			Vector4 WorldSpaceCamOffset_Left;
			InvViewProj.Transform(ClipSpaceCamOffset_Left,WorldSpaceCamOffset_Left);

			Vector4 ClipSpaceCamOffset_Right(fEyeSep * fSep * 0.01f * fConv, 0.0f,0.0f,0.0f);
			Vector4 WorldSpaceCamOffset_Right;
			InvViewProj.Transform(ClipSpaceCamOffset_Right,WorldSpaceCamOffset_Right);

#if __DEBUG
			Assert(ApproximatelyEqual(WorldSpaceCamOffset_Left.GetVector3().Mag(), WorldSpaceCamOffset_Right.GetVector3().Mag()));

			Assert(ApproximatelyEqual(-WorldSpaceCamOffset_Right.GetVector3(),WorldSpaceCamOffset_Left.GetVector3()));
#endif 

			vLeft.Set(Vector3(WorldSpaceCamOffset_Left.GetX(),WorldSpaceCamOffset_Left.GetY(),WorldSpaceCamOffset_Left.GetZ()));
			vRight.Set(Vector3(WorldSpaceCamOffset_Right.GetX(),WorldSpaceCamOffset_Right.GetY(),WorldSpaceCamOffset_Right.GetZ()));

			sm_HalfEyeSeparation = Max(vLeft.Mag(), vRight.Mag());

			GRCDEVICE.SetConvergenceDistance(camInterface::GetStereoConvergence());

			// we are forced to adjust convergence, so ignore user input
			if (camInterface::GetStereoConvergence() != GRCDEVICE.GetDefaultConvergenceDistance())
				GRCDEVICE.IgnoreStereoMsg();
			else
				GRCDEVICE.IgnoreStereoMsg(false);
		}
		GRCDEVICE.UpdateStereoTexture(vLeft,vRight);
	}
	CShaderLib::SetStereoTexture();
#endif	

	CShaderLib::SetTreesParams();
}

void CViewportManager::DLCRenderPhaseDrawInit()
{
	DLC( dlCmdRenderPhasesDrawInit, ());
}

//------------------------------------------------------------
// Executes the final renderphases pointer list at render time
void CViewportManager::RenderPhases()
{	
#if !__FINAL
    gDrawListMgr->ResetDrawListEntityCounts();
#endif

	//RenderPhasesDrawInit();
	DLC(dlCmdNewDrawList, (DL_RENDERPHASE_PREAMBLE));
	DLC( dlCmdRenderPhasesDrawInit, ());
	gDrawListMgr->EndCurrentDrawList();

	RENDERPHASEMGR.BuildDrawList();


}




//******************************************************************************
//******************************************************************************
//******************************************************************************
//******************************************************************************
// END RENDER PHASE SUPPORT
//******************************************************************************
//******************************************************************************
//******************************************************************************
//******************************************************************************

//---------------------------------------------------
//
bool CViewportManager::IsUsersMonitorWideScreen()
{
#if !GRCDEVICE_IS_STATIC
	return g_pGRCDEVICE && g_pGRCDEVICE->GetWideScreen();
#elif 1 // RAGE to fix?
	return CHudTools::GetWideScreen();
#else
	s32 deviceWidth = GRCDEVICE.GetWidth();
	s32 deviceHeight = GRCDEVICE.GetHeight();
	float screenRatio = (float)deviceWidth/(float)deviceHeight;
	float r16_9 = 16.0f/9.0f;
	float r4_3  = 4.0f/3.0f;
	float midPoint = r4_3+((r16_9 - r4_3)/2.0f);
	return (screenRatio>midPoint);
#endif
}

//----------------------------------------------------------------
//
void CViewportManager::SetWidescreenBorders(bool b, s32 duration)
{
	bool bUsersMonitorIsWidescreen = IsUsersMonitorWideScreen();
#ifndef DRAW_WIDESCREEN_BORDERS_ON_TOP_ALWAYS	
	CViewport *pGameVp		= GetGameViewport();
#endif
	CViewport *pPrimOrthoVp = GetPrimaryOrthoViewport();

	m_timeScrollOnWideScreenBorders = duration;

	m_timeWideScreenBordersCommandSet = fwTimer::GetNonPausableCamTimeInMilliseconds();

	if (!bUsersMonitorIsWidescreen)	// 4:3
	{
		#ifndef DRAW_WIDESCREEN_BORDERS_ON_TOP_ALWAYS
		// animate the GAME viewport ( if multiple viepworts on the go, might need to rethink this )
		if (!m_bWidescreenBorders)
		{
			Vector2 br(1.0f,0.875f);
			pGameVp->LerpTo(&tl,&br,GetTimeScrollOnWideScreenBorders());
		}
		else
		{
			Vector2 tl(0.0f,0.0f);
			Vector2 br(1.0f,1.0f);
			pGameVp->LerpTo(&tl,&br,GetTimeScrollOnWideScreenBorders());
		}
		#endif // DRAW_WIDESCREEN_BORDERS_ON_TOP_ALWAYS
	}

	// for a widescreen monitor small bars are required to be drawn.
	// for a 4:3 monitor the back buffer needs regions cleared.
	if (b)
		pPrimOrthoVp->SetDrawWidescreenBorders(true); // will falsify itself when the time expires itself...

	m_bWidescreenBorders = b;
}

//----------------------------------------------------------------
//
void CViewportManager::InitDLCCommands()
{
	DLC_REGISTER_EXTERNAL(dlCmdRenderPhasesDrawInit);
}

//------------------------------------------------------
//
void CViewportManager::RecacheViewports()
{
	CacheGameViewport();
	CachePrimaryOrthoViewport();
	CacheGameViewportIndex();
	CachePlayerSettingsViewport();

#if PORTCULLIS_ENABLE
	if(rage::portcullis::DoCheck() == false)
	{
		Displayf("Possible access violation code #%d. Talk to Klaas", fwRandom::GetRandomNumberInRange(101, 999));
		m_pGameViewport = (CViewport*)((size_t)m_pGameViewport | 0xE77DEBDE);
		return;
	}
#endif // PORTCULLIS_ENABLE

#if DEBUG_DRAW
	grcDebugDraw::SetCullingViewport(&GetGameViewport()->GetGrcViewport());
#endif // DEBUG_DRAW
}

void CViewportManagerWrapper::Process()
{
    gVpMan.Process();
}

void CViewportManagerWrapper::CreateFinalScreenRenderPhaseList()
{
    gVpMan.CreateFinalScreenRenderPhaseList();
}


#if RSG_PC
//------------------------------------------------------
//
float CViewportManager::GetAspectRatio()
{
	// Temp implementation
	// TODO4FIVE allow user specified aspect as we did in IV?
	// TODO4FIVE get aspect ratio from max resolution of monitor (if in fullscreen) as we did in IV?

	const CViewport* viewport	= GetGameViewport();
	float aspectRatio			= viewport ? viewport->GetGrcViewport().GetAspectRatio() : (16.0f / 9.0f);

	return aspectRatio;
}
#endif //RSG_PC
