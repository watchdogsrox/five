//
// filename:	RenderPhaseStd.cpp
// description:	
// 

// --- Include Files ------------------------------------------------------------
#include "RenderPhaseStd.h"
// C headers

// Rage headers
#include "bank/bkmgr.h"
#include "grcore/allocscope.h"
#include "grcore/device.h"
#include "grcore/quads.h"
#include "grcore/setup.h"
#include "grcore/stateblock.h"
#include "grmodel/setup.h"
#include "grprofile/timebars.h"
#include "input/input.h"
#include "profile/profiler.h"
#include "script/script.h"
#if RSG_ORBIS
#include "grcore/rendertarget_gnm.h"
#endif

#include "fwanimation/animmanager.h"
#include "fwscene/scan/scan.h"
#include "fwscene/stores/txdstore.h"
#include "video/VideoPlaybackThumbnailManager.h"

#if __XENON
#define DBG 0
# include "system/xtl.h"
#include "system/d3d9.h"
# include <xgraphics.h>
#endif

// Game headers
#include "Network/NetworkInterface.h"
#include "Network/SocialClubServices/SocialClubNewsStoryMgr.h"
#include "camera/helpers/Frame.h"
#include "camera/CamInterface.h"
#include "camera/system/CameraManager.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/aim/FirstPersonAimCamera.h"
#include "camera/viewports/ViewportManager.h"
#include "Control/stuntjump.h"
#include "control/videorecording/videorecording.h"
#include "cutscene/CutSceneManagerNew.h"
#include "debug/bar.h"
#include "debug/Rendering/DebugDeferred.h"
#include "debug/Rendering/DebugLighting.h"
#include "debug/TiledScreenCapture.h"
#include "frontend/BusySpinner.h"
#include "frontend/HudMarkerManager.h"
#include "frontend/NewHud.h"
#include "Frontend/UIReplayScaleformController.h"
#include "frontend/DisplayCalibration.h"
#include "frontend/MiniMap.h"
#include "frontend/MiniMapRenderThread.h"
#if RSG_PC
#include "frontend/MousePointer.h"
#include "frontend/MultiplayerChat.h"
#endif // #if RSG_PC
#include "frontend/MultiplayerGamerTagHud.h"
#include "frontend/Credits.h"
#include "frontend/GolfTrail.h"
#include "frontend/MobilePhone.h"
#include "frontend/PauseMenu.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/ReportMenu.h"
#include "frontend/WarningScreen.h"
#include "frontend/Store/StoreScreenMgr.h"
#include "frontend/GameStreamMgr.h"
#if GTA_REPLAY
#include "frontend/VideoEditor/ui/Editor.h"
#include "frontend/VideoEditor/VideoEditorInterface.h"
#include "frontend/VideoEditor/ui/TextTemplate.h"
#include "frontend/VideoEditor/ui/WatermarkRenderer.h"
#endif
#include "game/clock.h"
#include "Game/Weather.h"
#include "modelinfo/VehicleModelInfo.h"
#include "Objects/ObjectPopulation.h"
#include "pathserver/pathserver.h"
#include "peds/ped.h"
#include "peds/rendering/PedHeadshotManager.h"
#include "profile/trace.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/Deferred/GBuffer.h"
#include "renderer/DrawLists/drawListMgr.h"
#include "renderer/GtaDrawable.h"
#include "renderer/MeshBlendManager.h"
#include "renderer/Mirrors.h"
#include "renderer/OcclusionQueries.h"
#include "renderer/PostProcessFX.h"
#include "renderer/RenderListBuilder.h"
#include "renderer/renderListGroup.h"
#include "renderer/RenderTargetMgr.h"
#include "renderer/rendertargets.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseDebugOverlay.h"
#include "renderer/RenderPhases/RenderPhaseDefLight.h"
#include "renderer/RenderPhases/RenderPhaseEntitySelect.h"
#include "renderer/RenderPhases/RenderPhaseMirrorReflection.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/RenderPhases/RenderPhaseWater.h"
#include "renderer/RenderPhases/RenderPhaseWaterReflection.h"
#include "renderer/River.h"
#include "renderer/SSAO.h"
#include "renderer/lights/lights.h"
#include "renderer/occlusion.h"
#include "renderer/ScreenshotManager.h"
#include "renderer/shadows/shadows.h"
#include "renderer/UI3DDrawManager.h"
#include "renderer/water.h"
#include "renderer/HorizonObjects.h"
#include "renderer/ApplyDamage.h"
#include "replaycoordinator/ReplayCoordinator.h"
#include "SaveLoad/savegame_messages.h"
#include "SaveLoad/savegame_photo_manager.h"
#include "scene/portals/portalTracker.h"
#include "scene/scene.h"
#include "scene/world/GameWorld.h"
#include "script/script.h"
#include "script/script_hud.h"
#include "script/streamedscripts.h"
#include "shaders/CustomShaderEffectCable.h"
#include "shaders/ShaderLib.h"
#include "streaming/streamingengine.h"
#include "system/ControlMgr.h"
#include "system/system.h"
#include "text/TextFormat.h"
#include "timecycle/TimeCycle.h"
#include "vfx/VisualEffects.h"
#include "vfx/VfxHelper.h"
#include "vfx/particles/PtFxManager.h"
#include "vfx/misc/LODLightManager.h"
#include "vfx/misc/DistantLightsVertexBuffer.h"
#include "vfx/Misc/Markers.h"
#include "vfx/Misc/MovieMeshManager.h"
#include "vfx/misc/Puddles.h"
#include "pickups/Pickup.h"
#include "Weapons/weapon.h"

#include "frontend/SocialClubMenu.h"
#include "frontend/PolicyMenu.h"
#include "frontend/UIWorldIcon.h"
#if RSG_PC
#include "frontend/TextInputBox.h"
#endif // RSG_PC

#if __BANK
#include "vehicles/vehicleDamage.h"
#endif

#if GTA_REPLAY
#include "control/replay/replayinternal.h"
#endif // GTA_REPLAY

#if __DEV
//OPTIMISATIONS_OFF()
#endif

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------
enum DrawSceneGPUIds {

};

// --- Structure/Class Definitions ----------------------------------------------
// Custom draw list commands
class dlCmdProcessNonDepthFX : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_ProcessNonDepthFX,
	};

	s32 GetCommandSize()						{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase & /*cmd*/)	{ PostFX::ProcessNonDepthFX(); }
};

// --- Globals ------------------------------------------------------------------

// profiling globals
PF_PAGE(BuildRenderPhaseListPage,"BuildRenderPhaseList");

PF_GROUP(BuildRenderList);
PF_LINK(BuildRenderPhaseListPage,BuildRenderList);
PF_TIMER(CRenderPhaseDrawSceneRL,BuildRenderList);

PF_PAGE(RenderPhasePage,"RenderPhase");

PF_GROUP(RenderPhase);
PF_LINK(RenderPhasePage,RenderPhase);
PF_TIMER(CRenderPhaseDrawScene,RenderPhase);
PF_TIMER(CRenderPhaseSpeedTreeBillboards,RenderPhase);

// --- Static Globals -----------------------------------------------------------
#if __PS3
static BankBool g_bForwardRenderWaitsOnBubbleFence = false;
#endif

static grcBlendStateHandle s_lateVehicleBlend;
static grcRasterizerStateHandle s_lateVehicleRasterizer;

static CRenderPhaseDrawScene* g_DrawSceneRenderPhase         = NULL;
static grcViewport*         g_DrawSceneRenderPhaseViewport  = NULL;

__THREAD static bool ms_lateVehicleInteriorAlphaStateRT = false;
__THREAD static u32 ms_renderAlphaStateRT = RENDER_ALPHA_ALL;
static fwInteriorLocation ms_cameraInteriorLocationRT;

extern const CRenderPhaseScanned* m_WaterRenderPhase;

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------


// **********************************
// **** VIEWPORT PRERENDER PHASE ****
// **********************************

//
// name:		CRenderPhasePreRenderViewport::CRenderPhasePreRenderViewport
// description:	Constructor for prerender render phase
CRenderPhasePreRenderViewport::CRenderPhasePreRenderViewport(CViewport *pViewport)
: CRenderPhase(pViewport, "PreRenderViewport", DL_RENDERPHASE_PRE_RENDER_VP, 0.0f, false)
{
}

//----------------------------------------------------
// build and execute the drawlist for this renderphase
void CRenderPhasePreRenderViewport::BuildDrawList()
{
	if (!GetViewport()->IsActive())
	{
		return;
	}

	DrawListPrologue();

#if RSG_PC
	DLC_Add(PostFX::DoPauseResolve, PostFX::GetPauseResolve());
	if(PostFX::GetRequireResetDOFRenderTargets() && gRenderThreadInterface.IsUsingDefaultRenderFunction())
	{
		DLC_Add(PostFX::ResetDOFRenderTargets);
		PostFX::SetRequireResetDOFRenderTargets(false);
	}
#endif

	if(CLODLightManager::m_requireRenderthreadFrameInfoClear && gRenderThreadInterface.IsUsingDefaultRenderFunction()) {
		CLODLightManager::ClearRenderthreadFrameInfo();
		CLODLightManager::m_requireRenderthreadFrameInfoClear = false;
	}

#if DYNAMIC_BB
#if !DYNAMIC_GB
	DLC_Add(CRenderTargets::ResetAltBackBuffer);
#else
	DLC_Add(CRenderTargets::SetAltBackBuffer);
#endif
#endif
#if DYNAMIC_GB
	{
		DLC_Add(GBuffer::InitAltGBuffer,GBUFFER_RT_0);
		DLC_Add(GBuffer::InitAltGBuffer,GBUFFER_RT_1);
		DLC_Add(GBuffer::InitAltGBuffer,GBUFFER_RT_2);
		DLC_Add(GBuffer::InitAltGBuffer,GBUFFER_RT_3);
	}
#endif

	DLC(dlCmdSetCurrentViewport, (&GetGrcViewport()));
	DLC(dlCmdSetCurrentViewportToNULL, ());

	DrawListEpilogue();
}


// ***********************
// **** TIMEBAR PHASE ****
// ***********************

void CRenderPhaseTimeBars::StaticRenderCall(){
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));		// this command is forbidden outwith the render thread

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

#if ENABLE_CDEBUG && RAGE_TIMEBARS
	int callCount=0;
	const int RenderThreadID = 3; // Stream is in between main and render thread now
	static int MainThreadID = 1;
	g_pfTB.GetFunctionTotals("Wait for render", callCount, CDebug::sm_fWaitForRender, MainThreadID);
	g_pfTB.GetFunctionTotals("DL_END_DRAW", callCount, CDebug::sm_fEndDraw, RenderThreadID);
	g_pfTB.GetFunctionTotals("Wait for Main", callCount, CDebug::sm_fWaitForMain, RenderThreadID);
	g_pfTB.GetFunctionTotals("Wait for next DL", callCount, CDebug::sm_fDrawlistQPop, RenderThreadID);
	g_pfTB.GetFunctionTotals("Wait for GPU", callCount, CDebug::sm_fWaitForGPU, MainThreadID);

	CDebug::sm_fWaitForDrawSpu = CSystem::GetRageSetup()->GetDrawTaskTime();

	PPU_ONLY(CDebug::sm_fFifoStallTimeSpu = CSystem::GetRageSetup()->GetFifoStallTime());
#endif // ENABLE_CDEBUG

	// reset default vp mapping to Screen() as expected by timebars:
	PUSH_DEFAULT_SCREEN();

#if __BANK
	CTextFormat::FontTest();
#endif // __BANK

#if DEBUG_DRAW
#if __DEV
	if (CDebug::GetDisplayTimerBars())
#else
	if(grcDebugDraw::GetDisplayDebugText())
#endif
	{
		PF_DISPLAY_TIMEBARS();
	}
#elif RAGE_TIMEBARS
	static int display_timebars;
	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_B))
		display_timebars = (display_timebars + 1) % 3;
	if(display_timebars)
	{
		g_pfTB.SetDisplayOverbudget(display_timebars == 2);
		PF_DISPLAY_TIMEBARS();
	}
#endif

#if !__FINAL
	if( CControlMgr::GetKeyboard().GetKeyJustDown(KEY_NUMPAD9) )
	{
		rage::grmSetup* pSetup = CSystem::GetRageSetup();
		if (!pSetup->GetShowFrameTime())
		{
			pSetup->SetShowFrameTime(true);
			pSetup->SetShowFrameTicker(false);
		}
		else if (!pSetup->GetShowFrameTicker())
			pSetup->SetShowFrameTicker(true);
		else
			pSetup->SetShowFrameTime(false);
	}
#endif // !__FINAL

	POP_DEFAULT_SCREEN();
	RAGETRACE_UPDATE();
	PF_UPDATE_OVERBUDGET_TIMEBARS();
}

CRenderPhaseTimeBars::CRenderPhaseTimeBars(CViewport *pViewport)
: CRenderPhase(pViewport, "Timebars", DL_RENDERPHASE_TIMEBARS, 0.0f, false)
{
}

