//
// filename:	RenderPhaseDebugNY.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------

#include "grcore/debugdraw.h"
#include "debug/Debug.h"

#if DEBUG_DISPLAY_BAR

// C headers
// Framework headers
#include "fwscene/search/Search.h"
#include "fwscene/scan/Scan.h"
#include "video/VideoPlayback.h"

// Rage headers
#include "grcore/allocscope.h" 
#include "system/param.h" 
#include "pheffects/mouseinput.h"
#include "profile/timebars.h"
// Game headers
#include "audio/northaudioengine.h"
#include "camera/CamInterface.h"
#include "camera/viewports/Viewport.h"
#include "camera/viewports/ViewportManager.h"
#include "debug/bugstar/BugstarIntegrationSwitch.h"
#include "debug/DebugDraw.h"
#include "debug/DebugDraw/DebugHangingCableTest.h"
#include "debug/debugscene.h"
#include "debug/LightProbe.h"
#include "debug/NodeViewer.h"
#include "debug/TextureViewer/TextureViewer.h"
#include "debug/Rendering/DebugRendering.h"
#include "debug/OverviewScreen.h"
#include "debug/TiledScreenCapture.h"
#include "debug/UiGadget/UiGadgetBase.h"
#include "Event/ShockingEvents.h"
#include "game/weather.h"
#include "game/clock.h"
#include "Peds/rendering/PedDamage.h"
#include "renderer/DrawLists/DrawListMgr.h"
#include "renderer/occlusion.h"
#include "renderer/PlantsMgr.h"
#include "renderer/PostProcessFXHelper.h"
#include "renderer/rendertargets.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/zonecull.h"
#include "renderer/SpuPM/SpuPmMgr.h"
#include "renderer/ScreenshotManager.h"
#include "SaveLoad/savegame_photo_manager.h"
#include "scene/debug/PostScanDebug.h"
#include "scene/decorator/decoratorinterface.h"
#include "scene/downloadabletexturemanager.h"
#include "scene/scene.h"
#include "scene/EntityBatch.h"
#include "scene/portals/portalDebugHelper.h"
#include "scene/portals/Portal.h"
#include "scene/Volume/VolumeManager.h"
#include "System/ControlMgr.h"
#include "Peds/PopCycle.h"
#include "Peds/PedEventScanner.h"
#include "Network/NetworkInterface.h"
#include "Animation/debug/AnimViewer.h"
#include "Script/script.h"
#include "Script/script_hud.h"
#include "Vfx/visualeffects.h"
#include "Vfx/misc/MovieManager.h"
#include "physics/physics.h"
#include "Text/TextFormat.h"
#include "Peds/rendering/PedHeadshotManager.h"
#include "renderer/RenderPhases/RenderPhaseDebugNY.h"

#if __XENON
//OPTIMISATIONS_OFF()
#endif


// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------
// Custom draw list commands
#if DEBUG_DRAW
class dlCmdDebug2dStaticRender : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_Debug2dStaticRender,
	};

	explicit dlCmdDebug2dStaticRender(u32 numBatchers)	{ m_numBatchers = numBatchers; }
	s32 GetCommandSize()								{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase & cmd)			{ CRenderPhaseDebug2d::StaticRender(((dlCmdDebug2dStaticRender&)cmd).m_numBatchers); }

private:
	u32 m_numBatchers;
};
#endif // DEBUG_DRAW

class dlCmdDebug3dStaticRender : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_Debug3dStaticRender,
	};

	explicit dlCmdDebug3dStaticRender(u32 numBatchers)	{ m_numBatchers = numBatchers; }
	s32 GetCommandSize()								{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase & cmd)			{ CRenderPhaseDebug3d::StaticRender(((dlCmdDebug3dStaticRender&)cmd).m_numBatchers); }

private:
	u32 m_numBatchers;
};

class dlCmdDebugRenderReleaseInfo : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_DebugRenderReleaseInfo,
	};

	s32 GetCommandSize()						{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase & /*cmd*/)	{ CDebug::RenderReleaseInfoToScreen(); }
};

// --- Globals ------------------------------------------------------------------

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------

/// ***********************
// **** DEBUG2D PHASE ****
// ***********************

#if DEBUG_DISPLAY_BAR
grcRasterizerStateHandle CRenderPhaseDebug2d::sm_rasterizerState;
#endif // DEBUG_DISPLAY_BAR

