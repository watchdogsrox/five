//
// InviteMgr.cpp
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

//rage
#include "string/stringhash.h"
#include "fwnet/netchannel.h"
#include "rline/rlpc.h"
#include "rline/rlgamerinfo.h"

//game
#include "Network/Live/InviteMgr.h"
#include "Network/Live/LiveManager.h"
#include "Network/Cloud/UserContentManager.h"
#include "Network/Network.h"
#include "Network/General/NetworkUtil.h"
#include "Network/Sessions/NetworkGameConfig.h"
#include "Network/Sessions/NetworkSession.h"
#include "Network/SocialClubServices/GamePresenceEvents.h"
#include "Network/xlast/Fuzzy.schema.h"

#include "event/EventGroup.h"
#include "event/EventNetwork.h"
#include "frontend/BusySpinner.h"
#include "frontend/loadingscreens.h"
#include "frontend/landing_page/LandingPage.h"
#include "frontend/SocialClubMenu.h"
#include "frontend/Store/StoreScreenMgr.h"
#include "frontend/VideoEditor/VideoEditorInterface.h"
#include "frontend/WarningScreen.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_autoload.h"
#include "script/streamedscripts.h"
#include "scene/world/GameWorld.h"
#include "Stats/StatsInterface.h"
#include "network/Cloud/Tunables.h"
#include "network/Live/NetworkTelemetry.h"
#include "network/Live/NetworkDebugTelemetry.h"
#include "script/script.h"
#include "script/script_hud.h"

NETWORK_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, invite, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_invite

PARAM(netInviteScriptHandleNoPrivilege, "[Invite] Let script handle no privilege");
PARAM(netAutoActionPlatformInvites, "[Invite] Auto-accept platform invites");
PARAM(netInvitePlatformCheckStats, "[Invite] Whether we check stats or not when processing platform invites");

// tracking for boot invites
static bool gs_HasBooted = false;

// to generate invite Ids
static netRandom sRng;

#if !__NO_OUTPUT

#define CHECK_AND_LOG_INVITE_FLAG(x) if((nFlags & x) != 0) { gnetDebug1("\t%s", #x); }

void LogInviteFlags(const unsigned nFlags, const char* szFunction)
{
	gnetDebug1("%s :: LogInviteFlags (0x%x)", szFunction, nFlags);
	CHECK_AND_LOG_INVITE_FLAG(InviteFlags::IF_IsPlatform);
	CHECK_AND_LOG_INVITE_FLAG(InviteFlags::IF_IsJoin);
	CHECK_AND_LOG_INVITE_FLAG(InviteFlags::IF_ViaParty);
	CHECK_AND_LOG_INVITE_FLAG(InviteFlags::IF_Rockstar);
	CHECK_AND_LOG_INVITE_FLAG(InviteFlags::IF_Spectator);
	CHECK_AND_LOG_INVITE_FLAG(InviteFlags::IF_AllowPrivate);
	CHECK_AND_LOG_INVITE_FLAG(InviteFlags::IF_IsRockstarInactive);
	CHECK_AND_LOG_INVITE_FLAG(InviteFlags::IF_IsFollow);
	CHECK_AND_LOG_INVITE_FLAG(InviteFlags::IF_PlatformParty);
	CHECK_AND_LOG_INVITE_FLAG(InviteFlags::IF_PlatformPartyBoot);
	CHECK_AND_LOG_INVITE_FLAG(InviteFlags::IF_PlatformPartyJoin);
	CHECK_AND_LOG_INVITE_FLAG(InviteFlags::IF_PlatformPartyJvP);
	CHECK_AND_LOG_INVITE_FLAG(InviteFlags::IF_ViaJoinQueue);
	CHECK_AND_LOG_INVITE_FLAG(InviteFlags::IF_ViaAdmin);
	CHECK_AND_LOG_INVITE_FLAG(InviteFlags::IF_IsRockstarDev);
	CHECK_AND_LOG_INVITE_FLAG(InviteFlags::IF_NotTargeted);
	CHECK_AND_LOG_INVITE_FLAG(InviteFlags::IF_IsTournament);
	CHECK_AND_LOG_INVITE_FLAG(InviteFlags::IF_BadReputation);
}

void LogInternalFlags(const unsigned nFlags, const char* szFunction)
{
	gnetDebug1("%s :: LogInternalFlags (0x%x)", szFunction, nFlags);
	CHECK_AND_LOG_INVITE_FLAG(InternalFlags::LIF_FromBoot);
	CHECK_AND_LOG_INVITE_FLAG(InternalFlags::LIF_CompletedQuery);
	CHECK_AND_LOG_INVITE_FLAG(InternalFlags::LIF_ValidConfig);
	CHECK_AND_LOG_INVITE_FLAG(InternalFlags::LIF_Incompatible);
	CHECK_AND_LOG_INVITE_FLAG(InternalFlags::LIF_ProcessedConfig);
	CHECK_AND_LOG_INVITE_FLAG(InternalFlags::LIF_Accepted);
	CHECK_AND_LOG_INVITE_FLAG(InternalFlags::LIF_AutoConfirm);
	CHECK_AND_LOG_INVITE_FLAG(InternalFlags::LIF_Confirmed);
	CHECK_AND_LOG_INVITE_FLAG(InternalFlags::LIF_Actioned);
	CHECK_AND_LOG_INVITE_FLAG(InternalFlags::LIF_KnownToScript);
}

void LogResponseFlags(const unsigned nFlags, const char* szFunction)
{
	gnetDebug1("%s :: LogResponseFlags (0x%x)", szFunction, nFlags);
	CHECK_AND_LOG_INVITE_FLAG(InviteResponseFlags::IRF_StatusInSinglePlayer);
	CHECK_AND_LOG_INVITE_FLAG(InviteResponseFlags::IRF_StatusInFreeroam);
	CHECK_AND_LOG_INVITE_FLAG(InviteResponseFlags::IRF_StatusInLobby);
	CHECK_AND_LOG_INVITE_FLAG(InviteResponseFlags::IRF_StatusInActivity);
	CHECK_AND_LOG_INVITE_FLAG(InviteResponseFlags::IRF_RejectCannotAccessMultiplayer);
	CHECK_AND_LOG_INVITE_FLAG(InviteResponseFlags::IRF_RejectNoPrivilege);
	CHECK_AND_LOG_INVITE_FLAG(InviteResponseFlags::IRF_RejectBlocked);
	CHECK_AND_LOG_INVITE_FLAG(InviteResponseFlags::IRF_RejectBlockedInactive);
	CHECK_AND_LOG_INVITE_FLAG(InviteResponseFlags::IRF_StatusCheater);
	CHECK_AND_LOG_INVITE_FLAG(InviteResponseFlags::IRF_StatusBadSport);
	CHECK_AND_LOG_INVITE_FLAG(InviteResponseFlags::IRF_RejectCannotProcessRockstar);
	CHECK_AND_LOG_INVITE_FLAG(InviteResponseFlags::IRF_StatusInStore);
	CHECK_AND_LOG_INVITE_FLAG(InviteResponseFlags::IRF_StatusInCreator);
	CHECK_AND_LOG_INVITE_FLAG(InviteResponseFlags::IRF_StatusInactive);
	CHECK_AND_LOG_INVITE_FLAG(InviteResponseFlags::IRF_StatusFromJoinQueue);
	CHECK_AND_LOG_INVITE_FLAG(InviteResponseFlags::IRF_StatusInSession);
	CHECK_AND_LOG_INVITE_FLAG(InviteResponseFlags::IRF_RejectIncompatible);
	CHECK_AND_LOG_INVITE_FLAG(InviteResponseFlags::IRF_StatusInEditor);
	CHECK_AND_LOG_INVITE_FLAG(InviteResponseFlags::IRF_CannotProcessAdmin);
	CHECK_AND_LOG_INVITE_FLAG(InviteResponseFlags::IRF_RejectInvalidInviter);
}

const char* GetInviteActionAsString(InviteAction inviteAction)
{
	static const char* s_InviteActionStrings[] =
	{
		"IA_None",
		"IA_Accepted",
		"IA_Ignored",
		"IA_Rejected",
		"IA_Blocked",
		"IA_NotRelevant"
	};
	CompileTimeAssert(COUNTOF(s_InviteActionStrings) == InviteAction::IA_Max);
	return ((inviteAction >= InviteAction::IA_None) && (inviteAction < InviteAction::IA_Max)) ? s_InviteActionStrings[inviteAction] : "IA_Invalid";
}
#endif

///////////////////////////////////////////////////////////////////////////////
//  MetricAddRsInvite
///////////////////////////////////////////////////////////////////////////////

