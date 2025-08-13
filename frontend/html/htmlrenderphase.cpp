//
// filename:	htmlrenderphase.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------
#include "htmlrenderphase.h"

// C headers
// Rage headers
#include "grcore/viewport.h"
#include "profile/timebars.h"
// Game headers
#include "htmldocument.h"
#include "htmlrenderphase.h"
#include "camera/viewports/Viewport.h"
#include "Frontend\Hud.h"
#include "System\ControlMgr.h"


// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------

CRenderPhaseHtml::CRenderPhaseHtml(const CViewport* pGameViewport) : CRenderPhase(pGameViewport)
{
	m_sortVal = RPS_RENDER_PRE2D;
	m_drawListType = DL_RENDERPHASE_HTML;
}

//
// name:		CRenderPhaseHtml::SetDocument
// description:	
void CRenderPhaseHtml::SetDocument(CHtmlDocument* pDocument)
{
	float width, height;

	pDocument->GetFormattedDimensions(width, height);

	m_renderer.SetDocument(pDocument);
	m_renderer.SetScreenSize(width, height);

	CHud::SetUniqueDisplay(HUD_UNIQUE_DISPLAY_NONE);  // turn off the HUD
}

CRenderPhaseHtml::~CRenderPhaseHtml()
{
	CHud::SetUniqueDisplay(HUD_UNIQUE_DISPLAY_ALL);  // re-enable the hud
}


void CRenderPhaseHtml::BuildRenderList()
{
}

void CRenderPhaseHtml::UpdateViewport(CViewport* pViewport)
{
	m_grcViewport = pViewport->GetGrcViewport();
}


//----------------------------------------------------
// build and execute the drawlist for this renderphase
void CRenderPhaseHtml::BuildDrawList()											
{ 
	BuildDrawListPrologue();
	
	DLC(CSetCurrentViewportDC(&m_grcViewport));

	m_renderer.Render();

	DLC(CSetCurrentViewportToNULL());

	BuildDrawListEpilogue();
}