#if DEBUG_DRAW

//
// name:		PrintDebugInfo
static void PrintDebugInfo()
{
#if __BANK
	camInterface::DebugDisplayCameraInfoText();
#endif

#if __DEV
	char lString[255];
	if (CPortalDebugHelper::BuildCamLocationString(lString))
	{
		grcDebugDraw::AddDebugOutput(lString);
	}
#endif // __DEV

	CPopCycle::Display();

#if __BANK
	NetworkInterface::PrintNetworkInfo();  // display debug text for the network code
#endif

	DLC_Add(CDebug::RenderDebugScaleformInfo);
	DLC(dlCmdDebugRenderReleaseInfo, ());
}

void CRenderPhaseDebug2d::StaticRender(u32 numBatchers)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	CRenderTargets::LockUIRenderTargets();

	CSprite2d::SetAutoRestoreViewport(true);

	grcStateBlock::SetStates(sm_rasterizerState, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Normal);
	CShaderLib::SetGlobalEmissiveScale(1.0f,true);

	grcLightState::SetEnabled(false);

	grcBindTexture(NULL);

	grcWorldIdentity();

#if __BANK
	//CRenderPhaseCascadeShadowsInterface::DebugDraw_render(); -- doesn't work anymore here, since the depth buffer is not available
	CDebugTextureViewerInterface::Render();

	PF_START_TIMEBAR_DETAIL("Ped Damage Debug");
	CPedDamageManager::DebugDraw();
#endif

	PF_START_TIMEBAR_DETAIL("LastGen Message");
	DebugDraw::RenderLastGen();

	PF_START_TIMEBAR_DETAIL("Render Profiler");
	CDebug::RenderProfiler();

	PF_PUSH_TIMEBAR_DETAIL("MovieManager Debug");
	g_movieMgr.RenderDebug();
	PF_POP_TIMEBAR_DETAIL();

#if __BANK
	PF_PUSH_TIMEBAR_DETAIL("MovieManager Debug");
	CPhotoManager::RenderCameraPhotoDebug();
	PHONEPHOTOEDITOR.RenderDebug();
	VIDEO_PLAYBACK_ONLY(VideoPlayback::RenderVideoOverlayBank());
// 	if (CLowQualityScreenshot::IsRendering() && CLowQualityScreenshot::GetRenderDebug())
// 	{
// 		CLowQualityScreenshot::Render(false);
// 	}
	PF_POP_TIMEBAR_DETAIL();

	PF_PUSH_TIMEBAR_DETAIL("PedHeadshotManager Debug");
	PedHeadshotManager::DebugRender();
	PF_POP_TIMEBAR_DETAIL();

	PF_PUSH_TIMEBAR_DETAIL("DownloadableTextureManager Debug");
	DOWNLOADABLETEXTUREMGRDEBUG.Render();
	PF_POP_TIMEBAR_DETAIL();

	PF_START_TIMEBAR_DETAIL("Audio Debug");
	audNorthAudioEngine::DebugDraw();

#	if RSG_DURANGO
	// adding support for latency measurement (B*2058011 : use light sensor to measure input latency)
	if (CDebug::sm_bDisplayLightSensorQuad)
	{
		bool any =	CControlMgr::GetDebugPad().GetButtonCircle()	||
			CControlMgr::GetDebugPad().GetButtonCross()				||
			CControlMgr::GetDebugPad().GetButtonSquare()			||
			CControlMgr::GetDebugPad().GetButtonTriangle()			||
			CControlMgr::GetDebugPad().GetRightShoulder2()			||
			CControlMgr::GetDebugPad().GetLeftShoulder2();

		if (any)	CSprite2d::DrawRect(0.0f, 0.0f, 0.25f, 0.25f, 0.0f, Color_white);
		else		CSprite2d::DrawRect(0.0f, 0.0f, 0.25f, 0.25f, 0.0f, Color_black);
	}
#	endif
#endif

#if SPUPMMGR_TEST
	SpuPM_RenderTestTexture();
#endif

	PF_START_TIMEBAR_DETAIL("2D Debug Render");
	grcDebugDraw::Render2D(numBatchers);

	CSprite2d::SetAutoRestoreViewport(false);
	
	CRenderTargets::UnlockUIRenderTargets();
}

#endif // DEBUG_DRAW