class MetricAddRsInvite : public MetricPlayStat
{
	RL_DECLARE_METRIC(INV_R, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	static const u32 TEXT_SIZE = MetricPlayStat::MAX_STRING_LENGTH;

private:
	char m_Gamer[RL_MAX_GAMER_HANDLE_CHARS];
	char m_szContentId[TEXT_SIZE];
	u32 m_nPlaylistLength;
	u32 m_nPlaylistCurrent;
	bool m_bScTv;

public:
	MetricAddRsInvite (const rlGamerHandle& hGamer, const char* szContentId, u32 nPlaylistLength, u32 nPlaylistCurrent, bool bScTv) 
		: m_nPlaylistLength(nPlaylistLength)
		, m_nPlaylistCurrent(nPlaylistCurrent)
		, m_bScTv(bScTv)
	{
		m_Gamer[0] = '\0';
		if(gnetVerify(hGamer.IsValid()))
		{
			hGamer.ToString(m_Gamer, COUNTOF(m_Gamer));
		}

		m_szContentId[0] = '\0';
		if(gnetVerify(szContentId))
		{
			formatf(m_szContentId, COUNTOF(m_szContentId), szContentId);
		}
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return MetricPlayStat::Write(rw)
			&& rw->WriteString("gamer", m_Gamer)
			&& rw->WriteString("cid", m_szContentId)
			&& rw->WriteInt("plistl", m_nPlaylistLength)
			&& rw->WriteInt("plistc", m_nPlaylistCurrent)
			&& rw->WriteBool("sctv", m_bScTv);
	}
};

#if !__FINAL
class MetricDebugAddRsInvite : public MetricAddRsInvite
{
	RL_DECLARE_METRIC(INV_R, TELEMETRY_CHANNEL_DEV_DEBUG, LOGLEVEL_DEBUG2);

public:
	MetricDebugAddRsInvite (const rlGamerHandle& hGamer, const char* szContentId, u32 nPlaylistLength, u32 nPlaylistCurrent, bool bScTv) 
		: MetricAddRsInvite (hGamer, szContentId, nPlaylistLength, nPlaylistCurrent, bScTv) {}
};
#endif// !__FINAL

///////////////////////////////////////////////////////////////////////////////
//  sAcceptedInvite
///////////////////////////////////////////////////////////////////////////////

sAcceptedInvite::sAcceptedInvite()
{
	this->Clear();
}

void sAcceptedInvite::Shutdown()
{
    gnetAssert(!IsProcessing());
    this->Clear();
}

void sAcceptedInvite::Clear()
{
	gnetAssert(!IsProcessing());

	gnetDebug1("AcceptedInvite :: Clear");

    m_SessionInfo.Clear();
    m_Invitee.Clear();
    m_Inviter.Clear();
	m_SessionDetail.Clear();
	m_InviteFlags = InviteFlags::IF_None;
	m_InternalFlags = InternalFlags::LIF_None;
    m_NumDetails = 0;

	m_nGameMode = 0xFFFFFFFF;
    m_SessionPurpose = SessionPurpose::SP_Freeroam;
    m_nAimType = 0;
	m_TaskStatus.Reset();
	m_nNumBosses = 0;

	m_State = STATE_IDLE;
}

void sAcceptedInvite::Cancel()
{
	gnetDebug1("AcceptedInvite :: Cancel");
	
	// cancel in flight request
	if(m_TaskStatus.Pending())
	{
		if(m_State == STATE_QUERYING_SESSION)
			rlSessionManager::Cancel(&m_TaskStatus);
		else if(m_State == STATE_QUERYING_PRESENCE)
			rlPresenceQuery::Cancel(&m_TaskStatus);
	}

    // clear info to invalidate invite
    m_SessionInfo.Clear();
    m_State = STATE_IDLE;

	// clear info
	Clear();
}

bool sAcceptedInvite::IsPending() const
{
	return m_SessionInfo.IsValid();
}

bool sAcceptedInvite::IsProcessing() const 
{
	return ((IsPending() && !HasFinished()) || (m_State != STATE_IDLE));
}

bool sAcceptedInvite::HasFinished() const
{
	return HasInternalFlag(InternalFlags::LIF_CompletedQuery);
}

bool sAcceptedInvite::HasSucceeded() const
{
    return m_TaskStatus.Succeeded() && HasInternalFlag(InternalFlags::LIF_ValidConfig);
}

bool sAcceptedInvite::HasFailed() const
{
	return m_TaskStatus.Failed() || (HasInternalFlag(InternalFlags::LIF_CompletedQuery) && !HasInternalFlag(InternalFlags::LIF_ValidConfig));
}

void sAcceptedInvite::AddInternalFlag(const unsigned flag)
{
#if !__NO_OUTPUT
	if((m_InternalFlags & flag) != flag)
		LogInternalFlags(flag, "AddInternalFlag");
#endif
	m_InternalFlags |= flag;
}

void sAcceptedInvite::RemoveInternalFlag(const unsigned flag)
{
#if !__NO_OUTPUT
	if((m_InternalFlags & flag) != 0)
		LogInternalFlags(flag, "RemoveInternalFlag");
#endif
	m_InternalFlags &= ~flag;
}

bool sAcceptedInvite::GetConfigDetails()
{
	const rlSessionConfig& config = m_SessionDetail.m_SessionConfig;

    // validate that we have a discriminator attribute. If not, the target has likely just left their session
    // fail and re-enter the invite query flow to find out where our target is
	const u32* pDisc = config.m_Attrs.GetValueById(player_schema::SchemaBase::FIELD_ID_MMATTR_DISCRIMINATOR);
	if(!pDisc)
    {
        gnetError("AcceptedInvite :: GetConfigDetails - No discriminator! Failing...");
		return false;
    }

	// standard config data
	m_nGameMode = config.m_Attrs.GetGameMode();
	m_SessionPurpose = static_cast<SessionPurpose>(config.m_Attrs.GetSessionPurpose());

	// reset these - need to pull based on the session type
	m_nAimType = CPedTargetEvaluator::MAX_TARGETING_OPTIONS;
	m_nNumBosses = 0;
	m_nActivityType = 0;
	m_nActivityID = 0;
	
	// for activity sessions...
	if(m_SessionPurpose == SessionPurpose::SP_Activity)
	{
		const u32* pActivityType = config.m_Attrs.GetValueById(player_schema::SchemaBase::FIELD_ID_MMATTR_ACTIVITY_TYPE);
		if(gnetVerifyf(pActivityType, "AcceptedInvite :: GetConfigDetails - No Activity Type!"))
		{
			m_nActivityType = static_cast<int>(*pActivityType);
		}
		
		const u32* pActivityID = config.m_Attrs.GetValueById(player_schema::SchemaBase::FIELD_ID_MMATTR_ACTIVITY_ID);
		if(gnetVerifyf(pActivityID, "AcceptedInvite :: GetConfigDetails - No Activity ID!"))
		{
			m_nActivityID = *pActivityID;
		}

		// we cannot accept an invite to an activity session with a UGC creator that we are incompatible with
        // grab the user data and check our privilege
        if(m_SessionDetail.m_SessionUserDataSize == static_cast<unsigned>(sizeof(SessionUserData_Activity)))
        {
			bool bBlockedFromInactive = false;
            const SessionUserData_Activity* pData = reinterpret_cast<SessionUserData_Activity*>(m_SessionDetail.m_SessionUserData);

			gnetDebug1("AcceptedInvite :: GetConfigDetails - ContentCreator: %s", pData->m_hContentCreator.ToString());

			if(pData->m_hContentCreator.IsValid() && !pData->m_hContentCreator.IsLocal())
			{
				// script manage messaging when we don't have multiplayer access (todo: switch that to code)
				if(CLiveManager::CheckOnlinePrivileges())
				{				
#if RSG_XBOX
					// check content privileges
					if(!CLiveManager::CheckUserContentPrivileges())
					{
						// on Xbox - the local permission API only will return blocked even if we have selected "friends only"
						// so we need to issue a follow up check to the service to check against this specific user
						gnetDebug1("AcceptedInvite :: GetConfigDetails - Checking permissions...");
						IPrivacySettings::CheckPermission(NetworkInterface::GetLocalGamerIndex(), IPrivacySettings::PERM_ViewTargetUserCreatedContent, pData->m_hContentCreator, &m_UserContentResult, &m_UserContentStatus);
					}
#elif RSG_SCE
					// check content privileges for the local user
					if(CLiveManager::IsInBlockList(pData->m_hContentCreator))
					{
						gnetDebug1("AcceptedInvite :: GetConfigDetails - Content creator is in our block list");
						AddInternalFlag(InternalFlags::LIF_Incompatible);
						formatf(m_szErrorLocalisationId, "NT_INV_BLOCKED_CREATOR");
					}
					else if(!CLiveManager::CheckUserContentPrivileges())
					{
						gnetDebug1("AcceptedInvite :: GetConfigDetails - Invalid content privileges");
						AddInternalFlag(InternalFlags::LIF_Incompatible);
						formatf(m_szErrorLocalisationId, "NT_INV_INCOMPATIBLE_CREATOR");
					}
					// check content privileges for all users
					else if(CLiveManager::CheckUserContentPrivileges(GAMER_INDEX_ANYONE, false))
					{
						gnetDebug1("AcceptedInvite :: GetConfigDetails - Invalid content privileges on inactive user");
						AddInternalFlag(InternalFlags::LIF_Incompatible);
						bBlockedFromInactive = true;
						formatf(m_szErrorLocalisationId, "NT_INV_INCOMPATIBLE_CREATOR_INACTIVE");
					}
#else
					if(!CLiveManager::CheckUserContentPrivileges())
					{
						gnetDebug1("AcceptedInvite :: GetConfigDetails - Invalid content privileges");
						AddInternalFlag(InternalFlags::LIF_Incompatible);
						formatf(m_szErrorLocalisationId, "NT_INV_INCOMPATIBLE_CREATOR");
					}
#endif
				}
			}

            // capture invite error
            if(HasInternalFlag(InternalFlags::LIF_Incompatible))
            {
				NetworkInterface::GetInviteMgr().ResolveInvite(
					this,
					InviteAction::IA_Blocked,
					ResolveFlags::RF_SendReply,
					(bBlockedFromInactive ? InviteResponseFlags::IRF_RejectBlockedInactive : InviteResponseFlags::IRF_RejectBlocked));

				// remove any confirmation flags
				RemoveInternalFlag(InternalFlags::LIF_AutoConfirm);
				RemoveInternalFlag(InternalFlags::LIF_Confirmed);
            }

			// Also send real aim type
			m_nAimType = pData->m_nPlayerAim;
        }
	}
	else if(m_SessionPurpose == SessionPurpose::SP_Freeroam)
	{
		if(m_SessionDetail.m_SessionUserDataSize == static_cast<unsigned>(sizeof(SessionUserData_Freeroam)))
		{
			m_nAimType = reinterpret_cast<SessionUserData_Freeroam*>(m_SessionDetail.m_SessionUserData)->m_nPlayerAim;
			m_nNumBosses = reinterpret_cast<SessionUserData_Freeroam*>(m_SessionDetail.m_SessionUserData)->m_nNumBosses;
		}
	}

    // this is possible while we do not always apply the session user data for all sessions members
    // that would be required on 360 where invites are routed through a presence session
	if(m_nAimType >= CPedTargetEvaluator::MAX_TARGETING_OPTIONS)
    {
        // fall back to matchmaking value
        const u32* pAimValue = config.m_Attrs.GetValueById(player_schema::SchemaBase::FIELD_ID_MMATTR_AIM_TYPE);
        if(!pAimValue)
        {
            gnetError("GetConfigDetails :: No aim setting! Failing...");
            return false;
        }

        // the assisted aiming modes (casual / normal) are combined into one session bucket
        switch(*pAimValue)
        {
		case MatchmakingAimBucket::AimBucket_Assisted:
            m_nAimType = CPedTargetEvaluator::TARGETING_OPTION_GTA_TRADITIONAL;
            break;
		case MatchmakingAimBucket::AimBucket_Free:
            m_nAimType = CPedTargetEvaluator::TARGETING_OPTION_FREEAIM;
            break;
        default:
            m_nAimType = CPedTargetEvaluator::TARGETING_OPTION_GTA_TRADITIONAL;
            break;
        }
    }

	// logging
	gnetDebug1("=== Invite Config ===");
	gnetDebug1("  Token            = Token: 0x%016" I64FMT "x", m_SessionDetail.m_SessionInfo.GetToken().m_Value);
	gnetDebug1("  Host             = %s", m_SessionDetail.m_HostName);
	gnetDebug1("  Game Mode        = %d", config.m_Attrs.GetGameMode());
	gnetDebug1("  Public Slots     = %d / %d", m_SessionDetail.m_NumFilledPublicSlots, config.m_MaxPublicSlots);
	gnetDebug1("  Private Slots    = %d / %d", m_SessionDetail.m_NumFilledPrivateSlots, config.m_MaxPrivateSlots);
	gnetDebug1("  Aim Type         = %d", m_nAimType);
	gnetDebug1("  Discriminator    = 0x%08x", *pDisc);
	gnetDebug1("  UserDataSize     = %u", m_SessionDetail.m_SessionUserDataSize);
	gnetDebug1("  Session Purpose  = %s", NetworkUtils::GetSessionPurposeAsString(m_SessionPurpose));

#if RSG_OUTPUT
	// formatting is intentional for lining up with above
	if(m_SessionPurpose == SessionPurpose::SP_Freeroam)
	{
	gnetDebug1("   NumBosses       = %u", m_nNumBosses);
	}
	else if(m_SessionPurpose == SessionPurpose::SP_Freeroam)
	{
	gnetDebug1("   ActivityType    = %d", m_nActivityType);
	gnetDebug1("   ActivityId      = 0x%08x", m_nActivityID);
	}
#endif
	
	// retrieved details
	AddInternalFlag(InternalFlags::LIF_ValidConfig);
    return true; 
}

void sAcceptedInvite::Update(const int timeStep)
{
	// if we have invalid session info, we have nothing to do
	if(!IsPending())
		return;

	switch(m_State)
	{
	case STATE_IDLE:
		{

		}
		break;

	case STATE_CHECK_ONLINE_STATUS:
		{
			if(!NetworkInterface::IsLocalPlayerOnline())
			{
				if(!CLiveManager::IsSystemUiShowing())
				{
					gnetDebug1("AcceptedInvite :: Not online. Prompting sign in.");
					m_State = STATE_CHECK_SYSTEM_UI;

					// trigger UI
					CLiveManager::ShowSigninUi();
				}
			}
			else
			{
				gnetDebug1("AcceptedInvite :: Online - %s", CLiveManager::IsSystemUiShowing() ? "Checking SystemUI" : "Querying Session");
				m_State = CLiveManager::IsSystemUiShowing() ? STATE_CHECK_SYSTEM_UI : STATE_QUERY_SESSION;
			}
		}
		break;

	case STATE_CHECK_SYSTEM_UI:
		if(!CLiveManager::IsSystemUiShowing())
		{
			gnetDebug1("AcceptedInvite :: System UI closed");
			m_State = STATE_QUERY_SESSION;
		}
		break;

	case STATE_QUERY_SESSION:
		{
			// decrement retry timer
			if(m_QueryRetryTimer > 0)
			{
				m_QueryRetryTimer -= timeStep;
				if(m_QueryRetryTimer < 0)
					m_QueryRetryTimer = 0;
			}

			// script must be able to respond
		    if(m_QueryRetryTimer == 0 && (CNetwork::IsScriptReadyForEvents() || NetworkInterface::IsOnBootUi()))
			{
				if(m_SessionInfo.GetHostPeerAddress().IsLocal())
				{
					if(m_QueryNumRetries < QUERY_RETRY_MAX)
					{
						m_QueryNumRetries++;
						m_QueryRetryTimer = QUERY_RETRY_PRESENCE_WAIT_MS;
						gnetDebug1("AcceptedInvite :: Local Session! Looking up inviter via presence in %dms [%d/%d]", QUERY_RETRY_PRESENCE_WAIT_MS, m_QueryNumRetries, QUERY_RETRY_MAX);
						m_State = STATE_QUERY_PRESENCE;
					}
					else
					{
						AddInternalFlag(InternalFlags::LIF_CompletedQuery);
						m_State = STATE_IDLE;
					}
				}
				else if(rlSessionManager::IsInSession(m_SessionInfo))
				{
					if(m_QueryNumRetries < QUERY_RETRY_MAX)
					{
						m_QueryNumRetries++;
						m_QueryRetryTimer = QUERY_RETRY_PRESENCE_WAIT_MS;
						gnetDebug1("AcceptedInvite :: Already in session! Looking up inviter via presence in %dms [%d/%d]", QUERY_RETRY_PRESENCE_WAIT_MS, m_QueryNumRetries, QUERY_RETRY_MAX);
						m_State = STATE_QUERY_PRESENCE;
					}
					else
					{
						AddInternalFlag(InternalFlags::LIF_CompletedQuery);
						m_State = STATE_IDLE;
					}
				}
				else
				{
					gnetDebug1("AcceptedInvite :: Querying Session Detail");

					static const unsigned HOST_QUERY_MAX_ATTEMPTS = 6;

					// run detail query
					// for SCTV, we want to join the freeroam session, so do not pass in a target gamer handle
					bool bSuccess = rlSessionManager::QueryDetail(
						RL_NETMODE_ONLINE,
						NETWORK_SESSION_GAME_CHANNEL_ID,
						0,
						HOST_QUERY_MAX_ATTEMPTS,
						0,
						true,
						&m_SessionInfo,
						1,
						IsSCTV() ? rlGamerHandle() : m_Inviter,
						&m_SessionDetail,
						&m_NumDetails,
						&m_TaskStatus);

					// reset flag
					if(bSuccess)
					{
						gnetDebug1("AcceptedInvite :: Querying session");
						m_State = STATE_QUERYING_SESSION;
					}
					else
					{
						gnetDebug1("AcceptedInvite :: Query failed");
						AddInternalFlag(InternalFlags::LIF_CompletedQuery);
						m_State = STATE_IDLE;
					}
				}
			}
		}
		break;

	case STATE_QUERYING_SESSION:
		{
		    if(!m_TaskStatus.Pending())
			{
				gnetDebug1("AcceptedInvite :: Config Query Finished. Status: %s, Num Details: %u", m_TaskStatus.Succeeded() ? "SUCCEEDED" : "FAILED", m_NumDetails);

                bool bRetryQuery = false; 

				// check that the task succeeded and that we retrieved our details
				if(m_TaskStatus.Succeeded() && m_NumDetails > 0)
				{
					gnetDebug1("AcceptedInvite :: Populating config details - Host: %s, Token: 0x%016" I64FMT "x",
						m_SessionDetail.m_HostName,
						m_SessionDetail.m_SessionInfo.GetToken().m_Value);

                    if(!GetConfigDetails())
                    {
                        gnetDebug1("AcceptedInvite :: Invalid config. Retrying...");
                        bRetryQuery = true;
                    }

#if RSG_XBOX
					if(m_UserContentStatus.Pending())
						m_State = STATE_CHECKING_CONTENT_PERMISSIONS;
					else
#endif
					{
						// completed
						AddInternalFlag(InternalFlags::LIF_CompletedQuery);
						m_State = STATE_IDLE;
					}
				}
                else
                {
                    bRetryQuery = true;
                }

				if(bRetryQuery)
				{
					// failed to get a result - if the inviter is valid, try to look up what session they are in via presence
					if(m_QueryNumRetries < QUERY_RETRY_MAX)
					{
						bool isInviterValid = m_Inviter.IsValid();
						if(isInviterValid)
						{
							if(m_Inviter == NetworkInterface::GetLocalGamerHandle())
							{
#if RSG_PROSPERO
								// on PS5, for joins, we won't actually have a valid target handle, the handle will be set to the local gamer
								// in this case, allow a retry on the details query to workaround any potential issues that impacted the remote
								// hosts ability to reply promptly
								if(m_QueryNumRetries < QUERY_RETRY_MAX)
								{
									m_QueryNumRetries++;
									m_QueryRetryTimer = QUERY_RETRY_DETAILS_WAIT_MS;
									gnetDebug1("AcceptedInvite :: Local Inviter! Retrying details query in %dms [%d/%d]", QUERY_RETRY_DETAILS_WAIT_MS, m_QueryNumRetries, QUERY_RETRY_MAX);
									m_State = STATE_QUERY_SESSION;
								}
								else
#endif
								{
									isInviterValid = false;
								}
							}
							else
							{
								m_QueryNumRetries++;
								m_QueryRetryTimer = QUERY_RETRY_PRESENCE_WAIT_MS;

								gnetDebug1("AcceptedInvite :: Looking up inviter via presence in %dms [%d/%d]", QUERY_RETRY_PRESENCE_WAIT_MS, m_QueryNumRetries, QUERY_RETRY_MAX);
								m_State = STATE_QUERY_PRESENCE;
							}
						}
						
						if(!isInviterValid)
						{
							gnetDebug1("AcceptedInvite :: Invalid Inviter: %s. Exiting process", NetworkUtils::LogGamerHandle(m_Inviter));
							AddInternalFlag(InternalFlags::LIF_CompletedQuery);
							m_State = STATE_IDLE;
						}
					}
					else
					{
						AddInternalFlag(InternalFlags::LIF_CompletedQuery);
						m_State = STATE_IDLE;
					}
				}
			}
		}
		break;

	case STATE_QUERY_PRESENCE:
		{
			// decrement retry timer
			if(m_QueryRetryTimer > 0)
			{
				m_QueryRetryTimer -= timeStep;
				if(m_QueryRetryTimer < 0)
					m_QueryRetryTimer = 0;
			}

			// wait for our timer
			if(m_QueryRetryTimer == 0)
			{
				// reset task status and make request
				m_TaskStatus.Reset();
				bool bSuccess = rlPresenceQuery::GetSessionByGamerHandle(NetworkInterface::GetLocalGamerIndex(),
																			&m_Inviter,
																			1,
																			&m_PresenceInfo,
																			1,
																			&m_NumDetails,
																			&m_TaskStatus);

				// reset flag
				if(bSuccess)
				{
					gnetDebug1("AcceptedInvite :: Querying presence");
					m_State = STATE_QUERYING_PRESENCE;
				}
				else
				{
					gnetDebug1("AcceptedInvite :: Presence query failed");
					AddInternalFlag(InternalFlags::LIF_CompletedQuery);
					m_State = STATE_IDLE;
				}
	        }
		}
		break;

	case STATE_QUERYING_PRESENCE:
		{
		    if(!m_TaskStatus.Pending())
			{
				gnetDebug1("AcceptedInvite :: Presence Query Finished. Status: %s, Num Details: %u", m_TaskStatus.Succeeded() ? "SUCCEEDED" : "FAILED", m_NumDetails);

				// check that the task succeeded and that we retrieved our details
			    if(m_TaskStatus.Succeeded())
			    {
				    bool bQuerySession = false;
                    if(m_NumDetails > 0)
                    {
                        gnetDebug1("AcceptedInvite :: Presence Query Token: 0x%016" I64FMT "x", m_PresenceInfo.m_SessionInfo.GetToken().m_Value);

#if !__NO_OUTPUT
                        if(m_PresenceInfo.m_SessionInfo == m_SessionInfo)
						{
							// this means we've retrieve the same details from presence that we already had
                            gnetDebug1("AcceptedInvite :: Matches existing session details");
						}
#endif

						// check if the session token we've been given matches the local player's freeroam / main session
						if(CNetwork::GetNetworkSession().IsMySessionToken(m_PresenceInfo.m_SessionInfo.GetToken()))
						{
							// this could be for a transition session / unlaunched lobby - check to see if the target player is in our session 
							// and has advertised a transition session - if so, switch our session info to that
							const CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(m_Inviter);
							if(pPlayer && pPlayer->HasStartedTransition() && pPlayer->GetTransitionSessionInfo().IsValid())
							{
								// check we are not in the transition already...
								if(!CNetwork::GetNetworkSession().IsMyTransitionToken(pPlayer->GetTransitionSessionInfo().GetToken()))
								{
									gnetDebug1("AcceptedInvite :: Given local player main session. Switching to Inviter Transition: 0x%016" I64FMT "x", pPlayer->GetTransitionSessionInfo().GetToken().m_Value);
									m_SessionInfo = pPlayer->GetTransitionSessionInfo();
									bQuerySession = true;
								}
								else
								{
									// if we're already in both sessions, silently cancel the invite
									gnetDebug1("AcceptedInvite :: Already in main and transition sessions.");
									AddInternalFlag(InternalFlags::LIF_CompletedQuery);
									AddInternalFlag(InternalFlags::LIF_ValidConfig);
									m_State = STATE_IDLE;
								}
							}
						}
					
						if(!HasInternalFlag(InternalFlags::LIF_CompletedQuery))
						{
							// use what we were given if we didn't already re-assign
							if(!bQuerySession)
							{
								bQuerySession = true; 
								m_SessionInfo = m_PresenceInfo.m_SessionInfo;
							}
							m_State = STATE_QUERY_SESSION;
						}
                    }

				    if(!bQuerySession && !HasInternalFlag(InternalFlags::LIF_CompletedQuery))
				    {
					    // failed to get a result - if the inviter 
					    if(m_QueryNumRetries < QUERY_RETRY_MAX)
					    {
						    if(m_Inviter.IsValid() && m_Inviter != NetworkInterface::GetLocalGamerHandle())
						    {
							    m_QueryNumRetries++;
							    m_QueryRetryTimer = QUERY_RETRY_PRESENCE_WAIT_MS;

							    gnetDebug1("AcceptedInvite :: Looking up inviter via presence in %dms [%d/%d]", QUERY_RETRY_PRESENCE_WAIT_MS, m_QueryNumRetries, QUERY_RETRY_MAX);
							    m_State = STATE_QUERY_PRESENCE;
						    }
						    else
						    {
							    gnetDebug1("AcceptedInvite :: Invalid Inviter: %s. Exiting process", NetworkUtils::LogGamerHandle(m_Inviter));
								AddInternalFlag(InternalFlags::LIF_CompletedQuery);
								m_State = STATE_IDLE;
						    }
					    }
					    else
					    {
							AddInternalFlag(InternalFlags::LIF_CompletedQuery);
							m_State = STATE_IDLE;
					    }
				    }
			    }
				else
				{
				    // failed, just exit
					AddInternalFlag(InternalFlags::LIF_CompletedQuery);
					m_State = STATE_IDLE;
			    }
			}
		}
		break; 

#if RSG_XBOX
	case STATE_CHECKING_CONTENT_PERMISSIONS:
		{
			if(!m_UserContentStatus.Pending())
			{
				gnetDebug1("AcceptedInvite :: User Content - %s, Allowed: %s, DenyReason: %s", m_UserContentStatus.Succeeded() ? "Succeeded" : "Failed", m_UserContentResult.m_isAllowed ? "True" : "False", m_UserContentResult.m_denyReason);
				
				if(!m_UserContentResult.m_isAllowed)
				{
					AddInternalFlag(InternalFlags::LIF_Incompatible); 
					formatf(m_szErrorLocalisationId, "NT_INV_INCOMPATIBLE_CREATOR");

					// Display the Xbox system UI telling the user they can't access UGC.
					CLiveManager::ResolvePlatformPrivilege(NetworkInterface::GetLocalGamerIndex(), rlPrivileges::PrivilegeTypes::PRIVILEGE_USER_CREATED_CONTENT, true);

					// reply to inviter with blocked status
					NetworkInterface::GetInviteMgr().ResolveInvite(
						this,
						InviteAction::IA_Blocked,
						ResolveFlags::RF_SendReply,
						InviteResponseFlags::IRF_RejectBlocked);
				}

				// finished
				AddInternalFlag(InternalFlags::LIF_CompletedQuery);
				m_State = STATE_IDLE;
			}
		}
		break;
#endif
	}
}

bool
sAcceptedInvite::Init(const rlSessionInfo& sessionInfo,
				 const rlGamerInfo& invitee,
				 const rlGamerHandle& inviter,
				 const unsigned nInviteId, 
				 const unsigned nInviteFlags,
				 const unsigned nInternalFlags,
				 const int nGameMode,
				 const char* pUniqueMatchId,
				 const int nInviteFrom,
				 const int nInviteType)
{
    gnetAssert(sessionInfo.IsValid());
    gnetAssert(!this->IsPending());

	this->Clear();
    m_SessionInfo = sessionInfo;
    m_Invitee = invitee;
    m_Inviter = inviter;
	m_InviteFlags = nInviteFlags;
	m_InternalFlags = nInternalFlags;
	m_InviteId = nInviteId;
    m_nGameMode = nGameMode;
	safecpy(m_UniqueMatchId, pUniqueMatchId);
	m_InviteFrom = nInviteFrom;
	m_InviteType = nInviteType;

	gnetDebug1("AcceptedInvite :: Init");
	gnetDebug1("\tFrom: %s", NetworkUtils::LogGamerHandle(m_Inviter));
	gnetDebug1("\tToken: 0x%016" I64FMT "x", m_SessionInfo.GetToken().m_Value);
	gnetDebug1("\tInviteId: 0x%08x", m_InviteId);
	OUTPUT_ONLY(LogInviteFlags(m_InviteFlags, "AcceptedInvite::Init"));
	OUTPUT_ONLY(LogInternalFlags(m_InternalFlags, "AcceptedInvite::Init"));

    const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();
	if(pInfo && (*pInfo != m_Invitee))
    {
		// log out details
		gnetDebug1("AcceptedInvite :: For Inactive Gamer! Active: %s, Invited: %s, FromActive: %s", pInfo->GetName(), m_Invitee.GetName(), (m_Inviter == pInfo->GetGamerHandle()) ? "True" : "False");

		// check if the active gamer is already in this session
		NetworkGameConfig* pConfig = CNetwork::GetNetworkSession().GetInviteDetails(&m_SessionInfo);
		if(pConfig)
		{
			// log that we already 
			gnetDebug1("AcceptedInvite :: Active gamer in target session. Populating details from local config.");
			
			// we have the config already
			AddInternalFlag(InternalFlags::LIF_ValidConfig);
			AddInternalFlag(InternalFlags::LIF_CompletedQuery);

			// we use the detail response, fill this in
			m_nGameMode = pConfig->GetGameMode();
			m_SessionDetail.m_SessionInfo = m_SessionInfo;

			// grab config details
			m_SessionPurpose = static_cast<SessionPurpose>(pConfig->GetSessionPurpose());
			m_nAimType = pConfig->GetAimBucket();

			// use config logging
			gnetDebug1("=== Invite Config ==="); 
			gnetDebug1("  Token            = Token: 0x%016" I64FMT "x", m_SessionDetail.m_SessionInfo.GetToken().m_Value);
			gnetDebug1("  Game Mode        = %d", m_nGameMode);
			gnetDebug1("  Session Purpose  = %s", NetworkUtils::GetSessionPurposeAsString(m_SessionPurpose));

			if(m_SessionPurpose == SessionPurpose::SP_Activity)
			{
				m_nActivityType = static_cast<int>(pConfig->GetActivityType());
				m_nActivityID = pConfig->GetActivityID();
				gnetDebug1("  Activity Type    = %d", m_nActivityType);
				gnetDebug1("  Activity ID      = 0x%08x", m_nActivityID);
			}
		}
	}

    if(!HasInternalFlag(InternalFlags::LIF_CompletedQuery))
	    m_State = STATE_CHECK_ONLINE_STATUS;
    else
    {
        m_TaskStatus.SetPending();
        m_TaskStatus.SetSucceeded();
    }

    m_QueryNumRetries = 0;
    m_QueryRetryTimer = 0;

	return true;
}

///////////////////////////////////////////////////////////////////////////////
//  sPlatformInvite
///////////////////////////////////////////////////////////////////////////////

void sPlatformInvite::Init(const rlGamerInfo& hInvitee, const rlGamerHandle& hInviter, const rlSessionInfo& sessionInfo, const unsigned nInviteFlags)
{
	m_Invitee = hInvitee;
	m_Inviter = hInviter;
	m_SessionInfo = sessionInfo;
	m_Flags = nInviteFlags | InviteFlags::IF_IsPlatform;
	m_InviteId = InviteMgr::GenerateInviteId();
	m_TimeAccepted = sysTimer::GetSystemMsTime();

#if !__FINAL
	// apply auto accept flag if we request via command line
	if (PARAM_netAutoActionPlatformInvites.Get())
		m_Flags |= InviteFlags::IF_AutoAccept;
#endif

	gnetDebug1("PlatformInvite::Init :: Invitee: %s, Inviter: %s, Session: 0x%016" I64FMT "x, Flags: 0x%x, InviteId: 0x%08x", hInvitee.GetName(), hInviter.ToString(), sessionInfo.GetToken().GetValue(), nInviteFlags, m_InviteId);
}

void sPlatformInvite::Clear()
{
	m_Invitee.Clear();
	m_Inviter.Clear();
	m_SessionInfo.Clear();
	m_Flags = 0;
	m_InviteId = 0;
	m_TimeAccepted = 0;
}

bool sPlatformInvite::IsValid() const
{
	return m_TimeAccepted > 0;
}

///////////////////////////////////////////////////////////////////////////////
//  UnacceptedInvite
///////////////////////////////////////////////////////////////////////////////

unsigned UnacceptedInvite::ms_IdCount = 0;

UnacceptedInvite::UnacceptedInvite()
{
    this->Clear();
}

bool
UnacceptedInvite::IsValid() const
{
    return m_SessionInfo.IsValid();
}

//private:

void
UnacceptedInvite::Clear()
{
    m_SessionInfo.Clear();
    m_Inviter.Clear();
    m_InviterName[0] = '\0';
    m_Salutation[0] = '\0';
    m_Timeout = 0;
	m_Id = 0xFFFFFFFF;
}

void
UnacceptedInvite::Init(const rlSessionInfo& sessionInfo,
						const rlGamerHandle& inviter,
						const char* inviterName,
						const char* salutation)
{
	this->Clear();
	Reset(sessionInfo, inviter, inviterName, salutation);
	m_Id = ms_IdCount++;
}

void
UnacceptedInvite::Reset(const rlSessionInfo& sessionInfo,
						const rlGamerHandle& inviter,
						const char* inviterName,
						const char* salutation)
{
	gnetAssert(inviterName);
	gnetAssert(strlen(inviterName) > 0);
	gnetAssert(salutation);
    gnetAssert(sessionInfo.IsValid());
    gnetAssert(inviter.IsValid());

	m_SessionInfo = sessionInfo;
	m_Inviter = inviter;
	m_Timeout = UNACCEPTED_INVITE_TIMEOUT;
	safecpy(m_InviterName, inviterName, COUNTOF(m_InviterName));
	safecpy(m_Salutation, salutation, COUNTOF(m_Salutation));

	// do not change m_Id here, this is used to refresh existing invites
}

void
UnacceptedInvite::Update(const int timeStep)
{
    m_Timeout -= timeStep;

    if(m_Timeout <= 0)
    {
        m_Timeout = 0;
    }
}

bool
UnacceptedInvite::TimedOut() const
{
    return m_Timeout <= 0;
}

///////////////////////////////////////////////////////////////////////////////
//  RsInvite
///////////////////////////////////////////////////////////////////////////////

sRsInvite::sRsInvite()
{
	this->Clear();
}

bool sRsInvite::IsValid() const
{
	return m_SessionInfo.IsValid();
}

//private:

void sRsInvite::Clear()
{
	m_Invitee.Clear();
	m_SessionInfo.Clear();
	m_Inviter.Clear();
	m_InviterName[0] = '\0';
	m_szContentId[0] = '\0';
	m_nPlaylistLength = 0; 
	m_nPlaylistCurrent = 0;
    m_TournamentEventId = 0;
	m_TimeAdded = 0;
	m_InviteFlags = InviteFlags::IF_None;
	m_CrewId = RL_INVALID_CLAN_ID;
	m_InviteId = 0;
#if RSG_XBOX
	m_bAcknowledgedPermissionResult = false; 
#endif
}

bool sRsInvite::Init(
		const rlGamerInfo& hInfo,
		const rlSessionInfo& hSessionInfo,
		const rlGamerHandle& hFrom,
		const char* szGamerName,
		const unsigned nInviteId,
		const int nGameMode,
		unsigned nCrewId,
		const char* szContentId,
		const char* pUniqueMatchId,
		const int nInviteFrom,
		const int nInviteType,
		const unsigned nPlaylistLength,
		const unsigned nPlaylistCurrent,
		const unsigned nFlags)
{
	// verify that we've been given good data
	if(!gnetVerifyf(hInfo.IsValid(), "RsInvite :: Invalid invitee gamer info!"))
		return false;
	if(!gnetVerifyf(hSessionInfo.IsValid(), "RsInvite :: Invalid session info!"))
		return false;
	if(!gnetVerifyf(hFrom.IsValid(), "RsInvite :: Invalid inviter gamer handle!"))
		return false;
	if(!gnetVerifyf(strlen(szGamerName) > 0, "RsInvite :: Invalid gamer name!"))
		return false;

	// clearing out info
	Clear();

	// copy in parameters
	m_Invitee = hInfo;
	m_SessionInfo = hSessionInfo;
	m_Inviter = hFrom;
	safecpy(m_InviterName, szGamerName);

	m_InviteId = nInviteId;

	m_GameMode = nGameMode;
	safecpy(m_szContentId, szContentId);
	safecpy(m_UniqueMatchId, pUniqueMatchId);
	m_InviteFrom = nInviteFrom;
	m_InviteType = nInviteType;
	m_nPlaylistLength = nPlaylistLength;
	m_nPlaylistCurrent = nPlaylistCurrent;
	m_InviteFlags = nFlags | InviteFlags::IF_AllowPrivate | InviteFlags::IF_Rockstar;
	m_CrewId = nCrewId;
	m_TimeAdded = sysTimer::GetSystemMsTime();

	// success
	return true;
}

bool sRsInvite::InitAdmin(const rlGamerInfo& hInfo, const rlSessionInfo& hSessionInfo, const rlGamerHandle& hSessionHost, const unsigned inviteFlags, const InviteId inviteId)
{
	// verify that we've been given good data
	if(!gnetVerifyf(hInfo.IsValid(), "InitAdmin :: Invalid invitee gamer info!"))
		return false;
	if(!gnetVerifyf(hSessionInfo.IsValid(), "InitAdmin :: Invalid session info!"))
		return false;
	if(!gnetVerifyf(hSessionHost.IsValid(), "InitAdmin :: Invalid inviter gamer handle!"))
		return false;

	// clearing out info
	Clear();

	// copy in parameters
	m_Invitee = hInfo;
	m_SessionInfo = hSessionInfo;
	m_Inviter = hSessionHost;
	m_InviteId = inviteId;
	m_InviteFlags = inviteFlags | InviteFlags::IF_AllowPrivate | InviteFlags::IF_Rockstar | InviteFlags::IF_ViaAdmin;
	m_TimeAdded = sysTimer::GetSystemMsTime();

#if RSG_XBOX
	// we don't need to check this for admin invites so acknowledge automatically
	m_bAcknowledgedPermissionResult = true;
#endif

	// success
	return true;
}

bool sRsInvite::InitTournament(const rlGamerInfo& hInfo, const rlSessionInfo& hSessionInfo, const rlGamerHandle& hSessionHost, const char* szContentId, unsigned tournamentEventId, unsigned inviteFlags, int inviteId)
{
    // verify that we've been given good data
	if(!gnetVerifyf(hInfo.IsValid(), "InitTournament :: Invalid invitee gamer info!"))
		return false;
	if(!gnetVerifyf(hSessionInfo.IsValid(), "InitTournament :: Invalid session info!"))
        return false;

    // clearing out info
    Clear();

    // copy in parameters
	m_Invitee = hInfo;
	m_SessionInfo = hSessionInfo;
	m_Inviter = hSessionHost;
	safecpy(m_szContentId, szContentId, COUNTOF(m_szContentId));
    m_InviteId = inviteId;
    m_InviteFlags = inviteFlags | InviteFlags::IF_AllowPrivate | InviteFlags::IF_Rockstar | InviteFlags::IF_IsTournament;
    m_TournamentEventId = tournamentEventId;
	m_TimeAdded = sysTimer::GetSystemMsTime();

#if RSG_XBOX
	// we don't need to check this for tournaments so acknowledge automatically
	m_bAcknowledgedPermissionResult = true; 
#endif

    // success
    return true;
}

///////////////////////////////////////////////////////////////////////////////
//  InviteMgr
///////////////////////////////////////////////////////////////////////////////

unsigned DEFAULT_INVITE_VALID_SEND_GROUPS =
	SentFromGroup::Groups_CloseContacts |
	SentFromGroup::Group_RecentPlayer |
	SentFromGroup::Group_SameSession;

InviteMgr::InviteMgr()
	: m_ShownAlertReason(InviteAlertReason::IAR_None)
	, m_LastShownAlertReason(InviteAlertReason::IAR_None)
	, m_PendingAlertReason(InviteAlertReason::IAR_None)
	, m_PendingPlatformInviteFlowState(PendingPlatformInviteFlowState::FS_Inactive)
	, m_PendingPlatformScriptEventCheck(false)
	, m_ShowingConfirmationExternally(false)
	, m_bSuppressProcess(false)
	, m_bConfirmedDifferentUser(false)
	, m_bWaitingForScriptDeath(false)
	, m_bBlockedByScript(false)
	, m_bStoreInviteThroughRestart(false)
	, m_bAllowProcessInPlayerSwitch(false)
	, m_bBlockJoinQueueInvite(false)
	, m_bInviteUnavailablePending(false)
	, m_PendingNetworkAccessAlert(false)
	, m_HasClaimedBootInvite(false)
	, m_HasSkipBootUiResult(false)
	, m_DidSkipBootUi(false)
	, m_CanReceiveRsInvites(true)
	, m_WasBlockedLastCheck(false)
	, m_RsInviteAllowedGroups(DEFAULT_INVITE_VALID_SEND_GROUPS)
{
	m_szFailedScreenSubstring[0] = '\0';
}

InviteMgr::~InviteMgr()
{
	Shutdown();
}

void InviteMgr::Init()
{
	sRng.Reset(sysTimer::GetSystemMsTime());

	m_bSuppressProcess = false;
    m_bBlockedByScript = false;
    m_bBusySpinnerActivated = false;
    m_bAllowProcessInPlayerSwitch = false;
    m_bBlockJoinQueueInvite = false;
	ClearAll();
}

void InviteMgr::Shutdown()
{
	if(!(m_bConfirmedDifferentUser || m_bStoreInviteThroughRestart))
	{
		m_bSuppressProcess = false;
		ClearAll();
		m_AcceptedInvite.Shutdown();
		m_PendingAcceptedInvite.Shutdown();
	}
}

#if !__NO_OUTPUT
enum
{
	INVITE_BLOCKED_NOT_BLOCKED,
	INVITE_BLOCKED_NO_INVITE,
	INVITE_BLOCKED_AUTO_LOAD,
	INVITE_BLOCKED_PLAYER_SWITCH,
	INVITE_BLOCKED_SCRIPT_NOT_READY,
	INVITE_BLOCKED_SUPPRESSED,
	INVITE_BLOCKED_PENDING_SCRIPT_DEATH,
	INVITE_BLOCKED_PENDING_TUNABLES,
	INVITE_BLOCKED_PENDING_WARNING_SCREEN,
	INVITE_BLOCKED_PENDING_CALIBRATION_SCREEN,
	INVITE_BLOCKED_PENDING_SOCIAL_CLUB_MENU,
	INVITE_BLOCKED_PENDING_LOADING_SCREEN,
	INVITE_BLOCKED_PENDING_SYSTEM_UI,
	INVITE_BLOCKED_PENDING_SAVE_STATS,
	INVITE_BLOCKED_NUM,
};

static const char* gs_szInviteBlockedReason[] =
{
	"INVITE_BLOCKED_NOT_BLOCKED",				// INVITE_BLOCKED_NOT_BLOCKED
	"INVITE_BLOCKED_NO_INVITE",					// INVITE_BLOCKED_NO_INVITE
	"INVITE_BLOCKED_AUTO_LOAD",					// INVITE_BLOCKED_AUTO_LOAD
	"INVITE_BLOCKED_PLAYER_SWITCH",				// INVITE_BLOCKED_PLAYER_SWITCH
	"INVITE_BLOCKED_SCRIPT_NOT_READY",			// INVITE_BLOCKED_SCRIPT_NOT_READY
	"INVITE_BLOCKED_SUPPRESSED",				// INVITE_BLOCKED_SUPPRESSED
	"INVITE_BLOCKED_PENDING_SCRIPT_DEATH",		// INVITE_BLOCKED_PENDING_SCRIPT_DEATH
	"INVITE_BLOCKED_PENDING_TUNABLES",			// INVITE_BLOCKED_PENDING_TUNABLES
	"INVITE_BLOCKED_PENDING_WARNING_SCREEN",	// INVITE_BLOCKED_PENDING_WARNING_SCREEN
	"INVITE_BLOCKED_PENDING_CALIBRATION_SCREEN",// INVITE_BLOCKED_PENDING_CALIBRATION_SCREEN
	"INVITE_BLOCKED_PENDING_SOCIAL_CLUB_MENU",	// INVITE_BLOCKED_PENDING_SOCIAL_CLUB_MENU
	"INVITE_BLOCKED_PENDING_LOADING_SCREEN",	// INVITE_BLOCKED_PENDING_LOADING_SCREEN
	"INVITE_BLOCKED_PENDING_SYSTEM_UI",			// INVITE_BLOCKED_PENDING_SYSTEM_UI
	"INVITE_BLOCKED_PENDING_SAVE_STATS",		// INVITE_BLOCKED_PENDING_SAVE_STATS
};

void UpdateInviteBlock(const unsigned nInviteWaiting)
{
	static unsigned s_InviteWaiting = INVITE_BLOCKED_NOT_BLOCKED;
	if(s_InviteWaiting != nInviteWaiting)
	{
		gnetDebug1("UpdateInviteBlock :: Was: %s, Now: %s", gs_szInviteBlockedReason[s_InviteWaiting], gs_szInviteBlockedReason[nInviteWaiting]);
		s_InviteWaiting = nInviteWaiting;
	}
}
#endif

bool InviteMgr::UpdateAndCheckInviteBlocker(const bool isSuppressed)
{
	// if there's no pending invite...
	if(!m_AcceptedInvite.IsPending())
	{
		OUTPUT_ONLY(UpdateInviteBlock(INVITE_BLOCKED_NO_INVITE));
		return true;
	}

	// is the session code waiting
	bool bSessionWaiting = CNetwork::IsNetworkSessionValid() && CNetwork::GetNetworkSession().IsWaitingForInvite();

	if(!bSessionWaiting && CSavegameAutoload::IsInviteConfirmationBlockedDuringAutoload() && !NetworkInterface::IsOnBootUi())
	{
		OUTPUT_ONLY(UpdateInviteBlock(INVITE_BLOCKED_AUTO_LOAD));
		return true;
	}

	if(!bSessionWaiting && NetworkInterface::IsInPlayerSwitch())
	{
		// if we're in the player switch and not in a menu
		if(!m_bAllowProcessInPlayerSwitch && !(CPauseMenu::IsActive(PM_SkipStore | PM_SkipSocialClub) || CPauseMenu::IsCurrentScreenValid()))
		{
			OUTPUT_ONLY(UpdateInviteBlock(INVITE_BLOCKED_PLAYER_SWITCH));
			return true;
		}
	}

	if(!CNetwork::IsScriptReadyForEvents() && !NetworkInterface::IsOnBootUi())
	{
		OUTPUT_ONLY(UpdateInviteBlock(INVITE_BLOCKED_SCRIPT_NOT_READY));
		return true;
	}

	if(isSuppressed)
	{
		OUTPUT_ONLY(UpdateInviteBlock(INVITE_BLOCKED_SUPPRESSED));
		return true;
	}

	if(m_bWaitingForScriptDeath)
	{
		OUTPUT_ONLY(UpdateInviteBlock(INVITE_BLOCKED_PENDING_SCRIPT_DEATH));
		return true;
	}

	// wait for tunables
	if(!Tunables::GetInstance().HasCloudRequestFinished())
	{
		OUTPUT_ONLY(UpdateInviteBlock(INVITE_BLOCKED_PENDING_TUNABLES));
		return true;
	}

	// wait for UI screens that use the warning screen (exclude when the alert is an invite alert)
	if(IsNonInviteAlertActive())
	{
		OUTPUT_ONLY(UpdateInviteBlock(INVITE_BLOCKED_PENDING_WARNING_SCREEN));
		return true;
	}

	if(CPauseMenu::sm_bWaitOnDisplayCalibrationScreen)
	{
		OUTPUT_ONLY(UpdateInviteBlock(INVITE_BLOCKED_PENDING_CALIBRATION_SCREEN));
		return true;
	}

	if(SocialClubMenu::IsActive())
	{
		OUTPUT_ONLY(UpdateInviteBlock(INVITE_BLOCKED_PENDING_SOCIAL_CLUB_MENU));
		return true;
	}

	// wait for the loading screen (this seems to be true in some cases when the landing screen is active)
	// we want to process the invite here on the landing page so let that through
	if(NetworkInterface::IsOnLoadingScreen() && !NetworkInterface::IsOnBootUi())
	{
		OUTPUT_ONLY(UpdateInviteBlock(INVITE_BLOCKED_PENDING_LOADING_SCREEN));
		return true;
	}

	// wait for saves / stats
	if(StatsInterface::SavePending() ||
		StatsInterface::PendingSaveRequests() ||
		CLiveManager::GetProfileStatsMgr().PendingFlush() ||
		CLiveManager::GetProfileStatsMgr().PendingMultiplayerFlushRequest())
	{
		OUTPUT_ONLY(UpdateInviteBlock(INVITE_BLOCKED_PENDING_SAVE_STATS));
		return true;
	}

	// wait until the system UI is not showing
	if(CLiveManager::IsSystemUiShowing())
	{
		OUTPUT_ONLY(UpdateInviteBlock(INVITE_BLOCKED_PENDING_SYSTEM_UI));
		return true;
	}

	// all the way through, indicate that we're not blocked and processing freely
	OUTPUT_ONLY(UpdateInviteBlock(INVITE_BLOCKED_NOT_BLOCKED));

	return false;
}

bool InviteMgr::IsProcessingSuppressed()
{
	// check if the invite should be unsuppressed by the profile settings
	bool bSuppressed = m_bSuppressProcess;
	if(bSuppressed)
	{
		int nSetting = CProfileSettings::GetInstance().GetInt(CProfileSettings::JOB_ACTIVITY_ID_CHAR0);
		if((nSetting & PSFP_FM_INTRO_CAN_ACCEPT_INVITES) != 0)
			bSuppressed = false;
		else
		{
			nSetting = CProfileSettings::GetInstance().GetInt(CProfileSettings::JOB_ACTIVITY_ID_CHAR1);
			if((nSetting & PSFP_FM_INTRO_CAN_ACCEPT_INVITES) != 0)
				bSuppressed = false;
		}
	}

	// different user
	if(bSuppressed && IsAcceptedInviteForDifferentUser())
		bSuppressed = false;

	return bSuppressed;
}

bool InviteMgr::CanGenerateScriptEvents()
{
	return CNetwork::IsScriptReadyForEvents();
}

bool InviteMgr::CheckForInviteErrors()
{
	// current accepted 
	sAcceptedInvite* pInvite = GetAcceptedInvite();
	if(!pInvite || !pInvite->IsPending())
		return false;

	const bool bDifferentUser = IsAcceptedInviteForDifferentUser();

	// wait for profile settings
	if(!bDifferentUser && CProfileSettings::GetInstance().AreSettingsValid())
	{
		if(!CNetwork::CheckSpPrologueRequirement())
		{
			gnetDebug1("CheckForInviteErrors :: Cannot access multiplayer!");
			strcpy(m_szFailedScreenSubstring, "PM_DOPROB");
			m_PendingAlertReason = InviteAlertReason::IAR_CannotAccess;

			// reply to invite, cannot access multiplayer
			ResolveInvite(
				pInvite,
				InviteAction::IA_Blocked,
				ResolveFlags::RF_SendReply,
				InviteResponseFlags::IRF_RejectCannotAccessMultiplayer);
		}
	}

	bool clearInvite = false;
	if(!pInvite->IsProcessing())
	{
		if(!pInvite->HasSucceeded())
		{
			gnetWarning("CheckForInviteErrors :: Failed to retrieve session details!");

			// first time through
			if(!pInvite->HasInternalFlag(InternalFlags::LIF_ProcessedConfig))
			{
				// config has now been processed
				gnetDebug1("CheckForInviteErrors :: Processed Invite Config");
				pInvite->AddInternalFlag(InternalFlags::LIF_ProcessedConfig);
			}

			// check for sign out
			if(!NetworkInterface::IsLocalPlayerOnline())
			{
				gnetDebug1("\tInvite :: Signed offline");
				m_PendingAlertReason = InviteAlertReason::IAR_NotOnline;
			}
			else if(pInvite->IsViaPlatformPartyAny())
			{
				// if we're already in the session, ignore
				if(!CNetwork::GetNetworkSession().IsInSession(pInvite->m_SessionInfo))
				{
					// remove player from platform party
					XDK_ONLY(CNetworkSession::RemoveFromParty(false));

					if(pInvite->IsViaPlatformParty() || pInvite->IsViaPlatformPartyJVP())
					{
						m_PendingAlertReason = InviteAlertReason::IAR_PartyError;
					}
					else
					{
						gnetDebug1("\tInvite :: Via platform party auto-join. Ignoring.");
					}
				}
				else
				{
					gnetDebug1("\tInvite :: Via platform party to same session. Ignoring.");
				}
			}
			else if(pInvite->IsFromJoinQueue())
			{
				gnetDebug1("\tInvite :: Via join queue. Ignoring.");
			}
			else
			{
				m_PendingAlertReason = InviteAlertReason::IAR_SessionError;
			}

			// when we don't succeed, always clear the invite
			clearInvite = true;
		}
		else if(pInvite->IsIncompatible())
		{
			gnetDebug1("CheckForInviteErrors :: Compatibility Error!");
			m_PendingAlertReason = InviteAlertReason::IAR_Incompatibility;
			formatf(m_szFailedScreenSubstring, pInvite->GetErrorLocalisationId());
		}
	}

	if(clearInvite || (m_PendingAlertReason != InviteAlertReason::IAR_None))
	{
		ClearInvite();
		return true;
	}

	return false;
}

void InviteMgr::UpdateAcceptedInvite(const int timeStep)
{
	// current accepted 
	sAcceptedInvite* pInvite = GetAcceptedInvite();

	// cache a couple of parameters
	const bool bDifferentUser = IsAcceptedInviteForDifferentUser();
	const bool bIsProcessingSuppressed = IsProcessingSuppressed();

	// if the invite has not been suppressed (we don't update the invite so that when we unsuppress, we get the latest
	// details at that time)
	if(!bIsProcessingSuppressed)
		pInvite->Update(timeStep);

	// wait for all this to drop
	if((m_bConfirmedDifferentUser || m_bStoreInviteThroughRestart) && m_bWaitingForScriptDeath)
	{
		if(!CanGenerateScriptEvents())
			m_bWaitingForScriptDeath = false;
	}

	// not started the invite
	if(!pInvite->IsPending())
	{
		// check our pending invite
		if(m_PendingAcceptedInvite.IsPending())
		{
			if(CNetwork::GetNetworkSession().IsInSession(m_PendingAcceptedInvite.GetSessionInfo()))
				gnetDebug1("UpdateAcceptedInvite :: Invite has completed. Have pending info - Already in this session");
			else
			{
				gnetDebug1("UpdateAcceptedInvite :: Invite has completed. Have pending info - Processing now.");
				AcceptInvite(
					m_PendingAcceptedInvite.GetSessionInfo(),
					m_PendingAcceptedInvite.GetInvitee(),
					m_PendingAcceptedInvite.GetInviter(),
					m_PendingAcceptedInvite.GetInviteId(),
					m_PendingAcceptedInvite.GetFlags(),
					m_PendingAcceptedInvite.GetGameMode(),
					m_PendingAcceptedInvite.GetUniqueMatchId(),
					m_PendingAcceptedInvite.GetInviteFrom(),
					m_PendingAcceptedInvite.GetInviteType());
			}
			m_PendingAcceptedInvite.Cancel();
		}
	}

	// if we have the session details, kill the busy spinner
	if(!pInvite->IsProcessing() && m_bBusySpinnerActivated)
	{
		CBusySpinner::Off(SPINNER_SOURCE_INVITE_DETAILS);
		m_bBusySpinnerActivated = false;
	}

	// check for failure conditions 
	// the below checks will block any positive invite action but we should still action a failure
	if(CheckForInviteErrors())
		return;

	// check if the invite is blocked
	m_WasBlockedLastCheck = UpdateAndCheckInviteBlocker(bIsProcessingSuppressed);
	if(m_WasBlockedLastCheck)
		return;

	// wait for all this to drop
	if(m_bConfirmedDifferentUser && !bDifferentUser)
	{
		gnetDebug1("UpdateAcceptedInvite :: Processing different user invite confirmation!");

		// fire a script event
		CEventNetworkInviteAccepted inviteEvent(pInvite->GetInviter(),
			pInvite->m_nGameMode,
			pInvite->m_SessionPurpose,
			pInvite->IsViaParty(),
			pInvite->m_nAimType,
			pInvite->m_nActivityType,
			pInvite->m_nActivityID,
			pInvite->IsSCTV(),
			pInvite->GetFlags(),
			pInvite->m_nNumBosses);

		GetEventScriptNetworkGroup()->Add(inviteEvent);
		NotifyPlatformInviteAccepted(pInvite);

		// reset flag
		m_bConfirmedDifferentUser = false;

		// fast forward
		pInvite->AddInternalFlag(InternalFlags::LIF_Accepted);
		pInvite->AddInternalFlag(InternalFlags::LIF_ProcessedConfig);
		pInvite->AddInternalFlag(InternalFlags::LIF_AutoConfirm);
		pInvite->AddInternalFlag(InternalFlags::LIF_KnownToScript);
	}

	// wait for all this to drop
	if(m_bStoreInviteThroughRestart)
	{
		gnetDebug1("UpdateAcceptedInvite :: Processing invite stored through restart!");

		// fire a script event
		CEventNetworkInviteAccepted inviteEvent(pInvite->GetInviter(),
			pInvite->m_nGameMode,
			pInvite->m_SessionPurpose,
			pInvite->IsViaParty(),
			pInvite->m_nAimType,
			pInvite->m_nActivityType,
			pInvite->m_nActivityID,
			pInvite->IsSCTV(),
			pInvite->GetFlags(),
			pInvite->m_nNumBosses);

		GetEventScriptNetworkGroup()->Add(inviteEvent);
		NotifyPlatformInviteAccepted(pInvite);

		m_bStoreInviteThroughRestart = false;

		// fast forward
		pInvite->AddInternalFlag(InternalFlags::LIF_Accepted);
		pInvite->AddInternalFlag(InternalFlags::LIF_ProcessedConfig);
		pInvite->AddInternalFlag(InternalFlags::LIF_AutoConfirm);
		pInvite->AddInternalFlag(InternalFlags::LIF_KnownToScript);
	}

	// check for any conditions that mean we can auto confirm the invite without a warning screen...
	if(!pInvite->HasInternalFlag(InternalFlags::LIF_AutoConfirm))
	{
		// if we're already in multiplayer and this is a presence or follow invite, skip alert (already confirmed using in-game systems)
		if(CNetwork::IsNetworkSessionValid() && (CNetwork::GetNetworkSession().IsSessionEstablished() || CNetwork::GetNetworkSession().IsTransitionEstablished()))
		{
			if((pInvite->IsRockstar() && !pInvite->IsRockstarToInactive()) || pInvite->IsFollow())
			{
				gnetDebug1("UpdateAcceptedInvite :: Adding AutoConfirm, R* Invite");
				pInvite->AddInternalFlag(InternalFlags::LIF_AutoConfirm);
			}
		}

		// this is applied in AcceptInvite but we can be moving back to the landing from the title 
		if(NetworkInterface::IsOnBootUi())
		{
			gnetDebug1("UpdateAcceptedInvite :: Adding AutoConfirm, on BootUi");
			pInvite->AddInternalFlag(InternalFlags::LIF_AutoConfirm);
		}
	}

	// waiting on session details
	if(pInvite->IsProcessing())
	{
		// don't show if this arrived via an in-party invite or if the invitee not the main profile
		if(NetworkInterface::IsLocalPlayerOnline() && !pInvite->IsViaParty() && !m_bBlockedByScript && !pInvite->IsFromJoinQueue())
		{
			if(!CBusySpinner::IsOn())
			{
				// track this
				m_bBusySpinnerActivated = true;

				// kick off busy spinner
				char cHtmlFormattedBusyString[256];
				char* html = CTextConversion::TextToHtml(TheText.Get("NT_INV_CONFIG"), cHtmlFormattedBusyString, sizeof(cHtmlFormattedBusyString));
				CBusySpinner::On(html, SPINNER_ICON_LOADING, SPINNER_SOURCE_INVITE_DETAILS);
			}
		}

		// check for sign out
		if(!NetworkInterface::IsLocalPlayerOnline())
		{
			gnetDebug1("UpdateAcceptedInvite :: Signed offline while processing");

			// cancel the invite
			pInvite->Cancel();

			// show appropriate alert
			m_PendingAlertReason = InviteAlertReason::IAR_NotOnline;

			if(m_bBusySpinnerActivated)
			{
				gnetDebug1("UpdateAcceptedInvite :: Turning the spinner off");
				m_bBusySpinnerActivated = false;
				CBusySpinner::Off(SPINNER_SOURCE_INVITE_DETAILS);
			}
		}
	}
	else if(pInvite->HasSucceeded())
	{
		// flag to determine whether the invite should be cleared
		bool bClearInvite = false;

		// first time through
		if(!pInvite->HasInternalFlag(InternalFlags::LIF_ProcessedConfig))
		{
			gnetDebug1("UpdateAcceptedInvite :: Processed Invite Config");

			// config has now been processed
			pInvite->AddInternalFlag(InternalFlags::LIF_ProcessedConfig);

			// check if this is a session we are already in...
			if(!m_bBlockedByScript && !bDifferentUser && CNetwork::GetNetworkSession().IsInSession(pInvite->GetSessionInfo()))
			{
				if(pInvite->HasInternalFlag(InternalFlags::LIF_FromBoot))
				{
					gnetDebug1("UpdateAcceptedInvite :: Already in session requested by boot invite. Ignoring invite...");
					bClearInvite = true;
				}
				else
				{
					gnetDebug1("UpdateAcceptedInvite :: Already in this session!");
					m_PendingAlertReason = InviteAlertReason::IAR_AlreadyInSession;
				}
			}
			else if(!bDifferentUser)
			{
				// flag accepted
				pInvite->AddInternalFlag(InternalFlags::LIF_Accepted);
				pInvite->AddInternalFlag(InternalFlags::LIF_KnownToScript);

				// fire a script event
				CEventNetworkInviteAccepted inviteEvent(pInvite->GetInviter(),
					pInvite->m_nGameMode,
					pInvite->m_SessionPurpose,
					pInvite->IsViaParty(),
					pInvite->m_nAimType,
					pInvite->m_nActivityType,
					pInvite->m_nActivityID,
					pInvite->IsSCTV(),
					pInvite->GetFlags(),
					pInvite->m_nNumBosses);

				gnetDebug1("UpdateAcceptedInvite :: Generating EVENT_NETWORK_INVITE_ACCEPTED!");
				GetEventScriptNetworkGroup()->Add(inviteEvent);
				NotifyPlatformInviteAccepted(pInvite);
			}
		}

		// check whether we have flagged this invite to be cleared
		if(bClearInvite)
			ClearInvite();
		// ...if we haven't failed the invite
		else if(m_PendingAlertReason == InviteAlertReason::IAR_None)
		{
			// reasons why we might not want to process the invite
			bool bConfirmInvite = true;
			bConfirmInvite &= (!cStoreScreenMgr::IsStoreMenuOpen());

			// following checks only relevant for active player
			if(!bDifferentUser)
			{
				bConfirmInvite &= (!m_bBlockedByScript);

				// switched so that all states are shown here. 
				if(bConfirmInvite && CGameWorld::GetMainPlayerInfo())
				{
					CPlayerInfo::State nPlayerState = CGameWorld::GetMainPlayerInfo()->GetPlayerState();
					switch (nPlayerState)
					{
						// states we like
					case CPlayerInfo::PLAYERSTATE_LEFTGAME:
					case CPlayerInfo::PLAYERSTATE_RESPAWN:
					case CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE:
					case CPlayerInfo::PLAYERSTATE_PLAYING:
						break;
						// and states... we DO NOT
					case CPlayerInfo::PLAYERSTATE_HASDIED:
					case CPlayerInfo::PLAYERSTATE_HASBEENARRESTED:
					case CPlayerInfo::PLAYERSTATE_FAILEDMISSION:
					default:
						bConfirmInvite = false;
						return;
					}
				}
			}

			// process without confirmation for invites via party
			bool bActionInvite = pInvite->HasInternalFlag(InternalFlags::LIF_AutoConfirm);

			// process the invite if we haven't already confirmed and there's nothing blocking
			if(!pInvite->HasInternalFlag(InternalFlags::LIF_Confirmed) && bConfirmInvite)
			{
				if(!bActionInvite)
				{
					bool bProcessInvite = false;
					if(ShowAlert(InviteAlertReason::IAR_ConfirmInvite, nullptr, &bProcessInvite))
					{
						if(!bProcessInvite)
						{
							gnetDebug1("UpdateAcceptedInvite :: Declined by user!");

							// reply to invite, reject by user
							ResolveInvite(
								pInvite,
								InviteAction::IA_Rejected,
								ResolveFlags::RF_SendReply,
								InviteResponseFlags::IRF_None);

							// clear out invite
							ClearInvite();

							// event for script
							GetEventScriptNetworkGroup()->Add(CEventNetworkInviteRejected());
						}
						else
						{
							gnetDebug1("UpdateAcceptedInvite :: Confirmed by user!");
							bActionInvite = true;
						}
					}
				}
			}

			// confirm invite if requested
			if((!pInvite->HasInternalFlag(InternalFlags::LIF_Confirmed) || pInvite->HasInternalFlag(InternalFlags::LIF_FromBoot)) && bActionInvite)
				ConfirmInvite();
		}

		// clear the invite if something went wrong
		if(m_PendingAlertReason != InviteAlertReason::IAR_None)
		{
			gnetDebug1("UpdateAcceptedInvite :: Clearing failed invite");
			ClearInvite();
		}
	}
}

void InviteMgr::ShowPendingAlert()
{
	// check we have an alert...
	if(m_PendingAlertReason == InviteAlertReason::IAR_None)
		return;

	// wait until we can show an alert
	const bool canShowAlert = (NetworkInterface::IsOnLandingPage() || CanGenerateScriptEvents()) && !IsNonInviteAlertActive();
	if(!canShowAlert)
		return;

	bool bAlertActioned = false;
	if(ShowAlert(m_PendingAlertReason, nullptr, &bAlertActioned))
	{
		gnetDebug1("ShowPendingAlert :: Actioned");
		m_PendingAlertReason = InviteAlertReason::IAR_None;
		m_szFailedScreenSubstring[0] = '\0';
	}
}

void InviteMgr::UpdateFlags()
{
	// track whether script have ever been ready (booting or not)
	if(CanGenerateScriptEvents() && !gs_HasBooted)
	{
		gnetDebug1("UpdateFlags :: Flagging as booted");
		gs_HasBooted = true;
	}

	// on boot UI, reset script states
	if(m_bSuppressProcess && NetworkInterface::IsOnBootUi())
	{
		gnetDebug1("UpdateFlags :: Resetting SuppressProcess on BootUi");
		m_bSuppressProcess = false;
	}

	// on boot UI, reset script states
	if(!m_CanReceiveRsInvites && NetworkInterface::IsOnBootUi())
	{
		gnetDebug1("UpdateFlags :: Resetting CanReceiveRsInvites on BootUi");
		m_CanReceiveRsInvites = true;
	}

	// on boot UI, reset script states
	if(m_bBlockedByScript && NetworkInterface::IsOnBootUi())
	{
		gnetDebug1("UpdateFlags :: Resetting BlockedByScript on BootUi");
		m_bBlockedByScript = false;
	}
}

void InviteMgr::Update(const int timeStep)
{	
	UpdateFlags();
	UpdateUnacceptedInvites(timeStep);
	UpdateRsInvites();
	UpdatePendingPlatformFlowState();

	// assume no alert will be shown but capture the last frame setting for some warning screen checks below
	m_LastShownAlertReason = m_ShownAlertReason;
	m_ShownAlertReason = InviteAlertReason::IAR_None;
	m_ShowingConfirmationExternally = false;

	// check for a pending unavailable invite notification and send to script when ready
	if(m_bInviteUnavailablePending && CanGenerateScriptEvents())
	{
		gnetDebug1("Update :: Clearing Invite Unavailable Flag");

		// this event is what script currently use to detect rejected invites, but we should create a new event for this specifically
		GetEventScriptNetworkGroup()->Add(CEventNetworkSystemServiceEvent(sysServiceEvent::INVITATION_RECEIVED, 0));
		m_bInviteUnavailablePending = false;
	}

	// check for a pending unavailable invite notification and send to script when ready
	if(m_PendingNetworkAccessAlert && (CanGenerateScriptEvents() || NetworkInterface::IsOnLandingPage()))
	{
		gnetDebug1("Update :: Clearing Access Flag");
		CNetwork::CheckNetworkAccessAndAlertIfFail(AccessArea_Landing);
		m_PendingNetworkAccessAlert = false;
	}

	// update the accepted invite
	UpdateAcceptedInvite(timeStep);

	// show alert if needed
	ShowPendingAlert();

#if !__NO_OUTPUT
	// track alerts being shown for the first time
	if(m_ShownAlertReason != InviteAlertReason::IAR_None && m_LastShownAlertReason == InviteAlertReason::IAR_None)
	{
		static const char* s_InviteAlertReason[] =
		{
			"IAR_None",							// IAR_None
			"IAR_ConfirmInvite",				// IAR_ConfirmInvite,
			"IAR_ConfirmParty",					// IAR_ConfirmParty
			"IAR_ConfirmNonParty",				// IAR_ConfirmNonParty
			"IAR_ConfirmViaParty",				// IAR_ConfirmViaParty
			"IAR_InvalidContent",				// IAR_InvalidContent
			"IAR_SessionError",					// IAR_SessionError
			"IAR_PartyError",					// IAR_PartyError
			"IAR_AlreadyInSession",				// IAR_AlreadyInSession
			"IAR_ActiveHost",					// IAR_ActiveHost
			"IAR_Underage",						// IAR_Underage
			"IAR_Incompatibility",				// IAR_Incompatibility
			"IAR_CannotAccess",					// IAR_CannotAccess
			"IAR_NoOnlinePrivileges",			// IAR_NoOnlinePrivileges
			"IAR_NotOnline",					// IAR_NotOnline
			"IAR_VideoEditor",					// IAR_VideoEditor
			"IAR_VideoDirector",				// IAR_VideoDirector
			"IAR_NoOnlinePermissions",			// IAR_NoOnlinePermissions
			"IAR_NoMultiplayerCharacter",		// IAR_NoMultiplayerCharacter
			"IAR_NotCompletedMultiplayerIntro",	// IAR_NotCompletedMultiplayerIntro
			"IAR_NotCompletedSpIntro",			// IAR_NotCompletedSpIntro
		};
		CompileTimeAssert(COUNTOF(s_InviteAlertReason) == InviteAlertReason::IAR_Max);

		gnetDebug1("ShowAlert :: Showing Alert: %s (%d)", s_InviteAlertReason[m_ShownAlertReason], m_ShownAlertReason);
	}
#endif
}

void InviteMgr::ConfirmInvite()
{  
    // check we have a valid invite to confirm
    sAcceptedInvite* pInvite = GetAcceptedInvite();
    if(!gnetVerify(pInvite->IsPending()))
    {
        gnetError("ConfirmInvite :: Invalid invite!");
        return;
    }

	// 
	gnetDebug1("ConfirmInvite :: Auto: %s", pInvite->HasInternalFlag(InternalFlags::LIF_AutoConfirm) ? "True" : "False");

    // reply to invite, confirmed
    if(!IsAcceptedInviteForDifferentUser() && pInvite->GetInviter().IsValid())
    {
		ResolveInvite(pInvite, InviteAction::IA_Accepted, ResolveFlags::RF_SendReply, InviteResponseFlags::IRF_None);
    }

    if(m_bSuppressProcess)
    {
        gnetDebug1("ConfirmInvite :: Unsuppressing after confirmation. Character has PSFP_FM_INTRO_CAN_ACCEPT_INVITES");
        m_bSuppressProcess = false;
    }
    
    // kill the pause menu if it's open - to allow the invite to process from script
	NetworkInterface::CloseAllUiForInvites();

    // we confirm the invite for a different user later
    if(!IsAcceptedInviteForDifferentUser())
    {
		// confirmed
		pInvite->AddInternalFlag(InternalFlags::LIF_Confirmed);
		pInvite->AddInternalFlag(InternalFlags::LIF_KnownToScript);

        // fire a script event
        CEventNetworkInviteConfirmed inviteEvent(pInvite->GetInviter(),
                                                 pInvite->m_nGameMode, 
                                                 pInvite->m_SessionPurpose,
                                                 pInvite->IsViaParty(), 
                                                 pInvite->m_nAimType, 
                                                 pInvite->m_nActivityType, 
                                                 pInvite->m_nActivityID,
                                                 pInvite->IsSCTV(),
												 pInvite->GetFlags(),
												 pInvite->m_nNumBosses);

        GetEventScriptNetworkGroup()->Add(inviteEvent);
    }
    else if(!m_bConfirmedDifferentUser)
    {
        gnetDebug1("ConfirmInvite :: Flagging restart! Invite confirmed for different user!");

        // flag this and we'll pick it up later
        m_bConfirmedDifferentUser = true; 
        m_bWaitingForScriptDeath = true; 

        // reset pending wait flag
        CNetwork::GetNetworkSession().ClearWaitFlag();

        // restart
        CLiveManager::HandleInviteToDifferentUser(pInvite->GetInvitee().GetLocalIndex());
    }
}

void InviteMgr::CheckInviteIsValid()
{
	// check we have an invite
	if(!HasPendingAcceptedInvite())
		return;

	// check if we're now in this session
	if(CNetwork::GetNetworkSession().IsInSession(m_AcceptedInvite.m_SessionInfo) || 
	   CNetwork::GetNetworkSession().IsJoiningSession(m_AcceptedInvite.m_SessionInfo))
	{
		gnetDebug1("CheckInviteIsValid :: We've joined the target session");

		// stash invite details
		rlGamerInfo hInvitee = m_AcceptedInvite.GetInvitee();
		rlGamerHandle hInviter = m_AcceptedInvite.GetInviter();
		unsigned nInviteId = m_AcceptedInvite.m_InviteId;
		unsigned nInviteFlags = m_AcceptedInvite.m_InviteFlags;
		rlSessionInfo hInfo = m_AcceptedInvite.GetSessionInfo();
		const int nGameMode = m_AcceptedInvite.m_nGameMode;
		char uniqueMatchId[MAX_UNIQUE_MATCH_ID_CHARS];
		safecpy(uniqueMatchId, m_AcceptedInvite.m_UniqueMatchId);
		const int nInviteFrom = m_AcceptedInvite.m_InviteFrom;
		const int nInviteType = m_AcceptedInvite.m_InviteType;
		m_bConfirmedDifferentUser = false;

		// cancel this invite
		CancelInvite();

		// if this is a freeroam invite, check if we can reroute to the activity session
		const CNetGamePlayer* pPlayer = NetworkInterface::IsNetworkOpen() ? NetworkInterface::GetPlayerFromGamerHandle(hInviter) : NULL;
		if(pPlayer && pPlayer->HasStartedTransition() && pPlayer->GetTransitionSessionInfo().IsValid())
		{
			if(hInfo != pPlayer->GetTransitionSessionInfo())
			{
				// re-initialise using the transition session information
				gnetDebug1("CheckInviteIsValid :: Retargeting to inviter transition session");
				m_AcceptedInvite.Init(pPlayer->GetTransitionSessionInfo(), hInvitee, hInviter, nInviteId, nInviteFlags, InternalFlags::LIF_None, nGameMode, uniqueMatchId, nInviteFrom, nInviteType);
			}
		}
	}
}

void InviteMgr::ActionInvite()
{
	gnetDebug1("ActionInvite");

	// flag that the invite was actioned 
	m_AcceptedInvite.AddInternalFlag(InternalFlags::LIF_Actioned);

#if RSG_XDK
	// we need to inform the Xbox One party system so that we can perform any cleanup
	rlXbl::FlagInviteActioned();
#endif

	// force clear the invite
	ClearInvite(ClearFlags::CF_Force);
}

bool InviteMgr::CanClearInvite(const ClearFlags clearFlags)
{
	// if we have forced, allow
	if((clearFlags & ClearFlags::CF_Force) != 0)
		return true;
	// if this is from script, only when they have been informed
	else if((clearFlags & ClearFlags::CF_FromScript) != 0)
		return m_AcceptedInvite.HasInternalFlag(InternalFlags::LIF_KnownToScript);
	else
	{
		// otherwise, when processing has completed or we have an error
		return m_AcceptedInvite.HasInternalFlag(InternalFlags::LIF_ProcessedConfig) ||
			m_AcceptedInvite.HasInternalFlag(InternalFlags::LIF_Incompatible);
	}
}

void InviteMgr::ClearAll()
{
	OUTPUT_ONLY(static bool s_bFailed = false;)

    if(CanClearInvite(ClearFlags::CF_Default))
	{
		OUTPUT_ONLY(s_bFailed = false;)
		m_PendingAlertReason = InviteAlertReason::IAR_None;
		m_szFailedScreenSubstring[0] = '\0';
		ClearInvite();
		ClearUnaccepted();
	}
#if !__NO_OUTPUT
	else
	{
		// we don't want to spam this
		if(!s_bFailed)
		{
			gnetDebug1("ClearAll :: Failed. Processing!");
			s_bFailed = true;
		}
	}
#endif
}

void InviteMgr::ClearInvite(const ClearFlags clearFlags)
{
	if(CanClearInvite(clearFlags))
	{
		gnetDebug1("ClearInvite :: Pending: %s, Actioned: %s", 
			m_AcceptedInvite.IsPending() ? "True" : "False", 
			m_AcceptedInvite.HasInternalFlag(InternalFlags::LIF_Actioned) ? "True" : "False");

		// always do this as a failsafe
		if(m_bBusySpinnerActivated)
		{
			CBusySpinner::Off(SPINNER_SOURCE_INVITE_DETAILS);
			m_bBusySpinnerActivated = false;
		}

		// need to keep the invite around here
		if(!(m_bConfirmedDifferentUser || m_bStoreInviteThroughRestart))
		{
#if RSG_XDK
			// remove player from platform party
			if(m_AcceptedInvite.IsViaPlatformPartyAny() && !m_AcceptedInvite.HasInternalFlag(InternalFlags::LIF_Actioned))
				CNetworkSession::RemoveFromParty(false);

			// we need to inform the Xbox One party system so that we can perform any cleanup
			rlXbl::FlagInviteActioned();
#endif

            m_bAllowProcessInPlayerSwitch = false;
			m_AcceptedInvite.Clear();
		}
		else
		{
			gnetDebug1("ClearInvite :: Not clearing. Different user pending.");
		}
	}
	else
	{
		gnetDebug1("ClearInvite :: Cannot clear invite - Flags: 0x%x", clearFlags);
	}
}

void InviteMgr::CancelInvite()
{
	if(m_bConfirmedDifferentUser)
	{
		gnetDebug1("CancelInvite skipped. Invite for a different user can't be canceled directly.");
		return;
	}

    gnetDebug1("CancelInvite");

	if(m_AcceptedInvite.IsPending() && !m_AcceptedInvite.HasInternalFlag(InternalFlags::LIF_Actioned))
	{
#if RSG_XDK
		// remove player from platform party
		if(m_AcceptedInvite.IsViaPlatformPartyAny())
			CNetworkSession::RemoveFromParty(false);
#endif
	}

    // always do this as a failsafe
    if(m_bBusySpinnerActivated)
    {
        CBusySpinner::Off(SPINNER_SOURCE_INVITE_DETAILS);
        m_bBusySpinnerActivated = false;
    }

#if RSG_XDK
	// we need to inform the Xbox One party system so that we can perform any cleanup
	rlXbl::FlagInviteActioned();
#endif

    m_AcceptedInvite.Cancel();

	// wipe parameters when canceling
	m_bAllowProcessInPlayerSwitch = false;
}

void InviteMgr::ClearUnaccepted()
{
	m_UnacceptedInvites.Reset();
}

bool InviteMgr::IsAcceptedInviteForDifferentUser() const
{
	if(!HasPendingAcceptedInvite())
		return false;

	// Checking the gamer handle here due to how the inactive user is handled on Xbox
	const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();
	if(pInfo && pInfo->GetGamerHandle() != m_AcceptedInvite.m_Invitee.GetGamerHandle())
		return true;

	return false;
}

const char* InviteMgr::GetConfirmText()
{
	if(IsAcceptedInviteForDifferentUser())
		return m_AcceptedInvite.IsRockstarToInactive() ? "NT_INV_DIFFERENT_PROFILE_PRESENCE" : "NT_INV_DIFFERENT_PROFILE";
	else if(m_AcceptedInvite.IsViaPlatformPartyAny())
	{
		bool bNoSave = CScriptHud::bUsingMissionCreator || !NetworkInterface::IsAnySessionActive() || StatsInterface::GetBlockSaveRequests();
		if(m_AcceptedInvite.IsViaPlatformParty())			
		{
			if(NetworkInterface::IsGameInProgress())
				return bNoSave ? "NT_INV_PARTY_INVITE_MP" : "NT_INV_PARTY_INVITE_MP_SAVE";
			else
				return bNoSave ? "NT_INV_PARTY_INVITE" : "NT_INV_PARTY_INVITE_SAVE";
		}
		else if(m_AcceptedInvite.IsViaPlatformPartyJVP())	return bNoSave ? "NT_INV_PARTY_JVP" : "NT_INV_PARTY_JVP_SAVE";
		else if(m_AcceptedInvite.IsViaPlatformPartyBoot())	return bNoSave ? "NT_INV_PARTY_BOOT" : "NT_INV_PARTY_BOOT_SAVE";
		else if(m_AcceptedInvite.IsViaPlatformPartyJoin())	
		{
			if(m_AcceptedInvite.GetSessionDetail().m_SessionConfig.m_Attrs.GetSessionPurpose() == SessionPurpose::SP_Activity)
				return bNoSave ? "NT_INV_PARTY_JOIN_JOB" : "NT_INV_PARTY_JOIN_JOB_SAVE";
			else 
				return bNoSave ? "NT_INV_PARTY_JOIN" : "NT_INV_PARTY_JOIN_SAVE";
		}
	}
	else if(m_AcceptedInvite.IsFromJoinQueue())
	{
		bool bNoSave = CScriptHud::bUsingMissionCreator || !NetworkInterface::IsAnySessionActive() || StatsInterface::GetBlockSaveRequests();
		return bNoSave ? "NT_INV_JOIN_QUEUE" : "NT_INV_JOIN_QUEUE_SAVE";
	}
	else
	{
		if(CSavegameAutoload::IsPerformingAutoLoadAtStartOfGame())
			return "NT_INV_BOOT";
		else if(CScriptHud::bUsingMissionCreator)
			return "NT_INV_CREATOR";
		else if(NetworkInterface::IsGameInProgress())
		{
			if(StatsInterface::GetBlockSaveRequests())
				return "NT_INV_FREE";
			else
				return "NT_INV_MP_SAVE";
		}
		// leaving for reference, but we don't support single player auto-save via invites
		//else if(CPauseMenu::GetMenuPreference(PREF_AUTOSAVE))
		//	return "NT_INV_SP_SAVE");
		else
			return "NT_INV";
	}

	return "NT_INV";
}

const char* InviteMgr::GetConfirmSubstring()
{
	if(IsAcceptedInviteForDifferentUser())
		return m_AcceptedInvite.GetInvitee().GetName();

	return NULL;
}

void InviteMgr::AddAdminInviteNotification(const rlGamerHandle& invitedGamer)
{
	gnetDebug1("AddAdminInviteNotification :: Adding %s", NetworkUtils::LogGamerHandle(invitedGamer));

	// just delete the oldest index
	if(m_AdminInvitedGamers.IsFull())
		m_AdminInvitedGamers.DeleteFast(0);

	m_AdminInvitedGamers.Push(invitedGamer);
}

bool InviteMgr::HasAdminInviteNotification(const rlGamerHandle& invitedGamer)
{
	return m_AdminInvitedGamers.Find(invitedGamer) >= 0;
}

void InviteMgr::OnTunablesRead()
{
	m_RsInviteTimeout = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("RS_INVITE_TIMEOUT", 0x12582fda), sRsInvite::DEFAULT_RS_INVITE_TIMEOUT);
	m_RsInviteReplyTTL = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("RS_INVITE_TTL", 0x24a1dbdc), sRsInvite::DEFAULT_RS_REPLY_TTL);

	m_bSendRsInviteAcks = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SEND_RS_INVITE_ACKS", 0x1514e455), true);
	m_bSendRsInviteBusyForActivities = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SEND_RS_INVITE_BUSY_FOR_ACTIVITIES", 0x2c0c621e), true);
	m_bAllowAdminInviteProcessing = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_ALLOW_ADMIN_INVITE_PROCESSING", 0xd37f3392), true);

	m_RsInviteAllowedGroups = CLiveManager::GetValidSentFromGroupsFromTunables(DEFAULT_INVITE_VALID_SEND_GROUPS, "INVITE");
}


