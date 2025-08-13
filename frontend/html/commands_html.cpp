//
// filename:	commands_html.cpp
// description:	Script commands for html renderer
//

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "script/wrapper.h"
// Game headers
#include "htmldocument.h"
#include "htmlnode.h"
#include "htmlviewport.h"
#include "htmlrenderphase.h"
#include "htmlrenderer.h"
#include "htmlscriptobject.h"
#include "camera/viewports/ViewportManager.h"
#include "frontend/FrontendNY.h"
#include "script/mission_cleanup.h"
#include "script/script.h"

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------

namespace html_commands
{

	void CreateHtmlViewport(s32& id)
	{
		CViewport *pVp = gVpMan.Add(rage_new CViewportHtml());
		id = pVp->GetId();
		CFrontEndNY::iHtmlViewportId = id;  // we need to store the id of the viewport for the frontend use

		CMissionCleanup::GetInstance()->AddEntityToList(id, CLEANUP_VIEWPORT, CTheScripts::GetCurrentGtaScriptThread(), CLEANUP_SOURCE_SCRIPT);
	}

	void LoadWebPage(s32 id, const char* pFilename)
	{
		CViewport *pVp = gVpMan.FindById(id);
		CViewportHtml *pVpHtml = dynamic_cast<CViewportHtml*>(pVp);

		Assertf(pVpHtml, "LOAD_WEB_PAGE: this is a non HTML viewport");

		pVpHtml->LoadWebPage(pFilename);
	}

	void ReloadWebPage(s32 id)
	{
		CViewport *pVp = gVpMan.FindById(id);
		CViewportHtml *pVpHtml = dynamic_cast<CViewportHtml*>(pVp);

		Assertf(pVpHtml, "RELOAD_WEB_PAGE: this is a non HTML viewport");

		pVpHtml->ReloadWebPage();
	}

	bool DoesWebPageExist(const char* pFilename)
	{
		char filename[RAGE_MAX_PATH];

		CViewportHtml::GetHtmlFilename(filename, pFilename);

		return ASSET.Exists(filename, "");
	}

	float GetWebPageHeight(s32 id)
	{
		CViewport *pVp = gVpMan.FindById(id);
		CViewportHtml *pVpHtml = dynamic_cast<CViewportHtml*>(pVp);
		Assertf(pVpHtml, "GET_WEB_PAGE_HEIGHT: this is a non HTML viewport");

		CHtmlDocument& document = pVpHtml->GetDocument();
		CHtmlElementNode* pBody = document.GetBodyNode();
		Assert(pBody);

		return pBody->GetRenderState().m_height;
	}

	void SetWebPageScroll(s32 id, float scroll)
	{
		CViewport *pVp = gVpMan.FindById(id);
		CViewportHtml *pVpHtml = dynamic_cast<CViewportHtml*>(pVp);
		Assertf(pVpHtml, "SET_WEB_PAGE_SCROLL: this is a non HTML viewport");
		CRenderPhaseHtml* pRenderPhase = pVpHtml->GetHtmlRenderPhase();
		Assert(pRenderPhase);
		
		pRenderPhase->GetRenderer().SetScroll(floorf(scroll));
	}

	s32 GetNumberOfWebPageLinks(s32 id)
	{
		CViewport *pVp = gVpMan.FindById(id);
		CViewportHtml *pVpHtml = dynamic_cast<CViewportHtml*>(pVp);
		Assertf(pVpHtml, "GET_NUMBER_OF_WEB_PAGE_LINKS: this is a non HTML viewport");

		return pVpHtml->GetDocument().GetNumLinks();
	}

	const char* GetWebPageLinkHref(s32 id, s32 index)
	{
		CViewport *pVp = gVpMan.FindById(id);
		CViewportHtml *pVpHtml = dynamic_cast<CViewportHtml*>(pVp);
		Assertf(pVpHtml, "GET_WEB_PAGE_LINK_HREF: this is a non HTML viewport");
		Assertf(index >=0 && index < pVpHtml->GetDocument().GetNumLinks(), "GET_WEB_PAGE_LINK: link index out of range");

		return pVpHtml->GetDocument().GetLinkRef(index);
	}

	void GetWebPageLinkPosn(s32 id, s32 index, float& x, float& y)
	{
		CViewport *pVp = gVpMan.FindById(id);
		CViewportHtml *pVpHtml = dynamic_cast<CViewportHtml*>(pVp);
		Assertf(pVpHtml, "GET_WEB_PAGE_LINK_POSN: this is a non HTML viewport");
		Assertf(index >=0 && index < pVpHtml->GetDocument().GetNumLinks(), "GET_WEB_PAGE_LINK: link index out of range");

		pVpHtml->GetDocument().GetLinkPosition(index, x, y);
		
		float width, height;
		pVpHtml->GetDocument().GetFormattedDimensions(width, height);

		// links are in the formatted dimensions space, not the viewport screen size space
		x *= pVpHtml->GetGrcViewport().GetWidth() / width;
		y *= pVpHtml->GetGrcViewport().GetHeight() / height;
	}

