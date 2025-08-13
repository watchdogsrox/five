//
// Facebook.h
//
// Copyright (C) 2013 Rockstar Games.  All Rights Reserved.
//
// Exposes facebook posting functionality to the game via rage.
// url:bugstar:982616 Integrate Rage facebook sharing into game.

#include "Facebook.h"

#if RL_FACEBOOK_ENABLED

#include "string.h"
#include "optimisations.h"
#include "fwsys/gameskeleton.h"
#include "Network/NetworkInterface.h"
#include "Network/Network.h"
#include "network/Commerce/CommerceManager.h"
#include "network/Live/livemanager.h"
#include "net/http.h"
#include "rline/rl.h"
#include "rline/ros/rlros.h"
#include "rline/rltitleid.h"
#include "frontend/ProfileSettings.h"
#include "frontend/BusySpinner.h"
#include "frontend/GameStreamMgr.h"
#include "peds/ped.h"

#if __BANK
#include "bank/bank.h"
#endif

NETWORK_OPTIMISATIONS()

#define FACEBOOK_SUCCESSFUL_MSG_DURATION 2000
#define APP_TOKEN_PRIVELEGES "publish_actions"

RAGE_DEFINE_SUBCHANNEL(net, facebook, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_facebook

namespace rage
{
	extern const rlTitleId* g_rlTitleId;
}

fbkMilestone FacebookMilestones[]={
	
	{FACEBOOK_MILESTONE_CHECKLIST,	"percentcomplete" },	// 100% Complete game.
	{FACEBOOK_MILESTONE_OVERVIEW,	"storycomplete" },	    // 100% Complete story mission.
	{FACEBOOK_MILESTONE_VEHICLES,	"vehicles" },	        // Driven all cars.
	{FACEBOOK_MILESTONE_PROPERTIES,	"properties" },	        // Bought all properties.
	{FACEBOOK_MILESTONE_PSYCH,		"psych" },		        // Future: psych report (TBD)
	{FACEBOOK_MILESTONE_MAPREVEAL,	"mapreveal"},			// Map revealed.
	{FACEBOOK_MILESTONE_PROLOGUE,	"prologue"}				// Prologue done.
};
CompileTimeAssert( NELEM(FacebookMilestones) == FACEBOOK_MILESTONE_COUNT );

// ---------------------------------------------------------
CFacebook::CFacebook()
: m_StartPhase(FACEBOOK_PHASE_IDLE)
, m_Phase(FACEBOOK_PHASE_IDLE)
, m_LastError(FACEBOOK_ERROR_OK)
, m_LastResult(0)
, m_LastOnlineState(false)
, m_LastSocialSharingPrivileges(false)
, m_FacebookSuccessTimer(0)
, m_uDelayStartTime(0)
#if RSG_PC
, m_bAllowFacebookKillswitch(false)
#endif
{
}

// ---------------------------------------------------------
CFacebook::~CFacebook()
{
}

// ---------------------------------------------------------
void CFacebook::Init()
{
	m_Status.Reset();
	m_PostId[0] = '\0';

	// UpdateOnlineStateIdle() silently checks for an access token in the update.
}

// ---------------------------------------------------------
void CFacebook::Shutdown()
{
}

// ---------------------------------------------------------
void CFacebook::Update()
{
	char* extraParams;
	const char* gameCharacter;
	switch( m_Phase )
	{
		case FACEBOOK_PHASE_IDLE:
			{
				UpdateOnlineStateIdle();
			}
			break;
		// Attempt to obtain a token for the user silently after a sign in.
		case FACEBOOK_PHASE_GETACCCESSTOKEN_SILENT_INIT:
			if(IsOnline() && HasSocialClubAccount() && CLiveManager::GetSocialNetworkingSharingPrivileges() && (IsProfileSettingEnabled() || HasProfileSettingHint()))
			{
				OutputProfileSettings();
				const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
				if(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
				{
					m_Status.Reset();

					if( rlFacebook::GetPlatformAccessTokenNoGui(localGamerIndex, &m_Status))
					{
						SetPhase(FACEBOOK_PHASE_GETACCCESSTOKEN_SILENT_WAIT);
					}
					else
					{
						SetPhase(FACEBOOK_PHASE_CLEANUP);
					}
				}
				else
				{
					SetPhase(FACEBOOK_PHASE_CLEANUP);
				}
			}
			else
			{
				OutputProfileSettings();
				SetPhase(FACEBOOK_PHASE_CLEANUP);
			}
			break;
		case FACEBOOK_PHASE_GETACCCESSTOKEN_SILENT_WAIT:
			if( !m_Status.Pending() )
			{
				SetPhase(FACEBOOK_PHASE_CLEANUP);
			}
			break;
		// Attempt to obtain an access token with the GUI ( Pause menu )
		case FACEBOOK_PHASE_GETACCCESSTOKEN_INIT:
			if(IsOnline() && HasSocialClubAccount() && CLiveManager::GetSocialNetworkingSharingPrivileges())
			{
				const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
				if(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
				{
					m_Status.Reset();

#if RSG_DURANGO || RSG_PC
					CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "FB_LINK_HEAD", "FB_LINK_BEFORE", FE_WARNING_OK_CANCEL);
					eWarningButtonFlags result = CWarningScreen::CheckAllInput();
					if(result == FE_WARNING_OK || result == FE_WARNING_YES)
					{
						if(rlFacebook::GetPlatformAccessTokenWithGui(localGamerIndex, &m_Status))
						{
							m_uDelayStartTime = sysTimer::GetSystemMsTime();
							SetPhase(FACEBOOK_PHASE_GETACCESSTOKEN_USER_INPUT_AFTER_DELAY);
						}
						else
						{
							gnetDebug1("FACEBOOK_PHASE_GETACCESSTOKEN_USER_INPUT_AFTER :: Confirmed");
							SetPhase(FACEBOOK_PHASE_CLEANUP);
							CWarningScreen::Remove();
						}
					}
					else if (result == FE_WARNING_NO || result == FE_WARNING_CANCEL)
					{
						gnetDebug1("FACEBOOK_PHASE_GETACCCESSTOKEN_INIT :: Cancelled");
						CWarningScreen::Remove();
						SetPhase(FACEBOOK_PHASE_CLEANUP);
					}
#else
					if( rlFacebook::GetPlatformAccessTokenWithGui(localGamerIndex, &m_Status) )
					{
						SetPhase(FACEBOOK_PHASE_GETACCESSTOKEN_WAIT);
					}
					else
					{
						m_LastError = FACEBOOK_ERROR_GETACESSTOKEN;
						SetPhase(FACEBOOK_PHASE_CLEANUP);
					}
#endif
				}
				else
				{
					m_LastError = FACEBOOK_ERROR_GETACESSTOKEN;
					SetPhase(FACEBOOK_PHASE_CLEANUP);
				}
			}
			else
			{
				m_LastError = FACEBOOK_ERROR_GETACESSTOKEN;
				SetPhase(FACEBOOK_PHASE_CLEANUP);
			}
			break;
		case FACEBOOK_PHASE_GETACCESSTOKEN_USER_INPUT_AFTER_DELAY:
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "FB_LINK_HEAD", "FB_BUSY", FE_WARNING_SPINNER);
			if (sysTimer::HasElapsedIntervalMs(m_uDelayStartTime, 3000)) // 3s
			{
				if (HasAccessToken())
				{
					CWarningScreen::Remove();
					SetPhase(FACEBOOK_PHASE_CLEANUP);
					SetProfileSettingHint(true);
				}
				else
				{
					SetPhase(FACEBOOK_PHASE_GETACCESSTOKEN_USER_INPUT_AFTER);
				}
			}
 			break;
		case FACEBOOK_PHASE_GETACCESSTOKEN_USER_INPUT_AFTER:
			{
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "FB_LINK_HEAD", "FB_LINK_AFTER", FE_WARNING_OK);
				eWarningButtonFlags result = CWarningScreen::CheckAllInput();
				if(result == FE_WARNING_OK || result == FE_WARNING_YES)
				{
					gnetDebug1("FACEBOOK_PHASE_GETACCESSTOKEN_USER_INPUT_AFTER :: Confirmed");
					const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
					if(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
					{
						if (rlFacebook::HaveAppPermissions(NetworkInterface::GetLocalGamerIndex(), &m_Status))
						{
							m_uDelayStartTime = sysTimer::GetSystemMsTime();
							SetPhase(FACEBOOK_PHASE_GETACCESSTOKEN_WAIT);
						}
						else
						{
							CWarningScreen::Remove();
							SetPhase(FACEBOOK_PHASE_CLEANUP);
						}
					}
					else
					{
						CWarningScreen::Remove();
						SetPhase(FACEBOOK_PHASE_CLEANUP);
					}
				}
				else if (result == FE_WARNING_NO)
				{
					gnetDebug1("FACEBOOK_PHASE_GETACCESSTOKEN_USER_INPUT_AFTER :: Cancelled");
					CWarningScreen::Remove();
					SetPhase(FACEBOOK_PHASE_CLEANUP);
				}
			}
			break;
		case FACEBOOK_PHASE_GETACCESSTOKEN_WAIT:
#if RSG_DURANGO || RSG_PC
			if(m_Status.Pending() || !sysTimer::HasElapsedIntervalMs(m_uDelayStartTime, 1000)) // minimum 1 second to make sure we dont flash UI
			{
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "FB_LINK_HEAD", "FB_BUSY", FE_WARNING_SPINNER);
			}
			else
			{
				CWarningScreen::Remove();
#else
			if(!m_Status.Pending())
			{
#endif
				if( m_Status.Succeeded() )
				{
					SetPhase(FACEBOOK_PHASE_CLEANUP);
				}
				else
				{
					m_LastResult = m_Status.GetResultCode();
					m_LastError = FACEBOOK_ERROR_GETACESSTOKEN;
					SetPhase(FACEBOOK_PHASE_CLEANUP);
				}
			}
			break;
		// Post to facebook.
		case FACEBOOK_PHASE_POST_INIT:
			if( CanReportToFacebook() )
			{
				EnableBusySpinner("FACEBOOK_SENDING");

				m_PostId[0] = '\0';
				extraParams = m_PostObjExtraParams;
				if( extraParams[0] == '\0' )
				{
					extraParams = NULL;
				}

				if( rlFacebook::PostOpenGraph(NetworkInterface::GetLocalGamerIndex(),m_PostAction,m_PostObjName,m_PostObjTargetData,extraParams,m_PostId,FACEBOOK_POST_ID_LEN,&m_Status) )
				{
					SetPhase(FACEBOOK_PHASE_POST_WAIT);
				}
				else
				{
					m_LastError = FACEBOOK_ERROR_POST;
					SetPhase(FACEBOOK_PHASE_CLEANUP);
				}
			}
			break;
		case FACEBOOK_PHASE_PRE_CLEANUP:
			if(fwTimer::GetSystemTimeInMilliseconds() > m_FacebookSuccessTimer + FACEBOOK_SUCCESSFUL_MSG_DURATION)
			{
				m_FacebookSuccessTimer = 0;
				SetPhase(FACEBOOK_PHASE_CLEANUP);
			}
			break;
		case FACEBOOK_PHASE_POST_WAIT:
			if( !m_Status.Pending() )
			{
				if( m_Status.Succeeded() )
				{
					EnableBusySpinner("FACEBOOK_SUCCESSFUL");
					m_FacebookSuccessTimer = fwTimer::GetSystemTimeInMilliseconds();

					SetPhase(FACEBOOK_PHASE_PRE_CLEANUP);
				}
				else
				{
					m_LastResult = m_Status.GetResultCode();
					m_LastError = FACEBOOK_ERROR_POST;
					SetPhase(FACEBOOK_PHASE_CLEANUP);
				}
			}
			break;
		case FACEBOOK_PHASE_CLEANUP:
			{
				// Wait for the game session state machine to stop loading
				if (!CGameSessionStateMachine::IsIdle())
				{
					break;
				}

				DisableBusySpinner();

				// set the hint for next time
				SetProfileSettingHint(HasAccessToken());

				// Check if we should report to facebook that we bought the game. ( Only after a link )
				if(  m_StartPhase==FACEBOOK_PHASE_GETACCCESSTOKEN_SILENT_INIT ||  m_StartPhase==FACEBOOK_PHASE_GETACCCESSTOKEN_INIT )
				{
					TryPostBoughtGame();
				}
				else
				{
					SetPhase(FACEBOOK_PHASE_IDLE);
				}
			}
			break;
		// Tell the world you bought GTAV
		case FACEBOOK_PHASE_POST_BOUGHT_GTA_INIT:
			gameCharacter = GetPlayerCharacter();
			if( gameCharacter != NULL )
			{
				if( rlFacebook::PostOpenGraph(NetworkInterface::GetLocalGamerIndex(),"play_as","character",gameCharacter,NULL,m_PostId,FACEBOOK_POST_ID_LEN,&m_Status) )
				{
					SetPhase(FACEBOOK_PHASE_POST_BOUGHT_GTA_WAIT);
				}
				else
				{
					DisableBusySpinner();
					SetPhase(FACEBOOK_PHASE_IDLE);
				}
			}
			else
			{
				gnetWarning("FACEBOOK_PHASE_POST_BOUGHT_GTA_INIT - No player character");
				DisableBusySpinner();
				SetPhase(FACEBOOK_PHASE_IDLE);
			}
			break;
		case FACEBOOK_PHASE_POST_BOUGHT_GTA_WAIT:
			if( !m_Status.Pending() )
			{
				DisableBusySpinner();
				if( m_Status.Succeeded() )
				{
					CGameStreamMgr::GetGameStream()->PostMessageText( TheText.Get("FB_ACCOUNT_LINKED_MSG"), "CHAR_FACEBOOK", "CHAR_FACEBOOK", false, 0, "", "" );
					SetPostedBoughtGame();
				}
				SetPhase(FACEBOOK_PHASE_IDLE);
			}
			break;
		default:
			break;
	}

#if RSG_PC
	UpdateKillswitch();
#endif

#if __BANK
	Bank_Update();
#endif
}

void CFacebook::TryPostBoughtGame()
{
	bool bShouldPostBoughtGame = ShouldPostBoughtGame(); 
	bool bCanReportToFacebook = CanReportToFacebook();
	gnetDebug1("TryPostBoughtGame :: ShouldPostBoughtGame(%s),  CanReportToFacebook(%s), Reason: %s", bShouldPostBoughtGame ? "TRUE" : "FALSE", bCanReportToFacebook ? "TRUE" : "FALSE", GetNoReportToFacebookReason());
	if( bShouldPostBoughtGame && bCanReportToFacebook )
	{
		EnableBusySpinner("FACEBOOK_SENDING");
		SetPhase(FACEBOOK_PHASE_POST_BOUGHT_GTA_INIT);
	}
	else
	{
		SetPhase(FACEBOOK_PHASE_IDLE);
	}
}

// ---------------------------------------------------------
bool CFacebook::IsBusy() const
{
	return( m_Phase!=FACEBOOK_PHASE_IDLE );
}

// ---------------------------------------------------------
bool CFacebook::DidSucceed() const
{
	return( m_LastError==FACEBOOK_ERROR_OK );
}

// ---------------------------------------------------------
eFbError CFacebook::GetLastError() const
{
	return( m_LastError );
}

rlFbGetTokenResult CFacebook::GetLastResult() const
{
	return rlFbGetTokenResult( m_LastResult );
}

// ---------------------------------------------------------
const char* CFacebook::GetLastPostId() const
{
	if( IsBusy() )
	{
		gnetAssertf( false, "CFacebook::GetLastPostId: Already posting to facebook." );
		return( NULL );
	}
	return( m_PostId );
}

// ---------------------------------------------------------
void CFacebook::EnableGUI( bool setEnabled )
{
	(void)setEnabled;
}

// ---------------------------------------------------------
void CFacebook::EnableAutoGetAccessToken( bool setEnabled )
{
	(void)setEnabled;
}

// ---------------------------------------------------------
bool CFacebook::HasAccessToken() const
{
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		return( rlFacebook::HasValidToken(localGamerIndex) );
	}
	return( false );
}