InviteId InviteMgr::GenerateInviteId()
{
	return static_cast<InviteId>(sRng.GetInt());
}

bool InviteMgr::ShowAlert(const InviteAlertReason alertReason, int* number, bool* pReturnSelection)
{
	// skip if this in an invalid reason
	if(alertReason == InviteAlertReason::IAR_None)
		return false;

	int numButtons = 1;
	eWarningButtonFlags buttonFlags = FE_WARNING_OK;
	char szBodyTextLabel[MAX_LOCALISATION_STRING];
	const char* szSubString1 = nullptr;
    const char* szSubString2 = nullptr;

	szBodyTextLabel[0] = '\0';

	bool insertNumber = number != nullptr;
	int numberToInsert = insertNumber ? *number : 0;

	// capture the alert shown reason
	m_ShownAlertReason = alertReason;

	// setup alert parameters
	switch(m_ShownAlertReason)
	{
	case InviteAlertReason::IAR_None:
	case InviteAlertReason::IAR_Max:
		gnetAssertf(0, "ShowAlert :: Invalid Reason");
		break;

	case InviteAlertReason::IAR_ConfirmInvite:
		numButtons = 2;
		buttonFlags = FE_WARNING_YES_NO;
		safecpy(szBodyTextLabel, CLiveManager::GetInviteMgr().GetConfirmText());
		szSubString1 = CLiveManager::GetInviteMgr().GetConfirmSubstring();
		break;

	case InviteAlertReason::IAR_ConfirmParty:
		numButtons = 2;
		buttonFlags = FE_WARNING_YES_NO;
		safecpy(szBodyTextLabel, "NT_INV_PARTY");
		break;

	case InviteAlertReason::IAR_ConfirmNonParty:
		numButtons = 2;
		buttonFlags = FE_WARNING_YES_NO;
		safecpy(szBodyTextLabel, "NT_INV_IN_PARTY");
		break;

	case InviteAlertReason::IAR_ConfirmViaParty:
		numButtons = 2;
		buttonFlags = FE_WARNING_YES_NO;
		safecpy(szBodyTextLabel, "NT_INV_VIA_PARTY");
		break;

	case InviteAlertReason::IAR_InvalidContent:
		safecpy(szBodyTextLabel, "NT_INV_CONT");
		break;

	case InviteAlertReason::IAR_Underage:
		safecpy(szBodyTextLabel, "HUD_CONNT");
		break;

	case InviteAlertReason::IAR_SessionError:
		safecpy(szBodyTextLabel, "NT_INV_CONERR");
		break;

	case InviteAlertReason::IAR_PartyError:
		safecpy(szBodyTextLabel, "NT_INV_CONERR_PARTY");
		break;

	case InviteAlertReason::IAR_NotOnline:
		safecpy(szBodyTextLabel, "NT_INV_SIGNED_OFFLINE");
		break;

	case InviteAlertReason::IAR_AlreadyInSession:
		safecpy(szBodyTextLabel, "NT_INV_IN_SESSION");
		break;

    case InviteAlertReason::IAR_ActiveHost:
        safecpy(szBodyTextLabel, "NT_INV_ACTIVE_HOST");
        szSubString1 = m_szFailedScreenSubstring;
        szSubString2 = NetworkInterface::GetLocalGamerName();
        break;

	case InviteAlertReason::IAR_VideoEditor:
		safecpy(szBodyTextLabel, "HUD_REPLYINVITE");
		break;

	case InviteAlertReason::IAR_VideoDirector:
		safecpy(szBodyTextLabel, "HUD_REPLYINVITE_DIRECTOR");
		break;

    case InviteAlertReason::IAR_Incompatibility:
        safecpy(szBodyTextLabel, m_szFailedScreenSubstring);
        break;

    case InviteAlertReason::IAR_CannotAccess:
        safecpy(szBodyTextLabel, m_szFailedScreenSubstring);
        if(m_AlertScreenNumber >= 1)
        {
            insertNumber = true;
			numberToInsert = m_AlertScreenNumber;
        }
        break;

    case InviteAlertReason::IAR_NoOnlinePermissions:
        safecpy(szBodyTextLabel, "HUD_PERM");
        break;

    case InviteAlertReason::IAR_NoOnlinePrivileges:
        safecpy(szBodyTextLabel, "HUD_PSPLUS2");
        break;

	case InviteAlertReason::IAR_NotCompletedSpIntro:
		safecpy(szBodyTextLabel, "NT_INV_NOT_COMPLETED_PROLOGUE");
		break;

	case InviteAlertReason::IAR_NotCompletedMultiplayerIntro:
		safecpy(szBodyTextLabel, "NT_INV_NO_MULTIPLAYER_CHARACTER");
		break;

	case InviteAlertReason::IAR_NoMultiplayerCharacter:
		safecpy(szBodyTextLabel, "NT_INV_NOT_COMPLETED_MULTIPLAYER_INTRO");
		break;
    }

	CWarningScreen::SetMessage(WARNING_MESSAGE_STANDARD, szBodyTextLabel, buttonFlags, insertNumber, numberToInsert, szSubString1, szSubString2);

	// work out if the alert screen has been actioned by the player
	bool alertActioned = false;
	if(numButtons > 0)
	{
		eWarningButtonFlags result = CWarningScreen::CheckAllInput();
		if(result == FE_WARNING_OK || result == FE_WARNING_YES)
		{
			gnetDebug1("ShowAlert :: Confirmed");
			*pReturnSelection = true;
			alertActioned = true;
			CWarningScreen::Remove();
		}
		else 
		{
			// also check back button when we have two possible responses
			if(numButtons == 2)
			{
				if(result == FE_WARNING_NO)
				{
					gnetDebug1("ShowAlert :: Declined");
					*pReturnSelection = false;
					alertActioned = true;
					CWarningScreen::Remove();
				}
			}	
		}
	}

	if(alertActioned)
	{
		m_ShownAlertReason = InviteAlertReason::IAR_None;
		m_szFailedScreenSubstring[0] = '\0';
	}

	return alertActioned;
}


