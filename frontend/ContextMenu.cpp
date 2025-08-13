//rage
#include "rline/rlsessioninfo.h"
#include "rline/rlsystemui.h"

// game
#include "Frontend/ContextMenu.h"
#include "Frontend/CFriendsMenu.h"
#include "Frontend/ReportMenu.h"
#include "Frontend/PauseMenu.h"
#include "Frontend/ui_channel.h"
#include "Network/Cloud/Tunables.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Network/Live/livemanager.h"
#include "Network/Live/NetworkClan.h"
#include "Network/NetworkInterface.h"
#include "Network/Sessions/NetworkSession.h"
#include "Network/Voice/NetworkVoice.h"
#include "Stats/StatsInterface.h"
#include "Text/TextConversion.h"
#include "Text/TextFile.h"

#if __XENON
#include <xparty.h>
#endif

FRONTEND_OPTIMISATIONS()
//OPTIMISATIONS_OFF()

//Debug flow control
BankBool ClanInviteManager::sDBGAlwaysShow = false;
BankBool ClanInviteManager::sDelayIdle = false;
BankUInt32 ClanInviteManager::sThrottleDuration = 10 * 1000; //10 seconds
BankFloat ClanInviteManager::sBlendInTimeInSeconds = 0.5f;
BankFloat ClanInviteManager::sBlendOutTimeInSeconds = 2.0f;
BankFloat ClanInviteManager::sTimeToDisplayMsgInSeconds = ClanInviteManager::sBlendInTimeInSeconds + ClanInviteManager::sBlendOutTimeInSeconds + 5.0f;

atRangeArray<bool, MAX_NUM_PHYSICAL_PLAYERS> CContextMenuHelper::sm_kickVotes;
u32 CContextMenuHelper::m_uLastReport = 0;
u32 CContextMenuHelper::m_uLastCommend = 0;
u32 CContextMenuHelper::m_uLastKick = 0;
bool CContextMenuHelper::m_bInTournament = false;

#if __BANK

int CContextMenuOption::sm_Count = 0;
int CContextMenuOption::sm_Allocs =0;
int CContextMenu::sm_Count = 0;
int CContextMenu::sm_Allocs =0;

bool CContextMenuHelper::m_bNonStopReports = false;
#endif // __BANK


#define LOCAL_PLAYER_CONTEXT "IsLocalPlayer"
#define DEFAULT_MAX_REPORT_COMMEND_STRENGTH 16

//////////////////////////////////////////////////////////////////////////////////////////////////////////////


ClanInviteManager::ClanInviteManager()
{
	currInviteCachedClan = RL_INVALID_CLAN_ID;
	lastSuccess = true;
	lastUpdateElapsedTime = sTimeToDisplayMsgInSeconds;
}

void ClanInviteManager::Update()
{
	lastUpdateElapsedTime += TIME.GetSeconds();

	//Update cache, removing any expired entries
	UpdateInviteCache();

	if(!IsIdle() && !m_netStatus.Pending())
	{
		lastSuccess = m_netStatus.Succeeded();
		if(m_netStatus.Failed())
		{
			//Cache failure code... in case anyone cares.
			lastResultCode = m_netStatus.GetResultCode();

			switch(lastResultCode)
			{
			case RL_CLAN_SUCCESS:
			case RL_CLAN_ERR_INVITE_EXISTS:
				//RL_CLAN_ERR_ALREADY_IN_CLAN - Questionable... might wanna report success here as well.
				lastSuccess = true;	//Not all failures are actually failures.
				break;
			}

			//Show detailed warning
			//MS_WarningScreen *pWarning = g_GUIInterface.GetMPController()->GetWarning();
			//if(AssertVerify(pWarning) && !lastSuccess && g_GUIInterface.GetMPController() && g_GUIInterface.GetMPController()->getCurrentState())
			//{
			//	pWarning->setCustomWarning("WARN_CREW_INVITE_FAIL", CNetwork::GetNetworkClan().GetErrorName(netStatus), MS_WarningScreen::eBUTTON_A_OK);
			//	g_GUIInterface.GetMPController()->getCurrentState()->setFSMInput(uCrcWarning);
			//}
		}
		else if(m_netStatus.Succeeded())
		{
			AddGamerToInviteCache(lastGamer);
		}

		ResetTimer();
		//Need to reset here so completed ops can spawn more ops ... 
		m_netStatus.Reset();
	}
}

void ClanInviteManager::UpdateInviteCache()
{
	for(int i = 0; i < inviteCache.GetCount(); ++i)
	{
		if(!inviteCache[i].IsThrottled(sThrottleDuration))
		{
			inviteCache.Delete(i--);	//Remove from cache, as this entry is no longer throttled.
		}
	}
}

bool ClanInviteManager::IsInviteeThrottled(const rlGamerHandle &gamer)
{
	bool throttled = false;
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	
	if (RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex) && rlClan::GetPrimaryClan(localGamerIndex).IsValid())
	{
		const rlClanId clanId = rlClan::GetPrimaryClan(localGamerIndex).m_Id;
		if(clanId != currInviteCachedClan)
		{
			//Primary clan changed. That invalidates the cache, so reset it and start over.
			inviteCache.Reset();
			currInviteCachedClan = clanId;
		}
		else
		{
			int cachedIndex = 0;
			if(inviteCache.Find(gamer, &cachedIndex))
			{
				throttled = inviteCache[cachedIndex].IsThrottled(sThrottleDuration);
			}
		}
	}

	return throttled;
}

void ClanInviteManager::AddGamerToInviteCache(const rlGamerHandle &gamer)
{
	int cachedIndex = 0;
	if(inviteCache.Find(gamer, &cachedIndex))
	{
		inviteCache[cachedIndex].UpdateLastTimeMS();
	}
	else
	{
		if(inviteCache.IsFull())
			inviteCache.Pop();

		inviteCache.Push(gamer);
	}
}