// Check for offline--->online or permission changes
// ---------------------------------------------------------
void CFacebook::UpdateOnlineStateIdle()
{
	if( !CLiveManager::GetCommerceMgr().HasThinDataPopulationBeenCompleted() )
	{
		return;
	}

	bool currentIsSignedIn = IsOnline();
	if( m_LastOnlineState != currentIsSignedIn )
	{
		if( currentIsSignedIn )
		{
			m_LastResult = 0;
			m_LastError = FACEBOOK_ERROR_OK;
			SetPhase(FACEBOOK_PHASE_GETACCCESSTOKEN_SILENT_INIT);
			m_StartPhase = m_Phase;
			rlFacebook::ClearAppPermissions();
		}
		m_LastOnlineState = currentIsSignedIn;
	}

	bool currentIsSocialSharingPrivileges = CLiveManager::GetSocialNetworkingSharingPrivileges();
	if (m_LastSocialSharingPrivileges != currentIsSocialSharingPrivileges)
	{
		if( currentIsSocialSharingPrivileges )
		{
			m_LastResult = 0;
			m_LastError = FACEBOOK_ERROR_OK;
			SetPhase(FACEBOOK_PHASE_GETACCCESSTOKEN_SILENT_INIT);
			m_StartPhase = m_Phase;
			rlFacebook::ClearAppPermissions();
		}

		m_LastSocialSharingPrivileges = currentIsSocialSharingPrivileges;
	}
}

