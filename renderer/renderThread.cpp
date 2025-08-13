/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    renderThread.cpp
// PURPOSE : stuff for setting up separate render thread & functions it requires
// AUTHOR :  john w.
// CREATED : 1/8/06
//
/////////////////////////////////////////////////////////////////////////////////

// 
// renderer/renderThread.cpp 
// 
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved. 
// 
#include "renderthread.h"
#if __PPU
// PS3 headers
#include <cell/gcm.h>
#endif

// Rage headers
#include "fwscene/stores/imposedTextureDictionary.h"
#include "grcore/config.h"
#include "grcore/device.h"
#include "grmodel/geometry.h"
#include "grmodel/setup.h"
#include "grprofile/timebars.h"
#include "rmptfx/ptxmanager.h"
#include "rmptfx/ptxdrawlist.h"
#include "system/cache.h"
#include "system/criticalsection.h"
#include "system/param.h"
#include "system/xtl.h"
#include "breakableglass/bgdrawable.h"
#include "breakableglass/glassmanager.h"
#include "fwpheffects/ropemanager.h"
#include "fwpheffects/taskmanager.h"
#if __D3D
#include "system/d3d9.h"
#endif
#include "system/memory.h"

#if __PPU
#include "grcore/wrapper_gcm.h"
#include "system/replay.h"
#endif

#if RSG_PC && BUGSTAR_INTEGRATION_ACTIVE
#include "qa/BugstarInterface.h"
#endif // RSG_PC && BUGSTAR_INTEGRATION_ACTIVE

// Framework headers
#include "fwanimation/animdirector.h"

// Game headers
#include "audio/northaudioengine.h"
#include "audio/frontendaudioentity.h"
#include "camera/CamInterface.h"
#include "camera/viewports/ViewportManager.h"
#include "cutscene/CutSceneManagerNew.h"
#include "debug/bar.h"
#include "debug/BudgetDisplay.h"
#include "debug/Editing/LightEditing.h"
#include "debug/FrameDump.h"
#include "debug/textureviewer/textureviewer.h"
#include "frontend/credits.h"
#include "frontend/PauseMenu.h"
#include "frontend/WarningScreen.h"
#include "frontend/MiniMap.h"
#include "frontend/GolfTrail.h"
#include "frontend/profilesettings.h"
#include "frontend/loadingscreens.h"
#if RSG_PC
#include "Frontend/MousePointer.h"
#endif // #if RSG_PC
#include "Frontend/MultiplayerGamerTagHud.h"
#include "Frontend/VideoEditor/ui/Editor.h"
#include "network/Live/NetworkClan.h"	//For deferred deletion of crew emblems
#include "physics/physics.h"
#include "peds/rendering/pedVariationPack.h"
#include "peds/Ped.h"
#include "performance/clearinghouse.h"
#include "renderer/DrawLists/drawListMgr.h"
#include "renderer/Lights/LODLights.h"
#include "renderer/renderListGroup.h"
#include "renderer/renderthread.h"
#include "renderer/Water.h"
#include "renderer/gtaDrawable.h"
#include "renderer/OcclusionQueries.h"
#include "renderer/RenderTargetMgr.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseStd.h"
#include "renderer/RenderPhases/RenderPhaseHeightMap.h"
#include "renderer/RenderPhases/RenderPhaseMirrorReflection.h"
#if RSG_PC
#include "renderer/River.h"
#endif
#include "renderer/PostProcessFX.h"
#include "renderer/PostProcessFXHelper.h"
#include "renderer/PlantsMgr.h"
#include "renderer/SpuPM/SpuPmMgr.h"
#include "renderer/ScreenshotManager.h"
#include "SaveLoad/savegame_autoload.h"
#include "SaveLoad/savegame_photo_manager.h"
#include "scene/lod/LodScale.h"
#include "scene/texLod.h"
#include "shaders/CustomShaderEffectTree.h"
#include "streaming/streamingengine.h"
#include "streaming/defragmentation.h"
#include "streaming/streamingdebug.h"
#include "system/controlMgr.h"
#include "system/system.h"
#include "system/gcmPerfmon.h"
#include "system/TamperActions.h"
#include "system/taskscheduler.h"
#include "system/ThreadPriorities.h"
#include "profile/cputrace.h"
#include "script/script.h"
#include "script/script_hud.h"
#include "Vfx/VisualEffects.h"
#include "Vfx/Misc/Coronas.h"
#include "vfx/misc/DistantLightsVertexBuffer.h"
#include "vfx/misc/Puddles.h"
#include "vfx/misc/LensFlare.h"
#include "Vfx/Particles/PtFxManager.h"
#include "text/Text.h"
#include "text/TextConversion.h"
#include "text/TextFile.h"
#include "text/TextFormat.h"
#include "shaders/ShaderEdit.h"
#include "peds/rendering/PedDamage.h"
#include "control/replay/ReplaySupportClasses.h"
#include "replaycoordinator/ReplayCoordinator.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"
#include "control/videorecording/videorecording.h"

