// Title	:	Debug.h
// Author	:	G. Speirs
// Started	:	10/08/99

#ifndef INC_DEBUG_H_
#define INC_DEBUG_H_

// Rage Headers
#include "diag/channel.h"
#include "scene/RegdRefTypes.h"
#include "physics/debugPlayback.h"
#include "phcore/constants.h"
#include "profile/rocky.h"
#include "vector/color32.h"

// Game header
#include "debug/bugstar/BugstarIntegrationSwitch.h"
#include "templates/dblBuf.h"
#include "control/replay/ReplaySettings.h"

// macro for marking stuff that needs to be fixed
// put owner's name then comments inside brackets
#define MUST_FIX_THIS(X)
#define NUM_DEBUG_COLOURS	(64)
#define STRING_BUFFER_SIZE (8192)
#define MAX_NUM_STORED_STRINGS (512)


#define DEBUG_CHAR_HEIGHT	11
#define DEBUG_CHAR_WIDTH	9

#define BUG_TYPE_ART 0
#define BUG_TYPE_LEVELS 1

#define ENABLE_CDEBUG		(__BANK)
#define DEBUG_DISPLAY_BAR	(ENABLE_CDEBUG || !__FINAL)
#define DEBUG_PAD_INPUT		(ENABLE_CDEBUG || !__FINAL)
#define DEBUG_SCREENSHOTS	(ENABLE_CDEBUG || !__FINAL)
#define DEBUG_FPS			(ENABLE_CDEBUG || !__FINAL)
#define DEBUG_BANK			(ENABLE_CDEBUG)
#define DEBUG_RENDER_BUG_LIST	(BUGSTAR_INTEGRATION_ACTIVE && ENABLE_CDEBUG)
#define DEBUG_CALL(x)		do {x;if(false&&(rand()%10==0)){x;}} while(0)

#if DEBUG_FPS
#define DEBUG_FPS_ONLY(x)		(x)
#else
#define DEBUG_FPS_ONLY(x)
#endif

class CEntity;
class GameRecorder;

RAGE_DECLARE_CHANNEL(debug) 

#define debugAssert(cond)                    RAGE_ASSERT(debug,cond)
#define debugAssertf(cond,fmt,...)           RAGE_ASSERTF(debug,cond,fmt,##__VA_ARGS__)
#define debugVerifyf(cond,fmt,...)           RAGE_VERIFYF(debug,cond,fmt,##__VA_ARGS__)
#define debugErrorf(fmt,...)					RAGE_ERRORF(debug,fmt,##__VA_ARGS__)
#define debugWarningf(fmt,...)               RAGE_WARNINGF(debug,fmt,##__VA_ARGS__)
#define debugDisplayf(fmt,...)               RAGE_DISPLAYF(debug,fmt,##__VA_ARGS__)
#define debugDebugf1(fmt,...)                RAGE_DEBUGF1(debug,fmt,##__VA_ARGS__)
#define debugDebugf2(fmt,...)                RAGE_DEBUGF2(debug,fmt,##__VA_ARGS__)
#define debugDebugf3(fmt,...)                RAGE_DEBUGF3(debug,fmt,##__VA_ARGS__)

#if __XENON && !__FINAL
#define PGOLITETRACE_DELAY 10000
#endif // __XENON && !__FINAL

namespace rage
{
	class qaBug;
	class netLoggingInterface;
}

class CEmergencyBar
{
public:
	enum
	{
		STATE_NORMAL = 0,
		STATE_STREAMING_EMERGENCY,
		STATE_STREAMING_FRAGMENTATION_EMERGENCY, 
		STATE_SCRIPT_EMERGENCY,
		STATE_SCRIPT_REQUEST_EMERGENCY,

		STATE_TOTAL
	};
	static void Update();
	static inline u32 GetState()					{ return ms_state; }
	static inline void SetState(u32 newState)	{ ms_state=newState; ms_timer=0; }
	static inline Color32& GetBgColor()				{ return ms_aBgColors[GetState()]; }
	static inline Color32& GetTextColor()			{ return ms_aTextColors[GetState()]; }
private:
	static u32 ms_timer;
	static u32 ms_state;
	static Color32 ms_aBgColors[STATE_TOTAL];
	static Color32 ms_aTextColors[STATE_TOTAL];
};


class CTestCase
{
public:
	static void StartTestCase();
	static void StopTestCase();
	static void UpdateTestCase();
	static void SetTestCase();
	static void AddWidgets(bkBank &bank);
	static s32 ms_TestCaseIndex;
	static s32 ms_CurrTestCaseIndex;
};


#if __BANK
// Used to ID objects that are MP only
class	CMPObjectUsage
{
public:

	static void AddWidgets(bkBank &bank);
	static bool	IsActive()	{ return m_bActive; }
	static bool IsUsedInMP(CEntity *pEntity);
	static bool ShouldDisplayOnMap(CEntity *pEntity);

private:
	static	bool	m_bActive;
};