// Display the "Post to Facebook" button?
// User has an SC account/signed in, facebook enabled and rlFacebook Has a valid token.
// ---------------------------------------------------------
bool CFacebook::CanReportToFacebook()
{
	rtry
	{
		rcheck(IsOnline(), catchall, );
		rcheck(HasSocialClubAccount(), catchall, );
		rcheck(IsProfileSettingEnabled(), catchall, );
		rcheck(HasAccessToken(), catchall, );
		rcheck(CLiveManager::GetSocialNetworkingSharingPrivileges(), catchall ,);
		return true;
	}
	rcatchall
	{
		return false;
	}
}

#if !__NO_OUTPUT
const char* CFacebook::GetNoReportToFacebookReason()
{
	if( !IsOnline() )
		return "GetNoReportToFacebookReason :: IsOnline == false";
	if(!HasSocialClubAccount())
		return "GetNoReportToFacebookReason :: HasSocialClubAccount == false";
	if( !IsProfileSettingEnabled())
		return "GetNoReportToFacebookReason :: IsProfileSettingEnabled == false";
	if( !HasAccessToken() )
		return "GetNoReportToFacebookReason :: HasAccessToken == false";
	if (!CLiveManager::GetSocialNetworkingSharingPrivileges())
		return "GetNoReportToFacebookReason :: GetSocialNetworkingSharingPrivileges == false";

	return "No Reason";
}
#endif // !__NO_OUTPUT