void CRenderPhaseTimeBars::BuildDrawList()
{ 
	if (!GetViewport()->IsActive())
		return;

	DrawListPrologue();

#if RSG_PC
	if(PostFX::GetPauseResolve() == PostFX::UNRESOLVE_PAUSE)
		DLC_Add(PostFX::DoPauseResolve, PostFX::UNRESOLVE_PAUSE);
#endif

#if (RSG_PC || RSG_DURANGO || RSG_PS3)
	DLC(dlCmdLockRenderTarget, (0, grcTextureFactory::GetInstance().GetBackBuffer(), NULL));
#endif // (RSG_PC || RSG_DURANGO || RSG_PS3)

	DLC(dlCmdSetCurrentViewport, (&GetGrcViewport()));
	DLC_Add(&CRenderPhaseTimeBars::StaticRenderCall);
	FINAL_DISPLAY_BAR_ONLY(DLC_Add(CDebugBar::Render));
	DEBUG_FPS_ONLY(DLC_Add(CDebug::RenderFpsCounter));
	DLC(dlCmdSetCurrentViewportToNULL, ());

#if ENTITY_GPU_TIME
	static bool bOnce = true;

	if (bOnce)
	{
		bOnce = false;
		dlCmdEntityGPUTimeFlush::SetSetup(CSystem::GetRageSetup());
	}

	DLC(dlCmdEntityGPUTimeFlush, ());
#endif // ENTITY_GPU_TIME

#if (RSG_PC || RSG_DURANGO || RSG_PS3)
		DLC(dlCmdUnLockRenderTarget, (0));
#endif // (RSG_PC || RSG_DURANGO || RSG_PS3)

	DrawListEpilogue();
}




// ***********************
// ****   HUD PHASE   ****
// ***********************


//
// name:		CRenderPhaseHud::CRenderPhaseHud
// description:	Constructor for hud render phase
CRenderPhaseHud::CRenderPhaseHud(CViewport *pViewport)
: CRenderPhase(pViewport, "Hud", DL_RENDERPHASE_HUD, 0.0f, false)
{
}

//----------------------------------------------------
// build and execute the drawlist for this renderphase
void CRenderPhaseHud::BuildDrawList()
{
	if (!GetViewport()->IsActive())
		return;

#if __BANK
	const bool isolatingCutscenePed = (CutSceneManager::GetInstance()->IsPlaying() && CutSceneManager::GetInstance()->IsIsolating());
	if (isolatingCutscenePed)
		return;
#endif

	DrawListPrologue();
	
#if RSG_PC
	DLC_Add(CShaderLib::SetReticuleDistTexture, false);

	// reset scaleform stereo detect 
	if (GRCDEVICE.IsStereoEnabled() && GRCDEVICE.CanUseStereo())
		DLC_Add(CShaderLib::SetStereoParams,Vector4(0.0f,0.0f,0.0f,0.0f));
#endif

	//This section handles when the pause menu is up in single player =========================================
	if( CPauseMenu::GetPauseRenderPhasesStatus() )
	{
		DLCPushTimebar_Budgeted("CPauseMenu::GetPauseRenderPhasesStatus", 2.0f);
#if RSG_PC
		if (GRCDEVICE.UsingMultipleGPUs())
		{
			DLCPushTimebar_Budgeted("GRCDEVICE.UsingMultipleGPUs()", 2.0f);
			if (PostFX::GetPauseResolve() == PostFX::RESOLVE_PAUSE)
			{
				DLC_Add(CRenderTargets::ResolveForMultiGPU, PostFX::RESOLVE_PAUSE);
				DLC_Add(PostFX::DoPauseResolve,PostFX::IN_RESOLVE_PAUSE);
				DLC_Add(PostFX::ResetDOFRenderTargets);
				DLC_Add(CMiniMap_RenderThread::ResetMultiGPUCount);

				//	update current thread pause resolve status
				PostFX::DoPauseResolve(PostFX::IN_RESOLVE_PAUSE);
			}
			else if (PostFX::GetPauseResolve() == PostFX::IN_RESOLVE_PAUSE)
			{
				DLC_Add(CRenderTargets::ResolveForMultiGPU, PostFX::IN_RESOLVE_PAUSE);
			}
			DLCPopTimebar(); // GRCDEVICE.UsingMultipleGPUs()
		}
#endif
		g_markers.DeleteAll();
		DLC(dlCmdSetCurrentViewport, (&(Water::GetWaterRenderPhase()->GetGrcViewport())));//there's no global pointer to the renderphasestd instance so I have to grab this from water...
		DLCPushGPUTimebar_Budgeted(GT_POSTFX,"PostFX",5.0f);
		DLC (dlCmdProcessNonDepthFX, ());
		DLCPopGPUTimebar();

		DLCPopTimebar(); // CPauseMenu::GetPauseRenderPhasesStatus
	}
	//=========================================================================================================
	DLCPushTimebar_Budgeted("PEDHEADSHOTMANAGER", 2.0f);

	if (UI3DDRAWMANAGER.HasPedsToDraw() || PEDHEADSHOTMANAGER.HasPedsToDraw())
	{
		DLCPushTimebar_Budgeted("CPedDamageManager", 2.0f);
#if RSG_DURANGO
		DLC_Add(GBuffer::ClearFirstTarget);	// remove when ped draw has AA
#endif
		CPedDamageManager::AddToDrawList(false,false); // regenerate the ped damage buffer if we need them for headshots of UI peds
		DLCPopTimebar(); // CPedDamageManager
	}
	else if (CPauseMenu::GetPauseRenderPhasesStatus()) // if we're paused this way, the main peddamage drawlist will not execute, so the compression will not advance.
	{
		DLCPushTimebar_Budgeted("PEDDECORATIONBUILDER", 2.0f);
		PEDDECORATIONBUILDER.AddToDrawList();
		DLCPopTimebar(); // PEDDECORATIONBUILDER
	}

#if RSG_DURANGO
	DLC(dlCmdLockRenderTarget, (0, grcTextureFactory::GetInstance().GetBackBuffer(), grcTextureFactory::GetInstance().GetBackBufferDepth()));
#endif // RSG_DURANGO

	DLCPushTimebar_Budgeted("pedheadshotmanager", 2.0f);
	PEDHEADSHOTMANAGER.AddToDrawList();
	DLCPopTimebar(); // pedheadshotmanager
	DLCPopTimebar(); // PEDHEADSHOTMANAGER

	DLCPushTimebar_Budgeted("UI RenderTargets", 2.0f);
	DLC(dlCmdSetCurrentViewport, (&GetGrcViewport()));
	DLC_Add(CRenderTargets::LockUIRenderTargets);

	AddItemsToDrawList();
	
	DLC_Add(CRenderTargets::UnlockUIRenderTargets, false);
	DLCPopTimebar(); // UI RenderTargets"

#if RSG_DURANGO
	DLC(dlCmdUnLockRenderTarget, (0));
#endif // RSG_DURANGO

	DLC(dlCmdSetCurrentViewportToNULL, ());

	// disable zbuffer test/write
	DLC(dlCmdSetDepthStencilState, (grcStateBlock::DSS_IgnoreDepth));
	DLC(dlCmdGrcLightStateSetEnabled, (false));

	DLCPushTimebar_Budgeted("MeshBlendManager", 2.0f);
	DLC_Add(MeshBlendManager::RenderThreadUpdate);
	DLCPopTimebar(); // MeshBlendManager

#if __BANK
	DLC_Add(CRenderTargets::RenderBank, 0);
	DLC_Add(CVehicleDeformation::RenderBank);
#endif

	DrawListEpilogue();
}

