//
// filename:	htmlrenderphase.h
// description:	
//

#ifndef INC_HTMLRENDERPHASE_H_
#define INC_HTMLRENDERPHASE_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
// Game headers
//#include "HtmlDocument.h"
#include "HtmlRenderer.h"
//#include "debug/debug.h"
#include "renderer/RenderPhases/renderphase.h"

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

//
// name:		CRenderPhaseHtml
// description:	Render phase rendering html page
class CRenderPhaseHtml : public CRenderPhase
{
public:
	CRenderPhaseHtml(const CViewport* pGameViewport);
	CRenderPhaseHtml();
	~CRenderPhaseHtml();

	void SetDocument(CHtmlDocument* pDocument);

	virtual void BuildRenderList();

	virtual void UpdateViewport(CViewport* );

	virtual void BuildDrawList();

#if __BANK
	virtual const char* GetTypeString() const {return "HTML";}
#endif

	CHtmlRenderer& GetRenderer() {return m_renderer;}

private:
	CHtmlRenderer m_renderer;
};

// --- Globals ------------------------------------------------------------------

#endif // !INC_HTMLRENDERPHASE_H_