class CBankDebug;
#endif	//__BANK


class CDebug
{
	// General
public:
	static void InitSystem(unsigned initMode);
#if !__FINAL
	static void Init(unsigned initMode);
	static void Update();
#else // !__FINAL
	static void Init(unsigned) {}
	static void Update() {}
#endif // !__FINAL
	static void UpdateLoading();
#if __ASSERT
	static void UpdateNoPopups(bool loading);
#endif

#if ENABLE_CDEBUG	
	static void Init_CDEBUG(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void Update_CDEBUG();
	static void RenderUpdate();
#else
public:
	static void Init_CDEBUG(unsigned UNUSED_PARAM(initMode)) { /* NoOp */ }
	static void Shutdown(unsigned UNUSED_PARAM(shutdownMode)) { /* NoOp */ }
	static void Update_CDEBUG() { /* NoOp */ }
	static void RenderUpdate() { /* NoOp */ }
#endif // ENABLE_CDEBUG

	static void InitProfiler(unsigned initMode);
	
	// Version Number
public:
	static const char* GetRawVersionNumber() { return m_sRawVersionId; }
	static bool SetVersionNumber();
	static const char* GetVersionNumber() {
		if(m_iVersionId[0] == 0)
		{
			SetVersionNumber();
		}
		return &m_iVersionId[0];
	}
#if !__FINAL
	static const char* GetFullVersionNumber() { return &m_sFullVersionId[0]; }
#endif
	static const char* GetOnlineVersionNumber() { return m_sOnlineVersionId;}
	static const char* GetInstallVersionNumber() { return &m_iInstallVersionId[0]; }
	static int GetSavegameVersionNumber() { return m_iSavegameVersionId; }
#if GTA_REPLAY
	static u32 GetReplayVersionNumber() { return m_iReplayVersionId; }
#endif	//GTA_REPLAY
	static const char *GetPackageType();
	static const char *GetFullVersionString(char *outBuffer, size_t bufferSize);

#if USE_PROFILER
	static void RockyCaptureControlCallback(Profiler::ControlState state, u32 mode);
#endif

private:
	static char 	m_iVersionId[32];		//< ie "345-dev_ng"
	static char		m_sRawVersionId[32];	//< ie "345"
	static char		m_sOnlineVersionId[32];
#if !__FINAL
	static char 	m_sFullVersionId[32];	//< ie "345-dev_ng-ob"
#endif
	static char 	m_iInstallVersionId[32];
	static int		m_iSavegameVersionId;
#if GTA_REPLAY
	static u32		m_iReplayVersionId;
#endif	//GTA_REPLAY
	static s32		m_iScaleformMovieId;
public:

	// Debug/display bar
#if DEBUG_DISPLAY_BAR
	static void UpdateReleaseInfoToScreen();
	static void RenderReleaseInfoToScreen();
#endif // DEBUG_DISPLAY_BAR

#if DEBUG_FPS
	static void RenderFpsCounter();
#endif

#if DEBUG_PAD_INPUT
	static void UpdateDebugScaleformInfo();
	static void RenderDebugScaleformInfo();
	static void UpdateDebugPadToScaleform();
	static void UpdateDebugPadMoviePositions();
#endif // #if DEBUG_PAD_INPUT

	static const char* GetCurrentMissionName() { return ms_currentMissionName; }
	static void SetCurrentMissionTag(const char* missionTag);
	static void SetCurrentMissionName(const char* missionName);

	// Screenshots
#if DEBUG_SCREENSHOTS
	static void RequestTakeScreenShot(bool bPng = false);
	static void HandleScreenShotRequests();
#endif // DEBUG_SCREENSHOTS

	// Asserts
#if __ASSERT && ENABLE_CDEBUG
	// Number of seconds since the last assert triggered
	static float GetTimeSinceLastAssert();
#endif // __ASSERT

#if DR_ENABLED
	static bool IsDebugRecorderDisplayEnabled();
#else
	static bool IsDebugRecorderDisplayEnabled() { return false; }
#endif

	// Bugstar
#if BUGSTAR_INTEGRATION_ACTIVE
	static void HandleBugRequests();
	static void RenderTheBugList();
	static bool GetInvincibleBugger()					{ return ms_invincibleBugger;}
	static bool IsTakingScreenshot()					{ return (ms_screenShotFrameDelay > 0); }
	static bool ms_disableSpaceBugScreenshots;
private:
	static void RequestAddBug();
	static void FillOutBugDetails(qaBug& bug);
	static void AddNetLogToBug(const netLoggingInterface& log, qaBug& bug, int& fieldNameDigit);
	static void DumpPedInfo();
	static void AddBug(bool requestAddBug, int bugType, bool requestDebugText, bool forceIgnoreVideoCapture, bool forceIgnoreScreenshot, bool forceSecondScreenshot);
//	static bool ms_requestAddBug;
//	static int ms_bugType;
//	static int ms_forceDebugTextInScreenshot;
//	static bool ms_debugTextForeced;
	static bool ms_requestTakeScreenShot;
	static bool ms_invincibleBugger;
	static bool ms_showInvincibleBugger;
	static u32  ms_screenShotFrameDelay;
#endif // BUGSTAR_INTEGRATION_ACTIVE

	static char ms_currentMissionName[256];
	static char ms_currentMissionTag[256];

#if DR_ENABLED
	static GameRecorder* ms_GameRecorder;
#endif

#if __BANK
public:
	static void DisplayObjectAndPlaceOnGround();

#if RSG_DURANGO
	static bool sm_bDisplayLightSensorQuad;
#endif

private:
	static void JumpToBugCoords();
	static void DumpDebugParams();
	static void SetDebugParams(float *data, u32 numElements);
	static void PopulatePackfileSourceCombo();
	static void PopulateObjectCombo();
	static void PickObjectToSpawn();
	static void ClearPackfileSourceCombo();
	static void ClearObjectComo();
	
	static s32 m_iBugNumber;
#endif // __BANK

	// Miscs
public:
	static bool sm_bPgoLiteTrace;
	static u32 sm_iPgoLiteTraceTime;
	static bool sm_bTraceUpdateThread;
	static bool sm_bStartedUpdateTrace;
	static unsigned int sm_iUpdateTraceCount;
	static bool sm_bTraceRenderThread;
	static bool sm_bStartedRenderTrace;
	static unsigned int sm_iRenderTraceCount;

	static float sm_fWaitForRender;
	static float sm_fWaitForGPU;
	static float sm_fWaitForMain;
	static float sm_fDrawlistQPop;
	static float sm_fEndDraw;
	static float sm_fWaitForDrawSpu;
	static float sm_fFifoStallTimeSpu;

	static bool sm_enableBoundByGPUFlip;

#if __ASSERT
	static int sm_iBlockedPopups;
	static const char* GetNoPopupsMessage(bool loading = false);
#endif // __ASSERT
#if __DEV
	static bool GetDisplaySpawnNodes();
	static bool GetDisplayTimerBars();
#endif // __DEV

#if __BANK
	static void	SetDisplayTimerBars(bool bEnable);
	static void InitWidgets();
	static void InitToolsWidgets();
	static void InitRageDebugChannelWidgets();
	static void AddRageDebugChannelWidgets(bkBank *bank);
	static void AddToolsWidgets(bkBank *bank);
	static void ShutdownRageDebugChannelWidgets();
	static void AddCrashWidgets(bkBank* bank);
	static bkBank* GetLiveEditBank();

	// this fontscale should be used by any debug display in order to have a one-size-fits-all fontscale
	static float sm_dbgFontscale;
#endif // __BANK

	// profiler
	static void UpdateProfiler();

#if ENABLE_CDEBUG
	// profiler
	static void RenderProfiler();

	// debug messages
	static void SetPrinterFn();

	static void DumpEntity(const CEntity* pEntity);
#else // ENABLE_CDEBUG
	// profiler
	static void RenderProfiler() { /* NoOp */ }

	// debug messages
	static void SetPrinterFn() { /* NoOp */ }

	static void DumpEntity(const CEntity*) { /* NoOp */ }
#endif // ENABLE_CDEBUG

#if !__FINAL && TRACK_COLLISION_TYPE_PAIRS
	static void DumpCollisionStatsToNetwork(const char* missionName);
#else
	static void DumpCollisionStatsToNetwork(const char* UNUSED_PARAM(missionName)) { /* NoOp */ }
#endif

#if __WIN32PC
	static void DebugSystemError(const char* pFile, s32 pLine);
#else
	static void DebugSystemError(const char* , s32 ) {}
#endif

#if __ASSERT
	static int OnLogBug(const char* summary, const char* details, u32 assertId, int editBug);
	static bool OnLogBug(const char* summary, const char* details, const char* callstack, u32 assertId, bool editBug);
	static int OnSpuAssert(int spu, const char* summary, const char* assertText, int defaultIfNoServer);
#endif


private:

#if __XENON && !__FINAL
	static void TraceThread(bool& bTrace, bool& bTraceStarted, const char* pFilename, unsigned int& iTraceCount);
#endif // __XENON && !__FINAL	

#if __BANK
	static int sm_token;
	static bkBank* ms_pRageDebugChannelBank;
	static bkBank* ms_pToolsBank;
	static bool sm_debugrespot;
	static bool sm_debugrespawninvincibility;
	static bool sm_debugdoublekill;
	static bool sm_debugexplosion;
	static bool sm_debuglicenseplate;
	static bool sm_debugaircraftcarrier;
	static bool sm_debugradiotest;
	static bool	m_debugParamUsed;
	static CBankDebug* ms_pBankDebug;
#endif // __BANK

};

#endif