void CRenderPhaseHud::AddItemsToDrawList()
{
#if __D3D11
#if RSG_PC
	DLC(dlCmdLockRenderTarget, (0, grcTextureFactory::GetInstance().GetBackBuffer(), CRenderTargets::GetUIDepthBuffer(true)));
#else
	DLC(dlCmdLockRenderTarget, (0, grcTextureFactory::GetInstance().GetBackBuffer(), CRenderTargets::GetDepthResolved()));
#endif
#endif

	DLC(dlCmdGrcLightStateSetEnabled, (false));

	//
	// Display Network HUD - this should appear below everything else
	//
	NetworkInterface::RenderNetworkInfo(); // display OHD debug text over network objects

#if __BANK
	CPhysical::RenderDebugObjectIDNames();
#endif

	DLC_Add(CScriptHud::ResetScaleformComponentsOnRTPerFrame);

	//
	// Items BEFORE the screen fade:
	//
	DLC(dlCmdSetCurrentViewport, (&GetGrcViewport()));
	CScriptHud::DrawScriptGraphics(CRenderTargetMgr::RTI_MainScreen, SCRIPT_GFX_ORDER_BEFORE_HUD);

	bool bIsCutscenePlayingBack = CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsCutscenePlayingBack();
	DLC_Add(CutSceneManager::RenderOverlayToMainRenderTarget, bIsCutscenePlayingBack);

	const bool bIsTakingPhoto = CPhoneMgr::CamGetState();
	if (!bIsTakingPhoto && !CPauseMenu::IsActive())
	{
		// only add command if movie is active
		if(SMultiplayerGamerTagHud::IsInstantiated() && SMultiplayerGamerTagHud::GetInstance().IsMovieActive())
		{
			DLCPushTimebar_Budgeted("CMultiplayerGamerTagHud::Render", 2.0f);
			DLC_Add(CMultiplayerGamerTagHud::Render);
			DLCPopTimebar();
		}
	}

	DLCPushTimebar_Budgeted("CMiniMap::Update()", 2.0f);
	CMiniMap::Update();  // update here as it also adds DLC commands inside the update
	DLCPopTimebar();

	//Update Markers after Minimap so we have up-to-date blip state
	if (auto MM = CHudMarkerManager::Get())
	{
		MM->Update();
		MM->AddDrawListCommands();
	}

	CGameStreamMgr::Update();

#if RSG_PC
	CMousePointer::Update();
#endif  // #if RSG_PC

	DLC_Add(CStuntJumpManager::Render);

#if GTA_REPLAY
	DLC_Add(CUIReplayScaleformController::RenderReplayStatic);
#endif // #if GTA_REPLAY

	const bool bRenderHudOverPauseMenu = ( ( (CPauseMenu::IsActive()) || (CScriptHud::bRenderFrontendBackgroundThisFrame) ) && (CScriptHud::ms_bDisplayHudWhenPausedThisFrame.GetWriteBuf()) );
	const bool bRenderHudOverFade = CScriptHud::bRenderHudOverFadeThisFrame;

#if GTA_REPLAY
	// B*2358927 - [Replay] Sniper reticule and zoom indicator still shows above a 100% black filter
	bool replayDontRenderHUD = false;
	if(CReplayMgr::IsReplayInControlOfWorld())
	{
		u32	modHash = g_timeCycle.GetReplayModifierHash();
		if (modHash > 0)
		{
			s32 replayModId =  g_timeCycle.FindModifierIndex(modHash,"ReplayModifierHash");
			tcModifier* pModifier = g_timeCycle.GetModifier(replayModId);
			if( pModifier )
			{
				if( pModifier->GetName() == ATSTRINGHASH("NG_blackout",0x8366c52c) && g_timeCycle.GetReplayEffectIntensity() == 1.0f)
				{
					replayDontRenderHUD = true;
				}
			}
		}
	}
#endif	//GTA_REPLAY

	// Render the hud before minimap and pausemenu if we can render it and script are not forcing it on
	if ( (CNewHud::CanRenderHud()) && (!bRenderHudOverPauseMenu) && (!bRenderHudOverFade) REPLAY_ONLY( && (replayDontRenderHUD == false))
#if RSG_PC
		&& !(CVideoEditorUi::IsActive() && ioKeyboard::ImeIsInProgress() )
#endif // RSG_PC
		)
	{
		DLCPushTimebar_Budgeted("CNewHud", 2.0f);
		DLC_Add(CNewHud::Render);
		DLCPopTimebar();
	}

#if SUPPORT_MULTI_MONITOR
	if(CNewHud::ShouldDrawBlinders())
		DLC_Add(CSprite2d::DrawMultiFade, false);
#endif // SUPPORT_MULTI_MONITOR

#if GTA_REPLAY
	DLCPushTimebar_Budgeted("RenderOverlaysAndEffects", 2.0f);
	DLC_Add(CReplayCoordinator::RenderOverlaysAndEffects);
	DLCPopTimebar();
#endif //GTA_REPLAY

	if ((!CPauseMenu::IsActive() 
#if RSG_PC
	|| CMiniMap_RenderThread::IsRenderMultiGPU()
#endif
	)
#if GTA_REPLAY
		&& !CVideoEditorInterface::IsActive()
		&& !CReplayCoordinator::IsActive()
#endif //GTA_REPLAY
		)
	{
		DLCPushTimebar_Budgeted("CMiniMap_RenderThread", 2.0f);
		DLC_Add(CMiniMap_RenderThread::Render);
		DLCPopTimebar();
	}

#if GTA_REPLAY
	if ( !CReplayCoordinator::IsActive() )
#endif //GTA_REPLAY
	{
		DLCPushTimebar_Budgeted("CScriptHud::DrawScriptGraphics", 2.0f);

		DLC(dlCmdSetCurrentViewport, (&GetGrcViewport()));
		CScriptHud::DrawScriptGraphics(CRenderTargetMgr::RTI_MainScreen, SCRIPT_GFX_ORDER_AFTER_HUD);

		DLC_Add(PostFX::DrawBulletImpactOverlaysAfterHud);
		DLCPopTimebar();
	}

	DLC_Add(CCredits::Render, true);

	if (CVfxHelper::ShouldRenderInGameUI())
	{
		CGolfTrailInterface::UpdateAndRender(false);
	}

	//
	// screen fade:
	//
	DLC(dlCmdSetCurrentViewport, (&GetGrcViewport()));
	camInterface::RenderFade();

	//
	// Items AFTER the screen fade:
	//

	// don't assume we've got a viewport set up at this point
	DLC(dlCmdSetCurrentViewport, (&GetGrcViewport()));

#if GTA_REPLAY
	if (!CVideoEditorUi::IsActive())
#endif // #if GTA_REPLAY
	{
		if( !CScriptHud::bSuppressPauseMenuRenderThisFrame)
		{
			DLCPushTimebar_Budgeted("CPauseMenu::RenderBackgroundContent", 2.0f);

			PauseMenuRenderDataExtra newData;
			CPauseMenu::SetRenderData(newData);

			DLC_Add(CPauseMenu::RenderBackgroundContent, newData);
	
			if(!newData.bHideMenu)
				UI3DDRAWMANAGER.AddToDrawList();

			DLC(dlCmdSetCurrentViewport, (&GetGrcViewport()));
			DLC_Add(CPauseMenu::RenderForegroundContent, newData);

			DLC_Add(SocialClubMenu::RenderWrapper);
			DLCPopTimebar();
		}
	}

#if GTA_REPLAY
	DLC_Add(CReplayCoordinator::RenderPostFadeOverlaysAndEffects);
	DLC_Add(CTextTemplate::PreReplayEditorRender);
	DLC_Add(WatermarkRenderer::Render);
#endif //GTA_REPLAY

	// if script are forcing the HUD on then display it after the pausemenu is rendered so it appears ontop (fixes 1429798)
	if (bRenderHudOverPauseMenu
#if RSG_PC
		&& !CVideoEditorUi::IsActive()
#endif // RSG_PC
		)
	{
		DLCPushTimebar_Budgeted("CNewHud", 2.0f);
		DLC_Add(CNewHud::Render);
		DLCPopTimebar();
	}


#if USE_SCREEN_WATERMARK
	DLC_Add(PostFX::RenderScreenWatermark);
	DLC_Add(CRenderTargets::LockUIRenderTargets);
#endif 
	
	//
	// Store screens:
	//
	DLC_Add(cStoreScreenMgr::Render);

	//DLC_Add(CBusySpinner::Render);

	DLC(dlCmdSetCurrentViewport, (&GetGrcViewport()));
	CScriptHud::DrawScriptGraphics(CRenderTargetMgr::RTI_MainScreen, SCRIPT_GFX_ORDER_AFTER_FADE);

	if (bRenderHudOverFade
#if RSG_PC
		&& !CVideoEditorUi::IsActive()
#endif // RSG_PC
		)
	{
		DLCPushTimebar_Budgeted("CNewHud", 2.0f);
		DLC_Add(CNewHud::Render);
		DLCPopTimebar();
	}

#if __BANK
	DLC_Add(CScaleformMgr::RenderDebugMovie);  // render any debug scaleform movie here
	DLC_Add(CNetworkTransitionNewsController::Bank_RenderDebugMovie);
#endif // __BANK

	DLC_Add(CCredits::Render, false);  // render credits BEFORE the fade so they fade out when skipped

#if __BANK
	DLC_Add(camManager::Render);
#endif
#if GTA_REPLAY
	DLC_Add(CReplayMgrInternal::Display);		// Render debug replay elements
#endif // GTA_REPLAY

#if defined(VIDEO_RECORDING_ENABLED) && VIDEO_RECORDING_ENABLED && (USES_MEDIA_ENCODER)
	DLCPushTimebar_Budgeted("CaptureVideoFrame", 2.0f);
	DLC_Add(VideoRecording::CaptureVideoFrame, CReplayMgr::GetExportFrameStep());
	DLCPopTimebar();
#endif

	//
	// Custom Warning Screen
	//
	DLC_Add(CReportMenu::Render);

	#if RSG_PC
	if(CPauseMenu::IsActive())
	{
		if(SUIContexts::IsActive(ATSTRINGHASH("IN_CORONA_MENU", 0x5849F1FA)) && NetworkInterface::IsSessionActive())
		{
			DLCPushTimebar_Budgeted("CMultiplayerChat::Render", 2.0f);
			DLC_Add(CMultiplayerChat::Render);
			DLCPopTimebar();
		}
	}
	else
	{
		if(NetworkInterface::IsSessionActive())
		{
			DLCPushTimebar_Budgeted("CMultiplayerChat", 2.0f);
			DLC_Add(CMultiplayerChat::Render);
			DLCPopTimebar();
		}
	}
	#endif

#if GTA_REPLAY
	DLCPushTimebar_Budgeted("CVideoEditorUi", 2.0f);
	DLC_Add(CVideoEditorUi::Render);
	if (CVideoEditorUi::IsActive())
	{
		DLC_Add(SocialClubMenu::RenderWrapper);
		DLC_Add(PolicyMenu::RenderWrapper);
		DLC_Add(CTextTemplate::PostReplayEditorRender);
	}
	DLCPopTimebar();
#endif // #if GTA_REPLAY

#if RSG_PC
	if(ioKeyboard::ImeIsInProgress() && CVideoEditorUi::IsActive())
		DLC_Add(CNewHud::Render);
#endif // RSG_PC

	if (!CPauseMenu::IsActive()
#if GTA_REPLAY
		&& !CVideoEditorInterface::IsActive()
		&& !CReplayCoordinator::IsActive()
#endif //GTA_REPLAY
		)
	{
		DLC_Add(CGameStreamMgr::Render);
	}
	else
	{
		if( CGameStreamMgr::GetGameStream()->IsForceRenderOn() )
		{
			DLC_Add(CGameStreamMgr::Render);
		}
	}


#if RSG_PC
	DLC_Add(CTextInputBox::Render);
#endif // RSG_PC

	//
	// warning messages
	//

	if (!CDisplayCalibration::IsActive())  // dont add the warning message if its going to be added for the display calib screen
	{
		if (CWarningScreen::CanRender())
		{
			DLC_Add(CWarningScreen::Render);
		}
	}

	if (CSavegameDialogScreens::ShouldRenderBlackScreen())
	{
		DLC_Add(CSavegameDialogScreens::RenderBlackScreen);
	}

#if RSG_PC
	DLC_Add(CMousePointer::Render);  // 2161312 - Render above fades all the time
#endif  // #if RSG_PC

	// SafeZone rendering
	if(CNewHud::ShouldDrawSafeZone())
		DLC_Add(CNewHud::DrawSafeZone);

#if __D3D11
	DLC(dlCmdUnLockRenderTarget, (0));
#endif

#if  USE_SCREEN_WATERMARK
	DLC_Add(CRenderTargets::UnlockUIRenderTargets, false);
#endif //  USE_SCREEN_WATERMARK
}



// *********************************
// **** 2D SCRIPT OBJECTS PHASE ****
// *********************************

//-------------------------------------------------------------------
//
void CRenderPhaseScript2d::RenderScriptedRTCallBack(unsigned count, const NamedRenderTargetBufferedData *rts)
{
	PF_AUTO_PUSH_TIMEBAR_BUDGETED("RenderScriptedRTCallBack", 2.0f);
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	grcViewport *viewport = grcViewport::GetCurrent();

	grcResolveFlags flags;
	//flags.MipMap = !__XENON;

	for(unsigned i=0;i<count;i++)
	{
		grcRenderTarget *rt = rts[i].rt;
		if( rt )
		{
			if(rts[i].tex)
			{
				// Update render target incase defrag has moved the texture we are
				// want to render to.
				rt->UpdateMemoryLocation(rts[i].tex->GetTexturePtr());

				u32 targetId = rts[i].id;

				grcTextureFactory::GetInstance().LockRenderTarget(0, rt, NULL);

				// we have to push the viewport, 
				// as LockRenderTarget tends to fuck it up for us.
				grcViewport::SetCurrent(viewport);

				GRCDEVICE.Clear(	true,	Color32(0x00000000), 
					false,	0.0f, 
					false,	0xffffffff);

				CScriptHud::DrawScriptSpritesAndRectangles(targetId, SCRIPT_GFX_ORDER_AFTER_HUD);
				CScriptHud::DrawScriptText(targetId, SCRIPT_GFX_ORDER_AFTER_HUD);
				CutSceneManager::RenderOverlayToRenderTarget(targetId);
                CVehicleModelInfo::RenderDialsToRenderTarget(targetId);

				grcTextureFactory::GetInstance().UnlockRenderTarget(0,&flags);
			}
		}
	}
}

//-------------------------------------------------------------------
//
bool CRenderPhaseScript2d::RenderScriptedRT()
{
//	if (CPauseMenu::IsActive())  // no script text should be rendered while frontend is active (but movies should be!)
//		return;

#if __BANK
	if (!CHudTools::bDisplayHud)  // dont render any script text if this debug flag is set
		return false;
#endif

	const unsigned count = gRenderTargetMgr.GetNamedRendertargetsCount();
	if( count )
	{
		NamedRenderTargetBufferedData *rts = (NamedRenderTargetBufferedData*)(gDCBuffer->AddDataBlock(NULL, count*sizeof(*rts)));
		gRenderTargetMgr.StoreNamedRendertargets(rts);
		DLC_Add(RenderScriptedRTCallBack, count, rts);

		return true;
	}
	else
		return false;
}

//-------------------------------------------------------------------
//
void CRenderPhaseScript2d::MovieMeshBlitToRenderTarget()
{
#if __BANK
	if (!CHudTools::bDisplayHud)  // dont render any script text if this debug flag is set
		return;
#endif

	CMovieMeshRenderTargetManager& movieMeshRTMgr = g_movieMeshMgr.GetRTManager();
	const unsigned count = movieMeshRTMgr.GetCount();
	if (count)
	{
		MovieMeshRenderTargetBufferedData* pRts = (MovieMeshRenderTargetBufferedData*)(gDCBuffer->AddDataBlock(NULL, count*sizeof(*pRts)));
		movieMeshRTMgr.BufferDataForDrawList(pRts);
		DLC_Add(MovieMeshBlitToRenderTargetCallback, count, pRts);
	}
}

//-------------------------------------------------------------------
//
void CRenderPhaseScript2d::MovieMeshBlitToRenderTargetCallback(unsigned count, const MovieMeshRenderTargetBufferedData *pRts)
{
	PF_AUTO_PUSH_TIMEBAR_BUDGETED("MovieMeshBlitToRenderTargetCallback", 2.0f);
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	grcViewport* viewport = grcViewport::GetCurrent();

	grcResolveFlags flags;

	for(unsigned i=0;i<count;i++)
	{
		grcRenderTarget* pRt = pRts[i].pRt;
		if( pRt )
		{
			if(pRts[i].tex)
			{
				// Update render target incase defrag has moved the texture we are
				// want to render to.
				pRt->UpdateMemoryLocation(pRts[i].tex->GetTexturePtr());

				u32 targetId = pRts[i].id;

				grcTextureFactory::GetInstance().LockRenderTarget(0, pRt, NULL);

				// we have to push the viewport, as LockRenderTarget tends to fuck it up for us.
				grcViewport::SetCurrent(viewport);

				GRCDEVICE.Clear(true, Color32(0x00000000), false, 0.0f, false, 0xffffffff);

				CScriptHud::DrawScriptSpritesAndRectangles(targetId, SCRIPT_GFX_ORDER_AFTER_HUD);
				CScriptHud::DrawScriptText(targetId, SCRIPT_GFX_ORDER_AFTER_HUD);

				grcTextureFactory::GetInstance().UnlockRenderTarget(0,&flags);
			}
		}
	}
}

//
// name:		CRenderPhaseScript2d::CRenderPhaseScript2d
// description:	Constructor for script 2d render phase
CRenderPhaseScript2d::CRenderPhaseScript2d(CViewport *pViewport)
: CRenderPhase(pViewport, "Script offscreen", DL_RENDERPHASE_SCRIPT, 0.0f, false)
{
	GetGrcViewport().SetNormWindow(0.0f,0.0f,1.0f,1.0f);
	GetGrcViewport().Ortho(0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f);

	Disable();
}

//-------------------------------------------------------------------
//
void CRenderPhaseScript2d::BuildDrawList()
{
	if (GetViewport() && !GetViewport()->IsActive())
	{
		return;
	}

	DrawListPrologue();

#if ENABLE_LEGACY_SCRIPTED_RT_SUPPORT	
	// Now we need to possibly draw the background.... which can come from another viewport.
	DLC(dlCmdLockRenderTarget, (0, CScriptHud::GetRenderTarget(), NULL));
	DLC(dlCmdSetCurrentViewport, (&GetGrcViewport()));

#if !__FINAL
	DLC(dlCmdClearRenderTarget, (true,Color32(0xff0000ff),false,1.0f,false,0));
#endif // !__FINAL

	DLC(dlCmdSetCurrentViewport, (&GetGrcViewport()));
	CScriptHud::DrawScriptGraphics(CRenderTargetMgr::RTI_Scripted, SCRIPT_GFX_ORDER_AFTER_HUD);

	DLC_Add(CutSceneManager::RenderOverlayToInGameTarget); 

	DLC(dlCmdUnLockRenderTarget, (0));
#else // ENABLE_LEGACY_SCRIPTED_RT_SUPPORT
	DLC(dlCmdSetCurrentViewport, (&GetGrcViewport()));
#endif // ENABLE_LEGACY_SCRIPTED_RT_SUPPORT

	bool bRendered = RenderScriptedRT();

	MovieMeshBlitToRenderTarget();

#if RSG_PC
	if (!bRendered)
		Disable();
#else
	(void)bRendered;
	Disable();
#endif
	DLC(dlCmdSetCurrentViewportToNULL, ());

	DrawListEpilogue();

}