#if NAVMESH_EXPORT
#include "pathserver/ExportCollision.h"
#endif

bool CGtaOldLoadingScreen::m_bForceLoadingScreen = false;
grcDepthStencilStateHandle CGtaOldLoadingScreen::m_depthState = grcStateBlock::DSS_Invalid;

#define FADEOUT_WITHOUT_LOADINGBAR	(2.0f)


#if __PPU
#include <sys/event.h>
#include "grcore/wrapper_gcm.h"
#endif // __PPU

#if __WIN32PC
namespace rage {
	XPARAM(d3dmt);
}
#endif

#if	!__FINAL
PARAM(debugonfades, "[code] display debug info during fades");
#endif  // !__FINAL

#if __BANK
extern void SetLastGenMode();
#endif // __BANK


namespace rage
{
	extern bool g_PauseClothSimulation;
}

static sysTimer g_LoadingTimer;
static float	g_fCurrentTimer = 0.0f;

CGtaRenderThreadGameInterface gGtaRenderThreadGameInterface;

// --- CRenderThreadInterface ------------------------------
// handle any setup & interface to the render thread proper

void CGtaRenderThreadGameInterface::ShutdownLevel(unsigned /*shutdownMode*/)
{
	DRAWLISTMGR->ClothCleanup();
	DRAWLISTMGR->CharClothCleanup();
}

#if RSG_PC
static void doRenderThreadFunction()
{
	gRenderThreadInterface.DoRenderFunction();
}
#endif

void CGtaRenderThreadGameInterface::InitClass()
{
	gRenderThreadInterface.SetGameInterface(&gGtaRenderThreadGameInterface);
#if RSG_PC
	GRCDEVICE.RenderFunctionCallback = doRenderThreadFunction;
#endif
}

bool CGtaOldLoadingScreen::IsUsingLoadingRenderFunction()
{
	if(gRenderThreadInterface.GetRenderFunction() == MakeFunctor(LoadingRenderFunction))
	{
		return true;
	}

	return false;
}

