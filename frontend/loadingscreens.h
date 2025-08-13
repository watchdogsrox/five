/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : LoadingScreens.cpp
// PURPOSE : manages the legal and loadingscreens when the game boots
// AUTHOR  : Derek Payne
// STARTED : 02/10/2009
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef INC_LOADINGSCREENS_H_
#define INC_LOADINGSCREENS_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "fwrenderer/renderthread.h"
#include "rline/ros/rlros.h"
#include "system/timer.h"
#include "vector/vector2.h"

// Game headers
#include "debug/BugstarIntegration.h"
#include "frontend/boot_flow/BootupScreen.h"
#include "frontend/CMenuBase.h"
#include "frontend/landing_page/LandingPageConfig.h"
#include "frontend/loadingscreencontext.h"
#include "frontend/PauseMenuData.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/SimpleTimer.h"
#include "renderer/sprite2d.h"
#if __PPU
#include "streaming/streaminginstall.h"
#endif
#include "system/control_mapping.h"


// --- Defines ------------------------------------------------------------------
#define MAX_BUG_SUMMARY_CHARS		(100)
#define MAX_BUG_TAGS_CHARS			(30)
#define MAX_BUG_CLASS_CHARS			(6)			// Upped to 6 to allow the K in TRACK
#define MAX_BUG_DUE_DATE_CHARS		(11)
#define MAX_BUG_QA_OWNER_CHARS		(30)
#define MAX_USERNAME_CHARS			(40)

#define DEFAULT_LOADING_TIME_IN_MILLISECONDS	(100000)  // 1:40 is the "default" loading time if we have never ran before

#define LOADINGSCREEN_LEVEL_GAME -1

// Progress bar tweekable values.
#define LOADINGSCREEN_PROGRESS_RATE_UNDER		1.0f
#define LOADINGSCREEN_PROGRESS_RATE_OVER		3.0f
#define LOADINGSCREEN_PROGRESS_SPEED_MIN		0.01f
#define LOADINGSCREEN_PROGRESS_SPEED_MAX		0.06f
#define LOADINGSCREEN_PROGRESS_INITSESSIONSPEED	0.04f	// This is the important one. The rate at which the progress bar runs from the start of an init session.

#define LOADINGSCREEN_MAX_LEGAL_PAGES			1
#define LOADINGSCREEN_LEGAL_PAGE1_TIME			(15000) // This specifies how long (in ms) the legal spash screen is displayed before switching to the main loading screen.
#define LOADINGSCREEN_LEGAL_PAGE2_TIME			(18000)	// This specifies how long (in ms) the main legal screen is displayed before switching to the game loading screen.

#define LOADINGSCREEN_MAX_INSTALL_STRING		32
#define LOADINGSCREEN_SCALEFORM_BACKOFF			3

#define LOADINGSCREEN_LOADGAME_PROGRESS_SEGMENTS 3.0f

#define LOADINGSCREEN_UPDATES_BEFORE_GAMETIPS	150		// Give the legal screen chance to fade out before starting the gametips...

#define BTN_LABEL_STORY			"LOADING_SPLAYER"		// "Story Mode"
#define BTN_LABEL_ONLINE		"LOADING_MPLAYER"		// "Online"
#define BTN_LABEL_SETTINGS		"IB_SETTINGS"			// "Settings"
#define BTN_LABEL_QUIT			"IB_QUIT"				// "Quit"

#define BTN_LABEL_LOADING_STORY				"LOADING_SPLAYER_L"			// "Loading Story Mode"
#define BTN_LABEL_LOADING_ONLINE			"LOADING_MPLAYER_L"			// "Loading Online"
#define BTN_LABEL_LOADING_INVITED			"LOADING_SPLAYER_INVITED"	// "Invite Accepted
#define BTN_LABEL_JOINING_GAME				"HUD_JOINING"				// "Joining GTA Online"
#define BTN_LABEL_LOADING_SC				"LOADING_SOCIAL_CLUB"		// "Loading Social Club"
#define BTN_LABEL_LOADING_BENCHMARK			"LOADING_BENCHMARK"			// "Loading Benchmark Tests"

// --- Constants ----------------------------------------------------------------