bool ClanInviteManager::IsIdle()
{
	if(sDelayIdle)
		return false;

	return (netStatus::NET_STATUS_NONE == m_netStatus.GetStatus());
}

bool ClanInviteManager::ShouldDisplayMessage()
{
	if(sDBGAlwaysShow)
		return true;

	return lastUpdateElapsedTime < sTimeToDisplayMsgInSeconds;
}

void ClanInviteManager::InviteGamer(const rlGamerHandle &gamer)
{
	if(m_netStatus.Pending())
	{
		//A previous request is still pending. Ignore new request... we should handle this in UI to prevent this from happening.
		Assertf(false, "Waiting for last request to process.");
	}
	else
	{
		if(IsInviteeThrottled(gamer))
		{
			//User has already sent this gamer an invite. Silently ignore the request. (Tell the user it succeeded, because the last one already did.)
			lastGamer = gamer;
			m_netStatus.ForceFailed(RL_CLAN_ERR_UNKNOWN);
			return;
		}

		if(!CLiveManager::GetNetworkClan().CanSendInvite(gamer))
		{
			//Blocked from sending an invite to this player
			lastGamer = gamer;
			m_netStatus.ForceFailed(RL_CLAN_ERR_UNKNOWN);
			return;
		}

		lastGamer = gamer;
		m_netStatus.Reset();

		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		if (RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
		{
			rlClanDesc primClan = rlClan::GetPrimaryClan(localGamerIndex);
			if (primClan.IsValid())
			{
				if(!rlClan::Invite(localGamerIndex, primClan.m_Id, -1, true, gamer, &m_netStatus))
				{
					lastSuccess = false;

					//Show detailed warning
					//MS_WarningScreen *pWarning = g_GUIInterface.GetMPController()->GetWarning();
					//if(AssertVerify(pWarning) && !lastSuccess && g_GUIInterface.GetMPController() && g_GUIInterface.GetMPController()->getCurrentState())
					//{
					//	pWarning->setCustomWarning("WARN_CREW_INVITE_FAIL", "RL_CLAN_ERR_SEND_REQUEST_FAILED", MS_WarningScreen::eBUTTON_A_OK);
					//	g_GUIInterface.GetMPController()->getCurrentState()->setFSMInput(uCrcWarning);
					//}

					//If operation queue request failed, then we know now that we failed.
					ResetTimer();
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

const char* CContextMenuOption::GetString() const
{
	return TheText.Get(text.GetHash(),
#if __BANK
		text.GetCStr()
#else
		NULL
#endif
		);
}

const char* CContextMenuOption::GetSubstring() const
{
	// bit of a kludge, but why not!
	if( contextOption == ATSTRINGHASH("SEND_CREW_INVITE",0xd6110a7b) )
	{
		if( CLiveManager::GetNetworkClan().HasPrimaryClan() )
		{
			static char InviteKludge[NetworkClan::FORMATTED_CLAN_TAG_LEN+4];
			return NetworkClan::GetUIFormattedClanTag( *(CLiveManager::GetNetworkClan().GetPrimaryClan()), -1, InviteKludge, COUNTOF(InviteKludge));
		}
	}

	return NULL;
}

const int CContextMenuOption::GetType() const
{
	if( contextOption == ATSTRINGHASH("SEND_CREW_INVITE",0xd6110a7b) )
		return 2;
	return 0;
}

void CContextMenuOption::preLoad(parTreeNode* pTreeNode)
{
	parTreeNode* pMenus = pTreeNode->FindChildWithName("displayCondition");
	if( pMenus )
	{
		displayConditions.ParseFromString(pMenus->GetData());
	}
}

bool CContextMenuOption::CanShow() const
{
	return displayConditions.CheckIfPasses();
}

void CContextMenuOption::AddSlot(int depth, int menuId, int index) const
{
	CScaleformMovieWrapper& movie = CContextMenu::GetMovie();
	if( movie.BeginMethod("SET_CONTEXT_SLOT") )
	{
		movie.AddParam(depth);  // The View the slot is to be added to
		movie.AddParam(index);  // The numbered slot the information is to be added to
		movie.AddParam(menuId + PREF_OPTIONS_THRESHOLD);  // The menu ID
		movie.AddParam( static_cast<int>(contextOption.GetHash()));  // The unique ID
		movie.AddParam(GetType());  // The Menu item type (see below)
		movie.AddParam(0);  // The initial index of the slot (0 default, can be 0,1,2...x in a multiple selection)
		movie.AddParam(GetIsSelectable());  // active or inactive
		movie.AddParam(GetString());  // The text label
		movie.AddParam(GetSubstring()); // Tag label
		movie.EndMethod();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

void CContextMenu::postLoad()
{
	uiAssertf(m_TriggerMenuId != MENU_UNIQUE_ID_INVALID, "A ContextMenu's TriggerMenuId needs to be set up!" );
	uiAssertf(m_ContextMenuId != MENU_UNIQUE_ID_INVALID, "A ContextMenu's ContextMenuId needs to be set up!" );
	uiAssertf(m_depth != MENU_DEPTH_HEADER, "Context Menu has a depth of 0/MENU_DEPTH_HEADER! That simply won't work! (Trigger ID: %s, ContextMenu ID of %s)", m_TriggerMenuId.GetParserName(), m_ContextMenuId.GetParserName() );
	
	// adjust for data/AS offset just right here.
	m_depth = static_cast<eDepth>(m_depth-1);
}

bool CContextMenu::WouldShowMenu() const
{
	for(int i=0; i<contextOptions.GetCount(); ++i)
	{
		const CContextMenuOption* pOption = CPauseMenu::GetMenuArray().GetContextMenuOption(contextOptions[i]);
		if(pOption && pOption->CanShow())
			return true;
	}

	return false;
}

bool CContextMenu::ShowMenu()
{
	// build a list of options to show as we go
	// so we don't have to evaluate CanShow twice
	atArray<const CContextMenuOption*> listToShow;

	for(int i=0; i<contextOptions.GetCount(); ++i)
	{
		const CContextMenuOption* pOption = CPauseMenu::GetMenuArray().GetContextMenuOption(contextOptions[i]);
		if(pOption && pOption->CanShow())
		{
			listToShow.PushAndGrow(pOption);
		}
	}

	if( !listToShow.empty() )
	{
		SET_MENU();

		for(int index=0; index < listToShow.GetCount(); ++index)
			listToShow[index]->AddSlot( m_depth, m_ContextMenuId.GetValue(), index);
		
		m_bIsOpen = true;
		DISPLAY_MENU();

		CScaleformMenuHelper::SET_WARNING_MESSAGE_VISIBILITY(false);

		return true;
	}

	return false;
}

void CContextMenu::CloseMenu()
{
	if( m_bIsOpen )
		SUIContexts::Activate(ATSTRINGHASH("HasContextMenu",0x52536cbf));

	m_bIsOpen = false;
	CLEAR_MENU();


	
	if(CPauseMenu::GetCurrentScreenData().HasDynamicMenu() && CPauseMenu::GetCurrentScreenData().GetDynamicMenu()->IsShowingWarningColumn())
	{
		CScaleformMenuHelper::SET_WARNING_MESSAGE_VISIBILITY(true);
	}
}

bool CContextMenu::IsOpen() const
{
	return m_bIsOpen;
}

void CContextMenu::ShowOrHideColumnCB(bool bShow)
{
	CScaleformMenuHelper::SHOW_COLUMN( static_cast<PM_COLUMNS>(m_depth), bShow);
}

CScaleformMovieWrapper& CContextMenu::GetMovie()
{
	return CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);
}

void CContextMenu::SET_MENU()
{
	GetMovie().CallMethod("SHOW_CONTEXT_MENU", true, m_depth);
}

void CContextMenu::DISPLAY_MENU()
{
	if( m_MenuCB.Get() )
		m_MenuCB->HandleContextMenuVisibility(false);
	else
		ShowOrHideColumnCB(false);

	GetMovie().CallMethod("DISPLAY_CONTEXT_SLOT", true, m_depth);
}

void CContextMenu::CLEAR_MENU()
{
	if( m_MenuCB.Get() )
		m_MenuCB->HandleContextMenuVisibility(true);
	else
		ShowOrHideColumnCB(true);

	GetMovie().CallMethod("SHOW_CONTEXT_MENU", false);
}

//////////////////////////////////////////////////////////////////////////

void CContextMenuHelper::InitVotes()
{
	for(PhysicalPlayerIndex i=0; i<sm_kickVotes.size(); ++i)
	{
		sm_kickVotes[i] = false;
	}
}

bool CContextMenuHelper::IsLocalPlayer(const rlGamerHandle& gamerHandle)
{
	const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();
	return pInfo && (pInfo->GetGamerHandle() == gamerHandle);
}

void CContextMenuHelper::BuildPlayerContexts(const rlGamerHandle& gamerHandle, bool bSuppressCrewInvites /* = false */, const rlClanMembershipData* pMembership /* = NULL*/)
{

#if RSG_DEV
	bool bDebugPrintContexts = false;
#endif

#if RSG_NP
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!g_rlNp.GetChatRestrictionFlag(localGamerIndex))
#endif
	{
		SUIContexts::Activate("CanMute");
	}

	SUIContexts::SetActive( LOCAL_PLAYER_CONTEXT, IsLocalPlayer(gamerHandle) );

	bool enableCrewInvite = CLiveManager::GetNetworkClan().CanSendInvite(gamerHandle);
#if RSG_SCE || RSG_PS3
	// PlayStation R4063 | R5063 - use the local gamer index
	enableCrewInvite &= CLiveManager::CheckUserContentPrivileges(GAMER_INDEX_LOCAL);
#endif

	if(!enableCrewInvite)
		SUIContexts::Activate("DisableCrewInvite");
	else
		SUIContexts::Deactivate("DisableCrewInvite");

#if __XENON
    // On 360, the app and Xbox mute are tied together. We always want the mute option. 
	SUIContexts::SetActive("MutedByOS", false);
#else	
	const VoiceGamerSettings* pVoiceSettings = NetworkInterface::GetVoice().FindGamer(gamerHandle);
	SUIContexts::SetActive("MutedByOS", pVoiceSettings && (pVoiceSettings->GetLocalVoiceFlags() & VoiceGamerSettings::VOICE_LOCAL_MUTED) == VoiceGamerSettings::VOICE_LOCAL_MUTED);
#endif

	const rlFriend* pFriend = CLiveManager::GetFriendsPage()->GetFriend(gamerHandle);
	if( pFriend)
	{
		SUIContexts::Activate("IsFriend");

#if RSG_DEV
		// TRACKING: url:bugstar:2547545 - Player was not shown "Join Game" option in Friends menu
		uiDebugf1("Printing active contexts for %s", pFriend->GetName());
		bDebugPrintContexts = true;
#endif

		if (!pFriend->IsPendingFriend())
		{
			// B* 1869980:
			// [XR-015] Player is able to "Join Game" of a friend that has See if you're online set to Blocked in the Privacy & online safety settings.
			// Since XB1 players can block their online status from friends, make sure they are also online since IsInSession references sessions and not presence.
			if (pFriend->IsInSession() DURANGO_ONLY(&& pFriend->IsOnline()))
			{
				SUIContexts::Activate("IsFriendPlayingMP");
			}
			else
			{
				// If the player is in my local session, ensure the context is correct
				rlGamerHandle gh;
				if (pFriend->GetGamerHandle(&gh) && (CNetwork::GetNetworkSession().IsSessionMember(gh) || CNetwork::GetNetworkSession().IsTransitionMember(gh)))
				{
					SUIContexts::Activate("IsFriendPlayingMP");
				}
			}
		}
	}
	else if( g_SystemUi.CanAddFriendUI())
	{
		SUIContexts::Activate("CanFriend"); // If not a friend or a pending friend.
	}

	if (g_SystemUi.CanSendMessages())
	{
		SUIContexts::Activate("CanSendMessage");
	}

	if(CLiveManager::IsInPlatformParty())
	{
		SUIContexts::Activate("IsInPlatParty");

#if PARTY_PLATFORM
		rlPlatformPartyMember partyMember[MAX_NUM_PARTY_MEMBERS];
		unsigned numMembers = CLiveManager::GetPlatformPartyInfo(partyMember, MAX_NUM_PARTY_MEMBERS);
		for(unsigned i=0; i<numMembers; ++i)
		{
			rlGamerHandle tempGamerHandle;

			partyMember[i].GetGamerHandle(&tempGamerHandle);
			if(tempGamerHandle == gamerHandle)
			{
				if(partyMember[i].IsInSession())
				{
					rlSessionInfo info;
					if(partyMember[i].GetSessionInfo(&info) && info.IsValid())
					{
						SUIContexts::Activate("IsPlatPartyInSession");
					}
				}
				break;
			}
		}
#endif
	}

	
	bool bReportKillswitch = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH,ATSTRINGHASH("DISABLE_PLAYER_REPORT", 0xAA4A8EB8), false);
	bool bCommendKillswitch = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("DISABLE_PLAYER_COMMENDATIONS", 0x91BDE453), false);

	
	SUIContexts::SetActive("DisableReport", bReportKillswitch);
	SUIContexts::SetActive("DisableCommend", bCommendKillswitch);


	if( !GetKickVote(gamerHandle) )
		SUIContexts::Activate("CanKickVote");

	bool bEnableKickUnkick = true;
	Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("ENABLE_KICK", 0xe2e1805e), bEnableKickUnkick);

	if(bEnableKickUnkick)
		SUIContexts::Activate("IsKickEnabled");

	if( CContextMenuHelper::InKickCooldown() )
		SUIContexts::Activate("InKickCooldown");

	if( m_bInTournament )
		SUIContexts::Activate("InTournament");

	if( CContextMenuHelper::InReportCooldown())
		SUIContexts::Activate("InReportCooldown");

	if( CContextMenuHelper::InCommendCooldown())
		SUIContexts::Activate("InCommendCooldown");

	bool isNetworked = false;

	if(NetworkInterface::IsNetworkOpen())
	{
		isNetworked = true;

		const CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(gamerHandle);

		// Rockstar Developer context
		SUIContexts::SetActive("RemoteIsRockstarDev", pPlayer && pPlayer->IsRockstarDev());

		// if they have a CNetGamePlayer, they are in my session, therefore, "InLocalSession"
		if( pPlayer )
		{
			SUIContexts::Activate("InLocalSession");

			bool bIsInTransition = pPlayer->HasStartedTransition() && pPlayer->GetTransitionSessionInfo().IsValid();
			if( bIsInTransition )
			{
				SUIContexts::Activate("PlayerInATransition");
			}
			
			if (pPlayer->GetPlayerPed())
			{
				if (pPlayer->GetPlayerPed() && NetworkInterface::IsPlayerPedSpectatingPlayerPed(pPlayer->GetPlayerPed(), NetworkInterface::GetLocalPlayer()->GetPlayerPed()) && !bIsInTransition)
				{
					SUIContexts::Activate("DisableKick");
				}
			}
		}
		else if (CNetwork::GetNetworkSession().IsTransitionMember(gamerHandle))
		{
			SUIContexts::Activate("InLocalSession");
			SUIContexts::Activate("PlayerInATransition");
		}

		if(CNetwork::IsNetworkSessionValid())
		{
			CNetworkSession& rSession = CNetwork::GetNetworkSession();

			if(rSession.IsHost())
				SUIContexts::Activate("AmHost");

			//if(rSession.IsInParty())
			//	SUIContexts::Activate("IsPartyActive");

			//if(rSession.IsPartyLeader())
			//	SUIContexts::Activate("PartyLeader");

			//if(rSession.IsPartyMember(gamerHandle))
			//	SUIContexts::Activate("IsMemberOfParty");

			if(rSession.IsTransitioning() && rSession.IsTransitionMember(gamerHandle))
			{
				SUIContexts::Activate("PlayerInLocalTransition");
			}
		}
	}

	// INVITE STUFF
	InviteMgr& inviteMgr = CLiveManager::GetInviteMgr();
	unsigned inviteIndex = inviteMgr.GetUnacceptedInviteIndex(gamerHandle);
	if(inviteIndex < inviteMgr.GetNumUnacceptedInvites())
	{
		gnetDebug1("ContextMenu::InviteIndex:%d, ", inviteIndex);
		const UnacceptedInvite* pInvite = inviteMgr.GetUnacceptedInvite(inviteIndex);
		if(AssertVerify(pInvite))
		{
			if (pInvite->m_InviterName)
			{
				gnetDebug1("ContextMenu::pInvite:%s ", pInvite->m_InviterName);
			}

			gnetDebug1("ContextMenu::GameInvitePending set");
			SUIContexts::Activate("GameInvitePending");
		}
	}

	// CLAN STUFF
	NetworkClan& clanMgr = CLiveManager::GetNetworkClan();
	if ( clanMgr.HasPrimaryClan() && !bSuppressCrewInvites )
	{
		const rlClanDesc* pMyClan	 = clanMgr.GetPrimaryClan();
		const rlClanDesc* pTheirClan = NULL;

		const rlClanMembershipData* pMyMembership = clanMgr.GetPrimaryClanMembershipData(NetworkInterface::GetLocalGamerHandle());
		if (!pMembership)
		{
			if( CNetGamePlayer* pNetPlayer = NetworkInterface::GetPlayerFromGamerHandle(gamerHandle))
			{
				pMembership = &pNetPlayer->GetClanMembershipInfo();
			}
		}

		if(pMembership)
		{
			pTheirClan = &pMembership->m_Clan;
		}
		// last resort, query this thingy
		else if( clanMgr.HasPrimaryClan(gamerHandle) )
		{
			pTheirClan = clanMgr.GetPrimaryClan(gamerHandle);
		}

		// if they don't have a clan, or it's different from ours, enable the invitation
		if((!pTheirClan || pTheirClan->m_Id != pMyClan->m_Id)
			&& (!pMyMembership || (pMyMembership->m_Rank.m_SystemFlags & RL_CLAN_PERMISSION_INVITE) != 0)
			&& clanMgr.CanSendInvite(gamerHandle))
		{
			SUIContexts::Activate("CanInviteToCrew");
		}
	}

#if RSG_DEV
	if (bDebugPrintContexts)
	{
		SUIContexts::DebugPrintActiveContexts();
	}
#endif

}

bool CContextMenuHelper::HandlePlayerContextOptions(atHashWithStringBank contextOption, const rlGamerHandle& gamerHandle)
{
	if( contextOption == ATSTRINGHASH("KICK",0x84643284) ||
		contextOption == ATSTRINGHASH("KICK_DISABLED",0xc6c189ea))
	{
		if(InKickCooldown() ||
			!SetKickVote(gamerHandle, true))
		{
			CreateWarningMessage("CWS_KICK_ERR");
		}
		
		return true;
	}
	//////////////////////////////////////////////////////////////////////////
	if( contextOption == ATSTRINGHASH("UNKICK",0xd8fea345) )
	{
		SetKickVote(gamerHandle, false);
		return true;
	}
	//////////////////////////////////////////////////////////////////////////
	if( contextOption == ATSTRINGHASH("REPORT",0xf1df07d9) ||
		contextOption == ATSTRINGHASH("REPORT_DISABLED",0x1cc65c37))
	{
		if(!ReportPlayer(gamerHandle))
			CreateWarningMessage("CWS_REPORT_ERR");

		return true;
	}
	//////////////////////////////////////////////////////////////////////////
	if( contextOption == ATSTRINGHASH("COMMEND",0xeec9b656) ||
		contextOption == ATSTRINGHASH("COMMEND_DISABLED",0x298bcd1f))
	{
		if(!CommendPlayer(gamerHandle))
			CreateWarningMessage("CWS_COMMEND_ERR");
		
		return true;
	}
	//////////////////////////////////////////////////////////////////////////
	if( contextOption == ATSTRINGHASH("GAMER_REVIEW",0x133606e4) )
	{
		CLiveManager::ShowGamerFeedbackUi(gamerHandle);
		return true;
	}
	//////////////////////////////////////////////////////////////////////////
	if( contextOption == ATSTRINGHASH("SHOW_GAMER_CARD",0xe24acca0) )
	{
		CLiveManager::ShowGamerProfileUi(gamerHandle);
		return true;
	}
	//////////////////////////////////////////////////////////////////////////
	if( contextOption == ATSTRINGHASH("ADD_A_FRIEND",0x4e1f812) )
	{
		CLiveManager::OpenEmptyFriendPrompt();
		return true;
	}
	//////////////////////////////////////////////////////////////////////////
	if( contextOption == ATSTRINGHASH("SEND_MESSAGE",0x70733039) )
	{
		CLiveManager::ShowMessageComposeUI(&gamerHandle, 1, TheText.Get("NT_MSG_SUBJECT"), "");
		return true;
	}
	//////////////////////////////////////////////////////////////////////////
	if( contextOption == ATSTRINGHASH("SEND_FRIEND_INVITE",0xf1384765) )
	{
		StatId_char friendStat("CL_SEND_FRIEND_REQUEST");
		StatsInterface::SetStatData(friendStat.GetStatId(), true);

		CLiveManager::AddFriend(gamerHandle, "");
		return true;
	}
	//////////////////////////////////////////////////////////////////////////
	if( contextOption == ATSTRINGHASH("MUTE",0x5c13a9b2) )
	{
        VoiceGamerSettings* pGamer = NetworkInterface::GetVoice().FindGamer(gamerHandle);
        if(pGamer)
        {
            BANK_ONLY(gnetDebug1("HandlePlayerContextOptions :: %s %s", !(pGamer->GetLocalVoiceFlags() & VoiceGamerSettings::VOICE_LOCAL_FORCE_MUTED) ? "Muting" : "Unmuting", NetworkUtils::LogGamerHandle(pGamer->GetGamerHandle()));)
            NetworkInterface::GetVoice().ToggleMute(gamerHandle);
        }
        return true;
	}

	return false;
}


void CContextMenuHelper::CreateWarningMessage( const char* pBody )
{
	CWarningMessage::Data newMessage;
	newMessage.m_TextLabelHeading = "CWS_WARNING";
	newMessage.m_TextLabelBody = pBody;
	newMessage.m_iFlags = FE_WARNING_OK;
	newMessage.m_acceptPressed = CContextMenuHelper::ClearWarningScreen;
	CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().SetMessage(newMessage);
}

void CContextMenuHelper::ClearWarningScreen()
{
	CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().Clear();
	CPauseMenu::PlayInputSound(FRONTEND_INPUT_ACCEPT);
}

bool CContextMenuHelper::ReportUGC(const rlGamerHandle& gamerHandle)
{
	if(!InReportCooldown())
	{
		SReportMenu::GetInstance().Open(gamerHandle, CReportMenu::eReportMenu_UGCMenu);

		return true;
	}

	return false;
}

bool CContextMenuHelper::ReportPlayer(const rlGamerHandle& gamerHandle)
{
	if(!InReportCooldown() && !SUIContexts::IsActive("DisableReport"))
	{
		SReportMenu::GetInstance().Open(gamerHandle, CReportMenu::eReportMenu_ReportMenu);

		return true;
	}

	return false;
}

bool CContextMenuHelper::CommendPlayer(const rlGamerHandle& gamerHandle)
{
	if(!InCommendCooldown() && !SUIContexts::IsActive("DisableCommend") ) 
	{
		SReportMenu::GetInstance().Open(gamerHandle, CReportMenu::eReportMenu_CommendMenu);

		return true;
	}

	return false;
}

void CContextMenuHelper::PlayerHasJoined(PhysicalPlayerIndex index)
{
	SetKickVote(index, false, false);
}

void CContextMenuHelper::PlayerHasLeft(PhysicalPlayerIndex index)
{
	SetKickVote(index, false, false);
}

bool CContextMenuHelper::SetKickVote(const rlGamerHandle& gamerHandle, bool vote)
{
	return SetKickVote(NetworkInterface::GetPhysicalPlayerIndexFromGamerHandle(gamerHandle), vote);
}

bool CContextMenuHelper::SetKickVote(PhysicalPlayerIndex index, bool vote, bool castNetworkVote)
{

	if(index < sm_kickVotes.size())
	{	
		if(vote)
		{
			if(InKickCooldown())
			{
				return false;
			}
			else
			{
				m_uLastKick = sysTimer::GetSystemMsTime();
			}
		}

		sm_kickVotes[index] = vote;

		if (castNetworkVote)
		{
			CNetGamePlayer* pPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(index);
			if(pPlayer)
			{
				if(vote)
				{
					NetworkInterface::LocalPlayerVoteToKickPlayer(*pPlayer);
				}
				else
				{
					NetworkInterface::LocalPlayerUnvoteToKickPlayer(*pPlayer);
				}
			}
		}

		return true;
	}

	return false;
}

bool CContextMenuHelper::GetKickVote(const rlGamerHandle& gamerHandle)
{
	bool retVal = false;

	PhysicalPlayerIndex index = NetworkInterface::GetPhysicalPlayerIndexFromGamerHandle(gamerHandle);
	if(index < sm_kickVotes.size())
	{
		retVal = sm_kickVotes[index];
	}

	return retVal;
}

static const int TIMEOUT_DEFAULT = 600;
static const int SPAM_TIMEOUT_DEFAULT = 1200;

bool CContextMenuHelper::InKickCooldown()
{
    return CooldownHelper(ATSTRINGHASH("KICK_TIMEOUT", 0xb2111fe4), ATSTRINGHASH("KICK_TIMEOUT_DISABLED", 0xd892e535), TIMEOUT_DEFAULT, m_uLastKick);
}

bool CContextMenuHelper::InReportCooldown()
{
    return CooldownHelper(ATSTRINGHASH("REPORT_TIMEOUT", 0xa13b434d), ATSTRINGHASH("REPORT_TIMEOUT_DISABLED", 0x7266e8b2), TIMEOUT_DEFAULT, m_uLastReport);
}

bool CContextMenuHelper::InCommendCooldown()
{
    return CooldownHelper(ATSTRINGHASH("COMMEND_TIMEOUT", 0xabe83c3b), ATSTRINGHASH("COMMEND_TIMEOUT_DISABLED", 0x73dc7aa3), TIMEOUT_DEFAULT, m_uLastCommend);
}

bool CContextMenuHelper::CooldownHelper(unsigned tunableNameHash, unsigned disabledNameHash, const int tunableDefault, u32 lastTime)
{
	int iTimeout = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, tunableNameHash, tunableDefault);
    
    bool bTimeoutDisabled = false;
    Tunables::GetInstance().Access(CD_GLOBAL_HASH, disabledNameHash, bTimeoutDisabled);

    return !bTimeoutDisabled &&
		  (lastTime != 0) &&
		  (sysTimer::GetSystemMsTime() - lastTime < iTimeout*1000);
}


void CContextMenuHelper::ShowCommendScreen()
{
	atArray<u32> nameHashArray;
	atArray<datCallback> callbackArray;

	nameHashArray.PushAndGrow(ATSTRINGHASH("CWS_FRIENDLY", 0x0cee051ef));
	nameHashArray.PushAndGrow(ATSTRINGHASH("CWS_HELPFUL", 0x085b73cf3));

	callbackArray.PushAndGrow(datCallback(CContextMenuHelper::clbCommendForFriendly));
	callbackArray.PushAndGrow(datCallback(CContextMenuHelper::clbCommendForHelpful));

//	CCustomWarningScreen::ShowCustomWarningScreen("CWS_COMMEND", "CWS_COMMEND_DESC", nameHashArray, callbackArray);
}

void CContextMenuHelper::clbReportCommendHelper(u32& lastTimestamp, const StatId cStatIdStrength, const StatId cStatIdPenalty, const StatId cStatIdRestore, const unsigned nTunableNameHash, const unsigned nDisableHash, const int tunableDefault, u32 eventHash)
{
	int iStrength = StatsInterface::GetIntStat(cStatIdStrength);
	u32 uDate = (CClock::GetYear() * 365) + (CClock::GetMonth() * 12) + CClock::GetDay();

    bool bReportTimeoutDisabled = false;
    Tunables::GetInstance().Access(CD_GLOBAL_HASH, nDisableHash, bReportTimeoutDisabled);
   
    int iStrengthPenaltyTimeout = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, nTunableNameHash, tunableDefault);
    int iMaxStrength = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("COMMENDREPORT_MAX_STRENGTH", 0xf227f666), DEFAULT_MAX_REPORT_COMMEND_STRENGTH);

	if(iStrength < iMaxStrength)
	{
		u32 uLastReportPenalty = StatsInterface::GetIntStat(cStatIdPenalty);
		u32 uLastReportRestore = StatsInterface::GetIntStat(cStatIdRestore);
		if(uDate - uLastReportPenalty > 0 && (uDate - uLastReportRestore > 0 || uLastReportRestore == 0))
		{
			StatsInterface::IncrementStat(cStatIdStrength, 1);
			StatsInterface::SetStatData(cStatIdRestore, uDate);
		}
	}

	if(iStrength > iMaxStrength)
	{
		StatsInterface::SetStatData(cStatIdStrength, iMaxStrength);
		iStrength = iMaxStrength;
	}

	if(!bReportTimeoutDisabled && 
		sysTimer::GetSystemMsTime() - lastTimestamp > iStrengthPenaltyTimeout*1000)
	{
		if(StatsInterface::GetIntStat(cStatIdStrength) > 0)
		{
			StatsInterface::DecrementStat(cStatIdStrength, 1);
			StatsInterface::SetStatData(cStatIdPenalty, uDate);
		}
	}

#if __BANK
	if(m_bNonStopReports)
	{
		m_uLastReport = 0;
		m_uLastCommend = 0;
	}
#endif //__BANK

	lastTimestamp = sysTimer::GetSystemMsTime();
	int iReportStrength = StatsInterface::GetIntStat(cStatIdStrength);
	netPlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(SReportMenu::GetInstance().GetReportedGamerHandle());
	CNetworkIncrementStatEvent::Trigger(eventHash, iReportStrength, pPlayer);
}