void CGtaRenderThreadGameInterface::PerformPtfxSafeModeOperations(BANK_ONLY(PtfxSafeModeOperationModeDebug ptfxSafeModeOperationDebug))
{
	BANK_ONLY(if(ptfxSafeModeOperationDebug == PTFX_SAFE_MODE_OPERATION_DEBUG_ALL || ptfxSafeModeOperationDebug == PTFX_SAFE_MODE_OPERATION_DEBUG_CALLBACKEFFECTS))
	{
		PF_AUTO_PUSH_TIMEBAR("g_ptFxManager.BeginFrame");

		//Let's perform the flip for particles in safe execution
		g_ptFxManager.BeginFrame();		

		int numEffects = 0;
		for(int i = 0; i < RMPTFXMGR.GetNumDrawLists(); i++)
		{
			numEffects += RMPTFXMGR.GetDrawList(i)->m_effectInstList.GetNumItems();
		}

		perfClearingHouse::SetValue(perfClearingHouse::PTFX, numEffects);

		ptxPointPool &pointPool = RMPTFXMGR.GetPointPool();

		perfClearingHouse::SetValue(perfClearingHouse::PARTICLES, pointPool.GetNumItemsActive());

	}

	BANK_ONLY(if(ptfxSafeModeOperationDebug == PTFX_SAFE_MODE_OPERATION_DEBUG_ALL || ptfxSafeModeOperationDebug == PTFX_SAFE_MODE_OPERATION_DEBUG_INSTANCE_SORTING))
	{
#if PTFX_ALLOW_INSTANCE_SORTING
#if __DEV
		// Yes, I know. But I promise we don't do any world::Add/Remove
		CGameWorld::AllowDelete(true);
#endif

		g_ptFxManager.GetSortedInterface().CleanUp(); // Post update cleanup.
		if (CRenderPhaseDrawSceneInterface::GetRenderPhase())
		{
			g_ptFxManager.GetSortedInterface().AddToRenderList(CRenderPhaseDrawSceneInterface::GetRenderPhase()->GetRenderFlags(), CRenderPhaseDrawSceneInterface::GetRenderPhase()->GetEntityListIndex());
		}

#if PTFX_RENDER_IN_MIRROR_REFLECTIONS
		if(CRenderPhaseMirrorReflectionInterface::GetRenderPhase() BANK_ONLY( && !g_ptFxManager.GetGameDebugInterface().GetDisableRenderMirrorReflections()))
		{
			g_ptFxManager.GetSortedInterface().AddToRenderList(CRenderPhaseMirrorReflectionInterface::GetRenderPhase()->GetRenderFlags(), CRenderPhaseMirrorReflectionInterface::GetRenderPhase()->GetEntityListIndex());
		}
#endif
		
#if __DEV
		// Yes, I know. But I promise we don't do any world::Add/Remove
		CGameWorld::AllowDelete(false);
#endif
#endif // PTFX_ALLOW_INSTANCE_SORTING	
	}

}

void CGtaRenderThreadGameInterface::PerformSafeModeOperations()
{
	PF_AUTO_PUSH_TIMEBAR_BUDGETED("Safe Execution", 1.0f);

	if (fwTxd::GetCurrent())
	{
		TASKLOG_ONLY(s_TaskLog.Dump());
		Quitf(ERR_GFX_RENDERTHREAD_TXD,"Someone forgot to pop a dictionary pointer!");
	}

#if GTA_REPLAY
#if VIDEO_RECORDING_ENABLED
	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("VideoRecording", 1.0f);
		VideoRecording::Update();
	}
#endif
	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("CReplayMgr", 1.0f);
		CReplayMgr::ProcessRecord(fwTimer::GetReplayTimeInMilliseconds());
	}
#endif // GTA_REPLAY

#if RSG_PC
	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("UpdateFlowMaps", 1.0f);
		River::UpdateFlowMaps();
	}
#endif

	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("Lights::WaitForProcessVisibilityDependency", 1.0f);
		Lights::WaitForProcessVisibilityDependency();
	}

	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("UpdateHighQualityScreenshots", 1.0f);
		// CHighQualityScreenshot::Update();
		CPhotoManager::UpdateHighQualityScreenshots();
	}

#if GTA_REPLAY
#if REPLAY_USE_PER_BLOCK_THUMBNAILS
	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("CReplayThumbnail", 1.0f);
		CReplayThumbnail::ProcessQueueForPopulation();
	}
#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS
#endif // GTA_REPLAY
	
#if USE_GCMPERFMON
	pm_Get();
#endif // USE_GCMPERFMON

#if __BANK
	SetLastGenMode();