// The main movie (LOADINGSCREEN_STARTUP) contains all the required screens.
// We switch between these screens by passing the values below to the SET_CONTEXT method.
enum LoadingScreenMovieContext
{
	LOADINGSCREEN_SCALEFORM_CTX_BLANK,						    // Blank Screen ( default value in the scaleform movie)
	LOADINGSCREEN_SCALEFORM_CTX_LEGAL,						    // Legal Screen ( With the spinner icons and descriptions )
	LOADINGSCREEN_SCALEFORM_CTX_INSTALL,					    // Installer Screen ( Uses a progress bar )
	LOADINGSCREEN_SCALEFORM_CTX_GAMELOAD,					    // Game loading screen ( Uses a progress bar )
	LOADINGSCREEN_SCALEFORM_CTX_GAMELOAD_NEWS,				    // Game loading (with network connectivity to retrieve rockstar news stories)
	UI_LANDING_PAGE_ONLY(LOADINGSCREEN_SCALEFORM_CTX_LANDING)	// PC & Gen9 Landing Context (with network connectivity to retrieve rockstar news stories, but does not cycle backgrounds automatically)
};

enum LoadingScreenMovieIndex
{
	LOADINGCREEN_MOVIE_INDEX_NONE,
	LOADINGCREEN_MOVIE_INDEX_STARTUP_FINAL,
	LOADINGCREEN_MOVIE_INDEX_STARTUP_DEBUG,
	LOADINGCREEN_MOVIE_INDEX_NEWGAME_FINAL,
	LOADINGCREEN_MOVIE_INDEX_NEWGAME_DEBUG
};

// --- Forward Definitions ------------------------------------------------------
namespace rage
{
	class bwMovie;
	class rlPresenceEvent;
}

// --- Structure/Class Definitions ----------------------------------------------

#if __BANK

// --- CLoadingScreenBugList ---------------------------------------------------------------

struct BugCount
{
	BugCount() : tasks(0), todos(0), a(0), b(0), c(0), d(0), total(0) {}
	int tasks;
	int todos;
	int a;
	int b;
	int c;
	int d;
	int total;

	void UpdateTotal() {total = a + b + c + d + tasks + todos;}
};

struct UserInfoStruct
{
	char username[MAX_USERNAME_CHARS];
	BugCount bugs;
	static bool compare(const UserInfoStruct& a, const UserInfoStruct& b) {return (strcmpi(a.username, b.username) < 0);}
};

struct BugInfoStruct
{
	enum DisplayPriority {
		PT_BUG,
		DISCUSS_BUG,
		MORE_INFO,
		OVERDUE,
		TRACK,				// Added here 'cos I'm not sure what priority TRACKED is (presumably quite high)
		TODO,
		A,
		B,
		C,
		D,
		TASK,
		UNDEFINED
	};

	u32 iBugNum;
	u32	iDaysOpen;
	char cBugSummary[MAX_BUG_SUMMARY_CHARS];
	char cBugTags[MAX_BUG_TAGS_CHARS];
	char cBugClass[MAX_BUG_CLASS_CHARS];
	char cBugDueDate[MAX_BUG_DUE_DATE_CHARS];
	char cBugOwnerQA[MAX_BUG_QA_OWNER_CHARS];
	bool bWithYouForMoreInfo;
	u8 displayPriority;

	static bool SortByDisplayPriority(const BugInfoStruct& a, const BugInfoStruct& b) {return (a.displayPriority < b.displayPriority);}
	static bool SortByDaysOpen(const BugInfoStruct& a, const BugInfoStruct& b) {return (a.iDaysOpen > b.iDaysOpen);}
};

class CLoadingScreenBugList
{
public:

	enum
	{
		TODO_NOW_OPEN_PT,
		TODO_NOW_MORE_INFO,
		TODO_NOW_OVERDUE,
		TODO_NOW_ON_HOLD_TO_YOU,
		TODO_NOW_NO_EST_TIME,
		TODO_NOW_DEFAULT_ASSIGNED,
		TODO_NOW_OPEN_SCREENSHOT_FLAGGED,

		TODO_NOW_MAX
	};