	s32 GetWebPageLinkAtPosn(s32 id, float x, float y)
	{
		CViewport *pVp = gVpMan.FindById(id);
		CViewportHtml *pVpHtml = dynamic_cast<CViewportHtml*>(pVp);
		Assertf(pVpHtml, "GET_WEB_PAGE_LINK_AT_POINT: this is a non HTML viewport");
		CRenderPhaseHtml* pRenderPhase = pVpHtml->GetHtmlRenderPhase();

		float width, height;
		pVpHtml->GetDocument().GetFormattedDimensions(width, height);

		// convert position to -1 to 1 space
		Vector3 posn((x-0.5f)*2.0f,(0.5f-y)*2.0f,0.0f);
		Vector3 outPosn;
		Matrix44 invProj;
		// transform posn by inverse projection
		invProj.Inverse(pVpHtml->GetGrcViewport().GetProjection());
		invProj.Transform(posn, outPosn);
		
		// multiply by html format size
		x = outPosn.x * width;//pVp->GetGrcViewport().GetWidth();
		y = outPosn.y * height;//pVp->GetGrcViewport().GetHeight();
		// add scroll value
		y += pRenderPhase->GetRenderer().GetScroll();
		return pVpHtml->GetDocument().GetLinkAtPosition(x, y);
	}

	void SetWebPageLinkActive(s32 id, s32 index, bool active)
	{
		CViewport *pVp = gVpMan.FindById(id);
		CViewportHtml *pVpHtml = dynamic_cast<CViewportHtml*>(pVp);
		Assertf(pVpHtml, "GET_WEB_PAGE_LINK: this is a non HTML viewport");
		Assertf(index >=0 && index < pVpHtml->GetDocument().GetNumLinks(), "GET_WEB_PAGE_LINK: link index out of range");

		CHtmlDocument& document = pVpHtml->GetDocument();
		document.SetActive(document.GetLink(index), active);
	}

	s32 CreateHtmlScriptObject(const char* pName)
	{
		return gHtmlScriptObjectMgr.Create(pName);
	}

	void DeleteHtmlScriptObject(s32 index)
	{
		return gHtmlScriptObjectMgr.Delete(index);
	}

	void DeleteAllHtmlScriptObjects()
	{
		return gHtmlScriptObjectMgr.DeleteAll();
	}

	void AddToHtmlScriptObject(s32 index, const char* pText)
	{
		gHtmlScriptObjectMgr.AddHtml(index, pText);
	}

	void SetupScriptCommands()
	{
		SCR_REGISTER_UNUSED(CREATE_HTML_VIEWPORT, 0x1b1e5bbc,				CreateHtmlViewport );
		SCR_REGISTER_UNUSED(LOAD_WEB_PAGE, 0xc1058c6,						LoadWebPage );	
		SCR_REGISTER_UNUSED(RELOAD_WEB_PAGE, 0xe9b1b93b,					ReloadWebPage );
		SCR_REGISTER_UNUSED(DOES_WEB_PAGE_EXIST, 0xf52fd85,				DoesWebPageExist );
		SCR_REGISTER_UNUSED(GET_WEB_PAGE_HEIGHT, 0xafdcc2ff,				GetWebPageHeight );
		SCR_REGISTER_UNUSED(SET_WEB_PAGE_SCROLL, 0x52549d0b,				SetWebPageScroll );
		SCR_REGISTER_UNUSED(GET_NUMBER_OF_WEB_PAGE_LINKS, 0x8721c767,		GetNumberOfWebPageLinks );
		SCR_REGISTER_UNUSED(GET_WEB_PAGE_LINK_HREF, 0xab4e18f,			GetWebPageLinkHref );
		SCR_REGISTER_UNUSED(GET_WEB_PAGE_LINK_POSN, 0x75596ea5,			GetWebPageLinkPosn );
		SCR_REGISTER_UNUSED(GET_WEB_PAGE_LINK_AT_POSN, 0x33eb42d0,			GetWebPageLinkAtPosn );
		SCR_REGISTER_UNUSED(SET_WEB_PAGE_LINK_ACTIVE, 0xbe670dc0,			SetWebPageLinkActive );

		SCR_REGISTER_UNUSED(CREATE_HTML_SCRIPT_OBJECT, 0xbb9dd763,			CreateHtmlScriptObject );
		SCR_REGISTER_UNUSED(DELETE_HTML_SCRIPT_OBJECT, 0x6113254c,			DeleteHtmlScriptObject );
		SCR_REGISTER_UNUSED(DELETE_ALL_HTML_SCRIPT_OBJECTS, 0xf54b51a1,	DeleteAllHtmlScriptObjects );
		SCR_REGISTER_UNUSED(ADD_TO_HTML_SCRIPT_OBJECT, 0x1015507,			AddToHtmlScriptObject );
	}

}
