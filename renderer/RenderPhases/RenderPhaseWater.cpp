//
// filename:	RenderPhaseWater.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------
#include "RenderPhaseWater.h"
#include "RenderPhaseReflection.h"

// C headers
#include <stdarg.h>

// Rage headers
#include "profile/group.h"
#include "profile/timebars.h"

// Framework Headers
#include "fwScene/scan/Scan.h"

// Game headers
#include "renderer/DrawLists/drawListMgr.h"
#include "renderer/water.h"
#include "Vfx/Sky/Sky.h"
#include "Vfx/GPUParticles/PtFxGPUManager.h"
#include "Vfx/Systems/VfxWeather.h"
#include "Vfx/VfxHelper.h"

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

EXT_PF_GROUP(RenderPhase);
PF_TIMER(CRenderPhaseWaterSurface,RenderPhase);

//***********************************************************************************************************//

// *****************************
// **** WATER SURFACE PHASE ****
// *****************************

CRenderPhaseWaterSurface::CRenderPhaseWaterSurface(CViewport* pGameViewport)
: CRenderPhase(pGameViewport, "Water Surface", DL_RENDERPHASE_WATER_SURFACE, 0.0f, true)
{
}

void CRenderPhaseWaterSurface::BuildDrawList()
{
	DrawListPrologue();

	DLCPushTimebar("Render Water Surface");

	if(Water::WaterEnabled())
	{
#if !__PS3
		DLC_Add(Water::UpdateDynamicGPU, Water::IsHeightSimulationActive());
#endif //!__PS3
		DLC_Add(Water::UpdateWetMap);
	}

	DLCPopTimebar();
	DrawListEpilogue();
}

//***********************************************************************************************************//

// ********************************
// **** CLOUD GENERATION PHASE ****
// ********************************

CRenderPhaseCloudGeneration::CRenderPhaseCloudGeneration(CViewport* pGameViewport)
: CRenderPhase(pGameViewport, "Clouds Generation", DL_RENDERPHASE_CLOUD_GEN, 0.0f, true)
{
}

void CRenderPhaseCloudGeneration::BuildDrawList() 
{ 
	DrawListPrologue();
	DLC_Add(CSky::PreRender);
	DrawListEpilogue();
}

//***********************************************************************************************************//

// ***************************
// **** RAIN UPDATE PHASE ****
// ***************************

CRenderPhaseRainUpdate::CRenderPhaseRainUpdate(CViewport* pGameViewport)
: CRenderPhase(pGameViewport, "Rain Update", DL_RENDERPHASE_RAIN_UPDATE, 0.0f, true)
{
}

void CRenderPhaseRainUpdate::BuildDrawList() 
{ 
	DrawListPrologue();

	// Only update the GPU particles if exterior is visible or camera is under water
	if(fwScan::GetScanResults().GetIsExteriorVisibleInGbuf() || CVfxHelper::GetGameCamWaterDepth() > 0.0f)
	{
		grcViewport* pVP = &GetGrcViewport();
		DLC(dlCmdSetCurrentViewport, (pVP));

		bool resetDropSystem = g_vfxWeather.GetPtFxGPUManager().GetResetDropSystem();
		bool resetMistSystem = g_vfxWeather.GetPtFxGPUManager().GetResetMistSystem();
		bool resetGroundSystem = g_vfxWeather.GetPtFxGPUManager().GetResetGroundSystem();

 		DLC_Add(CPtFxGPUManager::PreRender, fwTimer::IsGamePaused(), resetDropSystem, resetMistSystem, resetGroundSystem);

		g_vfxWeather.GetPtFxGPUManager().ClearResetSystemFlags();
	}

	DrawListEpilogue();
}