// *********************************
// ****       Phone Screen      ****
// *********************************

//
// name:		CRenderPhaseScript2d::CRenderPhaseScript2d
// description:	Constructor for script 2d render phase
CRenderPhasePhoneScreen::CRenderPhasePhoneScreen(CViewport *pViewport)
: CRenderPhase(pViewport, "Phone screen", DL_RENDERPHASE_SCRIPT, 0.0f, true)
#if SUPPORT_MULTI_MONITOR
, m_uLastDeviceSyncTimestamp(0)
#endif
{
	GetGrcViewport().SetNormWindow(0.0f,0.0f,1.0f,1.0f);
	GetGrcViewport().Ortho(0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f);

	grcBlendStateDesc blendDesc;
	blendDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALPHA;
	m_alphaClearBS = grcStateBlock::CreateBlendState(blendDesc);

	Disable();

#if GTA_REPLAY
	m_bInReplay = false;
#endif
}



//-------------------------------------------------------------------
//
void CRenderPhasePhoneScreen::BuildDrawList()
{
	if (!GetViewport()->IsActive())
	{
		return;
	}

	DrawListPrologue();

	DLC(dlCmdLockRenderTarget, (0, CPhoneMgr::GetRenderTarget(), NULL));
	DLC(dlCmdSetCurrentViewport, (&GetGrcViewport()));

// 	if (CLowQualityScreenshot::IsRendering() && CLowQualityScreenshot::GetRenderToPhone())
// 	{
// 		DLC_Add(CLowQualityScreenshot::Render, true);
// 	}
// 	else

	if (!CPhotoManager::RenderCameraPhoto())
	{
#if __ASSERT && RSG_PC
		grcRenderTarget* rt = CPhoneMgr::GetRenderTarget();
		Assert(rt && rt->GetName() && !stricmp("PHONE_SCREEN",rt->GetName()));
#endif

		DLC(dlCmdClearRenderTarget, (true, Color32(0,0,0), false, 0.0f, false, 0));
		DLC(dlCmdGrcLightStateSetEnabled, (false));

#if GTA_REPLAY
		if (m_bInReplay)
		{
			DLC_Add(CUIReplayScaleformController::RenderReplayPhoneStatic);
		}
		else
#endif		
		{
			CScriptHud::DrawScriptGraphics(CRenderTargetMgr::RTI_PhoneScreen, SCRIPT_GFX_ORDER_AFTER_HUD);
		}
	}

	// Clear Alpha once we're done, so we can use the target as a texture on an emissive shader.
	DLC(dlCmdSetBlendState, (m_alphaClearBS));
#if __PS3
	DLC(dlCmdClearRenderTarget, (true, Color32(0xFFFFFFFF), false, 0.0f, false, 0));
#else // __PS3
	DLC ( CDrawRectDC, ( fwRect(0.0f, 0.0f, 1.0f, 1.0f), Color32(0xffffffff)) );
#endif // __PS3	
	// Reset to default, if not, with all the crazy shit going to render the phone, stuff goes wrong.
	DLC(dlCmdSetBlendState, (grcStateBlock::BS_Default));

#if RSG_PC
	int gpuCount = GetMultiGPUDrawListCounter();

	if (gpuCount == 0)
		Disable();
	else
		SetMultiGPUDrawListCounter(gpuCount - 1);
#else
	Disable();
#endif

#if GTA_REPLAY
	m_bInReplay = false;
#endif

	DLC(dlCmdUnLockRenderTarget, (0));
	DLC(dlCmdSetCurrentViewportToNULL, ());

	DrawListEpilogue();
}

void CRenderPhasePhoneScreen::UpdateViewport()
{
	// This is no longer needed, it seems, since the phone contents are rendered in its own render target
#if SUPPORT_MULTI_MONITOR && 0
	const MonitorConfiguration &config = GRCDEVICE.GetMonitorConfig();
	if (m_uLastDeviceSyncTimestamp != config.m_ChangeTimestamp)
	{
		m_uLastDeviceSyncTimestamp = config.m_ChangeTimestamp;
		const fwRect &area = config.getCentralMonitor().getArea();
		GetGrcViewport().SetNormWindow( area.left, area.bottom, area.GetWidth(), area.GetHeight() );
	}
#endif	//SUPPORT_MULTI_MONITOR
}



// **************************
// **** DRAW SCENE PHASE ****
// **************************

#if RENDERLIST_DUMP_PASS_INFO
static void RenderListTest_DumpPassInfo_button() { g_RenderListDumpPassInfo++; }
#endif // RENDERLIST_DUMP_PASS_INFO

#if __BANK

static bool g_bUseDisabledDepthCompression = true;
static bool g_bDisableLateInteriorAlpha = false;
void CRenderPhaseDrawScene::AddWidgets(bkGroup& group)
{
	CRenderPhaseScanned::AddWidgets(group);
#if RENDERLIST_DUMP_PASS_INFO
	group.AddButton("Render List Test - Dump Pass Info", RenderListTest_DumpPassInfo_button);
#endif // RENDERLIST_DUMP_PASS_INFO

	PS3_ONLY( group.AddToggle("Wait on bubble fence", &g_bForwardRenderWaitsOnBubbleFence ) );

#if RSG_ORBIS
	group.AddToggle("Use Disabled Depth Compression", &g_bUseDisabledDepthCompression);
#endif
	group.AddToggle("Disable Late Interior Alpha", &g_bDisableLateInteriorAlpha);
}
#endif // __BANK

//
// name:		CRenderPhaseDrawScene::CRenderPhaseDrawScene
// description:	Constructor for standard draw scene render phase

CRenderPhaseDrawScene::CRenderPhaseDrawScene(CViewport *pViewport, CRenderPhase* pGBufferPhase)
	: CRenderPhaseScanned(pViewport, "DrawScene", DL_RENDERPHASE_DRAWSCENE, 0.0f
#if DRAW_LIST_MGR_SPLIT_DRAWSCENE
	, DL_RENDERPHASE_DRAWSCENE_FIRST, DL_RENDERPHASE_DRAWSCENE_LAST
#endif // !DRAW_LIST_MGR_SPLIT_DRAWSCENE
), m_pGBufferPhase(pGBufferPhase)
{
	g_DrawSceneRenderPhase = this;
	//this is not actually used to build a render list - only used to query if certain features are enabled
	SetRenderFlags(
		RENDER_SETTING_RENDER_ALPHA_POLYS |
		RENDER_SETTING_RENDER_DECAL_POLYS |
		RENDER_SETTING_RENDER_CUTOUT_POLYS |
		RENDER_SETTING_RENDER_WATER |
		RENDER_SETTING_RENDER_SHADOWS |
		RENDER_SETTING_RENDER_PARTICLES |
		RENDER_SETTING_RENDER_DISTANT_LIGHTS |
		RENDER_SETTING_RENDER_DISTANT_LOD_LIGHTS |
		RENDER_SETTING_RENDER_UI_EFFECTS |
		RENDER_SETTING_ALLOW_PRERENDER);

	m_pWaterSurfacePhase = NULL; // to change eventually (phases should not be reliant on other types of phases )- DW

	m_pPortalVisTracker = rage_new CPortalVisTracker();
	Assert(m_pPortalVisTracker);
	m_pPortalVisTracker->SetGrcViewport( &GetGrcViewport() );
	m_pPortalVisTracker->SetLoadsCollisions(true);
	CPortalTracker::AddToActivatingTrackerList(m_pPortalVisTracker);
	m_isUpdateTrackerPhase = true;

	m_pGBufferPhase = pGBufferPhase;

	m_renderAlphaState = RENDER_ALPHA_ALL;
	m_lateVehicleInteriorAlphaState = false;
	m_cameraInteriorLocation.MakeInvalid();
	ms_cameraInteriorLocationRT.MakeInvalid();
}

CRenderPhaseDrawScene::~CRenderPhaseDrawScene()
{
	//	This is called when the script de-activates a viewport
	if (m_pPortalVisTracker){
		CPortalTracker::RemoveFromActivatingTrackerList(m_pPortalVisTracker);
		delete m_pPortalVisTracker;
		m_pPortalVisTracker = NULL;
	}
}

void CRenderPhaseDrawScene::UpdateViewport()
{
	g_DrawSceneRenderPhaseViewport = NULL;
	SetActive(true);

	CRenderPhaseScanned::UpdateViewport();
	grcViewport& viewport = GetGrcViewport();
	PF_FUNC(CRenderPhaseDrawSceneRL);

	CViewport *pVp = GetViewport();

	const Vector3& position = pVp->GetFrame().GetPosition();

	if (m_pPortalVisTracker)
	{
		if (pVp->GetAttachEntity()){
			m_pPortalVisTracker->UpdateUsingTarget(pVp->GetAttachEntity(), position);
		} else {
			m_pPortalVisTracker->Update(position);
		}
	}

	g_DrawSceneRenderPhaseViewport = &viewport;
}

void CRenderPhaseDrawScene::BuildRenderList()
{

}

static void CopyShadowToMiniMap()
{
	CRenderPhaseCascadeShadowsInterface::CopyShadowsToMiniMap(Water::GetNoiseTexture(), Water::GetWaterLevel());
}

static void ProcessLateStoredLightsWrapper()
{
	g_ptFxManager.ProcessLateStoredLights();
}

#if __PS3
// Allows us to emulate the 360 (which doesn't necessarily resolve depth after alpha/forward rendering)
static void CopyPreAlphaDepth()
{
	// Right now we only need this if we're rendering volume lights
	if ( Lights::AnyVolumeLightsPresent() )
	{
		PF_PUSH_TIMEBAR_DETAIL("Copy Pre-Alpha Depth");
		CRenderTargets::UnlockSceneRenderTargets();
		CRenderTargets::CopyDepthBuffer(CRenderTargets::GetDepthBufferAsColor(), CRenderTargets::GetOffscreenDepthBuffer(), 0x0);
		CRenderTargets::LockSceneRenderTargets();
		PF_POP_TIMEBAR_DETAIL();
	}
}
#endif


#if RSG_ORBIS
static void ReenableDepthCompression()
{
#if __BANK
	if (g_bUseDisabledDepthCompression)
#endif
	{
		GRCDEVICE.ReenableDepthSurfaceCompression();
	}
}
#endif

// ************************************
// **** DRAW SCENE PHASE INTERFACE ****
// ************************************

bool CRenderPhaseDrawSceneInterface::RenderAlphaEntityInteriorLocationCheck_Common(bool bIsInSameInteriorAsCam)
{
	bool bIsRender = CSystem::IsThisThreadId(SYS_THREAD_RENDER);
	u32 renderAlphaState = (bIsRender?ms_renderAlphaStateRT:CRenderPhaseDrawSceneInterface::GetRenderPhase()->GetRenderAlphaState());
	u32 normalAlphaState = renderAlphaState & (RENDER_ALPHA_EXCLUDE_CURRENT_INTERIOR | RENDER_ALPHA_CURRENT_INTERIOR_ONLY);
	return (normalAlphaState == (RENDER_ALPHA_EXCLUDE_CURRENT_INTERIOR | RENDER_ALPHA_CURRENT_INTERIOR_ONLY) || (normalAlphaState == RENDER_ALPHA_CURRENT_INTERIOR_ONLY) == bIsInSameInteriorAsCam );
}

bool CRenderPhaseDrawSceneInterface::RenderAlphaEntityInteriorLocationCheck(fwInteriorLocation interiorLocation)
{
	bool bIsRender = CSystem::IsThisThreadId(SYS_THREAD_RENDER);
	fwInteriorLocation camInteriorLocation = (bIsRender?ms_cameraInteriorLocationRT:CRenderPhaseDrawSceneInterface::GetRenderPhase()->GetCameraInteriorLocation());
	return RenderAlphaEntityInteriorLocationCheck_Common((interiorLocation.GetInteriorProxyIndex() == camInteriorLocation.GetInteriorProxyIndex()));
}

bool CRenderPhaseDrawSceneInterface::RenderAlphaEntityInteriorHashCheck(u64 interiorHash)
{
	bool bIsRender = CSystem::IsThisThreadId(SYS_THREAD_RENDER);
	fwInteriorLocation camInteriorLocation = (bIsRender?ms_cameraInteriorLocationRT:CRenderPhaseDrawSceneInterface::GetRenderPhase()->GetCameraInteriorLocation());
	u64 camInteriorHash = CVfxHelper::GetHashFromInteriorLocation(camInteriorLocation);
	return RenderAlphaEntityInteriorLocationCheck_Common(interiorHash == camInteriorHash);
}

