//
// filename:	htmlviewport.h
// description:	
//

#ifndef INC_HTMLVIEWPORT_H_
#define INC_HTMLVIEWPORT_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "file/limits.h"
// Game headers
#include "camera/viewports/Viewport.h"


// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------
class CHtmlDocument;

class CRenderPhaseHtml;

class CViewportHtml : public CViewport
{
public:
	CViewportHtml();
	~CViewportHtml();

	virtual char* GetText() { return "HTML"; }

	virtual void CreateRenderPhases(const CRenderSettings* const settings);
	
	void LoadWebPage(const char* pFilename);
	void ReloadWebPage();
	CHtmlDocument& GetDocument() {return *m_pDocument;}
	CRenderPhaseHtml* GetHtmlRenderPhase() {return m_pHtmlRenderPhase;}

	static void GetHtmlFilename(char* pDest, const char* pSrc);
private:
	char m_filename[RAGE_MAX_PATH];
	CHtmlDocument* m_pDocument;
	CRenderPhaseHtml* m_pHtmlRenderPhase;
};

// --- Globals ------------------------------------------------------------------

#endif // !INC_HTMLVIEWPORT_H_