void CContextMenuHelper::clbReportForGriefing()
{
	// If someone is already in the cheater pool don't send a report but give the impression
	// that one was sent by enforcing the wait period between reports / commends.
	if(CNetwork::IsCheater())
	{
		m_uLastReport = sysTimer::GetSystemMsTime();
		return;
	}

	clbReportCommendHelper(m_uLastReport,
		ATSTRINGHASH("MPPLY_REPORT_STRENGTH", 0x850CF917), ATSTRINGHASH("MPPLY_LAST_REPORT_PENALTY", 0xB87499EA), ATSTRINGHASH("MPPLY_LAST_REPORT_RESTORE", 0x211774A4),
		ATSTRINGHASH("REPORT_SPAM_TIMEOUT", 0x332f884e), ATSTRINGHASH("REPORT_SPAM_TIMEOUT_DISABLED", 0x33406312), SPAM_TIMEOUT_DEFAULT,
		ATSTRINGHASH("MPPLY_GRIEFING", 0x9C6A0C42));
}

void CContextMenuHelper::clbReportForOffensiveLang()
{
	// If someone is already in the cheater pool don't send a report but give the impression
	// that one was sent by enforcing the wait period between reports.
	if(CNetwork::IsCheater())
	{
        //@@: location CCONTEXTMENUHELPER_CLBREPORTFOROFFENSIVELANG_POST_ISCHEATER_A
		m_uLastReport = sysTimer::GetSystemMsTime();
		return;
	}
    //@@: location CCONTEXTMENUHELPER_CLBREPORTFOROFFENSIVELANG_POST_ISCHEATER_B
	clbReportCommendHelper(m_uLastReport,
		ATSTRINGHASH("MPPLY_REPORT_STRENGTH", 0x850CF917), ATSTRINGHASH("MPPLY_LAST_REPORT_PENALTY", 0xB87499EA), ATSTRINGHASH("MPPLY_LAST_REPORT_RESTORE", 0x211774A4),
		ATSTRINGHASH("REPORT_SPAM_TIMEOUT", 0x332f884e), ATSTRINGHASH("REPORT_SPAM_TIMEOUT_DISABLED", 0x33406312), SPAM_TIMEOUT_DEFAULT,
		ATSTRINGHASH("MPPLY_OFFENSIVE_LANGUAGE", 0x3CDB43E2));
}

