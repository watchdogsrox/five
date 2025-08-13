//
// filename:		app.h
// description:		wrapper class for the basic application code
// 

#ifndef INC_APP_H_
#define INC_APP_H_

#include "file/file_config.h"
#include "fwsys/app.h"
#include "profile/stalldetection.h"
#include "system/exception.h"

#if RSG_ORBIS
#include <coredump.h>
#pragma comment(lib,"SceCoredump_stub_weak")
#endif


class CApp : public fwApp
{
public:
	enum
	{
		State_InitSystem = 0,
		State_InitGame,
		State_RunGame,
		State_ShutdownGame,
		State_ShutdownSystem
	};

	virtual fwFsm::Return UpdateState(const s32 state, const s32 iMessage, const fwFsm::Event event);

	fwFsm::Return InitSystem();
	fwFsm::Return InitGame();
	fwFsm::Return RunGame_OnEnter();
	fwFsm::Return RunGame();
	fwFsm::Return RunGame_OnExit();
	fwFsm::Return ShutdownGame();
	fwFsm::Return ShutdownSystem();

	bool WantToExit();
	void CheckDongle();

#if RSG_PC
	static bool IsShutdownConfirmed();
	static bool IsScuiDisabledForShutdown();
	static void CheckExit();
	static void Restart();
	static void WriteExitFile(const char* errorInfo);
	static bool IsGameRunning();
#endif
	static CApp* GetInstance() { return sm_Self; }
	static void  SetInstance(CApp* self) { sm_Self = self; }

#if RSG_LAUNCHER_CHECK
	bool CheckParentProcess();
#endif

#if BACKTRACE_ENABLED && (RSG_PC || RSG_DURANGO)
	static void WriteCrashContextLog(const wchar_t* path);
	static void CollectAdditionalAttributes();
#endif

private:
	pfStallDetection m_StallDetection;

	static CApp* sm_Self;
};


#endif // INC_APP_H_