void InviteMgr::HandleEventPlatformInviteAccepted(const rlPresenceEventInviteAccepted* pEvent)
{
	// we need to close all UI to let script process invites
	gnetDebug1("HandleEventPlatformInviteAccepted");

	// check if this should run a spectator join
	unsigned inviteFlags = InviteFlags::IF_IsPlatform | InviteFlags::IF_AllowPrivate;
	if((pEvent->m_InviteFlags & rlPresenceInviteFlags::Flag_IsSpectator) != 0)
		inviteFlags |= InviteFlags::IF_Spectator;

	AddPlatformInvite(pEvent->m_GamerInfo, pEvent->m_Inviter, pEvent->m_SessionInfo, inviteFlags);
}

void InviteMgr::HandleEventInviteUnavailable(const rlPresenceEventInviteUnavailable* pEvent)
{
	gnetDebug1("HandleEventInviteUnavailable");
	
#if RSG_SCE
	(void)pEvent;
	m_bInviteUnavailablePending = true;
#elif RSG_XBOX
	switch (pEvent->m_Reason)
	{
	case rlPresenceInviteUnavailableReason::Reason_NoOnlinePrivilege:
		// use resolution platform UI
		CLiveManager::ResolvePlatformPrivilege(NetworkInterface::GetLocalGamerIndex(), rlPrivileges::PrivilegeTypes::PRIVILEGE_MULTIPLAYER_SESSIONS, true);
		break;
	default:
		m_bInviteUnavailablePending = true;
		break;
	}
#else
	(void)pEvent;
#endif
}

void InviteMgr::HandleEventPlatformJoinViaPresence(const rlPresenceEventJoinedViaPresence* pEvent)
{
    gnetDebug1("HandleEventPlatformJoinViaPresence");

#if RSG_NP
	if(pEvent->m_TargetHandle == NetworkInterface::GetLocalGamerHandle() && pEvent->m_SessionInfo.GetHostPeerAddress().IsLocal())
	{
		gnetDebug1("HandleEventPlatformJoinViaPresence :: JvP to Session Hosted by Local, Skipping");
		return;
	}
#endif

	// target player
	rlGamerHandle targetPlayer = pEvent->m_TargetHandle;

	// if we have an unaccepted invite from this same target player, consider this to 
	unsigned inviteFlags = InviteFlags::IF_IsPlatform;
	if(GetUnacceptedInviteIndex(targetPlayer) >= 0)
	{
		gnetDebug1("HandleEventPlatformJoinViaPresence :: Found unaccepted invite with target handle, treating as invite");
		inviteFlags |= InviteFlags::IF_AllowPrivate;
	}
	else
	{
		const int unacceptedInviteIdx = GetMostRecentUnacceptedInviteIndex(pEvent->m_SessionInfo);
		if(unacceptedInviteIdx >= 0)
		{
			gnetDebug1("HandleEventPlatformJoinViaPresence :: Found unaccepted invite with target session, treating as invite");

			// assume we have private access 
			inviteFlags |= InviteFlags::IF_AllowPrivate;

			// if we don't have an invite target or the invite target is the local player (this is possible on PlayStation platforms)
			// use the one from the invite
			if(!targetPlayer.IsValid() || targetPlayer.IsLocal())
			{
				const UnacceptedInvite* unacceptedInvite = GetUnacceptedInvite(unacceptedInviteIdx);
				
				gnetDebug1("HandleEventPlatformJoinViaPresence :: Event target invalid, switching to %s [%s]", unacceptedInvite->m_InviterName, NetworkUtils::LogGamerHandle(unacceptedInvite->m_Inviter));
				targetPlayer = unacceptedInvite->m_Inviter;
			}
		}
		else
		{
			// just treat it as a join
			inviteFlags |= InviteFlags::IF_IsJoin;
		}
	}

	// if we added the invite flag, remove any matching unaccepted invites
	if((inviteFlags & InviteFlags::IF_AllowPrivate) != 0)
	{
		// remove matching invites
		RemoveMatchingUnacceptedInvites(targetPlayer);
		RemoveMatchingUnacceptedInvites(pEvent->m_SessionInfo);
	}

	// check if this should run a spectator join
	if((pEvent->m_InviteFlags & rlPresenceInviteFlags::Flag_IsSpectator) != 0)
		inviteFlags |= InviteFlags::IF_Spectator;

	AddPlatformInvite(pEvent->m_GamerInfo, targetPlayer, pEvent->m_SessionInfo, inviteFlags);
}

void InviteMgr::HandleJoinRequest(const rlSessionInfo& hInfo, const rlGamerInfo& hInvitee, const rlGamerHandle& hInviter, unsigned inviteFlags)
{
	// at this stage, we just assume that this has gone through any required confirmation flow
	AcceptInvite(hInfo, hInvitee, hInviter, InviteMgr::GenerateInviteId(), inviteFlags, -1, "", -1, -1);
}

