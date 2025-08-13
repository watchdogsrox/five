#ifndef RENDERDOC_MANAGER_H_
#define RENDERDOC_MANAGER_H_

#define RENDERDOC_ENABLED (1 && RSG_PC && !__FINAL)

#if RENDERDOC_ENABLED

#include "renderdoc_app.h"

class RenderDocManager
{
public:	
	static void Init();
	static void TriggerCapture();
	static void Shutdown();

#if __BANK
	static void AddWidgets(bkBank *bk);
#endif

	static bool IsEnabled() { return sm_IsEnabled; }
	static char* GetLogPath() { return sm_logPath; }

private:
	static bool								sm_IsEnabled;
	static char								sm_logPath[256];
	static HINSTANCE						sm_RenderDocDLL;
	static RENDERDOC_API_1_4_1*				sm_pAPI;

};

#endif
#endif