void CContextMenuHelper::clbReportForOffensiveTagPlate()
{
	// These can still be sent when in the cheater pool

	clbReportCommendHelper(m_uLastReport,
		ATSTRINGHASH("MPPLY_REPORT_STRENGTH", 0x850CF917), ATSTRINGHASH("MPPLY_LAST_REPORT_PENALTY", 0xB87499EA), ATSTRINGHASH("MPPLY_LAST_REPORT_RESTORE", 0x211774A4),
		ATSTRINGHASH("REPORT_SPAM_TIMEOUT", 0x332f884e), ATSTRINGHASH("REPORT_SPAM_TIMEOUT_DISABLED", 0x33406312), SPAM_TIMEOUT_DEFAULT,
		ATSTRINGHASH("MPPLY_OFFENSIVE_TAGPLATE", 0xE8FB6DD5));
}

void CContextMenuHelper::clbReportForOffensiveUGC()
{
	// These can still be sent when in the cheater pool

	clbReportCommendHelper(m_uLastReport,
		ATSTRINGHASH("MPPLY_REPORT_STRENGTH", 0x850CF917), ATSTRINGHASH("MPPLY_LAST_REPORT_PENALTY", 0xB87499EA), ATSTRINGHASH("MPPLY_LAST_REPORT_RESTORE", 0x211774A4),
		ATSTRINGHASH("REPORT_SPAM_TIMEOUT", 0x332f884e), ATSTRINGHASH("REPORT_SPAM_TIMEOUT_DISABLED", 0x33406312), SPAM_TIMEOUT_DEFAULT,
		ATSTRINGHASH("MPPLY_OFFENSIVE_UGC", 0xF3DE4879));
}