	typedef struct	_TODO_NOW_DATA 
	{
		const char *pTitle;
		const char *pSearchString;
		const char *pFilenameString;
	}	ToDoNowInfoData;

	CLoadingScreenBugList() : m_loaded(false) {}

	const UserInfoStruct& GetUser(int i) const {return m_UserInfo[i];}
	int GetUserCount() const {return m_UserInfo.GetCount();}

	const BugInfoStruct& GetBug(int i) const {return m_BugInfo[i];}
	int GetBugCount() const {return m_BugInfo.GetCount();}


	atArray<BugInfoStruct>	&GetToDoNowBugs(int which)			{ return m_ToDoNow[which]; }
	ToDoNowInfoData			&GetToDoNowInfoData(int which)		{ return m_TodoNowData[which]; }

	atString				&GetToDoNowTAGS(int which)			{ return m_ToDoNowTAGS[which]; }
	atArray< atArray<BugInfoStruct> >	&GetToDoNowTAGBugs()	{ return m_ToDoNowTAGBugs; }

	const char* GetUserName() const {return m_userName;}

	const BugCount& GetTotalBugs() const {return m_totalBugs;}
	bool HasLoaded() {return m_loaded;}

	void Init();
	void LoadBugs();

	void LoadTagsFile();
	void LoadToDoNow();

	void Reset();

	void ResetBugList();
	void ResetToDoNowBugList();

	void UpdateBugList(parTree* pTree);
	void CalculateBugDisplayPriorities();

	void UpdateToDoNowBugList(parTree* pTree, atArray<BugInfoStruct> &theBuglist);

private:
	bool m_loaded;

	char m_userName[MAX_USERNAME_CHARS];
	BugCount m_totalBugs;

	atArray<UserInfoStruct> m_UserInfo;
	atArray<BugInfoStruct> m_BugInfo;

	static ToDoNowInfoData	m_TodoNowData[TODO_NOW_MAX];
	atArray<BugInfoStruct>	m_ToDoNow[TODO_NOW_MAX];

	atArray<atString>		m_ToDoNowTAGS;
	atArray< atArray<BugInfoStruct> >	m_ToDoNowTAGBugs;
};

#endif // __BANK

class CLoadingScreens : public CBootupScreen
{
	friend class CLoadingScreenBugList;
public:
	enum eInstallState
	{
		IS_INSTALL,
		IS_CLOUD,
		IS_INIT_DOWNLOAD,
		IS_DOWNLOAD,
	};

	static void Init(LoadingScreenContext eContext, s32 iLevel);
	static void Shutdown(unsigned shutdownMode);

	static void OnProfileChange(); 

	static void Suspend();
	static void Resume();
	static bool AreSuspended() { return ms_bSuspended; }

	static bool AreActive();
	static bool LoadingScreensPendingShutdown() {return (ms_bMovieNoLongerNeeded);}

	static void SetShutdown(bool s) { ms_bShutdown = s; } ;
	static bool AreShutdown() { return ms_bShutdown; };

	static bool IsDisplayingLegalScreen();
	static void SleepUntilPastLegalScreens();

#if UI_LANDING_PAGE_ENABLED
	static void GoToLanding();
	static bool IsDisplayingLandingScreen(){ return ms_Context == LOADINGSCREEN_CONTEXT_LANDING; }
	static void ExitLanding();
	static void DeviceLost() {}
	static void DeviceReset();
#endif // UI_LANDING_PAGE_ENABLED

	static void LoadLogo();

	static void Render();
	static void RenderLogo();
	static void Update();
	static void KeepAliveUpdate();
	static void RenderLoadingScreens();
	static void TickLoadingProgress( bool bInitSession );
	static void SetInstallerInfo( float fProgress, const char* cString );
	static LoadingScreenContext GetCurrentContext();
	static void CommitToMpSp(const char* labelOverride = NULL);
	static bool IsCommitedToMpSp() { return ms_MpSpCommit; }
	static void CheckInviteMgr();
	static LoadingScreenMovieIndex GetLegalMainMovieIndex();
	static void OnPresenceEvent(const rlPresenceEvent* pEvent);
	static void SetAllowRebootMessage(bool val) { ms_bAllowRebootMessage = val; }