#endif // __BANK
	
#if __BANK
	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("Banks", 1.0f);
		g_ShaderEdit::InstanceRef().ApplyUpdates();
		Water::UpdateShaderEditVars();
		CDebugTextureViewerInterface::Synchronise();
		DebugEditing::LightEditing::Update();
		CFrameDump::Update();

		TheText.DeleteAllFlaggedEntriesFromDebugStringMap();
	}
#endif // __BANK
	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("Synchronise", 1.0f);
		CRenderPhaseCascadeShadowsInterface::Synchronise();
		CParaboloidShadow::Synchronise();
		PostFX::g_adaptation.Synchronise();
		Lights::Synchronise();
		CLODLights::Synchronise();
		CutSceneManager::Synchronise(); 
	}
#if RSG_PC
	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("CMousePointer", 1.0f);
		CMousePointer::ResetMouseInput();  // reset mouse input before any scaleform or hud/frontend items
	}
#endif // #if RSG_PC
	
	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("CDistantLightsVertexBuffer", 1.0f);
		CDistantLightsVertexBuffer::FlipBuffers();
	}

#if TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("CCustomShaderEffectTree", 1.0f);
		CCustomShaderEffectTree::Synchronise();
	}
#endif // TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS

	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("GetRopeManager", 1.0f);
		CPhysics::GetRopeManager()->SafeUpdate();
	}
	
	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("ClothCleanup", 1.0f);
		DRAWLISTMGR->ClothCleanup();
		DRAWLISTMGR->CharClothCleanup();
	}

	if (!CSavegameAutoload::IsPerformingAutoLoadAtStartOfGame())
	{
		{
			PF_AUTO_PUSH_TIMEBAR_BUDGETED("CVisualEffects::UpdateSafe", 1.0f);
			// safely update the visual effects 
			CVisualEffects::UpdateSafe();
		}
		{
			PF_AUTO_PUSH_TIMEBAR_BUDGETED("PHONEPHOTOEDITOR.UpdateSafe", 1.0f);
			PHONEPHOTOEDITOR.UpdateSafe();
		}
	}

	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("CTaskScheduler::WaitOnResults", 1.0f);
		CTaskScheduler::WaitOnResults();
	}

	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("RenderTargets", 1.0f);
		CRenderPhaseHeight *rp = CRenderPhaseHeight::GetPtFxGPUCollisionMap();
		if( rp ) rp->SwapRenderTargets();	// swap doublebuffered heightmap

		gRenderTargetMgr.CleanupNamedRenderTargets();
	}

	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("PerformPtfxSafeModeOperations", 1.0f);
		PerformPtfxSafeModeOperations(BANK_ONLY(PTFX_SAFE_MODE_OPERATION_DEBUG_ALL));
	}
	
	//Let's process Deferred Additions to render list here
	//Because vehicle glass and Particles adds entities to the deferred list
	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("ProcessDeferredAdditions", 1.0f);
		CGtaRenderListGroup::ProcessDeferredAdditions();
	}

#if USE_DEFRAGMENTATION // as late as possible to allow as much drawable spu work to finish, because in some cases we have to block until they're finished - KS
	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("Defragmentation", 1.0f);
#if __XENON && __MASTER
		// HACK - Talk to Klaas before changing this
		bool debuggerAttached = sysBootManager::GetChrome();
		if (!debuggerAttached)
		// HACK - Talk to Klaas before changing this
#endif // __XENON && __MASTER
		{
			PF_AUTO_PUSH_TIMEBAR_BUDGETED("CTexLod::SetSwapStateAll", 1.0f);
			// shitty way of solving this. Intercept the txd::place in order to fix up a single txd instance
			CTexLod::SetSwapStateAll(false, true);
		}
	
		{
			PF_AUTO_PUSH_TIMEBAR_BUDGETED("strStreamingEngine", 1.0f);
			strStreamingEngine::GetDefragmentation()->Update();	
		}