void CContextMenuHelper::clbReportForGameExploit()
{
	// These can still be sent when in the cheater pool

	clbReportCommendHelper(m_uLastReport,
		ATSTRINGHASH("MPPLY_REPORT_STRENGTH", 0x850CF917), ATSTRINGHASH("MPPLY_LAST_REPORT_PENALTY", 0xB87499EA), ATSTRINGHASH("MPPLY_LAST_REPORT_RESTORE", 0x211774A4),
		ATSTRINGHASH("REPORT_SPAM_TIMEOUT", 0x332f884e), ATSTRINGHASH("REPORT_SPAM_TIMEOUT_DISABLED", 0x33406312), SPAM_TIMEOUT_DEFAULT,
		ATSTRINGHASH("MPPLY_GAME_EXPLOITS", 0xCBFD04A4));
}


void CContextMenuHelper::clbReportForExploit()
{
	// These can still be sent when in the cheater pool

	clbReportCommendHelper(m_uLastReport,
		ATSTRINGHASH("MPPLY_REPORT_STRENGTH", 0x850CF917), ATSTRINGHASH("MPPLY_LAST_REPORT_PENALTY", 0xB87499EA), ATSTRINGHASH("MPPLY_LAST_REPORT_RESTORE", 0x211774A4),
		ATSTRINGHASH("REPORT_SPAM_TIMEOUT", 0x332f884e), ATSTRINGHASH("REPORT_SPAM_TIMEOUT_DISABLED", 0x33406312), SPAM_TIMEOUT_DEFAULT,
		ATSTRINGHASH("MPPLY_EXPLOITS", 0x9F79BA0B));
}


