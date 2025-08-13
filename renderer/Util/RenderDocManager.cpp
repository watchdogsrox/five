// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage includes
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/console.h"

// game includes
#include "renderer/Util/RenderDocManager.h"

#if RENDERDOC_ENABLED

PARAM(renderdoc, "Enable RenderDoc frame capturing");
PARAM(renderdoc_overlay, "Enable RenderDoc overlay and F11 to do a capture");
PARAM(renderdoc_log, "Set name of RenderDocCapture");

XPARAM(noVendorAPI);

bool							RenderDocManager::sm_IsEnabled = false;
char							RenderDocManager::sm_logPath[256] = {"renderdoc_capture"};
HINSTANCE						RenderDocManager::sm_RenderDocDLL = NULL;
RENDERDOC_API_1_4_1*			RenderDocManager::sm_pAPI         = NULL;

void RenderDocManager::Init()
{
	if (!PARAM_renderdoc.Get())
	{		
		return;
	}

	sm_RenderDocDLL = ::LoadLibrary("renderdoc.dll");

	if(!sm_RenderDocDLL)
	{
		Warningf("Unable to load renderdoc.dll.");
		return;
	}

	pRENDERDOC_GetAPI pGetAPI = (pRENDERDOC_GetAPI)::GetProcAddress(sm_RenderDocDLL, "RENDERDOC_GetAPI");
	if (!pGetAPI(RENDERDOC_Version::eRENDERDOC_API_Version_1_4_1, (void**)&sm_pAPI)) {
		Warningf("Invalid RenderDoc API version %d!", RENDERDOC_Version::eRENDERDOC_API_Version_1_4_1);
		::FreeLibrary(sm_RenderDocDLL);
		return;
	}

	if (PARAM_renderdoc_log.Get())
	{
		const char* pSavePath = NULL;
		PARAM_renderdoc_log.Get(pSavePath);
		Assertf(strlen(pSavePath) > 4, "Renderdoc logfile is too short (at least 5 characters)!");
		strcpy(&sm_logPath[0], pSavePath);
	}
	
	sm_pAPI->SetLogFilePathTemplate(sm_logPath);

	sm_pAPI->SetCaptureOptionU32(RENDERDOC_CaptureOption::eRENDERDOC_Option_AllowFullscreen, 1);
	sm_pAPI->SetCaptureOptionU32(RENDERDOC_CaptureOption::eRENDERDOC_Option_AllowVSync, 1);

	
	if(PARAM_renderdoc_overlay.Get())
	{
		sm_pAPI->MaskOverlayBits(RENDERDOC_OverlayBits::eRENDERDOC_Overlay_Default, RENDERDOC_OverlayBits::eRENDERDOC_Overlay_Default);

		RENDERDOC_InputButton button = RENDERDOC_InputButton::eRENDERDOC_Key_F11;
		sm_pAPI->SetCaptureKeys(&button, 1);
	}
	else
	{
		sm_pAPI->MaskOverlayBits(RENDERDOC_OverlayBits::eRENDERDOC_Overlay_None, RENDERDOC_OverlayBits::eRENDERDOC_Overlay_None);
		sm_pAPI->SetCaptureKeys(NULL, 0);
	}	
	sm_pAPI->SetFocusToggleKeys(NULL, 0);

	sm_IsEnabled = true;

	AssertMsg(sm_IsEnabled && PARAM_noVendorAPI.Get(), "RenderDoc is enabled but still using vendor API. Replaying a capture done using Vendor API on a different card manufacturer might not be possible. Use -noVendorAPI to disable vendor API.");
}

void RenderDocManager::TriggerCapture()
{
	if(sm_IsEnabled)
	{		
		sm_pAPI->TriggerCapture();
		Displayf("RenderDoc Capture - Done! Capture saved to %s", sm_logPath);
	}
}

void RenderDocManager::Shutdown()
{	
	if(sm_IsEnabled)
	{
		::FreeLibrary(sm_RenderDocDLL);
		sm_RenderDocDLL = NULL;
		sm_IsEnabled = false;
	}	
}

#if __BANK
void RenderDocManager::AddWidgets(bkBank *bk)
{
	if(sm_IsEnabled)
	{
		bk->PushGroup("Renderdoc");
			bk->AddButton("Trigger Capture", RenderDocManager::TriggerCapture);
		bk->PopGroup();
	}	
}
#endif

#endif