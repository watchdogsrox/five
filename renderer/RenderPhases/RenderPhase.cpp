//**********************************
// RenderPhase.cpp 
// (C) Rockstar North Ltd.
// Started - 11/2005 - Derek Ward
//**********************************
#include "Renderer/RenderPhases/RenderPhase.h"

// Rage headers
#include "fwscene/scan/RenderPhaseCullShape.h"
#include "profile/timebars.h"
#include "grcore/device.h"
// Game hdrs
#include "camera/viewports/Viewport.h"
#include "renderer/DrawLists/drawListMgr.h"
#include "Renderer/RenderSettings.h"
#include "scene/portals/portalTracker.h"
#include "scene/RegdRefTypes.h"
#include "Renderer/clip_stat.h"


// Static class members
//__THREAD	int	ThreadID=0;
//---------------------------------------------------------


CRenderPhase::CRenderPhase(CViewport *viewport, const char *name, int drawListType, float updateBudget, bool disableOnPause)
: fwRenderPhase((viewport) ? &viewport->GetGrcViewport() : NULL, name, drawListType, updateBudget)
, m_Viewport(viewport), m_disableOnPause(disableOnPause)
{

}

#if RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS
CRenderPhase::CRenderPhase(CViewport *viewport, const char *name, int drawListType, float updateBudget, bool disableOnPause, int startDrawListType, int endDrawListType)
: fwRenderPhase((viewport) ? &viewport->GetGrcViewport() : NULL, name, drawListType, updateBudget, startDrawListType, endDrawListType)
, m_Viewport(viewport), m_disableOnPause(disableOnPause)
{

}
#endif // RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS


void CRenderPhase::UpdateViewport()
{
	if (GetViewport())
	{
		SetGrcViewport(GetViewport()->GetGrcViewport());
	}
}


CRenderPhaseScanned::CRenderPhaseScanned(CViewport *viewport, const char *name, int drawListType, float updateBudget)
: CRenderPhase(viewport, name, drawListType, updateBudget, true)
, m_pPortalVisTracker(NULL)
, m_isPrimaryTrackerPhase(false)
, m_isUpdateTrackerPhase(false)
{
}

#if RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS
CRenderPhaseScanned::CRenderPhaseScanned(CViewport *viewport, const char *name, int drawListType, float updateBudget, int startDrawListType, int endDrawListType)
: CRenderPhase(viewport, name, drawListType, updateBudget, true, startDrawListType, endDrawListType)
, m_pPortalVisTracker(NULL)
, m_isPrimaryTrackerPhase(false)
, m_isUpdateTrackerPhase(false)
{
}
#endif //RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS


const grcViewport* CRenderPhaseScanned::GetLodViewport() const
{
	const grcViewport* lodViewport = &GetGrcViewport();

	if (m_Scanner.GetLodViewport())
	{
		lodViewport = m_Scanner.GetLodViewport();
	}

	return lodViewport;
}

void CRenderPhaseScanned::UpdateViewport()
{
	CRenderPhase::UpdateViewport();

	const grcViewport* lodViewport = GetLodViewport();
	const Vec3V vecCameraPosition = lodViewport->GetCameraPosition();
	const Vec3V vecCameraDir = -lodViewport->GetCameraMtx().GetCol2();

	SetCameraForFadeCalcs(RCC_VECTOR3(vecCameraPosition),RCC_VECTOR3(vecCameraDir),lodViewport->GetFarClip());
}

void CRenderPhaseScanned::ConstructRenderList()
{
	const grcViewport* lodViewport = GetLodViewport();
	const Vec3V vecCameraDir = -lodViewport->GetCameraMtx().GetCol2();

	// heading
	const float camHeading = rage::Atan2f(-vecCameraDir.GetXf(), vecCameraDir.GetYf());	
	CRenderer::SetCameraHeading(camHeading);
	CRenderer::SetFarClipPlane(lodViewport->GetFarClip());
}

void CRenderPhaseScanned::SetCullShape(fwRenderPhaseCullShape &cullShape)
{
	// TODO: Move one class up
	spdTransposedPlaneSet8 frustum;
	frustum.Set(GetGrcViewport());
	cullShape.SetFlags(CULL_SHAPE_FLAG_GEOMETRY_FRUSTUM);
	cullShape.SetFrustum(frustum);
}

#if __PPU
///////////////////////////////////////////////////////////////////////////////
// Fences

bank_bool g_bWaitForBubbleFences = true;
static grcFenceHandle s_BubbleFences[CBubbleFences::BFENCE_COUNT];
static bool s_BubbleFencesDisabled[CBubbleFences::BFENCE_COUNT] = {0};

static void InsertBubbleFenceCB(CBubbleFences::kBubbleFence fence)
{
	if (!s_BubbleFencesDisabled[fence])
	{
		s_BubbleFences[fence] = GRCDEVICE.InsertFence();
	}
}
static void WaitForBubbleFenceCB(CBubbleFences::kBubbleFence fence)
{
	grcFenceHandle f = s_BubbleFences[fence];
	if (f)
	{
		// We don't need tight synchronization
		while (GRCDEVICE.IsFencePending(f))
				sys_timer_usleep(250);
	}
}

void CBubbleFences::InsertBubbleFenceDLC(CBubbleFences::kBubbleFence fence)
{
	DLC_Add(InsertBubbleFenceCB, fence);
}

void CBubbleFences::WaitForBubbleFenceDLC(CBubbleFences::kBubbleFence fence)
{
	if (g_bWaitForBubbleFences && s_BubbleFences[fence])
	{
		DLCPushTimebar("Waiting for Bubble Fence");

		DLC_Add(WaitForBubbleFenceCB, fence);

		DLCPopTimebar();
	}
}

#endif // __PPU