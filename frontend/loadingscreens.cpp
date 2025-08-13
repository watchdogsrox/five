/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : LoadingScreens.cpp
// PURPOSE : manages the legal and loadingscreens when the game boots
// AUTHOR  : Derek Payne
// STARTED : 02/10/2009
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////

#if __PPU
#include <sysutil/sysutil_common.h>
#include <sysutil/sysutil_sysparam.h>
#endif

// C headers
#if __BANK
#include <time.h>
#include <stdio.h>
#include <sys/stat.h>
#endif // __BANK

// Rage headers
#include "audiohardware/driver.h"
#include "bink/movie.h"
#include "string/stringutil.h"
#include "file/device_installer.h"
#include "file/remote.h"
#include "grcore/viewport.h"
#include "grcore/device.h"
#include "grprofile/timebars.h"
#include "math/amath.h"
#include "system/bootmgr.h"
#include "system/timer.h"
#include "system/exec.h"
#include "rline/rlpresence.h"
#include "streaming/streamingallocator.h"
#include "streaming/streamingengine.h"
#include "streaming/streamingrequestlist.h"
#include "streaming/streamingvisualize.h"
#include "streaming/zonedassetmanager.h"


#if RSG_ORBIS
#include "rline/rlnp.h"
#endif

// Framework headers
#include "fwmaths/random.h"
#include "fwnet/netchannel.h"
#include "fwutil/Gen9Settings.h"

// Game headers
#include "audio/frontendaudioentity.h"
#include "audio/northaudioengine.h"
#include "audio/superconductor.h"
#include "camera/viewports/ViewportManager.h"  // for detecting widescreen
#include "control/gamelogic.h"
#include "core/app.h"
#include "text/Text.h"
#include "text/TextConversion.h"
#include "renderer/MeshBlendManager.h"
#include "renderer/renderthread.h"
#include "renderer/rendertargets.h"
#include "loadingscreens.h"
#include "frontend/BinkDefs.h"
#include "frontend/landing_page/LandingPage.h"
#include "frontend/landing_page/LandingPageArbiter.h"
#include "frontend/landing_page/LegacyLandingScreen.h"
#include "frontend/MousePointer.h"
#include "frontend/DisplayCalibration.h"
#include "frontend/NewHud.h"
#include "frontend/PauseMenu.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/ui_channel.h"
#include "frontend/WarningScreen.h"
#include "frontend/GameStreamMgr.h"
#include "frontend/ProfileSettings.h"
#include "frontend/BusySpinner.h"
#include "Network/Sessions/NetworkSession.h"
#include "Network/SocialClubServices/SocialClubCommunityEventMgr.h"
#include "network/SocialClubServices/SocialClubNewsStoryMgr.h"
#include "parser/manager.h"
#include "peds/pedpopulation.h"
#include "SaveLoad/savegame_messages.h"
#include "scene/scene.h"
#include "scene/ExtraContent.h"
#include "script/script.h"
#include "streaming/populationstreaming.h"
#include "streaming/streaming.h"
#include "system/controlMgr.h"
#include "system/EndUserBenchmark.h"
#include "system/pad.h"
#include "system/service.h"
#include "file/asset.h"
#include "Network/Live/livemanager.h"
#include "scene/DownloadableTextureManager.h"
#include "vehicles/vehiclepopulation.h"
#include "vfx/misc/MovieManager.h"
#include "renderer/PostProcessFX.h"

PRE_DECLARE_THREAD_FUNC(LoadingScreenThreadEntryPoint);

#if	!__FINAL
PARAM(bugstarloadingscreen, "[code] show bugstar loadingscreens");
PARAM(nodebugloadingscreen, "[code] disable debug loadingscreens");
#endif

PARAM(bugstarusername, "Bugstar user name used in loading screens");

#if RSG_PC
NOSTRIP_PC_XPARAM(StraightIntoFreemode);
#elif RSG_ORBIS
XPARAM(StraightIntoFreemode);

// 50 seconds.
#define AVERAGE_SP_LOADING_TIME	(1000.0f * 50.0f)
// 1:30
#define AVERAGE_MP_LOADING_TIME (1000.0f * 90.0f)
// 1:56
#define AVERAGE_MP_JOB_LOADING_TIME (1000.0f * 116.0f)
#endif // RSG_ORBIS

#if __BANK
PARAM(noStartupBink,"Skip Bink intro movie (PC PIX has an issue with it right now)");
#endif

GEN9_UI_SIMULATION_ONLY(XPARAM(gen9BootFlow));

#define SKIPPABLE_RENDER (1)

extern bool g_GameRunning;

#define LOGO_TEXTURE_NAME ("gtav_logo")

const float CLoadingScreens::LOGO_RAW_TEXTURE_SIZE = 512.0f;
const float CLoadingScreens::LOGO_WIDTH = (186.0f / 1280.0f);
const float CLoadingScreens::LOGO_HEIGHT = (186.0f / 720.0f);
const float CLoadingScreens::TEXTURE_BOTTOM_OFFSET = (40.0f / LOGO_RAW_TEXTURE_SIZE * LOGO_HEIGHT);

// Bool for checking if SP or MP is being loaded.
bool CLoadingScreens::ms_LoadingSP = true;

// Final loading screen progress bar.
bool CLoadingScreens::ms_FinalProgressInitSessionTick = false;
float CLoadingScreens::ms_FinalProgressPercent = 0.0f;
float CLoadingScreens::ms_FinalProgressPercentCmp = 0.0f;
float CLoadingScreens::ms_FinalProgressTickInc = LOADINGSCREEN_PROGRESS_SPEED_MIN;
float CLoadingScreens::ms_FinalProgressSegCount = 0.0f;		// target.
float CLoadingScreens::ms_FinalProgressSegCurrent = 0.0f;	// Current segment. (Divisions) eg: | 0 --------> | 50 -------> | 100
															//                                  0             1             2

char *CLoadingScreens::ms_pBinkBits;
s32 CLoadingScreens::ms_iLevel = LOADINGSCREEN_LEVEL_GAME;
bool CLoadingScreens::ms_bSuspended = false;
bool CLoadingScreens::ms_bShutdown = false;
bool CLoadingScreens::ms_bAllowRebootMessage = false;
sysTimer CLoadingScreens::ms_LoadingTimer;
u32 CLoadingScreens::ms_iCurrentLoadingTime = 0;
u32 CLoadingScreens::ms_iStartLoadingTime = 0;
bool CLoadingScreens::ms_bMovieNoLongerNeeded = false;
int CLoadingScreens::ms_LineCount = 0;
CScaleformMovieWrapper CLoadingScreens::ms_LoadingScreenMovie;
s32 CLoadingScreens::ms_iNewsMovieId = INVALID_MOVIE_ID;
bool CLoadingScreens::ms_SpButtonDown = true;
bool CLoadingScreens::ms_MpButtonDown = true;
bool CLoadingScreens::ms_EventButtonDown = true;
bool CLoadingScreens::ms_RandomJobButtonDown = true;
#if RSG_ORBIS
bool CLoadingScreens::ms_PlusUpsellRequested = false;
#endif
bool CLoadingScreens::ms_DisplayingEventButton = false;
bool CLoadingScreens::ms_DisplayingJobButton = false;
bool CLoadingScreens::ms_bSelectedBootOption = false;
bool CLoadingScreens::ms_DisplayingOnlineButton = false;
bool CLoadingScreens::ms_MpSpCommit = false;
bool CLoadingScreens::ms_SpOnly = false;
float CLoadingScreens::ms_RecheckedSigninPercent = 0.0f;
u32 CLoadingScreens::ms_PrevSystemTime = 0;

int CLoadingScreens::ms_ScaleformBackoffCount = 0;
float CLoadingScreens::ms_fInstallProgress = 0.0f;
CLoadingScreens::eInstallState CLoadingScreens::ms_installState = CLoadingScreens::IS_INSTALL;
char CLoadingScreens::ms_cInstallString[LOADINGSCREEN_MAX_INSTALL_STRING] = {""};
int CLoadingScreens::ms_iInstallRender = 0;

LoadingScreenContext			CLoadingScreens::ms_Context				= LOADINGSCREEN_CONTEXT_NONE;
LoadingScreenContext			CLoadingScreens::ms_ContextStartup		= LOADINGSCREEN_CONTEXT_NONE;
LoadingScreenMovieIndex			CLoadingScreens::ms_CurrentMovieIndex	= LOADINGCREEN_MOVIE_INDEX_NONE;


sysTimer CLoadingScreens::ms_TransitionScreenTimer;
char CLoadingScreens::ms_LoadingScreenNameStartupFinal[]={"LOADINGSCREEN_STARTUP"};
char CLoadingScreens::ms_LoadingScreenNameStartupDebug[]={"startup"};
char CLoadingScreens::ms_LoadingScreenNameNewGameFinal[]={"LOADINGSCREEN_STARTUP"};	// B*1193927 Using the same movie for newgame.
char CLoadingScreens::ms_LoadingScreenNameNewGameDebug[]={"startup"};
char CLoadingScreens::ms_LoadingScreenNameInvalid[]={"invalid"};

RenderFn CLoadingScreens::ms_OriginalRenderFunc = NULL;

#if RSG_ORBIS
int CLoadingScreens::ms_SpinnerIndex = -1;
const char* CLoadingScreens::ms_SpinnerStringParam = NULL;
bool CLoadingScreens::ms_SpinnerStringNeedsLoc = true;
bool CLoadingScreens::ms_HasExternalContentLoadRequest = false;
u32 CLoadingScreens::ms_LoadingPercentageStartTime = 0u;
#endif // RSG_ORBIS

int CLoadingScreens::ms_LegalPage = 0;
int CLoadingScreens::ms_LegalPageWait[LOADINGSCREEN_MAX_LEGAL_PAGES] = {LOADINGSCREEN_LEGAL_PAGE1_TIME /* ,LOADINGSCREEN_LEGAL_PAGE2_TIME */};
LoadingScreenMovieContext CLoadingScreens::ms_LegalPageScaleformId[LOADINGSCREEN_MAX_LEGAL_PAGES] = {LOADINGSCREEN_SCALEFORM_CTX_LEGAL /* ,LOADINGSCREEN_SCALEFORM_CTX_LEGAL */};

bool CLoadingScreens::ms_ToolTipShown = false;
bool CLoadingScreens::ms_SetOnLoadingScreenWhenGsAvailable = false;
sysTimer CLoadingScreens::ms_GameTipTimer;
bool CLoadingScreens::ms_AcceptIsCross = true;
bool CLoadingScreens::ms_bReadyToInitNews = false;
bool CLoadingScreens::ms_bWaitingForNewsToDisplay = false;
bool CLoadingScreens::ms_bSpMpPrefSet = false;
bool CLoadingScreens::ms_bAllowScaleformUpdateAtEOF = false;

int CLoadingScreens::ms_CountDownBeforeGametips = LOADINGSCREEN_UPDATES_BEFORE_GAMETIPS;

CSprite2d CLoadingScreens::ms_Logo;
bwMovie* CLoadingScreens::ms_introMovie = NULL;
s32 CLoadingScreens::ms_FrameLock = 0xFFFFFFFF;
bool CLoadingScreens::ms_reInitBink = false;

#if __DEV
s32 CLoadingScreens::ms_iBarAlpha = 10;
#endif //__DEV

#if __BANK
#define	NUM_LOADING_START_SCREENS	(6)
u32 CLoadingScreens::ms_iPreviousLoadingTime = DEFAULT_LOADING_TIME_IN_MILLISECONDS;
u32 CLoadingScreens::ms_iLoadingStartScreen = 0;
s32 CLoadingScreens::ms_iCurrentUser = 0;
CLoadingScreenBugList gBugList;
#endif // __BANK

bool CLoadingScreens::ms_initialLoadingScreen = true;

// specific channel for network
RAGE_DEFINE_CHANNEL(net_loading, DIAG_SEVERITY_DEBUG3, DIAG_SEVERITY_WARNING, DIAG_SEVERITY_ASSERT)
#define netLoadingDebug(fmt, ...) RAGE_DEBUGF1(net_loading, fmt, ##__VA_ARGS__)

//OPTIMISATIONS_OFF()

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::InitUpdateIntroMovie()
// PURPOSE:	update the legal splash context (LOADINGSCREEN_CONTEXT_INTRO_MOVIE)
/////////////////////////////////////////////////////////////////////////////////////
#if RSG_PC
const float g_IntroBinkVolume = -10.f;
#elif RSG_XENON
const float g_IntroBinkVolume = -12.f;
#else
const float g_IntroBinkVolume = -6.f;
#endif
#if RSG_ORBIS
sysIpcThreadId s_launchThread = sysIpcThreadIdInvalid;
sysIpcPriority s_launchThreadPrio = PRIO_NORMAL;
#endif