// ---------------------------------------------------------
bool CFacebook::IsOnline()
{
	bool bIsOnline = NetworkInterface::IsLocalPlayerOnline() && NetworkInterface::IsOnlineRos();
	bool bHaveProfileSettingsLoadedAndReady = CProfileSettings::GetInstance().AreSettingsValid();
	return( bIsOnline && bHaveProfileSettingsLoadedAndReady );
}

// ---------------------------------------------------------
bool CFacebook::HasSocialClubAccount()
{
	return( NetworkInterface::HasValidRockstarId() );
}

// ---------------------------------------------------------
bool CFacebook::IsProfileSettingEnabled()
{
	// Because the setting could change while you're in the menu, and we don't update that until after the menu is closed, it was decided this would be cleaner
	return( CPauseMenu::GetMenuPreference(PREF_FACEBOOK_UPDATES) != 0 );
}

bool CFacebook::HasProfileSettingHint()
{
	CProfileSettings& settings = CProfileSettings::GetInstance();
	if( !settings.AreSettingsValid())
	{
		return false;
	}

	int hint = settings.Exists(CProfileSettings::FACEBOOK_LINKED_HINT) ? settings.GetInt(CProfileSettings::FACEBOOK_LINKED_HINT) : 0;
	if( hint == 1 ) 
	{
		return true;
	}

	return false;
}

void CFacebook::SetProfileSettingHint(bool bHintEnabled)
{
	CProfileSettings& settings = CProfileSettings::GetInstance();
	if(settings.AreSettingsValid())
	{
		settings.Set(CProfileSettings::FACEBOOK_LINKED_HINT, bHintEnabled ? 1 : 0);
	}

	gnetDebug1("SetProfileSettingHint - %s", bHintEnabled ? "enabled" : "disabled");
}

void CFacebook::OutputProfileSettings()
{
	gnetDebug1("OutputProfileSettings - Profile Setting %s.", IsProfileSettingEnabled() ? "enabled" : "disabled");
	gnetDebug1("OutputProfileSettings - Profile Hint %s.", HasProfileSettingHint() ? "enabled" : "disabled");
}

// ---------------------------------------------------------
bool CFacebook::ShouldPostBoughtGame()
{
	CProfileSettings& settings = CProfileSettings::GetInstance();
	if( !settings.AreSettingsValid())
	{
		return false;
	}

	if( settings.Exists(CProfileSettings::FACEBOOK_POSTED_BOUGHT_GAME) && settings.GetInt(CProfileSettings::FACEBOOK_POSTED_BOUGHT_GAME)==1 ) 
	{
		return false;
	}
	
	return true;
}

// ---------------------------------------------------------
void CFacebook::SetPostedBoughtGame()
{
	CProfileSettings& settings = CProfileSettings::GetInstance();
	if(settings.AreSettingsValid())
	{
		settings.Set(CProfileSettings::FACEBOOK_POSTED_BOUGHT_GAME,1);
	}
}

// Obtain an access token by showing the gui (GetUserAccessTokenWithGui)
// ---------------------------------------------------------
bool CFacebook::GetAccessToken( void )
{
	if( IsBusy() )
	{
		gnetAssertf( false, "CFacebook::GetAccessToken Already busy. Poll CFacebook::IsBusy() or FACEBOOK_HAS_POST_COMPLETED() before starting." );
		return( false );
	}

	m_LastResult = 0;
	m_LastError = FACEBOOK_ERROR_OK;
	SetPhase(FACEBOOK_PHASE_GETACCCESSTOKEN_INIT);
	m_StartPhase = m_Phase;
	return( true );
}