void InviteMgr::HandleInviteFromJoinQueue(const rlSessionInfo& hInfo, const rlGamerInfo& hInvitee, const rlGamerHandle& hInviter, const unsigned nInviteId, const unsigned nJoinQueueToken, unsigned nInviteFlags)
{
	// reject if we are no longer queuing for this session
	if(CNetwork::GetNetworkSession().GetJoinQueueToken() != nJoinQueueToken)
	{
		gnetDebug1("HandleInviteFromJoinQueue :: Not queuing for this token. MyToken: 0x%08x, Token: 0x%08x", CNetwork::GetNetworkSession().GetJoinQueueToken(), nJoinQueueToken);
		ResolveInvite(hInvitee, hInviter, hInfo, InviteAction::IA_NotRelevant, ResolveFlags::RF_SendReply, InviteResponseFlags::IRF_None, nInviteFlags, nInviteId, 0, "", 0, 0);
	}
	// if we're already in this session (this happens if the player we're joining to joins our session)
	else if(CNetwork::GetNetworkSession().IsInSession(hInfo))
	{
		gnetDebug1("HandleInviteFromJoinQueue :: Already in this session: 0x%" I64FMT "x", hInfo.GetToken().m_Value);
		ResolveInvite(hInvitee, hInviter, hInfo, InviteAction::IA_NotRelevant, ResolveFlags::RF_SendReply, InviteResponseFlags::IRF_StatusFromJoinQueue | InviteResponseFlags::IRF_StatusInSession, nInviteFlags, nInviteId, 0, "", 0, 0);
	}
	// do not show when we have blocked invites
	else if(m_bBlockJoinQueueInvite)
	{
		gnetDebug1("HandleInviteFromJoinQueue :: Blocked - Rejecting. MyToken: 0x%08x, Token: 0x%08x", CNetwork::GetNetworkSession().GetJoinQueueToken(), nJoinQueueToken);
		ResolveInvite(hInvitee, hInviter, hInfo, InviteAction::IA_Blocked, ResolveFlags::RF_SendReply, InviteResponseFlags::IRF_StatusFromJoinQueue | InviteResponseFlags::IRF_StatusInActivity, nInviteFlags, nInviteId, 0, "", 0, 0);
	}
	else
	{
		// handle invite - use the private flag, we were originally invited
		gnetDebug1("HandleInviteFromJoinQueue");
		CNetwork::GetNetworkSession().ClearQueuedJoinRequest();
		AcceptInvite(hInfo, hInvitee, hInviter, nInviteId, nInviteFlags | InviteFlags::IF_AllowPrivate, -1, "", -1, -1);

		// resolve invite
		ResolveInvite(hInvitee, hInviter, hInfo, InviteAction::IA_None, ResolveFlags::RF_SendReply, InviteResponseFlags::IRF_StatusFromJoinQueue, nInviteFlags, nInviteId, -1, "", -1, -1);
	}
}

#if RSG_XDK
void InviteMgr::HandleEventGameSessionReady(const rlPresenceEventGameSessionReady* pEvent, const unsigned nInviteFlags)
{
	gnetDebug1("HandleEventGameSessionReady :: Session: 0x%016" I64FMT "x, Invitee: %s, Flags: 0x%x, Party Members: %d", pEvent->m_SessionInfo.GetToken().m_Value, pEvent->m_GamerInfo.GetName(), nInviteFlags, pEvent->m_NumPartyPlayers, nInviteFlags);

	bool bProcess = true;

	// invite to non-active player
	const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();
	if(pInfo && pEvent->m_GamerInfo != *pInfo)
	{
		gnetDebug1("HandleEventGameSessionReady :: Invited non-active user %s", pEvent->m_GamerInfo.GetName());
	}
	else
	{
		bool bDisassociate = false;
		bool bLeaveParty = false;

		if(g_rlXbl.GetPartyManager()->IsInSoloParty())
		{
			// already in the session, safely ignore
			gnetDebug1("HandleEventGameSessionReady :: In solo party");
			bProcess = false;
		}
		else if(CNetwork::GetNetworkSession().IsInSession(pEvent->m_SessionInfo))
		{
			// already in the session, safely ignore
			gnetDebug1("HandleEventGameSessionReady :: Already in session!");
			bProcess = false;
		}
		else if(CNetwork::GetNetworkSession().IsJoiningSession(pEvent->m_SessionInfo))
		{
			// already joining this session, safely ignore
			gnetDebug1("HandleEventGameSessionReady :: Currently joining session!");
			bProcess = false;
		}
		else if(m_AcceptedInvite.IsPending() && m_AcceptedInvite.GetSessionInfo() == pEvent->m_SessionInfo)
		{
			// already processing an invite to this session
			gnetDebug1("HandleEventGameSessionReady :: Already processing invite to session!");
			bProcess = false;
		}
		else if(CNetwork::GetNetworkSession().IsInPlatformPartyGameSession())
		{
			// not in the session but in the MPSD session, eject to resolve situation
			gnetDebug1("HandleEventGameSessionReady :: Already in MPSD session!");
			bProcess = false;
			bLeaveParty = true; 
		}
		else if(pEvent->m_SessionInfo.GetHostPeerAddress().IsLocal())
		{
			// not in the session but session info is local to player, eject to resolve situation
			gnetDebug1("HandleEventGameSessionReady :: Local session. MPSD still holding an old session.");
			bProcess = false;
			bLeaveParty = true;
		}
		else if(pEvent->m_NumPartyPlayers == 0)
		{
			// no party members in the session, eject to resolve situation
			gnetDebug1("HandleEventGameSessionReady :: No party members in MPSD session!");
			bProcess = false;
			bLeaveParty = true;
		}
		else if(pEvent->m_NumPartyPlayers == 1 && pEvent->m_PartyPlayers[0] == NetworkInterface::GetLocalGamerHandle())
		{
			// only local player in MPSD session but not actually in it, eject to resolve situation
			gnetDebug1("HandleEventGameSessionReady :: Only local player in MPSD session!");
			bProcess = false;
			bLeaveParty = true;
		}
		else if(CNetwork::GetNetworkSession().IsNextJobTransition(pEvent->m_PartyPlayers[0], pEvent->m_SessionInfo))
		{
			gnetDebug1("HandleEventGameSessionReady :: Session is next job transition - ignoring invite");
			bProcess = false;
		}
#if !__NO_OUTPUT
		else 
		{
			// check if local player is already in the session - this isn't great, but maybe we weren't cleaned up 
			// from being in this same session previously
			for(int i = 0; i < pEvent->m_NumPartyPlayers; i++) if(pEvent->m_PartyPlayers[i] == NetworkInterface::GetLocalGamerHandle())
				gnetWarning("HandleEventGameSessionReady :: Already a member of MPSD session!");
		}
#endif

		// remove game session if tagged
		if(bDisassociate)
		{
			gnetDebug1("HandleEventGameSessionReady :: Disassociating");
			CNetwork::GetNetworkSession().DisassociatePlatformPartySession(NULL);
		}
		else if(bLeaveParty)
		{
			// if we're in multiplayer, a party changed handler in session code will fix this up
			gnetDebug1("HandleEventGameSessionReady :: Leaving Party");
			CNetwork::GetNetworkSession().RemoveFromParty(false);
		}
	}

	if(bProcess)
	{
		// pick a party member to be the inviter
		int nIndex = 0;
		for(int i = 0; i < pEvent->m_NumPartyPlayers; i++)
		{
			if(pEvent->m_PartyPlayers[i] != NetworkInterface::GetLocalGamerHandle())
				break;
			nIndex++;
		}

		// cap to the last index
		nIndex = Min(nIndex, pEvent->m_NumPartyPlayers - 1);
		gnetDebug1("HandleEventGameSessionReady :: Using Index %d, %s", nIndex, NetworkUtils::LogGamerHandle(pEvent->m_PartyPlayers[nIndex]));

		// use the first party member as inviter
		AddPlatformInvite(pEvent->m_GamerInfo, pEvent->m_PartyPlayers[nIndex], pEvent->m_SessionInfo, nInviteFlags);
	}
	else
	{
		rlXbl::FlagInviteActioned();
	}
}
#endif

bool InviteMgr::DidInviteFlowShowAlert() const
{
	return m_LastShownAlertReason != InviteAlertReason::IAR_None;
}

bool InviteMgr::IsAlertActive() const
{
	return CWarningScreen::IsActive();
}

bool InviteMgr::DoesUiRequirePlatformInvite()
{
	// work out if we can process a Rockstar invite
	if(cStoreScreenMgr::IsStoreMenuOpen())
	{
		gnetDebug1("DoesUiRequirePlatformInvite :: In Store");
		return true;
	}

	if(CScriptHud::bUsingMissionCreator)
	{
		gnetDebug1("DoesUiRequirePlatformInvite :: In Mission Creator");
		return true;
	}

	if(NetworkInterface::IsOnLandingPage())
	{
		gnetDebug1("DoesUiRequirePlatformInvite :: On Landing Page");
		return true;
	}

#if RSG_GEN9
	if(CInitialInteractiveScreen::IsActive())
	{
		gnetDebug1("DoesUiRequirePlatformInvite :: On IIS");
		return true;
	}
#endif

	// block invites when we aren't in multiplayer and we haven't completed the single player prologue
	if(!NetworkInterface::IsInOrTranitioningToAnyMultiplayer() &&								// check we aren't multiplayer
		(FindPlayerPed() != nullptr || NetworkInterface::IsTransitioningToSinglePlayer()) &&	// check we had a ped or are switching to single player
		CNetwork::HasCompletedSpPrologue() == IntroSettingResult::Result_NotComplete)			// check we haven't completed the prologue
	{
		gnetDebug1("DoesUiRequirePlatformInvite :: In SP and Not Completed Prologue");
		return true;
	}

	return false;
}

void InviteMgr::AcceptInvite(
	const rlSessionInfo& hOrigInfo, 
	const rlGamerInfo& hInvitee, 
	const rlGamerHandle& hInviter, 
	const unsigned nInviteId, 
	const unsigned nInviteFlags,
	const int nGameMode, 
	const char* pUniqueMatchId,
	const int nInviteFrom, 
	const int nInviteType)
{
	// make a copy since we may need to change it to the transition session's info
	rlSessionInfo hInfo = hOrigInfo;

	// logging
    gnetDebug1("AcceptInvite :: Session: 0x%016" I64FMT "x, Invitee: %s, Inviter: %s, InviteId: 0x%08x, Flags: 0x%x", 
		hInfo.GetToken().m_Value, 
		hInvitee.GetName(), 
		NetworkUtils::LogGamerHandle(hInviter), 
		nInviteId,
		nInviteFlags);
	OUTPUT_ONLY(LogInviteFlags(nInviteFlags, "AcceptInvite"));

	// clear previous failure
	m_PendingAlertReason = InviteAlertReason::IAR_None;
	m_szFailedScreenSubstring[0] = '\0';

	// track this for XDK multiplayer
	XDK_ONLY(bool bEjectFromParty = false);

    // close the calibration screen if open (warning screen compatibility)
    CPauseMenu::CloseDisplayCalibrationScreen();

    // close the social club menu if open (warning screen compatibility)
    if(SocialClubMenu::IsActive())
    {
        gnetDebug1("AcceptInvite :: Closing social club menu");
        SocialClubMenu::FlagForDeletion();
    }

    // grab local player info
    const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();

	// check if we need to reject the invite
	unsigned inviteResponseFlags = InviteResponseFlags::IRF_None;

	// internal flag capture
	unsigned internalFlags = InternalFlags::LIF_None; 

#if RSG_NP || RSG_GDK
	// this block needs to be above any of the checks below as these rely on a fixed user index
	// we can assume that an accepted invite is equivalent to choosing a profile
	// for a follow up, we should move platform invites to a state machine where we can allow a waiting
	// period for the main user index to be set correctly
	if(!gs_HasBooted)
	{
		gnetDebug1("AcceptInvite :: Accepted before or during boot. Auto confirming user at index %d", hInvitee.GetLocalIndex());

		// add internal flags to advance through flow
		internalFlags |= InternalFlags::LIF_FromBoot;
		internalFlags |= InternalFlags::LIF_Confirmed;
		internalFlags |= InternalFlags::LIF_AutoConfirm;

		// Setting the player index. If there's already one set we rely on the invite from different user flow
		const int localGamerIndex = hInvitee.GetLocalIndex();
		if(gnetVerify(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex)) && CControlMgr::GetMainPlayerIndex() == -1)
		{
			gnetDebug1("AcceptInvite :: Accepted before or during boot. Setting index to %d", localGamerIndex);
			CControlMgr::SetMainPlayerIndex(localGamerIndex);
		}
	}
	else if(NetworkInterface::IsOnBootUi() && RL_IS_VALID_LOCAL_GAMER_INDEX(hInvitee.GetLocalIndex()))
	{
		// TODO_ANDI: gs_HasBooted stays while on the landing page till we leave it. We could be in a store screen or something
		// there may be cases where we want a confirmation alert both in this case and the gs_HasBooted case
		gnetDebug1("AcceptInvite :: Accepted on landing page. Auto confirming user at index %d", hInvitee.GetLocalIndex());
		internalFlags |= InternalFlags::LIF_Confirmed;
		internalFlags |= InternalFlags::LIF_AutoConfirm;
	}
	else if(NetworkInterface::IsOnBootUi() && (!pInfo || hInvitee != *pInfo))
	{
		gnetDebug1("AcceptInvite :: Accepted on landing page from different user. Auto confirming user at index %d", hInvitee.GetLocalIndex());
		internalFlags |= InternalFlags::LIF_AutoConfirm;
	}
#endif

	// online privileges
	if(!CLiveManager::IsPlatformSubscriptionCheckPending() && !CLiveManager::HasPlatformSubscription())
	{
		gnetDebug1("AcceptInvite :: No platform subscription. Rejecting");

		// show alert
		m_PendingAlertReason = RSG_NP ? InviteAlertReason::IAR_NoOnlinePrivileges : InviteAlertReason::IAR_NoOnlinePermissions;

		// send response
		inviteResponseFlags = InviteResponseFlags::IRF_RejectBlocked | InviteResponseFlags::IRF_RejectNoPrivilege;

#if RSG_NP
		// show the account upgrade UI
		CLiveManager::ShowAccountUpgradeUI();
#elif RSG_XBOX
		// eject from session party on XDK and show resolution UI on all Xbox platforms
		XDK_ONLY(bEjectFromParty = true);
		CLiveManager::ResolvePlatformPrivilege(NetworkInterface::GetLocalGamerIndex(), rlPrivileges::PrivilegeTypes::PRIVILEGE_MULTIPLAYER_SESSIONS, true);
#endif
	}
#if GTA_REPLAY
    else if(CVideoEditorInterface::IsActive() || CTheScripts::GetIsInDirectorMode())
	{
		gnetDebug1("AcceptInvite :: In Video Editor! Invites not supported");
		
		// show alert
		m_PendingAlertReason = CVideoEditorInterface::IsActive() ? InviteAlertReason::IAR_VideoEditor : InviteAlertReason::IAR_VideoDirector;
		
		// send response
		inviteResponseFlags = InviteResponseFlags::IRF_RejectBlocked | InviteResponseFlags::IRF_StatusInEditor;

		// eject from session party on XDK
		XDK_ONLY(bEjectFromParty = true);
	}
#endif // GTA_REPLAY
	// invite to non-active player
	else if(pInfo && hInvitee != *pInfo)
    {
		// check if the active local player is host of the session we've been invited to
		if(NetworkInterface::IsHost(hInfo))
		{
			if(pInfo && hInviter == pInfo->GetGamerHandle())
			{
				// inviter is the active profile, we can't invite ourself so this must be an invite
				// to an inactive profile from the active profile
				// if the active profile is the session host, this will not work since the invite cannot
				// possibly succeed
				// in this case, show an error instead of even attempting to join
				m_PendingAlertReason = InviteAlertReason::IAR_ActiveHost;
				safecpy(m_szFailedScreenSubstring, hInvitee.GetName(), COUNTOF(m_szFailedScreenSubstring));

				// logging
				gnetDebug1("AcceptInvite :: Inactive profile invited to session hosted by active profile!");
			}
			// if this is the main session, check if we can route through the inviting players transition
			else if(CNetwork::GetNetworkSession().IsMySessionToken(hInfo.GetToken()))
			{
				const CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(hInviter);
				if(pPlayer && pPlayer->HasStartedTransition() && pPlayer->GetTransitionSessionInfo().IsValid())
				{
					gnetDebug1("AcceptInvite :: Invited to session hosted by active profile.");
					hInfo = pPlayer->GetTransitionSessionInfo();
				}
			}
		}
    }
    else
    {
        // if we're blocked by script - let script know we accepted an invite so that they can show the
        // correct alert for why we're blocked (this is used to display something more appropriate
        // when we receive an invite in a tutorial session from a same session player)
        if(!m_bBlockedByScript && CNetwork::GetNetworkSession().IsInSession(hInfo))
        {
			const CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(hInviter);
			if(pPlayer && pPlayer->HasStartedTransition() && pPlayer->GetTransitionSessionInfo().IsValid())
			{
				gnetDebug1("AcceptInvite :: Already in same game session as the inviter, switching to inviter's transition session");
				hInfo = pPlayer->GetTransitionSessionInfo();
			}
			else
			{
				m_PendingAlertReason = InviteAlertReason::IAR_AlreadyInSession;

				// eject from session party on XDK
				XDK_ONLY(bEjectFromParty = true);

				// logging
				gnetDebug1("AcceptInvite :: Already in session!");
			}
        }
	}

	// send a response if flagged
	if(inviteResponseFlags != InviteResponseFlags::IRF_None)
	{
		// need to send blocked reply
		ResolveInvite(
			hInvitee,
			hInviter,
			hOrigInfo,
			InviteAction::IA_Blocked,
			ResolveFlags::RF_SendReply,
			inviteResponseFlags,
			nInviteFlags,
			nInviteId,
			nGameMode,
			pUniqueMatchId,
			nInviteFrom,
			nInviteType);
	}
        
    // if we haven't failed the invite...
    if(m_PendingAlertReason == InviteAlertReason::IAR_None)
	{
		// this invite is now processing, hang on to this information and we'll process when we can
		if(HasConfirmedAcceptedInvite())
		{
			if(m_AcceptedInvite.m_SessionInfo == hInfo)
			{
				gnetDebug1("AcceptInvite :: Have existing confirmed invite to same session. Ignoring...");
			}
			else
			{
				gnetDebug1("AcceptInvite :: Have existing confirmed invite. Stashing new invite process later. Pending Invite: %s", m_PendingAcceptedInvite.IsPending() ? "Started" : "Clear");
				if(m_PendingAcceptedInvite.IsPending())
					m_PendingAcceptedInvite.Clear();
				m_PendingAcceptedInvite.Init(hInfo, hInvitee, hInviter, nInviteId, nInviteFlags, nGameMode, InternalFlags::LIF_None, pUniqueMatchId, nInviteFrom, nInviteType);
			}
			
			// bail out here
			return;
		}
		// if we've started, clear out the current invite
		else if(m_AcceptedInvite.IsPending())
		{
			gnetDebug1("AcceptInvite :: Existing invite - not confirmed. Overriding!");
			m_AcceptedInvite.Cancel();
			ClearInvite();
		}

		// close the pause menu on accepting if we are currently suppressed - we need script to be processing here to lift this
		if(m_bSuppressProcess)
			NetworkInterface::CloseAllUiForInvites();

		// reset this flag here (to make sure it's cleared from a restart), will confirm via the alert
		m_bConfirmedDifferentUser = false;

		// initialise the invite for processing
		m_AcceptedInvite.Init(hInfo, hInvitee, hInviter, nInviteId, nInviteFlags, internalFlags, nGameMode, pUniqueMatchId, nInviteFrom, nInviteType);

        // check if we already have the config...
        if(m_AcceptedInvite.HasConfig())
        {
			gnetDebug1("AcceptInvite :: Already have config");
			
			// we already have the config, just move forward
			m_AcceptedInvite.AddInternalFlag(InternalFlags::LIF_Accepted);
			m_AcceptedInvite.AddInternalFlag(InternalFlags::LIF_CompletedQuery);
			m_AcceptedInvite.AddInternalFlag(InternalFlags::LIF_KnownToScript);

            // fire a script event
			if(pInfo && hInvitee == *pInfo)
			{
				CEventNetworkInviteAccepted inviteEvent(m_AcceptedInvite.GetInviter(),
														m_AcceptedInvite.m_nGameMode, 
														m_AcceptedInvite.m_SessionPurpose,
														m_AcceptedInvite.IsViaParty(), 
														m_AcceptedInvite.m_nAimType, 
														m_AcceptedInvite.m_nActivityType, 
														m_AcceptedInvite.m_nActivityID, 
														m_AcceptedInvite.IsSCTV(),
														m_AcceptedInvite.GetFlags(),
														m_AcceptedInvite.m_nNumBosses);

				GetEventScriptNetworkGroup()->Add(inviteEvent);
				NotifyPlatformInviteAccepted(&m_AcceptedInvite);
			}
        }
	}
	else
	{
#if RSG_XDK
		// we need to eject from the party if on Xbox One if this is a party invite
		if(bEjectFromParty && sAcceptedInvite::AreFlagsViaPlatformPartyAny(nInviteFlags))
		{
			CNetwork::GetNetworkSession().RemoveFromParty(false);
			rlXbl::FlagInviteActioned();
		}
#endif
	}
}

bool InviteMgr::CheckAndMakeRoomForRsInvite()
{
	// work out if we have enough space
	if(!m_RsInvites.IsFull())
		return true; 

	// if not, eject the oldest invite
	unsigned nOldestTimeAdded = ~0u;
	int nOldestIndex = -1;

	// need to delete something, use the oldest for now
	int nInvites = m_RsInvites.GetCount();
	for(int i = 0; i < nInvites; i++)
	{
		if (m_RsInvites[i].m_TimeAdded < nOldestTimeAdded)
		{
			nOldestIndex = i;
			nOldestTimeAdded = m_RsInvites[i].m_TimeAdded;
		}
	}

	// punt the oldest invite
	if(nOldestIndex >= 0)
	{
		gnetDebug1("CheckAndMakeRoomForRsInvite :: Removing oldest Rockstar invite");
		return RemoveRsInvite(nOldestIndex, RemoveReason::RR_Oldest);
	}
	
	// shouldn't get here
	return false;
}

unsigned InviteMgr::ValidateRsInvite(const rlGamerHandle& hInviter, const rlClanId primaryCrewId, const unsigned inviteFlags)
{
	// check for invalid flags that cannot be applied for p2p invites
	if((inviteFlags & InviteFlags::IF_InvalidPeerFlags) != 0)
	{
		gnetDebug1("ValidateRsInvite :: Reject - Invalid Flags Included!");
		return InviteResponseFlags::IRF_RejectIncompatible | InviteResponseFlags::IRF_CannotProcessAdmin;
	}

	if(!CLiveManager::CanReceiveFrom(hInviter, primaryCrewId, m_RsInviteAllowedGroups OUTPUT_ONLY(, "Invite")))
	{
		gnetDebug1("ValidateRsInvite :: Reject - Not allowed to receive from %s", hInviter.ToString());
		return InviteResponseFlags::IRF_RejectIncompatible | InviteResponseFlags::IRF_RejectInvalidInviter;
	}

	// passed all checks
	return InviteResponseFlags::IRF_None;
}