CRenderPhaseDebug2d::CRenderPhaseDebug2d(CViewport* pGameViewport)
: CRenderPhase(pGameViewport, "Debug2D", DL_RENDERPHASE_DEBUG, 0.0f, false)
{
	USE_DEBUG_MEMORY();

	grcRasterizerStateDesc rasterizerDesc;
	rasterizerDesc.CullMode = grcRSV::CULL_NONE;
	rasterizerDesc.AntialiasedLineEnable = TRUE;
	sm_rasterizerState = grcStateBlock::CreateRasterizerState(rasterizerDesc);
}

void CRenderPhaseDebug2d::UpdateViewport()
{
#if __BANK
	TiledScreenCapture::Update();
#endif // __BANK
}

//----------------------------------------------------
// build and execute the drawlist for this renderphase
void CRenderPhaseDebug2d::BuildDrawList()
{ 
	if (!GetViewport()->IsActive() || !camInterface::IsInitialized())
	{
		return;
	}

	DrawListPrologue();

	DURANGO_ONLY(DLC_Add(CRenderTargets::LockUIRenderTargets);)

	DLC(dlCmdSetCurrentViewport, (&GetGrcViewport()));

	DLC_Add(&CTextFormat::FlushFontBuffer);

#if __BANK
	TiledScreenCapture::BuildDrawList();
	DLC_Add(CScriptHud::DrawScriptScaleformDebugInfo);
#endif // __BANK

#if DEBUG_DRAW
	CRenderPhaseCascadeShadowsInterface::DebugDraw_update();
	PrintDebugInfo();

	// Close the main threads batchers, so that any further main thread debug drawing is forced to the next render frame.
	grcDebugDraw::CloseBatcher(grcDebugDraw::LINGER_2D);
	DLC(dlCmdDebug2dStaticRender, (grcDebugDraw::GetNumBatchers(grcDebugDraw::LINGER_2D)));
	grcDebugDraw::ClearRenderState();
#else
	DLC_Add(CDebug::RenderDebugScaleformInfo);
	DLC_Add(CDebug::RenderReleaseInfoToScreen);
#endif

//#if __BANK
//	DLC_Add(CRenderTargets::RenderBank, 0);
//#endif

	DLC(dlCmdSetCurrentViewportToNULL, ());

	DURANGO_ONLY(DLC_Add(CRenderTargets::UnlockUIRenderTargets, false);)

	DrawListEpilogue();
}

// ***********************
// **** DEBUG3D PHASE ****
// ***********************

grcRasterizerStateHandle CRenderPhaseDebug3d::sm_rasterizerState;

void CRenderPhaseDebug3d::StaticRender(u32 DEBUG_DRAW_ONLY(numBatchers))
{
#if __BANK //this whole class should not exist in non-bank but this cleans up the ifdef hell below a bit and preserves most of the logic...
	CShaderLib::SetGlobalEmissiveScale(1.0f,true);

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	CRenderTargets::LockUIRenderTargets();

	CSprite2d::SetAutoRestoreViewport(true);

	if (grcDebugDraw::GetGlobalDoDebugZTest())
	{
		grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
	}
	else
	{
		grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	}

	grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);
	grcStateBlock::SetRasterizerState(sm_rasterizerState);

	grcLightState::SetEnabled(true);

#if __DEV
	CDebugScene::RenderModelViewer();
#endif // __DEV

	CDebugScene::Render_RenderThread();

	COverviewScreenManager::Render();

	grcBindTexture(NULL);
	grcLightState::SetEnabled(false);


	// Debug Rendering draw
	DebugRenderingMgr::Draw();

	LightProbe::Render();

#if __DEV
#if HANGING_CABLE_TEST
	CHangingCableTestInterface::Render();
#endif // HANGING_CABLE_TEST
#endif // __DEV

#if DEBUG_DRAW
	fwSearch::DrawDebug();
	CPostScanDebug::Draw();
#endif // DEBUG_DRAW

	// visual effects debug
	CVisualEffects::RenderDebug();


	// visual effects debug
#if VOLUME_SUPPORT
	g_VolumeManager.RenderDebug();
#endif // __BANK && VOLUME_SUPPORT

#if DECORATOR_DEBUG
	gp_DecoratorInterface->RenderDebug();