void CContextMenuHelper::clbReportForUGCContent()
{
}

void CContextMenuHelper::clbReportForVCAnnoying()
{
	// These can still be sent when in the cheater pool

	clbReportCommendHelper(m_uLastReport,
		ATSTRINGHASH("MPPLY_REPORT_STRENGTH", 0x850CF917), ATSTRINGHASH("MPPLY_LAST_REPORT_PENALTY", 0xB87499EA), ATSTRINGHASH("MPPLY_LAST_REPORT_RESTORE", 0x211774A4),
		ATSTRINGHASH("REPORT_SPAM_TIMEOUT", 0x332f884e), ATSTRINGHASH("REPORT_SPAM_TIMEOUT_DISABLED", 0x33406312), SPAM_TIMEOUT_DEFAULT,
		ATSTRINGHASH("MPPLY_VC_ANNOYINGME", 0x62EB8C5A));
}

void CContextMenuHelper::clbReportForVCHate()
{
	// These can still be sent when in the cheater pool

	clbReportCommendHelper(m_uLastReport,
		ATSTRINGHASH("MPPLY_REPORT_STRENGTH", 0x850CF917), ATSTRINGHASH("MPPLY_LAST_REPORT_PENALTY", 0xB87499EA), ATSTRINGHASH("MPPLY_LAST_REPORT_RESTORE", 0x211774A4),
		ATSTRINGHASH("REPORT_SPAM_TIMEOUT", 0x332f884e), ATSTRINGHASH("REPORT_SPAM_TIMEOUT_DISABLED", 0x33406312), SPAM_TIMEOUT_DEFAULT,
		ATSTRINGHASH("MPPLY_VC_HATE", 0xE7072CD));
}