#if __XENON && __MASTER
		// HACK - Talk to Klaas before changing this
		if (!debuggerAttached)
		// HACK - Talk to Klaas before changing this
#endif // __XENON && __MASTER
		{
			PF_AUTO_PUSH_TIMEBAR_BUDGETED("SetSwapStateAll", 1.0f);
			CTexLod::SetSwapStateAll(true, true);
		}
	}
#endif // USE_DEFRAGMENTATION

	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("StartLocalEffectsUpdate", 1.0f);
		// Signal the particle update thread to continue
		g_ptFxManager.StartLocalEffectsUpdate();
	}

	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("fwAnimDirector", 1.0f);
		fwAnimDirector::UpdateClass();
	}
	
	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("Water::FlipBuffers", 1.0f);
		Water::FlipBuffers();
	}

	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("CPlantMgrWrapper", 1.0f);
		CPlantMgrWrapper::UpdateSafeMode();
	}

	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("OcclusionQueries", 1.0f);
		OcclusionQueries::GatherQueryResults();
		PuddlePassSingleton::GetInstance().GetQueriesResults();
		CLensFlare::ProcessQuery();
	}

	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("bgGlassManager", 1.0f);
		// Glass 
		bgGlassManager::ProcessDeferredDeletes();
	
		bgDrawable::EndFrame();
	}

	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("SwapBuffers", 1.0f);
		// Streaming (debug stuff)
		CStreamingDebug::SwapBuffers();
	
		if( !rage::g_PauseClothSimulation )
		{
			if( !fwTimer::IsGamePaused() )
			{
				grmGeometryQB::SwapBuffers();
			}
		#if GTA_REPLAY
			// Replay ties some geometry to the secondary index - see ReplayClothPacketManager::ConnectToRealClothInstances()).
			grmGeometryQB::SwapSecondaryBuffers();
		#endif // GTA_REPLAY
			CPhysics::GetClothManager()->ShiftBufferIndex();
		}
	}

	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("GetRopeManager", 1.0f);
		CPhysics::GetRopeManager()->ProcessShutdownList();
	}

	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("GetClothInstanceTasksManager", 1.0f);
		CPhysics::GetClothInstanceTasksManager()->WaitForTasks();
	}

	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("CScaleformMgr", 1.0f);
		CTextFormat::EndFrame();
		CScaleformMgr::UpdateAtEndOfFrame(true);
	}

	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("CNewHud", 1.0f);
		CNewHud::UpdateAtEndOfFrame();
	}

	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("CMiniMap", 1.0f);
		CMiniMap::UpdateAtEndOfFrame();
	}

	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("CMultiplayerGamerTagHud", 1.0f);
		CMultiplayerGamerTagHud::UpdateAtEndOfFrame();
	}

	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("fwImposedTextureDictionary", 1.0f);
		fwImposedTextureDictionary::RTUpdateCleanup();
	}

	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("MeshBlendManager", 1.0f);
		MeshBlendManager::UpdateAtEndOfFrame();
	}

#if __BANK
	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("camInterface", 1.0f);
		camInterface::UpdateAtEndOfFrame();
	}
#endif // __BANK

	{
		PF_AUTO_PUSH_TIMEBAR_BUDGETED("timeWaited", 1.0f);
		s32 timeWaitedForUpdate = gRenderThreadInterface.GetTimeWaitedForUpdate();
		s32 timeWaitedForGpu = gRenderThreadInterface.GetTimeWaitedForGpu();

		perfClearingHouse::SetValue(perfClearingHouse::WAIT_FOR_UPDATE, MAX(timeWaitedForUpdate - timeWaitedForGpu, 0));
		perfClearingHouse::SetValue(perfClearingHouse::WAIT_FOR_GPU, timeWaitedForGpu);
	
#if RSG_PC
		{
			PF_AUTO_PUSH_TIMEBAR_BUDGETED("CSettingsManager", 1.0f);
			CSettingsManager::GetInstance().SafeUpdate();
		}

		if (GRCDEVICE.UsingMultipleGPUs())
		{
			GRCDEVICE.CopyGPUIndex();
			CPauseMenu::DecrementGPUCountDown();
		}
		else
		{
			CPauseMenu::DecrementGPUCountDown();
		}
#elif RSG_DURANGO
		CPauseMenu::DecrementGPUCountDown();
#endif
	}
    //@@: range CGTARENDERTHREADGAMEINTERFACE_PERFORMSAFEMODEOPERATIONS {