	static bool ShowRebootMessage(const char* label, u32 rebootId);

	static bool IsLandingPageEnabled();
	static bool GetLoadingSP() { return ms_LoadingSP; };
	static void SetLoadingSP(bool loadingSP) { ms_LoadingSP = loadingSP; };

	static bool IsInitialLoadingScreenActive(void) {return ms_initialLoadingScreen;}
	static bool SetStartupFlowPref();

	// Only allow UpdateAtEndOfFrame if m_gameInterface->PerformSafeModeOperations isn't expected to run for a while (i.e. on a CGame::InitSession)
	static void SetAllowScaleformUpdateAtEOF(bool bAllow) { ms_bAllowScaleformUpdateAtEOF = bAllow; }

	static void UpdateDisplayConfig();

	static void SetSocialClubLoadingSpinner();
	static void ShutdownSocialClubLoadingSpinner();

	static void SetScreenOrder(bool bSingleplayer);
	static void SetNewsScreenOrder(bool bSingleplayer);

    static void LoadNewsMovie( s32& inout_newsMovieId );
    static void ShutdownNewsMovie( s32& inout_newsMovieId );
    static void RenderLoadingScreenMovie();
    static void EnableLoadingScreenMovieProgressText();
    static void DisableLoadingScreenMovieProgressText();
    static void ResetButtons();

private:
	static void InitInitialLoadingScreen();

	static void RenderIntroMovie();
	static void RenderLegalText();
	static void LoadLoadingScreenMovie( LoadingScreenMovieIndex iIndex );
	static void UnloadMovie();

	static void LoadNewsMovieInternal();
	static void ShutdownNews();
	static bool DoesCurrentContextAllowNews();
	static bool UpdateGameStreamState();
	static bool CanShowNews();

	static void UpdateLoadingProgressBar();
	static void UpdateInstallProgressBar( float fProgressPercent, const char* cProgressString );

	static void UpdateInput();
	static void CheckStartupFlowPref();
	static bool HasExternalContentLoadRequest();

	static const char* GetMovieNameFromIndex( LoadingScreenMovieIndex iIndex );
	static void UpdateSysTime();
	static void SetLoadingContext(LoadingScreenContext eContext);
	static void SetMovieContext( LoadingScreenMovieContext iMovieContext);
	static bool IsLoadingMovieActive();
	static LoadingScreenMovieIndex GetNewGameMovieIndex();

	static void InitUpdateIntroMovie( bool bInit );
	static void InitUpdateLegalSplash( bool bInit );
	static void InitUpdateLegalMain( bool bInit );
	static void InitUpdateLoadGame( bool bInit );
	static void InitUpdateInstall( bool bInit );
	static void InitUpdateLoadLevel( bool bInit );

	static void ResetProgressCounters( float fSegCount );
	static bool CanUseScaleform();
	static void SetButton( int iIndex, eInstructionButtons iButtonType, const char* stringParam = NULL );
	static void SetButtonLoc( int iIndex, eInstructionButtons iButtonType, const char* locLabel );
	static void SetSpinner( int iIndex, const char* stringParam, bool bIsLocString = true );
#if RSG_ORBIS
	static void UpdateSpinner();
#endif // RSG_ORBIS
	static void SetOriginalRenderFunction();
	static void SetupLegalPage( int Page );
	static void RefreshButtonLayout();

	static void TickGameTips();

	static void SetButtonSwapFlag();
	static bool AcceptIsCross();
	static eInstructionButtons GetAcceptButton();
	
	static bool ms_LoadingSP;
	static bool ms_FinalProgressInitSessionTick;
	static bool ms_bSuspended;
	static bool ms_bShutdown;
	static bool ms_bAllowRebootMessage;
	static bool ms_bSwitchReadyToLoad;
	static bool ms_bMovieNoLongerNeeded;
	static bool ms_SpButtonDown;
	static bool ms_MpButtonDown;
	static bool ms_EventButtonDown;
	static bool ms_RandomJobButtonDown;
#if RSG_ORBIS
    static bool ms_PlusUpsellRequested;
#endif
	static bool ms_MpSpCommit;
	static bool ms_DisplayingJobButton;
	static bool ms_DisplayingEventButton;
    static bool ms_DisplayingOnlineButton;
	static bool ms_bSelectedBootOption;
	static bool ms_SpOnly;
	static bool ms_ToolTipShown;
	static bool ms_SetOnLoadingScreenWhenGsAvailable;
	static bool ms_AcceptIsCross;
	static bool ms_bReadyToInitNews;
	static bool ms_bWaitingForNewsToDisplay;
	static bool ms_bSpMpPrefSet; 
	static bool ms_bAllowScaleformUpdateAtEOF;