bool CRenderPhaseDrawSceneInterface::RenderAlphaInteriorCheck(fwInteriorLocation interiorLocation, bool bIsVehicleAlphaInterior)
{
	bool bIsRender = CSystem::IsThisThreadId(SYS_THREAD_RENDER);
	u32 renderAlphaState = (bIsRender?ms_renderAlphaStateRT:CRenderPhaseDrawSceneInterface::GetRenderPhase()->GetRenderAlphaState());
	u32 vehicleAlphaState = renderAlphaState & (RENDER_VEHICLE_ALPHA_EXCLUDE_INTERIOR | RENDER_VEHICLE_ALPHA_INTERIOR_ONLY);
	u32 normalAlphaState = renderAlphaState & (RENDER_ALPHA_EXCLUDE_CURRENT_INTERIOR | RENDER_ALPHA_CURRENT_INTERIOR_ONLY);


	if(vehicleAlphaState == RENDER_VEHICLE_ALPHA_EXCLUDE_INTERIOR || vehicleAlphaState == RENDER_VEHICLE_ALPHA_INTERIOR_ONLY)
	{
		if(bIsVehicleAlphaInterior)
		{
			return ((vehicleAlphaState & RENDER_VEHICLE_ALPHA_INTERIOR_ONLY) != 0);
		}
		else if(vehicleAlphaState == RENDER_VEHICLE_ALPHA_INTERIOR_ONLY)
		{
			//If the mode is rendering vehicle interiors, we should skip rest of the alpha entities
			return false;
		}
	}
	//if we reach here then entity is not part of vehicle interior or that mode is not enabled
	if(normalAlphaState == RENDER_ALPHA_CURRENT_INTERIOR_ONLY || normalAlphaState == RENDER_ALPHA_EXCLUDE_CURRENT_INTERIOR)
	{
		return RenderAlphaEntityInteriorLocationCheck(interiorLocation);
	}

	return true;
}

bool CRenderPhaseDrawSceneInterface::AlphaEntityInteriorCheck(fwRenderPassId UNUSED_PARAM(id), fwRenderBucket UNUSED_PARAM(bucket), fwRenderMode UNUSED_PARAM(renderMode), int ASSERT_ONLY(subphasePass), float UNUSED_PARAM(sortVal), fwEntity *entity)
{
	Assertf(subphasePass == SUBPHASE_ALPHA_INTERIOR, "Function should only be called when subPhasePass = %d, now subPhase = %d", SUBPHASE_ALPHA_INTERIOR, subphasePass);
	CEntity* pEntity = static_cast<CEntity*>(entity);
	u32 renderAlphaState = CRenderPhaseDrawSceneInterface::GetRenderPhase()->GetRenderAlphaState();
	u32 vehicleAlphaState = renderAlphaState & (RENDER_VEHICLE_ALPHA_EXCLUDE_INTERIOR | RENDER_VEHICLE_ALPHA_INTERIOR_ONLY);
	u32 normalAlphaState = renderAlphaState & (RENDER_ALPHA_EXCLUDE_CURRENT_INTERIOR | RENDER_ALPHA_CURRENT_INTERIOR_ONLY);

	//Special case for vehicle interior alpha
	bool isVehicleInteriorAlpha = false;
	fwInteriorLocation entityInteriorLocation;
	if(vehicleAlphaState == RENDER_VEHICLE_ALPHA_EXCLUDE_INTERIOR || vehicleAlphaState == RENDER_VEHICLE_ALPHA_INTERIOR_ONLY)
	{		
		if(CVfxHelper::IsEntityInCurrentVehicle(pEntity))
		{
			isVehicleInteriorAlpha = true;
		}
		else if(pEntity->GetIsTypePrtSys())
		{
			CPtFxSortedEntity* pSortedPtfxEntity = static_cast<CPtFxSortedEntity*>(pEntity);
			ptxEffectInst* pEffectInst = pSortedPtfxEntity->GetFxInst();
			if(pEffectInst)
			{
				isVehicleInteriorAlpha = pEffectInst->GetDataDB()->GetCustomFlag(PTFX_IS_VEHICLE_INTERIOR);
			}
		}
	}

	//if we reach here then entity is not part of vehicle interior or that mode is not enabled
	if(normalAlphaState == RENDER_ALPHA_CURRENT_INTERIOR_ONLY || normalAlphaState == RENDER_ALPHA_EXCLUDE_CURRENT_INTERIOR)
	{
		if(pEntity->GetIsTypePrtSys())
		{
			CPtFxSortedEntity* pSortedPtfxEntity = static_cast<CPtFxSortedEntity*>(pEntity);
			if(pSortedPtfxEntity->GetFxInst())
			{
				entityInteriorLocation.SetAsUint32(pSortedPtfxEntity->GetFxInst()->GetInteriorLocation());
			}
		}
		else
		{
			entityInteriorLocation = pEntity->GetInteriorLocation();
		}
	}

	return RenderAlphaInteriorCheck(entityInteriorLocation, isVehicleInteriorAlpha);
}

bool CRenderPhaseDrawSceneInterface::AlphaEntityHybridCheck(fwRenderPassId UNUSED_PARAM(id), fwRenderBucket UNUSED_PARAM(bucket), fwRenderMode UNUSED_PARAM(renderMode), int ASSERT_ONLY(subphasePass), float UNUSED_PARAM(sortVal), fwEntity *entity)
{
	Assertf(subphasePass == SUBPHASE_ALPHA_HYBRID, "Function should only be called when subPhasePass = %d, now subPhase = %d", SUBPHASE_ALPHA_HYBRID, subphasePass);
	CEntity *const pEntity = static_cast<CEntity*>(entity);
	CCustomShaderEffectBaseType *const cse = static_cast<const CBaseModelInfo*>(pEntity->GetArchetype())->GetCustomShaderEffectType();
	
	if (cse && cse->GetType() == CSE_CABLE)
	{
		return CCustomShaderEffectCable::IsCableVisible(pEntity);
	}else
	{
		return true;
	}
}


void CRenderPhaseDrawScene::BuildDrawListAlpha(int numEntityPasses)
{
	//doing this will make sure that we split the filter lists by this amount. If it's disabled setting it to 1 will not split the list at all
	int numFilterLists =(numEntityPasses > 1)?CRenderListBuilder::GetAlphaFilterListCount():1;
	
	int passIndex = 0;

	for(int filterListIndex = 0; filterListIndex < numFilterLists; filterListIndex++)
	{
		for(int entityIndex=0; entityIndex<numEntityPasses; entityIndex++, passIndex++)
		{
#if DRAW_LIST_MGR_SPLIT_DRAWSCENE
			if(numEntityPasses > 1)
			{
				DrawListPrologue(DL_RENDERPHASE_DRAWSCENE_ALPHA_01 + passIndex);

				DLC_Add(LockAlphaRenderTarget);

				CViewportManager::DLCRenderPhaseDrawInit();

#if USE_INVERTED_PROJECTION_ONLY
				grcViewport viewport = GetGrcViewport();
				viewport.SetInvertZInProjectionMatrix( true );
#else
				const grcViewport& viewport = GetGrcViewport();
#endif		
				DLC(dlCmdSetCurrentViewport, (&viewport));

				// Not sure if this is strictly necessary here, but it should be safe to do and won't cost much
				// if another thread has already setup the lights anyway
				Lights::SetupRenderThreadLights();
				
				// We won't inherit state from earlier passes if we're using multiple alpha lists, so we have to set it all up again
				DLC_Add(CShaderLib::ResetAllVar);
				DLC_Add(CTimeCycle::SetShaderParams);
				DLC_Add(PostFX::SetLDR16bitHDR16bit);
				DLC_Add(CShaderLib::ApplyDayNightBalance);
				DLC_Add(CShaderLib::SetGlobalEmissiveScale, 1.0f, false);
				DLC_Add(CRenderPhaseDrawSceneInterface::SetLateVehicleInteriorAlphaStateDC, m_lateVehicleInteriorAlphaState);
			}
#endif

			SetupDrawListAlphaState();

			const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrUpdateKeyframe();
			DLC_Add(Lights::SetupDirectionalAndAmbientGlobalsTweak, 1.0f - currKeyframe.GetVar(TCVAR_CLOUD_SHADOW_OPACITY), 1.0f, 1.0f);

			grcRenderTarget* depthBuffer = 
#if DEVICE_MSAA
				GRCDEVICE.GetMSAA() ? CRenderTargets::GetDepthResolved() :
#endif	//DEVICE_MSAA
#if __D3D11
				!GRCDEVICE.IsReadOnlyDepthAllowed()?CRenderTargets::GetDepthBufferCopy():
#endif
#if RSG_PC
				CRenderTargets::GetDepthBuffer_PreAlpha();
#else
				CRenderTargets::GetDepthBuffer();
#endif
			// update the depth buffer in the ptfx manager and in the ptfx_sprite shader params
			DLC_Add(CPtFxManager::UpdateDepthBuffer, depthBuffer);
#if RSG_PC
			DLC_Add(CPtFxManager::SetDepthWriteEnabled, (numEntityPasses>1)?true:false);
#endif			
			DLC_Add(ptxShaderTemplateList::SetGlobalVars);

			RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseDrawScene - alpha/fading");
			{
				fwRenderSubphase subPhase = ((m_renderAlphaState != RENDER_ALPHA_ALL)?SUBPHASE_ALPHA_INTERIOR:SUBPHASE_NONE);
				CRenderListBuilder::AddToDrawListAlpha(GetEntityListIndex(), CRenderListBuilder::RENDER_ALPHA|CRenderListBuilder::RENDER_FADING|CRenderListBuilder::RENDER_FADING_ONLY, CRenderListBuilder::USE_DEFERRED_LIGHTING, subPhase, NULL, NULL, NULL, entityIndex, numEntityPasses, filterListIndex, numFilterLists);
			}
			RENDERLIST_DUMP_PASS_INFO_END();
#if RSG_PC
			DLC_Add(CPtFxManager::SetDepthWriteEnabled, false);
#endif
			CleanupDrawListAlphaState();

#if DRAW_LIST_MGR_SPLIT_DRAWSCENE
			if(numEntityPasses > 1)
			{

				DLC_Add(CRenderPhaseDrawSceneInterface::SetLateVehicleInteriorAlphaStateDC, false);
				DLC_Add(UnLockAlphaRenderTarget, false);
				DrawListEpilogue(DL_RENDERPHASE_DRAWSCENE_ALPHA_01 + passIndex);
			}
#endif

		}
	}
}

GRC_ALLOC_SCOPE_SINGLE_THREADED_DECLARE(static, s_vehicleAllocScope)
static grcViewport* s_vehicleCurrentView = NULL;
static grcViewport s_vehiclePrevView;
static void lateVehicleStateSet(void)
{
	PF_PUSH_TIMEBAR_DETAIL("Late Vehicle Alpha");
	GRC_ALLOC_SCOPE_SINGLE_THREADED_PUSH(s_vehicleAllocScope)
		CShaderLib::SetGlobalEmissiveScale(1.0f);
	grcLightState::SetEnabled(true);
	// store viewport settings and matrices as they get changed
	s_vehicleCurrentView = grcViewport::GetCurrent();
	s_vehiclePrevView = *s_vehicleCurrentView; 
	grcViewport::SetCurrentWorldIdentity();
	grcStateBlock::SetBlendState(s_lateVehicleBlend);
	grcStateBlock::SetRasterizerState(s_lateVehicleRasterizer);
	grcStateBlock::SetDepthStencilState(CShaderLib::DSS_TestOnly_LessEqual_Invert);
	CRenderPhaseReflection::SetReflectionMap();
}

static void lateVehicleStateReset(void)
{
	// Revert viewport settings
	*s_vehicleCurrentView = s_vehiclePrevView;
	grcViewport::SetCurrent(s_vehicleCurrentView); 
	GRC_ALLOC_SCOPE_SINGLE_THREADED_POP(s_vehicleAllocScope)
		PF_POP_TIMEBAR_DETAIL();
}

// Lock the appropriate render targets for alpha rendering (depends on if we are doing alpha/particle DOF)
void CRenderPhaseDrawScene::LockAlphaRenderTarget()
{
#if PTFX_APPLY_DOF_TO_PARTICLES
	if (CPtFxManager::UseParticleDOF())
	{
		CPtFxManager::LockPtfxDepthAlphaMap(false);
	}
	else
#endif // PTFX_APPLY_DOF_TO_PARTICLES
	{
		// Re-lock the scene targets.
		CRenderTargets::LockSceneRenderTargets(true);
	}
}

// Unlock whatever was locked by CRenderPhaseDrawScene::LockAlphaRenderTarget()
void CRenderPhaseDrawScene::UnLockAlphaRenderTarget(bool finalize)
{
#if PTFX_APPLY_DOF_TO_PARTICLES
	if (CPtFxManager::UseParticleDOF())
	{
		CPtFxManager::UnLockPtfxDepthAlphaMap();
		if (finalize)
		{
			CPtFxManager::FinalizePtfxDepthAlphaMap();
		}
	}
	else
#endif // PTFX_APPLY_DOF_TO_PARTICLES
	{
		(void)finalize;
		CRenderTargets::UnlockSceneRenderTargets();
	}
}

void CRenderPhaseDrawScene::SetupDrawListAlphaState(bool forForcedAlpha)
{
	DLC_Add(CRenderPhaseReflection::SetReflectionMap);
	DLC_Add(CShaderLib::SetFogRayTexture);
	DLC_Add(CRenderPhaseCascadeShadowsInterface::SetDeferredLightingShaderVars);
	DLC_Add(CRenderPhaseCascadeShadowsInterface::SetSharedShaderVars);
	DLC_Add(CRenderPhaseDrawSceneInterface::SetLateInteriorStateDC, m_cameraInteriorLocation, m_renderAlphaState);
	DLC_Add(Lights::BeginLightweightTechniques);

	if( forForcedAlpha )
	{
		DLC(dlCmdSetDepthStencilState, (DeferredLighting::m_geometryPass_DS));
	}
	else
	{
		DLC(dlCmdSetDepthStencilState, (CShaderLib::DSS_LessEqual_Invert));
	}

	DLC(dlCmdSetRasterizerState, (grcStateBlock::RS_Default));
}
void CRenderPhaseDrawScene::CleanupDrawListAlphaState()
{
	DLC_Add(Lights::EndLightweightTechniques);

}