bool InviteMgr::AddRsInvite(
	const rlGamerInfo& hInvitee,
	const rlSessionInfo& hSessionInfo,
	const rlGamerHandle& hFrom,
	const char* szGamerName,
	const unsigned nStaticDiscriminatorEarly,
	const unsigned nStaticDiscriminatorInGame,
	const unsigned nInviteId,
	const int nGameMode,
	unsigned nCrewId,
	const char* szContentId,
	const rlGamerHandle& hContentCreator,
	const char* pUniqueMatchId,
	const int nInviteFrom,
	const int nInviteType,
	const unsigned nPlaylistLength,
	const unsigned nPlaylistCurrent,
	const unsigned nInviteFlags)
{
	const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();
    if(pInfo == nullptr || *pInfo != hInvitee)
    {
        // reply to invite, inactive gamer profile
        gnetDebug1("AddRsInvite :: Invite to inactive profile. Cannot process Rockstar Invite.");
		ResolveInvite(hInvitee, hFrom, hSessionInfo, InviteAction::IA_None, ResolveFlags::RF_SendReply, InviteResponseFlags::IRF_RejectCannotProcessRockstar, nInviteFlags, nInviteId, nGameMode, pUniqueMatchId, nInviteFrom, nInviteType);
		return true;
    }

    // check if we're already in the session
    if(CNetwork::GetNetworkSession().IsInSession(hSessionInfo))
    {
        // reply to invite, in this session already
        gnetDebug1("AddRsInvite :: Invite to session we're already a member of");
		ResolveInvite(hInvitee, hFrom, hSessionInfo, InviteAction::IA_NotRelevant, ResolveFlags::RF_SendReply, InviteResponseFlags::IRF_StatusInFreeroam, nInviteFlags, nInviteId, nGameMode, pUniqueMatchId, nInviteFrom, nInviteType);
		return true;
    }

	// the static discriminator is built from values that are known prior to loading any level data
	// use this as a quick rejection method for any invites we receive that aren't compatible
	if(nStaticDiscriminatorEarly != NetworkGameConfig::GetStaticDiscriminator(StaticDiscriminatorType::Type_Early))
	{
		gnetDebug1("AddRsInvite :: Invalid Early Static Discriminator: 0x%08x, Local: 0x%08x", nStaticDiscriminatorEarly, NetworkGameConfig::GetStaticDiscriminator(StaticDiscriminatorType::Type_Early));
		ResolveInvite(hInvitee, hFrom, hSessionInfo, InviteAction::IA_Blocked, ResolveFlags::RF_SendReply, InviteResponseFlags::IRF_RejectIncompatible | InviteResponseFlags::IRF_RejectBlocked, nInviteFlags, nInviteId, nGameMode, pUniqueMatchId, nInviteFrom, nInviteType);
		return true;
	}

	// we can also check this when we are in a network game
	if(nStaticDiscriminatorInGame != NetworkGameConfig::GetStaticDiscriminator(StaticDiscriminatorType::Type_InGame))
	{
		bool isMismatched = false;

		// only check this when we are in an actual network game and we know that our local assets are fully calculated
		// we could also do this in a single player game but there's no equivalent call
		if(NetworkInterface::IsGameInProgress())
		{
			gnetDebug1("AddRsInvite :: Invalid In-Game Discriminator (Rejecting - IsGameInProgress): 0x%08x, Local: 0x%08x", nStaticDiscriminatorInGame, NetworkGameConfig::GetStaticDiscriminator(StaticDiscriminatorType::Type_InGame));
			isMismatched = true;
		}
		else
		{
			// otherwise, the static discriminator on it's own will fail to pick up any asset differences (these aren't know until a level load completes)
			// in the case of an indiscriminate invite (i.e. invite skill-matched, invite to all crew), we don't want these to trigger any notifications or
			// follow up platforms invites when out of level so check the full discriminator here (i.e. we don't want a skill matched invite to 
			// end up generating a platform invite to someone on the front-end)
			const bool isTargeted = ((nInviteFlags & IF_NotTargeted) == 0);
			if (!isTargeted)
			{
				gnetDebug1("AddRsInvite :: Invalid In-Game Discriminator (Rejected - !IsGameInProgress and NotTargeted): 0x%08x, Local: 0x%08x", nStaticDiscriminatorInGame, NetworkGameConfig::GetStaticDiscriminator(StaticDiscriminatorType::Type_InGame));
				isMismatched = true;
			}
			else
			{
				gnetDebug1("AddRsInvite :: Invalid In-Game Discriminator (Ignoring - !IsGameInProgress but Targeted): 0x%08x, Local: 0x%08x", nStaticDiscriminatorInGame, NetworkGameConfig::GetStaticDiscriminator(StaticDiscriminatorType::Type_InGame));
			}
		}

		if(isMismatched)
		{
			ResolveInvite(hInvitee, hFrom, hSessionInfo, InviteAction::IA_Blocked, ResolveFlags::RF_SendReply, InviteResponseFlags::IRF_RejectIncompatible | InviteResponseFlags::IRF_RejectBlocked, nInviteFlags, nInviteId, nGameMode, pUniqueMatchId, nInviteFrom, nInviteType);
			return true;
		}
	}

	// validate this invite
	const unsigned validateResponseFlags = ValidateRsInvite(hFrom, nCrewId, nInviteFlags);
	if(validateResponseFlags != InviteResponseFlags::IRF_None)
	{
		gnetDebug1("OnReceivedRsInvite :: ValidateRsInvite rejected!");
		ResolveInvite(hInvitee, hFrom, hSessionInfo, InviteAction::IA_Blocked, ResolveFlags::RF_None, validateResponseFlags, nInviteFlags, nInviteId, nGameMode, pUniqueMatchId, nInviteFrom, nInviteType);
		return true;
	}

#if GTA_REPLAY
	if(CVideoEditorInterface::IsActive() || CTheScripts::GetIsInDirectorMode())
	{
		gnetDebug1("AddRsInvite :: In Video Editor! Invites not supported");
		ResolveInvite(hInvitee, hFrom, hSessionInfo, InviteAction::IA_Blocked, ResolveFlags::RF_SendReply, InviteResponseFlags::IRF_StatusInEditor | InviteResponseFlags::IRF_RejectBlocked, nInviteFlags, nInviteId, nGameMode, pUniqueMatchId, nInviteFrom, nInviteType);
		return true;
	}
#endif

#if RSG_XDK
	// silently block any invites that straddle a reputation (we don't want bad / good to be invited by each other)
	bool bInviteIsFromBadReputation = (nInviteFlags & InviteFlags::IF_BadReputation) != 0;
	if(bInviteIsFromBadReputation != CLiveManager::IsOverallReputationBad())
	{
		gnetDebug1("AddRsInvite :: Invite from %s reputation. Local reputation: %s. Blocking.", bInviteIsFromBadReputation ? "Bad" : "Good", CLiveManager::IsOverallReputationBad() ? "Bad" : "Good");
		ResolveInvite(hInvitee, hFrom, hSessionInfo, InviteAction::IA_Blocked, ResolveFlags::RF_SendReply, InviteResponseFlags::IRF_RejectBlocked, nInviteFlags, nInviteId, nGameMode, pUniqueMatchId, nInviteFrom, nInviteType);
		return true;
    }
#endif

#if RSG_SCE
	if(CLiveManager::IsInBlockList(hFrom))
	{
		gnetDebug1("OnReceivedRsInvite :: Invite from blocked user. Blocking.");
		ResolveInvite(hInvitee, hFrom, hSessionInfo, InviteAction::IA_Blocked, ResolveFlags::RF_SendReply, InviteResponseFlags::IRF_RejectBlocked, nInviteFlags, nInviteId, nGameMode, pUniqueMatchId, nInviteFrom, nInviteType);
		return true;
	}
#endif

	// check prologue requirements
	if(!CNetwork::CheckSpPrologueRequirement())
	{
		// reply to invite, cannot access multiplayer
		ResolveInvite(hInvitee, hFrom, hSessionInfo, InviteAction::IA_Blocked, ResolveFlags::RF_SendReply, InviteResponseFlags::IRF_RejectCannotAccessMultiplayer, nInviteFlags, nInviteId, nGameMode, pUniqueMatchId, nInviteFrom, nInviteType);
		return true;
	}

	// check if script have indicated that they can receive / process invites
	if(!m_CanReceiveRsInvites)
	{
		// reply to invite, cannot access multiplayer
		gnetDebug1("AddRsInvite :: Reject - Script cannot process Rockstar invites");
		ResolveInvite(hInvitee, hFrom, hSessionInfo, InviteAction::IA_Blocked, ResolveFlags::RF_SendReply, InviteResponseFlags::IRF_RejectCannotProcessRockstar, nInviteFlags, nInviteId, nGameMode, pUniqueMatchId, nInviteFrom, nInviteType);
		return true;
	}

	// check if UI requirements mean we cannot show a Rockstar invite
	if(DoesUiRequirePlatformInvite())
	{
		// respond to invite, cannot process invites - this should result in a system invite being sent
		gnetDebug1("AddRsInvite :: Reject - UI Requires Platform Invite");
		ResolveInvite(hInvitee, hFrom, hSessionInfo, InviteAction::IA_Blocked, ResolveFlags::RF_SendReply, InviteResponseFlags::IRF_RejectCannotProcessRockstar, nInviteFlags, nInviteId, nGameMode, pUniqueMatchId, nInviteFrom, nInviteType);
		return true;
	}

	// if this invite already exists, reset the timeout
	int nInvites = m_RsInvites.GetCount();
	for(int i = 0; i < nInvites; i++)
	{
		if(m_RsInvites[i].m_Inviter == hFrom)
        {
			// refresh the time
			m_RsInvites[i].m_TimeAdded = sysTimer::GetSystemMsTime();
			
			// if this is a different session, copy the new info and send a reply
			if(m_RsInvites[i].m_SessionInfo != hSessionInfo)
			{
				gnetDebug1("AddRsInvite :: Invite to new session from %s. Token: 0x%016" I64FMT "x, InviteId: 0x%08x", NetworkUtils::LogGamerHandle(hFrom), hSessionInfo.GetToken().m_Value, m_RsInvites[i].m_InviteId);
				m_RsInvites[i].m_SessionInfo = hSessionInfo;
				ResolveInvite(hInvitee, hFrom, hSessionInfo, InviteAction::IA_None, ResolveFlags::RF_SendReply, InviteResponseFlags::IRF_None, nInviteFlags, nInviteId, nGameMode, pUniqueMatchId, nInviteFrom, nInviteType);
			}
			else
			{
				gnetDebug1("AddRsInvite :: Refreshing invite from %s. Token: 0x%016" I64FMT "x, InviteId: 0x%08x", NetworkUtils::LogGamerHandle(hFrom), hSessionInfo.GetToken().m_Value, m_RsInvites[i].m_InviteId);
			}
			return true;
		}
		else if(m_RsInvites[i].m_SessionInfo == hSessionInfo)
		{
			// discard invites to the same session from different players
			gnetDebug1("AddRsInvite :: Discarding duplicate invite from %s. Token: 0x%016" I64FMT "x, InviteId: 0x%08x", NetworkUtils::LogGamerHandle(hFrom), hSessionInfo.GetToken().m_Value, m_RsInvites[i].m_InviteId);
			return true;
		}
	}

	// work out if we have enough space
	if(!CheckAndMakeRoomForRsInvite())
	{
		gnetDebug1("OnReceivedRsInvite :: No room to add new invite!");
		return false;
	}

	// initialise the invite
	sRsInvite tInvite;
	tInvite.Init(hInvitee, hSessionInfo, hFrom, szGamerName, nInviteId, nGameMode, nCrewId, szContentId, pUniqueMatchId, nInviteFrom, nInviteType, nPlaylistLength, nPlaylistCurrent, nInviteFlags);

	// logging
	gnetDebug1("AddRsInvite :: To: %s, From: %s. Token: 0x%016" I64FMT "x, Crew: %d, InviteId: 0x%08x, Flags: 0x%x", hInvitee.GetName(), NetworkUtils::LogGamerHandle(hFrom), hSessionInfo.GetToken().m_Value, tInvite.m_CrewId, tInvite.m_InviteId, tInvite.m_InviteFlags);

	// trigger telemetry event
	bool inviteTelemetryActive = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("TELEMETRY_RS_INVITE_ACTIVE", 0xe331f380), false);
	if(inviteTelemetryActive)
	{
		MetricAddRsInvite m(tInvite.m_Inviter, tInvite.m_szContentId, tInvite.m_nPlaylistLength, tInvite.m_nPlaylistCurrent, tInvite.IsSCTV());
		CNetworkTelemetry::AppendMetric(m);
	}
#if !__FINAL
	else
	{
		MetricDebugAddRsInvite m(tInvite.m_Inviter, tInvite.m_szContentId, tInvite.m_nPlaylistLength, tInvite.m_nPlaylistCurrent, tInvite.IsSCTV());
		CNetworkDebugTelemetry::AppendDebugMetric(&m);
	}
#endif// !__FINAL

	// add and indicate success
	m_RsInvites.Push(tInvite);

#if RSG_XBOX
	if(!(PARAM_netInviteScriptHandleNoPrivilege.Get() && !CLiveManager::CheckOnlinePrivileges()))
	{
		sRsInvite& tNewInvite = m_RsInvites.Top();

		// we always check multiplayer
		IPrivacySettings::CheckPermission(hInvitee.GetLocalIndex(), IPrivacySettings::PERM_PlayMultiplayer, hFrom, &tNewInvite.m_MultiplayerCheck.m_Result, &tNewInvite.m_MultiplayerCheck.m_Status);
		tNewInvite.m_MultiplayerCheck.m_bCheckRequired = true;

		// check user content if we're blocked
		if(hContentCreator.IsValid() && !CLiveManager::CheckUserContentPrivileges() && rlFriendsManager::IsFriendsWith(NetworkInterface::GetLocalGamerIndex(), hContentCreator))
		{
			gnetDebug1("AddRsInvite :: Content Creator: %s", NetworkUtils::LogGamerHandle(hContentCreator));

			tNewInvite.m_ContentCheck.m_bCheckRequired = true;
			IPrivacySettings::CheckPermission(hInvitee.GetLocalIndex(), IPrivacySettings::PERM_ViewTargetUserCreatedContent, hContentCreator, &tNewInvite.m_ContentCheck.m_Result, &tNewInvite.m_ContentCheck.m_Status);
		}
	}
	else
#else
	if(!PARAM_netInviteScriptHandleNoPrivilege.Get() && (!CLiveManager::CheckUserContentPrivileges() && hContentCreator.IsValid()))
	{
		// reply to invite, inactive gamer profile
		gnetDebug1("AddRsInvite :: No user content privileges. Content Creator: %s, Blocked", NetworkUtils::LogGamerHandle(hContentCreator));
		ResolveInvite(hInvitee, hFrom, hSessionInfo, InviteAction::IA_Blocked, ResolveFlags::RF_SendReply, InviteResponseFlags::IRF_RejectBlocked, nInviteFlags, nInviteId, nGameMode, pUniqueMatchId, nInviteFrom, nInviteType);
		return true;
	}
	else
#endif
	{
		// reply to invite, no decision made
		ResolveInvite(hInvitee, hFrom, hSessionInfo, InviteAction::IA_None, ResolveFlags::RF_SendReply, InviteResponseFlags::IRF_None, nInviteFlags, nInviteId, nGameMode, pUniqueMatchId, nInviteFrom, nInviteType);

		// generate script event
		CEventNetworkPresenceInvite tEvent(tInvite.m_Inviter, tInvite.m_InviterName, tInvite.m_szContentId, tInvite.m_nPlaylistLength, tInvite.m_nPlaylistCurrent, tInvite.IsSCTV(), tInvite.m_InviteId);
		GetEventScriptNetworkGroup()->Add(tEvent);
	}

	return true; 
}

bool InviteMgr::OnReceivedAdminInvite(const rlGamerInfo& hInvitee, const rlSessionInfo& hSessionInfo, const rlGamerHandle& hSessionHost, const unsigned inviteFlags)
{
    const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();
    if(pInfo && *pInfo != hInvitee)
    {
        // reply to invite, inactive gamer profile
        gnetDebug1("OnReceivedAdminInvite :: Cannot process. Invite to inactive profile");
        return false;
    }

	// generate an invite Id
	const InviteId inviteId = InviteMgr::GenerateInviteId();

	// validate that we can process admin invites
	if(!m_bAllowAdminInviteProcessing)
	{
		gnetDebug1("OnReceivedAdminInvite :: Admin invites disabled!");
		ResolveInvite(hInvitee, hSessionHost, hSessionInfo, InviteAction::IA_Blocked, ResolveFlags::RF_None, InviteResponseFlags::IRF_RejectIncompatible | InviteResponseFlags::IRF_CannotProcessAdmin, inviteFlags, inviteId, 0, nullptr, -1, -1);
		return true;
	}

    // if this invite already exists, reset the timeout
    int nInvites = m_RsInvites.GetCount();
    for(int i = 0; i < nInvites; i++)
    {
        if(m_RsInvites[i].m_SessionInfo == hSessionInfo)
        {
            // logging
            gnetDebug1("OnReceivedAdminInvite :: Refreshing - Token: 0x%016" I64FMT "x, InviteId: 0x%08x", hSessionInfo.GetToken().m_Value, m_RsInvites[i].m_InviteId);

            // just refresh the timeout
            m_RsInvites[i].m_TimeAdded = sysTimer::GetSystemMsTime();
            return true;
        }
    }

	// work out if we have enough space
	if(!CheckAndMakeRoomForRsInvite())
	{
		gnetDebug1("OnReceivedAdminInvite :: No room to add new invite!");
		return false;
	}

	// initialise the invite
	sRsInvite tInvite;
	tInvite.InitAdmin(hInvitee, hSessionInfo, hSessionHost, inviteFlags, inviteId);

    // logging
    gnetDebug1("OnReceivedAdminInvite :: Token: 0x%016" I64FMT "x, InviteId: 0x%08x", hSessionInfo.GetToken().m_Value, tInvite.m_InviteId);

    // generate script event
    CEventNetworkPresenceInvite tEvent(tInvite.m_Inviter, tInvite.m_InviterName, tInvite.m_szContentId, tInvite.m_nPlaylistLength, tInvite.m_nPlaylistCurrent, tInvite.IsSCTV(), tInvite.m_InviteId);
    GetEventScriptNetworkGroup()->Add(tEvent);

    // add and indicate success
    m_RsInvites.Push(tInvite);
    return true; 
}

bool InviteMgr::OnReceivedTournamentInvite(const rlGamerInfo& hInvitee, const rlSessionInfo& hSessionInfo, const rlGamerHandle& hSessionHost, const char* szContentId, const unsigned nTournamentEventId, const unsigned inviteFlags)
{
    const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();
    if(pInfo && *pInfo != hInvitee)
    {
        // reply to invite, inactive gamer profile
        gnetDebug1("OnReceivedTournamentInvite :: Cannot process. Invite to inactive profile");
        return false;
    }

	// generate an invite Id
	const InviteId inviteId = InviteMgr::GenerateInviteId();

	// validate that we can process admin invites
	if(!m_bAllowAdminInviteProcessing)
	{
		gnetDebug1("OnReceivedTournamentInvite :: Admin invites disabled!");
		ResolveInvite(hInvitee, hSessionHost, hSessionInfo, InviteAction::IA_Blocked, ResolveFlags::RF_None, InviteResponseFlags::IRF_RejectIncompatible | InviteResponseFlags::IRF_CannotProcessAdmin, inviteFlags, inviteId, 0, nullptr, -1, -1);
		return true;
	}

    // if this invite already exists, reset the timeout
    int nInvites = m_RsInvites.GetCount();
    for(int i = 0; i < nInvites; i++)
    {
        if(m_RsInvites[i].m_SessionInfo == hSessionInfo)
        {
            // logging
            gnetDebug1("OnReceivedTournamentInvite :: Refreshing - Token: 0x%016" I64FMT "x, InviteId: 0x%08x", hSessionInfo.GetToken().m_Value, m_RsInvites[i].m_InviteId);

            // just refresh the timeout
            m_RsInvites[i].m_TimeAdded = sysTimer::GetSystemMsTime();
            return true;
        }
    }

	// work out if we have enough space
	if(!CheckAndMakeRoomForRsInvite())
	{
		gnetDebug1("OnReceivedTournamentInvite :: No room to add new invite!");
		return false;
	}

	// initialise the invite
	sRsInvite tInvite;
	tInvite.InitTournament(hInvitee, hSessionInfo, hSessionHost, szContentId, nTournamentEventId, inviteFlags, inviteId);

    // logging
    gnetDebug1("OnReceivedTournamentInvite :: Token: 0x%016" I64FMT "x, InviteId: 0x%08x", hSessionInfo.GetToken().m_Value, tInvite.m_InviteId);

    // generate script event
    CEventNetworkPresenceInvite tEvent(tInvite.m_Inviter, tInvite.m_InviterName, tInvite.m_szContentId, tInvite.m_nPlaylistLength, tInvite.m_nPlaylistCurrent, tInvite.IsSCTV(), tInvite.m_InviteId);
    GetEventScriptNetworkGroup()->Add(tEvent);

    // add and indicate success
    m_RsInvites.Push(tInvite);
    return true; 
}

bool InviteMgr::CancelRsInvite(const rlSessionInfo& hSessionInfo, const rlGamerHandle& hFrom)
{
	// logging
	gnetDebug1("CancelRsInvite :: From: %s, Token: 0x%016" I64FMT "x", NetworkUtils::LogGamerHandle(hFrom), hSessionInfo.GetToken().m_Value);

	// cancel all invites to this session
	int nInvitesBefore = m_RsInvites.GetCount();
	int nInvites = nInvitesBefore;
	for(int i = 0; i < nInvites; i++)
	{
		if(m_RsInvites[i].m_SessionInfo == hSessionInfo && 
           m_RsInvites[i].m_Inviter == hFrom)
		{
			// remove invite
			RemoveRsInvite(i, RemoveReason::RR_Cancelled);
			--i;
			--nInvites;
		}
	}

	// return whether we removed some invites
	return (nInvites < nInvitesBefore);
}

bool InviteMgr::OnInviteResponseReceived(const rlGamerHandle& hFrom, const rlSessionToken& sessionToken, const unsigned OUTPUT_ONLY(nInviteId), const unsigned responseFlags, const unsigned inviteFlags, const int nCharacterRank, const char* szClanTag, const bool bDecisionMade, const bool bDecision)
{
    gnetDebug1("OnInviteResponseReceived :: From: %s, Token: 0x%016" I64FMT "x, InviteId: 0x%08x, ResponseFlags: 0x%x, InviteFlags: 0x%x, Rank: %d, ClanTag: %s, DecisionMade: %s, Decision: %s", 
		NetworkUtils::LogGamerHandle(hFrom), 
		sessionToken.m_Value, 
		nInviteId, 
		responseFlags, 
		inviteFlags,
		nCharacterRank, 
		szClanTag, 
		bDecisionMade ? "T" : "F",
		bDecision ? "T" : "F");

	OUTPUT_ONLY(LogResponseFlags(responseFlags, "OnInviteResponseReceived"));

	// if the remote user was sent a direct invite and cannot process Rockstar invites, send a follow up platform invite
	if((responseFlags & InviteResponseFlags::IRF_RejectCannotProcessRockstar) != 0 &&
		(inviteFlags & InviteFlags::IF_Rockstar) == 0 &&
		(inviteFlags & InviteFlags::IF_NotTargeted) == 0)
	{
		gnetDebug1("OnInviteResponseReceived :: %s cannot receive R* invites, sending a platform invite", hFrom.ToString());
		CNetwork::GetNetworkSession().SendPlatformInvite(sessionToken, &hFrom, 1);
	}

	if((responseFlags & InviteResponseFlags::IRF_StatusFromJoinQueue) != 0)
		CNetwork::GetNetworkSession().HandleJoinQueueInviteReply(hFrom, sessionToken, bDecisionMade, bDecision, responseFlags);
	else
	{
		// check if this reply for a session we are still a part of and still have a pending invite for
		bool bProcessReply = false;
		if(CNetwork::GetNetworkSession().IsMySessionToken(sessionToken))
		{
			bProcessReply = CNetwork::GetNetworkSession().GetInvitedPlayers().DidInvite(hFrom);
			gnetDebug1("OnInviteResponseReceived :: Reply for Main Session. Has Invite Record: %s", bProcessReply ? "True" : "False");
		}
		else if(CNetwork::GetNetworkSession().IsMyTransitionToken(sessionToken))
		{
			bProcessReply = CNetwork::GetNetworkSession().GetTransitionInvitedPlayers().DidInvite(hFrom);
			gnetDebug1("OnInviteResponseReceived :: Reply for Transition Session. Has Invite Record: %s", bProcessReply ? "True" : "False");
		}

		if(bProcessReply)
		{
			// acknowledge this invite (if an ack reply) and set status
			CNetwork::GetNetworkSession().AckInvite(hFrom);
			CNetwork::GetNetworkSession().SetInviteStatus(hFrom, responseFlags);

			// flag that a decision has been made
			if(bDecisionMade)
				CNetwork::GetNetworkSession().SetInviteDecisionMade(hFrom);

			// generate script event (not if we have already made a decision and this message indicates we haven't - this occurs due to 
			// aggregating Rockstar messages on the client)
			if(!(CNetwork::GetNetworkSession().IsInviteDecisionMade(hFrom) && !bDecisionMade))
			{
				CEventNetworkPresenceInviteReply tEvent(hFrom, responseFlags, nCharacterRank, szClanTag, bDecisionMade, bDecision);
				GetEventScriptNetworkGroup()->Add(tEvent);
			}
		}
		else
		{
			gnetDebug1("OnInviteResponseReceived :: Not processing reply. In Session: %s, Invited to Session: %s, In Transition: %s, Invited to Transition: %s",
						CNetwork::GetNetworkSession().IsMySessionToken(sessionToken) ? "True" : "False",
						CNetwork::GetNetworkSession().GetInvitedPlayers().DidInvite(hFrom) ? "True" : "False",
						CNetwork::GetNetworkSession().IsMyTransitionToken(sessionToken) ? "True" : "False",
						CNetwork::GetNetworkSession().GetTransitionInvitedPlayers().DidInvite(hFrom) ? "True" : "False");
		}
	}
    
    return true;
}

void InviteMgr::ResolveInvite(sAcceptedInvite* pAcceptedInvite, const InviteAction inviteAction, const unsigned nResolveFlags, const unsigned nResponseFlags)
{
	ResolveInvite(
		pAcceptedInvite->GetInvitee(),
		pAcceptedInvite->GetInviter(),
		pAcceptedInvite->GetSessionInfo(),
		inviteAction,
		nResolveFlags,
		nResponseFlags,
		pAcceptedInvite->GetFlags(),
		pAcceptedInvite->GetInviteId(),
		pAcceptedInvite->GetGameMode(),
		pAcceptedInvite->GetUniqueMatchId(),
		pAcceptedInvite->GetInviteFrom(),
		pAcceptedInvite->GetInviteType());
}

void InviteMgr::ResolveInvite(const sRsInvite& rsInvite, const InviteAction inviteAction, const unsigned nResolveFlags, const unsigned nResponseFlags)
{
	ResolveInvite(
		rsInvite.m_Invitee,
		rsInvite.m_Inviter,
		rsInvite.m_SessionInfo,
		inviteAction,
		nResolveFlags,
		nResponseFlags,
		rsInvite.m_InviteFlags,
		rsInvite.m_InviteId,
		rsInvite.m_GameMode,
		rsInvite.m_UniqueMatchId,
		rsInvite.m_InviteFrom,
		rsInvite.m_InviteType);
}

void InviteMgr::ResolveInvite(sPlatformInvite& platformInvite, const InviteAction inviteAction, const unsigned nResolveFlags, const unsigned nResponseFlags)
{
	ResolveInvite(
		platformInvite.m_Invitee,
		platformInvite.m_Inviter,
		platformInvite.m_SessionInfo,
		inviteAction,
		nResolveFlags,
		nResponseFlags,
		platformInvite.m_Flags,
		platformInvite.m_InviteId,
		-1,
		nullptr,
		0,
		0);
}

