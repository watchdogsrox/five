//
// name:		system.h
// description:	Class grouping all the system control classes
//
#ifndef INC_SYSTEM_H_
#define INC_SYSTEM_H_

#include "system/tls.h"

#include "grcore/setup.h"
#include "system/threadtype.h"

namespace rage
{
class bkBank;
class grmSetup;
}

#define MAX_WORKING_FILES	128

#define SYS_DEFAULT_TASK_THREAD_MASK	(0x2|0x4|0x8|0x20)
#define SYS_DEFAULT_TASK_PRIORITY		PRIO_ABOVE_NORMAL

// bit flags because a thread might be both main and render thread...
enum eThreadId {
	SYS_THREAD_INVALID			= 0,
	SYS_THREAD_UPDATE				= sysThreadType::THREAD_TYPE_UPDATE,
	SYS_THREAD_RENDER			= sysThreadType::THREAD_TYPE_RENDER,
	SYS_THREAD_AUDIO			= BIT0 << sysThreadType::USER_THREAD_TYPE_SHIFT,
	SYS_THREAD_PATHSERVER		= BIT1 << sysThreadType::USER_THREAD_TYPE_SHIFT,
	SYS_THREAD_PARTICLES		= BIT2 << sysThreadType::USER_THREAD_TYPE_SHIFT,
	SYS_THREAD_POSTSCAN			= BIT3 << sysThreadType::USER_THREAD_TYPE_SHIFT,
	SYS_NUM_THREADS
};

class CSystem
{
public:
	static bool Init(const char* pAppName);
	static void Shutdown();
	static void BeginUpdate();
	static void EndUpdate();
	static bool BeginRender();
	static void EndRender();

	static void AddDrawList_BeginRender();
	static void AddDrawList_EndRender();

	static bool WantToExit();
	static bool WantToRestart();
	static bool IsDisplayingSystemUI();
	static grmSetup* GetRageSetup() {return reinterpret_cast<grmSetup*>(rage::grcSetupInstance);}

	static bool	IsThisThreadId(eThreadId currThread);
	static void SetThisThreadId(eThreadId currThread);
	static void ClearThisThreadId(eThreadId currThread);

    static bool InWindow();

	static void InitDLCCommands();

	static void InitAfterConfigLoad();
	
#if (1 == __BANK) && (1 == __XENON)
	static void SetupRingBufferWidgets(bkBank& bank);
#endif

	static void RenderThreadInit();

#if RSG_ORBIS &!__FINAL
	static void StartGPUTraceCapture();
#endif // RSG_ORBIS &!__FINAL

#if !__NO_OUTPUT
	static void RenderBuildInformation();
	static void CenteredPrint(float yPos, const char *string);
#endif // !__NO_OUTPUT

	static bool ms_bDebuggerDetected;
};


// return true if the current thread Id matches the one given
inline bool CSystem::IsThisThreadId(eThreadId currThread) { 

	return( (sysThreadType::GetCurrentThreadType() & currThread) != 0); 
}


#endif // INC_SYSTEM_H_
