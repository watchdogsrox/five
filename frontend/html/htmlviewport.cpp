//
// filename:	htmlviewport.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------
#include "htmlviewport.h"

// C headers
// Rage headers
#include "file/diskcache.h"
#include "system/platform.h"
#include "system/param.h"

// Game headers
#include "htmldocument.h"
#include "htmlrenderphase.h"
#include "frontend\frontendNY.h"
#include "renderer\RenderPhases\RenderPhase.h"
#include "scene\DataFileMgr.h"

// --- Defines ------------------------------------------------------------------

#define HTML_FORMAT_WIDTH			(640)
#define HTML_FORMAT_HEIGHT			(480)
#define HTML_FORMAT_HIDEF_WIDTH			(640)
#define HTML_FORMAT_HIDEF_HEIGHT		(640)

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

XPARAM(htmlpath);

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------

CViewportHtml::CViewportHtml() : 
CViewport(), 
m_pDocument(NULL),
m_pHtmlRenderPhase(NULL)
{
	CCreateRenderListGroupDC::SetForceSmallBuffer(true);

	if(GRCDEVICE.GetHiDef())  // Hi-Definition
	{
		if (GRCDEVICE.GetWideScreen())  // 16:9
			m_grcViewport.Ortho(-1.0f/6.0f, 7.0f/6.0f, 1.0f, 0.0f, 0.0f, 1.0f);
		else
			 SetUnitOrtho();  // 4:3
	}
	else  // Standard Definition (CRT)
	{
		if (GRCDEVICE.GetWideScreen())  // 16:9
			m_grcViewport.Ortho(-1.0f/6.0f, 7.0f/6.0f, 1.0f, 0.0f, 0.0f, 1.0f);
		else
			SetUnitOrtho();  // 4:3
	}
}

CViewportHtml::~CViewportHtml(){
	if(m_pDocument)
	{
		gRenderThreadInterface.Flush();

		++sysMemIsDeleting;
		delete m_pDocument;
		--sysMemIsDeleting;
		m_pDocument = NULL;
	}

	CFrontEndNY::iHtmlViewportId = 0;
	CCreateRenderListGroupDC::SetForceSmallBuffer(false);
}

void CViewportHtml::CreateRenderPhases(const CRenderSettings* const settings)
{
	CViewport::CreateRenderPhases(settings);
	m_pHtmlRenderPhase = (CRenderPhaseHtml*)m_renderPhaseList.Add(rage_new CRenderPhaseHtml(this));
	m_pHtmlRenderPhase->SetDocument(m_pDocument);
}

/*void ResourceWebPage(const char* pFilename)
{
	char name[32];

	ASSET.BaseName(name, 32, ASSET.FileName(pFilename));
	CHtmlDocument::PushHtmlFolder(pFilename);

	size_t filesize;
	pgRscBuilder::BeginBuild(16000*1024);

	CHtmlDocument* pDoc = rage_new CHtmlDocument;

	CHtmlDocument::EnableParseScriptObjects(false);

	pDoc->LoadDocument(pFilename);

	CHtmlDocument::EnableParseScriptObjects(true);

	pgRscBuilder::SaveBuild(name, "#hm", 1, 0, 0, 0, 0, &filesize);

	delete pDoc;

	pgRscBuilder::EndBuild(pDoc);

	ASSET.PopFolder();

}*/

void CViewportHtml::GetHtmlFilename(char* pDest, const char* pSrc)
{
	char filename[RAGE_MAX_PATH];

	if(!strncmp("http:/", pSrc, 6))
		pSrc += 6;
	// Construct filename
	strcpy(filename, pSrc);
	// add index.html on the end if filename doesn't include .html extension
	s32 len = strlen(filename);
	// find if web address includes extension. If not treat it as a folder and add index.html at the end
	char* pExtension = const_cast<char*>(ASSET.FindExtensionInPath(filename));
	if(pExtension == NULL || (stricmp(pExtension, ".html") && stricmp(pExtension+2, "hm")))
	{
		if(filename[len-1] == '/' ||
			filename[len-1] == '\\')
			strcat(filename, "index.html");
		else
			strcat(filename, "/index.html");
		pExtension = const_cast<char*>(ASSET.FindExtensionInPath(filename));
	}
#if __DEV
	if(!PARAM_htmlpath.Get())
#endif
	{
		pExtension[1] = g_sysPlatform;//'\0';
		pExtension[2] = 'h';
		pExtension[3] = 'm';
		pExtension[4] = '\0';
	}

	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::HTML_FOLDER);

	while(DATAFILEMGR.IsValid(pData))
	{
		sprintf(pDest, "%s/%s", pData->m_filename, filename);
		if(ASSET.Exists(pDest, ""))
			return;
		pData = DATAFILEMGR.GetNextFile(pData);
	}
	
}

void CViewportHtml::LoadWebPage(const char* pFilename)
{
	char fullFilename[RAGE_MAX_PATH];

	strcpy(m_filename, pFilename);

	GetHtmlFilename(fullFilename, pFilename);

/*	if(!ASSET.Exists(m_filename, ""))
	{
		GetHtmlFilename(m_filename, "cannotfind");
	}*/

	if(m_pDocument)
	{
		gRenderThreadInterface.Flush();

		++sysMemIsDeleting;
		delete m_pDocument;
		--sysMemIsDeleting;
		m_pDocument = NULL;
	}

	fiDiskCache::Disable();

#if __DEV
	if(!PARAM_htmlpath.Get())
#endif
	{
		char name[RAGE_MAX_PATH];

		// get base name 
		ASSET.RemoveExtensionFromPath(name, sizeof(name), fullFilename); 
		if (g_strStreamingGlue->MakeSpaceForResourceFile(name, "#hm", 1))
		{
			pgRscBuilder::Load(m_pDocument, name, "#hm", 1);
			Assertf(m_pDocument, "%s:Cannot find file", fullFilename);
		}

	}
#if __DEV
	else
	{
		m_pDocument = rage_new CHtmlDocument;
		m_pDocument->LoadDocument(fullFilename);
	}
#endif

	m_pDocument->PostProcess();

	// if widescreen format as 640x640 (web page is centred)
	if(GRCDEVICE.GetHiDef())
		m_pDocument->FormatDocument(HTML_FORMAT_HIDEF_WIDTH, HTML_FORMAT_HIDEF_HEIGHT);
	else
		m_pDocument->FormatDocument(HTML_FORMAT_WIDTH, HTML_FORMAT_HEIGHT);

	if(m_pHtmlRenderPhase)
		m_pHtmlRenderPhase->SetDocument(m_pDocument);
}

void CViewportHtml::ReloadWebPage()
{
	LoadWebPage(m_filename);
}