void CContextMenuHelper::clbReportForTextChatAnnoying()
{
	// These can still be sent when in the cheater pool

	clbReportCommendHelper(m_uLastReport,
		ATSTRINGHASH("MPPLY_REPORT_STRENGTH", 0x850CF917), ATSTRINGHASH("MPPLY_LAST_REPORT_PENALTY", 0xB87499EA), ATSTRINGHASH("MPPLY_LAST_REPORT_RESTORE", 0x211774A4),
		ATSTRINGHASH("REPORT_SPAM_TIMEOUT", 0x332f884e), ATSTRINGHASH("REPORT_SPAM_TIMEOUT_DISABLED", 0x33406312), SPAM_TIMEOUT_DEFAULT,
		ATSTRINGHASH("MPPLY_TC_ANNOYINGME", 0x762F9994));
}

void CContextMenuHelper::clbReportForTextChatHate()
{
	// These can still be sent when in the cheater pool

	clbReportCommendHelper(m_uLastReport,
		ATSTRINGHASH("MPPLY_REPORT_STRENGTH", 0x850CF917), ATSTRINGHASH("MPPLY_LAST_REPORT_PENALTY", 0xB87499EA), ATSTRINGHASH("MPPLY_LAST_REPORT_RESTORE", 0x211774A4),
		ATSTRINGHASH("REPORT_SPAM_TIMEOUT", 0x332f884e), ATSTRINGHASH("REPORT_SPAM_TIMEOUT_DISABLED", 0x33406312), SPAM_TIMEOUT_DEFAULT,
		ATSTRINGHASH("MPPLY_TC_HATE", 0xB722D6C0));
}