// Action:      : play
// TargetObject : gta5mission
// ---------------------------------------------------------
bool CFacebook::PostPlayMission( const char* missionId, int xpEarned, int rank )
{
	char tempExtraParams[FACEBOOK_MAX_EXTRAPARAMS];
	formatf(tempExtraParams, sizeof(tempExtraParams), "xp_earned=%d&rank=%d", xpEarned, rank );
	return( PostRaw( "play", "gta5mission", missionId, tempExtraParams ) );
}

// Action:      : like
// TargetObject : gta5mission
// ---------------------------------------------------------
bool CFacebook::PostLikeMission( const char* missionId )
{
	return( PostRaw( "like", "gta5mission", missionId, "" ) );
}

// Action:      : publish
// TargetObject : gta5mission
// ---------------------------------------------------------
bool CFacebook::PostPublishMission( const char* missionId )
{
	return( PostRaw( "publish", "gta5mission", missionId, "" ) );
}

// Action:      : publish
// TargetObject : photo
// ---------------------------------------------------------
bool CFacebook::PostPublishPhoto( const char* screenshotId )
{
	return( PostRaw( "publish", "photo", screenshotId, "" ) );
}

bool CFacebook::PostTakePhoto( const char* screenshotId )
{
	return( PostRaw( "take", "photo", screenshotId, "" ) );
}

// Action:      : complete
// TargetObject : heist
// Parameters:
//	heistId - Id of the heist, e.g. “PRO1, ARM1, ARM2, FRA0"
//	cashEarned - The amount of cash earned during the heist.
//	xpEarned - the amount of xp earned during the heist.
bool CFacebook::PostCompleteHeist( const char* heistId, int cashEarned, int xpEarned )
{
	char tempExtraParams[FACEBOOK_MAX_EXTRAPARAMS];
	formatf(tempExtraParams, sizeof(tempExtraParams), "cash_earned=%d&xp_earned=%d", cashEarned, xpEarned );
	gnetDebug1("Posting heist facebook message. Heist Id: %s Params: %s",heistId,tempExtraParams);
	return( PostRaw( "complete", "heist", heistId, tempExtraParams ) );
}

// Action:      : complete
// TargetObject : milestone
// Parameters:
//	milestoneId - Id of the heist, e.g. "PRO1, ARM1, ARM2, FRA0"
bool CFacebook::PostCompleteMilestone( eMileStoneId milestoneId )
{
	const char* msName = TranslateMilestoneName( milestoneId );
	if( msName == NULL )
	{
		gnetAssertf( false, "CFacebook::PostCompleteMilestone unknown milestone id %d", (int)milestoneId );
		return( false );
	}
	gnetDebug1("Posting milestone facebook message. Milestone name: %s",msName);
	return( PostRaw( "complete", "milestone", msName, NULL ) );
}

// Action:      : create
// TargetObject : character
// Parameters:
//	none
bool CFacebook::PostCreateCharacter()
{
	gnetDebug1("Posting character creation facebook message.");
	return( PostRaw( "create", "character", "gtaonline", NULL ) );
}

// ---------------------------------------------------------
bool CFacebook::PostRaw( const char* action, const char* objName, const char* objTargetData, const char* extraParams )
{
	if( IsBusy() )
	{
		gnetAssertf( false, "CFacebook::PostRaw Already busy. Poll CFacebook::IsBusy() or FACEBOOK_HAS_POST_COMPLETED() before starting." );
		return( false );
	}

	safecpy( m_PostAction, action, sizeof(m_PostAction) );
	safecpy( m_PostObjName, objName, sizeof(m_PostObjName) );
	safecpy( m_PostObjTargetData, objTargetData, sizeof(m_PostObjTargetData) );
	safecpy( m_PostObjExtraParams, extraParams, sizeof(m_PostObjExtraParams) );

	gnetDebug3("PostRaw: %s [%s] (%s) (%s)", m_PostAction, m_PostObjName, m_PostObjTargetData, m_PostObjExtraParams);

	m_LastResult = 0;
	m_LastError = FACEBOOK_ERROR_OK;
	SetPhase(FACEBOOK_PHASE_POST_INIT);
	m_StartPhase = m_Phase;
	return( true );
}

#if RSG_PC
void CFacebook::UpdateKillswitch()
{
	bool bKillSwitch = false;

	if (m_bAllowFacebookKillswitch)
	{
		rlRosGeoLocInfo geoLocInfo;
		if(rlRos::GetGeoLocInfo(&geoLocInfo))
		{
			if (stricmp(geoLocInfo.m_CountryCode,"CN") == 0)
			{
				bKillSwitch = true;
			}
		}
	}

#if RSG_BANK
	if (sm_BankKillswitch)
	{
		bKillSwitch = true;
	}
#endif

	if (rlFacebook::IsKillSwitchEnabled() != bKillSwitch)
	{
		gnetDebug1("%s Facebook kill switch due to geo loc results and tunable", bKillSwitch ? "Enabling" : "Disabling");
		rlFacebook::SetKillSwitchEnabled(bKillSwitch);
	}
}
#endif

void CFacebook::InitTunables()
{
#if RSG_PC
	m_bAllowFacebookKillswitch = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("FACEBOOK_REGION_KILLSWITCH", 0x3F20DBCC), true);
#endif
}

const char* CFacebook::GetPlayerCharacter()
{
	CPed* player = FindPlayerPed();
	if( player != NULL )
	{
		if( player->GetPedType() == PEDTYPE_PLAYER_0 )
		{
			return( "michael" );
		}

		if( player->GetPedType() == PEDTYPE_PLAYER_1 )
		{
			return( "franklin" );
		}

		if( player->GetPedType() == PEDTYPE_PLAYER_2 )
		{
			return( "trevor" );
		}
	}
	return( NULL );
}