//----------------------------------------------------
// build and execute the drawlist for this renderphase
void CRenderPhaseDrawScene::BuildDrawList()
{
	if(!HasEntityListIndex() || !GetViewport()->IsActive() BANK_ONLY(|| DebugDeferred::m_GBufferIndex > 0))
	{
		return;
	}

#if !DRAW_LIST_MGR_SPLIT_DRAWSCENE
	DrawListPrologue();
#else // DRAW_LIST_MGR_SPLIT_DRAWSCENE
	DrawListPrologue(DL_RENDERPHASE_DRAWSCENE_FIRST);
	DrawListEpilogue(DL_RENDERPHASE_DRAWSCENE_FIRST);

	DrawListPrologue(DL_RENDERPHASE_DRAWSCENE_PRE_ALPHA);
#endif // DRAW_LIST_MGR_SPLIT_DRAWSCENE

	DLCPushTimebar("Setup");	

#if MULTIPLE_RENDER_THREADS
	CViewportManager::DLCRenderPhaseDrawInit();
#endif

#if USE_INVERTED_PROJECTION_ONLY
	grcViewport viewport = GetGrcViewport();
	viewport.SetInvertZInProjectionMatrix( true );
#else
	const grcViewport& viewport = GetGrcViewport();
#endif


	//Start out with locking Scene Rendertargets. Comments added as there will be more locks/unlock in the code below
	DLC_Add(CRenderTargets::LockSceneRenderTargets, true);

	DLC(dlCmdSetCurrentViewport, (&viewport));

#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES
	CRenderer::SetTessellationVars( viewport, (float)VideoResManager::GetSceneWidth()/(float)VideoResManager::GetSceneHeight() );
#endif //RAGE_SUPPORT_TESSELLATION_TECHNIQUES


	PF_FUNC(CRenderPhaseDrawScene);

	DLC_Add(CTimeCycle::SetShaderParams);

#if __XENON
	DLC_Add(PostFX::SetLDR10bitHDR10bit);
#elif __PS3
	DLC_Add(PostFX::SetLDR8bitHDR16bit);
#else
	DLC_Add(PostFX::SetLDR16bitHDR16bit);
#endif

	if (GetRenderFlags() & RENDER_SETTING_RENDER_SHADOWS)
	{
		DLC_Add(CRenderPhaseCascadeShadowsInterface::SetSharedShaderVars);
	}

	DLCPopTimebar();

	// Timebars don't work if we're spreading the alpha work across multiple render threads
	if(DRAWLISTMGR->GetNumAlphaEntityLists() == 1)
	{
		DLCPushTimebar("Render");
	}

#if RSG_ORBIS
	// Some alpha stuff writes to the depth, so unless we block the compression with DbRenderControl,
	// we have to decompress it again here
	// Keep depth compression disabled
	DLC_Add(GRCDEVICE.DecompressDepthSurface, static_cast<const sce::Gnm::DepthRenderTarget*>(NULL), BANK_SWITCH(g_bUseDisabledDepthCompression, true));
#endif

	DLC_Add(CShaderLib::ApplyDayNightBalance);

	XENON_ONLY( DLC_Add( dlCmdEnableAutomaticHiZ, true ) );

#if GPU_DAMAGE_WRITE_ENABLED && !(RSG_PC && __D3D11 && MULTIPLE_RENDER_THREADS)

	//Unlock scene rendertargets as DrawDamage locks it's own targets
	DLC_Add(CRenderTargets::UnlockSceneRenderTargets);
	
	DLCPushTimebar("DrawDamage"); 
	DLC_Add(CApplyDamage::DrawDamage); 
	DLCPopTimebar();

	//Lock Scene Render targets back
	DLC_Add(CRenderTargets::LockSceneRenderTargets, true);
#endif

	const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrUpdateKeyframe();

	if(CPickup::GetPool()->GetNoOfUsedSpaces() > 0)
	{
		DLCPushTimebar("Pickups"); 
		// If any pickups is in the water, we need to render before...
		DLC_Add(CRenderPhaseReflection::SetReflectionMap);
		DLC_Add(CShaderLib::SetFogRayTexture);
		DLC_Add(CRenderPhaseCascadeShadowsInterface::SetSharedShaderVars);
		DLC(dlCmdSetCurrentViewport, (&viewport));
		DLC_Add(Lights::BeginLightweightTechniques);
		DLC_Add(Lights::SetupDirectionalAndAmbientGlobalsTweak, 1.0f - currKeyframe.GetVar(TCVAR_CLOUD_SHADOW_OPACITY), 1.0f, 1.0f);
		DLC(dlCmdSetDepthStencilState, (CShaderLib::DSS_LessEqual_Invert));
		DLC(dlCmdSetRasterizerState, (grcStateBlock::RS_Default));
		DLC( dlCmdSetBucketsAndRenderMode, (CRenderer::RB_OPAQUE, rmStandard));

		for (s32 i=0; i<CPickup::GetPool()->GetSize(); i++)
		{
			CPickup* pPickup = CPickup::GetPool()->GetSlot(i);

			if (pPickup && pPickup->GetIsInWater() && pPickup->GetIsVisible()) 
			{
				fwDrawData* pDrawHandler = pPickup->GetDrawHandlerPtr();
				if (pDrawHandler)
				{
					CDrawDataAddParams drawParams;
					drawParams.AlphaFade = pPickup->GetAlpha();
					drawParams.m_SetupLights = true;
					pDrawHandler->AddToDrawList(pPickup, &drawParams);
				}
			}
		}

		DLC_Add(Lights::EndLightweightTechniques);
		DLCPopTimebar();
	}

#if __XENON
	DLC_Add(PostFX::SetLDR10bitHDR10bit);
#elif __PS3
	DLC_Add(PostFX::SetLDR8bitHDR16bit);
#else
	DLC_Add(PostFX::SetLDR16bitHDR16bit);
#endif

#if __BANK
	const bool isolatingCutscenePed = (CutSceneManager::GetInstance()->IsPlaying() && CutSceneManager::GetInstance()->IsIsolating());

	if(!isolatingCutscenePed)
#endif
	{
		DLCPushTimebar("Visual Effects Before Water");
		DLCPushGPUTimebar_Budgeted(GT_VISUALEFFECTS,"Visual Effects Before Water", 3.0f);
		CVisualEffects::RenderBeforeWater(GetRenderFlags());
		DLCPopGPUTimebar();
		DLCPopTimebar();

		// update the depth buffer in the ptfx manager and in the ptfx_sprite shader params
		DLCPushTimebar("PTFX Shadowed Buffer");
		grcRenderTarget* depthBuffer = 
#if DEVICE_MSAA
			GRCDEVICE.GetMSAA() ? CRenderTargets::GetDepthResolved() :
#endif	//DEVICE_MSAA
#if __D3D11
			!GRCDEVICE.IsReadOnlyDepthAllowed()?CRenderTargets::GetDepthBufferCopy():
#endif
			CRenderTargets::GetDepthBuffer();
		DLC_Add(CPtFxManager::UpdateDepthBuffer, depthBuffer);
		DLC_Add(ptxShaderTemplateList::SetGlobalVars);
#if __BANK
		if (!TiledScreenCapture::IsEnabled())
#endif // __BANK
		{
#if USE_SHADED_PTFX_MAP
			DLC_Add(CPtFxManager::UpdateShadowedPtFxBuffer);
#endif
		}
		DLC_Add(ptxShaderTemplateList::SetGlobalVars);
		DLCPopTimebar();
	}

	if(Water::IsCameraUnderwater())
	{
		// if we're underwater, draw the sky first ..
		DLCPushGPUTimebar(GT_SKY,"Sky");
		DLC_Add( CVisualEffects::RenderSky, (CVisualEffects::RM_DEFAULT), false, false, -1, -1, -1, -1, true, 1.0f );
		DLCPopGPUTimebar();
	}

#if __BANK
	if(!isolatingCutscenePed)
#endif
	{
		//Water Render
		Water::ProcessWaterPass();
	}

#if __DEV
	DLCPushMarker("Mirrors");
	if( CMirrors::GetIsMirrorVisible() )
	{
		DLC_Add(CMirrors::RenderDebug, false);
	}
	DLCPopMarker();
#endif // __DEV

	DLCPushGPUTimebar(GT_DEPTHPOSTFX,"DepthFX");
	XENON_ONLY( DLC_Add( dlCmdDisableHiZ, true ) );
	DLC_Add(CRenderPhaseDrawSceneInterface::Static_PostFxDepthPass);
	XENON_ONLY( DLC_Add( dlCmdEnableAutomaticHiZ, true ) );
	DLCPopGPUTimebar();

	DLC_Add(PostFX::SetLDR16bitHDR16bit);

	if(!Water::IsCameraUnderwater() BANK_ONLY(&& !isolatingCutscenePed))
	{
		// Render this first in order to cover sky dome pixels
		DLCPushGPUTimebar(GT_HORIZON_OBJECTS, "Horizon Objects");
		DLC_Add(CHorizonObjects::Render);
		DLCPopGPUTimebar();

		DLCPushGPUTimebar(GT_SKY,"Sky");
		DLC_Add( CVisualEffects::RenderSky, (CVisualEffects::RM_DEFAULT), false, false, -1, -1, -1, -1, true, CClouds::GetCloudAlpha());
		DLC_Add( CVisualEffects::IssueLensFlareOcclusionQuerries );
		DLCPopGPUTimebar();
	}

	DLC_Add(PostFX::SetLDR16bitHDR16bit);

	CCustomShaderEffectCable::UpdateWindParams();
	if ("Hybrid Alpha")
	{
		DLCPushTimebar("Hybrid Alpha");
		CCustomShaderEffectCable::SwitchDrawCore(true);
		SetupDrawListAlphaState();
		DLC_Add(CShaderLib::SetGlobalEmissiveScale, 1.0f, false);
		RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseStd - hybrid alpha");
		CRenderListBuilder::AddToDrawListAlpha(GetEntityListIndex(),
			CRenderListBuilder::RENDER_ALPHA|CRenderListBuilder::RENDER_FADING|CRenderListBuilder::RENDER_FADING_ONLY,
			CRenderListBuilder::USE_DEFERRED_LIGHTING, SUBPHASE_ALPHA_HYBRID);
		RENDERLIST_DUMP_PASS_INFO_END();
		CleanupDrawListAlphaState();
		CCustomShaderEffectCable::SwitchDrawCore(false);
		DLCPopTimebar();
	}

#if __BANK
	DLC_Add(CRenderPhaseDebugOverlayInterface::StencilMaskOverlayRender, 1);
#endif // __BANK

	// The call to Lights::SetupDepthForVolumetricLights() below may or may not have to lock another target.
	//Let unlock the scene rendertargets so we can make a copy of the depth buffer
	DLC_Add(CRenderTargets::UnlockSceneRenderTargets);

#if RSG_ORBIS
	//TODO: figure out why Decompress is needed here, since the depth compression is forbidden at this point
	//DLC_Add(GRCDEVICE.FlushCaches, grcDevice::CACHE_DEPTH);
	DLC_Add(GRCDEVICE.DecompressDepthSurface,
		reinterpret_cast<grcRenderTargetGNM*>(CRenderTargets::GetDepthBuffer())->GetDepthTarget(),
		BANK_SWITCH(g_bUseDisabledDepthCompression, true));
#endif //RSG_ORBIS

	//We need a copy of the depth buffer pre alpha for the depth of field through windows, etc...
	if( GRCDEVICE.GetMSAA()>1)
		DLC_Add(CRenderTargets::ResolveDepth, depthResolve_Dominant, CRenderTargets::GetDepthBuffer_PreAlpha());
	else
		DLC_Add(CRenderTargets::CopyDepthBuffer, CRenderTargets::GetDepthBuffer(), CRenderTargets::GetDepthBuffer_PreAlpha());	

#if RSG_PC
	if (VideoResManager::IsSceneScaled())
	{
		DLC_Add(CRenderTargets::CopyDepthBuffer, CRenderTargets::GetDepthBuffer_PreAlpha(), CRenderTargets::GetUIDepthBuffer_PreAlpha());
	}
#endif

	//This will setup draw command to downsample the depth buffer before alpha
	BANK_ONLY(if(DebugLighting::m_usePreAlphaDepthForVolumetricLight))
	{
		Lights::SetupRenderThreadLights();
		DLC_Add(Lights::SetupDepthForVolumetricLights);
	}

#if PTFX_APPLY_DOF_TO_PARTICLES
	if (CPtFxManager::UseParticleDOF())
	{
		DLC_Add(CPtFxManager::ClearPtfxDepthAlphaMap);
	}
#endif //PTFX_APPLY_DOF_TO_PARTICLES

	//Now lock the alpha render targets for BuildDrawListAlpha
	DLC_Add(LockAlphaRenderTarget);

	// Timebars don't work if we're spreading the alpha work across multiple render threads
	if(DRAWLISTMGR->GetNumAlphaEntityLists() == 1)
	{
		DLCPushTimebar("Alpha Bucket");
		DLCPushGPUTimebar_Budgeted(GT_FORWARD,"Alpha Bucket", 2.5f);
	}
	
	DLC_Add(ProcessLateStoredLightsWrapper);

	CViewport* pVp = gVpMan.GetGameViewport();
	m_cameraInteriorLocation.MakeInvalid();
	bool bPerformLateInteriorAlpha = false;


	// Alpha last for the player
	bool bPerformLateVehicleInteriorAlpha = ( camInterface::IsRenderedCameraInsideVehicle() || CVfxHelper::GetCamInVehicleFlag() || (CVfxHelper::GetRenderLastVehicle() != NULL && CVfxHelper::GetRenderLastVehicle()->IsVisible()) );	

	m_renderAlphaState = RENDER_ALPHA_ALL;
	m_lateVehicleInteriorAlphaState = bPerformLateVehicleInteriorAlpha;
	DLC_Add(CRenderPhaseDrawSceneInterface::SetLateVehicleInteriorAlphaStateDC, m_lateVehicleInteriorAlphaState);

	if(pVp)
	{
		CInteriorProxy* pInteriorProxy = pVp->GetInteriorProxy();
		CInteriorInst* pInterior = (pInteriorProxy==NULL) ? NULL : pInteriorProxy->GetInteriorInst();
		if(pInterior && CInteriorInst::GetPool()->IsValidPtr(pInterior))
		{
			m_cameraInteriorLocation.SetInteriorProxyIndex(CInteriorProxy::GetPool()->GetJustIndex(pInteriorProxy));
			m_cameraInteriorLocation.SetRoomIndex(pVp->GetRoomIdx());
		}
		//If the camera is in an interior, lets add Late Interior Alpha
		bPerformLateInteriorAlpha = m_cameraInteriorLocation.IsValid();
	}

	BANK_ONLY(bPerformLateInteriorAlpha = bPerformLateInteriorAlpha && !g_bDisableLateInteriorAlpha);

#if __BANK
	if (DebugDeferred::m_GBufferIndex == 0)
#endif // __BANK
	{
		// Timebars don't work if we're spreading the alpha work across multiple render threads
		if(DRAWLISTMGR->GetNumAlphaEntityLists() == 1)
		{
			DLCPushTimebar("DrawScene - Alpha/Fading");
		}

		m_renderAlphaState = RENDER_ALPHA_ALL;
		if(bPerformLateInteriorAlpha)
		{
			//Skip interior alpha 
			m_renderAlphaState &= (~RENDER_ALPHA_CURRENT_INTERIOR_ONLY);
		}
		if(bPerformLateVehicleInteriorAlpha)
		{
			//skip vehicle interior alpha
			m_renderAlphaState &= (~RENDER_VEHICLE_ALPHA_INTERIOR_ONLY);
		}

		//Adding forced alpha drawlist

		DLCPushTimebar("Forced Alpha - Opaque");

		//Disable the premultiply of directional lighting with cloud shadows as force alpha should use the same cloud shadows algorithm as used by shadow reveal for them to match
		DLC_Add(Lights::SetupDirectionalAndAmbientGlobals);
		SetupDrawListAlphaState(true);
		//We have to setup the cloud shadow parameters in order to get the right cloud shadows on the force alpha stuff
		DLC_Add(CRenderPhaseCascadeShadowsInterface::SetCloudShadowParams);
		CRenderListBuilder::AddToDrawList_ForcedAlpha(m_WaterRenderPhase->GetEntityListIndex(), CRenderListBuilder::RENDER_FADING_ONLY, CRenderListBuilder::USE_FORWARD_LIGHTING, SUBPHASE_NONE);
		DLC_Add(CVisualEffects::RenderForwardEmblems);
		CleanupDrawListAlphaState();

		DLC_Add(Lights::SetupDirectionalAndAmbientGlobalsTweak, 1.0f - currKeyframe.GetVar(TCVAR_CLOUD_SHADOW_OPACITY), 1.0f, 1.0f);
		DLCPopTimebar();

#if DRAW_LIST_MGR_SPLIT_DRAWSCENE
		if(DRAWLISTMGR->GetNumAlphaEntityLists() > 1)
		{
			//If there are more than one alpha passes, then we unlock the alpha render target as the function will lock
			//it's own alpha render targets for that renderthread
			DLC_Add(UnLockAlphaRenderTarget, false);			
			DrawListEpilogue(DL_RENDERPHASE_DRAWSCENE_PRE_ALPHA);
		}
#endif // DRAW_LIST_MGR_SPLIT_DRAWSCENE

		BuildDrawListAlpha(DRAWLISTMGR->GetNumAlphaEntityLists());
				
#if DRAW_LIST_MGR_SPLIT_DRAWSCENE
		if(DRAWLISTMGR->GetNumAlphaEntityLists() > 1)
		{
			DrawListPrologue(DL_RENDERPHASE_DRAWSCENE_POST_ALPHA);
			//If there are more than one alpha passes, then BuildDrawListAlpha will unlock the alpha render target
			//So let's lock it back
			DLC_Add(LockAlphaRenderTarget);

			Lights::SetupRenderThreadLights();
			CViewportManager::DLCRenderPhaseDrawInit();
			DLC(dlCmdSetCurrentViewport, (&viewport));
			DLC_Add(CShaderLib::ResetAllVar);
			DLC_Add(CTimeCycle::SetShaderParams);
			DLC_Add(PostFX::SetLDR16bitHDR16bit);
			DLC_Add(CShaderLib::ApplyDayNightBalance);
			DLC_Add(CRenderPhaseReflection::SetReflectionMap);
			DLC_Add(CShaderLib::SetFogRayTexture);
			DLC_Add(CRenderPhaseCascadeShadowsInterface::SetDeferredLightingShaderVars);
			DLC_Add(CRenderPhaseCascadeShadowsInterface::SetSharedShaderVars);
			DLC_Add(CRenderPhaseDrawSceneInterface::SetLateInteriorStateDC, m_cameraInteriorLocation, m_renderAlphaState);

			DLC_Add(CRenderPhaseDrawSceneInterface::SetLateVehicleInteriorAlphaStateDC, m_lateVehicleInteriorAlphaState);
		}
#endif // DRAW_LIST_MGR_SPLIT_DRAWSCENE

		// Timebars don't work if we're spreading the alpha work across multiple render threads
		if(DRAWLISTMGR->GetNumAlphaEntityLists() == 1)
		{
			DLCPopTimebar(); //"DrawScene - Alpha/Fading"
		}
	}

	// Timebars don't work if we're spreading the alpha work across multiple render threads
	if(DRAWLISTMGR->GetNumAlphaEntityLists() == 1)
	{
		DLCPopGPUTimebar();
		DLCPopTimebar(); //"Alpha Bucket"
	}

	//Locking back the scene render targets
	DLC_Add(UnLockAlphaRenderTarget, true);
	DLC_Add(CRenderTargets::LockSceneRenderTargets, true);

	DLC_Add(PostFX::SetLDR16bitHDR16bit);

	DLCPushTimebar("Distant Lights");
	DLCPushGPUTimebar(GT_DISTANTLIGHTS, "Distant Lights");
	DLC_Add(CVisualEffects::RenderDistantLights, CVisualEffects::RM_DEFAULT, GetRenderFlags(), 1.0f);
	DLCPopGPUTimebar();
	DLCPopTimebar();
	
	DLCPushTimebar("Displacement Bucket");
	DLCPushGPUTimebar(GT_DISPLACEMENT,"Displacement Bucket");
	// execute displacement alpha pass only if there're entities using this shader:
	if( CRenderListBuilder::CountDrawListEntities(GetEntityListIndex(), CRenderListBuilder::RENDER_DISPL_ALPHA, fwEntity::HAS_DISPL_ALPHA)>0 
#if __BANK
		&& (DebugDeferred::m_GBufferIndex == 0)
#endif // __BANK
		)
	{
		// set displacement map as reflection map:
		
#if RSG_PC
		// TODO - Should do a full screen copy of the back buffer but that is expensive.  
		// Ideally this would only be done if displacement entities actually exist which seems to be a rare thing.
		DLC_Add(CRenderPhaseReflection::SetReflectionMapExt, (grcTexture*)CRenderTargets::GetBackBufferCopy(false));
#else
		DLC_Add(CRenderPhaseReflection::SetReflectionMapExt, (grcTexture*)CRenderTargets::GetBackBuffer());
#endif // RSG_PC
		// required renderstate:
		DLC(dlCmdSetStates, (grcStateBlock::RS_Default, CShaderLib::DSS_LessEqual_Invert, grcStateBlock::BS_Normal));

		DLC_Add(Lights::BeginLightweightTechniques);
		RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseDrawScene - displacement alpha");
		CRenderListBuilder::AddToDrawList(GetEntityListIndex(), CRenderListBuilder::RENDER_DISPL_ALPHA, CRenderListBuilder::USE_DEFERRED_LIGHTING);
		RENDERLIST_DUMP_PASS_INFO_END();
		DLC_Add(Lights::EndLightweightTechniques);

		DLC_Add(CRenderPhaseReflection::SetReflectionMap);	// restore reflection map
	} // end of displacement alpha pass...
	DLCPopGPUTimebar();
	DLCPopTimebar(); //"Displacement Bucket";

	// Timebars don't work if we're spreading the alpha work across multiple render threads
	if(DRAWLISTMGR->GetNumAlphaEntityLists() == 1)
	{
		DLCPopTimebar(); //"Render"
	}

#if DEVICE_MSAA
	if(GRCDEVICE.GetMSAA()>1)
	{
		// Resolve light buffer before we draw visual effects
		DLC_Add(CRenderTargets::ResolveBackBuffer,true);
	}
#endif // DEVICE_MSAA

#if __XENON
	DLC_Add(PostFX::SetLDR10bitHDR10bit);
#elif __PS3
	DLC_Add(PostFX::SetLDR8bitHDR16bit);
#else
	DLC_Add(PostFX::SetLDR16bitHDR16bit);
#endif

#if __BANK
	if(!isolatingCutscenePed)
#endif
	{
		DLCPushTimebar("Visual Effects");
		DLCPushGPUTimebar_Budgeted(GT_VISUALEFFECTS,"Visual Effects", 2.5f);
		//This function will unlock scene targets, use the alpha render targets, and 
		//then lock the scene rendertargets
		CVisualEffects::RenderAfter(GetRenderFlags());
		DLCPopGPUTimebar();
		DLCPopTimebar();
	}

	//Start out with Alpha render targets locked
	DLC_Add(CRenderTargets::UnlockSceneRenderTargets);
	DLC_Add(LockAlphaRenderTarget);

	//render late interior stuff here
	if(bPerformLateInteriorAlpha)
	{
		//render only interior alpha
		m_renderAlphaState = RENDER_ALPHA_ALL & (~RENDER_ALPHA_EXCLUDE_CURRENT_INTERIOR);
		if(bPerformLateVehicleInteriorAlpha)
		{
			//if this mode is enable, make sure we skip vehicle interior alpha
			//satisfies the vehicle in interior situation
			m_renderAlphaState &= (~RENDER_VEHICLE_ALPHA_INTERIOR_ONLY);
		}

		//Building draw list with alpha targets locked.
		//BuildDrawList does not change targets if number of passes is 1
		DLC_Add(CRenderPhaseDrawSceneInterface::SetLateInteriorStateDC, m_cameraInteriorLocation, m_renderAlphaState);
		DLCPushTimebar("DrawScene - Late Interior Alpha/Fading");
		DLCPushGPUTimebar(GT_LATE_INTERIOR_ALPHA,"Late Interior Alpha");
		BuildDrawListAlpha();
		DLCPopGPUTimebar();
		DLCPopTimebar(); //"DrawScene - Late Interior Alpha/Fading"

		//RenderLateInterior performs it's own lock/unlock so make sure we start out with the scene rendertargets
		DLC_Add(UnLockAlphaRenderTarget, false);
		DLC_Add(CRenderTargets::LockSceneRenderTargets, true);
		CVisualEffects::RenderLateInterior(GetRenderFlags());

		//At this point the scene rendertargets are still locked
		DLC_Add(CRenderTargets::UnlockSceneRenderTargets);
		DLC_Add(LockAlphaRenderTarget);
		//now its back to Alpha Render Targets
	}

	CVisualEffects::ResetDecalsForwardContext();

	//render late Vehicle interior stuff here
	if(bPerformLateVehicleInteriorAlpha)
	{
		//start out with alpha render targets locked

		//render only interior vehicle alpha
		m_renderAlphaState = RENDER_VEHICLE_ALPHA_INTERIOR_ONLY;
		DLC_Add(CRenderPhaseDrawSceneInterface::SetLateInteriorStateDC, m_cameraInteriorLocation, m_renderAlphaState);

		//Building draw list with alpha targets locked.
		//BuildDrawList does not change targets if number of passes is 1
		DLCPushTimebar("DrawScene - Late Vehicle Interior Alpha/Fading");
		DLCPushGPUTimebar(GT_LATE_INTERIOR_ALPHA,"Late Vehicle Interior Alpha");
		DLC_Add( lateVehicleStateSet );
		BuildDrawListAlpha();
		//CDrawListPrototypeManager::Flush();
		DLC_Add( lateVehicleStateReset);
		DLCPopGPUTimebar();
		DLCPopTimebar(); //"DrawScene - Late Vehicle Interior Alpha/Fading"		

		//RenderLateInterior performs it's own lock/unlock so make sure we start out with the scene rendertargets
		DLC_Add(UnLockAlphaRenderTarget, false);
		DLC_Add(CRenderTargets::LockSceneRenderTargets, true);
		CVisualEffects::RenderLateVehicleInterior(GetRenderFlags());

		//At this point the scene rendertargets are still locked
		DLC_Add(CRenderTargets::UnlockSceneRenderTargets);
		DLC_Add(LockAlphaRenderTarget);
		//now its back to Alpha Render Targets
	}

	DLC_Add(UnLockAlphaRenderTarget, true);

	//Lock scene render targets back
	DLC_Add(CRenderTargets::LockSceneRenderTargets, true);

	m_renderAlphaState = RENDER_ALPHA_ALL;
	DLC_Add(CRenderPhaseDrawSceneInterface::SetLateInteriorStateDC, m_cameraInteriorLocation, m_renderAlphaState);

	//reset late vehicle interior alpha state
	m_lateVehicleInteriorAlphaState = false;
	DLC_Add(CRenderPhaseDrawSceneInterface::SetLateVehicleInteriorAlphaStateDC, m_lateVehicleInteriorAlphaState);

	//Render Coronas after visual effects because it is a lens based effect and nothing should stomp on it
	DLCPushTimebar("Coronas");
	DLCPushGPUTimebar(GT_CORONAS,"Coronas");
	XENON_ONLY( DLC_Add( dlCmdDisableHiZ, true ) ); 
	DLC_Add(CVisualEffects::RenderCoronas, CVisualEffects::RM_DEFAULT, 1.0f, 1.0f);
	XENON_ONLY( DLC_Add( dlCmdEnableAutomaticHiZ, true ) ); 
	DLCPopGPUTimebar();
	DLCPopTimebar();

	// Render the lens flare
	if(!Water::IsCameraUnderwater())
	{
		DLC_Add( CVisualEffects::RenderLensFlare, (camInterface::GetFrame().GetFlags() & (camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation)) > 0);
 	}

	XENON_ONLY( DLC_Add( dlCmdDisableHiZ, true ) ); // Do we want this on for any passes below?

#if !__WIN32PC || __D3D11 || RSG_ORBIS
		DLCPushGPUTimebar_Budgeted(GT_SSA,"SSA",1.5f);
		DLC_Add( PostFX::ProcessSubSampleAlpha, CRenderTargets::GetBackBuffer(), CRenderTargets::GetDepthBuffer() );
		DLCPopGPUTimebar();
#endif // !__WIN32PC

	DLCPushTimebar("Finalize");

#if __PS3
	if (CPhotoManager::AreRenderPhasesDisabled() == false)
#endif
	{
		DLCPushGPUTimebar(GT_VOLUMETRICEFFECT,"Volumetric Effects");
	#if __XENON
		static dev_s32 debugVertexThreads=32;
		DLC ( dlCmdSetShaderGPRAlloc, (debugVertexThreads,128-debugVertexThreads));
	#endif //__XENON
		Lights::AddToDrawlistVolumetricLightingFx((GetRenderFlags() & RENDER_SETTING_RENDER_SHADOWS) != 0);
	#if __XENON
		DLC ( dlCmdSetShaderGPRAlloc, (0,0));
	#endif //__XENON
		DLCPopGPUTimebar();
	}

#if RSG_ORBIS
	DLC_Add(ReenableDepthCompression);
#endif

	DLC_Add(CRenderTargets::UnlockSceneRenderTargets);

#if DRAW_LIST_MGR_SPLIT_DRAWSCENE
	DLCPopTimebar(); // "Finalize"
	if(DRAWLISTMGR->GetNumAlphaEntityLists() == 1)
	{
		DrawListEpilogue(DL_RENDERPHASE_DRAWSCENE_PRE_ALPHA);
	}
	else
	{
		DrawListEpilogue(DL_RENDERPHASE_DRAWSCENE_POST_ALPHA);
	}
	
	DrawListPrologue(DL_RENDERPHASE_DRAWSCENE_NON_DEPTH_FX);

	CViewportManager::DLCRenderPhaseDrawInit();
	DLC(dlCmdSetCurrentViewport, (&viewport));
	DLC_Add(CShaderLib::ResetAllVar);
	DLC_Add(CTimeCycle::SetShaderParams);
	DLC_Add(PostFX::SetLDR16bitHDR16bit);
	DLC_Add(CShaderLib::ApplyDayNightBalance);
#endif // DRAW_LIST_MGR_SPLIT_DRAWSCENE

	//====================== PostFX Main Pass ==========================
	DLCPushGPUTimebar_Budgeted(GT_POSTFX,"PostFX",5.0f);
	XENON_ONLY(DLC_Add(CRenderTargets::GetBackBuffer7e3toInt, true);)
	if(Water::IsCameraUnderwater())
	{
		DLC_Add(CRenderPhaseCascadeShadowsInterface::SetSharedShaderVars);
		DLC_Add(CopyShadowToMiniMap);
	}
#if RSG_PC
	if (GRCDEVICE.UsingMultipleGPUs())
	{
		if (PostFX::GetPauseResolve() == PostFX::RESOLVE_PAUSE)
		{
			DLC_Add(CRenderTargets::ResolveForMultiGPU, PostFX::RESOLVE_PAUSE);
			DLC_Add(PostFX::DoPauseResolve,PostFX::IN_RESOLVE_PAUSE);
			DLC_Add(PostFX::ResetDOFRenderTargets);
			DLC_Add(CMiniMap_RenderThread::ResetMultiGPUCount);

			PostFX::DoPauseResolve(PostFX::IN_RESOLVE_PAUSE);
		}
		else if (PostFX::GetPauseResolve() == PostFX::UNRESOLVE_PAUSE)
		{
			DLC_Add(CRenderTargets::ResolveForMultiGPU, PostFX::UNRESOLVE_PAUSE);
			DLC_Add(PostFX::DoPauseResolve,PostFX::NOT_RESOVE_PAUSE);
			DLC_Add(PostFX::ResetDOFRenderTargets);

			PostFX::DoPauseResolve(PostFX::NOT_RESOVE_PAUSE);
		}
	}
#endif
	DLC (dlCmdProcessNonDepthFX, ());
	DLCPopGPUTimebar();
	//=================================================================

#if DRAW_LIST_MGR_SPLIT_DRAWSCENE
	DrawListEpilogue(DL_RENDERPHASE_DRAWSCENE_NON_DEPTH_FX);
	DrawListPrologue(DL_RENDERPHASE_DRAWSCENE_DEBUG);
	CViewportManager::DLCRenderPhaseDrawInit();
	DLC(dlCmdSetCurrentViewport, (&viewport));
#endif // DRAW_LIST_MGR_SPLIT_DRAWSCENE
	
	//A bunch of Debug UI stuff goes here.
	DLC_Add(CRenderTargets::LockUIRenderTargets);
	
	DLCPushGPUTimebar(GT_DEBUG,"Debug");

#if __BANK 
	// Only show SSAO
	if (SSAO::Isolated())
	{
		DLC_Add(SSAO::Debug);
	}

	//EntitySelect Debug Draw
	ENTITYSELECT_ONLY(CRenderPhaseEntitySelect::BuildDebugDrawList());
#endif // __BANK

#if !DRAW_LIST_MGR_SPLIT_DRAWSCENE
	DLCPopTimebar(); // "Finalize"
#endif // !DRAW_LIST_MGR_SPLIT_DRAWSCENE

	//Is this still needed? It's not marked for bank only I and I'll need to keep an lock and unlock around just for this

	DLC_Add(CRenderTargets::UnlockUIRenderTargets, false);

	if(CObjectPopulation::IsCObjectsAlbedoSupportEnabled())
	{
	#if __D3D11 || RSG_ORBIS
		#if RSG_PC
			grcRenderTarget* pDepthBuffer = VideoResManager::IsSceneScaled() ? CRenderTargets::GetUIDepthBuffer_PreAlpha() : CRenderTargets::GetDepthBuffer_PreAlpha();
		#else
			grcRenderTarget* pDepthBuffer = CRenderTargets::GetDepthBuffer_PreAlpha();
		#endif
		DLC(dlCmdLockRenderTarget, (0, grcTextureFactory::GetInstance().GetBackBuffer(), pDepthBuffer));
	#endif

		CObjectPopulation::RenderCObjectsAlbedo();

	#if __D3D11 || RSG_ORBIS
		DLC(dlCmdUnLockRenderTarget, (0));
	#endif
	}


#if __D3D11 || RSG_ORBIS
	#if RSG_PC
		grcRenderTarget* pDepthBuffer = GRCDEVICE.IsReadOnlyDepthAllowed() ? CRenderTargets::GetDepthBuffer_PreAlpha_ReadOnly() :
			VideoResManager::IsSceneScaled() ? CRenderTargets::GetUIDepthBuffer_PreAlpha() : CRenderTargets::GetDepthBuffer_PreAlpha();
	#else
		grcRenderTarget* pDepthBuffer = CRenderTargets::GetDepthBuffer_PreAlpha();
	#endif
	DLC(dlCmdLockRenderTarget, (0, grcTextureFactory::GetInstance().GetBackBuffer(), pDepthBuffer));
#endif

	CVisualEffects::RenderMarkers(CVisualEffects::MM_ALL);
	UIWorldIconManager::AddItemsToDrawListWrapper();

#if __D3D11 || RSG_ORBIS
	DLC(dlCmdUnLockRenderTarget, (0));
#endif

	DLCPopGPUTimebar();
	
	DLC(dlCmdSetCurrentViewportToNULL, ());

#if !DRAW_LIST_MGR_SPLIT_DRAWSCENE
	DrawListEpilogue();
#else // !DRAW_LIST_MGR_SPLIT_DRAWSCENE
	DrawListEpilogue(DL_RENDERPHASE_DRAWSCENE_DEBUG);
	
	DrawListPrologue(DL_RENDERPHASE_DRAWSCENE_LAST);
	DrawListEpilogue(DL_RENDERPHASE_DRAWSCENE_LAST);
#endif // !DRAW_LIST_MGR_SPLIT_DRAWSCENE
}