	static CSprite2d ms_Logo;

	static bool ms_reInitBink;
	static bwMovie* ms_introMovie;
	static s32  ms_FrameLock;

	static const float LOGO_RAW_TEXTURE_SIZE;
	static const float LOGO_WIDTH;
	static const float LOGO_HEIGHT;
	static const float TEXTURE_BOTTOM_OFFSET;

	static float ms_RecheckedSigninPercent;
	static float ms_FinalProgressPercent;
	static float ms_FinalProgressPercentCmp;
	static float ms_FinalProgressTickInc;
	static float ms_FinalProgressTickMax;
	static float ms_FinalProgressSegCount;
	static float ms_FinalProgressSegCurrent;
	static float ms_fInstallProgress;
	static eInstallState ms_installState;
	static int ms_CountDownBeforeGametips;
	static s32 ms_iLevel;
	static u32 ms_iCurrentLoadingTime;
	static CScaleformMovieWrapper ms_LoadingScreenMovie;
	static u32 ms_iStartLoadingTime;
	static s32 ms_iNewsMovieId;
	static int ms_LineCount;
	static int ms_Mode;
	static int ms_ScaleformBackoffCount;
	static int ms_iInstallRender;
	static u32 ms_PrevSystemTime;

	static int ms_LegalPage;
	static int ms_LegalPageWait[LOADINGSCREEN_MAX_LEGAL_PAGES];
	static LoadingScreenMovieContext ms_LegalPageScaleformId[LOADINGSCREEN_MAX_LEGAL_PAGES];

#if __DEV
	static s32 ms_iBarAlpha;
#endif  // __DEV

	static sysTimer ms_GameTipTimer;
	static sysTimer ms_LoadingTimer;

	static char	*ms_pBinkBits;

	static char ms_cInstallString[LOADINGSCREEN_MAX_INSTALL_STRING];
	static LoadingScreenContext ms_Context;
	static LoadingScreenContext ms_ContextStartup;
	static LoadingScreenMovieIndex ms_CurrentMovieIndex;
	static sysTimer ms_TransitionScreenTimer;
	static char ms_LoadingScreenNameStartupFinal[];
	static char ms_LoadingScreenNameStartupDebug[];
	static char ms_LoadingScreenNameNewGameFinal[];
	static char ms_LoadingScreenNameNewGameDebug[];
	static char ms_LoadingScreenNameInvalid[];
	static RenderFn ms_OriginalRenderFunc;

#if RSG_ORBIS
	static int ms_SpinnerIndex;
	static const char* ms_SpinnerStringParam;
	static bool ms_SpinnerStringNeedsLoc;
	static bool ms_HasExternalContentLoadRequest;
	// The start time the loading screen was shown, not including the legal screens.
	static u32 ms_LoadingPercentageStartTime;
#endif // RSG_ORBIS

#if !__FINAL
	static void NonFinal_UpdateStatusText();
#endif // !__FINAL

#if __BANK
	static void Bank_UpdateBugScreen();
	static void	Bank_AddToDoNowBugs(const char *pTitle, atArray<BugInfoStruct> &theBuglist );
	static void Bank_AddToDoNowTRACKBugs();
	static void Bank_SetupScaleformFromBugstarData();
	static void Bank_LoadTimeFromDataFile();
	static void Bank_SaveTimeToDataFile();
	static void Bank_UpdateSummaryText();

	static s32 ms_iCurrentUser;
	static u32 ms_iPreviousLoadingTime;
	static u32 ms_iLoadingStartScreen;
#endif // __BANK

	static bool ms_initialLoadingScreen; //tells us whether the current loading screen is the initial startup one
};

#endif // !INC_LOADINGSCREENS_H_