const char* CFacebook::TranslateMilestoneName( eMileStoneId milestoneId )
{
	for( int i=0; i!=FACEBOOK_MILESTONE_COUNT; i++ )
	{
		if( FacebookMilestones[i].Id == milestoneId )
		{
			return( FacebookMilestones[i].Name );
		}
	}
	return( NULL );
}

void CFacebook::SetPhase(eFbPhase e)
{
#if !__NO_OUTPUT
	static const char *s_StateNames[] =
	{
		"FACEBOOK_PHASE_IDLE",
		"FACEBOOK_PHASE_GETACCCESSTOKEN_SILENT_INIT",
		"FACEBOOK_PHASE_GETACCCESSTOKEN_SILENT_WAIT",
		"FACEBOOK_PHASE_GETACCESSTOKEN_USER_INPUT_BEFORE",
		"FACEBOOK_PHASE_GETACCCESSTOKEN_INIT",
		"FACEBOOK_PHASE_GETACCESSTOKEN_WAIT",
		"FACEBOOK_PHASE_GETACCESSTOKEN_USER_INPUT_AFTER",
		"FACEBOOK_PHASE_GETACCESSTOKEN_USER_INPUT_AFTER_DELAY",
		"FACEBOOK_PHASE_POST_INIT",
		"FACEBOOK_PHASE_POST_WAIT",
		"FACEBOOK_PHASE_POST_BOUGHT_GTA_INIT",
		"FACEBOOK_PHASE_POST_BOUGHT_GTA_WAIT",
		"FACEBOOK_PHASE_PRE_CLEANUP",
		"FACEBOOK_PHASE_CLEANUP"
	};

	CompileTimeAssert(COUNTOF(s_StateNames) == FACEBOOK_PHASE_CLEANUP+1);

	gnetDebug1("SetPhase(%s)", s_StateNames[(int)e]);
#endif

	m_Phase = e;
}

void CFacebook::EnableBusySpinner(const char* source)
{
	gnetDebug1("Enabling Facebook Busy Spinner");
	CBusySpinner::On( TheText.Get(source), 4, SPINNER_SOURCE_FACEBOOK );
}

void CFacebook::DisableBusySpinner()
{
	gnetDebug1("Disable Facebook Busy Spinner");
	CBusySpinner::Off( SPINNER_SOURCE_FACEBOOK );
}

// ---------------------------------------------------------

#if __BANK

char CFacebook::sm_BankPhase[FACEBOOK_BANK_MAX_PHASE];
char CFacebook::sm_BankLastError[FACEBOOK_BANK_MAX_LASTERROR];
char CFacebook::sm_BankOutPostId[FACEBOOK_BANK_MAX_ID_LEN];
char CFacebook::sm_BankAccessTokenState[FACEBOOK_BANK_MAX_ACCESSTOKEN_STATE];
char CFacebook::sm_BankCanReportState[FACEBOOK_BANK_MAX_CANREPORT_STATE];

char CFacebook::sm_BankPostAction[FACEBOOK_BANK_MAX_POSTACTION];
char CFacebook::sm_BankPostObjName[FACEBOOK_BANK_MAX_OBJNAME];
char CFacebook::sm_BankPostObjTargetData[FACEBOOK_BANK_MAX_OBJTARGETDATA];
char CFacebook::sm_BankPostObjExtraParams[FACEBOOK_BANK_MAX_EXTRAPARAMS];

char CFacebook::sm_BankPostPublishPhotoA[FACEBOOK_BANK_MAX_PARAM];

char CFacebook::sm_BankPostPlayMissionA[FACEBOOK_BANK_MAX_PARAM];
char CFacebook::sm_BankPostPlayMissionB[FACEBOOK_BANK_MAX_PARAM];
char CFacebook::sm_BankPostPlayMissionC[FACEBOOK_BANK_MAX_PARAM];

char CFacebook::sm_BankPostLikeMissionA[FACEBOOK_BANK_MAX_PARAM];

char CFacebook::sm_BankPostPublishMissionA[FACEBOOK_BANK_MAX_PARAM];

char CFacebook::sm_BankPostCompleteHeistA[FACEBOOK_BANK_MAX_PARAM];
char CFacebook::sm_BankPostCompleteHeistB[FACEBOOK_BANK_MAX_PARAM];
char CFacebook::sm_BankPostCompleteHeistC[FACEBOOK_BANK_MAX_PARAM];

char CFacebook::sm_BankPostCompleteMilestoneA[FACEBOOK_BANK_MAX_PARAM];

#if RSG_PC
bool CFacebook::sm_BankKillswitch;
#endif