void CRenderPhaseDrawSceneInterface::Init(unsigned initMode)
{
	if (initMode == INIT_CORE)
	{
		// called once at the very start of the game
		grcBlendStateDesc blendDesc;
		blendDesc.BlendRTDesc[0].BlendEnable = TRUE;
		blendDesc.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
		blendDesc.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
		blendDesc.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
		s_lateVehicleBlend = grcStateBlock::CreateBlendState(blendDesc);

		grcRasterizerStateDesc rasterizerDesc;
		rasterizerDesc.CullMode = grcRSV::CULL_BACK;
		s_lateVehicleRasterizer = grcStateBlock::CreateRasterizerState(rasterizerDesc);
	}

}

__COMMENT(static) void CRenderPhaseDrawSceneInterface::InitDLCCommands()
{
	DLC_REGISTER_EXTERNAL(dlCmdProcessNonDepthFX);
}

__COMMENT(static) const grcViewport* CRenderPhaseDrawSceneInterface::GetViewport()
{
	return g_DrawSceneRenderPhaseViewport;
}

__COMMENT(static) CRenderPhaseDrawScene* CRenderPhaseDrawSceneInterface::GetRenderPhase()
{
	return g_DrawSceneRenderPhase;
}

//
//
//
//
void CRenderPhaseDrawSceneInterface::Static_BlitRT(grcRenderTarget* dst, grcRenderTarget* src, CSprite2d::CustomShaderType type)
{
	Assert(src);
	Assert(dst);
	grcTextureFactory::GetInstance().LockRenderTarget(0, dst, NULL);

	grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Default);

	CSprite2d spriteBlit;
	spriteBlit.BeginCustomList(type, src, -1);
	grcDrawSingleQuadf(-1,1,1,-1,0,0,0,1,1,Color32(255,255,255,255));
	spriteBlit.EndCustomList();

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);	
}