#if TAMPERACTIONS_ENABLED
	TamperActions::PerformSafeModeOperations();
#endif
    //@@: } CGTARENDERTHREADGAMEINTERFACE_PERFORMSAFEMODEOPERATIONS
}

void CGtaRenderThreadGameInterface::RenderThreadInit()
{
	if(!CSpuPmMgr::Init())
	{
		AssertMsg(0, "\n Error creating custom workload!");
	}

#if !__FINAL
	gRenderThreadInterface.SetUpdateBufferThreadAccess(sysThreadType::THREAD_TYPE_UPDATE | SYS_THREAD_AUDIO | SYS_THREAD_PARTICLES);
#endif // !__FINAL
}

void CGtaRenderThreadGameInterface::RenderThreadShutdown()
{
#if SPUPMMGR_ENABLED
	CSpuPmMgr::Shutdown();
#endif // SPUPMMGR_ENABLED
}

void CGtaRenderThreadGameInterface::DefaultPreRenderFunction()
{
	PF_FRAMEINIT_TIMEBARS(gRenderThreadInterface.GetRenderThreadFrameNumber());

	PF_START_TIMEBAR("DebugUpdate");
	CDebug::RenderUpdate();

	CShaderLib::SetGlobalTimer(float(2.0*PI*(double(fwTimer::GetTimeInMilliseconds())/1000.0)));

	CShaderLib::SetGlobalShaderVariables();


#if SPUPMMGR_ENABLED
	PF_START_TIMEBAR("SpuPmMgr update");
	CSpuPmMgr::Update();
#endif // SPUPMMGR_ENABLED
}

void CGtaRenderThreadGameInterface::DefaultPostRenderFunction()
{
	// clear any references to textures from the immediate mode shaders
	CSprite2d::ClearRenderState();
#if RSG_PC && BUGSTAR_INTEGRATION_ACTIVE
	qaBugstarInterface::HandleDelayedBugCreate();
#endif // RSG_PC && BUGSTAR_INTEGRATION_ACTIVE
}

// PURPOSE: this function is run regardless of whether the drawlists are run
void CGtaRenderThreadGameInterface::NonDefaultPostRenderFunction(bool bUpdateDone)
{
	if(bUpdateDone)
		CMiniMap_Common::RenderUpdate();
}

#if __WIN32
// PURPOSE: Flushes the Scaleform stuff that needs to be flushed even when the game/windows is locked
void CGtaRenderThreadGameInterface::FlushScaleformBuffers(void)
{
#if RSG_PC && RAGE_TIMEBARS
	::rage::g_pfTB.SetDisableMoreTimeBars(true);
#endif
	CScaleformComplexObjectMgr::SyncBuffers();
	CScaleformMgr::BeginFrame();
	CScaleformMgr::EndFrame();
#if RSG_PC && RAGE_TIMEBARS
	::rage::g_pfTB.SetDisableMoreTimeBars(false);
#endif
}
#endif