void InviteMgr::ResolveInvite(
	const rlGamerInfo& hInvitee,
	const rlGamerHandle& hFrom,
	const rlSessionInfo& hSessionInfo,
	const InviteAction inviteAction,
	const unsigned nResolveFlags,
	const unsigned nResponseFlags,
	const unsigned nInviteFlags,
	const unsigned inviteId,
	const int gameMode,
	const char* pUniqueMatchId,
	const int inviteFrom,
	const int inviteType)
{
	// logging
	gnetDebug1("ResolveInvite :: To: %s, Token: 0x%016" I64FMT "x, Action: %s, InviteId: 0x%08x, ResolveFlags: 0x%x, ResponseFlags: 0x%x, Flags: 0x%x", 
		NetworkUtils::LogGamerHandle(hFrom), 
		hSessionInfo.GetToken().GetValue(),
		GetInviteActionAsString(inviteAction),
		inviteId,
		nResolveFlags,
		nResponseFlags,
		nInviteFlags);
	
	// send telemetry if the invite is being completed
	if(inviteAction != InviteAction::IA_None)
	{
		// don't bother if we don't have an assigned local profile yet
		const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		if(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
		{
			const bool isFriendsWith = rlFriendsManager::IsFriendsWith(localGamerIndex, hFrom);

			CNetworkTelemetry::InviteResult(
				hSessionInfo.GetToken().GetValue(),
				pUniqueMatchId,
				gameMode,
				nInviteFlags,
				inviteId,
				inviteAction,
				inviteFrom,
				inviteType,
				hFrom,
				isFriendsWith);
		}
	}

	const bool sendResponse =
		((nResolveFlags & ResolveFlags::RF_SendReply) != 0) &&	// only when instructed to do so
		((nInviteFlags & InviteFlags::IF_ViaAdmin) == 0) &&		// don't send responses to admin invites
		hInvitee.IsValid();

	// send a reply if requested (and the invitee is valid)
	if(sendResponse)
	{
		bool bDecision = (inviteAction == InviteAction::IA_Accepted);
		SendInviteResponse(hInvitee, hFrom, hSessionInfo.GetToken(), inviteId, (inviteAction != InviteAction::IA_None) ? &bDecision : nullptr, nResponseFlags, nInviteFlags);
	}
}

void InviteMgr::SendInviteResponse(
	const rlGamerInfo& hInvitee, 
	const rlGamerHandle& hFrom, 
	const rlSessionToken& sessionToken, 
	const unsigned nInviteId, 
	bool* bDecision,
	unsigned responseFlags, 
	unsigned inviteFlags)
{
	// validate inviter
	if(!hFrom.IsValid() || hFrom == hInvitee.GetGamerHandle())
		return;

#if !RSG_XBOX
	// add this on when players do not have multiplayer privileges
	if(!NetworkInterface::CheckOnlinePrivileges())
		responseFlags |= InviteResponseFlags::IRF_RejectBlocked;
#endif

    // work out where we are in the game
    if(!NetworkInterface::IsNetworkOpen())
		responseFlags |= InviteResponseFlags::IRF_StatusInSinglePlayer;
    else
    {
        CNetworkSession& rSession = CNetwork::GetNetworkSession();
        if(rSession.IsTransitionActive() && !rSession.IsInTransitionToGame())
			responseFlags |= InviteResponseFlags::IRF_StatusInLobby;
        else if(rSession.IsActivitySession())
			responseFlags |= InviteResponseFlags::IRF_StatusInActivity;
        else
			responseFlags |= InviteResponseFlags::IRF_StatusInFreeroam;
    }

	// signal if this is a reply from a join queue
	if(CLiveManager::GetInviteMgr().GetAcceptedInvite()->IsFromJoinQueue())
		responseFlags |= InviteResponseFlags::IRF_StatusFromJoinQueue;

	//@@: range INVITEMGR_REPLYTOINVITE_CHECK_CHEATER {
	// check cheater and bad sport settings
	if(CNetwork::IsCheater(hInvitee.GetLocalIndex()))
		responseFlags |= InviteResponseFlags::IRF_StatusCheater;
	else if(!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) && StatsInterface::IsBadSport(OUTPUT_ONLY(true)))
		responseFlags |= InviteResponseFlags::IRF_StatusBadSport;
	//@@: } INVITEMGR_REPLYTOINVITE_CHECK_CHEATER

    // work out if we can process a Rockstar invite
    if(cStoreScreenMgr::IsStoreMenuOpen())
    {
		responseFlags |= InviteResponseFlags::IRF_StatusInStore;
    }

    if(CScriptHud::bUsingMissionCreator)
    {
		responseFlags |= InviteResponseFlags::IRF_StatusInCreator;
    }

	// if we have rejected the invite for blocked reasons (incompatibility, privileges, etc.) 
	// then remove this flag as we don't want to receive a platform invite
	if((responseFlags & InviteResponseFlags::IRF_RejectBlockedMask) != 0)
		responseFlags &= ~InviteResponseFlags::IRF_RejectCannotProcessRockstar;

	// if the invite was not targeted, remove the flag requesting a platform invite
	if((inviteFlags & InviteFlags::IF_NotTargeted) != 0)
		responseFlags &= ~InviteResponseFlags::IRF_RejectCannotProcessRockstar;

	// check flags for status types that script require
	bool bAlwaysSendReply = false;
	if(!m_bSendRsInviteAcks)
	{
		// considered 'blocked'
		bAlwaysSendReply |= ((responseFlags & InviteResponseFlags::IRF_RejectNoPrivilege) != 0);
		bAlwaysSendReply |= ((responseFlags & InviteResponseFlags::IRF_RejectBlocked) != 0);
		bAlwaysSendReply |= ((responseFlags & InviteResponseFlags::IRF_RejectBlockedInactive) != 0);
		// considered 'busy'
		bAlwaysSendReply |= ((responseFlags & InviteResponseFlags::IRF_StatusInStore) != 0);
		bAlwaysSendReply |= ((responseFlags & InviteResponseFlags::IRF_StatusInCreator) != 0);
		// considered 'cheater'
		bAlwaysSendReply |= ((responseFlags & InviteResponseFlags::IRF_RejectCannotAccessMultiplayer) != 0);
		bAlwaysSendReply |= ((responseFlags & InviteResponseFlags::IRF_StatusCheater) != 0);
		bAlwaysSendReply |= ((responseFlags & InviteResponseFlags::IRF_StatusBadSport) != 0);

		// additional tunable here, these are likely to be the bulk of the busy replies
		if(m_bSendRsInviteBusyForActivities)
		{
			bAlwaysSendReply |= ((responseFlags & InviteResponseFlags::IRF_StatusInLobby) != 0);
			bAlwaysSendReply |= ((responseFlags & InviteResponseFlags::IRF_StatusInActivity) != 0);
		}

		// always ack invites that are sent directly (and not part of an anonymous group)
		bAlwaysSendReply |= ((inviteFlags & InviteFlags::IF_NotTargeted) == 0);
	}
	
	// if we don't need to reply, don't
	if(!bDecision && !bAlwaysSendReply && !m_bSendRsInviteAcks)
		return;

    // grab the character rank
    int nCharacterRank = StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) ? 0 : StatsInterface::GetIntStat(STAT_RANK_FM.GetStatId());

    char szClanTag[MAX_CLAN_TAG_LENGTH];
	szClanTag[0] = '\0';
	
	// social query - if we have a current crew
	if(NetworkInterface::IsLocalPlayerSignedIn())
	{
		NetworkClan& tClan = CLiveManager::GetNetworkClan();
		if(tClan.HasPrimaryClan())
			NetworkClan::GetUIFormattedClanTag(*tClan.GetPrimaryClan(), -1, szClanTag, MAX_CLAN_TAG_LENGTH);
	}

	// logging
	gnetDebug1("SendInviteResponse :: To: %s, Token: 0x%016" I64FMT "x, InviteId: 0x%08x, ResponseFlags: 0x%x, InviteFlags: 0x%x, Rank: %d, ClanTag: %s, DecisionMade: %s, Decision: %s", 
		NetworkUtils::LogGamerHandle(hFrom), 
		sessionToken.m_Value, 
		nInviteId, 
		responseFlags, 
		inviteFlags, 
		nCharacterRank, 
		szClanTag, 
		bDecision != nullptr ? "T" : "F", 
		bDecision ? (*bDecision ? "T" : "F") : "None");

	OUTPUT_ONLY(LogResponseFlags(responseFlags, "SendInviteResponse"));

    // reply to the invite
    CGameInviteReply::Trigger(hFrom, sessionToken, nInviteId, responseFlags, inviteFlags, nCharacterRank, szClanTag, bDecision != nullptr, bDecision ? *bDecision : false);
}

bool InviteMgr::NotifyPlatformInviteAccepted(sAcceptedInvite* WIN32PC_ONLY(pInvite))
{
#if RSG_PC
	char sessionBuf[rlSessionInfo::TO_STRING_BUFFER_SIZE] = {0};
	if (rlVerify(pInvite->GetSessionInfo().ToString(sessionBuf)))
	{
		return g_rlPc.GetPresenceManager()->NotifyGameInviteConsumed(pInvite->GetInviter().GetRockstarId(), sessionBuf, rgsc::IPresenceManagerV4::ACCEPTED);
	}
#elif RSG_NP
	//  TODO: Future Invite Improvements
	//	The invitation ID is in NpBasic but currently does not propagate up to this system, but we could expand
	//   coverage for this, allowing the game to clear a PS4 system invite
	// g_rlNp.GetWebAPI().ClearInvitation(NetworkInterface::GetLocalGamerIndex(), pInvite->TheoreticalGetWebApiInvitationDetail());
#elif RSG_XBOX
	// Maybe someday...
#endif

	return false;
}

bool InviteMgr::HasPendingPlatformInvite() const
{
	return m_PendingPlatformInvite.IsValid();
}

bool InviteMgr::IsPlatformInviteJvP() const
{
	return m_PendingPlatformInvite.IsValid() && ((m_PendingPlatformInvite.m_Flags & InviteFlags::IF_IsJoin) != 0);
}

const rlGamerHandle& InviteMgr::GetPlatformInviteInviter() const
{
	return m_PendingPlatformInvite.IsValid() ? m_PendingPlatformInvite.m_Inviter : RL_INVALID_GAMER_HANDLE;
}

InviteId InviteMgr::GetPlatformInviteId() const
{
	return m_PendingPlatformInvite.IsValid() ? m_PendingPlatformInvite.m_InviteId : INVALID_INVITE_ID;
}

unsigned InviteMgr::GetPlatformInviteFlags() const
{
	return m_PendingPlatformInvite.IsValid() ? m_PendingPlatformInvite.m_Flags : 0;
}

void InviteMgr::AddPlatformInvite(const rlGamerInfo& hInvitee, const rlGamerHandle& hInviter, const rlSessionInfo& sessionInfo, unsigned inviteFlags)
{
	gnetDebug1("AddPlatformInvite :: Invitee: %s, Inviter: %s, Flags: 0x%x", hInvitee.GetName(), hInviter.ToString(), inviteFlags);

	// for invites to inactive users, it's no longer required to prompt so the switch is now handled 
	// at a lower level when the invite is received 
	if(!gnetVerifyf(hInvitee.GetGamerHandle() == NetworkInterface::GetLocalGamerHandle(), "AddPlatformInvite :: Received invite to inactive user!"))
	{
		// if this happens, we can just eject
		return;
	}

	// check if we have a pending (not actioned) platform invite
	if(m_PendingPlatformInvite.IsValid())
	{
		// resolve the previous invite
		const unsigned nLocalRejectFlags = InviteResponseFlags::IRF_RejectReplacedByNewerInvite;
		ResolveInvite(m_PendingPlatformInvite, InviteAction::IA_NotRelevant, ResolveFlags::RF_SendReply, nLocalRejectFlags);

		gnetDebug1("AddPlatformInvite :: Clearing pending platform invite");
		m_PendingPlatformInvite.Clear();
	}

	// remove any unaccepted invites matching this inviter
	RemoveMatchingUnacceptedInvites(sessionInfo, hInviter);

	// check if this is a boot invite
	if(m_HasSkipBootUiResult && !m_HasClaimedBootInvite)
	{
		inviteFlags |= InviteFlags::IF_IsBoot;
		m_HasClaimedBootInvite = true;
	}

	// capture the platform invite data
	m_PendingPlatformInvite.Init(hInvitee, hInviter, sessionInfo, inviteFlags);

	// there are some pre-checks that we need to wait for in order to process platform invites
	m_PendingPlatformInviteFlowState = PendingPlatformInviteFlowState::FS_WaitingForAccessChecks;
}

void InviteMgr::ClearPlatformInvite()
{
	if(!m_PendingPlatformInvite.IsValid())
		return;

	gnetDebug1("ClearPlatformInvite");
	m_PendingPlatformInvite.Clear();
	m_PendingPlatformInviteFlowState = PendingPlatformInviteFlowState::FS_Inactive;
	m_PendingPlatformScriptEventCheck = false;
}

void InviteMgr::UpdatePendingPlatformFlowState()
{
	switch (m_PendingPlatformInviteFlowState)
	{
	case PendingPlatformInviteFlowState::FS_Inactive:
		break;

	case PendingPlatformInviteFlowState::FS_WaitingForAccessChecks:
	{
		PlatformInviteCheckResult checkResult = CheckAndFlagPlatformInviteAlerts();
		switch(checkResult)
		{
		case PlatformInviteCheckResult::Result_NotReady:
			/* wait */
			break;

		case PlatformInviteCheckResult::Result_Failed:
			gnetDebug1("UpdatePendingPlatformFlowState :: CheckAndFlagPlatformInviteAlerts returned Result_Failed");
			ClearPlatformInvite();
			break;

		case PlatformInviteCheckResult::Result_Succeeded:
			gnetDebug1("UpdatePendingPlatformFlowState :: CheckAndFlagPlatformInviteAlerts returned Result_Succeeded");
			m_PendingPlatformInviteFlowState = PendingPlatformInviteFlowState::FS_WaitingForRequiredUi;

			// this clears out any pending bail screens as we're now progressing our invite
			CNetwork::OnPlatformInvitePassedChecks();

			break;

		default:
			break;
		}
	}
	break;

	case PendingPlatformInviteFlowState::FS_WaitingForRequiredUi:
		if(!NetworkInterface::IsOnUnskippableBootUi())
		{
			gnetDebug1("UpdatePendingPlatformFlowState :: Passed Required Ui");
			NetworkInterface::CloseAllUiForInvites();

			// advance state and make an immediate check to see if we can generate an invite
			m_PendingPlatformInviteFlowState = PendingPlatformInviteFlowState::FS_WaitingForScript;
			TryProcessPlatformInvite(OUTPUT_ONLY("UpdatePendingPlatformFlowState::FS_WaitingForRequiredUi"));
		}
		break;

	case PendingPlatformInviteFlowState::FS_WaitingForScript:
		if(m_PendingPlatformScriptEventCheck)
		{
			// we only need to check this flag here, we'll attempt separately for other blocking checks like access
			TryProcessPlatformInvite(OUTPUT_ONLY("UpdatePendingPlatformFlowState::FS_WaitingForScript"));
			m_PendingPlatformScriptEventCheck = false;
		}
		break;
	}
}

InviteMgr::PlatformInviteCheckResult InviteMgr::CheckAndFlagPlatformInviteAlerts()
{
	// check if we're ready to make platform invite checks...
	if(!NetworkInterface::IsReadyToCheckNetworkAccess(eNetworkAccessArea::AccessArea_Multiplayer))
		return PlatformInviteCheckResult::Result_NotReady;

	// also check stats...
	if(ShouldPlatformInviteCheckStats() && !NetworkInterface::IsReadyToCheckStats())
		return PlatformInviteCheckResult::Result_NotReady;

	// check if we can access multiplayer
	if(!NetworkInterface::CanAccessNetworkFeatures(eNetworkAccessArea::AccessArea_Multiplayer))
	{
		gnetDebug1("CheckAndFlagPlatformInviteAlerts :: Cannot access multiplayer! Access alert pending...");
		m_PendingNetworkAccessAlert = true;

		// invite checks failed
		return PlatformInviteCheckResult::Result_Failed;
	}

	// check early reject conditions
	unsigned nLocalRejectFlags = 0;

	// block invites when we haven't completed the single player prologue
	if(!CNetwork::CheckSpPrologueRequirement())
	{
		gnetDebug1("CheckAndFlagPlatformInviteAlerts :: Reject - Not Completed Single Player Prologue");
		nLocalRejectFlags = InviteResponseFlags::IRF_RejectNotCompletedSpPrologue;
		m_PendingAlertReason = InviteAlertReason::IAR_NotCompletedSpIntro;
	}
	// block invites when we haven't completed the multiplayer intro flow and it's a requirement
	else if(ShouldPlatformInviteCheckStats() && !NetworkInterface::HasActiveCharacter())
	{
		gnetDebug1("CheckAndFlagPlatformInviteAlerts :: Reject - No Multiplayer Character");
		nLocalRejectFlags = InviteResponseFlags::IRF_RejectNoMultiplayerCharacter;
		m_PendingAlertReason = InviteAlertReason::IAR_NoMultiplayerCharacter;
	}
	// block invites when we haven't completed the multiplayer intro flow and it's a requirement
	else if(ShouldPlatformInviteCheckStats() && !NetworkInterface::HasCompletedMultiplayerIntro())
	{
		gnetDebug1("CheckAndFlagPlatformInviteAlerts :: Reject - Not Completed MultiPlayer Intro Flow");
		nLocalRejectFlags = InviteResponseFlags::IRF_RejectNotCompletedMultiplayerIntro;
		m_PendingAlertReason = InviteAlertReason::IAR_NotCompletedMultiplayerIntro;
	}
#if GTA_REPLAY
	else if(CVideoEditorInterface::IsActive() || CTheScripts::GetIsInDirectorMode())
	{
		gnetDebug1("CheckAndFlagPlatformInviteAlerts :: Reject - In Video Editor");
		nLocalRejectFlags = InviteResponseFlags::IRF_RejectBlocked | InviteResponseFlags::IRF_StatusInEditor;
		m_PendingAlertReason = CVideoEditorInterface::IsActive() ? InviteAlertReason::IAR_VideoEditor : InviteAlertReason::IAR_VideoDirector;
	}
#endif // GTA_REPLAY

	// if we have some rejection reasons
	if(nLocalRejectFlags != 0)
	{
		gnetDebug1("CheckAndFlagPlatformInviteAlerts :: Rejected, Flags: 0x%x", nLocalRejectFlags);
		ResolveInvite(m_PendingPlatformInvite, InviteAction::IA_Blocked, ResolveFlags::RF_SendReply, nLocalRejectFlags);

		// invite checks failed
		return PlatformInviteCheckResult::Result_Failed;
	}

	// checks passed
	return PlatformInviteCheckResult::Result_Succeeded;
}

void InviteMgr::TryProcessPlatformInvite(OUTPUT_ONLY(const char* szSource))
{
	if(!m_PendingPlatformInvite.IsValid())
		return;

	if(m_PendingPlatformInviteFlowState != PendingPlatformInviteFlowState::FS_WaitingForScript)
		return;

	// keep checking failure conditions in case they change while we're waiting
	PlatformInviteCheckResult checkResult = CheckAndFlagPlatformInviteAlerts();
	if(checkResult == PlatformInviteCheckResult::Result_Failed)
	{
		gnetDebug1("TryProcessPlatformInvite :: Failing invite, CheckPlatformInviteAlerts failed");
		ClearPlatformInvite();
		return;
	}

	if(!NetworkInterface::HasCredentialsResult())
	{
		gnetDebug1("TryProcessPlatformInvite :: %s - No credentials result", szSource);
		return;
	}

	// we can now generate a script event
	gnetDebug1("TryProcessPlatformInvite :: %s - Generating platform script event", szSource);
	
	// on V, we can just move to initialise our accepted invite
	ActionPlatformInvite();

	// reset flow state
	m_PendingPlatformInviteFlowState = PendingPlatformInviteFlowState::FS_Inactive;
}

bool InviteMgr::ShouldPlatformInviteCheckStats() const
{
#if !__FINAL
	int value = 0;
	if(PARAM_netInvitePlatformCheckStats.Get(value))
		return value != 0;
#endif

	return (Tunables::IsInstantiated() && Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("INVITE_PLATFORM_CHECK_STATS", 0xd5a5e0c1), true));
}

bool InviteMgr::ActionPlatformInvite()
{
	if(!gnetVerifyf(m_PendingPlatformInvite.IsValid(), "ActionPlatformInvite :: Invalid pending platform invite!"))
		return false;

	gnetDebug1("ActionPlatformInvite :: InviteId: 0x%08x, Time Pending: %ums", m_PendingPlatformInvite.m_InviteId, sysTimer::GetSystemMsTime() - m_PendingPlatformInvite.m_TimeAccepted);

	// accept and clear the invite
	AcceptInvite(m_PendingPlatformInvite.m_SessionInfo, m_PendingPlatformInvite.m_Invitee, m_PendingPlatformInvite.m_Inviter, m_PendingPlatformInvite.m_InviteId, m_PendingPlatformInvite.m_Flags, -1, "", -1, -1);
	m_PendingPlatformInvite.Clear();

	return true;
}

void InviteMgr::OnSetCredentialsResult(const bool bHasResult, const bool OUTPUT_ONLY(bResult))
{
	gnetDebug1("OnSetCredentialsResult :: HasResult: %s, Success: %s", bHasResult ? "True" : "False", bResult ? "True" : "False");
	if(bHasResult)
		TryProcessPlatformInvite(OUTPUT_ONLY("OnSetCredentialsResult"));
}

int InviteMgr::GetRsInviteIndexByInviteId(const InviteId nInviteId)
{
	// need to delete something, use the oldest for now
	int nInvites = m_RsInvites.GetCount();
	for(int i = 0; i < nInvites; i++)
	{
		if(m_RsInvites[i].m_InviteId == nInviteId)
			return i;
	}

	// not found
	return -1;
}

unsigned InviteMgr::GetNumRsInvites()
{
	return static_cast<unsigned>(m_RsInvites.GetCount());
}

bool InviteMgr::AcceptRsInvite(const unsigned inviteIdx)
{
    if(!gnetVerify(inviteIdx < GetNumRsInvites()))
    {
        gnetError("AcceptRsInvite :: Invalid Index of %d!", inviteIdx);
        return false;
    }

	sRsInvite& rsInvite = m_RsInvites[inviteIdx];
	if(!gnetVerify(rsInvite.IsValid()))
    {
        gnetError("AcceptRsInvite :: Invite from %s is invalid!", rsInvite.m_InviterName);
		return false;
    }

	// check the local gamer is signed in
	const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();
	if(!gnetVerify(pInfo))
    {
        gnetError("AcceptRsInvite :: Local gamer is signed out!");
        return false;
    }

	// check that the invitee is still the invited player
	if(*pInfo != rsInvite.m_Invitee)
	{
		gnetError("AcceptRsInvite :: The invited player is no longer the active user!");
		return false;
	}

	// check the local gamer is online
	if(!gnetVerify(NetworkInterface::IsLocalPlayerOnline()))
    {
        gnetError("AcceptRsInvite :: Local gamer is offline!");
        return false;
    }

	// logging
	gnetDebug1("AcceptRsInvite :: From %s. Token: 0x%016" I64FMT "x, InviteId: 0x%08x, Flags: 0x%x", rsInvite.m_InviterName, rsInvite.m_SessionInfo.GetToken().m_Value, rsInvite.m_InviteId, rsInvite.m_InviteFlags);

	// fake an invite accepted event
	AcceptInvite(
		rsInvite.m_SessionInfo,
		rsInvite.m_Invitee,
		rsInvite.m_Inviter,
		rsInvite.m_InviteId,
		rsInvite.m_InviteFlags,
		rsInvite.m_GameMode,
		rsInvite.m_UniqueMatchId,
		rsInvite.m_InviteFrom,
		rsInvite.m_InviteType);

	// remove invite
	RemoveRsInvite(inviteIdx, RemoveReason::RR_Accepted);

	// success
	return true; 
}

bool InviteMgr::RemoveRsInvite(const unsigned inviteIdx, const RemoveReason removeReason)
{
	if(!gnetVerify(inviteIdx < GetNumRsInvites()))
    {
        gnetError("RemoveRsInvite :: Invalid Index of %d!", inviteIdx);
		return false;
    }

	// logging
	gnetDebug1("RemoveRsInvite :: From %s. Token: 0x%016" I64FMT "x, InviteId: 0x%08x, Reason: %d", NetworkUtils::LogGamerHandle(m_RsInvites[inviteIdx].m_Inviter), m_RsInvites[inviteIdx].m_SessionInfo.GetToken().m_Value, m_RsInvites[inviteIdx].m_InviteId, static_cast<int>(removeReason));

	// blocked invites are never provided to script
	if(removeReason != RemoveReason::RR_Blocked)
	{
		CEventNetworkPresenceInviteRemoved tEvent(m_RsInvites[inviteIdx].m_InviteId, static_cast<int>(removeReason));
		GetEventScriptNetworkGroup()->Add(tEvent);
	}

	InviteAction inviteAction = InviteAction::IA_None;
	switch(removeReason)
	{
		// accepted invites are moved to the next step, no resolution needed now
	case RemoveReason::RR_Accepted:
		// blocked invites are resolved separately with specific flags
	case RemoveReason::RR_Blocked:
		// just assume this invite never happened
	case RemoveReason::RR_Cancelled:
		break;
		
		// both of these can be considered ignored
	case RemoveReason::RR_Oldest:
	case RemoveReason::RR_TimedOut:
		inviteAction = InviteAction::IA_Ignored;
		break;

	case RemoveReason::RR_Script:
		inviteAction = InviteAction::IA_Rejected;
		break;
	}

	if(inviteAction != InviteAction::IA_None)
	{
		const bool bReply = (sysTimer::GetSystemMsTime() - m_RsInvites[inviteIdx].m_TimeAdded) < m_RsInviteReplyTTL;
		ResolveInvite(m_RsInvites[inviteIdx], inviteAction, bReply ? ResolveFlags::RF_SendReply : ResolveFlags::RF_None, InviteResponseFlags::IRF_None);
	}
    
	m_RsInvites.Delete(inviteIdx);
	return true;
}

void InviteMgr::FlushRsInvites(const rlSessionInfo& hSessionInfo)
{
    gnetDebug1("FlushRsInvites :: Token: 0x%016" I64FMT "x", hSessionInfo.GetToken().m_Value);
    
    int nInvites = m_RsInvites.GetCount();
    for(int i = 0; i < nInvites; i++)
    {
        if(m_RsInvites[i].m_SessionInfo == hSessionInfo)
        {
            RemoveRsInvite(i, RemoveReason::RR_Cancelled);
            --i;
            --nInvites;
        }
    }
}

InviteId InviteMgr::GetRsInviteId(const unsigned inviteIdx)
{
    if(!gnetVerify(inviteIdx < GetNumRsInvites()))
    {
        gnetError("GetRsInviteId :: Invalid Index of %d!", inviteIdx);
        return INVALID_INVITE_ID;
    }

    return m_RsInvites[inviteIdx].m_InviteId;
}

const char* InviteMgr::GetRsInviteInviter(const unsigned inviteIdx)
{
    if(!gnetVerify(inviteIdx < GetNumRsInvites()))
    {
        gnetError("GetRsInviteInviter :: Invalid Index of %d!", inviteIdx);
        return "INVALID";
    }

	return m_RsInvites[inviteIdx].m_InviterName;
}

unsigned InviteMgr::GetRsInviteInviterCrewId(const unsigned inviteIdx)
{
	if(!gnetVerify(inviteIdx < GetNumRsInvites()))
	{
		gnetError("GetRsInviteInviterCrewId :: Invalid Index of %d!", inviteIdx);
		return 0;
	}

	return m_RsInvites[inviteIdx].m_CrewId;
}

rlGamerHandle* InviteMgr::GetRsInviteHandle(const unsigned inviteIdx, rlGamerHandle* pHandle)
{
	pHandle->Clear();
	if(!gnetVerify(inviteIdx < GetNumRsInvites()))
    {
        gnetError("GetRsInviteHandle :: Invalid Index of %d!", inviteIdx);
        return pHandle;
    }

	*pHandle = m_RsInvites[inviteIdx].m_Inviter;
	return pHandle;
}

rlSessionInfo* InviteMgr::GetRsInviteSessionInfo(const unsigned inviteIdx, rlSessionInfo* pInfo)
{
    pInfo->Clear();
    if(!gnetVerify(inviteIdx < GetNumRsInvites()))
    {
        gnetError("GetRsInviteSessionInfo :: Invalid Index of %d!", inviteIdx);
        return pInfo;
    }

    *pInfo = m_RsInvites[inviteIdx].m_SessionInfo;
    return pInfo;
}

const char* InviteMgr::GetRsInviteContentId(const unsigned inviteIdx)
{
	if(!gnetVerify(inviteIdx < GetNumRsInvites()))
    {
        gnetError("GetRsInviteContentId :: Invalid Index of %d!", inviteIdx);
        return NULL;
    }
    
	return m_RsInvites[inviteIdx].m_szContentId;
}