#endif // DECORATOR_DEBUG
	
	CNodeViewer::Render();
	
	PF_START_TIMEBAR_DETAIL("CCullZones Render");
	CCullZones::Render();

	PF_START_TIMEBAR_DETAIL("Ped Damage Debug");
	CPedDamageManager::DebugDraw3D();

	PF_START_TIMEBAR_DETAIL("Local Shadow Debug");
	CParaboloidShadow::BankDebugDraw();

	PF_START_TIMEBAR_DETAIL("CPlantMgr Debug");
	CPlantMgr::RenderDebugForward();

#if __BANK
	PF_START_TIMEBAR_DETAIL("CEntityBatch Debug");
	CEntityBatch::RenderDebug();
#endif

	// render all debug lines/sphere/polys etc
#if(DEBUG_RENDER_BUG_LIST)
	PF_START_TIMEBAR_DETAIL("Buglist Render");
	CDebug::RenderTheBugList();
#endif // DEBUG_RENDER_BUG_LIST
#if DEBUG_DRAW
	PF_START_TIMEBAR_DETAIL("3D Debug Render");

	grcDebugDraw::SetCameraPosition(RCC_VEC3V(camInterface::GetPos()));
	grcDebugDraw::Render3D(numBatchers);

#if  __PFDRAW
	if (CPhysics::ms_mouseInput)
	{
		CPhysics::ms_mouseInput->Draw();
	}
#endif // __PFDRAW 
#endif // DEBUG_DRAW

	PF_START_TIMEBAR_DETAIL("Occluder Render");
	COcclusion::DrawDebug();

	CSprite2d::SetAutoRestoreViewport(false);
	
	CRenderTargets::UnlockUIRenderTargets();
#endif //__BANK
}

CRenderPhaseDebug3d::CRenderPhaseDebug3d(CViewport* pGameViewport)
: CRenderPhase(pGameViewport, "Debug3d", DL_RENDERPHASE_DEBUG, 0.0f, false)
{
	USE_DEBUG_MEMORY();

	grcRasterizerStateDesc rasterizerDesc;
	rasterizerDesc.CullMode = grcRSV::CULL_NONE;
	rasterizerDesc.AntialiasedLineEnable = TRUE;
	sm_rasterizerState = grcStateBlock::CreateRasterizerState(rasterizerDesc);
}

//----------------------------------------------------
// build and execute the drawlist for this renderphase
void CRenderPhaseDebug3d::BuildDrawList()
{
	if (!GetViewport()->IsActive() || !camInterface::IsInitialized())
	{
		return;
	}

	DrawListPrologue();

#if USE_INVERTED_PROJECTION_ONLY
	grcViewport viewport = GetGrcViewport();
	viewport.SetInvertZInProjectionMatrix( true );
#else
	const grcViewport& viewport = GetGrcViewport();
#endif

	DLC(dlCmdSetCurrentViewport, (&viewport));

	// Close the main threads batchers, so that any further main thread debug drawing is forced to the next render frame.
	DEBUG_DRAW_ONLY(grcDebugDraw::CloseBatcher(grcDebugDraw::LINGER_3D);)
	DLC(dlCmdDebug3dStaticRender, (0 DEBUG_DRAW_ONLY(+grcDebugDraw::GetNumBatchers(grcDebugDraw::LINGER_3D))));

	DLC(dlCmdSetCurrentViewportToNULL, ());

	DrawListEpilogue();
}

CRenderPhaseScreenShot::CRenderPhaseScreenShot(CViewport* pGameViewport)
: CRenderPhase(pGameViewport, "Screenshot", DL_RENDERPHASE_DEBUG, 0.0f, false)
{
}

void CRenderPhaseScreenShot::BuildDrawList()
{
	DrawListPrologue();

#if BUGSTAR_INTEGRATION_ACTIVE
	PF_START_TIMEBAR_DETAIL("BugRequests");
	CDebug::HandleBugRequests();
#endif // BUGSTAR_INTEGRATION_ACTIVE

	DrawListEpilogue();
}

namespace CRenderPhaseDebug
{
	void InitDLCCommands()
	{
#if DEBUG_DRAW
		DLC_REGISTER_EXTERNAL(dlCmdDebug2dStaticRender);
#endif // DEBUG_DRAW
		DLC_REGISTER_EXTERNAL(dlCmdDebug3dStaticRender);
		DLC_REGISTER_EXTERNAL(dlCmdDebugRenderReleaseInfo);
	}

} // namespace CRenderPhaseDebug

#endif //DEBUG_DISPLAY_BAR