void CGtaRenderThreadGameInterface::RenderFunctionChanged(RenderFn oldFn, RenderFn newFn)
{

	// If render function was the loading thread then enable controls again
	if(oldFn == MakeFunctor(CGtaOldLoadingScreen::LoadingRenderFunction))
	{
		CPed * pPlayer = CGameWorld::FindLocalPlayer();
		if(pPlayer)
			pPlayer->GetPlayerInfo()->EnableControlsLoadingScreen();

		if (CWarningScreen::AllowUnpause())
		{
			fwTimer::EndUserPause();
		}
	}

	// If render function is the loading thread then disable controls again
	if(newFn == MakeFunctor(CGtaOldLoadingScreen::LoadingRenderFunction))
	{
		CPed * pPlayer = CGameWorld::FindLocalPlayer();
		if(pPlayer)
			pPlayer->GetPlayerInfo()->DisableControlsLoadingScreen();
	}

	if (!newFn)	// only operator bool is defined, and NULL isn't always zero
	{
		// reset timers to ensure we always start at 0 next 'Loading...' time
		if (g_fCurrentTimer != 0.0f)
		{
			g_LoadingTimer.Reset();
			g_fCurrentTimer = 0.0f;
		}
	}
}

void CGtaRenderThreadGameInterface::Flush()
{
	bool drawListsDone = gDrawListMgr->IsDrawListCreationDone();
	if (!drawListsDone)
	{
		g_ptFxManager.BeginFrame();
	}

	DRAWLISTMGR->ClothCleanup();
	DRAWLISTMGR->CharClothCleanup();

	CTexLod::FlushMemRemaps();
}



void CGtaOldLoadingScreen::SetLoadingScreenIfRequired()
{
#if !__FINAL
	if (PARAM_debugonfades.Get())
	{
		gRenderThreadInterface.SetDefaultRenderFunction();
	}
	else
#endif
	{

		if( CLoadingScreens::AreActive() )		// If the loading screens are active, then draw them above all else
		{
			gRenderThreadInterface.SetRenderFunction(MakeFunctor(CLoadingScreens::RenderLoadingScreens));
		}
		else if( (m_bForceLoadingScreen || 		// If faded out and credits not running then run faded out renderer
			 ((!CPauseMenu::IsActive() && !CCredits::IsRunning() && !CWarningScreen::IsActive() && !CScriptHud::GetStopLoadingScreen() ) &&  // always display "loading..." if on a black screen and we are accessing the savegame stuff
			 camInterface::IsFadedOut() ) ) )
		{
			CGtaOldLoadingScreen::SetLoadingRenderFunction();
		}
		else
		{
			gRenderThreadInterface.SetDefaultRenderFunction();
		}
	}
}

void CGtaOldLoadingScreen::SetLoadingRenderFunction()
{
	if(gRenderThreadInterface.GetRenderFunction() != MakeFunctor(LoadingRenderFunction))
	{
		g_LoadingTimer.Reset();
		g_fCurrentTimer = 0.0f;
	}

	gRenderThreadInterface.SetRenderFunction(MakeFunctor(LoadingRenderFunction));

#if DEBUG_DRAW
	if ( ::rage::bkManager::IsEnabled() )
	{
		CDebug::Update();  // update the debug here
	}
#endif
}

void CGtaOldLoadingScreen::ForceLoadingRenderFunction(bool bForce)
{
	m_bForceLoadingScreen = bForce;
	if(m_bForceLoadingScreen)
	{
		CLoadingText::SetActive(true);
		SetLoadingRenderFunction();
	}
}


//
// name:		CRenderThreadInterface::LoadingRenderFunction
// description:	Render function that is called whenever screen is faded out
#if __PS3
namespace rage {
extern bool s_NeedWaitFlip;
}
#endif // __PS3