// ---------------------------------------------------------
void CFacebook::InitWidgets( bkBank& bank )
{
	bkGroup* pGroup = bank.AddGroup("Facebook");

	strcpy( sm_BankPhase, "" );
	strcpy( sm_BankLastError, "" );
	strcpy( sm_BankOutPostId, "" );

	memset( sm_BankPostAction,0,sizeof(sm_BankPostAction) );
	memset( sm_BankPostObjName,0,sizeof(sm_BankPostObjName) );
	memset( sm_BankPostObjTargetData,0,sizeof(sm_BankPostObjTargetData) );
	memset( sm_BankPostObjExtraParams,0,sizeof(sm_BankPostObjExtraParams) );
	memset( sm_BankAccessTokenState,0,sizeof(sm_BankAccessTokenState) );
	memset( sm_BankCanReportState,0,sizeof(sm_BankCanReportState) );

	memset( sm_BankPostPublishPhotoA, 0, sizeof(sm_BankPostPublishPhotoA) );
	memset( sm_BankPostPlayMissionA, 0, sizeof(sm_BankPostPlayMissionA) );
	memset( sm_BankPostPlayMissionB, 0, sizeof(sm_BankPostPlayMissionB) );
	memset( sm_BankPostPlayMissionC, 0, sizeof(sm_BankPostPlayMissionC) );
	memset( sm_BankPostLikeMissionA, 0, sizeof(sm_BankPostLikeMissionA) );
	memset( sm_BankPostPublishMissionA, 0, sizeof(sm_BankPostPublishMissionA) );

	memset( sm_BankPostCompleteHeistA, 0, sizeof(sm_BankPostCompleteHeistA) );
	memset( sm_BankPostCompleteHeistB, 0, sizeof(sm_BankPostCompleteHeistB) );
	memset( sm_BankPostCompleteHeistC, 0, sizeof(sm_BankPostCompleteHeistC) );

#if RSG_PC
	sm_BankKillswitch = false;
#endif

	strcpy( sm_BankPostCompleteMilestoneA, "0" );
	
	pGroup->AddSeparator();
	pGroup->AddText( "Phase              :", sm_BankPhase, sizeof(sm_BankPhase), true);
	pGroup->AddText( "LastError          :", sm_BankLastError, sizeof(sm_BankLastError), true);
	pGroup->AddText( "ReturnPostId       :", sm_BankOutPostId, sizeof(sm_BankOutPostId), true);
	pGroup->AddText( "LastResult         :", &m_LastResult);
	pGroup->AddText( "Linked?            :", sm_BankAccessTokenState, sizeof(sm_BankAccessTokenState), true);
	pGroup->AddText( "CanReport?         :", sm_BankCanReportState, sizeof(sm_BankCanReportState), true);

	pGroup->AddSeparator();
	pGroup->AddButton( "Link to facebook [GetAccessToken]", datCallback(  MFA(CFacebook::Bank_LinkToFacebook), this ) );
	pGroup->AddSeparator();

	pGroup->AddText( "Action            :", sm_BankPostAction, sizeof(sm_BankPostAction) , false);
	pGroup->AddText( "ObjName           :", sm_BankPostObjName, sizeof(sm_BankPostObjName) , false);
	pGroup->AddText( "ObjTargetData     :", sm_BankPostObjTargetData, sizeof(sm_BankPostObjTargetData) , false);
	pGroup->AddText( "ObjExtraParams    :", sm_BankPostObjExtraParams, sizeof(sm_BankPostObjExtraParams) , false);
	pGroup->AddButton( "PostRaw", datCallback(  MFA(CFacebook::Bank_PostRaw), this ) );

	pGroup->AddSeparator();

	pGroup->AddText( "ScreenshotID      :", sm_BankPostPublishPhotoA, sizeof(sm_BankPostPublishPhotoA) , false);
	pGroup->AddButton( "PostPublishPhoto", datCallback(  MFA(CFacebook::Bank_PostPublishPhoto), this ) );
	
	pGroup->AddSeparator();

	pGroup->AddText( "MissionID         :", sm_BankPostPlayMissionA, sizeof(sm_BankPostPlayMissionA) , false);
	pGroup->AddText( "XpEarned          :", sm_BankPostPlayMissionB, sizeof(sm_BankPostPlayMissionB) , false);
	pGroup->AddText( "Rank              :", sm_BankPostPlayMissionC, sizeof(sm_BankPostPlayMissionC) , false);
	pGroup->AddButton( "PostPlayMission", datCallback(  MFA(CFacebook::Bank_PostPlayMission), this ) );

	pGroup->AddSeparator();

	pGroup->AddText( "MissionID         :", sm_BankPostLikeMissionA, sizeof(sm_BankPostLikeMissionA) , false);
	pGroup->AddButton( "PostLikeMission", datCallback(  MFA(CFacebook::Bank_PostLikeMission), this ) );

	pGroup->AddSeparator();

	pGroup->AddText( "MissionID         :", sm_BankPostPublishMissionA, sizeof(sm_BankPostPublishMissionA) , false);
	pGroup->AddButton( "PostPublishMission", datCallback(  MFA(CFacebook::Bank_PostPublishMission), this ) );

	pGroup->AddSeparator();

	pGroup->AddText( "HeistID           :", sm_BankPostCompleteHeistA, sizeof(sm_BankPostCompleteHeistA) , false);
	pGroup->AddText( "Cash Earned       :", sm_BankPostCompleteHeistB, sizeof(sm_BankPostCompleteHeistB) , false);
	pGroup->AddText( "XP Earned         :", sm_BankPostCompleteHeistC, sizeof(sm_BankPostCompleteHeistC) , false);
	pGroup->AddButton( "PostCompleteHeist", datCallback(  MFA(CFacebook::Bank_PostCompleteHeist), this ) );

	pGroup->AddSeparator();

	pGroup->AddText( "MilestoneID       :", sm_BankPostCompleteMilestoneA, sizeof(sm_BankPostCompleteMilestoneA) , false);
	pGroup->AddButton( "PostCompleteMilestone", datCallback(  MFA(CFacebook::Bank_PostCompleteMilestone), this ) );

	pGroup->AddSeparator();

	pGroup->AddButton( "PostCreateCharacter", datCallback(  MFA(CFacebook::Bank_PostCreateCharacter), this ) );

	pGroup->AddSeparator();

#if RSG_PC
	pGroup->AddToggle( "Facebook Killswitch" , &sm_BankKillswitch );
#endif
}