unsigned InviteMgr::GetRsInvitePlaylistLength(const unsigned inviteIdx)
{
	if(!gnetVerify(inviteIdx < GetNumRsInvites()))
    {
        gnetError("GetRsInvitePlaylistLength :: Invalid Index of %d!", inviteIdx);
        return 0;
    }

	return m_RsInvites[inviteIdx].m_nPlaylistLength;
}

unsigned InviteMgr::GetRsInvitePlaylistCurrent(const unsigned inviteIdx)
{
    if(!gnetVerify(inviteIdx < GetNumRsInvites()))
    {
        gnetError("GetRsInvitePlaylistCurrent :: Invalid Index of %d!", inviteIdx);
        return 0;
    }

	return m_RsInvites[inviteIdx].m_nPlaylistCurrent;
}

unsigned InviteMgr::GetRsInviteTournamentEventId(const unsigned inviteIdx)
{
    if(!gnetVerify(inviteIdx < GetNumRsInvites()))
    {
        gnetError("GetRsInviteTournamentEventId :: Invalid Index of %d!", inviteIdx);
        return 0;
    }

    return m_RsInvites[inviteIdx].m_TournamentEventId;
}

bool InviteMgr::GetRsInviteScTv(const unsigned inviteIdx)
{
    if(!gnetVerify(inviteIdx < GetNumRsInvites()))
    {
        gnetError("GetRsInviteScTv :: Invalid Index of %d!", inviteIdx);
        return false;
    }

	return m_RsInvites[inviteIdx].IsSCTV();
}

bool InviteMgr::GetRsInviteFromAdmin(const unsigned inviteIdx)
{
    if(!gnetVerify(inviteIdx < GetNumRsInvites()))
    {
        gnetError("GetRsInviteFromAdmin :: Invalid Index of %d!", inviteIdx);
        return false;
    }

    return m_RsInvites[inviteIdx].IsAdmin();
}

bool InviteMgr::GetRsInviteIsTournament(const unsigned inviteIdx)
{
    if(!gnetVerify(inviteIdx < GetNumRsInvites()))
    {
        gnetError("GetRsInviteIsTournament :: Invalid Index of %d!", inviteIdx);
        return false;
    }

    return m_RsInvites[inviteIdx].IsTournament();
}

void InviteMgr::UpdateRsInvites()
{
	// clear out timed out invites
	bool bFinished = false;
	while(!bFinished)
	{
		unsigned nCurrentTime = sysTimer::GetSystemMsTime();

		bFinished = true; 
		int nInvites = m_RsInvites.GetCount();
		for(int i = 0; i < nInvites; i++)
		{
			if((nCurrentTime - m_RsInvites[i].m_TimeAdded) > m_RsInviteTimeout)
			{
				gnetDebug1("UpdateRsInvites :: Invite Timed Out, InviteId: 0x%08x", m_RsInvites[i].m_InviteId);
				RemoveRsInvite(i, RemoveReason::RR_TimedOut);
				bFinished = false;
				break;
			}

#if RSG_XBOX
			if(!m_RsInvites[i].m_bAcknowledgedPermissionResult)
			{
				bool bPermissionChecksFinished = true; 
				bool bHasPermission = true; 

				if(m_RsInvites[i].m_ContentCheck.m_bCheckRequired)
				{
					if(!m_RsInvites[i].m_ContentCheck.m_Status.Pending())
					{
						if(!(m_RsInvites[i].m_ContentCheck.m_Status.Succeeded() && m_RsInvites[i].m_ContentCheck.m_Result.m_isAllowed))
							bHasPermission = false;
					}
					else
						bPermissionChecksFinished = false;
				}

				if(m_RsInvites[i].m_MultiplayerCheck.m_bCheckRequired)
				{
					if(!m_RsInvites[i].m_MultiplayerCheck.m_Status.Pending())
					{
						if(!(m_RsInvites[i].m_MultiplayerCheck.m_Status.Succeeded() && m_RsInvites[i].m_MultiplayerCheck.m_Result.m_isAllowed))
							bHasPermission = false;
					}
					else
						bPermissionChecksFinished = false;
				}

				// poll pending status
				if(bPermissionChecksFinished)
				{
#if !__NO_OUTPUT
					if(m_RsInvites[i].m_ContentCheck.m_bCheckRequired)
						gnetDebug1("UpdateRsInvites :: Content check %s. InviteId: 0x%08x, Allowed: %s", m_RsInvites[i].m_ContentCheck.m_Status.Succeeded() ? "Succeeded" : "Failed", m_RsInvites[i].m_InviteId, m_RsInvites[i].m_ContentCheck.m_Result.m_isAllowed ? "True" : "False");
					if(m_RsInvites[i].m_MultiplayerCheck.m_bCheckRequired)
						gnetDebug1("UpdateRsInvites :: Multiplayer check %s. InviteId: 0x%08x, Allowed: %s", m_RsInvites[i].m_MultiplayerCheck.m_Status.Succeeded() ? "Succeeded" : "Failed", m_RsInvites[i].m_InviteId, m_RsInvites[i].m_MultiplayerCheck.m_Result.m_isAllowed ? "True" : "False");
#endif
					m_RsInvites[i].m_bAcknowledgedPermissionResult = true;
					if(!bHasPermission)
					{
						// reply with BLOCKED
						ResolveInvite(m_RsInvites[i], InviteAction::IA_Blocked, ResolveFlags::RF_SendReply, InviteResponseFlags::IRF_RejectBlocked);

						// remove invite
						RemoveRsInvite(i, RemoveReason::RR_Blocked);
						bFinished = false;
						break;
					}
					else
					{
						// reset timeout 
						m_RsInvites[i].m_TimeAdded = sysTimer::GetSystemMsTime();

						// reply to invite, no decision made
						ResolveInvite(m_RsInvites[i], InviteAction::IA_None, ResolveFlags::RF_SendReply, InviteResponseFlags::IRF_None);

						// generate script event to add the invite
						CEventNetworkPresenceInvite tEvent(m_RsInvites[i].m_Inviter, m_RsInvites[i].m_InviterName, m_RsInvites[i].m_szContentId, m_RsInvites[i].m_nPlaylistLength, m_RsInvites[i].m_nPlaylistCurrent, m_RsInvites[i].IsSCTV(), m_RsInvites[i].m_InviteId);
						GetEventScriptNetworkGroup()->Add(tEvent);
					}
				}
			}
#endif
		}
	}
}

void InviteMgr::SetFollowInvite(const rlSessionInfo& hInfo, const rlGamerHandle& hFrom)
{
    gnetDebug1("SetFollowInvite :: Token: 0x%016" I64FMT "x, Handle: %s", hInfo.GetToken().m_Value, NetworkUtils::LogGamerHandle(hFrom));

    if(!hInfo.IsValid())
    {
        gnetError("SetFollowInvite :: Invalid Session Info!");
        return;
    }

    if(!hFrom.IsValid())
    {
        gnetError("SetFollowInvite :: Invalid Inviter Gamer Handle!");
        return;
    }

	if(CNetwork::GetNetworkSession().IsInSession(hInfo))
	{
		gnetError("SetFollowInvite :: Already in this session!");
		return;
	}

	// capture info
    m_FollowInviteSessionInfo = hInfo;
    m_FollowInviteGamerHandle = hFrom;

    // generate script event
    GetEventScriptNetworkGroup()->Add(CEventNetworkFollowInviteReceived(m_FollowInviteGamerHandle));
}

bool InviteMgr::HasValidFollowInvite()
{
    return m_FollowInviteSessionInfo.IsValid();
}

const rlSessionInfo& InviteMgr::GetFollowInviteSessionInfo()
{
    return m_FollowInviteSessionInfo;
}

const rlGamerHandle& InviteMgr::GetFollowInviteHandle()
{
    return m_FollowInviteGamerHandle;
}

bool InviteMgr::ActionFollowInvite()
{
    gnetDebug1("ActionFollowInvite");

    if(!CLiveManager::IsOnline())
    {
        gnetError("ActionFollowInvite :: Not Online!");
        return false;
    }

    if(!HasValidFollowInvite())
    {
        gnetError("ActionFollowInvite :: Invite Invalid!");
        return false;
    }

    // support a private join
    unsigned nInviteFlags = InviteFlags::IF_IsFollow | InviteFlags::IF_AllowPrivate;

    // these situations mean we're joining as SCTV
    if(CNetwork::GetNetworkSession().IsTransitionActive() && CNetwork::GetNetworkSession().IsActivitySpectator())
        nInviteFlags |= InviteFlags::IF_Spectator;
    else if(CNetwork::GetNetworkSession().IsTransitionActive() && CNetwork::GetNetworkSession().GetMatchmakingGroup() == MM_GROUP_SCTV)
        nInviteFlags |= InviteFlags::IF_Spectator;

    // fake an invite accepted event
    HandleJoinRequest(m_FollowInviteSessionInfo, *NetworkInterface::GetActiveGamerInfo(), m_FollowInviteGamerHandle, nInviteFlags);

    // handled - clear and indicate success
    ClearFollowInvite();
    return true; 
}

bool InviteMgr::ClearFollowInvite()
{
    gnetDebug1("ClearFollowInvite");
    bool bWasValid = HasValidFollowInvite();
    m_FollowInviteSessionInfo.Clear();
    m_FollowInviteGamerHandle.Clear();
    return bWasValid;
}

bool InviteMgr::RequestConfirmEvent()
{
    gnetDebug1("RequestConfirmEvent");

    // verify that we have an invite
    sAcceptedInvite* pInvite = GetAcceptedInvite();
    if(!gnetVerify(pInvite->IsPending()))
    {
        gnetError("RequestConfirmEvent :: Invalid invite!");
        return false;
    }

	// verify that we have confirmed
	if(!gnetVerify(HasConfirmedAcceptedInvite()))
	{
		gnetError("RequestConfirmEvent :: Invite not confirmed!");
		return false;
	}

    // verify that we have details (if not a boot invite)
    if(!pInvite->HasValidConfig())
    {
		// check if this was a boot invite, if so, we may not have the config yet
		if(pInvite->HasInternalFlag(InternalFlags::LIF_FromBoot))
		{
			gnetDebug1("RequestConfirmEvent :: Config not available for boot invite yet!");
			return false;
		}

		// check if the config is pending / blocked
		if(m_WasBlockedLastCheck)
		{
			gnetDebug1("RequestConfirmEvent :: Invite processing currently blocked!");
			return false;
		}

		gnetDebug1("RequestConfirmEvent :: Invite config not available yet!");
        return false;
    }

	if(pInvite->IsIncompatible())
	{
		gnetDebug1("RequestConfirmEvent :: Invite is Incompatible!");
		return false;
	}

    // if we get here, script are processing a previous confirm - clear the store flag
    m_bStoreInviteThroughRestart = false;

	// if script are informed of an invite this way, update the flag
	pInvite->AddInternalFlag(InternalFlags::LIF_KnownToScript);

    // fire a script event
    CEventNetworkInviteConfirmed inviteEvent(pInvite->GetInviter(),
                                             pInvite->m_nGameMode, 
                                             pInvite->m_SessionPurpose, 
                                             pInvite->IsViaParty(), 
                                             pInvite->m_nAimType, 
                                             pInvite->m_nActivityType, 
                                             pInvite->m_nActivityID,
                                             pInvite->IsSCTV(),
                                             pInvite->GetFlags(),
											 pInvite->m_nNumBosses);

    GetEventScriptNetworkGroup()->Add(inviteEvent);

    return true;
}

void InviteMgr::AutoConfirmInvite()
{ 
	if(HasPendingAcceptedInvite() && !m_AcceptedInvite.HasInternalFlag(InternalFlags::LIF_AutoConfirm))
	{
		gnetDebug1("AutoConfirmInvite");
		m_AcceptedInvite.AddInternalFlag(InternalFlags::LIF_AutoConfirm);
	}
}

void InviteMgr::SuppressProcess(bool bSuppress)
{
    if(bSuppress != m_bSuppressProcess)
    {
        gnetDebug1("SuppressProcess :: %s Invite Processing", bSuppress ? "Disabling" : "Enabling");
	    m_bSuppressProcess = bSuppress;

		// now able to process, check if we should cancel
		if(!m_bSuppressProcess)
			CheckInviteIsValid();
    }
}

void InviteMgr::BlockInvitesFromScript(bool bBlocked)
{
    if(bBlocked != m_bBlockedByScript)
    {
        gnetDebug1("BlockInvitesFromScript :: %s from script", bBlocked ? "Block" : "Do not block");
        m_bBlockedByScript = bBlocked;

		// now able to process, check if we should cancel
		if(!m_bBlockedByScript)
			CheckInviteIsValid();
    }
}

void InviteMgr::StoreInviteThroughRestart()
{
    if(!m_bStoreInviteThroughRestart)
    {
        gnetDebug1("StoreInviteThroughRestart :: Setting flag");
        m_bStoreInviteThroughRestart = true;
        m_bWaitingForScriptDeath = true;
    }
}

void InviteMgr::AllowProcessInPlayerSwitch(bool bAllow)
{
    if(m_bAllowProcessInPlayerSwitch != bAllow)
    {
        gnetDebug1("AllowProcessInPlayerSwitch :: %s", bAllow ? "Allow" : "Do not allow");
        m_bAllowProcessInPlayerSwitch = bAllow;
    }
}

void InviteMgr::BlockJoinQueueInvites(bool bBlocked)
{
    if(m_bBlockJoinQueueInvite != bBlocked)
    {
        gnetDebug1("BlockJoinQueueInvites :: %s", bBlocked ? "Blocked" : "Not Blocked");
        m_bBlockJoinQueueInvite = bBlocked;
    }
}

void InviteMgr::SetCanReceiveRsInvites(const bool canReceive)
{
	if(m_CanReceiveRsInvites != canReceive)
	{
		gnetDebug1("SetCanReceiveRsInvites :: %s -> %s", m_CanReceiveRsInvites ? "True" : "False", canReceive ? "True" : "False");
        m_CanReceiveRsInvites = canReceive;
    }
}

bool InviteMgr::IsAwaitingInviteResponse() const
{ 
	if(!m_AcceptedInvite.IsPending())
        return false;

    // we don't want to indicate this if this is for a different user
    // or if it's already been confirmed
    return !IsAcceptedInviteForDifferentUser() && !HasConfirmedAcceptedInvite();
}

bool InviteMgr::HasPendingPlatformNativeInvite() const
{
	// returns whether we have an in-flight boot invite
#if RSG_SCE
	return g_rlNp.GetBasic().IsInviteBeingProcessed();
#elif RSG_XBOX
	return g_rlXbl.HasPendingInvite();
#else
	return false;
#endif
}

bool InviteMgr::HasBootInviteToProcess(OUTPUT_ONLY(bool logresult))
{
    #define LOG_INVITE_CHECK(txt) OUTPUT_ONLY( if (logresult) { gnetDebug3(txt); } )

	bool hasBootInviteToProcess = false;

    if(gs_HasBooted)
    {
        LOG_INVITE_CHECK("HasBootInviteToProcess :: Booted");
    }
	else if(HasPendingPlatformNativeInvite())
	{
		gnetDebug1("HasBootInviteToProcess :: Has Pending Boot Invite");
		hasBootInviteToProcess = true;
	}
	else if(HasPendingPlatformInvite())
	{
		gnetDebug1("HasBootInviteToProcess :: Has Pending Platform Invite");
		hasBootInviteToProcess = true;
	}
	else if(m_AcceptedInvite.IsPending())
    {
		LOG_INVITE_CHECK("HasBootInviteToProcess :: Has Pending Accepted Invite");
		hasBootInviteToProcess = true;
    }

	// track whether we recorded this or not
	if(!gs_HasBooted && !m_HasSkipBootUiResult)
	{
		m_HasSkipBootUiResult = true;
		m_DidSkipBootUi = hasBootInviteToProcess;
		m_HasClaimedBootInvite = false;
	}

	if(!hasBootInviteToProcess)
	{
		LOG_INVITE_CHECK("HasBootInviteToProcess :: No, no invite");
	}

    return hasBootInviteToProcess;
}

bool InviteMgr::DidSkipBootUi() const
{
	return m_HasSkipBootUiResult &&
		m_DidSkipBootUi;
	//&& CGameSessionStateMachine::DidSkipBootLandingPage();
}

bool InviteMgr::SignInInviteeForBoot()
{
#if !RSG_XBOX
	if(CLiveManager::IsSignedIn())
	{
		gnetDebug1("SignInInviteeForBoot :: Player already signed in");
		return true;
	}

	// There's only one active player on these platforms so we're good to pick the first
	if(!gs_HasBooted && CControlMgr::GetMainPlayerIndex() == -1)
	{
		for(int i = 0; i < RL_MAX_LOCAL_GAMERS; ++i)
		{
			if(rlPresence::IsSignedIn(i))
			{
				gnetDebug1("SignInInviteeForBoot :: Signing in default player to %d", i);
				CControlMgr::SetMainPlayerIndex(i);
				return true;
			}
		}
	}
#endif

	return false;
}

sAcceptedInvite* InviteMgr::GetAcceptedInvite()
{
	return &m_AcceptedInvite;
}

void InviteMgr::SetShowingConfirmationExternally()	
{ 
	if(!m_ShowingConfirmationExternally)
	{
		gnetDebug1("SetShowingConfirmationExternally");
		m_ShowingConfirmationExternally = true;
	}
}

unsigned InviteMgr::GetNumUnacceptedInvites()
{
	return static_cast<unsigned>(m_UnacceptedInvites.GetCount());
}

int InviteMgr::GetUnacceptedInviteIndex(const rlGamerHandle& inviter)
{
	for(int i = 0; i < m_UnacceptedInvites.GetCount(); ++i)
	{
		if(m_UnacceptedInvites[i].m_Inviter == inviter)
		{
			return i;
		}
	}

	return -1;
}

const UnacceptedInvite* InviteMgr::GetUnacceptedInvite(const int idx)
{
	return gnetVerify(idx < GetNumUnacceptedInvites()) ? &m_UnacceptedInvites[idx] : NULL;
}

bool InviteMgr::AcceptInvite(const int idx)
{
	const UnacceptedInvite* pInvite = GetUnacceptedInvite(idx);
	if(!gnetVerify(pInvite))
		return false;

	if(!gnetVerify(pInvite->IsValid()))
		return false;

	const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();
	if(!gnetVerify(pInfo))
		return false;

	gnetDebug1("AcceptInvite :: Accepted invitation from \"%s\"", pInvite->m_InviterName);

	// initialise our platform invite
	if(gnetVerify(pInvite->IsValid()))
		AddPlatformInvite(*pInfo, pInvite->m_Inviter, pInvite->m_SessionInfo, InviteFlags::IF_IsPlatform | InviteFlags::IF_AllowPrivate);

	return true;
}

void InviteMgr::RemoveMatchingUnacceptedInvites(const rlSessionInfo& sessionInfo, const rlGamerHandle& inviter)
{
	if(!sessionInfo.IsValid())
		return;

	if(!inviter.IsValid())
		return;

	// iterate in reverse over the array to safely remove from it.
	int nUnacceptedInvites = m_UnacceptedInvites.GetCount();
	for(int i = nUnacceptedInvites - 1; i >= 0; i--)
	{
		// if the invite at the current index matches session info and inviter, remove
		if(sessionInfo == m_UnacceptedInvites[i].m_SessionInfo || inviter == m_UnacceptedInvites[i].m_Inviter)
		{
			gnetDebug1("RemoveMatchingUnacceptedInvites :: Removing %d (Session and Inviter), From: %s [%s], Session: 0x%016" I64FMT "x",
				i,
				NetworkUtils::LogGamerHandle(m_UnacceptedInvites[i].m_Inviter),
				m_UnacceptedInvites[i].m_InviterName,
				m_UnacceptedInvites[i].m_SessionInfo.GetToken().m_Value);

			RemoveUnacceptedInvite(i);
		}
	}
}

void InviteMgr::RemoveMatchingUnacceptedInvites(const rlSessionInfo& sessionInfo)
{
	if(!sessionInfo.IsValid())
		return;

	// iterate in reverse over the array to safely remove from it.
	int nUnacceptedInvites = m_UnacceptedInvites.GetCount();
	for(int i = nUnacceptedInvites - 1; i >= 0; i--)
	{
		// if the invite at the current index matches session info and inviter, remove
		if(sessionInfo == m_UnacceptedInvites[i].m_SessionInfo)
		{
			gnetDebug1("RemoveMatchingUnacceptedInvites :: Removing %d (Session), From: %s [%s], Session: 0x%016" I64FMT "x",
				i,
				NetworkUtils::LogGamerHandle(m_UnacceptedInvites[i].m_Inviter),
				m_UnacceptedInvites[i].m_InviterName,
				m_UnacceptedInvites[i].m_SessionInfo.GetToken().m_Value);

			RemoveUnacceptedInvite(i);
		}
	}
}

void InviteMgr::RemoveMatchingUnacceptedInvites(const rlGamerHandle& inviter)
{
	if(!inviter.IsValid())
		return;

	// iterate in reverse over the array to safely remove from it.
	int nUnacceptedInvites = m_UnacceptedInvites.GetCount();
	for(int i = nUnacceptedInvites - 1; i >= 0; i--)
	{
		// if the invite at the current index matches session info and inviter, remove
		if(inviter == m_UnacceptedInvites[i].m_Inviter)
		{
			gnetDebug1("RemoveMatchingUnacceptedInvites :: Removing %d (Inviter), From: %s [%s], Session: 0x%016" I64FMT "x",
				i,
				NetworkUtils::LogGamerHandle(m_UnacceptedInvites[i].m_Inviter),
				m_UnacceptedInvites[i].m_InviterName,
				m_UnacceptedInvites[i].m_SessionInfo.GetToken().m_Value); 
			
			RemoveUnacceptedInvite(i);
		}
	}
}

void InviteMgr::AddUnacceptedInvite(
	const rlSessionInfo& sessionInfo,
	const rlGamerHandle& inviter,
	const char* inviterName,
	const char* salutation)
{
	gnetDebug1("AddUnacceptedInvite :: Session: 0x%016" I64FMT "x, Inviter: %s [%s]", sessionInfo.GetToken().m_Value, inviterName, NetworkUtils::LogGamerHandle(inviter));

	if(!gnetVerify(inviterName))
		return;

	if(!gnetVerify(sessionInfo.IsValid()))
		return;

	// check for duplicates.  If a duplicate is found, update the invitation
	int nUnacceptedInvites = m_UnacceptedInvites.GetCount();
	bool bInviteExists = false;
	for(int i = 0; i < nUnacceptedInvites; ++i)
	{
		if(inviter == m_UnacceptedInvites[i].m_Inviter)
		{
			gnetDebug1("AddUnacceptedInvite :: Found duplicate invite from %s. Refreshing Invite", inviterName);
			m_UnacceptedInvites[i].Reset(sessionInfo, inviter, inviterName, salutation);
			bInviteExists = true; 
			break;
		}
	}

	if(!bInviteExists)
	{
		UnacceptedInvite sInvite;
		sInvite.Init(sessionInfo, inviter, inviterName, salutation);

		// add the invite 
		m_UnacceptedInvites.PushAndGrow(sInvite);
		SortUnacceptedInvites();

		gnetDebug1("AddUnacceptedInvite :: Added invite with Id: %u. Count: %d", sInvite.m_Id, m_UnacceptedInvites.GetCount());

#if RSG_ORBIS
		// on PS4, if we have a matching unaccepted platform invite, we are required to allow joins into private sessions
		const unsigned privateFlagTolerance = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("INVITE_NP_PRIVATE_INVITE_JVP_TOLERANCE", 0x1e168a49), 0);
		if(HasPendingPlatformInvite() &&
			(m_PendingPlatformInvite.m_Inviter == inviter) &&
			((m_PendingPlatformInvite.m_Flags & InviteFlags::IF_IsJoin) != 0) &&
			((m_PendingPlatformInvite.m_Flags & InviteFlags::IF_AllowPrivate) == 0) &&
			((privateFlagTolerance == 0) || ((sysTimer::GetSystemMsTime() - m_PendingPlatformInvite.m_TimeAccepted) <= privateFlagTolerance)))
		{
			gnetDebug1("AddUnacceptedInvite :: Adding unaccepted invite to %s. Enabling private flag", inviter.ToString());
			m_PendingPlatformInvite.m_Flags |= InviteFlags::IF_AllowPrivate;
		}
#endif
	}
}

void InviteMgr::RemoveUnacceptedInvite(const unsigned idx)
{
	if(gnetVerify(idx < m_UnacceptedInvites.GetCount()))
	{
		OUTPUT_ONLY(unsigned inviteId = m_UnacceptedInvites[idx].m_Id);
		m_UnacceptedInvites.Delete(idx);
		gnetDebug1("RemoveUnacceptedInvite :: Removing invite with ID: %u, Count: %d", inviteId, m_UnacceptedInvites.GetCount());
	}
}

void InviteMgr::UpdateUnacceptedInvites(const int timeStep)
{
	// clear out timed out invites
	bool bFinished = false;
	while(!bFinished)
	{
		bFinished = true; 
		int nUnacceptedInvites = m_UnacceptedInvites.GetCount();
		for(int i = 0; i < nUnacceptedInvites; i++)
		{
			UnacceptedInvite* pInvite = &m_UnacceptedInvites[i];
			pInvite->Update(timeStep);

			if(pInvite->TimedOut())
			{
				gnetDebug1("UpdateUnacceptedInvites :: Removing timed out invite with ID: %u", m_UnacceptedInvites[i].m_Id);
				RemoveUnacceptedInvite(i);
				bFinished = false;
				break;
			}
		}
	}
}

// used to sort invitations in alphabetical order.
int CompareInvites(const UnacceptedInvite* uiA, const UnacceptedInvite* uiB)
{
	// case insensitive sort
	return stricmp(uiA->m_InviterName, uiB->m_InviterName);
}

void InviteMgr::SortUnacceptedInvites()
{
	int nUnacceptedInvites = m_UnacceptedInvites.GetCount();
	if(nUnacceptedInvites > 1)
		m_UnacceptedInvites.QSort(0, nUnacceptedInvites, CompareInvites);
}

int InviteMgr::GetMostRecentUnacceptedInviteIndex(const rlSessionInfo& sessionInfo)
{
	if(!sessionInfo.IsValid())
		return -1;

	// iterate in reverse over the array to safely remove from it.
	int nUnacceptedInvites = m_UnacceptedInvites.GetCount();
	for(int i = nUnacceptedInvites - 1; i >= 0; i--)
	{
		// if the invite at the current index matches session info and inviter, remove
		if(sessionInfo == m_UnacceptedInvites[i].m_SessionInfo)
			return i;
	}

	return -1;
}

// eof