void CRenderPhaseDrawSceneInterface::Static_PostFxDepthPass()
{
	PostFX::ProcessDepthFX(); 
}

void CRenderPhaseDrawSceneInterface::SetLateInteriorStateDC(fwInteriorLocation cameralocation, u32 renderAlphaState)	
{ 
	Assertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "Should be called from Render Thread"); 
	ms_renderAlphaStateRT = renderAlphaState; 
	ms_cameraInteriorLocationRT = cameralocation; 
}

void CRenderPhaseDrawSceneInterface::SetLateVehicleInteriorAlphaStateDC(bool value)
{
	Assertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "Should be called from Render Thread"); 
	ms_lateVehicleInteriorAlphaStateRT = value; 
}
bool CRenderPhaseDrawSceneInterface::GetLateVehicleInterAlphaState_Render()
{
	Assertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "Should be called from Render Thread"); 
	return ms_lateVehicleInteriorAlphaStateRT; 
}

bool CRenderPhaseDrawSceneInterface::IsRenderLateVehicleAlpha()	
{ 
	Assertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "Should be called from Render Thread"); 
	u32 vehicleAlphaState = ms_renderAlphaStateRT & (RENDER_VEHICLE_ALPHA_EXCLUDE_INTERIOR | RENDER_VEHICLE_ALPHA_INTERIOR_ONLY);  
	return (vehicleAlphaState == RENDER_VEHICLE_ALPHA_INTERIOR_ONLY); 
}