void CContextMenuHelper::clbCommendForFriendly()
{
	clbReportCommendHelper(m_uLastCommend,
		ATSTRINGHASH("MPPLY_COMMEND_STRENGTH", 0xADA7E7D3), ATSTRINGHASH("MPPLY_LAST_COMMEND_PENALTY", 0x8CF94EB4), ATSTRINGHASH("MPPLY_LAST_COMMEND_RESTORE", 0x181F07A6),
		ATSTRINGHASH("COMMEND_SPAM_TIMEOUT", 0xa76d3517), ATSTRINGHASH("COMMEND_SPAM_TIMEOUT_DISABLED", 0x6133e077), SPAM_TIMEOUT_DEFAULT,
		ATSTRINGHASH("MPPLY_FRIENDLY", 0xDAFB10F9));
}

void CContextMenuHelper::clbCommendForHelpful()
{
	clbReportCommendHelper(m_uLastCommend,
		ATSTRINGHASH("MPPLY_COMMEND_STRENGTH", 0xADA7E7D3), ATSTRINGHASH("MPPLY_LAST_COMMEND_PENALTY", 0x8CF94EB4), ATSTRINGHASH("MPPLY_LAST_COMMEND_RESTORE", 0x181F07A6),
		ATSTRINGHASH("COMMEND_SPAM_TIMEOUT", 0xa76d3517), ATSTRINGHASH("COMMEND_SPAM_TIMEOUT_DISABLED", 0x6133e077), SPAM_TIMEOUT_DEFAULT,
		ATSTRINGHASH("MPPLY_HELPFUL", 0x893E1390));
}

// eof