void CGtaOldLoadingScreen::Render()
{
	// This is the actual draw function, do whatever you need to render your frame here
	if( m_depthState == grcStateBlock::DSS_Invalid )
	{ // Lazy init, yurk...
		grcDepthStencilStateDesc desc;
		desc.DepthEnable = false;
		desc.DepthFunc = grcRSV::CMP_LESS;
		m_depthState = grcStateBlock::CreateDepthStencilState(desc);
	}

	grcViewport vp;
	vp.Ortho(0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f); 
	grcViewport::SetCurrent(&vp);

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
	grcStateBlock::SetDepthStencilState(m_depthState);

	grcLightState::SetEnabled(false);
	grcWorldIdentity();

#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	GRCDEVICE.Clear( true, Color32(0,0,0,255), false, 1.0f, false, 0 );
#elif __PS3
	GRCDEVICE.ClearRect(0, 0, vp.GetWidth(), vp.GetHeight(), true, Color32(0,0,0,255),false,1.0f,false,0);
#else //__PS3
	GRCDEVICE.ClearRect(0, 0, vp.GetWidth(), vp.GetHeight(), 0.0f, Color32(0,0,0,255));
#endif //__PS3

	if (CWarningScreen::IsActive())  // we may need to show warning messages on the "loading screens"
	{
		CWarningScreen::Render();
	}
	else
	{
		CScriptHud::DrawScriptSpritesAndRectangles(CRenderTargetMgr::RTI_MainScreen, SCRIPT_GFX_ORDER_AFTER_FADE);
		CScriptHud::DrawScriptText(CRenderTargetMgr::RTI_MainScreen, SCRIPT_GFX_ORDER_AFTER_FADE);

		// If loading render is blank then finish and exit function
		if (!CScriptHud::bHideLoadingAnimThisFrame)
		{
			g_fCurrentTimer += g_LoadingTimer.GetTime();
		}
		else
		{
			g_fCurrentTimer = 0.0f;
			CScriptHud::bHideLoadingAnimThisFrame = false;
		}

		g_LoadingTimer.Reset();

		CLoadingText::UpdateAndRenderLoadingText(g_fCurrentTimer);

#if GTA_REPLAY
		// replay editor requires logos on this fadeout loadingscreen & gamefeed
		if (CVideoEditorUi::IsActive())
		{
			CVideoEditorUi::RenderLoadingScreenOverlay();
		}
#endif // GTA_REPLAY
	}

#if DEBUG_DRAW
#if DEBUG_DISPLAY_BAR
	CDebug::RenderReleaseInfoToScreen();
#endif

#if DEBUG_PAD_INPUT	
	CDebug::RenderDebugScaleformInfo();
#endif
#endif
	FINAL_DISPLAY_BAR_ONLY(CDebugBar::Render());

	// clear any references to textures from the immediate mode shaders
	CSprite2d::ClearRenderState();

	// clear any ropes created and deleted during loading screens ( blame it on the vehicles generation code )
	CPhysics::GetRopeManager()->ProcessShutdownList();
}

void CGtaOldLoadingScreen::LoadingRenderFunction()
{
#if NAVMESH_EXPORT
	if(CNavMeshDataExporter::WillExportCollision())
		return;
#endif

	// add any draw setup required here
	PF_FRAMEINIT_TIMEBARS(0);

#if __WIN32PC
	GRCDEVICE.Manage();
	if (!GRCDEVICE.IsReady())
		return;
#endif

#if __PS3
	s_NeedWaitFlip = true;
	grcTextureFactory::GetInstance().BindDefaultRenderTargets();
#endif //__PS3
	CSystem::BeginRender();

#if GTA_REPLAY
	//add a check so we only do this when prepping a clip to be loaded
	if(CReplayCoordinator::IsSettingUpFirstFrame())
	{
#	if GPU_DAMAGE_WRITE_ENABLED
		CApplyDamage::DrawDamage();
#	endif
		// Prime the water height texture with the correct values for the first frame.
		Water::UpdateDynamicGPU(true);
	}
#endif

	CRenderTargets::LockUIRenderTargets();

	CGtaOldLoadingScreen::Render();

	CRenderTargets::UnlockUIRenderTargets();

	MeshBlendManager::RenderThreadUpdate();

	PostFX::UpdateOnLoadingScreen();

	CSystem::EndRender();
}