void CLoadingScreens::InitUpdateIntroMovie( bool bInit )
{
	if (bInit)
	{
		Assert(!ms_pBinkBits);

		const char* movieName = "platform:/movies/rockstar_logos";

		fiStream *S = ASSET.Open(movieName, "bik");
		if (S)
		{
			MEM_USE_USERDATA(MEMUSERDATA_BINK);
			int binkSize = S->Size();
			RAGE_TRACK(LogoMovie);
			// This is early enough in the load we can just take it from the streaming heap.
			ms_pBinkBits = (char*) strStreamingEngine::GetAllocator().Allocate(binkSize, 16, MEMTYPE_RESOURCE_VIRTUAL);
			Assert(ms_pBinkBits != NULL);
			S->Read(ms_pBinkBits, binkSize);
			S->Close();

			// Apparently our movies aren't always byte-swapped properly.
			/* // may be just a bink 1 thing, left in case it changes again, but is not required for the latest movies.
			if (ms_pBinkBits[__BE * 3] != 'B') 
			{
				unsigned *u = (unsigned*)ms_pBinkBits;
				unsigned c = binkSize/4;
				while (c--) {
					unsigned x = *u;
					x = (x >> 24) | (x << 24) | ((x >> 8) & 0xFF00) | ((x << 8) & 0xFF0000);
					*u++ = x;
				}
			}
			*/
		}

		bwMovie::bwMovieParams movieParams;
		safecpy(movieParams.pFileName, movieName);
		movieParams.extraFlags = 0;
		movieParams.ioSize = FRONTEND_BINK_READ_AHEAD_BUFFER_SIZE;
		movieParams.preloadedData = ms_pBinkBits;

		ms_introMovie = bwMovie::Create();
#if RSG_PC
		ms_FrameLock = GRCDEVICE.GetFrameLock();
		GRCDEVICE.SetFrameLock(1, false);
#endif // RSG_PC

		if (Verifyf(ms_introMovie, "failed to create bink intro movie!"))
		{
			bool skipIntro=false;
#if __BANK
			if (PARAM_noStartupBink.Get())
				skipIntro=true;
#endif // __BANK
			if (skipIntro || !ms_introMovie->SetMovieSync(movieParams))
			{
				MEM_USE_USERDATA(MEMUSERDATA_BINK);
				strStreamingEngine::GetAllocator().Free(ms_pBinkBits);
				ms_pBinkBits = NULL;

				Assertf(false || skipIntro, "Failed to setup intro movie '%s'", movieName);
				return;
			}

            ms_introMovie->SetLoaded(true);
			ms_introMovie->SetLooping(false);
			//@@: location CLOADINGSCREENS_INITUPDATEINTROMOVIE_SET_VOLUME
			ms_introMovie->SetVolume(g_IntroBinkVolume);
			ms_introMovie->PlaySync();

#if RSG_ORBIS
            s_launchThread = sysIpcGetThreadId();
            s_launchThreadPrio = sysIpcGetCurrentThreadPriority();
            sysIpcSetThreadPriority(s_launchThread, PRIO_LOWEST);
#endif
		}
	}
	else if (ms_introMovie)
	{
		if (!ms_introMovie->IsPlaying())
		{
           SetLoadingContext(LOADINGSCREEN_CONTEXT_NONE);

			if (ms_CurrentMovieIndex == GetLegalMainMovieIndex())
			{
				SetLoadingContext(LOADINGSCREEN_CONTEXT_LEGALMAIN);
				InitUpdateLegalMain(true);
			}

			bwMovie::Destroy(ms_introMovie, false);
			ms_introMovie = NULL;

#if RSG_PC
			Assert(ms_FrameLock >= 0);
			GRCDEVICE.SetFrameLock(ms_FrameLock, true);
			ms_FrameLock = 0xFFFFFFFF;
#endif // RSG_PC


			MEM_USE_USERDATA(MEMUSERDATA_BINK);
			strStreamingEngine::GetAllocator().Free(ms_pBinkBits);
			ms_pBinkBits = NULL;

			ms_reInitBink = true;

#if RSG_ORBIS
            if (s_launchThread != sysIpcThreadIdInvalid)
			{
				sysIpcSetThreadPriority(s_launchThread, s_launchThreadPrio);
                s_launchThread = sysIpcThreadIdInvalid;
			}
#endif
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::InitUpdateLegalSplash()
// PURPOSE:	update the legal splash context (LOADINGSCREEN_CONTEXT_LEGALSPLASH)
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::InitUpdateLegalSplash( bool bInit )
{
	(void)bInit;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::InitUpdateLegalMain()
// PURPOSE:	update the legal main context (LOADINGSCREEN_CONTEXT_LEGALMAIN)
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::InitUpdateLegalMain( bool bInit )
{
	if( bInit )
	{
		ms_LegalPage = 0;
		ms_TransitionScreenTimer.Reset();
		SetupLegalPage( ms_LegalPage );
		ResetProgressCounters(LOADINGSCREEN_LOADGAME_PROGRESS_SEGMENTS);
		
#if __BANK
		Bank_LoadTimeFromDataFile();

		if( PARAM_bugstarloadingscreen.Get() )
		{
			gBugList.Init();
			gBugList.LoadToDoNow();
			gBugList.LoadBugs();		// Make this last as it sets the m_Loaded flag
			if( ms_CurrentMovieIndex == LOADINGCREEN_MOVIE_INDEX_STARTUP_DEBUG )
			{
				if(gBugList.HasLoaded())
				{
					Bank_SetupScaleformFromBugstarData();
					// Set the start screen
					if (ms_LoadingScreenMovie.BeginMethod("GOTO_STATE"))
					{
						CScaleformMgr::AddParamInt(ms_iLoadingStartScreen);
						CScaleformMgr::EndMethod();
					}
				}
				else
				{
					ms_LoadingScreenMovie.CallMethod("NO_BUGSTAR_DATA");
				}
				ms_LoadingScreenMovie.CallMethod("START");
			}
		}
#endif // __BANK
		return;
	}

	if( ms_LegalPage < LOADINGSCREEN_MAX_LEGAL_PAGES )
	{
		if( ms_TransitionScreenTimer.GetMsTime() >= ms_LegalPageWait[ms_LegalPage] )
		{
			ms_TransitionScreenTimer.Reset();
			ms_LegalPage++;
			SetupLegalPage( ms_LegalPage );
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::InitUpdateLoadGame()
// PURPOSE:	update the load game context (LOADINGSCREEN_CONTEXT_LOADGAME)
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::InitUpdateLoadGame( bool bInit )
{
	// in some cases(race condition?), the bink wasn't getting reinitialized(B*1890077), so pulled it out of the if(bInit)
	if (ms_reInitBink && audDriver::GetMixer())
	{
		Displayf("Reinitialising Bink");
		bwMovie::Init();
		ms_reInitBink = false;
	}

	if( bInit )
	{
		UpdateInstallProgressBar( 0.0f, "" ); // Clear the text line.
		// Landing page already commits an action, so don't clear the flag here
        bool const c_landingScreenEnabled = CLandingPageArbiter::ShouldShowLandingPageOnBoot();
		if(!c_landingScreenEnabled)
		{
			ms_MpSpCommit = false;
		}
		ORBIS_ONLY(ms_HasExternalContentLoadRequest = false);
		ms_SpOnly = false;
		ms_DisplayingEventButton = false;
		ms_DisplayingOnlineButton = false;
		ms_bSpMpPrefSet = false;
		ms_FinalProgressPercent = 0.0f;
		ms_RecheckedSigninPercent = 0.0f;

		RefreshButtonLayout();

#if RSG_PC
		if(!c_landingScreenEnabled)
		{
			UpdateDisplayConfig();
		}
#endif

		return;
	}

	// Check the profile setting for booting into SP or MP
	CheckStartupFlowPref();

	if( ms_FinalProgressPercent > ms_RecheckedSigninPercent )
	{
		RefreshButtonLayout();
		ms_RecheckedSigninPercent += 10.0f;
	}

	UpdateLoadingProgressBar();
	UpdateInput();
	CheckInviteMgr();
}

#define LOADINGSCREENS_GAMETIP_CYCLE_TIME	9000 //20000
void CLoadingScreens::TickGameTips()
{
	if( ms_CountDownBeforeGametips > 0 )
	{
		ms_CountDownBeforeGametips--;
		return;
	}

	CGameStream* GameStream = CGameStreamMgr::GetGameStream();
	if( GameStream != NULL )
	{
		if (!GameStream->IsScriptHidingThisFrame())  // dont attempt to render any gametips if script are hiding the feed this frame
		{
			if( ms_GameTipTimer.GetMsTime() > LOADINGSCREENS_GAMETIP_CYCLE_TIME )
			{
				CGameStream::eGameStreamTipType currentTipType;

				// changes for 1626068
				if (CNetwork::GetGoStraightToMultiplayer())  // MP only
				{
					currentTipType = CGameStream::TIP_TYPE_MP;
				}
				else if (CNetwork::CanAccessNetworkFeatures(eNetworkAccessArea::AccessArea_Multiplayer))  // randomly select MP or SP
				{
					if (fwRandom::GetRandomTrueFalse())
					{
						currentTipType = CGameStream::TIP_TYPE_MP;
					}
					else
					{
						currentTipType = CGameStream::TIP_TYPE_SP;
					}
				}
				else  // single player only
				{
					currentTipType = CGameStream::TIP_TYPE_SP;
				}

				if( GameStream->PostGametip(GAMESTREAM_GAMETIP_RANDOM, currentTipType) )
				{
					ms_GameTipTimer.Reset();
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::InitUpdateInstall()
// PURPOSE:	update the install context (LOADINGSCREEN_CONTEXT_INSTALL)
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::InitUpdateInstall( bool bInit )
{
	if( bInit )
	{
		UpdateInstallProgressBar( 0.0f, "" ); // Clear the text line.
		ResetProgressCounters(0.0f);
		ResetButtons();

		return;
	}

	CheckInviteMgr();

	if( CanUseScaleform() )
	{
		UpdateInstallProgressBar( ms_fInstallProgress, ms_cInstallString );
	}
}

void CLoadingScreens::CheckInviteMgr()
{
	if (CLiveManager::GetInviteMgr().HasPendingAcceptedInvite())
	{
		netLoadingDebug("CLoadingScreens::CheckInviteMgr - %s, committing", CLiveManager::GetInviteMgr().GetAcceptedInvite()->IsJVP() ? "Joined" : "Invited");
#if RSG_ORBIS
		CNetwork::SetGoStraightToMultiplayer(true);
        ms_bSelectedBootOption = true;
#else
		CNetwork::SetGoStraightToMultiplayer(false);
#endif
		CommitToMpSp();
	}
}

void CLoadingScreens::OnPresenceEvent( const rlPresenceEvent* pEvent )
{
	if (!pEvent)
		return;

#if !__NO_OUTPUT
	static const char *s_ContextNames[] =
	{
		"LOADINGSCREEN_CONTEXT_NONE",
		"LOADINGSCREEN_CONTEXT_INTRO_MOVIE",
		"LOADINGSCREEN_CONTEXT_LEGALSPLASH",
        "LOADINGSCREEN_CONTEXT_LEGALMAIN",
        "LOADINGSCREEN_CONTEXT_SWAP_UNUSED",
		"LOADINGSCREEN_CONTEXT_LANDING",
		"LOADINGSCREEN_CONTEXT_LOADGAME",
		"LOADINGSCREEN_CONTEXT_INSTALL",
		"LOADINGSCREEN_CONTEXT_LOADLEVEL",
		"LOADINGSCREEN_CONTEXT_MAPCHANGE",
		"LOADINGSCREEN_CONTEXT_LAST_FRAME"
	};

	CompileTimeAssert(COUNTOF(s_ContextNames) == LOADINGSCREEN_CONTEXT_LAST_FRAME+1);

	if (ms_Context >= 0 && ms_Context <= LOADINGSCREEN_CONTEXT_LAST_FRAME)
	{
		netLoadingDebug("OnPresenceEvent :: State: %s", s_ContextNames[ms_Context]);
	}
#endif

	if (ms_Context >= LOADINGSCREEN_CONTEXT_NONE && ms_Context < LOADINGSCREEN_CONTEXT_LAST_FRAME)
	{
		switch(pEvent->GetId())
		{
		case PRESENCE_EVENT_JOINED_VIA_PRESENCE:
			netLoadingDebug("OnPresenceEvent :: Joining, committing");

			if(!CNetwork::IsScriptReadyForEvents())
			{
				netLoadingDebug("OnPresenceEvent :: During boot");
#if RSG_ORBIS
				CNetwork::SetGoStraightToMultiplayer(true);
                ms_bSelectedBootOption = true;
#else
				CNetwork::SetGoStraightToMultiplayer(false);
#endif
			}

			CommitToMpSp(BTN_LABEL_JOINING_GAME);	// Match CLoadingScreens::CheckInviteToMpSp
			break;

		case PRESENCE_EVENT_INVITE_ACCEPTED:
			netLoadingDebug("OnPresenceEvent :: Invited, committing");

			if(!CNetwork::IsScriptReadyForEvents())
			{
				netLoadingDebug("OnPresenceEvent :: During boot");
#if RSG_ORBIS
				CNetwork::SetGoStraightToMultiplayer(true);
                ms_bSelectedBootOption = true;
#else
				CNetwork::SetGoStraightToMultiplayer(false);
#endif
			}

			CommitToMpSp(BTN_LABEL_LOADING_INVITED);	// Match CLoadingScreens::CheckInviteToMpSp
			break;

#if RL_NP_SUPPORT_PLAY_TOGETHER
		case PRESENCE_EVENT_PLAY_TOGETHER_HOST:
			if(!CNetwork::IsScriptReadyForEvents())
			{
				// if we've not got script up yet, launch this via loading
				netLoadingDebug("OnPresenceEvent :: PlayTogether - Committing");
				CNetwork::SetGoStraightToMultiplayer(true);
				CNetwork::SetGoStraightToMPEvent(false);
				CNetwork::SetGoStraightToMPRandomJob(false);
				ms_bSelectedBootOption = true;
				CommitToMpSp(BTN_LABEL_JOINING_GAME);
			}
			break;
#endif

		default:
			break;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::InitUpdateLoadLevel()
// PURPOSE:	update the load level context (LOADINGSCREEN_CONTEXT_LOADLEVEL)
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::InitUpdateLoadLevel( bool bInit )
{
	if( bInit )
	{
		UpdateInstallProgressBar( 0.0f, "" ); // Clear the text line.
		ResetProgressCounters(1.0f);
		return;
	}
	UpdateLoadingProgressBar();
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::Init()
// PURPOSE:	init the loading screens
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::Init(LoadingScreenContext eContext, s32 iLevel)
{
	uiDebugf3("CLoadingScreens::Init -- Context = %d, iLevel =%d", eContext, iLevel);

	if( ms_Context != LOADINGSCREEN_CONTEXT_NONE  )
	{
		// if we're showing the intro movie, setup the next state so we can switch to it when ready
		if (ms_Context == LOADINGSCREEN_CONTEXT_INTRO_MOVIE && eContext == LOADINGSCREEN_CONTEXT_LEGALMAIN)
		{
			InitInitialLoadingScreen();
			LoadLogo();
		}

		return;
    }

	if(eContext != LOADINGSCREEN_CONTEXT_LAST_FRAME)
	{
		g_FrontendAudioEntity.StartLoadingTune();
	}	

	SetButtonSwapFlag();

	ms_SetOnLoadingScreenWhenGsAvailable = true;
	UpdateGameStreamState();
	ms_CountDownBeforeGametips = LOADINGSCREEN_UPDATES_BEFORE_GAMETIPS;
	ms_ToolTipShown = false;
	ms_OriginalRenderFunc = gRenderThreadInterface.GetRenderFunction();

	ms_ContextStartup = eContext;

	// Determine which movie to load (if any) for this context:
	switch( eContext )
	{
        case LOADINGSCREEN_CONTEXT_INTRO_MOVIE:
			ms_CurrentMovieIndex = LOADINGCREEN_MOVIE_INDEX_NONE;
			InitUpdateIntroMovie( true );
            break;

		case LOADINGSCREEN_CONTEXT_LEGALSPLASH:
			ms_CurrentMovieIndex = LOADINGCREEN_MOVIE_INDEX_NONE;
			InitUpdateLegalSplash( true );
			break;

		case LOADINGSCREEN_CONTEXT_LEGALMAIN:
			LoadLogo();
			InitInitialLoadingScreen();
			InitUpdateLegalMain(true);
			break;

		case LOADINGSCREEN_CONTEXT_LOADLEVEL:
 			SCE_ONLY(ms_LoadingPercentageStartTime = 0u);
			LoadLogo();
			ms_CurrentMovieIndex = GetNewGameMovieIndex();
			LoadLoadingScreenMovie( ms_CurrentMovieIndex );
			SetScreenOrder(!(CNetwork::GetGoStraightToMultiplayer() || CLiveManager::GetInviteMgr().HasPendingAcceptedInvite()));
			SetMovieContext( LOADINGSCREEN_SCALEFORM_CTX_GAMELOAD );
			InitUpdateLoadLevel( true );

            if (CNetwork::GetGoStraightToMultiplayer() || CLiveManager::GetInviteMgr().HasPendingAcceptedInvite())
			{
				netLoadingDebug("Init :: LOADINGSCREEN_CONTEXT_LOADLEVEL - GetGoStraightToMultiplayer :: %s, Invited: %s", 
								 CNetwork::GetGoStraightToMultiplayer() ? "True" : "False",
								 CLiveManager::GetInviteMgr().HasPendingAcceptedInvite() ? "True" : "False");

				ms_MpSpCommit = false;
				CommitToMpSp();
			}
			else
			{
				// Ensure buttons are cleared in case load states are changing and getting crossed up
				ResetButtons();
				// Show Story Mode Spinner
				SetSpinner(0, BTN_LABEL_LOADING_STORY);
			}

			break;
		case LOADINGSCREEN_CONTEXT_LANDING:
#if UI_LANDING_PAGE_ENABLED
			ms_CurrentMovieIndex = GetNewGameMovieIndex();
			LoadLoadingScreenMovie(ms_CurrentMovieIndex);
			GoToLanding();
#endif // UI_LANDING_PAGE_ENABLED
			break;
		case LOADINGSCREEN_CONTEXT_LAST_FRAME:
			{
				sysTimer failSafe;
				const char* label =  GetLoadingSP() ? BTN_LABEL_LOADING_STORY : BTN_LABEL_LOADING_ONLINE;

			#if !RSG_DURANGO
				CPauseMenu::TogglePauseRenderPhases(false, OWNER_LOADING_SCR, __FUNCTION__ );
			#endif
				CBusySpinner::Preload(true);
				CBusySpinner::On(TheText.Get(label), BUSYSPINNER_LOADING, SPINNER_SOURCE_LOADING_SCREEN);

				// Give the warning screen a chance to shutdown
				while (CWarningScreen::IsActive())
					CWarningScreen::Update();

				// Make sure the spinner is active
				while (!CBusySpinner::IsActive() && failSafe.GetTime() <= 3.0f)
				{
					CBusySpinner::SetInstantUpdate(true);
					CBusySpinner::Update();
				}

				ResetButtons();
				ms_CurrentMovieIndex = LOADINGCREEN_MOVIE_INDEX_NONE;
			}
			break;

		case LOADINGSCREEN_CONTEXT_MAPCHANGE:
		{
			ORBIS_ONLY(ms_LoadingPercentageStartTime = 0u);
			LoadLogo();
			ms_CurrentMovieIndex = GetNewGameMovieIndex();
			LoadLoadingScreenMovie( ms_CurrentMovieIndex );
			SetScreenOrder(GetLoadingSP());
			SetMovieContext( LOADINGSCREEN_SCALEFORM_CTX_GAMELOAD );
			InitUpdateLoadLevel( true );

			// Ensure buttons are cleared in case load states are changing and getting crossed up
			ResetButtons();

			// Show Map Change Spinner
			const char* label =  GetLoadingSP() ? BTN_LABEL_LOADING_STORY : BTN_LABEL_LOADING_ONLINE;

#if !RSG_DURANGO 
			SetButtonLoc(0, ICON_SPINNER, label);
#else 
			SetButtonLoc(1, ICON_SPINNER, label);
#endif // RSG_DURANGO

			break;
		}

		default:
			uiAssertf( false, "CLoadingScreens::Init -- Bad context: %d", eContext);
			ms_CurrentMovieIndex = LOADINGCREEN_MOVIE_INDEX_NONE;
			break;
	}

	ms_bMovieNoLongerNeeded = false;
		
	SetLoadingContext(eContext);
	ms_iLevel = iLevel;
	
	// Extra setup...

#if __DEV
	ms_iBarAlpha = 10;
#endif  // _DEV

	ms_iCurrentLoadingTime = 0;

	ms_LoadingTimer.Reset();
	ms_GameTipTimer.Reset();
	ms_iStartLoadingTime = ms_LoadingTimer.GetSystemMsTime();
		
	ms_TransitionScreenTimer.Reset();

	gRenderThreadInterface.SetRenderFunction(MakeFunctor(RenderLoadingScreens));

	// flush the render thread to ensure it picks up the new render function
	gRenderThreadInterface.Flush(true);
}

void CLoadingScreens::LoadLogo()
{
	if(!ms_Logo.HasTexture())
	{
		strLocalIndex iTxdId = strLocalIndex(g_TxdStore.FindSlot(CHudTools::GetFrontendTXDPath()));

		if (iTxdId != -1)
		{
			g_TxdStore.StreamingRequest(iTxdId, (STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD | STRFLAG_DONTDELETE));
			CStreaming::SetDoNotDefrag(iTxdId, g_TxdStore.GetStreamingModuleId());
			CStreaming::LoadAllRequestedObjects(true);

			if (g_TxdStore.HasObjectLoaded(iTxdId))
			{
				g_TxdStore.AddRef(iTxdId, REF_OTHER);
				g_TxdStore.PushCurrentTxd();
				g_TxdStore.SetCurrentTxd(iTxdId);
				if (Verifyf(fwTxd::GetCurrent(), "Current Texture Dictionary (id %u) is NULL ",iTxdId.Get()))
				{
					ms_Logo.SetTexture(LOGO_TEXTURE_NAME);
				}

				g_TxdStore.PopCurrentTxd();
			}
		}
	}
}

void CLoadingScreens::InitInitialLoadingScreen()
{
	ms_CurrentMovieIndex = GetLegalMainMovieIndex();

	if(!CHudColour::IsInitialized())
	{
		CHudColour::SetValuesFromDataFile();  // Ensure HudColours is initialized
	}

	LoadLoadingScreenMovie( ms_CurrentMovieIndex );
}
	

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::Shutdown()
// PURPOSE:	shutdown the loading screens
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::Shutdown(unsigned /*shutdownMode*/)
{
	uiDebugf3("CLoadingScreens::Shutdown -- Startup Context = %d, Current Context = %d", ms_Context, ms_ContextStartup);

	if( ms_Context == LOADINGSCREEN_CONTEXT_NONE  )
	{
		return;
	}

	if( ms_Context == LOADINGSCREEN_CONTEXT_LOADGAME 
		|| ms_Context == LOADINGSCREEN_CONTEXT_LOADLEVEL
		|| ms_Context == LOADINGSCREEN_CONTEXT_MAPCHANGE
		|| ms_Context == LOADINGSCREEN_CONTEXT_LAST_FRAME )
	{
		ms_initialLoadingScreen = false;
	}

	// this flags the movie as no longer needed, so once the loadingscreens are finished
	// with, and the game has moved on, the movie is deleted.
	if( IsLoadingMovieActive() )
	{
		ms_bMovieNoLongerNeeded = true;
	}

	g_FrontendAudioEntity.StopLoadingTune();

	// Check and handle any shutdown requirements for the original _startup_ context only.
	switch( ms_ContextStartup )
	{
        case LOADINGSCREEN_CONTEXT_INTRO_MOVIE:
		case LOADINGSCREEN_CONTEXT_LEGALSPLASH:
			SetOriginalRenderFunction();
			break;

		case LOADINGSCREEN_CONTEXT_LEGALMAIN:
			CGtaOldLoadingScreen::SetLoadingRenderFunction();
			gRenderThreadInterface.Flush();

#if __BANK
			gBugList.Reset();
			Bank_SaveTimeToDataFile();
#endif
			break;

		case LOADINGSCREEN_CONTEXT_LAST_FRAME:
			{
				CPauseMenu::TogglePauseRenderPhases(true, OWNER_LOADING_SCR, __FUNCTION__ );
				CBusySpinner::Off(SPINNER_SOURCE_LOADING_SCREEN);

				if (CWarningScreen::IsActive())  // fixes 1593426 - remove the warningscreen before we call SetLoadingRenderFunction
					SetOriginalRenderFunction();
				else
					CGtaOldLoadingScreen::SetLoadingRenderFunction();

				gRenderThreadInterface.Flush();
			}
			break;

		case LOADINGSCREEN_CONTEXT_LOADLEVEL:
		case LOADINGSCREEN_CONTEXT_MAPCHANGE:
			if (CWarningScreen::IsActive())  // fixes 1593426 - remove the warningscreen before we call SetLoadingRenderFunction
			{
				SetOriginalRenderFunction();
			}
			else
			{
				CGtaOldLoadingScreen::SetLoadingRenderFunction();
			}

			gRenderThreadInterface.Flush();
			break;

		default:
		break;
	}

	ms_Logo.Delete();

	ms_SetOnLoadingScreenWhenGsAvailable = false;
	CGameStream* GameStream = CGameStreamMgr::GetGameStream();
	if( GameStream != NULL )
	{
		GameStream->SetIsOnLoadingScreen(false);

		//Flush out the tips which may be on screen as well as any potential news. We don't want any gamestream message to go from the loading screen to the game.
		GameStream->FlushQueue();
	}

	// Shutdown the news feed
	ShutdownNews();

	// don't simulate player positioned for default gta5 level, or streaming can start too early while the 
	// player is positioned at the origin. script will position the player when they're ready
	if (CGameLogic::GetRequestedLevelIndex() != CGameSessionStateMachine::DEFAULT_START_LEVEL)
		CStreaming::SetIsPlayerPositioned(true);

	CLoadingText::SetActive(true);

	SetLoadingContext(LOADINGSCREEN_CONTEXT_NONE);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::OnProfileChange()
// PURPOSE:	handle a profile change
////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::OnProfileChange()
{
	const LoadingScreenContext context =
#if RSG_GEN9
		LOADINGSCREEN_CONTEXT_SIGNOUT;
#else
		LOADINGSCREEN_CONTEXT_LOADLEVEL;
#endif

	if(!CLoadingScreens::AreActive())
		CLoadingScreens::Init(context, 0);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::SetupLegalPage()
// PURPOSE:	Tells scaleform to display a specific legal page and performs
//          extra setup steps for each page type.
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::SetupLegalPage( int Page )
{
	if( Page < LOADINGSCREEN_MAX_LEGAL_PAGES && ms_CurrentMovieIndex == LOADINGCREEN_MOVIE_INDEX_STARTUP_FINAL )
	{
		SetMovieContext( ms_LegalPageScaleformId[Page] );

		// extra setup for the pages here:
		switch( ms_LegalPageScaleformId[Page] )
		{
		case LOADINGSCREEN_SCALEFORM_CTX_LEGAL:
			if( ms_LoadingScreenMovie.BeginMethod("LEGAL") )
			{
				CScaleformMgr::AddParamString( "LEGAL_MAIN_DISK_USAGE" );
				CScaleformMgr::AddParamString( "LEGAL_MAIN_CLOUD_USAGE" );
				CScaleformMgr::AddParamString( "LEGAL_MAIN_LOADING_USAGE" );

				// QA Build Version
				char buildVers[64] = {0};
				formatf(buildVers, "%s %s", TheText.Get("GAME_BUILD"), CDebug::GetRawVersionNumber());
				CScaleformMgr::AddParamString( buildVers );

				// Online Version
				char netVers[64] = {0};
				formatf(netVers, "%s %s", TheText.Get("ONLINE_BUILD"), CDebug::GetOnlineVersionNumber());
				CScaleformMgr::AddParamString( netVers ); 

				CScaleformMgr::EndMethod();
			}
			break;

		default:
			break;
		}
	}
}

void CLoadingScreens::SetOriginalRenderFunction()
{	
	gRenderThreadInterface.SetRenderFunction(ms_OriginalRenderFunc);
	gRenderThreadInterface.Flush();
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CommitToMpSp
// PURPOSE:	locks out any further changes to the sp/mp switch.
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::CommitToMpSp(const char* labelOverride)
{
	if( ms_MpSpCommit )
	{
		return;
	}

	ResetButtons();

#if !__NO_OUTPUT
	bool bJVP = CLiveManager::GetInviteMgr().HasPendingAcceptedInvite() && CLiveManager::GetInviteMgr().GetAcceptedInvite()->IsJVP();

	netLoadingDebug("CommitToMpSp :: GetGoStraightToMultiplayer: %s, GetGoStraightToMPRandomJob : %s, GetGoStraightToMPEvent : %s, Invited: %s, Joined: %s", 
					 CNetwork::GetGoStraightToMultiplayer() ? "True" : "False",
					 CNetwork::GetGoStraightToMPRandomJob() ? "True" : "False",
					 CNetwork::GetGoStraightToMPEvent() ? "True" : "False",
					 CLiveManager::GetInviteMgr().HasPendingAcceptedInvite() && !bJVP ? "True" : "False",
					 bJVP ? "True" : "False");
#endif

	if(labelOverride)
	{
		SetSpinner( 0, labelOverride );
	}
	else if(CNetwork::GetGoStraightToMPEvent() && CNetwork::GetGoStraightToMultiplayer())
	{
		SetSpinner( 2, GetEventButtonText(true), false );
	}
	else if(CNetwork::GetGoStraightToMPRandomJob() && CNetwork::GetGoStraightToMultiplayer())
	{
		SetSpinner( 2, GetJobButtonText(true), false );
	}
	else if(CLiveManager::GetInviteMgr().HasPendingAcceptedInvite())
	{
		// JVP == Joined via Presence
		SetSpinner( 0, CLiveManager::GetInviteMgr().GetAcceptedInvite()->IsJVP() ? BTN_LABEL_JOINING_GAME : BTN_LABEL_LOADING_INVITED );
	}
	else if(CNetwork::GetGoStraightToMultiplayer())
	{
		SetSpinner( 1, BTN_LABEL_LOADING_ONLINE );
	}
#if COLLECT_END_USER_BENCHMARKS
	else if(EndUserBenchmarks::GetWasStartedFromPauseMenu(FE_MENU_VERSION_LANDING_MENU))
	{
		SetSpinner( 0, BTN_LABEL_LOADING_BENCHMARK);
	}
#endif
	else
	{
		SetSpinner( 0, BTN_LABEL_LOADING_STORY );
	}
	ms_MpSpCommit = true;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::RefreshButtonLayout()
// PURPOSE:	Updates button layout based on the player's sign-in state.
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::RefreshButtonLayout()
{
	if( ms_MpSpCommit )
	{
		return;
	}


    // Clear Buttons
    ResetButtons();
    
	// Making the assumption that an external content load request can only force you into online
	if(HasExternalContentLoadRequest())
	{
		// Force this to always go straight to MP
		netLoadingDebug("RefreshButtonLayout :: Straight to Multiplayer - HasExternalContentLoadRequest");
		CNetwork::SetGoStraightToMultiplayer(true);
		SetSpinner( 0, BTN_LABEL_LOADING_ONLINE);
	}
	else
	{  
        if(CanSelectOnline())
		{
#if RSG_ORBIS || RSG_PC
			static bool firstMPButtonRefresh = true;
			if(firstMPButtonRefresh && PARAM_StraightIntoFreemode.Get())
			{
				netLoadingDebug("RefreshButtonLayout :: Straight to Multiplayer - Command line StraightIntoFreemode");
				CNetwork::SetGoStraightToMultiplayer(true);
				CNetwork::SetGoStraightToMPEvent(false);
				CNetwork::SetGoStraightToMPRandomJob(false);
				firstMPButtonRefresh = false;
			}
#endif

			const bool bShouldShowJobBtn = SocialClubEventMgr::Get().GetDataReceived();
			const bool bShouldShowEventBtn = SocialClubEventMgr::Get().HasFeaturedEvent() ORBIS_ONLY(|| ShouldShowPlusUpsell());

			if(CNetwork::GetGoStraightToMultiplayer())
			{
				SetButtonLoc(0, GetAcceptButton(), BTN_LABEL_STORY);

				if(CNetwork::GetGoStraightToMPEvent()) // loading event
				{
					SetButtonLoc( 1, BUTTON_X, BTN_LABEL_ONLINE );
					if (bShouldShowJobBtn) SetButton( 2, BUTTON_R1, GetJobButtonText(false) );
					if (bShouldShowEventBtn) SetSpinner( 3, GetEventButtonText(true), false );
				}
				else if(CNetwork::GetGoStraightToMPRandomJob()) // loading random job
				{
					SetButtonLoc( 1, BUTTON_X, BTN_LABEL_ONLINE );
					if (bShouldShowJobBtn) SetSpinner( 2, GetJobButtonText(true), false );
					if (bShouldShowEventBtn) SetButton( 3, BUTTON_Y, GetEventButtonText(false) );
				}
				else // loading freeplay online
				{
					SetSpinner( 1, BTN_LABEL_LOADING_ONLINE );
					if (bShouldShowJobBtn) SetButton( 2, BUTTON_R1, GetJobButtonText(false) );
					if (bShouldShowEventBtn) SetButton( 3, BUTTON_Y, GetEventButtonText(false) );
				}
			}
#if RSG_ORBIS
            else if(ms_PlusUpsellRequested)
            {
                SetButtonLoc(0, GetAcceptButton(), BTN_LABEL_STORY);
                SetButtonLoc(1, BUTTON_X, BTN_LABEL_ONLINE);
                if(bShouldShowJobBtn) SetButton(2, BUTTON_R1, GetJobButtonText(false));
                if(bShouldShowEventBtn) SetSpinner(3, GetEventButtonText(true), false);
            }
#endif
			else // loading story
			{
				netLoadingDebug("RefreshButtonLayout :: Show MP Button (Job: %s, Event: %s)", bShouldShowJobBtn ? "True" : "False", bShouldShowEventBtn ? "True" : "False");
				SetSpinner( 0, BTN_LABEL_LOADING_STORY );
				SetButtonLoc( 1, BUTTON_X, BTN_LABEL_ONLINE );
				if (bShouldShowJobBtn) SetButton( 2, BUTTON_R1, GetJobButtonText(false) );
				if (bShouldShowEventBtn) SetButton( 3, BUTTON_Y, GetEventButtonText(false) );
			}

			ms_SpOnly = false;
		}
		else
		{
#if !__NO_OUTPUT
			if (CNetwork::GetGoStraightToMultiplayer())
			{
				netLoadingDebug("RefreshButtonLayout :: Hiding MP Buttons and forcing SP");
				if (!NetworkInterface::IsLocalPlayerOnline())
				{
					netLoadingDebug("RefreshButtonLayout :: REASON: IsLocalPlayerOnline() returned false.");
				}
				else if (!NetworkInterface::CheckOnlinePrivileges())
				{
					netLoadingDebug("RefreshButtonLayout :: REASON: CheckOnlinePrivileges() returned false.");
				}
				else if (!HasCompletedPrologue())
				{
					netLoadingDebug("RefreshButtonLayout :: REASON: HasCompletedPrologue() returned false.");
				}
				else if (CLiveManager::GetInviteMgr().HasPendingAcceptedInvite())
				{
					netLoadingDebug("RefreshButtonLayout :: REASON: CLiveManager::GetInviteMgr().HasPendingAcceptedInvite() returned true.");
				}
				else
				{
					netLoadingDebug("RefreshButtonLayout :: REASON: Unknown. Update CLoadingScreens::RefreshMultiplayerButton.");
				}
			}
#endif
			netLoadingDebug("RefreshButtonLayout :: Cannot Access Multiplayer");

#if RSG_ORBIS
			// this can be shown in SP loading (bump to index 1 here but use the same button as MP loading)
			const bool bShouldShowPlusBtn = ShouldShowPlusUpsell();
			if(bShouldShowPlusBtn && ms_PlusUpsellRequested)
			{
				netLoadingDebug("RefreshButtonLayout :: SinglePlayer - Showing Plus Upsell Loading");
				SetSpinner(1, GetEventButtonText(true), false);
			}
			else if(bShouldShowPlusBtn && !ms_PlusUpsellRequested)
			{
				netLoadingDebug("RefreshButtonLayout :: SinglePlayer - Showing Plus Promotion");
				SetButton(1, BUTTON_Y, GetEventButtonText(false));
			}
#endif

			// we can only load story
			SetSpinner(0, BTN_LABEL_LOADING_STORY);
				
            if(!ms_bSelectedBootOption)
            {
                CNetwork::SetGoStraightToMultiplayer(false);
            }
			ms_SpOnly = true;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	 GetAcceptButton
// PURPOSE:	Returns the correct eInstructionButton which represents the "Accept" button
/////////////////////////////////////////////////////////////////////////////////////
eInstructionButtons CLoadingScreens::GetAcceptButton()
{
	return AcceptIsCross() ? BUTTON_A : BUTTON_B;
}

LoadingScreenContext CLoadingScreens::GetCurrentContext()
{
	return( ms_Context );
}

void CLoadingScreens::SetInstallerInfo( float fProgress, const char* cString )
{
	ms_iInstallRender = 5;
	ms_fInstallProgress = fProgress;
	ms_installState = IS_INSTALL;

	safecpy(ms_cInstallString, cString);
}

void CLoadingScreens::ResetProgressCounters( float fSegCount )
{
	ms_FinalProgressInitSessionTick = false;
	ms_FinalProgressSegCount = fSegCount;
	ms_FinalProgressPercent = 0.0f;
	ms_FinalProgressPercentCmp = 0.0f;
	ms_FinalProgressTickInc = LOADINGSCREEN_PROGRESS_SPEED_MIN;
	ms_FinalProgressSegCurrent = 0.0f;
	ms_fInstallProgress = 0.0f;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CanUseScaleform
// PURPOSE:	don't hit scaleform every frame
/////////////////////////////////////////////////////////////////////////////////////
bool CLoadingScreens::CanUseScaleform()
{
	ms_ScaleformBackoffCount++;
	if( ms_ScaleformBackoffCount >= LOADINGSCREEN_SCALEFORM_BACKOFF )
	{
		ms_ScaleformBackoffCount = 0;
		return( true );
	}
	return( false );
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	RenderLoadingScreens
// PURPOSE:	call to the main update and render functions
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::RenderLoadingScreens()
{
	PF_FRAMEINIT_TIMEBARS(0);
	CLoadingScreens::Update();

#if RSG_PC
	CShaderLib::SetStereoTexture();
	CShaderLib::SetReticuleDistTexture(false);
#endif
	CLoadingScreens::Render();
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::Update()
// PURPOSE:	The main update function of the loading screens
//			This runs on the RENDER thread!
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::Update()
{
	//
	// update the actual loadingscreens that are active:
	//
	
	// CControlMgr is updated on the update thread for the landing page
	// CControlMgr is updated on the update thread for the entitlement states
	if(ms_Context != LOADINGSCREEN_CONTEXT_NONE 
		UI_LANDING_PAGE_ONLY(&& ms_Context != LOADINGSCREEN_CONTEXT_LANDING)
		WIN32PC_ONLY(&& !CGame::IsEntitlementUpdating()))
	{
		if(!ms_bSuspended)
			CControlMgr::Update();
	}

	switch( ms_Context )
	{
		case LOADINGSCREEN_CONTEXT_NONE:
			if (ms_bMovieNoLongerNeeded)
			{
				ConductorMessageData messageData;
				messageData.conductorName = SuperConductor;
				messageData.message = UnmuteGameWorld;
				SUPERCONDUCTOR.SendConductorMessage(messageData);
				UnloadMovie();
			}
			break;

		case LOADINGSCREEN_CONTEXT_INTRO_MOVIE:
			UpdateSysTime();
			InitUpdateIntroMovie( false );
			break;

		case LOADINGSCREEN_CONTEXT_LEGALSPLASH:
			InitUpdateLegalSplash( false );
			break;

		case LOADINGSCREEN_CONTEXT_LEGALMAIN:
			if( ms_CurrentMovieIndex == LOADINGCREEN_MOVIE_INDEX_STARTUP_FINAL )
			{
#if RSG_PC
				if (GRCDEVICE.GetRequestPauseMenu())
				{
					ms_TransitionScreenTimer.Reset();
					GRCDEVICE.SetRequestPauseMenu(false);
				}
#endif
				if( ms_LegalPage >= LOADINGSCREEN_MAX_LEGAL_PAGES)
				{
#if UI_LANDING_PAGE_ENABLED
					if(CLandingPageArbiter::ShouldShowLandingPageOnBoot())
					{
						GoToLanding();
					}
					else
#endif // UI_LANDING_PAGE_ENABLED
					{
						SetLoadingContext(LOADINGSCREEN_CONTEXT_LOADGAME);
						SetScreenOrder(GetLoadingSP());
						SetMovieContext(LOADINGSCREEN_SCALEFORM_CTX_GAMELOAD);
						InitUpdateLoadGame(true);
					}
				}
				else
				{
					InitUpdateLegalMain(false);
				}
			}
			else
			{
				SetLoadingContext(LOADINGSCREEN_CONTEXT_LOADGAME);	// skip forward immediately if we're not using the final loading screen
				SetScreenOrder(GetLoadingSP());
				SetMovieContext(LOADINGSCREEN_SCALEFORM_CTX_GAMELOAD);
				InitUpdateLoadGame(true);
			}
			UpdateSysTime();
			break;

		case LOADINGSCREEN_CONTEXT_LOADGAME:
			UpdateSysTime();
			if( ms_iInstallRender > 0 )
			{
				SetLoadingContext(LOADINGSCREEN_CONTEXT_INSTALL);
				SetScreenOrder(GetLoadingSP());
				SetMovieContext( LOADINGSCREEN_SCALEFORM_CTX_INSTALL );
				InitUpdateInstall( true );
			}
			else
			{
				if( CNetwork::GetGoStraightToMultiplayer() && ms_MpSpCommit )
				{
					ms_FinalProgressSegCount = LOADINGSCREEN_LOADGAME_PROGRESS_SEGMENTS + 1.0f; // Add an extra segment for loading multiplayer.
				}
				
				InitUpdateLoadGame(	false );
			}
#if !__FINAL
			if( !PARAM_nodebugloadingscreen.Get() )
			{
				NonFinal_UpdateStatusText();
			}
#endif //!__FINAL
			break;

		case LOADINGSCREEN_CONTEXT_INSTALL:
			UpdateSysTime();
			if( ms_iInstallRender == 0 )
			{
				SetLoadingContext(LOADINGSCREEN_CONTEXT_LOADGAME);
				SetScreenOrder(GetLoadingSP());
				SetMovieContext( LOADINGSCREEN_SCALEFORM_CTX_GAMELOAD );
				InitUpdateLoadGame( true );
			}
			else
			{
				InitUpdateInstall(false);
			}
			break;

		case LOADINGSCREEN_CONTEXT_LOADLEVEL:
		case LOADINGSCREEN_CONTEXT_MAPCHANGE:
		case LOADINGSCREEN_CONTEXT_LAST_FRAME:
#if RSG_ORBIS
			UpdateSysTime();
#endif // RSG_ORBIS

			InitUpdateLoadLevel(false);
			break;
#if UI_LANDING_PAGE_ENABLED
		case LOADINGSCREEN_CONTEXT_LANDING:
			UpdateSysTime();
#if GEN9_LANDING_PAGE_ENABLED
			if (!CLandingPage::IsActive() && 
			    (CGameSessionStateMachine::GetGameSessionState() == CGameSessionStateMachine::GameSessionState::AUTOLOAD_SAVE_GAME ||
				 CGameSessionStateMachine::GetGameSessionState() == CGameSessionStateMachine::GameSessionState::LOAD_LEVEL))
			{
				ExitLanding();
			}
#endif
			break;
#endif // UI_LANDING_PAGE_ENABLED
		default:
			uiAssertf(false,"CLoadingScreens::Update -- Bad context: %d", ms_Context);
			break;
	}

	// Set the feed in the correct mode when it becomes available.
	if( ms_SetOnLoadingScreenWhenGsAvailable && UpdateGameStreamState())
	{
		ms_SetOnLoadingScreenWhenGsAvailable = false;
	}

	if( ms_iInstallRender > 0 )
	{
		ms_iInstallRender--;
	}

#if __BANK
	if( PARAM_bugstarloadingscreen.Get() )
	{
		if(ms_CurrentMovieIndex == LOADINGCREEN_MOVIE_INDEX_STARTUP_DEBUG )
		{
			Bank_UpdateBugScreen();
		}
	}
#endif
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::KeepAliveUpdate()
// PURPOSE: special update which needs to called from the main thread.
//          ( for CScaleformMgr::UpdateAtEndOfFrame() )
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::KeepAliveUpdate()
{
	if( !AreActive())
	{
		return;
	}

	if( DoesCurrentContextAllowNews() )
	{
		if(ms_bReadyToInitNews && CanShowNews())
		{
			ms_bReadyToInitNews = false;

			LoadNewsMovieInternal();

			// Initialize news if we're ready, our movie is active, and the news manager has stories to show
			CNetworkTransitionNewsController::Get().InitControl( ms_iNewsMovieId, CNetworkTransitionNewsController::MODE_CODE_LOADING_SCREEN );

			ms_bWaitingForNewsToDisplay = true;
		}

		if(CScaleformMgr::IsMovieActive(ms_iNewsMovieId) && ms_bWaitingForNewsToDisplay && CNetworkTransitionNewsController::Get().IsDisplayingNews())
		{
			ms_bWaitingForNewsToDisplay = false;

#if UI_LANDING_PAGE_ENABLED
			if(ms_Context != LOADINGSCREEN_CONTEXT_LANDING)
#endif // UI_LANDING_PAGE_ENABLED
			{
				// Wait until news is actually displaying before we change the loading screen movie context to fade out
				SetScreenOrder(GetLoadingSP());
				SetNewsScreenOrder(GetLoadingSP());
				SetMovieContext(LOADINGSCREEN_SCALEFORM_CTX_GAMELOAD_NEWS);		
			}

			// Stop the game stream (feed) if we're displaying news
			CGameStream* pGameStream = CGameStreamMgr::GetGameStream();
			if(pGameStream)
			{
				pGameStream->AutoPostGameTipOff();
				pGameStream->FlushQueue();
			}
		}
	}

	DOWNLOADABLETEXTUREMGR.Update();

	// Only allow UpdateAtEndOfFrame if renderThread::Synchronise isn't expected to run for a while (i.e. on a CGame::InitSession)
	if (!g_GameRunning || ms_bAllowScaleformUpdateAtEOF)
	{
		CScaleformMgr::UpdateAtEndOfFrame(false);  // specific call in order for movies to update on the UT during the loadingscreens
	}
#if EC_CLOUD_MANIFEST
	EXTRACONTENT.UpdateCloudStorage();
#endif

	strStreamingEngine::GetLoader().LoadRequestedObjects();		// Pump the streaming system to issue new requests
	strStreamingEngine::GetLoader().ProcessAsyncFiles();
}

bool CLoadingScreens::UpdateGameStreamState()
{
	CGameStream* GameStream = CGameStreamMgr::GetGameStream();
	if( GameStream != NULL )
	{
		GameStream->SetIsOnLoadingScreen(true);
		return true;
	}

	return false;
}

bool CLoadingScreens::CanShowNews()
{
	return CLiveManager::IsOnline() && CNetworkNewsStoryMgr::Get().GetNumStories(NEWS_TYPE_TRANSITION_HASH) > 0;
}

bool CLoadingScreens::DoesCurrentContextAllowNews()
{
    // On Gen9 we don't have news during loading screens, and we need to ensure we never launch it otherwise
    // it may interfere with our usage of the news movie (since GTAV doesn't instance movie views)
#if IS_GEN9_PLATFORM
     bool const c_allowNews = false;
#elif GEN9_LANDING_PAGE_ENABLED
    bool const c_allowNews = !PARAM_gen9BootFlow.Get();
#else
    bool const c_allowNews = true;
#endif
	return c_allowNews && ( ms_Context == LOADINGSCREEN_CONTEXT_LOADGAME 
			WIN32PC_ONLY( || ( ms_Context == LOADINGSCREEN_CONTEXT_LANDING && 
                ( !SLegacyLandingScreen::IsInstantiated() || SLegacyLandingScreen::GetInstance().DoesCurrentContextAllowNews() ) ) ) );
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::Render()
// PURPOSE:	the main render function of the loading screens
/////////////////////////////////////////////////////////////////////////////////////
#if __PS3
namespace rage {
extern bool s_NeedWaitFlip;
}
#endif // __PS3

void CLoadingScreens::Render()
{
	LoadingScreenContext currContext = ms_Context;

	if( currContext == LOADINGSCREEN_CONTEXT_NONE  )
	{
		return;
	}

#if __PS3
	s_NeedWaitFlip = true;
#endif // __PS3

	if (SKIPPABLE_RENDER && (!RSG_DURANGO || currContext != LOADINGSCREEN_CONTEXT_INTRO_MOVIE))
	{
#if RSG_PC
		const float fDesiredFrameRate = 60;
#else
		const float fDesiredFrameRate = 30;
#endif
		const float fTimeToRenderLoadScreenMS = 1.0f;
		const float fDesiredWait = ((1000.0f / fDesiredFrameRate) - fTimeToRenderLoadScreenMS);

 		static sysTimer oSkipTimer;
		if (oSkipTimer.GetMsTime() < fDesiredWait)
			return;

		oSkipTimer.Reset();
	}

	CSystem::BeginRender();
	{
		SYS_CS_SYNC(CScaleformMgr::GetRealSafeZoneToken()); // The load screen doesn't respect normal save zones, so we have to fake it

		// Blit the last frame to the screen
		if (currContext == LOADINGSCREEN_CONTEXT_LAST_FRAME)
		{
			static grcViewport vp = *grcViewport::GetDefaultScreen();	// take a copy as PostFX will modify the current viewport
			
		#if RSG_PC
			if (GRCDEVICE.UsingMultipleGPUs())
			{
				if (PostFX::GetPauseResolve() == PostFX::RESOLVE_PAUSE)
				{
					CRenderTargets::ResolveForMultiGPU(PostFX::RESOLVE_PAUSE);
					PostFX::DoPauseResolve(PostFX::IN_RESOLVE_PAUSE);
					PostFX::ResetDOFRenderTargets();
				}
				else if (PostFX::GetPauseResolve() == PostFX::IN_RESOLVE_PAUSE)
				{
					CRenderTargets::ResolveForMultiGPU(PostFX::IN_RESOLVE_PAUSE);
				}
			}
		#endif
			grcViewport::SetCurrent(&vp);
			PostFX::ProcessNonDepthFX();
		}

		CRenderTargets::LockUIRenderTargets();
		const grcViewport* pVp = grcViewport::GetDefaultScreen();

		// Don't clear here, we blitted that buffer already!
			if (currContext != LOADINGSCREEN_CONTEXT_LAST_FRAME)
			GRCDEVICE.Clear(true, Color32(0,0,0), false, 0, false, 0);

		grcViewport::SetCurrent(pVp);
		grcStateBlock::Default();

		switch( currContext )
		{
        	case LOADINGSCREEN_CONTEXT_INTRO_MOVIE:
           		RenderIntroMovie();
           	 	break;

			case LOADINGSCREEN_CONTEXT_LEGALSPLASH:
				RenderLegalText();
				break;

			case LOADINGSCREEN_CONTEXT_LEGALMAIN:
				UpdateSysTime();
				break;
#if UI_LANDING_PAGE_ENABLED
			case LOADINGSCREEN_CONTEXT_LANDING:
				UpdateSysTime();
				break;
#endif // UI_LANDING_PAGE_ENABLED
			case LOADINGSCREEN_CONTEXT_LOADGAME:
				UpdateSysTime();
#if RSG_ORBIS
				CheckInviteMgr();
				UpdateSpinner();
#endif // RSG_ORBIS
				UpdateLoadingProgressBar();
				UpdateInput();
				break;

			case LOADINGSCREEN_CONTEXT_INSTALL:
				UpdateSysTime();
				break;

			case LOADINGSCREEN_CONTEXT_LOADLEVEL:
				ORBIS_ONLY(UpdateSpinner();)
				break;

			case LOADINGSCREEN_CONTEXT_MAPCHANGE:
				break;

			case LOADINGSCREEN_CONTEXT_LAST_FRAME:
				break;

			default:
				uiAssertf(false,"Bad context: 0x%08x", currContext);
				break;
		}

		// USE ORTHO VIEWPORT FOR NORMAL UI RENDERING
		if(gVpMan.PrimaryOrthoViewportExists())
		{
			grcViewport::SetCurrent(&gVpMan.GetPrimaryOrthoViewport()->GetGrcViewport());
		}

		// Only render loading screen scaleform movies if loading screens aren't suspended
		if(!AreSuspended())
		{
			if (currContext != LOADINGSCREEN_CONTEXT_LAST_FRAME)
			{
				// Render Main Loading Screen Movie
                if ( currContext != LOADINGSCREEN_CONTEXT_INTRO_MOVIE )
                {
                    RenderLoadingScreenMovie();
				}
			}
			else
			{
				CBusySpinner::Render();
			}

			bool bRenderGameStream = !CPauseMenu::IsActive();

			// Render News
			if( CNetworkTransitionNewsController::Get().IsDisplayingNews() )
			{
				if(CScaleformMgr::IsMovieActive(ms_iNewsMovieId))
				{
					CScaleformMgr::RenderMovie( ms_iNewsMovieId, 0.0f, true );
				}
				bRenderGameStream = false;
			}
			
			if(!IsDisplayingLegalScreen() && bRenderGameStream)
			{
				// Render Game Stream (If we're not showing news, and pause menu isn't active)
				TickGameTips();

				// This stuff is also messing with Scaleform so needs to be inside this fake save zone
				// Update/Render Gamestream Movie
				CGameStreamMgr::Update();
				CGameStreamMgr::Render();
			}
		}

		// Render Landing Overlay
#if RSG_PC
		if(currContext == LOADINGSCREEN_CONTEXT_LANDING && SLegacyLandingScreen::IsInstantiated())
		{
			// Render the pause menu ourselves when viewing landing settings
			SLegacyLandingScreen::GetInstance().Render();
		}
#endif

		if(currContext > LOADINGSCREEN_CONTEXT_LEGALMAIN)
		{
			RenderLogo();
		}

		// Render Warning/Display Calibration
		if (CDisplayCalibration::IsActive())
		{
			CDisplayCalibration::Render();
		}
		else
		{
			if (CWarningScreen::CanRender())
			{
				CWarningScreen::Render();  // render any warning screen
			}
		}

		// Render Mouse
		KEYBOARD_MOUSE_ONLY( CMousePointer::Render(); )

		grcViewport::SetCurrent(pVp);

		CRenderTargets::UnlockUIRenderTargets();
		PS3_ONLY(CRenderTargets::Rescale();)

		MeshBlendManager::RenderThreadUpdate();
	}
	CSystem::EndRender();
}

void CLoadingScreens::RenderLogo()
{
	if(ms_Logo.HasTexture())
	{
		grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
		grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
		grcStateBlock::SetBlendState(CHudTools::GetStandardSpriteBlendState());

		ms_Logo.SetRenderState();


		// Safe Zone
		float fSafeZoneX[2], fSafeZoneY[2];

#if RSG_DURANGO
		// Fake safe zone on durango since movies on the loading screen all need to be adjusted to the same fake values
		if(CLoadingScreens::IsInitialLoadingScreenActive())
		{
			CHudTools::GetFakeLoadingScreenSafeZone(fSafeZoneX[0], fSafeZoneY[0], fSafeZoneX[1], fSafeZoneY[1]);
		}
		else
#endif
		{
			CHudTools::GetMinSafeZone(fSafeZoneX[0], fSafeZoneY[0], fSafeZoneX[1], fSafeZoneY[1]);
		}

		Vector2 vPos( fSafeZoneX[0], fSafeZoneY[1] - LOGO_HEIGHT + TEXTURE_BOTTOM_OFFSET);
		Vector2 vSize( LOGO_WIDTH, LOGO_HEIGHT);
		CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(WIDESCREEN_FORMAT_SIZE_ONLY, &vPos, &vSize);

		Vector2 v[4], uv[4];

		v[0].Set(vPos.x,vPos.y+vSize.y);
		v[1].Set(vPos.x,vPos.y);
		v[2].Set(vPos.x+vSize.x,vPos.y+vSize.y);
		v[3].Set(vPos.x+vSize.x,vPos.y);

		uv[0].Set(0.0f, 1.0f);
		uv[1].Set(0.0f, 0.0f);
		uv[2].Set(1.0f, 1.0f);
		uv[3].Set(1.0f, 0.0f);


		MULTI_MONITOR_ONLY( ms_Logo.MoveToScreenGUI(v) );
		ms_Logo.Draw(v[0], v[1], v[2], v[3], uv[0], uv[1], uv[2], uv[3], CRGBA(255,255,255));

		grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
		grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_Default);
		grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
		CSprite2d::ClearRenderState();
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::GetLegalMainMovieIndex()
// PURPOSE:	Returns the correct movie index based on args and build type.
/////////////////////////////////////////////////////////////////////////////////////
LoadingScreenMovieIndex CLoadingScreens::GetLegalMainMovieIndex()
{
#if	__FINAL
	return( LOADINGCREEN_MOVIE_INDEX_STARTUP_FINAL );
#else
	if( PARAM_bugstarloadingscreen.Get() )
	{		
#if __BANK
		return( LOADINGCREEN_MOVIE_INDEX_STARTUP_DEBUG );
#else
		return( LOADINGCREEN_MOVIE_INDEX_STARTUP_FINAL );
#endif
	}
	else
	{
		return( LOADINGCREEN_MOVIE_INDEX_STARTUP_FINAL );
	}
#endif // __FINAL
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::GetNewGameMovieIndex()
// PURPOSE:	Returns the correct movie index based on args and build type.
/////////////////////////////////////////////////////////////////////////////////////
LoadingScreenMovieIndex CLoadingScreens::GetNewGameMovieIndex()
{
#if	__FINAL
	return( LOADINGCREEN_MOVIE_INDEX_NEWGAME_FINAL );
#else
	if( PARAM_bugstarloadingscreen.Get() )
	{
#if __BANK
		return( LOADINGCREEN_MOVIE_INDEX_NEWGAME_DEBUG );
#else
		return( LOADINGCREEN_MOVIE_INDEX_NEWGAME_FINAL );
#endif	//__BANK
	}
	else
	{
		return( LOADINGCREEN_MOVIE_INDEX_NEWGAME_FINAL );
	}
#endif // __FINAL
}

void CLoadingScreens::SetLoadingContext(LoadingScreenContext eContext)
{
	if(eContext != ms_Context)
	{
		uiDebugf3("CLoadingScreens::SetLoadingContext -- Old Context = %d, New Context = %d", ms_Context, eContext);

		ms_Context = eContext;

		if(DoesCurrentContextAllowNews() && !CNetworkTransitionNewsController::Get().IsActive())
		{
			ms_bReadyToInitNews = true;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::SetMovieContext()
// PURPOSE:	Sets the main loading screen movie context ( Legal/Loadgame/Install, etc )
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::SetMovieContext( LoadingScreenMovieContext iMovieContext)
{
	if( IsLoadingMovieActive() )
	{
		if( ms_CurrentMovieIndex == LOADINGCREEN_MOVIE_INDEX_STARTUP_FINAL ||  ms_CurrentMovieIndex == LOADINGCREEN_MOVIE_INDEX_NEWGAME_FINAL )
		{
			if( ms_LoadingScreenMovie.BeginMethod("SET_CONTEXT") )
			{
				CScaleformMgr::AddParamInt(iMovieContext);
				CScaleformMgr::EndMethod();
			}
		}
	}
}

void CLoadingScreens::SetScreenOrder(bool bSingleplayer)
{
	if( ms_LoadingScreenMovie.BeginMethod("SET_SCREEN_ORDER") )
	{
		CScaleformMgr::AddParamBool(bSingleplayer);
		CScaleformMgr::EndMethod();
	}
}

void CLoadingScreens::SetNewsScreenOrder(bool bSingleplayer)
{
	if( ms_LoadingScreenMovie.BeginMethod("SET_NEWS_SCREEN_ORDER") )
	{
		CScaleformMgr::AddParamBool(bSingleplayer);
		CScaleformMgr::EndMethod();
	}
}

void CLoadingScreens::LoadNewsMovie(s32& inout_newsMovieId)
{
    if(!CScaleformMgr::IsMovieActive(inout_newsMovieId))
    {
        inout_newsMovieId = CScaleformMgr::CreateMovieAndWaitForLoad("GTAV_ONLINE", Vector2(0.0f, 0.0f), Vector2(1.0f,1.0f), false);
        if(uiVerifyf(inout_newsMovieId != INVALID_MOVIE_ID, "Failed to load news movie"))
        {
            CScaleformMgr::ForceMovieUpdateInstantly( inout_newsMovieId, true);  // this movie requires instant updating
            CScaleformMgr::CallMethod(inout_newsMovieId, SF_BASE_CLASS_GENERIC, "HIDE_ONLINE_LOGO", true);	// Immediately hide the GTAV Online Logo
            CScaleformMgr::SetMovieDisplayConfig(inout_newsMovieId, SF_BASE_CLASS_GENERIC, CScaleformMgr::SDC::ForceWidescreen | CScaleformMgr::SDC::UseFakeSafeZoneOnBootup);
        }
    }
}

void CLoadingScreens::ShutdownNewsMovie(s32& inout_newsMovieId)
{
    if(CScaleformMgr::IsMovieActive(inout_newsMovieId))
    {
        CScaleformMgr::ForceMovieUpdateInstantly(inout_newsMovieId, false);  // no further updates after removal
        CScaleformMgr::RequestRemoveMovie(inout_newsMovieId);

        inout_newsMovieId = INVALID_MOVIE_ID;
    }
}

void CLoadingScreens::RenderLoadingScreenMovie()
{
    if( IsLoadingMovieActive() )
    {
        static float previousTime;
        float const c_currTime = ms_LoadingTimer.GetTime();
        float const c_timeDelta = Min(c_currTime - previousTime, 0.1f);
        previousTime = c_currTime;

        CScaleformMgr::RenderMovie(ms_LoadingScreenMovie.GetMovieID(), c_timeDelta, true);
    }
}

void CLoadingScreens::EnableLoadingScreenMovieProgressText()
{
    if( IsLoadingMovieActive() )
    {
        if( ms_LoadingScreenMovie.BeginMethod( "SHOW_TEXT_FOR_LANDING" ) )
        {
            ms_LoadingScreenMovie.EndMethod();
        }
    }
}

void CLoadingScreens::DisableLoadingScreenMovieProgressText()
{
    if( IsLoadingMovieActive() )
    {
        if( ms_LoadingScreenMovie.BeginMethod( "HIDE_TEXT_FOR_LANDING" ) )
        {
            ms_LoadingScreenMovie.EndMethod();
        }
    } 
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::IsMovieActive()
// PURPOSE:	Check if there is a current active movie.
/////////////////////////////////////////////////////////////////////////////////////
bool CLoadingScreens::IsLoadingMovieActive()
{
	return ms_LoadingScreenMovie.IsActive();
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::GetMovieNameFromIndex()
// PURPOSE:	returns the movie name from it's predefined index.
/////////////////////////////////////////////////////////////////////////////////////
const char* CLoadingScreens::GetMovieNameFromIndex( LoadingScreenMovieIndex iIndex )
{
	const char* ret = ms_LoadingScreenNameInvalid;

	switch( iIndex )
	{
	case LOADINGCREEN_MOVIE_INDEX_STARTUP_FINAL:
		ret = ms_LoadingScreenNameStartupFinal;
		break;

	case LOADINGCREEN_MOVIE_INDEX_STARTUP_DEBUG:
		ret = ms_LoadingScreenNameStartupDebug;
		break;

	case LOADINGCREEN_MOVIE_INDEX_NEWGAME_FINAL:
		ret = ms_LoadingScreenNameNewGameFinal;
		break;

	case LOADINGCREEN_MOVIE_INDEX_NEWGAME_DEBUG:
		ret = ms_LoadingScreenNameNewGameDebug;
		break;

	default:
		uiAssertf(false, "Invalid movie index");
		break;
	}
	return( ret );
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::UpdateSysTime()
// PURPOSE:	Updates the loading time.
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::UpdateSysTime()
{
	u32 currentSystemTime = ms_LoadingTimer.GetSystemMsTime();

#if __ENABLE_RFS
	// Update the starting time instead of the current time if things are paused.
	if (fiIsShowingMessageBox || AreSuspended())
	{
		ms_iStartLoadingTime += (currentSystemTime - ms_PrevSystemTime);
	}
#endif

	if ( (currentSystemTime-ms_iStartLoadingTime) > (ms_iCurrentLoadingTime + 1000) )  // if we have somehow skipped a second (ie if waiting on an assert), then adjust accordingly for that amount of time
	{
		ms_iStartLoadingTime += (currentSystemTime - ms_PrevSystemTime);
	}

	ms_iCurrentLoadingTime = currentSystemTime-ms_iStartLoadingTime;  // increment our loading time
	ms_PrevSystemTime = currentSystemTime;

#if RSG_ORBIS
	if(IsDisplayingLegalScreen() || ms_LoadingPercentageStartTime == 0u || ms_Context == LOADINGSCREEN_CONTEXT_INSTALL)
	{
		// We use ms_iCurrentLoadingTime and not ms_iStartLoadingTime as our timer is based on ms_iCurrentLoadingTime. 
		ms_LoadingPercentageStartTime = ms_iCurrentLoadingTime;
	}
#endif // RSG_ORBIS
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::TickLoadingProgress()
// PURPOSE:	Steps the loading progress into the next segment.
//			bInitSession set to true will quickly move the loading bar to 100%
//          *Production loading screen only*
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::TickLoadingProgress( bool bInitSession )
{
	if( ms_FinalProgressInitSessionTick )
	{
		return;
	}

	if( bInitSession )
	{
		ms_FinalProgressInitSessionTick = true;
		ms_FinalProgressSegCurrent = ms_FinalProgressSegCount;
		return;
	}
	
	if( ms_FinalProgressSegCurrent != ms_FinalProgressSegCount )
	{
		ms_FinalProgressSegCurrent += 1.0f;
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::UpdateLoadingProgressBar()
// PURPOSE:	Update the progress bar to reflect loading progress.
//          *Production loading screen only*
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::UpdateLoadingProgressBar()
{
	if( ms_CurrentMovieIndex == LOADINGCREEN_MOVIE_INDEX_STARTUP_FINAL ||  ms_CurrentMovieIndex == LOADINGCREEN_MOVIE_INDEX_NEWGAME_FINAL )
	{
		if( !IsLoadingMovieActive() )
		{
			return;
		}

		float speedMax = LOADINGSCREEN_PROGRESS_SPEED_MAX;
		float speedMaxInitSession = LOADINGSCREEN_PROGRESS_INITSESSIONSPEED;
		if( ms_Context == LOADINGSCREEN_CONTEXT_LOADLEVEL || ms_Context == LOADINGSCREEN_CONTEXT_MAPCHANGE )
		{
			// Hurry things up for new game...
			speedMax = LOADINGSCREEN_PROGRESS_SPEED_MAX*24.0f;
			speedMaxInitSession = LOADINGSCREEN_PROGRESS_INITSESSIONSPEED*24.0f;
		}

		if( ms_FinalProgressInitSessionTick )
		{
			ms_FinalProgressTickInc = speedMaxInitSession;
		}
		else
		{
			float progressTarget = ms_FinalProgressSegCurrent*(100.0f/ms_FinalProgressSegCount);

			if( progressTarget > ms_FinalProgressPercent )
			{
				ms_FinalProgressTickInc = ( progressTarget-ms_FinalProgressPercent ) / LOADINGSCREEN_PROGRESS_RATE_UNDER;
			}

			if( progressTarget <= ms_FinalProgressPercent )
			{
				ms_FinalProgressTickInc = ( speedMax - ( ms_FinalProgressPercent-progressTarget ) ) / LOADINGSCREEN_PROGRESS_RATE_OVER;
			}

			ms_FinalProgressTickInc = Clamp(ms_FinalProgressTickInc, LOADINGSCREEN_PROGRESS_SPEED_MIN, speedMax);
		}
				
		ms_FinalProgressPercent += ms_FinalProgressTickInc;
		if( ms_FinalProgressPercent > 100.0f )
		{
			ms_FinalProgressPercent = 100.0f;
		}

		if( (ms_FinalProgressPercent-ms_FinalProgressPercentCmp) >= 0.1f || ms_FinalProgressPercent == 100.0f )
		{
			if( ms_FinalProgressPercent != ms_FinalProgressPercentCmp )
			{
				if (ms_LoadingScreenMovie.BeginMethod("SET_PROGRESS_BAR"))
				{
					CScaleformMgr::AddParamFloat(ms_FinalProgressPercent);
					CScaleformMgr::EndMethod();
				}
				ms_FinalProgressPercentCmp = ms_FinalProgressPercent;
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::UpdateInstallProgressBar()
// PURPOSE:	Update the progress bar to reflect loading progress.
//          *Production loading screen only*
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::UpdateInstallProgressBar( float fProgressPercent, const char* cProgressString )
{
	if( ms_CurrentMovieIndex == LOADINGCREEN_MOVIE_INDEX_STARTUP_FINAL )
	{
		if( !IsLoadingMovieActive() )
		{
			return;
		}

		if (ms_LoadingScreenMovie.BeginMethod("SET_PROGRESS_BAR"))
		{
			CScaleformMgr::AddParamFloat( fProgressPercent );
			CScaleformMgr::EndMethod();
		}

		

		if (ms_LoadingScreenMovie.BeginMethod("SET_PROGRESS_TEXT"))
		{
			CScaleformMgr::AddParamString(cProgressString);
			CScaleformMgr::EndMethod();
		}

		if( cProgressString[0] != '\0' )
		{
			if (ms_LoadingScreenMovie.BeginMethod("SET_PROGRESS_TITLE"))
			{
				char* installString = NULL;
				switch (ms_installState)
				{
				case IS_INSTALL:
					installString = TheText.Get("INSTALL_SCREEN_INSTALLING");
					break;
				case IS_CLOUD:
					installString = TheText.Get("INSTALL_SCREEN_CLOUD");
					break;
				case IS_INIT_DOWNLOAD:
					installString = TheText.Get("INSTALL_SCREEN_INIT");
					break;
				case IS_DOWNLOAD:
					installString = TheText.Get("INSTALL_SCREEN_DOWNLOAD");
					break;
				}
				CScaleformMgr::AddParamString(installString);
				CScaleformMgr::EndMethod();
			}
		}
		else
		{
			if (ms_LoadingScreenMovie.BeginMethod("SET_PROGRESS_TITLE"))
			{
				CScaleformMgr::AddParamString("");
				CScaleformMgr::EndMethod();
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::SetButton()
// PURPOSE:	Sets up the a button ( for the install / load game screen. )
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::SetButton( int iIndex, eInstructionButtons iButtonType, const char* stringParam)
{
	if( ms_CurrentMovieIndex == LOADINGCREEN_MOVIE_INDEX_STARTUP_FINAL || ms_CurrentMovieIndex == LOADINGCREEN_MOVIE_INDEX_NEWGAME_FINAL)
	{
		if( !IsLoadingMovieActive() )
		{
			return;
		}

		if (ms_LoadingScreenMovie.BeginMethod("SET_BUTTONS"))
		{
			CScaleformMgr::AddParamInt( iIndex );
			CScaleformMgr::AddParamInt( iButtonType );

			if ( stringParam != NULL )
			{
				CScaleformMgr::AddParamString(stringParam, false);
			}

			CScaleformMgr::EndMethod();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::SetButtonLoc()
// PURPOSE:	Sets up the a button ( for the install / load game screen. )
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::SetButtonLoc( int iIndex, eInstructionButtons iButtonType, const char* locLabel)
{
	SetButton(iIndex, iButtonType, TheText.Get(locLabel));
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::SetSpinner()
// PURPOSE:	Sets up the loading spinner ( for the install / load game screen. )
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::SetSpinner( int iIndex, const char* stringParam, bool bIsLocString )
{
#if RSG_ORBIS
	// Store the information required so we can update the spinner percentage.
	ms_SpinnerIndex		  = iIndex;
	ms_SpinnerStringParam = stringParam;
	ms_SpinnerStringNeedsLoc = bIsLocString;

	float averageLoadTime;

	if(bIsLocString && strcmp(stringParam, BTN_LABEL_LOADING_STORY) == 0)
	{
		averageLoadTime = AVERAGE_SP_LOADING_TIME;
	}
	else if(bIsLocString && strcmp(stringParam, BTN_LABEL_ONLINE) == 0)
	{
		averageLoadTime = AVERAGE_MP_LOADING_TIME;
	}
	else
	{
		// Assume a random job with the longest loading time.
		averageLoadTime = AVERAGE_MP_JOB_LOADING_TIME;
	}

	const float remainingTime	 = Min(static_cast<float>(ms_iCurrentLoadingTime - ms_LoadingPercentageStartTime), averageLoadTime);
	u32 remainingPercent		 = Min(static_cast<u32>((remainingTime / averageLoadTime) * 100.0f), 99u); // never show 100%

	// Only show the percentage as multiples of PERCENT_INCREMENT.
	const u32 PERCENT_INCREMENT = 10;

	// Floor to nearest PERCENT_INCREMENT. We Floor so we never reach 100%.
	remainingPercent /= PERCENT_INCREMENT;
	remainingPercent *= PERCENT_INCREMENT;
	
	const u32 DISPLAY_STRING_LEN = 255u;
	char displayString[DISPLAY_STRING_LEN] = {0};

	formatf(displayString, "%s: (%u%%)", bIsLocString ? TheText.Get(stringParam) : stringParam, remainingPercent);
	SetButton(iIndex, ICON_SPINNER, displayString);

#else
#if RSG_DURANGO
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		char displayName[RL_MAX_DISPLAY_NAME_BUF_SIZE];
		bool bSuccess = rlPresence::GetDisplayName(localGamerIndex, displayName, RL_MAX_DISPLAY_NAME_BUF_SIZE);

		if(displayName && bSuccess)
		{
			char stringWithGamerName[RL_MAX_DISPLAY_NAME_BUF_SIZE + 128];
			formatf(stringWithGamerName, sizeof(stringWithGamerName), "%s: <C>%s</C>", bIsLocString ? TheText.Get(stringParam) : stringParam, displayName);
			SetButton(iIndex, ICON_SPINNER, stringWithGamerName);
			return;
		}
	}
#endif

	SetButton(iIndex, ICON_SPINNER, bIsLocString ? TheText.Get(stringParam) : stringParam );

#endif // RSG_ORBIS
}

#if RSG_ORBIS
/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::UpdateSpinner()
// PURPOSE:	Updates the loading spinner ( for the install / load game screen. )
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::UpdateSpinner()
{
	if(ms_SpinnerStringParam != NULL)
	{
		SetSpinner(ms_SpinnerIndex, ms_SpinnerStringParam, ms_SpinnerStringNeedsLoc);
	}
}
#endif // RSG_ORBIS

// tell the player we rebooted the game for him
bool CLoadingScreens::ShowRebootMessage(const char* label, u32 rebootId)
{
	if (IsDisplayingLegalScreen())
		return false;

	if (!label || label[0] == '\0')
		return false;

	static bool showedMessage = false;

	if (showedMessage)
		return false;

	if (!fiDeviceInstaller::HasRebooted(rebootId))
	{
		showedMessage = true;
		return false;
	}

	CWarningScreen::SetMessage(WARNING_MESSAGE_STANDARD, label, (eWarningButtonFlags)(FE_WARNING_OK|FE_WARNING_NOSOUND));
	if (CWarningScreen::CheckAllInput(false) == FE_WARNING_OK)
	{
		CWarningScreen::Remove();
		showedMessage = true;
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::ResetButtons()
// PURPOSE:	Hides buttons.
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::ResetButtons()
{
	if( ms_CurrentMovieIndex == LOADINGCREEN_MOVIE_INDEX_STARTUP_FINAL || ms_CurrentMovieIndex == LOADINGCREEN_MOVIE_INDEX_NEWGAME_FINAL)
	{
		if( IsLoadingMovieActive() )
		{
			ms_LoadingScreenMovie.CallMethod("HIDE_BUTTONS");
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::UpdateInput()
// PURPOSE:	Handles bootup option input selections (Story, Online, Event,  Random Job, etc.)
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::UpdateInput()
{
	if (CDisplayCalibration::IsActive() /* fixes 1589924*/ || CPauseMenu::IsActive())
	{
		return;
	}

	if( !IsLoadingMovieActive() )
	{
		return;
	}

	if( CSavegameDialogScreens::GetMostRecentErrorCode() != SAVE_GAME_DIALOG_NONE )
	{
		return;
	}

	// If we are being told what to load then do not give the option to change.
	if(HasExternalContentLoadRequest())
	{
		return;
	}

    // if we're setup for SP only and online becomes available, refresh the buttons
    // return early so that we draw the buttons before they can be selected
    if(ms_SpOnly && CanSelectOnline())
    {
        RefreshButtonLayout();
        return;
    }

	// whether we should refresh the button layout - this will be set by various conditions below
	bool refreshButtons = false;
	bool selectedBootOption = false;

	// for checking inputs
	CPad *pPad = CControlMgr::GetPlayerPad();

#if RSG_ORBIS
	// PS+ promotion button can be selected in SP or MP flows
	bool eventPressed = pPad && pPad->ButtonTriangleJustUp();
	if(eventPressed)
	{
		if(!ms_PlusUpsellRequested && !ms_EventButtonDown && ShouldShowPlusUpsell())
		{
			ms_EventButtonDown = true;
			ms_PlusUpsellRequested = true;

			netLoadingDebug("CheckInput :: Showing Upgrade Account UI");
			CLiveManager::ShowAccountUpgradeUI(Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PLUS_PROMOTION_LOADING_SCREEN_BROWSER_CLOSE_TIME", 0x168f1305), 0));
			CLoadingScreens::SetScreenOrder(false);
			refreshButtons = true;
		}
	}
	else
	{
		ms_EventButtonDown = false;
	}
#endif

	// All other options are only when we can select online
	if(!ms_SpOnly)
	{
		// Check Story Selection
		bool acceptPressed = pPad && (AcceptIsCross() ? pPad->ButtonCrossJustUp() : pPad->ButtonCircleJustUp()); // Down to Up so it fixes 1589924
		if(acceptPressed)
		{
			if(!selectedBootOption && !ms_SpButtonDown)
			{
				ms_SpButtonDown = true;
				if(!ms_MpSpCommit)
				{
					netLoadingDebug("CheckInput :: Selected Story - Resetting GTAO auto launch flags");
					CNetwork::SetGoStraightToMultiplayer(false);
					CNetwork::SetGoStraightToMPEvent(false);
					CNetwork::SetGoStraightToMPRandomJob(false);
					CLoadingScreens::SetScreenOrder(true);
					CLoadingScreens::SetNewsScreenOrder(true);
					refreshButtons = true;
					selectedBootOption = true;
				}
				else
				{
					netLoadingDebug("CheckInput :: Selected Story - Already committed selection!");
				}
			}
		}
		else
		{
			ms_SpButtonDown = false;
		}

		// Check Online Selection
		bool onlinePressed = pPad && pPad->ButtonSquareJustUp();
		if(onlinePressed)
		{
			if(!selectedBootOption && !ms_MpButtonDown)
			{
				ms_MpButtonDown = true;
				if(!ms_MpSpCommit)
				{
					netLoadingDebug("CheckInput :: Selected Online (General)");
					CNetwork::SetGoStraightToMultiplayer(true);
					CNetwork::SetGoStraightToMPEvent(false);
					CNetwork::SetGoStraightToMPRandomJob(false);
					CLoadingScreens::SetScreenOrder(false);
					CLoadingScreens::SetNewsScreenOrder(false);
					refreshButtons = true;
					selectedBootOption = true;
				}
				else
				{
					netLoadingDebug("CheckInput :: Selected Online - Already committed selection!");
				}
			}
		}
		else
		{
			ms_MpButtonDown = false;
		}

		// Check Online Event Selection
		bool eventPressed = pPad && pPad->ButtonTriangleJustUp();
        if(eventPressed)
        {
            if(!selectedBootOption && !ms_EventButtonDown && SocialClubEventMgr::Get().HasFeaturedEvent() ORBIS_ONLY(&& !ShouldShowPlusUpsell()))
            {
				ms_EventButtonDown = true;
				if(!ms_MpSpCommit)
				{
					netLoadingDebug("CheckInput :: Selected Online Event");
					CNetwork::SetGoStraightToMultiplayer(true);
					CNetwork::SetGoStraightToMPEvent(true);
					CNetwork::SetGoStraightToMPRandomJob(false);
					CLoadingScreens::SetScreenOrder(false);
					refreshButtons = true;
					selectedBootOption = true;
				}
            }
        }
        else
        {
            ms_EventButtonDown = false;
        }

		// Check Random Job Selection
		bool jobPressed = pPad && pPad->RightShoulder1JustUp();
		if(jobPressed)
		{
            if(!selectedBootOption && !ms_RandomJobButtonDown && SocialClubEventMgr::Get().GetDataReceived())
            {
                ms_RandomJobButtonDown = true;
			    if(!ms_MpSpCommit)
			    {
				    netLoadingDebug("CheckInput :: Selected Online Job");
				    CNetwork::SetGoStraightToMultiplayer(true);
				    CNetwork::SetGoStraightToMPEvent(false);
				    CNetwork::SetGoStraightToMPRandomJob(true);
				    CLoadingScreens::SetScreenOrder(false);
				    refreshButtons = true;
					selectedBootOption = true;
			    }
            }
		}
		else
		{
			ms_RandomJobButtonDown = false;
		}

        if(!ms_DisplayingJobButton && SocialClubEventMgr::Get().GetDataReceived())
        {
			netLoadingDebug("CheckInput :: Event Data Received - Refresh Buttons");
			ms_DisplayingJobButton = true;
            refreshButtons = true; 
        }

        if(!ms_DisplayingEventButton && SocialClubEventMgr::Get().HasFeaturedEvent())
        {
			netLoadingDebug("CheckInput :: Has Featured Event - Refresh Buttons");
			ms_DisplayingEventButton = true;
            refreshButtons = true;
        }

        if(!ms_DisplayingOnlineButton && CanSelectOnline())
        {
            ms_DisplayingOnlineButton = true;
            refreshButtons = true;
        }
	}

#if RSG_ORBIS
	// PS+ promotion button can be shown in SP or MP paths
	if(!ms_DisplayingEventButton && ShouldShowPlusUpsell())
	{
		netLoadingDebug("CheckInput :: Should Show PS+ Upsell - Refresh Buttons");
		g_rlNp.GetCommonDialog().PreloadBrowserDialog();
		ms_DisplayingEventButton = true;
		refreshButtons = true;
	}

	if(ms_PlusUpsellRequested)
	{
		if(!g_rlNp.GetCommonDialog().IsDialogShowing())
		{
			netLoadingDebug("CheckInput :: Should Show PS+ Upsell - Refresh Buttons");
			ms_PlusUpsellRequested = false;
			refreshButtons = true;
		}
	}
#endif
	
	if(refreshButtons)
	{
		RefreshButtonLayout();
		if(selectedBootOption)
		{
			ms_bSelectedBootOption = true;
		}
	}
}

void CLoadingScreens::CheckStartupFlowPref()
{
	// Don't read startup flow profile setting if we've already set the layout based on profile setting,
	// If the command line / sys params tell us to load directly into MP, landing page is enabled, or the user has selected a loading option
	if(ms_bSpMpPrefSet || HasExternalContentLoadRequest() || CLandingPageArbiter::ShouldShowLandingPageOnBoot() || ms_bSelectedBootOption)
	{
		return;
	}

	if(SetStartupFlowPref())
	{
		RefreshButtonLayout();
		ms_bSpMpPrefSet = true;
	}
}

bool CLoadingScreens::SetStartupFlowPref()
{
	bool bSet = false;

	if(CProfileSettings::GetInstance().AreSettingsValid())
	{
		const int DEFAULT_TO_MP = 1;
		int iStartFlow = CPauseMenu::GetMenuPreference(PREF_STARTUP_FLOW);
		if(iStartFlow == DEFAULT_TO_MP)
		{
			netLoadingDebug("SetStartupFlowPref :: SetGoStraightToMultiplayer: True");
			CNetwork::SetGoStraightToMultiplayer(true);
			CNetwork::SetGoStraightToMPEvent(false);
			bSet = true;
		}
		else
		{
			netLoadingDebug("SetStartupFlowPref :: SetGoStraightToMultiplayer: False");
			CNetwork::SetGoStraightToMultiplayer(false);
			CNetwork::SetGoStraightToMPEvent(false);
			bSet = true;
		}
	}

	return bSet;
}

void CLoadingScreens::UpdateDisplayConfig()
{
	ms_LoadingScreenMovie.SetDisplayConfig(CScaleformMgr::SDC::ForceWidescreen | CScaleformMgr::SDC::UseFakeSafeZoneOnBootup);
}

void CLoadingScreens::SetSocialClubLoadingSpinner()
{
	CLoadingScreens::SetLoadingContext(LOADINGSCREEN_CONTEXT_LOADGAME);
	SetScreenOrder(GetLoadingSP());
	SetNewsScreenOrder(GetLoadingSP());
	CLoadingScreens::SetMovieContext(LOADINGSCREEN_SCALEFORM_CTX_GAMELOAD_NEWS);
	ShutdownSocialClubLoadingSpinner();
	CLoadingScreens::SetSpinner(0, BTN_LABEL_LOADING_SC);
}

void CLoadingScreens::ShutdownSocialClubLoadingSpinner()
{
	ResetButtons();
}

bool CLoadingScreens::HasExternalContentLoadRequest()
{
#if RSG_ORBIS
	if( !ms_HasExternalContentLoadRequest && 
		( g_SysService.HasParam("-StraightIntoFreemode") || 
		( g_SysService.HasParam("-LiveAreaContentType") && g_SysService.HasParam("-LiveAreaLoadContent") ) ) )
	{
		// Cache the param state as script clear it during load.
		ms_HasExternalContentLoadRequest = true;
	}
	return PARAM_StraightIntoFreemode.Get() || ms_HasExternalContentLoadRequest;
#elif RSG_PC
	return PARAM_StraightIntoFreemode.Get();
#else
	return false;
#endif // RSG_ORBIS
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::RenderIntroMovie()
// PURPOSE:	renders the bink intro movie
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::RenderIntroMovie()
{
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);

#if SUPPORT_MULTI_MONITOR
	const GridMonitor &mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();
	float width = (float)mon.getWidth();
	float height = width * ((float)1080.0f / (float)1920.0f);
	float offsetX = (float)mon.uLeft;
	float offsetY = mon.uTop + 0.5f * (mon.getHeight() - height);
#else //SUPPORT_MULTI_MONITOR
	float width = (float)VideoResManager::GetNativeWidth();
	float height = width * ((float)1080.0f / (float)1920.0f);
	float offsetX = 0.f;
	float offsetY = ((float)VideoResManager::GetNativeHeight() - height) / 2.f;
#endif //SUPPORT_MULTI_MONITOR

	Vector2 p1(offsetX, offsetY + height);
	Vector2 p2(offsetX, offsetY);
	Vector2 p3(offsetX + width, offsetY + height);
	Vector2 p4(offsetX + width, offsetY);
	Color32 color = Color_white;

	GRCDBG_PUSH("IntroBink");
	if (ms_introMovie)
	{
		ms_introMovie->UpdateMovieTime(ms_PrevSystemTime);

		ms_introMovie->BeginDraw(RSG_DURANGO ? false : true);
		CSprite2d::DrawSwitch(false, false, 0.f, p1, p2, p3, p4, color);

		if (ms_introMovie->IsWithinBlackout())
		{
			CSprite2d::DrawSwitch(false, false, 0.f, p1, p2, p3, p4, Color_black);
		}

		ms_introMovie->EndDraw();
		bwMovie::ShaderEndDraw(ms_introMovie);
	}
	GRCDBG_POP();

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::RenderLegalText()
// PURPOSE:	renders legal guff
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::RenderLegalText()
{
#if !__FINAL
	if( PARAM_bugstarloadingscreen.Get() )
	{
		return;
	}
#endif
	
	static char gxtLegalString[2048]={'\0'};

	CTextLayout LegalTextLayout;

	LegalTextLayout.SetOrientation(FONT_CENTRE);
	LegalTextLayout.SetColor(CRGBA(225,225,225,255));
	Vector2 vCalabTextPos = Vector2(0.5f, 0.4f);
	Vector2 vCalabTextSize = Vector2(0.33f, 0.4785f);
	Vector2 vCalabTextWrap = Vector2(0.15f, 0.85f);

	CHudTools::AdjustForWidescreen(WIDESCREEN_FORMAT_SIZE_ONLY, NULL, &vCalabTextSize, NULL);

	LegalTextLayout.SetScale(&vCalabTextSize);
	LegalTextLayout.SetWrap(&vCalabTextWrap);

	if(gxtLegalString[0] == '\0')
	{
#if __PPU
		CTextConversion::charStrcpy(gxtLegalString, TheText.Get("LEGAL_PS3_US_1"));
#elif __XENON
		CTextConversion::charStrcpy(gxtLegalString, TheText.Get("LEGAL_360_US_1"));
#else
		uiAssertf(0, "Invalid legal text!");
		gxtLegalString[0] = '\0';
#endif
	}

	// centre the text
	//vCalabTextPos.y -= (LegalTextLayout.GetNumberOfLines(&vCalabTextPos, gxtLegalString) * LegalTextLayout.GetCharacterHeight()) * 0.5f;

	LegalTextLayout.Render(&vCalabTextPos, gxtLegalString);

	CText::Flush();

	CSprite2d::DrawRect( fwRect(0.0f, 0.2f, 1.0f, 0.0f),  CRGBA(30,30,30,255));
	CSprite2d::DrawRect( fwRect(0.0f, 0.8f, 1.0f, 1.0f),  CRGBA(30,30,30,255));
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::LoadMovie()
// PURPOSE:	loads a scaleform movie for the loadingscreens to display
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::LoadLoadingScreenMovie( LoadingScreenMovieIndex iIndex )
{
	if( uiVerifyf(!ms_bMovieNoLongerNeeded, "Old movie still active"))
	{
		ms_LoadingScreenMovie.CreateMovieAndWaitForLoad(SF_BASE_CLASS_GENERIC, GetMovieNameFromIndex(iIndex), Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f), false);
	
		if(uiVerifyf( ms_LoadingScreenMovie.GetMovieID() != INVALID_MOVIE_ID, "Missing or invalid loading screens"))
		{
			CScaleformMgr::ForceMovieUpdateInstantly( ms_LoadingScreenMovie.GetMovieID(), true);  // this movie requires instant updating
			UpdateDisplayConfig();
		}
	}	
}

void CLoadingScreens::LoadNewsMovieInternal()
{
	LoadNewsMovie( ms_iNewsMovieId );
}

void CLoadingScreens::ShutdownNews()
{
    CNetworkTransitionNewsController::Get().Reset();
    ShutdownNewsMovie( ms_iNewsMovieId );
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::UnloadMovie()
// PURPOSE:	unloads a movie if it needs to be removed.  This needs to be called after
//			we have finished rendering
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::UnloadMovie()
{
	Assert(ms_bMovieNoLongerNeeded);

	if (gRenderThreadInterface.IsUsingDefaultRenderFunction())  // only remove loadingscreen movies whilst in the default render function
	{
		if( IsLoadingMovieActive() )
		{
			CScaleformMgr::ForceMovieUpdateInstantly(ms_LoadingScreenMovie.GetMovieID(), false); // no further updates after removal
			ms_LoadingScreenMovie.RemoveMovie();

			ms_bMovieNoLongerNeeded = false;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::Suspend()
// PURPOSE:	suspend until resume() is called
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::Suspend()
{
	ms_bSuspended = true;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::Resume()
// PURPOSE:	resume loadingscreens from suspend()
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::Resume()
{
	ms_bSuspended = false;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::AreActive()
// PURPOSE:	returns whether the loading screens are active
/////////////////////////////////////////////////////////////////////////////////////
bool CLoadingScreens::AreActive()
{
	return( ms_Context != LOADINGSCREEN_CONTEXT_NONE );
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::IsDisplayingLegalScreen()
// PURPOSE:	returns whether the legal screen has finished displaying or not
/////////////////////////////////////////////////////////////////////////////////////
bool CLoadingScreens::IsDisplayingLegalScreen()
{
	switch( ms_Context )
	{
		case LOADINGSCREEN_CONTEXT_INTRO_MOVIE:
		case LOADINGSCREEN_CONTEXT_LEGALSPLASH:
		case LOADINGSCREEN_CONTEXT_LEGALMAIN:
			return true;

		default:
			return false;
	}
}

void CLoadingScreens::SleepUntilPastLegalScreens()
{
	if(CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		Assertf(0, "CLoadingScreens::SleepUntilPastLegalScreens cannot be called on the render thread!");
		return;
	}

	while( IsDisplayingLegalScreen() )
	{
		Warningf("Waiting for the legal screens are passed before displaying a warning screen. If you see this for a prolonged period of time, this is being called too early!");
		sysIpcSleep( 100 );
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::SetButtonSwapFlag()
// PURPOSE:	sets up the flag to determine if the X/O buttons should be swapped
//          The pause menu already sets it's own flag for this but that's not
//          initialized until later on. 
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::SetButtonSwapFlag()
{
	ms_AcceptIsCross = true;

#if __PPU
	int value = 1;
	// The PS3 Documentation state that the accept/cancel buttons must be swapped in certain regions. This does not get set by the settings but is read from the console.
	uiVerify(cellSysutilGetSystemParamInt(CELL_SYSUTIL_SYSTEMPARAM_ID_ENTER_BUTTON_ASSIGN, &value)ASSERT_ONLY(==CELL_OK));
	if( !value )
	{
		ms_AcceptIsCross = false;
	}
#endif
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::AcceptIsCross
// PURPOSE:	used to determine if the X/O buttons should be swapped
/////////////////////////////////////////////////////////////////////////////////////
bool CLoadingScreens::AcceptIsCross()
{
	return ms_AcceptIsCross;
}

// --------------------------------------------------------------------------------------
// --- BANK
// --------------------------------------------------------------------------------------

#if __BANK

// --- CLoadingScreenBugList ------------------------------------------------------------

CLoadingScreenBugList::ToDoNowInfoData	CLoadingScreenBugList::m_TodoNowData[TODO_NOW_MAX] = 
{
	// Title,																search,							filename
	{ "Open PT",															"/TaggedBugs/pt",				"TaggedPT_" },
	{ "More Info",															"/MoreInfoBugs",				"MoreInfoBugs_" },
	{ "Overdue",															"/OverdueBugs",					"OverdueBugs_" },
	{ "On Hold To You",														"/OnHoldBugs",					"OnHoldBugs_" },
	{ "No Estimated Time",													"/SubordinatesNoEstTimeBugs",	"SubordinatesNoEstTimeBugs_" },
	{ "Bugs assigned to default teams in which you are an emailed member",	"/BugsNeedingAssigned",			"BugsNeedingAssigned_" },
	{ "Open Screenshot Flagged Bugs",										"/OpenScreenshotFlagBugs",		"OpenScreenshotFlagBugs_" }
};

void CLoadingScreenBugList::Init()
{
	USE_DEBUG_MEMORY();

	const char* pUserName=NULL;

	if(PARAM_bugstarusername.Get(pUserName))
		strcpy(m_userName, pUserName);
	else
		sysGetEnv("USERNAME",m_userName,MAX_USERNAME_CHARS);
}


void CLoadingScreenBugList::Reset()
{
	USE_DEBUG_MEMORY();

	m_BugInfo.Reset();
	ResetToDoNowBugList();
	m_UserInfo.Reset();
	m_loaded = false;
}

void CLoadingScreenBugList::ResetBugList()
{
	USE_DEBUG_MEMORY();

	m_BugInfo.Reset();
}

void CLoadingScreenBugList::LoadBugs()
{
	USE_DEBUG_MEMORY();

	char query[256];
	char filename[RAGE_MAX_PATH];

	sprintf(query, BUGSTAR_GTA_PROJECT_PATH "Missions/UserData/%s", m_userName);
	sprintf(filename, "buglist_%s.xml", m_userName);
	parTree* pTree = CBugstarIntegration::GetSavedBugstarRequestList().LoadBugstarRequest(filename);
	CBugstarIntegration::GetSavedBugstarRequestList().Get(filename, query, 60, 32*1024);
	if(pTree)
	{
		UpdateBugList(pTree);
		delete pTree;
	}

	// construct xml for loading "User Open Bugs by Age" report
/*	char xml[1024];
	strcpy(xml, "<Parameters internalName=\"UOBBA\" title=\"User Open Bugs by Age\">");
	strcat(xml,"<Parameter exposed=\"false\" friendlyName=\"Not Shown\" name=\"projectId\" type=\"java.lang.Long\" value=\"1546\"/>");
	strcat(xml,"<Parameter exposed=\"true\" friendlyName=\"Classes\" name=\"categories\" type=\"java.util.List\" value=\"A,TODO,WISH\"/>");
	strcat(xml,"<Parameter exposed=\"true\" friendlyName=\"Minimum bug count\" name=\"bugCountThreshold\" type=\"java.lang.Integer\" value=\"30\"/>");
	strcat(xml,"<Parameter exposed=\"true\" friendlyName=\"Minimum bug count\" name=\"maxCount\" type=\"java.lang.Integer\" value=\"10000\"/>");
	strcat(xml,"<Parameter exposed=\"true\" friendlyName=\"Studio\" name=\"studio\" type=\"java.lang.Long\" value=\"-1\"/>");
//	strcat(xml, "<Parameter name=\"projectId\" type=\"java.lang.Long\" value=\"1546\"/>");
//	strcat(xml, "<Parameter name=\"categories\" type=\"java.util.List\" value=\"A,B,C,D,TODO,TASK\" />");
//	strcat(xml, "<Parameter name=\"bugCountThreshold\" type=\"java.lang.Integer\" value=\"30\"/>");
//	strcat(xml, "<Parameter name=\"studio\" type=\"java.lang.Long\" value=\"-1\" />");
	strcat(xml, "</Parameters>");

	pTree = CBugstarIntegration::GetSavedBugstarRequestList().LoadBugstarRequest("uobba.xml");
	if(pTree)
		delete pTree;
	CBugstarIntegration::GetSavedBugstarRequestList().PostXml("uobba.xml", "Report/Data/UOBBA", xml, strlen(xml), 24*60, 32*1024);*/
}

void CLoadingScreenBugList::UpdateBugList(parTree* pTree)
{
	USE_DEBUG_MEMORY();

	if (pTree)
	{
		parTreeNode* pNodePerUserData= pTree->GetRoot();

		if (pNodePerUserData)
		{
			parTreeNode* pNodeBugSummary = pNodePerUserData->FindChildWithName("BugSummary");
			if (pNodeBugSummary)
			{
				m_totalBugs.total = atoi(pNodeBugSummary->GetElement().FindAttributeAnyCase("grandTotal")->GetStringValue());
				m_totalBugs.a = atoi(pNodeBugSummary->GetElement().FindAttributeAnyCase("totalA")->GetStringValue());
				m_totalBugs.todos = atoi(pNodeBugSummary->GetElement().FindAttributeAnyCase("totalTodo")->GetStringValue());
				m_totalBugs.b = atoi(pNodeBugSummary->GetElement().FindAttributeAnyCase("totalB")->GetStringValue());
				m_totalBugs.tasks = atoi(pNodeBugSummary->GetElement().FindAttributeAnyCase("totalTask")->GetStringValue());
				m_totalBugs.c = atoi(pNodeBugSummary->GetElement().FindAttributeAnyCase("totalC")->GetStringValue());
				m_totalBugs.d = atoi(pNodeBugSummary->GetElement().FindAttributeAnyCase("totalD")->GetStringValue());
			}

			parTreeNode* pNodeUser = NULL;

			while ((pNodeUser = pNodePerUserData->FindChildWithName("User", pNodeUser)) != NULL)
			{
				UserInfoStruct newUserInfo;

				safecpy(newUserInfo.username, pNodeUser->GetElement().FindAttributeAnyCase("username")->GetStringValue(), MAX_USERNAME_CHARS);
				newUserInfo.bugs.tasks = (u8)Min(255, atoi(pNodeUser->GetElement().FindAttributeAnyCase("tasks")->GetStringValue()));
				newUserInfo.bugs.todos = (u8)Min(255, atoi(pNodeUser->GetElement().FindAttributeAnyCase("todos")->GetStringValue()));
				newUserInfo.bugs.a = (u8)Min(255, atoi(pNodeUser->GetElement().FindAttributeAnyCase("aClass")->GetStringValue()));
				newUserInfo.bugs.b = (u8)Min(255, atoi(pNodeUser->GetElement().FindAttributeAnyCase("bClass")->GetStringValue()));
				newUserInfo.bugs.c = (u8)Min(255, atoi(pNodeUser->GetElement().FindAttributeAnyCase("cClass")->GetStringValue()));
				newUserInfo.bugs.d = (u8)Min(255, atoi(pNodeUser->GetElement().FindAttributeAnyCase("dClass")->GetStringValue()));
				newUserInfo.bugs.UpdateTotal();

				if (!strcmpi(newUserInfo.username, m_userName))
				{
					parTreeNode* pNodeDevBugs = NULL;

					//
					// get any WithYouForMoreInfo bugs:
					//
					pNodeDevBugs = pNodeUser->FindChildWithName("WithYouForMoreInfo", pNodeDevBugs);

					if (pNodeDevBugs)
					{
						parTreeNode* pNodeBug = NULL;

						while ((pNodeBug = pNodeDevBugs->FindChildWithName("Bug", pNodeBug)) != NULL)
						{
							const char *pClass = pNodeBug->GetElement().FindAttributeAnyCase("class")->GetStringValue();
							// the list of bug class below are the only ones we want to deal with:
							if ((!strcmp(pClass, "A")) ||
								(!strcmp(pClass, "TODO")) ||
								(!strcmp(pClass, "B")) ||
								(!strcmp(pClass, "C")) ||
								(!strcmp(pClass, "D")) ||
								(!strcmp(pClass, "TASK")) ||
								(!strcmp(pClass, "TRACK")) )
							{
								BugInfoStruct newBugInfo;

								newBugInfo.iBugNum = atoi(pNodeBug->GetElement().FindAttributeAnyCase("id")->GetStringValue());
								newBugInfo.iDaysOpen = 0;	// Not used in this search, init'd to zero (change later if required)
								safecpy(newBugInfo.cBugSummary, pNodeBug->GetElement().FindAttributeAnyCase("summary")->GetStringValue(), MAX_BUG_SUMMARY_CHARS);
								safecpy(newBugInfo.cBugTags, pNodeBug->GetElement().FindAttributeAnyCase("tags")->GetStringValue(), MAX_BUG_TAGS_CHARS);
								safecpy(newBugInfo.cBugClass, pNodeBug->GetElement().FindAttributeAnyCase("class")->GetStringValue(), MAX_BUG_CLASS_CHARS);
								safecpy(newBugInfo.cBugOwnerQA, pNodeBug->GetElement().FindAttributeAnyCase("qaOwner")->GetStringValue(), MAX_BUG_QA_OWNER_CHARS);
								safecpy(newBugInfo.cBugDueDate, pNodeBug->GetElement().FindAttributeAnyCase("dueDate")->GetStringValue(), MAX_BUG_DUE_DATE_CHARS);

								if (!strcmp(newBugInfo.cBugDueDate, "0000-00-00"))  // no valid due date
								{
									newBugInfo.cBugDueDate[0] = '\0';  // null the string
								}

								newBugInfo.bWithYouForMoreInfo = true;  

								m_BugInfo.PushAndGrow(newBugInfo);
							}
						}
					}

					//
					// get any DevBugs bugs:
					//
					pNodeDevBugs = pNodeUser->FindChildWithName("DevBugs", pNodeDevBugs);

					if (pNodeDevBugs)
					{
						parTreeNode* pNodeBug = NULL;

						while ((pNodeBug = pNodeDevBugs->FindChildWithName("Bug", pNodeBug)) != NULL)
						{
							// the list of bug class below are the only ones we want to deal with:
							if ((!strcmp(pNodeBug->GetElement().FindAttributeAnyCase("class")->GetStringValue(), "A")) ||
								(!strcmp(pNodeBug->GetElement().FindAttributeAnyCase("class")->GetStringValue(), "TODO")) ||
								(!strcmp(pNodeBug->GetElement().FindAttributeAnyCase("class")->GetStringValue(), "B")) ||
								(!strcmp(pNodeBug->GetElement().FindAttributeAnyCase("class")->GetStringValue(), "C")) ||
								(!strcmp(pNodeBug->GetElement().FindAttributeAnyCase("class")->GetStringValue(), "D")) ||
								(!strcmp(pNodeBug->GetElement().FindAttributeAnyCase("class")->GetStringValue(), "TASK")) ||
								(!strcmp(pNodeBug->GetElement().FindAttributeAnyCase("class")->GetStringValue(), "TRACK")) )
							{
								BugInfoStruct newBugInfo;

								newBugInfo.iBugNum = atoi(pNodeBug->GetElement().FindAttributeAnyCase("id")->GetStringValue());
								newBugInfo.iDaysOpen = 0;
								safecpy(newBugInfo.cBugSummary, pNodeBug->GetElement().FindAttributeAnyCase("summary")->GetStringValue(), MAX_BUG_SUMMARY_CHARS);
								safecpy(newBugInfo.cBugTags, pNodeBug->GetElement().FindAttributeAnyCase("tags")->GetStringValue(), MAX_BUG_TAGS_CHARS);
								safecpy(newBugInfo.cBugClass, pNodeBug->GetElement().FindAttributeAnyCase("class")->GetStringValue(), MAX_BUG_CLASS_CHARS);
								safecpy(newBugInfo.cBugOwnerQA, pNodeBug->GetElement().FindAttributeAnyCase("qaOwner")->GetStringValue(), MAX_BUG_QA_OWNER_CHARS);
								safecpy(newBugInfo.cBugDueDate, pNodeBug->GetElement().FindAttributeAnyCase("dueDate")->GetStringValue(), MAX_BUG_DUE_DATE_CHARS);

								if (!strcmp(newBugInfo.cBugDueDate, "0000-00-00"))  // no valid due date
								{
									newBugInfo.cBugDueDate[0] = '\0';  // null the string
								}

								newBugInfo.bWithYouForMoreInfo = false;  

								m_BugInfo.PushAndGrow(newBugInfo);
							}
						}
					}
				}

				m_UserInfo.PushAndGrow(newUserInfo);
			}
		}
	}

	std::sort(m_UserInfo.begin(), m_UserInfo.end(), &UserInfoStruct::compare);

	CalculateBugDisplayPriorities();

	m_loaded = true;

}


void CLoadingScreenBugList::ResetToDoNowBugList()
{
	USE_DEBUG_MEMORY();

	for(int i=0;i<TODO_NOW_MAX;i++)
	{
		atArray<BugInfoStruct> &buglist = GetToDoNowBugs(i);
		buglist.Reset();
	}

	m_ToDoNowTAGS.Reset();
	m_ToDoNowTAGBugs.Reset();
}

// Loads the textfile that describes which tags to show on the To Do Now Screen
void CLoadingScreenBugList::LoadTagsFile()
{
#define LOADSCREEN_TAGS_FILENAME	"common:/data/debug/loadScreenTAGS.txt"

	USE_DEBUG_MEMORY();

	m_ToDoNowTAGS.Reset();

	CFileMgr::SetDir("");
	FileHandle fid = INVALID_FILE_HANDLE;
	if (ASSET.Exists(LOADSCREEN_TAGS_FILENAME, ""))
	{ 
		fid = CFileMgr::OpenFile(LOADSCREEN_TAGS_FILENAME);
		if (CFileMgr::IsValidFileHandle(fid))
		{
			char* pLine;
			while( ((pLine = CFileMgr::ReadLine(fid)) != NULL) )
			{
				if( *pLine != 0 )
				{
					atString theTag(pLine);
					m_ToDoNowTAGS.Grow() = theTag;
				}
			}
		}
		CFileMgr::CloseFile(fid);
	}
}

void CLoadingScreenBugList::LoadToDoNow()
{
	USE_DEBUG_MEMORY();

	// Now do the HTTP queries for the "To Do Now" bugs
	char query[256];
	char filename[RAGE_MAX_PATH];

	for(int i=0;i<TODO_NOW_MAX;i++)
	{
		CLoadingScreenBugList::ToDoNowInfoData &info = gBugList.GetToDoNowInfoData(i);

		sprintf(query,"%s%s%s",BUGSTAR_GTA_PROJECT_PATH,m_userName,info.pSearchString);
		sprintf(filename,"%s%s.xml", info.pFilenameString, m_userName);
		parTree* pTree = CBugstarIntegration::GetSavedBugstarRequestList().LoadBugstarRequest(filename);
		CBugstarIntegration::GetSavedBugstarRequestList().Get(filename, query, 60, 32*1024);
		if(pTree)
		{
			atArray<BugInfoStruct> &theBugList = gBugList.GetToDoNowBugs(i);
			UpdateToDoNowBugList(pTree, theBugList);
			delete pTree;
			// Now sort
			std::sort(theBugList.begin(), theBugList.end(), &BugInfoStruct::SortByDaysOpen);
		}
	}

	// Now load & parse the loadScreenTAGS.txt file
	LoadTagsFile();
	// Resize the array to account for the TAGGED bugs, 1 for each TAG
	m_ToDoNowTAGBugs.ResizeGrow(m_ToDoNowTAGS.size());

	// Do a search for each tag defined
	for(int i=0;i<m_ToDoNowTAGS.size();i++)
	{
		//https://rest.bugstar.rockstargames.com:8443/BugstarRestService-1.0/rest/Projects/1546/<USERNAME>/TaggedBugs/<TAGNAME>
		sprintf(query,"%s%s/TaggedBugs/%s",BUGSTAR_GTA_PROJECT_PATH,m_userName, m_ToDoNowTAGS[i].c_str());
		sprintf(filename,"%s_%s.xml", m_ToDoNowTAGS[i].c_str(), m_userName);
		parTree* pTree = CBugstarIntegration::GetSavedBugstarRequestList().LoadBugstarRequest(filename);
		CBugstarIntegration::GetSavedBugstarRequestList().Get(filename, query, 60, 32*1024);
		if(pTree)
		{
			atArray<BugInfoStruct> &theBugList = m_ToDoNowTAGBugs[i];
			UpdateToDoNowBugList(pTree, theBugList);
			delete pTree;
			// Now sort
			std::sort(theBugList.begin(), theBugList.end(), &BugInfoStruct::SortByDaysOpen);
		}
	}

}

// Add the bugs to our list of bugs, check if it's a dupe since we're merging several queries
void CLoadingScreenBugList::UpdateToDoNowBugList(parTree* pTree, atArray<BugInfoStruct> &theBuglist)
{
	USE_DEBUG_MEMORY();

	if (pTree)
	{
		parTreeNode* root = pTree->GetRoot()->GetChild();
		if(root)
		{
			parTreeNode::ChildNodeIterator i = root->BeginChildren();
			for(; i != root->EndChildren(); ++i)
			{
				parTreeNode* id = (*i)->FindFromXPath("Id");			// Bug num
				
				if(id)
				{
					u32 bugNum = (u32)atoi((char*)id->GetData());
					BugInfoStruct newBugInfo;

					newBugInfo.iBugNum = bugNum;

					parTreeNode *sum = (*i)->FindFromXPath("Summary");		// Summary
					parTreeNode *days = (*i)->FindFromXPath("DaysOpen");	// Days open

					if(sum)
					{
						safecpy(newBugInfo.cBugSummary, sum->GetData(),MAX_BUG_SUMMARY_CHARS);
					}
					if(days)
					{
						newBugInfo.iDaysOpen = atoi((char*)days->GetData());
					}
					theBuglist.PushAndGrow(newBugInfo);
				}
			}
		}
	}
}

void CLoadingScreenBugList::CalculateBugDisplayPriorities()
{
	USE_DEBUG_MEMORY();

	//
	// Get what we consider as "todays date"
	//
	s32 iTodaysDateDay = 0;
	s32	iTodaysDateMonth = 0;
	s32 iTodaysDateYear = 0;

	time_t rawtime;
	struct tm * pTimeInfo;
	time ( &rawtime );
	pTimeInfo = localtime ( &rawtime );

	if (pTimeInfo)
	{
		iTodaysDateDay = pTimeInfo->tm_mday;
		iTodaysDateMonth = pTimeInfo->tm_mon + 1;
		iTodaysDateYear = pTimeInfo->tm_year + 1900;
	}
	Displayf("TODAYS DATE IS: %02d-%02d-%04d", iTodaysDateDay, iTodaysDateMonth, iTodaysDateYear);

	for(int i=0; i<m_BugInfo.GetCount(); i++)
	{
		int iDueDateDay = 0;
		int iDueDateMonth = 0;
		int iDueDateYear = 9999;
		if (m_BugInfo[i].cBugDueDate[0] != '\0')  // check we actually have a due date
			sscanf(m_BugInfo[i].cBugDueDate, "%d-%d-%d", &iDueDateYear, &iDueDateMonth, &iDueDateDay);

		if(stristr(m_BugInfo[i].cBugTags, "[PT]"))
			m_BugInfo[i].displayPriority = BugInfoStruct::PT_BUG;
		else if(stristr(m_BugInfo[i].cBugTags, "[DISCUSS]"))
			m_BugInfo[i].displayPriority = BugInfoStruct::DISCUSS_BUG;
		else if(m_BugInfo[i].bWithYouForMoreInfo)
			m_BugInfo[i].displayPriority = BugInfoStruct::MORE_INFO;
		else if	((iTodaysDateYear > iDueDateYear) ||
			(iTodaysDateMonth > iDueDateMonth && iTodaysDateYear == iDueDateYear) ||
			(iTodaysDateDay > iDueDateDay && iTodaysDateMonth == iDueDateMonth && iTodaysDateYear == iDueDateYear))
			m_BugInfo[i].displayPriority = BugInfoStruct::OVERDUE;
		else if(!strcmp(m_BugInfo[i].cBugClass, "A"))
			m_BugInfo[i].displayPriority = BugInfoStruct::A;
		else if(!strcmp(m_BugInfo[i].cBugClass, "B"))
			m_BugInfo[i].displayPriority = BugInfoStruct::B;
		else if(!strcmp(m_BugInfo[i].cBugClass, "C"))
			m_BugInfo[i].displayPriority = BugInfoStruct::C;
		else if(!strcmp(m_BugInfo[i].cBugClass, "D"))
			m_BugInfo[i].displayPriority = BugInfoStruct::D;
		else if(!strcmp(m_BugInfo[i].cBugClass, "TODO"))
			m_BugInfo[i].displayPriority = BugInfoStruct::TODO;
		else if(!strcmp(m_BugInfo[i].cBugClass, "TASK"))
			m_BugInfo[i].displayPriority = BugInfoStruct::TASK;
		else if(!strcmp(m_BugInfo[i].cBugClass, "TRACK"))
			m_BugInfo[i].displayPriority = BugInfoStruct::TRACK;
		else
			m_BugInfo[i].displayPriority = BugInfoStruct::UNDEFINED;
	}

	std::sort(m_BugInfo.begin(), m_BugInfo.end(), &BugInfoStruct::SortByDisplayPriority);
}

#define STICK_THRESHOLD	(100)

void CLoadingScreens::Bank_UpdateBugScreen()
{
	if ( IsLoadingMovieActive() && (ms_CurrentMovieIndex==LOADINGCREEN_MOVIE_INDEX_STARTUP_DEBUG) && (!AreSuspended()) )
	{
		bool bUpdateTextDisplay = false;

		// * 128.0f to convert to old input value ranges.
		s32 XAxis = static_cast<s32>(CControlMgr::GetMainFrontendControl().GetFrontendLeftRight().GetNorm() * 128.0f);
		s32 YAxis = static_cast<s32>(CControlMgr::GetMainFrontendControl().GetFrontendUpDown().GetNorm() * 128.0f);

		CPad *pPad = CControlMgr::GetPlayerPad();

		//if( pPad && pPad->GetButtonCross() )
		//{
		//	CNetwork::SetGoStraightToMultiplayer(true);
		//}

		if (gBugList.HasLoaded())
		{
			if((pPad && pPad->DPadUpJustDown()) || YAxis < -STICK_THRESHOLD)
			{
				if (ms_LoadingScreenMovie.BeginMethod("SET_INPUT_EVENT"))
				{
					CScaleformMgr::AddParamInt(8);
					CScaleformMgr::EndMethod();
				}
			}
			if((pPad && pPad->DPadDownJustDown()) || YAxis > STICK_THRESHOLD)
			{
				if (ms_LoadingScreenMovie.BeginMethod("SET_INPUT_EVENT"))
				{
					CScaleformMgr::AddParamInt(9);
					CScaleformMgr::EndMethod();
				}
			}
			if((pPad && pPad->DPadRightJustDown()) || XAxis > STICK_THRESHOLD)
			{
				if (ms_iCurrentUser < gBugList.GetUserCount()-1)
					ms_iCurrentUser++;

				bUpdateTextDisplay = true;
			}
			if((pPad && pPad->DPadLeftJustDown()) || XAxis < -STICK_THRESHOLD)
			{
				if (ms_iCurrentUser > 0)
					ms_iCurrentUser--;

				bUpdateTextDisplay = true;
			}

			if(bUpdateTextDisplay)
				Bank_UpdateSummaryText();
		}

		//
		// display a progress bar
		//
		if (ms_iPreviousLoadingTime > 0 && ms_iCurrentLoadingTime > 0)
		{
			s32 iCurrentTimeMins = 0;
			s32 iPreviousTimeMins = 0;
			s32 iCurrentTimeSecs = ms_iCurrentLoadingTime/1000;
			s32 iPreviousTimeSecs = ms_iPreviousLoadingTime/1000;

			while (iCurrentTimeSecs >= 60)
			{
				iCurrentTimeSecs -= 60;
				iCurrentTimeMins++;
			}

			while (iPreviousTimeSecs >= 60)
			{
				iPreviousTimeSecs -= 60;
				iPreviousTimeMins++;
			}

			char cProgressText[256];
			static char cPrevProgressText[256];

			static float fPrevPercentage = -1.0f;
			float fPercentage = floorf(((float)ms_iCurrentLoadingTime / (float)ms_iPreviousLoadingTime) * 100.0f);

			if (fPercentage <= 100.0f)
			{
				if (fPercentage != fPrevPercentage)
				{
					if (ms_LoadingScreenMovie.BeginMethod("SET_PROGRESS_BAR"))
					{
						CScaleformMgr::AddParamFloat(fPercentage);
						CScaleformMgr::EndMethod();
					}
				}
			}
			else
			{
				fPercentage = 100.0f;
			}

			fPrevPercentage = fPercentage;

#if __ENABLE_RFS
			if (fiIsShowingMessageBox)
				safecpy(cProgressText,"PAUSED ON MESSAGE BOX");
			else
#endif
			if( CNetwork::GetGoStraightToMultiplayer() )
			{
				formatf(cProgressText, "[  Multiplayer  ] Loading %0.0f%% %d:%02d / %d:%02d", fPercentage, iCurrentTimeMins, iCurrentTimeSecs, iPreviousTimeMins, iPreviousTimeSecs);
			}
			else
			{
#if __PS3
				formatf(cProgressText, "[X - Multiplayer] Loading %0.0f%% %d:%02d / %d:%02d", fPercentage, iCurrentTimeMins, iCurrentTimeSecs, iPreviousTimeMins, iPreviousTimeSecs);
#else
				formatf(cProgressText, "[A - Multiplayer] Loading %0.0f%% %d:%02d / %d:%02d", fPercentage, iCurrentTimeMins, iCurrentTimeSecs, iPreviousTimeMins, iPreviousTimeSecs);
#endif
			}

#if __ASSERT
			const char* cNoPopupsMessage = CDebug::GetNoPopupsMessage(true);
			
			if (cNoPopupsMessage)
			{
				safecat(cProgressText, " - ");
				safecat(cProgressText, cNoPopupsMessage);
			}
#endif // __ASSERT

			if (strcmp(cProgressText, cPrevProgressText))
			{
				if (ms_LoadingScreenMovie.BeginMethod("SET_PROGRESS_TEXT"))
				{
					CScaleformMgr::AddParamString(cProgressText);
					CScaleformMgr::EndMethod();
				} 

				safecpy(cPrevProgressText, cProgressText);
			}
		}
	}
}


void CLoadingScreens::Bank_AddToDoNowBugs(const char *pTitle, atArray<BugInfoStruct> &theBuglist )
{

	// Don't add a title if there are no bugs
	if( theBuglist.size() )
	{
		// Title
		if (ms_LoadingScreenMovie.BeginMethod("ADD_DO_NOW_ROW"))
		{
			CScaleformMgr::AddParamInt(ms_LineCount);
			CScaleformMgr::AddParamInt(-99);				// -ve bug number means "Sub Heading"
			CScaleformMgr::AddParamString(pTitle, false);
			CScaleformMgr::AddParamInt(0);
			CScaleformMgr::EndMethod();
			ms_LineCount++;
		}

		// Bugs
		for(int i=0; i<theBuglist.size(); i++)
		{
			if (ms_LoadingScreenMovie.BeginMethod("ADD_DO_NOW_ROW"))
			{
				CScaleformMgr::AddParamInt(ms_LineCount);
				CScaleformMgr::AddParamInt(theBuglist[i].iBugNum);
				CScaleformMgr::AddParamString(theBuglist[i].cBugSummary, false);  // do not modify the string as it may contain tokens in the bug like in 463591
				CScaleformMgr::AddParamInt(theBuglist[i].iDaysOpen);
				CScaleformMgr::EndMethod();
				ms_LineCount++;
			}
		}
	}
}

// Parse the bugs looking for TRACK bugs
void CLoadingScreens::Bank_AddToDoNowTRACKBugs()
{
	if(gBugList.GetBugCount())
	{
		atArray<BugInfoStruct> trackBugs;

		for(int i=0;i<gBugList.GetBugCount();i++)
		{
			const BugInfoStruct &theBug = gBugList.GetBug(i);
			if(strnicmp(theBug.cBugClass, "TRACK", strlen("TRACK")) == 0 )
			{
				trackBugs.Grow() = theBug;
			}
		}

		std::sort(trackBugs.begin(), trackBugs.end(), &BugInfoStruct::SortByDaysOpen);
		Bank_AddToDoNowBugs("TRACK Bugs", trackBugs);
	}
}

void CLoadingScreens::Bank_SetupScaleformFromBugstarData()
{
	//
	// process the data we got from the xml:
	//
	if (gBugList.HasLoaded())
	{
		//
		// go through and insert all bugs in a certain order:
		//
		s32 iSlotNum = 0;
		u32 iLastBugEntered = 0;  // this is to mask over the fact Bugstar can pass the same bug twice to code.
		for(int i=0; i<gBugList.GetBugCount(); i++)
		{
			if (gBugList.GetBug(i).iBugNum == iLastBugEntered)  // if we try to enter the same bug twice in succession, we ignore and only add 1 bug
				continue;

			if (ms_LoadingScreenMovie.BeginMethod("SET_DATA_SLOT"))
			{
				CScaleformMgr::AddParamInt(iSlotNum);
				CScaleformMgr::AddParamInt(gBugList.GetBug(i).iBugNum);
				CScaleformMgr::AddParamInt((gBugList.GetBug(i).displayPriority <= BugInfoStruct::OVERDUE) ? 1 : 0);
				CScaleformMgr::AddParamString(gBugList.GetBug(i).cBugSummary, false);  // do not modify the string as it may contain tokens in the bug like in 463591
				CScaleformMgr::AddParamString(gBugList.GetBug(i).cBugTags);
				CScaleformMgr::AddParamString(gBugList.GetBug(i).cBugClass);
				CScaleformMgr::AddParamString(gBugList.GetBug(i).cBugDueDate);
				CScaleformMgr::AddParamString(gBugList.GetBug(i).cBugOwnerQA);
				CScaleformMgr::EndMethod();
			}

			iLastBugEntered = gBugList.GetBug(i).iBugNum;

			iSlotNum++;
		}

		for(int i=0;i<CLoadingScreenBugList::TODO_NOW_MAX;i++)
		{
			CLoadingScreenBugList::ToDoNowInfoData &info = gBugList.GetToDoNowInfoData(i);

			Bank_AddToDoNowBugs(info.pTitle, gBugList.GetToDoNowBugs(i));
		}

		// Add the ToDoNowTAG bugs
		atArray< atArray<BugInfoStruct> >	&toDoNowTagBugs = gBugList.GetToDoNowTAGBugs();
		for(int i=0;i<toDoNowTagBugs.size();i++)
		{
			atArray<BugInfoStruct> &theBugList = toDoNowTagBugs[i];
			if( theBugList.size() )
			{
				Bank_AddToDoNowBugs(gBugList.GetToDoNowTAGS(i).c_str(), theBugList);
			}
		}

		// Add bugs with the class "TRACKED"
		Bank_AddToDoNowTRACKBugs();

		Bank_UpdateSummaryText();
	}

	if(	ms_LoadingScreenMovie.BeginMethod("DRAW_MENU_LIST"))
	{
		CScaleformMgr::AddParamInt(0);
		CScaleformMgr::EndMethod();
	}
	if(	ms_LoadingScreenMovie.BeginMethod("DRAW_MENU_LIST"))
	{
		CScaleformMgr::AddParamInt(1);
		CScaleformMgr::EndMethod();
	}

	gBugList.ResetBugList();
	gBugList.ResetToDoNowBugList();
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::Bank_UpdateSummaryText()
// PURPOSE:	sends the summary text into the scaleform movie
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::Bank_UpdateSummaryText()
{
	//
	// setup the movie with this data:
	//
	char cFullReport[400];

	char cProjectReport[200];
	char cUserReport[200];

	sprintf(cProjectReport, "Project : Todos-%d, Tasks-%d, A-%d, B-%d, C-%d, D-%d, TOTAL - %d",
		gBugList.GetTotalBugs().todos,
		gBugList.GetTotalBugs().tasks,
		gBugList.GetTotalBugs().a, 
		gBugList.GetTotalBugs().b, 
		gBugList.GetTotalBugs().c,
		gBugList.GetTotalBugs().d,
		gBugList.GetTotalBugs().total);

	if (ms_iCurrentUser >= 0 && ms_iCurrentUser < gBugList.GetUserCount())
	{
		sprintf(cUserReport, "%s : Todos-%d, Tasks-%d, A-%d, B-%d, C-%d, D-%d, TOTAL - %d",
			gBugList.GetUser(ms_iCurrentUser).username,
			gBugList.GetUser(ms_iCurrentUser).bugs.todos,
			gBugList.GetUser(ms_iCurrentUser).bugs.tasks,
			gBugList.GetUser(ms_iCurrentUser).bugs.a, 
			gBugList.GetUser(ms_iCurrentUser).bugs.b, 
			gBugList.GetUser(ms_iCurrentUser).bugs.c,
			gBugList.GetUser(ms_iCurrentUser).bugs.d,
			gBugList.GetUser(ms_iCurrentUser).bugs.total);
	}
	else
	{
		sprintf(cUserReport, "user unknown");
	}

	sprintf(cFullReport, "%s\n%s", cProjectReport, cUserReport);

	if (ms_LoadingScreenMovie.BeginMethod("SET_TEXT"))
	{
		CScaleformMgr::AddParamString(cFullReport);
		CScaleformMgr::EndMethod();
	} 
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::Bank_LoadTimeFromDataFile()
// PURPOSE:	loads the last time it took to load from the data file
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::Bank_LoadTimeFromDataFile()
{
	CFileMgr::SetDir("");

	// new episodic code to read in a new credits.dat file for the episodes instead of the main game one:
	FileHandle fid = CFileMgr::OpenFile("common:/DATA/debug/loadingtime.dat", "rb");
	char* pLine;

	int	dataID = 0;		// Which bit of data we're loading

	if (CFileMgr::IsValidFileHandle(fid))
	{
		while( ((pLine = CFileMgr::ReadLine(fid)) != NULL) )
		{
			if(*pLine == '#' || *pLine == '\0')
				continue;

			if( dataID == 0 )
			{
				sscanf(pLine, "%*s %d", &ms_iPreviousLoadingTime);
			}
			else if(dataID == 1)
			{
				sscanf(pLine, "%*s %d", &ms_iLoadingStartScreen);
			}
			dataID++;
		}
		CFileMgr::CloseFile(fid);
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CLoadingScreens::Bank_SaveTimeToDataFile()
// PURPOSE:	saves the time out to a data file to load in next time
/////////////////////////////////////////////////////////////////////////////////////
void CLoadingScreens::Bank_SaveTimeToDataFile()
{
	if(!sysBootManager::IsBootedFromDisc() && !fiDeviceInstaller::GetIsBootedFromHdd())
	{
		CFileMgr::SetDir("");

		// new episodic code to read in a new credits.dat file for the episodes instead of the main game one:	
		FileHandle fid = CFileMgr::OpenFileForWriting("common:/DATA/debug/loadingtime.dat");

		if (CFileMgr::IsValidFileHandle(fid))
		{
			char tempText[128];
			sprintf(tempText,  "LOADING_TIME: %d\n", ms_iCurrentLoadingTime);
			CFileMgr::Write(fid, tempText, istrlen(tempText));
			ms_iLoadingStartScreen++;
			ms_iLoadingStartScreen %= NUM_LOADING_START_SCREENS;
			sprintf(tempText,  "START_SCREEN: %d\n", ms_iLoadingStartScreen);
			CFileMgr::Write(fid, tempText, istrlen(tempText));
			CFileMgr::CloseFile(fid);
		}
	}
}

#endif // __BANK

#if !__FINAL
void CLoadingScreens::NonFinal_UpdateStatusText()
{
	//if ( (CScaleformMgr::IsMovieActive(ms_iLoadingMovieId)) &&(ms_CurrentMovieIndex==LOADINGCREEN_MOVIE_INDEX_STARTUP_DEBUG) && (!AreSuspended()) )
	if ( ms_LoadingScreenMovie.IsActive() && !AreSuspended() )
	{
#if GEN9_LANDING_PAGE_ENABLED
        // Skip from initial landing, otherwise we see this over the news
        if( CLandingPage::ShouldShowOnBoot() )
        {
            return;
        }
#endif

		//
		// display a progress bar
		//
		if (ms_iCurrentLoadingTime > 0)
		{
			s32 iCurrentTimeMins = ms_iCurrentLoadingTime/60000;
			s32 iCurrentTimeSecs = (ms_iCurrentLoadingTime/1000)%60;

			char cProgressText[256];
			static char cPrevProgressText[256];

#if __ENABLE_RFS
			if (fiIsShowingMessageBox)
				safecpy(cProgressText,"PAUSED ON MESSAGE BOX");
			else
#endif
			{
#if RAGE_TIMEBARS
				pfTimeBars::sFuncTime* timeBarsFuncTime = ::rage::g_pfTB.GetStartupBar().GetCurrentFunc();
#endif //RAGE_TIMEBARS
				if (CTheScripts::GetScriptLaunched())
				{
					formatf(cProgressText, "Running Scripts ... (%d:%02d)", iCurrentTimeMins, iCurrentTimeSecs);
				}
#if RAGE_TIMEBARS
				else if (timeBarsFuncTime != NULL)
				{
					formatf(cProgressText, "Loading : %s (%d:%02d)", timeBarsFuncTime->nameptr, iCurrentTimeMins, iCurrentTimeSecs);
				}
#endif //RAGE_TIMEBARS
				else
				{
					formatf(cProgressText, "Loading... %d:%02d", iCurrentTimeMins, iCurrentTimeSecs);
				}

				if (fwAssetStoreBase::GetProgressText()[0] != 0)
				{
					safecpy(cProgressText, fwAssetStoreBase::GetProgressText());
				}

#if __ASSERT
				const char* cNoPopupsMessage = CDebug::GetNoPopupsMessage(true);

				if (cNoPopupsMessage)
				{
					safecat(cProgressText, " - ");
					safecat(cProgressText, cNoPopupsMessage);
				}
#endif // __ASSERT

				if (strcmp(cProgressText, cPrevProgressText))
				{
					if (ms_LoadingScreenMovie.BeginMethod("SET_PROGRESS_TEXT"))
					{
						CScaleformMgr::AddParamString(cProgressText);
						CScaleformMgr::EndMethod();
					} 

					safecpy(cPrevProgressText, cProgressText);
				}
			}
		}
	}
}

#endif // !__FINAL

#if UI_LANDING_PAGE_ENABLED

void CLoadingScreens::GoToLanding()
{
	SetLoadingContext(LOADINGSCREEN_CONTEXT_LANDING);
	SetScreenOrder(GetLoadingSP());
	SetMovieContext(LOADINGSCREEN_SCALEFORM_CTX_LANDING);

	// Make sure there aren't any loading screen buttons set
	ResetButtons();
	UpdateDisplayConfig();
}

void CLoadingScreens::ExitLanding()
{
	SetLoadingContext(LOADINGSCREEN_CONTEXT_LOADGAME);
	SetMovieContext(LOADINGSCREEN_SCALEFORM_CTX_GAMELOAD_NEWS);
	InitUpdateLoadGame(true);

	// If the Game is checking entitlement,do not automatically commit to MpSp.  We'll be back :)
#if RSG_ENTITLEMENT_ENABLED
	if (!CGame::IsCheckingEntitlement())
#endif // RSG_ENTITLEMENT_ENABLED
	{
		CommitToMpSp();
	}
}

void CLoadingScreens::DeviceReset()
{
	if (ms_LoadingScreenMovie.GetMovieID() != INVALID_MOVIE_ID)
	{
		CLoadingScreens::UpdateDisplayConfig();
		CScaleformMgr::UpdateMovieParams(ms_LoadingScreenMovie.GetMovieID(), CScaleformMgr::GetRequiredScaleMode(ms_LoadingScreenMovie.GetMovieID(), true));
	}
}

#endif // UI_LANDING_PAGE_ENABLED

// eof