// ---------------------------------------------------------
void CFacebook::Bank_Update()
{
	switch( m_Phase )
	{
		case FACEBOOK_PHASE_IDLE:							strcpy(sm_BankPhase,"FACEBOOK_PHASE_IDLE");break;
		case FACEBOOK_PHASE_GETACCCESSTOKEN_SILENT_INIT:	strcpy(sm_BankPhase,"FACEBOOK_PHASE_GETACCCESSTOKEN_SILENT_INIT");break;
		case FACEBOOK_PHASE_GETACCCESSTOKEN_SILENT_WAIT:	strcpy(sm_BankPhase,"FACEBOOK_PHASE_GETACCCESSTOKEN_SILENT_WAIT");break;
		case FACEBOOK_PHASE_GETACCCESSTOKEN_INIT:			strcpy(sm_BankPhase,"FACEBOOK_PHASE_GETACCCESSTOKEN_INIT");break;
		case FACEBOOK_PHASE_GETACCESSTOKEN_WAIT:			strcpy(sm_BankPhase,"FACEBOOK_PHASE_GETACCESSTOKEN_WAIT");break;
		case FACEBOOK_PHASE_POST_INIT:						strcpy(sm_BankPhase,"FACEBOOK_PHASE_POST_INIT");break;
		case FACEBOOK_PHASE_POST_WAIT:						strcpy(sm_BankPhase,"FACEBOOK_PHASE_POST_WAIT");break;
		case FACEBOOK_PHASE_CLEANUP:						strcpy(sm_BankPhase,"FACEBOOK_PHASE_CLEANUP");break;
	
		default:
		formatf( sm_BankPhase, sizeof(sm_BankPhase), "Unknown: %d", m_Phase );
		break;
	}

	if( m_Phase == FACEBOOK_PHASE_IDLE )
	{
		switch( m_LastError )
		{
			case FACEBOOK_ERROR_OK:						strcpy(sm_BankLastError,"FACEBOOK_ERROR_OK");break;
			case FACEBOOK_ERROR_GETACESSTOKEN:			formatf(sm_BankLastError,sizeof(sm_BankLastError),"FACEBOOK_ERROR_GETACESSTOKEN (%d)", m_LastResult);break;
			case FACEBOOK_ERROR_POST:					formatf(sm_BankLastError,sizeof(sm_BankLastError),"FACEBOOK_ERROR_POST (%d)", m_LastResult);break;
		
			default:
			formatf( sm_BankLastError, sizeof(sm_BankLastError), "Unknown: %d", m_LastError );
			break;		
		}
		
		strcpy( sm_BankOutPostId, m_PostId );
	}
	else
	{
		strcpy( sm_BankLastError, "" );
	}

	if( HasAccessToken() )
	{
		strcpy( sm_BankAccessTokenState, "Linked!" );
	}
	else
	{
		strcpy( sm_BankAccessTokenState, "Not linked. (No access token)" );
	}

	if( CanReportToFacebook() )
	{
		strcpy( sm_BankCanReportState, "Yes (Show the option to post)" );
	}
	else
	{
		strcpy( sm_BankCanReportState, "No (Hide option to post) [ " );
		char tmp[32]={"000000"};
		tmp[0] = '1';
		if( IsOnline() )
		{
			tmp[1] = '1';
			if( HasSocialClubAccount() )
			{
				tmp[2] = '1';
				if( IsProfileSettingEnabled() )
				{
					tmp[3] = '1';
					if( HasAccessToken() )
					{
						tmp[4] = '1';
						if (CLiveManager::GetSocialNetworkingSharingPrivileges())
						{
							tmp[5] = '1';
						}
					}
				}
			}
		}

		strcat( sm_BankCanReportState, tmp );
		strcat( sm_BankCanReportState, " ]" );
	}
}

// ---------------------------------------------------------
void CFacebook::Bank_PostRaw()
{
	if( !IsBusy() )
	{
		PostRaw( sm_BankPostAction, sm_BankPostObjName, sm_BankPostObjTargetData, sm_BankPostObjExtraParams );
	}
}

// ---------------------------------------------------------
void CFacebook::Bank_PostPlayMission()
{
	if( !IsBusy() )
	{
		PostPlayMission( sm_BankPostPlayMissionA, atoi(sm_BankPostPlayMissionB), atoi(sm_BankPostPlayMissionC) );
	}
}

// ---------------------------------------------------------
void CFacebook::Bank_PostLikeMission()
{
	if( !IsBusy() )
	{
		PostLikeMission( sm_BankPostLikeMissionA );
	}
}

// ---------------------------------------------------------
void CFacebook::Bank_PostPublishMission()
{
	if( !IsBusy() )
	{
		PostPublishMission( sm_BankPostPublishMissionA );
	}
}

// ---------------------------------------------------------
void CFacebook::Bank_PostPublishPhoto()
{
	if( !IsBusy() )
	{
		PostPublishPhoto( sm_BankPostPublishPhotoA );
	}
}

// ---------------------------------------------------------
void CFacebook::Bank_PostCompleteHeist()
{
	if( !IsBusy() )
	{
		PostCompleteHeist( sm_BankPostCompleteHeistA, atoi(sm_BankPostCompleteHeistB), atoi(sm_BankPostCompleteHeistC) );
	}
}

// ---------------------------------------------------------
void CFacebook::Bank_PostCompleteMilestone()
{
	if( !IsBusy() )
	{
		PostCompleteMilestone( (eMileStoneId)atoi(sm_BankPostCompleteMilestoneA) );
	}
}

// ---------------------------------------------------------
void CFacebook::Bank_PostCreateCharacter()
{
	if( !IsBusy() )
	{
		PostCreateCharacter();
	}
}

// ---------------------------------------------------------
void CFacebook::Bank_LinkToFacebook()
{
	if( !IsBusy() )
	{
		GetAccessToken();
	}
}


#endif // __BANK
#endif // RL_FACEBOOK_ENABLED