//#define ENABLE_TRAPS_IN_THIS_FILE


#include "fwnet/netscgameconfigparser.h"

#include "Frontend/CCrewMenu.h"

#include "Frontend/ButtonEnum.h"
#include "Frontend/Scaleform/ScaleformMgr.h"
#include "Frontend/CFriendsMenu.h"
#include "Frontend/ui_channel.h"
#include "Frontend/SocialClubMenu.h"
#include "Network/NetworkInterface.h"
#include "Network/Live/livemanager.h"
#include "rline/rlstats.h"
#include "frontend/MousePointer.h"

#include "Network/Crews/NetworkCrewDataMgr.h"
#include "Network/crews/NetworkCrewMetadata.h"
#include "Network/Stats/NetworkLeaderboardSessionMgr.h"

#include <time.h>

#include "text/GxtString.h"
#include "Network/Live/PlayerCardDataManager.h"

// helper to maintain readability for atHashWithString objects, when ultimately we don't really need 'em
#if __DEV
#define UIATSTRINGHASH(str,code) str
#else
#define UIATSTRINGHASH(str,code) ATSTRINGHASH(str,code)
#endif

FRONTEND_MENU_OPTIMISATIONS()

static BankInt32 CARD_MOVEMENT_INTERVAL = 850;
static BankInt32 CARD_FIRST_TIME_OFFSET = -450;
static BankInt32 CLEAR_COOLDOWN			= 60000;
static BankInt32 EVENT_COOLDOWN			= 3000; // time after receiving a net event before we process what happened (to prevent hyper spam)

IMPLEMENT_UI_DATA_PAGE(UICrewLBPage);
IMPLEMENT_UI_DATA_PAGE(UIRequestPage);
IMPLEMENT_UI_DATA_PAGE(UIInvitePage);
IMPLEMENT_UI_DATA_PAGE(UIOpenClanPage);
IMPLEMENT_UI_DATA_PAGE(UIFriendsClanPage);

#define LEADERBOARD_FOR_XP "FREEMODE_PLAYER_XP"
#define COMPARE_CONTEXT		UIATSTRINGHASH("CAN_COMPARE",0xbe91158d)
#define IN_CREW_DETAILS		ATSTRINGHASH("IN_CREW_DETAILS",0x1fca594e)

// defining enum hacks to match the scaleform movie
#define CREW_COLUMN_RESULTS			PM_COLUMN_MIDDLE
#define CREW_COLUMN_MAIN_CARD		static_cast<PM_COLUMNS>(2)
#define CREW_COLUMN_LB_RANKS		static_cast<PM_COLUMNS>(3)
#define CREW_COLUMN_COMPARE_LEFT	static_cast<PM_COLUMNS>(4)
#define CREW_COLUMN_COMPARE_RIGHT	static_cast<PM_COLUMNS>(5)

#define EMPTY_ROW_STYLE				eOPTION_DISPLAY_STYLE(3)

const Leaderboard* UICrewLBPage::sm_pLBData = NULL;

#if __BANK
bool CCrewMenu::sm_bDbgAppendNumbers = false;
bool s_gFakeNetworkLoss = false;

#endif

//////////////////////////////////////////////////////////////////////////
bool UICrewLBPage::SetData(CCrewMenu* pOwner)
{
	m_pDynamicOwner = pOwner;

	if( !sm_pLBData )
		sm_pLBData = GAME_CONFIG_PARSER.GetLeaderboard(LEADERBOARD_FOR_XP);

	if( !uiVerifyf(sm_pLBData, "Unable to find a leaderboard called " LEADERBOARD_FOR_XP "!"))
		return false;

	// using GetPagedIndex(), we can determine what data to collect
	CNetworkReadLeaderboards& readmgr = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr();
	rlLeaderboard2GroupSelector groupSelector;

	const netLeaderboardRead* pResult = readmgr.AddReadByRank(NetworkInterface::GetLocalGamerIndex()
		, sm_pLBData->m_id
		, RL_LEADERBOARD2_TYPE_CLAN
		, groupSelector
		, RL_INVALID_CLAN_ID
		, GetMaxSize()
		, (GetPagedIndex() * VISIBLE_CREW_ITEMS * CREW_UI_PAGE_COUNT)+1 );

	uiAssertf(pResult, "Was unable to do a leaderboard request from " LEADERBOARD_FOR_XP " for index %d", GetPagedIndex()+1);

	// need to cast it away so we can regdref it.
	m_LeaderboardRead = const_cast<netLeaderboardRead*>(pResult);
	
	return pResult != NULL;
}

void UICrewLBPage::Update()
{

	if (!IsSCAvailable())
	{
		return;
	}

	// we've sorted? sit quietly
	if( m_LocalDataIndex_to_DescIndex[0] != UNSORTED_LUT )
		return;

	// no LB? nothing to do
	if( !m_LeaderboardRead.Get() )
		return;
	
	if( m_LeaderboardRead->Failed() )
	{
		m_Descs.m_RequestStatus.ForceFailed();
		m_LocalDataIndex_to_DescIndex[0] = INVALID_LUT;
		return;
	}

	// no success? nothing to do.
	if( !m_LeaderboardRead->Suceeded() )
		return;

	// if the description request has failed, sameify the values
	if( m_Descs.m_RequestStatus.Failed() )
	{
		m_LocalDataIndex_to_DescIndex[0] = INVALID_LUT;
		return;
	}

	// it's successful, sort that sumbitch
	// because the servers just can't be bothered to put them into the order we want.
	if( m_Descs.m_RequestStatus.Succeeded() )
	{
		// for every data row
		unsigned results = m_LeaderboardRead->GetNumRows();
		u8 firstAvailableEntry = UNSORTED_LUT;
		for( unsigned rowIndex = 0; rowIndex < results; ++rowIndex )
		{
			const rlLeaderboard2Row* pRow = m_LeaderboardRead->GetRow(rowIndex);

			// because disbanded crews don't return a ClanDesc, set it as an INVALID entry first
			m_LocalDataIndex_to_DescIndex[rowIndex] = INVALID_LUT;

			// for every result
			for( u8 resultIndex = 0; resultIndex < results; ++resultIndex )
			{
				if( m_Descs[resultIndex].m_Id == pRow->m_ClanId )
				{
					m_LocalDataIndex_to_DescIndex[rowIndex] = resultIndex;
					break;
				}
				// find the end of the array so we can push a fake entry in there if necessary
				else if( m_Descs[resultIndex].m_Id == RL_INVALID_CLAN_ID
					&& firstAvailableEntry == UNSORTED_LUT )
				{
					firstAvailableEntry = resultIndex;
				}
			}

			// fell off the end, means we get to FAKE a description! WOO
			if( m_LocalDataIndex_to_DescIndex[rowIndex] == INVALID_LUT
				&& firstAvailableEntry < m_Descs.GetMaxSize() )
			{
				m_LocalDataIndex_to_DescIndex[rowIndex] = firstAvailableEntry;
				rlClanDesc& newDesc = m_Descs[firstAvailableEntry++];
				newDesc.m_Id           = pRow->m_ClanId;
				newDesc.m_clanColor    = CHudColour::GetIntColour(HUD_COLOUR_GREY);
				newDesc.m_MemberCount  = 0;
				newDesc.m_IsOpenClan   = false;
				newDesc.m_IsSystemClan = false;
				formatf(newDesc.m_ClanName, RL_CLAN_NAME_MAX_CHARS, pRow->m_ClanName);
				formatf(newDesc.m_ClanTag,  RL_CLAN_TAG_MAX_CHARS,  pRow->m_ClanTag);
			}
		}
		return;
	}

	// it's not doing anything, make it!
	if( !m_Descs.m_RequestStatus.Pending() )
	{
		// kick off our Desc read
		const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();


		// prepare our contiguous list to make the rlClan thing below happy
		atArray<rlClanId> Ids;
		Ids.Reserve(m_LeaderboardRead->GetNumRows());
		for( int i = 0; i < Ids.GetCapacity(); ++i )
		{
			const rlLeaderboard2Row* pRow = m_LeaderboardRead->GetRow(i);
			Ids.Push( pRow->m_ClanId );
		}
		// lie about its size
		m_Descs.m_iResultSize = m_LeaderboardRead->GetNumRows();
		if( Ids.GetCount() == 0 )
		{
			m_Descs.m_RequestStatus.ForceSucceeded();
			m_LocalDataIndex_to_DescIndex[0] = INVALID_LUT;
		}
		else if( !rlClan::GetDescs(localGamerIndex, &Ids[0], Ids.GetCount(), &m_Descs[0], &m_Descs.m_RequestStatus) )
		{
			uiAssertf(false, "Unable to request Clan Descs for LB page #%i", GetPagedIndex());
			m_Descs.m_RequestStatus.ForceFailed();
			m_LocalDataIndex_to_DescIndex[0] = INVALID_LUT;
		}
	}
}

void UICrewLBPage::Shutdown()
{
	if( m_LeaderboardRead.Get() )
	{
		CNetworkReadLeaderboards& readmgr = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr();
		readmgr.ClearLeaderboardRead(m_LeaderboardRead);
		m_LeaderboardRead = NULL;
	}
}

bool UICrewLBPage::IsSCAvailable()
{
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	if( !rlClan::IsServiceReady(localGamerIndex) ) 
		return false; 
    if( !rlClan::IsFullServiceReady(localGamerIndex) ) 
        return false; 

	if( !NetworkInterface::IsOnlineRos() ) 
		return false;  

	if( !NetworkInterface::IsCloudAvailable())
		return false;

	return true;
}

bool UICrewLBPage::IsReady() const
{
	if( m_pDynamicOwner && !m_pDynamicOwner->IsReady() )
		return false;

	// haven't gotten to the sorting step? not ready
	if( m_LocalDataIndex_to_DescIndex[0] == UNSORTED_LUT )
		return false;

	// this happens AFTER the leaderboard, so I guess we could check it first. For science?
	if( !(m_Descs.m_RequestStatus.Succeeded() || m_Descs.m_RequestStatus.Failed()) )
		return false;

	return m_LeaderboardRead.Get() && (m_LeaderboardRead->Finished() || m_LeaderboardRead->Failed());
}

bool UICrewLBPage::HasFailed() const
{
	return (m_LeaderboardRead.Get() && m_LeaderboardRead->Failed()) || m_Descs.m_RequestStatus.Failed();
}

int UICrewLBPage::GetResultCode() const
{
	uiAssertf(HasFailed(), "Leaderboard page queried for failure reason, but it didn't really fail...");
	if( m_LeaderboardRead.Get() && m_LeaderboardRead->Failed())
		return m_LeaderboardRead->GetResultCode();

	if( m_Descs.m_RequestStatus.Failed() )
		return m_Descs.m_RequestStatus.GetResultCode();

	return -1;
}

const rlClanDesc& UICrewLBPage::GetClanDesc(LocalDataIndex iItemIndex) const
{
	if( uiVerifyf(IsReady(), "Can't get ClanDesc for %i if we're not ready!", iItemIndex) 
		&& uiVerifyf(iItemIndex >= 0 && iItemIndex < m_Descs.m_iResultSize, "Can't get iItemIndex %i, it's outside of our range [0,%i)", iItemIndex, m_Descs.m_iResultSize ) 
		&& uiVerifyf( m_LocalDataIndex_to_DescIndex[iItemIndex] != UNSORTED_LUT, "Trying to get an unsorted entry for %i!", iItemIndex )
		&& m_LocalDataIndex_to_DescIndex[iItemIndex] != INVALID_LUT 
		)
		return m_Descs[ m_LocalDataIndex_to_DescIndex[iItemIndex] ];

	return RL_INVALID_CLAN_DESC;
}

int UICrewLBPage::GetSize() const
{
	if( !IsReady() )
		return 0;
	return m_LeaderboardRead->GetNumRows();
}

int UICrewLBPage::GetTotalSize() const
{
	if( !IsReady() )
		return 0;
	return m_LeaderboardRead->GetNumTotalRows();
}


bool UICrewLBPage::FillScaleformEntry( LocalDataIndex iItemIndex, int iUIIndex, MenuScreenId ColumnId, DataIndex iUniqueId, int iColumn, bool bIsUpdate )
{
	const rlLeaderboard2Row* pRow = m_LeaderboardRead->GetRow(iItemIndex);


	bool bCheckedOff		=  false;
	bool bShowBox			= m_pDynamicOwner->AmIInThisClan(pRow->m_ClanId, &bCheckedOff);
	const char* pszClanName = pRow->m_ClanName;

	if( CScaleformMenuHelper::SET_DATA_SLOT(PM_COLUMNS(iColumn), iUIIndex, ColumnId + PREF_OPTIONS_THRESHOLD, iUniqueId
		, OPTION_DISPLAY_STYLE_SLIDER, 0, 1, pszClanName, false, bIsUpdate) )
	{
		CScaleformMgr::AddParamBool(bShowBox);
		CScaleformMgr::AddParamBool(bCheckedOff);
		CScaleformMgr::EndMethod();
	}

	if( CScaleformMenuHelper::SET_DATA_SLOT_NO_LABEL( CREW_COLUMN_LB_RANKS, iUIIndex, ColumnId + PREF_OPTIONS_THRESHOLD, iUniqueId, OPTION_DISPLAY_STYLE_SLIDER, 0, 1, false) )
	{
		CScaleformMgr::AddParamFormattedInt(pRow->m_Rank);
		CScaleformMgr::AddParamString("",false);
		CScaleformMgr::AddParamFormattedInt(pRow->m_ColumnValues[0].Int64Val);
		CScaleformMgr::EndMethod();
	}


	return true;
}

bool UICrewLBPage::FillEmptyScaleformSlots( LocalDataIndex /*iItemIndex*/, int iUIIndex, MenuScreenId ColumnId, DataIndex iUniqueId, int iColumn )
{
	CScaleformMenuHelper::SET_DATA_SLOT( PM_COLUMNS(iColumn), iUIIndex, ColumnId + PREF_OPTIONS_THRESHOLD, iUniqueId, EMPTY_ROW_STYLE, 0, 0, ""); 

	if( CScaleformMenuHelper::SET_DATA_SLOT_NO_LABEL( CREW_COLUMN_LB_RANKS, iUIIndex, ColumnId + PREF_OPTIONS_THRESHOLD, iUniqueId, EMPTY_ROW_STYLE, 0, 0, false) )
	{
		CScaleformMgr::AddParamString("",false);
		CScaleformMgr::AddParamString("",false);
		CScaleformMgr::AddParamString("",false);
		CScaleformMgr::EndMethod();
	}

	return true;
}

void UICrewLBPage::FillScaleformBase(int iColumn, MenuScreenId ColumnId, DataIndex iStartingIndex, int iMaxPerPage, bool bHasFocus)
{
	if (!IsSCAvailable())
		return;

	CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(CREW_COLUMN_LB_RANKS);

	// base function
	UIDataPageBase::FillScaleformBase(iColumn, ColumnId, iStartingIndex, iMaxPerPage, bHasFocus);
	//

	CScaleformMenuHelper::DISPLAY_DATA_SLOT(CREW_COLUMN_LB_RANKS);
	
	if( !m_pDynamicOwner->ShouldShowCrewCards() && !m_pDynamicOwner->IsShowingWarningColumn() )
		m_pDynamicOwner->SHOW_COLUMN(CREW_COLUMN_LB_RANKS, true);
}


void UICrewLBPage::SetSelectedItem(int iColumn, int iSlotIndex, DataIndex playerIndex)
{
	UIDataPageBase::SetSelectedItem(iColumn, iSlotIndex, playerIndex);
	CCrewMenu* pOwner = verify_cast<CCrewMenu*>(CPauseMenu::GetCurrentScreenData().GetDynamicMenu());
	if( pOwner )
		pOwner->SetCrewIndexAndPrepareCard(playerIndex,false);
}

//////////////////////////////////////////////////////////////////////////

const rlClanDesc& UICrewLBPaginator::GetClanDesc(DataIndex indexToGet) const
{
	const UICrewLBPage* pCurPage = verify_cast<const UICrewLBPage*>(GetActivePage());
	if(uiVerifyf(pCurPage, "No Active page for Get ClanID? Teh Fu?!") )
	{
		if( uiVerifyf(pCurPage->IsReady(), "We definitely shouldn't be getting a clan description before you're ready for one!" ))
		{
			return pCurPage->GetClanDesc( indexToGet - pCurPage->GetLowestEntry() );
		}
	}

	return RL_INVALID_CLAN_DESC;
}

//////////////////////////////////////////////////////////////////////////
bool UIFriendsClanPage::IsReady() const
{
	if( m_pDynamicOwner && !m_pDynamicOwner->IsReady() )
		return false;

	return CPauseMenu::GetDynamicPauseMenu()->GetFriendClanData().GetStatus().Succeeded();
}

int UIFriendsClanPage::GetTotalSize() const
{
	return CPauseMenu::GetDynamicPauseMenu()->GetFriendClanData().GetGroupClanCount();
}

const rlClanDesc& UIFriendsClanPaginator::GetClanDesc(DataIndex indexToGet) const
{
	return *(CPauseMenu::GetDynamicPauseMenu()->GetFriendClanData().GetGroupClanDesc(indexToGet));
}

bool UIFriendsClanPage::FillScaleformEntry( LocalDataIndex UNUSED_PARAM(iItemIndex), int iUIIndex, MenuScreenId UNUSED_PARAM(ColumnId), DataIndex iUniqueId, int iColumn, bool bIsUpdate )
{
	const rlClanDesc* pFriendData = CPauseMenu::GetDynamicPauseMenu()->GetFriendClanData().GetGroupClanDesc(iUniqueId);

	if( uiVerifyf(pFriendData, "Can't have a friendsize with no clan data!" ) )
	{
		bool bCheckedOff		=  false;
		bool bShowBox			= m_pDynamicOwner->AmIInThisClan(pFriendData->m_Id, &bCheckedOff);
		const char* pszClanName = pFriendData->m_ClanName;

		if( CScaleformMenuHelper::SET_DATA_SLOT(PM_COLUMNS(iColumn), iUIIndex, MENU_UNIQUE_ID_CREW_LIST + PREF_OPTIONS_THRESHOLD, iUniqueId
			, OPTION_DISPLAY_STYLE_SLIDER, 0, true, pszClanName, false, bIsUpdate) )
		{
			CScaleformMgr::AddParamBool(bShowBox);
			CScaleformMgr::AddParamBool(bCheckedOff);
			CScaleformMgr::EndMethod();
			return true;
		}
	}

	return false;
}

bool UIFriendsClanPage::FillEmptyScaleformSlots( LocalDataIndex UNUSED_PARAM(iItemIndex), int iUIIndex, MenuScreenId ColumnId, DataIndex UNUSED_PARAM(iUniqueId), int iColumn )
{
	CScaleformMenuHelper::SET_DATA_SLOT_NO_LABEL(PM_COLUMNS(iColumn), iUIIndex, ColumnId + PREF_OPTIONS_THRESHOLD, -1
		, EMPTY_ROW_STYLE, 0, 0 );
	return true;
}

void UIFriendsClanPage::SetSelectedItem(int iColumn, int iSlotIndex, DataIndex playerIndex)
{
	UIDataPageBase::SetSelectedItem(iColumn, iSlotIndex, playerIndex);
	m_pDynamicOwner->SetCrewIndexAndPrepareCard(playerIndex,false);
}

//////////////////////////////////////////////////////////////////////////

bool UIGeneralCrewPageBase::IsReady() const
{
	return !m_pDynamicOwner || m_pDynamicOwner->IsReady();
}

bool UIGeneralCrewPageBase::FillEmptyScaleformSlots( LocalDataIndex UNUSED_PARAM(iItemIndex), int iUIIndex, MenuScreenId ColumnId, DataIndex UNUSED_PARAM(iUniqueId), int iColumn )
{
	CScaleformMenuHelper::SET_DATA_SLOT_NO_LABEL(PM_COLUMNS(iColumn), iUIIndex, ColumnId + PREF_OPTIONS_THRESHOLD, -1
		, EMPTY_ROW_STYLE, 0, 0 );
	return true;
}

void UIGeneralCrewPageBase::SetSelectedItem(int iColumn, int iSlotIndex, DataIndex playerIndex)
{
	UIDataPageBase::SetSelectedItem(iColumn, iSlotIndex, playerIndex);
	m_pDynamicOwner->SetCrewIndexAndPrepareCard(playerIndex,false);
}


//////////////////////////////////////////////////////////////////////////

bool UIRequestPage::SetData(rlClanId PrimaryClan, CCrewMenu* pOwner)
{
	// because this data is cached elsewhere... lie a little bit.
	m_Data.m_iTotalResultSize = NETWORK_CLAN_INST.GetReceivedJoinRequestCount();
	m_Data.m_iResultSize	  = Max(0, int(m_Data.m_iTotalResultSize) - (GetPagedIndex() * UI_PAGE_SIZE));
	m_Data.m_RequestStatus.ForceSucceeded();
	m_pDynamicOwner = pOwner;


	if( PrimaryClan == RL_INVALID_CLAN_ID )
	{
		m_Data.ForceSucceeded();
		return true;
	}
	for(int i=0; i < m_Data.m_iResultSize; ++i)
	{
		const rlClanJoinRequestRecieved* pInvite = NETWORK_CLAN_INST.GetReceivedJoinRequest(i);
		if( uiVerifyf(pInvite, "Not sure why we don't have an invite #%i, we thought there were %i total.", i, m_Data.m_iTotalResultSize) )
			m_Data.m_Data[i] = pInvite->m_RequestId;
	}

	return true;
}

bool UIRequestPage::FillScaleformEntry( LocalDataIndex iItemIndex, int iUIIndex, MenuScreenId ColumnId, DataIndex iUniqueId, int iColumn, bool bIsUpdate )
{
	const rlClanJoinRequestRecieved* pRequest = NETWORK_CLAN_INST.GetReceivedJoinRequest(iUniqueId);
	if( !uiVerifyf(pRequest, "Couldn't get a request for index %i!", iUniqueId) )
		return false;

	uiAssertf(pRequest->m_RequestId == m_Data[iItemIndex], "Couldn't get a matching request! Expected an id of %" I64FMT "i, but got %" I64FMT "i!", m_Data[iItemIndex], pRequest->m_RequestId);

	CGxtString pszName;
	pszName.Append(pRequest->m_Player.m_Gamertag);

	if( m_Completed.IsSet(iItemIndex) )
	{
		pszName.Append(" - ");
		if( m_AcceptedOrRejected.IsSet(iItemIndex) )
			pszName.Append( TheText.Get("CRW_REQACCEPTED") );
		else
			pszName.Append( TheText.Get("CRW_REQDENIED") );
	}

	CScaleformMenuHelper::SET_DATA_SLOT( PM_COLUMNS(iColumn), iUIIndex, ColumnId + PREF_OPTIONS_THRESHOLD, iUniqueId, OPTION_DISPLAY_STYLE_TEXTFIELD
		, 0, !m_Completed.IsSet(iItemIndex), pszName.GetData(), true, bIsUpdate); 
	return true;
}

//////////////////////////////////////////////////////////////////////////

// don't let this happen; since it can't?
template<> const rlClanDesc& 
UIGeneralCrewPaginator<UIRequestPage, REQUEST_CACHED_PAGES>::GetClanDesc(DataIndex UNUSED_PARAM(whichClan)) const
{
	uiAssertf(0, "Makes no sense to call GetDesc on the RequestPaginator!");
	return RL_INVALID_CLAN_DESC;
}

//////////////////////////////////////////////////////////////////////////

void UIRequestPaginator::HandleRequest(DataIndex entry, bool bAcceptedOrRejected)
{
	if( UIRequestPage* pRightType = GetPageSafe(entry) )
	{
		LocalDataIndex ldi = entry - pRightType->GetLowestEntry();
		if( !pRightType->m_Completed.GetAndSet(ldi) )
		{
			pRightType->m_AcceptedOrRejected.Set(ldi, bAcceptedOrRejected);
			UpdateIndex(entry);
		}
	}
}

bool UIRequestPaginator::IsHandled(DataIndex entry) const
{
	if( const UIRequestPage* pRightType = GetPageSafe(entry) )
	{
		LocalDataIndex ldi = entry - pRightType->GetLowestEntry();
		return pRightType->m_Completed.IsSet(ldi);
	}

	return false;
}

const rlGamerHandle& UIRequestPaginator::GetGamerHandle(DataIndex entry) const
{
	return GetRequest(entry).m_Player.m_GamerHandle;
}

const rlClanJoinRequestRecieved& UIRequestPaginator::GetRequest(DataIndex entry) const
{
	if( GetPageSafe(entry) )
	{
		const rlClanJoinRequestRecieved* pRequest = NETWORK_CLAN_INST.GetReceivedJoinRequest(entry);
		if( uiVerifyf(pRequest, "Couldn't get a request for index %i!", entry) )
			return *pRequest;
	}

	uiAssertf(0, "Couldn't find a request for index: %i. You get garbage.", entry);
	static const rlClanJoinRequestRecieved invalid_Request;
	return invalid_Request;
}

//////////////////////////////////////////////////////////////////////////

bool UIInvitePage::SetData(rlClanId UNUSED_PARAM(PrimaryClan), CCrewMenu* pOwner)
{
	// because this data is cached elsewhere... lie a little bit.
	m_Data.m_iTotalResultSize = NETWORK_CLAN_INST.GetInvitesReceivedCount();
	m_Data.m_iResultSize	  = Max(0, int(m_Data.m_iTotalResultSize) - (GetPagedIndex() * UI_PAGE_SIZE));
	m_Data.m_RequestStatus.ForceSucceeded();
	m_pDynamicOwner = pOwner;

	for(int i=0; i < m_Data.m_iResultSize; ++i)
	{
		const rlClanInvite* pInvite = NETWORK_CLAN_INST.GetReceivedInvite(i);
		if( uiVerifyf(pInvite, "Not sure why we don't have an invite #%i, we thought there were %i total.", i, m_Data.m_iTotalResultSize) )
			m_Data.m_Data[i] = pInvite->m_Id;
	}
	return true;
}

bool UIInvitePage::FillScaleformEntry( LocalDataIndex iItemIndex, int iUIIndex, MenuScreenId ColumnId, DataIndex iUniqueId, int iColumn, bool bIsUpdate )
{
	if((u32)iUniqueId >= NETWORK_CLAN_INST.GetInvitesReceivedCount())
	{
		return false;
	}

	const rlClanInvite* pInvite = NETWORK_CLAN_INST.GetReceivedInvite(iUniqueId);
	if( !uiVerifyf(pInvite, "Couldn't get an invite for index %i!", iUniqueId) )
		return false;

	uiAssertf(pInvite->m_Id == m_Data[iItemIndex], "Couldn't get a matching invite! Expected an id of %" I64FMT "i, but got %" I64FMT "i!", m_Data[iItemIndex], pInvite->m_Id);

	CGxtString pszName;
	pszName.Append(pInvite->m_Clan.m_ClanName);

	if( m_Completed.IsSet(iItemIndex) )
	{
		pszName.Append(" - ");
		if( m_AcceptedOrRejected.IsSet(iItemIndex) )
			pszName.Append( TheText.Get("CRW_INVACCEPTED") );
		else
			pszName.Append( TheText.Get("CRW_INVDECLINED") );
	}

	CScaleformMenuHelper::SET_DATA_SLOT( PM_COLUMNS(iColumn), iUIIndex, ColumnId + PREF_OPTIONS_THRESHOLD, iUniqueId, OPTION_DISPLAY_STYLE_TEXTFIELD
		, 0, !m_Completed.IsSet(iItemIndex), pszName.GetData(), true, bIsUpdate); 
	return true;
}

//////////////////////////////////////////////////////////////////////////

// specialize this
template<> const rlClanDesc& 
	UIGeneralCrewPaginator<UIInvitePage, INVITE_CACHED_PAGES>::GetClanDesc(DataIndex whichInvite) const
{
	if( GetPageSafe(whichInvite) )
	{
		const rlClanInvite* pInvite = NETWORK_CLAN_INST.GetReceivedInvite(whichInvite);
		if( uiVerifyf(pInvite, "Couldn't get an invite for index %i!", whichInvite) )
			return pInvite->m_Clan;
	}

	return RL_INVALID_CLAN_DESC;
}

const rlClanInvite* UIInvitePaginator::GetInvite(DataIndex whichInvite)
{
	// unfortunately, you may have to use template specialization because of our different types
	if( GetPageSafe(whichInvite) )
	{
		const rlClanInvite* pInvite = NETWORK_CLAN_INST.GetReceivedInvite(whichInvite);
		if( uiVerifyf(pInvite, "Couldn't get an invite for index %i!", whichInvite) )
			return pInvite;
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////

void UIInvitePaginator::HandleRequest(DataIndex entry, bool bAcceptedOrRejected)
{
	if( UIInvitePage* pRightType = GetPageSafe(entry) )
	{
		LocalDataIndex ldi = entry - pRightType->GetLowestEntry();
		if( !pRightType->m_Completed.GetAndSet(ldi) )
		{
			pRightType->m_AcceptedOrRejected.Set(ldi, bAcceptedOrRejected);
			UpdateIndex(entry);
		}
	}
}

bool UIInvitePaginator::IsHandled(DataIndex entry) const
{
	if( const UIInvitePage* pRightType = GetPageSafe(entry) )
	{
		LocalDataIndex ldi = entry - pRightType->GetLowestEntry();
		return pRightType->m_Completed.IsSet(ldi);
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////

bool UIOpenClanPage::SetData(rlClanId UNUSED_PARAM(PrimaryClan), CCrewMenu* pOwner)
{
	m_pDynamicOwner = pOwner;
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	return rlClan::GetOpenClans(localGamerIndex, GetPagedIndex()
		, m_Data.m_Data.GetElements(), m_Data.GetMaxSize()
		, &m_Data.m_iResultSize, &m_Data.m_iTotalResultSize
		, &m_Data.m_RequestStatus);
}

bool UIOpenClanPage::FillScaleformEntry( LocalDataIndex iItemIndex, int iUIIndex, MenuScreenId ColumnId, DataIndex iUniqueId, int iColumn, bool bIsUpdate )
{
	CGxtString pszName;
#if __BANK
	if( CCrewMenu::sm_bDbgAppendNumbers )
	{
		char number[32];
		formatf(number, 32, "L%d UI%d D%d  ", iItemIndex, iUIIndex, iUniqueId);
		pszName.Set(number);
	}
#endif
	pszName.Append(m_Data[iItemIndex].m_ClanName);

	if( CScaleformMenuHelper::SET_DATA_SLOT( PM_COLUMNS(iColumn), iUIIndex, ColumnId + PREF_OPTIONS_THRESHOLD, iUniqueId, OPTION_DISPLAY_STYLE_TEXTFIELD
		, 0, true, pszName.GetData(), false, bIsUpdate) )
	{
		CScaleformMgr::AddParamBool(false); // never any boxes
		CScaleformMgr::AddParamBool(false); // never checked off
		CScaleformMgr::EndMethod();
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
CCrewMenu::CCrewMenu(CMenuScreen& owner) : CMenuBase(owner)
, m_eVariant(CrewVariant_Mine)
, m_pCrewDetails(NULL)
, m_LBPaginator(&owner)
, m_RequestPaginator(&owner)
, m_OpenClanPaginator(&owner)
, m_InvitePaginator(&owner)
, m_FriendsPaginator(&owner)
, m_myPrimaryClan(RL_INVALID_CLAN_ID)
, m_myPrimaryClanIndex(-1)
, m_bInSubMenu(false)
, m_bIgnoreNextLayoutChangedBecauseItIsShit(false)
, m_bVisibilityControlledByWarningScreen(false)
, m_bLastFetchHadErrors(false)
{
#if __BANK
	m_pDebugWidget = NULL;
#endif
	Clear();
	m_NetworkClanDelegate.Bind(this, &CCrewMenu::OnEvent_NetworkClan);
	m_rlClanDelegate.Bind(this, &CCrewMenu::OnEvent_rlClan);

	NETWORK_CLAN_INST.AddDelegate(&m_NetworkClanDelegate);
	rlClan::AddDelegate(&m_rlClanDelegate);

	for(int i=0;i<CrewVariant_MAX;++i)
		m_Paginators[i] = NULL;

	m_Paginators[CrewVariant_Friends]		= &m_FriendsPaginator;
	m_Paginators[CrewVariant_Invites]		= &m_InvitePaginator;
	m_Paginators[CrewVariant_OpenClans]		= &m_OpenClanPaginator;
	m_Paginators[CrewVariant_Requests]		= &m_RequestPaginator;
	m_Paginators[CrewVariant_Leaderboards]	= &m_LBPaginator;

	// no common base for init, so can't do this smart

	// set up the visibility handler
	datCallback showHideCB( MFA1(CCrewMenu::HandleContextMenuVisibility), this, 0, true);
	datCallback noResultsCB( MFA1(CCrewMenu::ShowNoResultsCB), this, 0, true);

	m_LBPaginator.Init(			MENU_UNIQUE_ID_CREW_LIST,	MENU_UNIQUE_ID_CREW_LEADERBOARDS,	showHideCB,		noResultsCB,	 this);
	m_FriendsPaginator.Init(	MENU_UNIQUE_ID_CREW_LIST,	MENU_UNIQUE_ID_CREW_FRIENDS,		showHideCB,		noResultsCB,	 this);
	m_OpenClanPaginator.Init(	MENU_UNIQUE_ID_CREW_LIST,	MENU_UNIQUE_ID_CREW_ROCKSTAR,		showHideCB,		noResultsCB,	 this);
	m_InvitePaginator.Init(		MENU_UNIQUE_ID_CREW_LIST,	MENU_UNIQUE_ID_CREW_INVITES,		showHideCB,		noResultsCB,	 this);
	m_RequestPaginator.Init(	MENU_UNIQUE_ID_CREW_LIST,	MENU_UNIQUE_ID_CREW_REQUEST,		showHideCB,		noResultsCB,	 this);

	if( uiVerifyf(GetContextMenu(), "Crew menu expected a context menu!") )
		GetContextMenu()->SetShowHideCB(this);

	CPauseMenu::GetScreenData(MENU_UNIQUE_ID_CREW_REQUEST).GetContextMenu()->SetShowHideCB(this);
}

void CCrewMenu::ReinitPaginators()
{
	for(int i=0;i<CrewVariant_MAX;++i)
	{
		if(m_Paginators[i])
			m_Paginators[i]->ResetPages();
	}

	PopulateMenuForCurrentVariant();
}

CCrewMenu::~CCrewMenu()
{
	NETWORK_CLAN_INST.RemoveDelegate(&m_NetworkClanDelegate);
	rlClan::RemoveDelegate(&m_rlClanDelegate);

#if __BANK
	if( m_pDebugWidget )
		m_pDebugWidget->Destroy();
#endif
	for(int i=0; i < CrewVariant_MAX;++i)
	{
		if( m_Paginators[i] )
			m_Paginators[i]->Shutdown();
	}
}

void CCrewMenu::Clear(bool bDontClearEverything/* =false */)
{
	m_eState = CrewState_MembershipFetch;
	m_iSelectedCrewIndex = -1;
	m_curSlotBits.Reset();
	m_compareSlotBits.Reset();
	m_bIgnoreNextLayoutChangedBecauseItIsShit = false;
	if( !bDontClearEverything )
	{
		m_MyMemberships.Clear();
	}
	m_CardTimeout.Zero();
	m_bLastFetchHadErrors = false;

	// shift out of the submenu if we clear.
	if( m_bInSubMenu )
		CPauseMenu::MENU_SHIFT_DEPTH(kMENUCEPT_SHALLOWER,false,true);
	if( GetContextMenu()->IsOpen() )
		GetContextMenu()->CloseMenu();

	m_bInSubMenu = false;
	UpdateColumnModeContexts(Col_SingleCard);
	m_LastBusyColumn = PM_COLUMN_MAX;

}

bool CCrewMenu::IsSCAvailable(int gamerIndex, bool bCheckConnectionsToo ) const
{
	if( !rlClan::IsServiceReady(gamerIndex) )
		return false;
	if( !rlClan::IsFullServiceReady(gamerIndex) )
		return false;

	if( bCheckConnectionsToo )
	{
		if(!IsCloudAvailable())
			return false;
	}

	return CLiveManager::GetSocialClubMgr().IsConnectedToSocialClub();
}

bool CCrewMenu::IsCloudAvailable() const
{
	if( !NetworkInterface::IsOnlineRos() )
		return false;

	if( !NetworkInterface::IsCloudAvailable())
		return false;

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Blind ghosting functions.
// just hands off functionality if it CrewDetails is set up

void CCrewMenu::LoseFocus()
{
	if( m_pCrewDetails )
	{
		m_pCrewDetails->LoseFocus();
	}

	ClearCrewDetails();
	
	for(int i=0; i < CrewVariant_MAX;++i)
	{
		if( m_Paginators[i] )
			m_Paginators[i]->LoseFocus();
	}
	SetColumnBusy(PM_COLUMN_MAX);
	SUIContexts::Deactivate(UIATSTRINGHASH("HIDE_ACCEPTBUTTON",0x14211b54));
	m_bLastFetchHadErrors = false;
	m_bInSubMenu = false;

	if(GetContextMenu() && GetContextMenu()->IsOpen())
	{
		GetContextMenu()->CloseMenu();
	}
}

void CCrewMenu::Render(const PauseMenuRenderDataExtra* renderData)
{
	if( m_pCrewDetails )
	{
		m_pCrewDetails->Render(renderData);
		return;
	}
}

void CCrewMenu::SetCrewIndexAndPrepareCard( int iNewIndex, bool bUpdatePaginator )
{
	m_iSelectedCrewIndex = iNewIndex;
	if( bUpdatePaginator )
	{
		SUIContexts::SetActive(COMPARE_CONTEXT, m_eVariant != CrewVariant_Requests && (m_MyMemberships.m_iResultSize>0||m_eVariant==CrewVariant_Leaderboards) );
		m_bInSubMenu = true;
		if( m_Paginators[m_eVariant] )
			m_Paginators[m_eVariant]->UpdateItemSelection(iNewIndex, true);
	}

	if( m_eState > CrewState_WaitingForClanData && m_eState <= CrewState_Done)
		m_eState = CrewState_WaitingForClanData;

	if( m_hasWarning == 1 ) // only showing a single warning panel == last crew was disbanded
		ClearWarningColumn();

	if( m_bInSubMenu )
	{
		bool bShouldSet = false;
		// need this scoped so we can 'leak' HIDE_ACCEPTBUTTON
		{
			SUICONTEXT_AUTO_PUSH();
			BuildContexts();
			bShouldSet = !GetContextMenu()->WouldShowMenu();
		}
		SUIContexts::SetActive( UIATSTRINGHASH("HIDE_ACCEPTBUTTON",0x14211b54), bShouldSet);
		CPauseMenu::RedrawInstructionalButtons();
	}
	
	if( ShouldShowCrewCards() || m_eVariant != CrewVariant_Leaderboards)
	{
		//uiAssertf(m_eState >= CrewState_WaitingForClanData, "Just jumped our state ahead from %i in SetCrewIndexAndPrepareCard!", m_eState );
		m_curSlotBits.Reset();
		m_compareSlotBits.Reset();

		// if this is the first time they've bumped the index, increment the time forward so it responds faster
		if( !m_CardTimeout.IsStarted() )
			m_CardTimeout.Start( CARD_FIRST_TIME_OFFSET );
		else
			m_CardTimeout.Start();
	}
}

bool CCrewMenu::TriggerEvent(MenuScreenId MenuId, s32 iUniqueId )
{
	if( m_pCrewDetails )
	{
		return m_pCrewDetails->TriggerEvent(MenuId, iUniqueId);
	}

	if( m_Owner.FindItemIndex(MenuId) != -1 )
	{
		SetCrewIndexAndPrepareCard(m_iSelectedCrewIndex);
	}
	
	return false;
}

bool CCrewMenu::InitScrollbar()
{
	if( m_eVariant == CrewVariant_Leaderboards )
	{
		CMenuScreen& rData = CPauseMenu::GetScreenData(MENU_UNIQUE_ID_CREW_LEADERBOARDS);
		atFixedBitSet16 cloneMe( rData.m_ScrollBarFlags );

		bool bWidthOne = ShouldShowCrewCards() || GetContextMenu()->IsOpen();
		cloneMe.Set( Width_1,  bWidthOne);
		cloneMe.Set( Width_2, !bWidthOne);
		cloneMe.Set( InitiallyInvisible, m_pCrewDetails ? true : false);
		rData.INIT_SCROLL_BAR_Generic(cloneMe, rData.depth);

		return true;
	}
	return false;
}

bool CCrewMenu::HandleContextOption(atHashWithStringBank whichOption)
{
	if( m_pCrewDetails )
	{
		return m_pCrewDetails->HandleContextOption(whichOption);
	}

	CWarningMessage& rMessage = CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage();
	NetAction&		 rAction  = CPauseMenu::GetDynamicPauseMenu()->GetNetAction();

	if( !uiVerifyf(m_iSelectedCrewIndex >= 0 && m_iSelectedCrewIndex < GetResultsForCurrentVariant(), "Currently selected Crew Index %i is out of range [0, %i)!", m_iSelectedCrewIndex, GetResultsForCurrentVariant() ) )
		return false;

	if( m_eVariant == CrewVariant_Requests )
	{
		const rlClanJoinRequestRecieved& received = m_RequestPaginator.GetRequest(m_iSelectedCrewIndex);
		if( CContextMenuHelper::HandlePlayerContextOptions(whichOption, received.m_Player.m_GamerHandle) )
			return true;

		if( whichOption == ATSTRINGHASH("REQUEST_ACCEPT",0x7841d906) )
		{
			uiDisplayf("User is accepting invite request of %s/%s", received.m_Player.m_Nickname, received.m_Player.m_Gamertag);
			if( NETWORK_CLAN_INST.AcceptRequest(m_iSelectedCrewIndex) )
			{
				rAction.SetExternal(NetworkClan::OP_ACCEPT_REQUEST, "CRW_INVITE", received.m_Player.m_Gamertag, m_MyMemberships[m_myPrimaryClanIndex].m_Clan.m_ClanName);
				m_RequestPaginator.HandleRequest(m_iSelectedCrewIndex, true);
			}
			return true;
		}

		if( whichOption == ATSTRINGHASH("REQUEST_REJECT",0x44770c4) )
		{
			if( NETWORK_CLAN_INST.RejectRequest(m_iSelectedCrewIndex))
			{
				rAction.SetExternal(NetworkClan::OP_REJECT_REQUEST, "CRW_DECLINE", received.m_Player.m_Gamertag);
				m_RequestPaginator.HandleRequest(m_iSelectedCrewIndex, false);
			}
			return true;
		}

		uiAssertf(0, "Not sure what other options are expected inside the request menu!");
		return true;
	}

	const rlClanDesc& curClanDesc = GetClanDescForCurrentVariant(m_iSelectedCrewIndex);
	m_CachedDesc = curClanDesc;

	//////////////////////////////////////////////////////////////////////////
	if( whichOption == ATSTRINGHASH("JOIN_CREW",0x65778ca3) 
		|| whichOption == ATSTRINGHASH("INVITE_ACCEPT",0x376e39bf))
	{
		if( int(m_MyMemberships.m_iResultSize) == m_MyMemberships.GetMaxSize())
		{
			uiDisplayf("User is already in too many clans (%i) and isn't allowed to join %" I64FMT "i, '%s'", m_MyMemberships.GetMaxSize(), curClanDesc.m_Id, curClanDesc.m_ClanName);
			CWarningMessage::Data messageData;
			messageData.m_TextLabelBody = "CLAN_ERR_CLAN_MAX_JOIN_COUNT_EXCEEDED";
			messageData.m_iFlags = FE_WARNING_OK;
			messageData.m_acceptPressed = datCallback(MFA(CMenuScreen::BackOutOfWarningMessageToContextMenu), (datBase*)&m_Owner);

			rMessage.SetMessage(messageData);
			return false;
		}
		else
		{
			// show  "leaving session" confirmation message if joining first crew (therefore changing primary) (but only in MP)
			if(int(m_MyMemberships.m_iResultSize) == 0 && NetworkInterface::IsNetworkOpen())
			{
				CWarningMessage::Data messageData;
				messageData.m_TextLabelBody = "CRW_JOINCONFIRM";
				messageData.m_FirstSubString.Set( curClanDesc.m_ClanName );
				messageData.m_iFlags = FE_WARNING_YES_NO;
				messageData.m_acceptPressed = datCallback(MFA(CCrewMenu::JoinSelectedClanCB), this);
				messageData.m_declinePressed = datCallback(MFA(CMenuScreen::BackOutOfWarningMessageToContextMenu), (datBase*)&m_Owner);
				uiDisplayf("User is being prompted if they want to join clan %" I64FMT "i, '%s'", curClanDesc.m_Id, curClanDesc.m_ClanName);

				rMessage.SetMessage(messageData);
			}
			else
			{
				// already in a crew so short to callback
				JoinSelectedClanCB(); 
			}
		}
	}
	//////////////////////////////////////////////////////////////////////////
	else if( whichOption == ATSTRINGHASH("REQUEST_INVITE",0x9e3cfd9) ) 
	{
		RequestSelectedClanCB();
	}
	//////////////////////////////////////////////////////////////////////////
	else if( whichOption == ATSTRINGHASH("LEAVE_CREW",0x4264d0c4) )
	{
		bool isPrimary;
		if( uiVerifyf(AmIInThisClan(curClanDesc.m_Id, &isPrimary), "Really gotta be IN a crew to even consider leaving it..."))
		{
			CWarningMessage::Data messageData;
			messageData.m_TextLabelBody = isPrimary&&NetworkInterface::IsNetworkOpen()? "CRW_LEAVEPRIMCONFIRM" : "CRW_LEAVECONFIRM";	//   "leaving session"  message if leaving your primary
			messageData.m_FirstSubString.Set( curClanDesc.m_ClanName );
			messageData.m_iFlags = FE_WARNING_YES_NO;
			messageData.m_acceptPressed = datCallback(MFA(CCrewMenu::LeaveSelectedClanCB), this);
			messageData.m_declinePressed = datCallback(MFA(CMenuScreen::BackOutOfWarningMessageToContextMenu), (datBase*)&m_Owner);
			uiDisplayf("User is being prompted if they want to leave clan %" I64FMT "i, '%s'", curClanDesc.m_Id, curClanDesc.m_ClanName);

			rMessage.SetMessage(messageData);
		}

	}
	//////////////////////////////////////////////////////////////////////////
	else if( whichOption == ATSTRINGHASH("INVITE_DECLINE",0x93d714fb) )
	{
		if( uiVerifyf(m_eVariant == CrewVariant_Invites, "Shouldn't have the ability to decline an invite while not in the invitation menu..."))
		{
			uiDisplayf("User is decline invitation to %" I64FMT "i, '%s'", curClanDesc.m_Id, curClanDesc.m_ClanName);

			const rlClanInvite* pInvite = m_InvitePaginator.GetInvite(m_iSelectedCrewIndex);
			
			if(uiVerifyf(pInvite, "Couldn't get an invite object for invite #%i!", m_iSelectedCrewIndex) )
			{
				NETWORK_CLAN_INST.RejectInvite(m_iSelectedCrewIndex);
				m_InvitePaginator.HandleRequest(m_iSelectedCrewIndex,false);
				rAction.SetExternal(NetworkClan::OP_REJECT_INVITE, "CRW_DECLINE", curClanDesc.m_ClanName);
			}
		}

	}
	//////////////////////////////////////////////////////////////////////////
	else if( whichOption == ATSTRINGHASH("SET_PRIMARY",0xf79bd8fc) )
	{
		// show "leaving session" confirmation message if changing your primary crew
		if( NetworkInterface::IsNetworkOpen() )
		{
			CWarningMessage::Data messageData;
			messageData.m_TextLabelBody = "CRW_PRIMARYCONFIRM";
			messageData.m_FirstSubString.Set( curClanDesc.m_ClanName );
			messageData.m_iFlags = FE_WARNING_YES_NO;
			messageData.m_acceptPressed = datCallback(MFA(CCrewMenu::SetPrimaryClanCB), this);
			messageData.m_declinePressed = datCallback(MFA(CMenuScreen::BackOutOfWarningMessageToContextMenu), (datBase*)&m_Owner);
			uiDisplayf("User is being prompted if they want to set primary clan %" I64FMT "i, '%s'", curClanDesc.m_Id, curClanDesc.m_ClanName);

			rMessage.SetMessage(messageData);
		}
		else
		{
			SetPrimaryClanCB();
		}

		
	}
	//////////////////////////////////////////////////////////////////////////
	else if( whichOption == ATSTRINGHASH("VIEW_MEMBERS",0xacdfc1e8) )
	{
		EnterViewMembers(curClanDesc.m_Id);
	}
	else
	{
		uiAssertf(0, "No idea how to handle context option %s (0x%08x)", whichOption.TryGetCStr() ? whichOption.TryGetCStr() : "-no hash-", whichOption.GetHash() );
		return false;
	}
	return true;
}

void CCrewMenu::EnterViewMembers(const rlClanId& clanId)
{
	const CMenuScreen& CrewData = CPauseMenu::GetScreenData( MENU_UNIQUE_ID_CREW );
	if( uiVerifyf(CrewData.HasDynamicMenu(), "Pretty sure we need the CREW menu to have a dynamic menu type" ))
	{
		m_eState = CrewState_InsideCrewMenu;
		CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod( "FRONTEND_CONTEXT_PRESS", MENU_UNIQUE_ID_CREW_LIST + PREF_OPTIONS_THRESHOLD );

		m_pCrewDetails = CrewData.GetDynamicMenu();
		SUIContexts::SetActive(IN_CREW_DETAILS, true);

		CPlayerCardManager& cardMgr = SPlayerCardManager::GetInstance();			
		CClanPlayerCardDataManager* pCard = static_cast<CClanPlayerCardDataManager*>(cardMgr.GetDataManager(CPlayerCardManager::CARDTYPE_CLAN));
		bool bIsPrimary = false;
		AmIInThisClan(clanId, &bIsPrimary);
		pCard->SetClan( clanId, bIsPrimary );

		m_pCrewDetails->InitScrollbar();
		m_pCrewDetails->Populate(MENU_UNIQUE_ID_CREW_OPTIONS);
		m_pCrewDetails->TriggerEvent(MENU_UNIQUE_ID_CREW_OPTIONS, 0);
		m_pCrewDetails->OnNavigatingContent(true);

		if(m_Paginators[m_eVariant])
			m_Paginators[m_eVariant]->LoseFocus();

		m_bIgnoreNextLayoutChangedBecauseItIsShit = true;
		// these are wiped away in the hand off
		ClearColumnVisibilities();
	}
}

CContextMenu* CCrewMenu::GetContextMenu()
{
	const CCrewMenu* pThis = this;
	return const_cast<CContextMenu*>(pThis->GetContextMenu());
}

const CContextMenu* CCrewMenu::GetContextMenu() const
{
	if( m_pCrewDetails )
		return m_pCrewDetails->GetContextMenu();

	if( m_eVariant == CrewVariant_Requests )
		return CPauseMenu::GetScreenData(MENU_UNIQUE_ID_CREW_REQUEST).GetContextMenu();

	return CMenuBase::GetContextMenu();
}

void CCrewMenu::BuildContexts()
{
	if( m_pCrewDetails )
	{
		m_pCrewDetails->BuildContexts();
		return;
	}

	if( GetResultsForCurrentVariant() <= 0 )
		SUIContexts::Activate(UIATSTRINGHASH("NO_RESULTS",0xabe8912c));

	else if (m_iSelectedCrewIndex >= 0 && m_iSelectedCrewIndex < GetResultsForCurrentVariant() )
	{

		if(m_eVariant == CrewVariant_Requests )
		{
			const rlGamerHandle& rGamerHandle = m_RequestPaginator.GetGamerHandle(m_iSelectedCrewIndex);
			CContextMenuHelper::BuildPlayerContexts(rGamerHandle);
			if( m_RequestPaginator.IsHandled(m_iSelectedCrewIndex) )
				SUIContexts::Activate( UIATSTRINGHASH("CREW_RequestHandled",0xb4bedc1d) );
		}
		else
		{
			bool bIsPrimary;
			const rlClanDesc& thisClan = GetClanDescForCurrentVariant(m_iSelectedCrewIndex);
			if( AmIInThisClan( thisClan.m_Id, &bIsPrimary ) )
			{
				SUIContexts::Activate(UIATSTRINGHASH("IN_CREW",0x17d59a33));
				if( !bIsPrimary )
					SUIContexts::Activate(UIATSTRINGHASH("SET_AS_PRIMARY_CREW",0xdb0fc0d1));
			}
			// handle weirdness where there's an empty clan (as they'll be cleaned up anyway)
			else if( thisClan.IsValid() && (thisClan.m_MemberCount > 0||thisClan.m_IsSystemClan) )
			{
				if( thisClan.m_IsOpenClan )
					SUIContexts::Activate(UIATSTRINGHASH("CAN_JOIN",0x1f40b6a0));
				else
					SUIContexts::Activate(UIATSTRINGHASH("CAN_REQUEST",0xd8d60d85));
			}
			else
				SUIContexts::Activate("HAS_NO_MEMBERS");
		}
	}
}

bool CCrewMenu::Populate(MenuScreenId newScreenId)
{
	if( m_pCrewDetails )
		return m_pCrewDetails->Populate(newScreenId);

	// handle when first GainFocus this way
	if( newScreenId == MENU_UNIQUE_ID_CREWS )
	{
		//Clear any cache being used in script.
		CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr().ClearCachedDisplayData();

		Clear( !m_ClearTimeout.IsComplete(CLEAR_COOLDOWN,false));
		m_ClearTimeout.Start();
		SetVariant( GetVariantForMenuScreen( m_Owner.MenuItems[m_Owner.FindOnscreenItemIndex(0,MENU_OPTION_ACTION_LINK)].MenuUniqueId) );
	}
	else
	{
		Variant var = GetVariantForMenuScreen(newScreenId);
		if(var != CrewVariant_MAX && m_Paginators[var] )
			m_Paginators[var]->UpdateItemSelection(0,false);
	}

	Update();

	// optionally, if the data's already ready, we could populate instantly instead of waiting on the Update() loop
	return true;
}

//////////////////////////////////////////////////////////////////////////

void CCrewMenu::PrepareInstructionalButtons( MenuScreenId MenuId, s32 iUniqueId)
{
	if( m_pCrewDetails )
		m_pCrewDetails->PrepareInstructionalButtons( MenuId, iUniqueId);
}

bool CCrewMenu::CheckNetworkLoss()
{
#if __BANK
	if( s_gFakeNetworkLoss )
		return true;
#endif
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	return ( localGamerIndex == -1 || !rlClan::IsServiceReady(localGamerIndex) );
}

void CCrewMenu::HandleNetworkLoss()
{
	if( m_eState != CrewState_ErrorOffline )
	{
		m_eState = CrewState_ErrorOffline;
		PopulateWithMessage(TheText.Get("NOT_CONNECTED"), "WARNING_NOT_CONNECTED_TITLE" );
	}
}

void CCrewMenu::LeaveCrewDetails()
{
	CCrewDetailMenu* pMenu = verify_cast<CCrewDetailMenu*>(m_pCrewDetails);
	if( pMenu )
	{
		pMenu->LoseFocusLite();
		pMenu->ShutdownPaginator();
		pMenu->ClearWarningColumn();
	}

	ClearCrewDetails();

	m_bIgnoreNextLayoutChangedBecauseItIsShit = true;

	CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod( "FRONTEND_CONTEXT_PRESS", MENU_UNIQUE_ID_CREW_OPTIONS + PREF_OPTIONS_THRESHOLD );
	//m_Owner.PromptContextMenu();
	m_eState = CrewState_Done;
}


bool CCrewMenu::UpdateInput(s32 eInput)
{
	// never let square ever, EVER do anything (to crews)
	if( m_pCrewDetails && eInput != PAD_SQUARE)
	{
		// if the User has pressed B, back the shit out!
		if( eInput == PAD_CIRCLE )
		{
			LeaveCrewDetails();
			CPauseMenu::PlaySound("BACK");
			return true;
		}
		else
			return m_pCrewDetails->UpdateInput(eInput);
	}

#if KEYBOARD_MOUSE_SUPPORT
	if(CMousePointer::IsMouseWheelDown())
	{
		eInput = PAD_DPADDOWN;
		CPauseMenu::PlayInputSound(FRONTEND_INPUT_DOWN);
		CMousePointer::ConsumeInput();
	}

	if(CMousePointer::IsMouseWheelUp())
	{
		eInput = PAD_DPADUP;
		CPauseMenu::PlayInputSound(FRONTEND_INPUT_UP);
		CMousePointer::ConsumeInput();
	}
#endif // KEYBOARD_MOUSE_SUPPORT

	if( eInput == PAD_NO_BUTTON_PRESSED || GetContextMenu()->IsOpen() )
		return false;

	// no SC? Join here!
	switch( m_eState )
	{
		case CrewState_ErrorNoCloud:
		case CrewState_Refreshing:
			if( eInput == PAD_CIRCLE || eInput == PAD_LEFTSHOULDER1 || eInput == PAD_RIGHTSHOULDER1 || eInput == PAD_DPADUP || eInput == PAD_DPADDOWN )
				return false;
			// none shall pass
			return true;

		case CrewState_NeedToJoinSC:
		case CrewState_ErrorOffline:
		case CrewState_NoPrivileges:
		
			// the string table says INPUT_FRONTEND_ACCEPT... not sure how to tease that out to the proper version.
			if( eInput == PAD_CROSS )
			{
				if( m_eState == CrewState_NeedToJoinSC)
				{
					SocialClubMenu::SetTourHash(ATSTRINGHASH("General",0x6b34fe60));
					CPauseMenu::EnterSocialClub();
				}
				else if( m_eState == CrewState_ErrorOffline )
				{
					CLiveManager::ShowSigninUi();
				}
				return true;
			}
	
			if( eInput == PAD_CIRCLE || eInput == PAD_LEFTSHOULDER1 || eInput == PAD_RIGHTSHOULDER1 )
				return false;

			// block all other inputs
			return true;

		//////////////////////////////////////////////////////////////////////////
		// shut up, compiler
		default:
			break;
	}

	if( m_bInSubMenu && m_Paginators[m_eVariant] )
	{
		if( m_Paginators[m_eVariant]->UpdateInput(eInput) )
			return true;
	}

	// nah. you can't go into the middle column if we haven't finished stuff
	if( CPauseMenu::IsNavigatingContent() && eInput == PAD_CROSS )
	{
		// nope. you can't navigate to an empty list.
		if( m_eState <= CrewState_WaitingForMembership || GetResultsForCurrentVariant() <= 0 )
			return true;
	}

	if( eInput == PAD_SQUARE )
	{
		if( m_pCrewDetails==NULL && SUIContexts::IsActive(COMPARE_CONTEXT) )
		{
			ColumnMode oldColumnMode = m_eRightColumnMode;
			UpdateColumnModeContexts( ColumnMode((m_eRightColumnMode+1)%Col_MAX_MODES) );
			if( oldColumnMode != m_eRightColumnMode )
				CPauseMenu::PlayInputSound(FRONTEND_INPUT_X);

			uiAssertf(m_eState >= CrewState_WaitingForClanData, "Just jumped our state ahead from %i in SetCrewIndexAndPrepareCard!", m_eState );
			m_eState = CrewState_WaitingForClanData;
			m_curSlotBits.Reset();
			m_compareSlotBits.Reset();
			SetThirdColumnVisible(false);
			ClearWarningColumn();
			if( ShouldShowCrewCards() )
				SetColumnBusy(PM_COLUMN_RIGHT);
			CPauseMenu::RedrawInstructionalButtons();
		}
		return true;
	}

	return false;
}

void CCrewMenu::GoBack()
{
	CPauseMenu::PlayInputSound(FRONTEND_INPUT_ACCEPT);
	CPauseMenu::MENU_SHIFT_DEPTH(kMENUCEPT_SHALLOWER,false,true);
}

void CCrewMenu::OfferAccountUpgrade()
{
	CPauseMenu::PlayInputSound(FRONTEND_INPUT_ACCEPT);
	CPauseMenu::MENU_SHIFT_DEPTH(kMENUCEPT_SHALLOWER,false,true);
	CLiveManager::ShowAccountUpgradeUI();
}

void CCrewMenu::JoinSelectedClanCB()
{
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (!IsSCAvailable(localGamerIndex) )
	{
		// not sure what happen if you disconnect while in the warning screen...
		return;
	}

	const rlClanDesc& curClanDesc = m_CachedDesc;
	NetAction& rAction = CPauseMenu::GetDynamicPauseMenu()->GetNetAction();

	uiDisplayf("User is now joining clan %" I64FMT "i, '%s'", curClanDesc.m_Id, curClanDesc.m_ClanName);

	datCallback JSMPCB(MFA(CCrewMenu::JoinSuccessMakePrimaryCB), this);

	if( m_eVariant == CrewVariant_Invites )
	{
		NETWORK_CLAN_INST.AcceptInvite(m_iSelectedCrewIndex);
		m_InvitePaginator.HandleRequest(m_iSelectedCrewIndex,true);
		rAction.SetExternal( NetworkClan::OP_ACCEPT_INVITE, "CRW_JOIN", curClanDesc.m_ClanName, NULL, JSMPCB );
	}
	else
	{
		rlClan::Join( localGamerIndex, curClanDesc.m_Id, 
			rAction.Set("CRW_JOIN", curClanDesc.m_ClanName, NULL, JSMPCB ) 
			);
	}
}


void CCrewMenu::JoinSuccessMakePrimaryCB()
{
	// if this was our first join, it's ALSO de facto our primary. Skip this part.
	if( m_MyMemberships.m_iResultSize == 0 )
		return;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (!IsSCAvailable(localGamerIndex) )
	{
		// not sure what happen if you disconnect while in the warning screen...
		return;
	}

	CWarningMessage& rMessage = CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage();
	const rlClanDesc& curClanDesc = m_CachedDesc;

	CWarningMessage::Data messageData;
	messageData.m_TextLabelBody = "CRW_JOINSUCCESS";
	if (NetworkInterface::IsGameInProgress())
	{
		messageData.m_TextLabelSubtext = "CRW_JOINPRIMARY_MP";
	}
	else
	{
		messageData.m_TextLabelSubtext = "CRW_JOINPRIMARY";
	}
	
	messageData.m_FirstSubString.Set( curClanDesc.m_ClanName );
	messageData.m_iFlags = FE_WARNING_YES_NO;
	messageData.m_acceptPressed = datCallback(MFA(CCrewMenu::SetPrimaryClanCB), this);
	messageData.m_declinePressed = datCallback(MFA(CMenuScreen::BackOutOfWarningMessageToContextMenu), (datBase*)&m_Owner);
	uiDisplayf("User is being prompted if they want to make clan %" I64FMT "i, '%s', their primary.", curClanDesc.m_Id, curClanDesc.m_ClanName);

	rMessage.SetMessage(messageData);
}


void CCrewMenu::RequestSelectedClanCB()
{
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (!IsSCAvailable(localGamerIndex) )
	{
		// not sure what happen if you disconnect while in the warning screen...
		return;
	}

	const rlClanDesc& curClanDesc = m_CachedDesc;
	NetAction& rAction = CPauseMenu::GetDynamicPauseMenu()->GetNetAction();

	uiDisplayf("User is now joining clan %" I64FMT "i, '%s'", curClanDesc.m_Id, curClanDesc.m_ClanName);

	rlClan::JoinRequest( localGamerIndex, curClanDesc.m_Id, 
		rAction.Set("CRW_REQ", curClanDesc.m_ClanName ) 
		);
}

void CCrewMenu::SetPrimaryClanCB()
{
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (!IsSCAvailable(localGamerIndex) )
	{
		// not sure what happen if you disconnect while in the warning screen...
		return;
	}

	const rlClanDesc& curClanDesc = m_CachedDesc;
	NetAction& rAction = CPauseMenu::GetDynamicPauseMenu()->GetNetAction();

	uiDisplayf("User is setting their primary clan to %" I64FMT "i, '%s'", curClanDesc.m_Id, curClanDesc.m_ClanName);					
	rlClan::SetPrimaryClan( localGamerIndex, curClanDesc.m_Id
		, rAction.Set("CRW_PRIMARY", curClanDesc.m_ClanName) 
	);
}

void CCrewMenu::LeaveSelectedClanCB()
{
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (!IsSCAvailable(localGamerIndex) )
	{
		// not sure what happen if you disconnect while in the warning screen...
		return;
	}

	const rlClanDesc& curClanDesc = m_CachedDesc;
	NetAction& rAction = CPauseMenu::GetDynamicPauseMenu()->GetNetAction();

	uiDisplayf("User is now leaving clan %" I64FMT "i, '%s'", curClanDesc.m_Id, curClanDesc.m_ClanName);

	rlClan::Leave( localGamerIndex, curClanDesc.m_Id, 
		rAction.Set("CRW_LEAVE", curClanDesc.m_ClanName)
		);
}

void CCrewMenu::SetVariant( Variant newVariant )
{
	//always pop the old one (because the default is currently 'Mine')
	SUIContexts::Deactivate(GetVariantName(m_eVariant));
	m_eVariant = newVariant;
	SUIContexts::Activate(GetVariantName(m_eVariant));
}

void CCrewMenu::UpdateColumnModeContexts(ColumnMode newMode /* = Col_MAX_MODES */)
{
	if(newMode != Col_MAX_MODES)
		m_eRightColumnMode = newMode;

	// don't let us try and be in the weird mode
	if( m_eRightColumnMode == Col_LBStats && m_eVariant != CrewVariant_Leaderboards )
		m_eRightColumnMode = Col_SingleCard;

	if( m_eRightColumnMode == Col_Compare && m_MyMemberships.m_iResultSize==0 && m_eVariant == CrewVariant_Leaderboards )
		m_eRightColumnMode = Col_LBStats;

	InitScrollbar();

	SUIContexts::SetActive(UIATSTRINGHASH("CREW_SingleCard",0x92a4690f),	m_eRightColumnMode==Col_SingleCard);
	SUIContexts::SetActive(UIATSTRINGHASH("CREW_Compare",0x743a01c2),		m_eRightColumnMode==Col_Compare);
	SUIContexts::SetActive(UIATSTRINGHASH("CREW_Stats",0x918d7af8),			m_eRightColumnMode==Col_LBStats);
}


void CCrewMenu::LayoutChanged( MenuScreenId iPreviousLayout, MenuScreenId iNewLayout, s32 iUniqueId, eLAYOUT_CHANGED_DIR eDir )
{
	// layout changes from the scaleform set these up. Better handle it properly
	// We need to do this if we're coming back from the leaderboard, else if we put up a warning, we won't hide the columns properly (B* 2102125)
	if( m_bIgnoreNextLayoutChangedBecauseItIsShit 
		|| (iNewLayout.GetValue() != MENU_UNIQUE_ID_CREW_LIST && !m_bVisibilityControlledByWarningScreen))
	{
		m_columnVisibility.Set(PM_COLUMN_LEFT,				true);
		m_columnVisibility.Set(PM_COLUMN_MIDDLE,			true);
		m_columnVisibility.Set(CREW_COLUMN_COMPARE_LEFT,	false);
		m_columnVisibility.Set(CREW_COLUMN_COMPARE_RIGHT,	false);
		m_columnVisibility.Set(CREW_COLUMN_MAIN_CARD,		false);
		m_columnVisibility.Set(CREW_COLUMN_LB_RANKS,		false);
	}

	// Hack to fix B*1832079 - Script is hammering CommandSetPMWarningScreenActive(true) 2x on every movement through online menu
	// even though there's no warning active.
	CPauseMenu::SetPauseMenuWarningScreenActive(false);

	if( m_bIgnoreNextLayoutChangedBecauseItIsShit )
	{
		m_bIgnoreNextLayoutChangedBecauseItIsShit = false;
		CScaleformMgr::ForceCollectGarbage(CPauseMenu::GetContentMovieId());
		if( !m_pCrewDetails )
		{
			if( m_eVariant == CrewVariant_Mine )
			{
				PopulateMenuForCurrentVariant();
				CScaleformMenuHelper::SET_COLUMN_HIGHLIGHT(PM_COLUMN_MIDDLE, m_iSelectedCrewIndex);
			}

			SetCrewIndexAndPrepareCard(m_iSelectedCrewIndex);
			SetThirdColumnVisible( !CMenuBase::IsShowingWarningColumn()	);
		}
		return;
	}

	if( m_pCrewDetails )
	{
		m_pCrewDetails->LayoutChanged(iPreviousLayout, iNewLayout, iUniqueId, eDir);
		return;
	}

	if( m_eState == CrewState_InsideCrewMenu || m_eState == CrewState_NeedToJoinSC )
	{
		return;
	}

	if( iNewLayout.GetValue() == MENU_UNIQUE_ID_CREW_LIST)
	{
		SetCrewIndexAndPrepareCard( iUniqueId );
		return;
	}


	if( IsShowingWarningColumn() && m_eState > CrewState_Done )
	{
		SHOW_COLUMN(PM_COLUMN_MIDDLE, false);
	}

	m_bInSubMenu = false;
	SUIContexts::SetActive(COMPARE_CONTEXT, false);
	m_Owner.DrawInstructionalButtons(iNewLayout, iUniqueId);

	//	Displayf("LAYOUT CHANGED ON CREW MENU. Prev: %i, New: %i, Unique: %i", iPreviousLayout, iNewLayout, iUniqueId);
	
	switch(iNewLayout.GetValue())
	{
	case MENU_UNIQUE_ID_CREWS:
		// do nothing
		ClearCrewDetails();
		return;
	case MENU_UNIQUE_ID_CREW_MINE:
	case MENU_UNIQUE_ID_CREW_ROCKSTAR:
	case MENU_UNIQUE_ID_CREW_FRIENDS:
	case MENU_UNIQUE_ID_CREW_INVITES:
	case MENU_UNIQUE_ID_CREW_REQUEST:
	case MENU_UNIQUE_ID_CREW_LEADERBOARDS:
		SetVariant( GetVariantForMenuScreen( iNewLayout ));
		break;

	default:
		// downgrading from an assert because it's firing off on menu change now
		uiErrorf("Got a new layout, %i, I don't know what to do with.", iNewLayout.GetValue());		
		return;
	}

	ClearCrewDetails();

	for(int i=0; i < CrewVariant_MAX; ++i)
	{
		if(m_Paginators[i] && i != m_eVariant )
			m_Paginators[i]->LoseFocus();
	}



	if( iPreviousLayout != MENU_UNIQUE_ID_CREW_LIST )
	{
		UpdateColumnModeContexts(m_eVariant==CrewVariant_Leaderboards ? Col_LBStats : Col_SingleCard);
		if(m_eState > CrewState_WaitingForMembership )
		{
			m_eState = CrewState_WaitingForMembership;
			m_iSelectedCrewIndex = -1;
			m_curSlotBits.Reset();
			m_compareSlotBits.Reset();
			SetThirdColumnVisible(true);

			if( m_hasWarning == 1 ) // only showing a single warning panel == last crew was disbanded
				ClearWarningColumn();
		}
	}
}

void CCrewMenu::ClearCrewDetails()
{
	m_pCrewDetails = NULL;
	SUIContexts::SetActive(IN_CREW_DETAILS, false);
}

//////////////////////////////////////////////////////////////////////////
// Matches what ScaleForm expects for Dataslot ids... more or less
enum Scaleform_CrewCardItems
{
	CrewCard_Rank,
	CrewCard_Color,
	CrewCard_Animation,
	
	CrewCard_StartOfStats,
	CrewCard_Stat1 = CrewCard_StartOfStats,
	CrewCard_Stat2,

	// no more slots, now just bitfields for tracking
	CrewCard_Type,
	CrewCard_ColumnShown,
	CrewCard_ColumnCleared,
	CrewCard_Description,
	CrewCard_Title,
	CrewCard_TitleColor,
	CrewCard_Emblem,

	CrewCard_EndOfRequired,

	// not required for the ALL_SET flag
	CrewCard_ColumnHidden = CrewCard_EndOfRequired,
	CrewCard_Animation_GaveUp,
	CrewCard_Color_GaveUp,
	CrewCard_Errors,
	CrewCard_Disbanded,

	CrewCard_TOTAL_BITS,

	CrewCard_ALL_SET = BIT(CrewCard_EndOfRequired)-1
};

bool CrewCardStatus::IsDone() const
{
	// if this becomes an issue, you'll need to the bump up the size of CrewCardStatus' base type
	CompileTimeAssert(CrewCard_TOTAL_BITS <= CrewCardStatus::NUM_BITS);

	return (GetRawBlock(0) & CrewCard_ALL_SET) == CrewCard_ALL_SET;
}

bool CrewCardStatus::IsBareMinimumSet() const
{
	//return IsSet(CrewCard_Title) && (GetRawBlock(0) & CrewCard_ALL_SET) != 0;
	const int theThings = CrewCard_ALL_SET - (BIT(CrewCard_Stat1) + BIT(CrewCard_Stat2) + BIT(CrewCard_ColumnShown));

	return (GetRawBlock(0) & theThings) == theThings;
}


void GenerateHumanReadableDate(int posixDate, CGxtString& out_Date)
{
	time_t date = static_cast<time_t>(posixDate);
	struct tm* ptm = gmtime(&date);

	// @TODO Figure out a way to localize this properly, as different countries show it different ways.
	if( ptm )
	{
		char monthId[10];
		formatf(monthId, "MONTH_%i", ptm->tm_mon+1);

		char crapDate[24];
		formatf(crapDate, " %i, %i", ptm->tm_mday, ptm->tm_year+1900);

		out_Date.Append( TheText.Get(monthId) );
		out_Date.Append(crapDate);
	}
}


//////////////////////////////////////////////////////////////////////////
int CCrewMenu::GetResultsForCurrentVariant()
{
	if(m_Paginators[m_eVariant])
		return m_Paginators[m_eVariant]->GetTotalSize();

	switch( m_eVariant )
	{
	case CrewVariant_Mine:		return m_MyMemberships.m_iResultSize;

	default:
		uiAssertf(0, "Unexpected Variant %i for GetResults!", m_eVariant);
	}

	return 0;
}

CCrewMenu::Variant CCrewMenu::GetVariantForMenuScreen(MenuScreenId id) const
{
	switch(id.GetValue())
	{
		case MENU_UNIQUE_ID_CREW_MINE:			return CrewVariant_Mine;
		case MENU_UNIQUE_ID_CREW_ROCKSTAR:		return CrewVariant_OpenClans;
		case MENU_UNIQUE_ID_CREW_FRIENDS:		return CrewVariant_Friends;
		case MENU_UNIQUE_ID_CREW_INVITES:		return CrewVariant_Invites;
		case MENU_UNIQUE_ID_CREW_REQUEST:		return CrewVariant_Requests;
		case MENU_UNIQUE_ID_CREW_LEADERBOARDS:	return CrewVariant_Leaderboards;
	}

	return CrewVariant_MAX;
}

MenuScreenId CCrewMenu::GetMenuScreenForVariant(Variant var) const
{
	switch(var)
	{
	case CrewVariant_Mine:						return MENU_UNIQUE_ID_CREW_MINE;
	case CrewVariant_OpenClans:					return MENU_UNIQUE_ID_CREW_ROCKSTAR;
	case CrewVariant_Friends:					return MENU_UNIQUE_ID_CREW_FRIENDS;
	case CrewVariant_Invites:					return MENU_UNIQUE_ID_CREW_INVITES;
	case CrewVariant_Requests:					return MENU_UNIQUE_ID_CREW_REQUEST;
	case CrewVariant_Leaderboards:				return MENU_UNIQUE_ID_CREW_LEADERBOARDS;
	default:
		break; // here to shut up the compiler
	}

	return MENU_UNIQUE_ID_INVALID;
}

UIContext CCrewMenu::GetVariantName(Variant forThis) const
{
	switch( forThis )
	{
	case CrewVariant_Mine:			return UIATSTRINGHASH("VARIANT_MINE",0xa338d1fd);
	case CrewVariant_Friends:		return UIATSTRINGHASH("VARIANT_FRIENDS",0xb9946519);
	case CrewVariant_OpenClans:		return UIATSTRINGHASH("VARIANT_ROCKSTAR",0x5e1f5ae0);
	case CrewVariant_Invites:		return UIATSTRINGHASH("VARIANT_INVITE",0xa39933a9);
	case CrewVariant_Requests:		return UIATSTRINGHASH("VARIANT_REQUESTS",0x87a91fe7);
	case CrewVariant_Leaderboards:	return UIATSTRINGHASH("VARIANT_LBS",0xd79687ec);

	default:
		uiAssertf(0, "Unexpected Variant %i for GetVariantName! Returning \"\"!", forThis);
	}

	return "";
}

const char* CCrewMenu::GetNoResultsString(Variant forThis) const
{
	switch( forThis )
	{
	case CrewVariant_Friends:		return "CRW_NOFRIENDS";
	case CrewVariant_OpenClans:		return "CRW_NOSUGGESTED";
	case CrewVariant_Invites:		return "CRW_NOINVITES";
	case CrewVariant_Requests:		return "CRW_NOREQUESTS";
	case CrewVariant_Leaderboards:	return "CRW_NOLBRESULTS";

	default:
		uiAssertf(0, "Unexpected Variant %i for GetVariantName! Returning \"\"!", forThis);
	}

	return "";
}

const netStatus& CCrewMenu::GetStatusForVariant(Variant whichVariant) const
{

	switch( whichVariant )
	{
	case CrewVariant_Mine:		return m_MyMemberships.m_RequestStatus;

	default:
		uiAssertf(0, "Unexpected Variant %i for GetStatus! Returning MyMembership's out of emergency!", m_eVariant);
	}

	return m_MyMemberships.m_RequestStatus;
}

const rlClanId CCrewMenu::GetClanIdForCurrentVariant(int index) const
{
	return GetClanDescForCurrentVariant(index).m_Id;
}

const rlClanDesc& CCrewMenu::GetClanDescForCurrentVariant(int index) const
{
	switch( m_eVariant )
	{
	case CrewVariant_Mine:			return m_MyMemberships	[index].m_Clan;
	
	case CrewVariant_Friends:		return m_FriendsPaginator.	GetClanDesc(index);
	case CrewVariant_Invites:		return m_InvitePaginator.	GetClanDesc(index);
	case CrewVariant_OpenClans:		return m_OpenClanPaginator.	GetClanDesc(index);
	case CrewVariant_Leaderboards:	return m_LBPaginator.		GetClanDesc(index);

	// because they're all currently to get into MY primary clan, just use this
	case CrewVariant_Requests:		return m_MyMemberships[m_myPrimaryClanIndex].m_Clan;
	default:
		uiAssertf(0, "Unexpected Variant %i for GetClanDescForCurrentVariant!", m_eVariant);
	}

	return RL_INVALID_CLAN_DESC;
}

bool CCrewMenu::AmIInThisClan(rlClanId clanId, bool* out_pbIsPrimary)
{
	if( uiVerifyf(m_MyMemberships.m_RequestStatus.Succeeded(), "Attempting to fetch Clan Membership but haven't received the results back!") )
	{
		for(int i=0; i < m_MyMemberships.m_iResultSize; ++i )
		{
			if( m_MyMemberships[i].m_Clan.m_Id == clanId )
			{
				if( out_pbIsPrimary )
					*out_pbIsPrimary = m_MyMemberships[i].m_IsPrimary;
				return true;
			}
		}
	}

	if( out_pbIsPrimary )
		*out_pbIsPrimary = false;
	return false;
}

void CCrewMenu::PopulateMenuForCurrentVariant()
{
	if( m_Paginators[m_eVariant] )
	{
		// nothing doin'. Handled by the paginator
		m_Paginators[m_eVariant]->UpdateItemSelection(m_iSelectedCrewIndex,m_bInSubMenu);
		return;
	}

	CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(CREW_COLUMN_RESULTS);

	if( m_eVariant != CrewVariant_Mine )
	{
		uiAssertf(0, "Unexpected Variant %i for PopulateMenuForCurrentVariant!", m_eVariant);
		return;
	}

	int memberIndex = 0;
	int iResultSize = GetResultsForCurrentVariant();
	
	for(; memberIndex < iResultSize; ++memberIndex)
	{
		const char* pszClanName = m_MyMemberships[memberIndex].m_Clan.m_ClanName;
	
		if( CScaleformMenuHelper::SET_DATA_SLOT(CREW_COLUMN_RESULTS, memberIndex, MENU_UNIQUE_ID_CREW_LIST + PREF_OPTIONS_THRESHOLD, memberIndex
			, OPTION_DISPLAY_STYLE_SLIDER, 0, true, pszClanName, false) )
		{
			CScaleformMgr::AddParamBool(true); // always show box
			CScaleformMgr::AddParamBool(m_MyMemberships[memberIndex].m_IsPrimary); // check if primary
			CScaleformMgr::EndMethod();
		}
	}

	// pad out the rest of the slots
	for(;memberIndex<m_MyMemberships.GetMaxSize();++memberIndex)
	{
		CScaleformMenuHelper::SET_DATA_SLOT(CREW_COLUMN_RESULTS, memberIndex, MENU_UNIQUE_ID_CREW_LIST + PREF_OPTIONS_THRESHOLD, -1
				, EMPTY_ROW_STYLE, 0, false, TheText.Get("CRW_EMPTY") );
	}

	ShowNoResultsCB(false);
	CScaleformMenuHelper::DISPLAY_DATA_SLOT(CREW_COLUMN_RESULTS);
}

void CCrewMenu::PopulateMenuForJoinSC()
{
	PopulateWithMessage(TheText.Get("CRW_JOINSC"), "ERROR_NO_SC_TITLE");
}

void CCrewMenu::PopulateMenuForError(Variant forThis)
{
	netStatus status;
	if( m_Paginators[forThis] )
	{
		uiAssertf(m_Paginators[forThis]->HasFailed(), "Not sure why think this has errored when it hasn't");
		status.ForceFailed(m_Paginators[forThis]->GetResultCode());
	}
	else
		status = GetStatusForVariant(forThis);

	//There are only valid error codes for failures
	const char* pszError = NetworkClan::GetErrorName(status);

	if( atStringHash(pszError) == ATSTRINGHASH("RL_CLAN_UNKNOWN",0x7769bd50) )
	{
		CNumberWithinMessage Number;
		Number.Set( status.GetResultCode() );

		char errorString[MAX_CHARS_IN_MESSAGE];
		CMessages::InsertNumbersAndSubStringsIntoString(TheText.Get("RL_CLAN_UNKNOWN_EXT"), 
			&Number, 1, 
			NULL, 0, 
			errorString, MAX_CHARS_IN_MESSAGE);

		PopulateWithMessage( errorString, "MO_SC_ERR_HEAD", false );
	}
	else
	{
		PopulateWithMessage( TheText.Get(pszError), "MO_SC_ERR_HEAD", false );
	}
}

void CCrewMenu::PopulateWithMessage(const char* pError, const char* pTitle, bool bFullBlockage)
{
	SUIContexts::Activate( "WARNING_ACTIVE" );

	SHOW_COLUMN(PM_COLUMN_LEFT,			!bFullBlockage);
	SHOW_COLUMN(PM_COLUMN_MIDDLE,			false);
	SHOW_COLUMN(CREW_COLUMN_COMPARE_LEFT,	false);
	SHOW_COLUMN(CREW_COLUMN_COMPARE_RIGHT,false);
	SHOW_COLUMN(CREW_COLUMN_LB_RANKS,		false);
	SHOW_COLUMN(CREW_COLUMN_MAIN_CARD,	false);
	if( bFullBlockage )
		ShowColumnWarning(PM_COLUMN_LEFT, 3, pError, TheText.Get(pTitle));
	else
		ShowColumnWarning(PM_COLUMN_MIDDLE, 2, pError, TheText.Get(pTitle));
	SUIContexts::Activate(UIATSTRINGHASH("HIDE_ACCEPTBUTTON",0x14211b54));
	SetColumnBusy(PM_COLUMN_MAX);
	if( m_bInSubMenu )
		CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod( "MENU_SHIFT_DEPTH", -1 );
	m_bInSubMenu = false;
	if( GetContextMenu()->IsOpen() )
		GetContextMenu()->CloseMenu();

	m_bVisibilityControlledByWarningScreen = bFullBlockage;
}

void CCrewMenu::UnPopulateMessage()
{
	if( SUIContexts::IsActive( "WARNING_ACTIVE" ) )
	{
		SUIContexts::Deactivate( "WARNING_ACTIVE" );
	}

	ClearWarningColumn_EX(PM_COLUMN_LEFT_MIDDLE);
	SUIContexts::Deactivate(UIATSTRINGHASH("HIDE_ACCEPTBUTTON",0x14211b54));
}
netStatus::StatusCode CCrewMenu::CheckSuccess(const netStatus& thisStatus, const char* OUTPUT_ONLY(pszTypeofCheck)) const
{
	if( thisStatus.Succeeded() )
		return netStatus::NET_STATUS_SUCCEEDED;

	// if NOT pending, it's counted as an error, because we totally expected it to be success or pending!
	if(!thisStatus.Pending())
	{
		uiErrorf("NetStatus for %s was neither Success or pending, was state %i, result code %i!", pszTypeofCheck, thisStatus.GetStatus(), thisStatus.GetResultCode());
		return netStatus::NET_STATUS_FAILED;
	}

	return netStatus::NET_STATUS_PENDING;
}

netStatus::StatusCode CCrewMenu::CheckSuccessForCurrentVariant(const char* pszTypeofCheck) const
{
	if(m_Paginators[m_eVariant])
	{
		if( m_Paginators[m_eVariant]->IsReady() )
		{
			if( m_Paginators[m_eVariant]->HasFailed())
				return netStatus::NET_STATUS_FAILED;
			else
				return netStatus::NET_STATUS_SUCCEEDED;
		}


		// no failed state... for now?
		return netStatus::NET_STATUS_PENDING;
	}

	return CheckSuccess(GetStatusForCurrentVariant(),pszTypeofCheck);
}

bool CCrewMenu::SetColumnBusy( int whichColumn, bool bForceIt )
{
	// don't show busy columns if we're in the Updating state (but we're not trying to hide it)
	if( m_EventCooldown.IsStarted() && whichColumn != PM_COLUMN_MAX)
		return false;

	if( m_LastBusyColumn != whichColumn || bForceIt)
	{
		if(!bForceIt)
			m_LastBusyColumn = whichColumn;

		if( !GetContextMenu()->IsOpen() && whichColumn != PM_COLUMN_MAX )
			CScaleformMenuHelper::SET_BUSY_SPINNER( static_cast<PM_COLUMNS>(whichColumn), true);
		else
			CScaleformMenuHelper::SET_BUSY_SPINNER(PM_COLUMN_RIGHT, false);
		return true;
	}

	return false;
}

bool CCrewMenu::ShouldBlockEntrance(MenuScreenId UNUSED_PARAM(iMenuScreenBeingFilledOut))
{
	switch( m_eState )
	{
		case CrewState_ErrorFetching:
		case CrewState_NeedToJoinSC:
		case CrewState_ErrorOffline:
		case CrewState_NoPrivileges:
			return true;
		default:
			return false;
	}
}

bool CCrewMenu::ShouldPlayNavigationSound(bool UNUSED_PARAM(bNavUp))
{
	if(m_bInSubMenu && m_Paginators[m_eVariant])
		return m_Paginators[m_eVariant]->IsReady();

	return true;
}

bool CCrewMenu::IsReady() const
{
	return m_MyMemberships.m_RequestStatus.Succeeded() || m_MyMemberships.m_RequestStatus.Failed();
}

void CCrewMenu::Update()
{
	if( m_EventCooldown.IsStarted() )
	{
		if( m_EventCooldown.IsComplete(EVENT_COOLDOWN) )
		{
			m_EventCooldown.Zero();
			uiDebugf1("Acting on delayed clear!");
			Clear();
			ReinitPaginators();
			UnPopulateMessage();
		}
		else if( m_eState != CrewState_Refreshing )
		{
			// have to set the state AFTER we clear it. Otherwise there's weirdness.
			m_eState = CrewState_Refreshing;
			PopulateWithMessage(TheText.Get("CRW_REFRESHING_CNT"), "CRW_REFRESHING",false );
		}
	}

	if(m_Paginators[m_eVariant])
		m_Paginators[m_eVariant]->Update( m_pCrewDetails == NULL );

	bool bHasLostNetwork = CheckNetworkLoss();

	if( m_pCrewDetails )
	{
		if(bHasLostNetwork)
		{
			HandleNetworkLoss();
		}
		else
		{
			m_pCrewDetails->Update();
		}
		return;
	}

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	// NEED TO HANDLE NOT BEING ONLINE HERE
	if(bHasLostNetwork)
	{
		HandleNetworkLoss();
		return;
	}
#if RSG_ORBIS
	else if(!CLiveManager::HasPlatformSubscription())
	{
		if( m_eState != CrewState_NoSubscription )
		{
			m_eState = CrewState_NoSubscription;
			CWarningMessage::Data newMessage;
			newMessage.m_TextLabelHeading = "CWS_WARNING";
			newMessage.m_TextLabelBody = "HUD_PSPLUS2";
			newMessage.m_iFlags = FE_WARNING_YES_NO;
			newMessage.m_bCloseAfterPress = true;
			newMessage.m_acceptPressed = datCallback( MFA(CCrewMenu::OfferAccountUpgrade), this);
			newMessage.m_declinePressed = datCallback( MFA(CCrewMenu::GoBack), this);
			CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().SetMessage(newMessage);
			return;
		}
	}
#endif
	#if RSG_DURANGO || RSG_XENON || RSG_PC
	// Xbox TRC TCR 094 - use every gamer index
	else if( !CLiveManager::CheckUserContentPrivileges(GAMER_INDEX_EVERYONE))
	#elif RSG_ORBIS || RSG_PS3
	// Playstation R4063 - use the local gamer index
	else if( !CLiveManager::CheckUserContentPrivileges(GAMER_INDEX_LOCAL))
	#endif
	{
		if( m_eState != CrewState_NoPrivileges )
		{
#if RSG_XBOX
			// Display the system UI.
			CLiveManager::ResolvePlatformPrivilege(NetworkInterface::GetLocalGamerIndex(), rlPrivileges::PrivilegeTypes::PRIVILEGE_USER_CREATED_CONTENT, true);
#endif
			m_eState = CrewState_NoPrivileges;
			CWarningMessage::Data newMessage;
			newMessage.m_TextLabelHeading = "PM_CREWS";
			newMessage.m_TextLabelBody = "ERROR_NOT_PRIVILEGE";
			newMessage.m_iFlags = FE_WARNING_OK;
			newMessage.m_bCloseAfterPress = true;
			newMessage.m_acceptPressed = datCallback( MFA(CCrewMenu::GoBack), this);
			CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().SetMessage(newMessage);
			return;
		}
	}
	else if( !IsCloudAvailable() || m_bLastFetchHadErrors )
	{
		if( m_eState != CrewState_ErrorNoCloud )
		{
			m_eState = CrewState_ErrorNoCloud;
			PopulateWithMessage(TheText.Get("HUD_CLOUDFAILMSG"), "MO_SC_ERR_HEAD", false );
			return;
		}
	}
	// check if the Clan or Social Club privilege has been revoked
	else if( rlRos::GetCredentials(localGamerIndex).IsValid() 
		&& (!rlRos::GetCredentials(localGamerIndex).HasPrivilege(RLROS_PRIVILEGEID_SOCIAL_CLUB)
		 || !rlRos::GetCredentials(localGamerIndex).HasPrivilege(RLROS_PRIVILEGEID_CLAN) )
		 )
	{
		if( m_eState != CrewState_NoPrivileges )
		{
			m_eState = CrewState_NoPrivileges;
			CWarningMessage::Data newMessage;
			newMessage.m_TextLabelHeading = "PM_CREWS";
			newMessage.m_TextLabelBody = "ERROR_NOT_PRIVILEGE_SC";
			newMessage.m_iFlags = FE_WARNING_OK;
			newMessage.m_bCloseAfterPress = true;
			newMessage.m_acceptPressed = datCallback( MFA(CCrewMenu::GoBack), this);
			CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().SetMessage(newMessage);
			return;
		}
	}
	// back online!
	else if(m_eState == CrewState_ErrorOffline  || m_eState == CrewState_ErrorNoCloud )
	{
		UnPopulateMessage();
		Clear();
	}

	switch( m_eState )
	{
		//////////////////////////////////////////////////////////////////////////
	case CrewState_MembershipFetch:
		{
			// Check if invalid credentials (not signed up for SC)
			if( !IsSCAvailable(localGamerIndex) )
			{
				PopulateMenuForJoinSC();

				// Need to add this because SHOW_COLUMN is gated and the list populate on layoutchanged forces visibility
				// making the gate out of sync.

				if (CPauseMenu::GetCurrentScreenData().GetDynamicMenu() && CPauseMenu::GetCurrentScreenData().GetDynamicMenu()->GetMenuScreenId() == MENU_UNIQUE_ID_CREW_LEADERBOARDS)
					SHOW_COLUMN(PM_COLUMN_LEFT,true);

				m_eState = CrewState_NeedToJoinSC;
				break;
			}
			else if(!CLiveManager::GetSocialClubMgr().IsOnlinePolicyUpToDate())
			{
				PopulateWithMessage(TheText.Get("CRW_UPDATESC"), "ERROR_NO_SC_TITLE");
				m_eState = CrewState_NeedToJoinSC;
				break;
			}

			UpdateColumnModeContexts(m_eVariant==CrewVariant_Leaderboards ? Col_LBStats : Col_SingleCard);
			SetThirdColumnVisible(false);

			// Fetch MY memberships
			if( m_MyMemberships.m_RequestStatus.GetStatus() == netStatus::NET_STATUS_NONE )
			{
				if( !rlClan::GetMine(localGamerIndex, m_MyMemberships.m_Data.GetElements(), m_MyMemberships.GetMaxSize(), &m_MyMemberships.m_iResultSize, NULL, &m_MyMemberships.m_RequestStatus))
					m_MyMemberships.m_RequestStatus.ForceFailed();
			}

			// Fetch friendship data
			CPauseMenu::GetDynamicPauseMenu()->GetFriendClanData().Init();


			m_eState = CrewState_WaitingForAllData;
		}
		break;

		//////////////////////////////////////////////////////////////////////////
	case CrewState_WaitingForAllData:
		{
			switch( CheckSuccess( GetStatusForVariant(CrewVariant_Mine),		"Initial My Membership Fetch") )
			{
			case netStatus::NET_STATUS_FAILED:
				{
					PopulateMenuForError(CrewVariant_Mine);
					m_eState = CrewState_ErrorFetching;
					break;
				}
			case netStatus::NET_STATUS_SUCCEEDED:
				{
					DoDataFixup();

					if (m_eVariant == CrewVariant_Leaderboards)
						PopulateMenuForCurrentVariant();

					m_eState = CrewState_WaitingForMembership;
					break;
				}

			default:
				SetColumnBusy(PM_COLUMN_MIDDLE_RIGHT);

			}
		}
		break;

		//////////////////////////////////////////////////////////////////////////
	case CrewState_WaitingForMembership:
		{
			bool bGoOn = false;
			switch( CheckSuccessForCurrentVariant("Crew Status Request") )
			{
			case netStatus::NET_STATUS_SUCCEEDED:
				{
					if( GetResultsForCurrentVariant() > 0 )
						m_iSelectedCrewIndex = 0;

					PopulateMenuForCurrentVariant();
					bGoOn = true;
					m_eState = CrewState_WaitingForClanData;
				}
				break;

				// error, it's not success or pending, so something must be off
			case netStatus::NET_STATUS_FAILED:
				PopulateMenuForError(m_eVariant);
				m_eState = CrewState_Done;
				break;

			default:
				// just hang out and wait 'til it finishes.
				SetColumnBusy(PM_COLUMN_MIDDLE_RIGHT);
				break;
			}

			if(!bGoOn)
				break;
		}

		//////////////////////////////////////////////////////////////////////////
	case CrewState_WaitingForClanData:
		{
			if( HandleCrewCard() )
				m_eState = CrewState_Done;
		}
		break;
		//////////////////////////////////////////////////////////////////////////
	case CrewState_Done:
	case CrewState_InsideCrewMenu:
	case CrewState_ErrorFetching:
	case CrewState_NeedToJoinSC:
	case CrewState_ErrorOffline:
	case CrewState_NoPrivileges:
	case CrewState_ErrorNoCloud:
	case CrewState_Refreshing:
		if( !m_Paginators[m_eVariant] || m_Paginators[m_eVariant]->IsReady())
		{
			if (m_eState == CrewState_NeedToJoinSC)
				SHOW_COLUMN(PM_COLUMN_LEFT,false);

			SetColumnBusy(PM_COLUMN_MAX);
		}
		break;



		//////////////////////////////////////////////////////////////////////////
	default:
		uiAssertf(false, "Unhandled crew menu state %i!", m_eState);
		break;
	}
}

void CCrewMenu::OnEvent_NetworkClan(const NetworkClan::NetworkClanEvent& evt)
{
	uiDebugf1("Received a clan event '%s' with status %i", NetworkClan::GetOperationName(evt.m_opType), evt.m_statusRef.GetStatus());
	if( evt.m_statusRef.Canceled() )
		return;

	switch(evt.m_opType)
	{
		case NetworkClan::OP_LEAVE_CLAN:
		case NetworkClan::OP_REFRESH_MINE:
		case NetworkClan::OP_SET_PRIMARY_CLAN:
		case NetworkClan::OP_REFRESH_JOIN_REQUESTS_RECEIVED:
		case NetworkClan::OP_REFRESH_INVITES_RECEIVED:
		case NetworkClan::OP_REJECT_INVITE:
		case NetworkClan::OP_ACCEPT_INVITE:
		case NetworkClan::OP_CANCEL_INVITE:
		//case NetworkClan::OP_ACCEPT_REQUEST:
		//case NetworkClan::OP_REJECT_REQUEST:
			uiDebugf1("Time WAS %i, but now it's %i", m_EventCooldown.GetTime(), m_EventCooldown.GetTimeFunc() );
			m_EventCooldown.Start();
			break;

		default: 
			uiDebugf1("But ignored it.");
			// ignored!
			break;
	}
}

void CCrewMenu::OnEvent_rlClan(const rlClanEvent& evt)
{
	uiDebugf1("Received a rlclan event '%s'", evt.GetName());
	switch (evt.GetId())
	{
		case RLCLAN_EVENT_JOINED:
		case RLCLAN_EVENT_LEFT:
		case RLCLAN_EVENT_KICKED:
		//case RLCLAN_EVENT_SYNCHED_MEMBERSHIPS:
		case RLCLAN_EVENT_PRIMARY_CLAN_CHANGED:
		case RLCLAN_EVENT_NOTIFY_DESC_CHANGED:
		case RLCLAN_EVENT_NOTIFY_JOIN_REQUEST:
		case RLCLAN_EVENT_INVITE_RECEIVED:
		{
			uiDebugf1("Time WAS %i, but now it's %i", m_EventCooldown.GetTime(), m_EventCooldown.GetTimeFunc() );
			m_EventCooldown.Start();
			break;
		}

		default: 
			// ignored!
			uiDebugf1("But ignored it.");
			break;
	}
}


bool CCrewMenu::HandleCrewCard()
{
	if( m_eVariant == CrewVariant_Leaderboards && !ShouldShowCrewCards() )
	{
		SetThirdColumnVisible(true);
		InitScrollbar();
		return true;
	}

	// error state, boooo
	if( m_iSelectedCrewIndex < 0 || m_iSelectedCrewIndex >= GetResultsForCurrentVariant() )
	{
		bool bShown = false;
		FillOutCrewCard(bShown, RL_INVALID_CLAN_ID, GetContextMenu()->IsOpen());
		SHOW_COLUMN(PM_COLUMN_RIGHT, bShown);
		m_iSelectedCrewIndex = -1;
		return true;
	}

	// don't immediately request the cards to spare networking
	bool bItsTime = m_CardTimeout.IsComplete(CARD_MOVEMENT_INTERVAL, false);
	if( bItsTime )
	{
		if( m_CardTimeout.IsStarted())
			uiDisplayf("-------------- CARD TIMEOUT EXHAUSTED REQUESTING DATA ----------------");
		m_CardTimeout.Zero();
	}
	
	const rlClanDesc& curDesc = GetClanDescForCurrentVariant(m_iSelectedCrewIndex);
	const rlClanId curId = curDesc.m_Id;

	bool bNeedCompareMode = m_eRightColumnMode == Col_Compare && m_myPrimaryClan != RL_INVALID_CLAN_ID;
	PM_COLUMNS primaryColumn = bNeedCompareMode ? CREW_COLUMN_COMPARE_LEFT : CREW_COLUMN_MAIN_CARD;
	PM_COLUMNS secondaryColumn = CREW_COLUMN_COMPARE_RIGHT;
	
	bool bShownColumn = false;
	bool bPrimarySuccess = FillOutCrewCard(bShownColumn, curId, GetContextMenu()->IsOpen(), &m_curSlotBits, primaryColumn, bNeedCompareMode, bItsTime);
	SHOW_COLUMN(primaryColumn, bShownColumn);

	// double this up for ease of comparing below
	CrewCardStatus* pCompareStatus = &m_curSlotBits;
	bool bSecondarySuccess = bPrimarySuccess;
	if( bNeedCompareMode )
	{
		pCompareStatus = &m_compareSlotBits;
		bSecondarySuccess = FillOutCrewCard(bShownColumn, m_myPrimaryClan, GetContextMenu()->IsOpen(), &m_compareSlotBits, secondaryColumn, true, bItsTime);
		SHOW_COLUMN(secondaryColumn, bShownColumn);
	}

	// check for errors
	m_bLastFetchHadErrors = m_curSlotBits.IsSet(CrewCard_Errors) || pCompareStatus->IsSet(CrewCard_Errors);

	// both done
	if( bPrimarySuccess && bSecondarySuccess)
	{
		SetColumnBusy(PM_COLUMN_MAX);
		if( m_curSlotBits.IsSet(CrewCard_Disbanded) )
		{
			ShowColumnWarning_EX(PM_COLUMN_RIGHT, "CRW_DISBANDED_MSG", "CRW_DISBANDED_TITLE");
			SHOW_COLUMN(secondaryColumn, false);
			SUIContexts::Activate(UIATSTRINGHASH("HIDE_ACCEPTBUTTON",0x14211b54));
			return true;
		}
		else if( m_curSlotBits.IsDone() && pCompareStatus->IsDone())
			return true;
	}
	// Primary, not secondary
	else if( bPrimarySuccess && !bSecondarySuccess )
		SetColumnBusy(PM_COLUMN_EXTRA4);
	// secondary, not primary
	else if( !bPrimarySuccess && bSecondarySuccess )
		SetColumnBusy(PM_COLUMN_EXTRA3);
	// neither
	else
		SetColumnBusy(PM_COLUMN_RIGHT);

	return false;	
}

void CCrewMenu::SetStates(bool bBaseVis, bool& bMainCardVis, bool& bLeftCompare, bool& bRightCompare, bool& bLBRanks)
{
	bMainCardVis = bBaseVis && m_eRightColumnMode == Col_SingleCard && m_curSlotBits.IsSet(CrewCard_ColumnShown);
	bLeftCompare = bBaseVis && m_eRightColumnMode == Col_Compare	&& m_curSlotBits.IsSet(CrewCard_ColumnShown);
	bRightCompare= bBaseVis && m_eRightColumnMode == Col_Compare	&& m_compareSlotBits.IsSet(CrewCard_ColumnShown);
	bLBRanks	 = bBaseVis && m_eRightColumnMode == Col_LBStats	&& m_LBPaginator.IsReady(); // maaay be unnecessary?
}

void CCrewMenu::SetThirdColumnVisibleOnly(bool bVisibility)
{
	bool bMainCardVis, bLeftCompare, bRightCompare, bLBRanks;
	SetStates(bVisibility, bMainCardVis, bLeftCompare, bRightCompare, bLBRanks);
	SHOW_COLUMN(CREW_COLUMN_MAIN_CARD,		bMainCardVis);
	SHOW_COLUMN(CREW_COLUMN_COMPARE_LEFT,	bLeftCompare);
	SHOW_COLUMN(CREW_COLUMN_COMPARE_RIGHT,	bRightCompare);
	SHOW_COLUMN(CREW_COLUMN_LB_RANKS,		bLBRanks);
}

void CCrewMenu::SetThirdColumnVisible(bool bVisibility)
{
	CScaleformMovieWrapper& content = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);

	bool bMainCardVis, bLeftCompare, bRightCompare, bLBRanks;
	SetStates(bVisibility, bMainCardVis, bLeftCompare, bRightCompare, bLBRanks);

#define SET_VISIBLE(column, vis) \
	content.AddParam(column);\
	content.AddParam(vis);\
	m_columnVisibility.Set(column, vis);

	if( content.BeginMethod("SHOW_AND_CLEAR_COLUMNS") )
	{
		SET_VISIBLE(CREW_COLUMN_MAIN_CARD,		bMainCardVis);
		SET_VISIBLE(CREW_COLUMN_COMPARE_LEFT,	bLeftCompare);
		SET_VISIBLE(CREW_COLUMN_COMPARE_RIGHT,	bRightCompare);
		content.EndMethod();
	}
	SHOW_COLUMN(CREW_COLUMN_LB_RANKS, bLBRanks);
}

void CCrewMenu::HandleContextMenuVisibility(bool bVisibility)
{
	// skip these steps if we're about to enter the crew details menu

	if( !m_pCrewDetails )
	{
		if (!CPauseMenu::GetPauseMenuWarningScreenActive())
		{
			InitScrollbar();
			SetThirdColumnVisibleOnly(bVisibility);
		}
	}

	if( m_LastBusyColumn != PM_COLUMN_MAX )
	{
		// if we're hiding the columns, hide the spinner too
		if( !bVisibility )
			SetColumnBusy(PM_COLUMN_MAX, true);
		else
			SetColumnBusy(m_LastBusyColumn, true);
	}
}

void CCrewMenu::ShowNoResultsCB(bool bVisibility)
{
	if(!bVisibility)
	{
		ClearWarningColumn_EX(PM_COLUMN_MIDDLE_RIGHT);
		SUIContexts::Deactivate(UIATSTRINGHASH("HIDE_ACCEPTBUTTON",0x14211b54));
		return;
	}

	SUIContexts::Activate(UIATSTRINGHASH("HIDE_ACCEPTBUTTON",0x14211b54));

	int MenuIndex = m_Owner.FindItemIndex(GetMenuScreenForVariant(m_eVariant));
	if( uiVerifyf(MenuIndex != -1, "Couldn't find an XML item with MENU_UNIQUE_ID_CREW_INVITES for me to UPDATE!") )
	{
		CMenuItem& curItem = m_Owner.MenuItems[MenuIndex];

		if(m_Paginators[m_eVariant] && m_Paginators[m_eVariant]->HasFailed() )
		{
			ShowColumnWarning_EX(PM_COLUMN_MIDDLE_RIGHT, "HUD_CLOUDFAILMSG", "PCARD_SYNC_ERROR_TITLE");
		}
		else
			ShowColumnWarning_EX(PM_COLUMN_MIDDLE_RIGHT, GetNoResultsString(m_eVariant), curItem.cTextId.GetHash());
	}
}



int SortMemberships(const void* va, const void* vb)
{
	const rlClanMembershipData* a = reinterpret_cast<const rlClanMembershipData*>(va);
	const rlClanMembershipData* b = reinterpret_cast<const rlClanMembershipData*>(vb);

	// primary memberships to the top.
	// B* 1129370: Screw the primary clan! Alphabetical order über alles!
	/*
	if( a->m_IsPrimary )
		return -1;
	if( b->m_IsPrimary )
		return 1;
	*/
	return strcmp(a->m_Clan.m_ClanName, b->m_Clan.m_ClanName );
}

void CCrewMenu::DoDataFixup()
{
	// PREPARE MY MEMBERSHIP
	// (put my primary clan on top)
	qsort(&m_MyMemberships.m_Data[0], m_MyMemberships.m_iResultSize, sizeof(m_MyMemberships.m_Data[0]), &SortMemberships);

	m_myPrimaryClan = RL_INVALID_CLAN_ID;
	m_myPrimaryClanIndex = -1;
	for(int i=0; i < m_MyMemberships.m_iResultSize; ++i)
	{
		if(m_MyMemberships[i].m_IsPrimary)
		{
			m_myPrimaryClanIndex = i;
			m_myPrimaryClan = m_MyMemberships[i].m_Clan.m_Id;
			break;
		}
	}

	uiAssertf(m_myPrimaryClan != RL_INVALID_CLAN_ID || m_MyMemberships.m_iResultSize == 0, "Expected user to be in a primary clan, but he's not in any of his %i!", m_MyMemberships.m_iResultSize);


	SUIContexts::SetActive(UIATSTRINGHASH("CREW_NoMembership",0x3645f515), m_MyMemberships.m_iResultSize==0);

	// UPDATE ANY CREW INVITE NOTIFICATIONS
	UpdateItemCount(MENU_UNIQUE_ID_CREW_INVITES, NETWORK_CLAN_INST.GetInvitesReceivedCount());
	UpdateItemCount(MENU_UNIQUE_ID_CREW_REQUEST, NETWORK_CLAN_INST.GetReceivedJoinRequestCount());

	m_RequestPaginator.SetData(m_myPrimaryClan);
}

void CCrewMenu::UpdateItemCount(MenuScreenId itemToFind, u32 iCount)
{
	CMenuScreen& owner = GetOwner();
	int onScreenIndex = 0;
	int MenuIndex = owner.FindItemIndex(itemToFind, &onScreenIndex);
	if( uiVerifyf(MenuIndex != -1, "Couldn't find an XML item with MENU_UNIQUE_ID_CREW_INVITES for me to UPDATE!") )
	{
		CMenuItem& curItem = owner.MenuItems[MenuIndex];
		PM_COLUMNS iDepth = static_cast<PM_COLUMNS>(owner.depth-1);

		// if we got invites, set 'em up
		if( iCount > 0 )
		{
			CScaleformMenuHelper::SET_DATA_SLOT(iDepth, onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD
				, onScreenIndex, OPTION_DISPLAY_STYLE_TEXTFIELD, iCount, true, TheText.Get(curItem.cTextId.GetHash(),""), true, true);
		}
		// if we didn't, back to defaults
		else
		{
			CScaleformMenuHelper::SET_DATA_SLOT(iDepth, onScreenIndex, curItem.MenuUniqueId + PREF_OPTIONS_THRESHOLD
				, onScreenIndex, OPTION_DISPLAY_NONE, 0,					true, TheText.Get(curItem.cTextId.GetHash(),""), true, true);
		}
	}
}

#if __BANK
bool CheckStatus(netStatus::StatusCode inStatus, netStatus::StatusCode& outStatus, bool bFailureCounts, const rlClanId clanID, const char* pszErrorReason)
#else
bool CheckStatus(netStatus::StatusCode inStatus, netStatus::StatusCode& outStatus, bool bFailureCounts)
#endif
{
	switch(inStatus)
	{
		// these states will be tracked as 'fails', and will always stomp the out value
		default:
		case netStatus::NET_STATUS_CANCELED:
		case netStatus::NET_STATUS_FAILED:
			if( !bFailureCounts )
			{
				BANK_ONLY( uiErrorf("Fetch for Clan %" I64FMT "i's %s failed! Luckily, this is allowed.", clanID, pszErrorReason); )
				if( outStatus != netStatus::NET_STATUS_FAILED && outStatus != netStatus::NET_STATUS_PENDING )
					outStatus = netStatus::NET_STATUS_SUCCEEDED;
				return true;
			}

			BANK_ONLY( uiErrorf("Fetch for Clan %" I64FMT "i's %s failed! Unluckily, this ends it.", clanID, pszErrorReason); )
			outStatus = netStatus::NET_STATUS_FAILED;
		return false;


		// pendings trump success, but not failure
		case netStatus::NET_STATUS_NONE:
		case netStatus::NET_STATUS_PENDING:
			if( outStatus != netStatus::NET_STATUS_FAILED )
				outStatus = netStatus::NET_STATUS_PENDING;
			return false;

		// success is the most elusive; loses to failure and pending
		case netStatus::NET_STATUS_SUCCEEDED:
			if( outStatus != netStatus::NET_STATUS_FAILED && outStatus != netStatus::NET_STATUS_PENDING)
				outStatus = netStatus::NET_STATUS_SUCCEEDED;
			return true;
	}
}

bool CCrewMenu::FillOutCrewCard(bool& rbShowColumn, const rlClanId clanToFetchFor, bool bIsContextMenuOpen, CrewCardStatus* pOutStatus, PM_COLUMNS whichColumn, bool bIsCompareCard, bool bIsSoonEnough)
{
	if( clanToFetchFor == RL_INVALID_CLAN_ID || (pOutStatus && pOutStatus->IsSet(CrewCard_Errors)) )
	{
		rbShowColumn = false;
		if( pOutStatus )
			pOutStatus->Set(CrewCard_ColumnHidden);

		return true;
	}
	bool bHasUGCPermission = true PS3_ONLY(&&CLiveManager::CheckCommunicationPrivileges());

	netStatus::StatusCode totalSuccess = netStatus::NET_STATUS_NONE;

#if __BANK
	#define CHECK_IT(func, reason)			CheckStatus(func, totalSuccess, true, clanToFetchFor, reason)
	#define CHECK_IT_CAN_FAIL(func, reason) CheckStatus(func, totalSuccess, false, clanToFetchFor, reason)
#else
	#define CHECK_IT(func, reason)			CheckStatus(func, totalSuccess, true)
	#define CHECK_IT_CAN_FAIL(func, reason) CheckStatus(func, totalSuccess, false)
#endif

	CNetworkCrewDataMgr& dataMgr = CLiveManager::GetCrewDataMgr();

	CrewCardStatus tempFillingStatus;
	CrewCardStatus& rStatus = pOutStatus ? *pOutStatus : tempFillingStatus;
	CrewCardStatus originalStatus(rStatus);
	bool bIsUpdate = rStatus.IsSet(CrewCard_ColumnShown);

	bool bDataEmptied = false;
	bool bDataNeedsDisplaying = false;
	if( !rStatus.GetAndSet(CrewCard_ColumnCleared) )
	{
		bDataEmptied = true;
		CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(whichColumn);
	}

	const rlClanDesc* pDesc = NULL;
	const NetworkCrewMetadata* pMetadata = NULL;
	const char* pClanEmblem = NULL;

	bool bItsSafeToFetch = false;
	bool bEmblemIsReady = false;

	if( bIsSoonEnough || dataMgr.IsEverythingReadyFor(clanToFetchFor, false) == netStatus::NET_STATUS_SUCCEEDED)
	{
		bool bBecauseItDisbanded = false;
		netStatus::StatusCode clanStatus = dataMgr.GetClanDesc(clanToFetchFor, pDesc, bBecauseItDisbanded);
		if( clanStatus == netStatus::NET_STATUS_FAILED && bBecauseItDisbanded )
		{
			rStatus.Set(CrewCard_Disbanded);
			rStatus.Set(CrewCard_ColumnShown);
			totalSuccess = netStatus::NET_STATUS_SUCCEEDED;
		}
		else
		{
			bItsSafeToFetch = true;
			CHECK_IT(clanStatus, "Clan Description");
			CHECK_IT_CAN_FAIL(	dataMgr.GetClanMetadata(clanToFetchFor, pMetadata), "Clan Metadata" );

			netStatus::StatusCode emblemStatus = dataMgr.GetClanEmblem(clanToFetchFor, pClanEmblem, true);
			CHECK_IT_CAN_FAIL(emblemStatus, "Emblem");
			bEmblemIsReady = emblemStatus != netStatus::NET_STATUS_PENDING || dataMgr.GetClanEmblemIsRetrying(clanToFetchFor);
		}

	}
	

	bool bColorWasFetched = false;
	bool bCrewTypeWasFetched = false;

	//int colorR, colorG, colorB;
	Color32 crewColor(CHudColour::GetRGBA(HUD_COLOUR_GREY));
	if (pDesc)
	{
		bColorWasFetched = true;
		crewColor.Set(pDesc->m_clanColor);
	}	

	const int SIZE_ENOUGH = 65;
	char szCrewType[SIZE_ENOUGH];
	formatf(szCrewType, SIZE_ENOUGH, "NoType");

	if( bItsSafeToFetch && pMetadata && pMetadata->Succeeded())
	{
		// Attempt Colour / Crew Type Fetch
		bCrewTypeWasFetched = true;
		pMetadata->GetIndexValueForSetName("CrewTypes", 0, szCrewType, SIZE_ENOUGH);			
	}

	// set up the emblem and stuff:
	if( !rStatus.IsSet(CrewCard_Title) 
		|| (!rStatus.IsSet(CrewCard_Emblem) && (pClanEmblem||bEmblemIsReady))
		|| (!rStatus.IsSet(CrewCard_TitleColor) && bColorWasFetched)
		|| (!rStatus.IsSet(CrewCard_Type) && bCrewTypeWasFetched)
		)
	{
		if( pDesc && CScaleformMenuHelper::SET_COLUMN_TITLE(whichColumn, pDesc->m_ClanName, false) )
		{
			const char* pszRecruitingStatus = pDesc->m_IsOpenClan?"CRW_OpenEnrollment":"CRW_NotRecruiting";

			CScaleformMgr::AddParamString( bHasUGCPermission&&pClanEmblem?pClanEmblem:NULL, false ); // clan texture name

			CScaleformMgr::AddParamInt( crewColor.GetRed()	); // R
			CScaleformMgr::AddParamInt( crewColor.GetGreen() ); // G
			CScaleformMgr::AddParamInt( crewColor.GetBlue() ); // B

			CGxtString newDate;
			GenerateHumanReadableDate(pDesc->m_CreatedTimePosix, newDate);
			CScaleformMgr::AddParamString( newDate.GetData(), false );
			CScaleformMgr::AddParamLocString( pszRecruitingStatus, false );
			CScaleformMgr::AddParamFormattedInt( pDesc->m_MemberCount );

			CScaleformMgr::AddParamString(szCrewType);
			CScaleformMgr::EndMethod();

			rStatus.Set(CrewCard_Type, bCrewTypeWasFetched);
			rStatus.Set(CrewCard_Title);
			rStatus.Set(CrewCard_Emblem, pClanEmblem != NULL || bEmblemIsReady || !bHasUGCPermission);
			rStatus.Set(CrewCard_TitleColor, bColorWasFetched);
		}
	}

	if( pDesc && !rStatus.GetAndSet(CrewCard_Description))
	{	
		//CFriendClanData& rFriendClanData = CPauseMenu::GetDynamicPauseMenu()->GetFriendClanData();
		//int friendsInThisClan = rFriendClanData.GetFriendCountForClan(pDesc->m_Id);
		SET_DESCRIPTION(bHasUGCPermission?pDesc->m_ClanMotto:NULL, 0, !pDesc->m_IsSystemClan, pDesc->IsRockstarClan(), pDesc->m_ClanTag, whichColumn);
	}

	bool bSomethingNewCameIn = false;

	unsigned worldRank = 0;
	if( bItsSafeToFetch && CHECK_IT( dataMgr.GetClanWorldRank(clanToFetchFor, worldRank), "World Rank" ) && !rStatus.GetAndSet(CrewCard_Rank) )
		bSomethingNewCameIn = true;
/*
	if( bIsCompareCard )
	{
		rStatus.Set(CrewCard_Color);
		rStatus.Set(CrewCard_Animation);
		rStatus.Set(CrewCard_Animation_GaveUp);
		rStatus.Set(CrewCard_Color_GaveUp);
	}
	else */
	if( pMetadata )
	{
		// color
		if( bColorWasFetched  && !rStatus.GetAndSet(CrewCard_Color) )
			bSomethingNewCameIn = true;

		// animation
		if( !rStatus.IsSet(CrewCard_Animation) )
		{
			char tempData[SIZE_ENOUGH];
			if( pMetadata->GetCrewInfoValueString("anim", tempData, SIZE_ENOUGH) )
			{
				rStatus.Set(CrewCard_Animation);
				bSomethingNewCameIn = true;
			}
			// if it's never coming, just give up.
			else if( pMetadata->Succeeded() )
			{
				rStatus.Set(CrewCard_Animation);
				rStatus.Set(CrewCard_Animation_GaveUp);
			}
		}
	}

	

	// due to the way the SF is drawn, we don't want to show empty slots
	// so what we gotta do is call SET_DATA_SLOT_EMPTY to wipe out the slots
	// then add them back in so it'll be the right size
	// because awesome.
	if( bSomethingNewCameIn )
	{
		if( !bDataEmptied )
		{
			CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(whichColumn);
			bDataEmptied = true;
		}

		if( rStatus.IsSet(CrewCard_Rank) )
		{
			AddCrewWidget(CrewCard_Rank, bIsCompareCard ? "CRW_RANKING" : "CRW_RANK", worldRank, false, whichColumn, OPTION_DISPLAY_STYLE_TEXTFIELD);
		}

		if( rStatus.IsSet(CrewCard_Color) && !rStatus.IsSet(CrewCard_Color_GaveUp))
			AddCrewWidget(CrewCard_Color, "CRW_COLOR",	crewColor.GetRed(), crewColor.GetGreen(), crewColor.GetBlue(), false, whichColumn);

		if( rStatus.IsSet(CrewCard_Animation) )
		{
			bool bUseDefault = true;
			if( !rStatus.IsSet(CrewCard_Animation_GaveUp) )
			{
				char tempData[SIZE_ENOUGH];
				if( uiVerifyf(pMetadata->GetCrewInfoValueString("anim", tempData, SIZE_ENOUGH), "Suddenly our 'anim' metadata went from FINE to FUCKED?!" ) )
				{
					atHashWithStringBank animName(tempData);
					if( animName != ATSTRINGHASH("null",0x3adb3357) )
					{
						// due to some just amazing stuff on the backend, we get to localize this now.
						const char* pKeyToUse = tempData;

						if( animName == ATSTRINGHASH("Bro Love", 0x57E06DE0) )
							pKeyToUse = TheText.Get("CRW_ANIM_BROL");
						else if( animName == ATSTRINGHASH("The Bird", 0x818585C1) )
							pKeyToUse = TheText.Get("CRW_ANIM_FING");
						else if( animName == ATSTRINGHASH("Jerk", 0xF54F3D29) )
							pKeyToUse = TheText.Get("CRW_ANIM_WANK");
						else if( animName == ATSTRINGHASH("Up Yours", 0x8064466) )
							pKeyToUse = TheText.Get("CRW_ANIM_UPYO");

						AddCrewWidget(CrewCard_Animation, "CRW_ANIMATION", pKeyToUse, false, whichColumn);
						bUseDefault = false;
					}
				}
			}

			if( bUseDefault )
				AddCrewWidget(CrewCard_Animation, "CRW_ANIMATION", TheText.Get("SM_LCNONE"), false, whichColumn);
		}

		bDataNeedsDisplaying = true;
	}





	CNetworkCrewDataMgr::WinLossPair wins;
	if( bItsSafeToFetch && CHECK_IT( dataMgr.GetClanChallengeWinLoss(clanToFetchFor, wins), "Challenge Win/Loss" ))
	{
		if( !rStatus.GetAndSet(CrewCard_Stat1) )
			AddCrewStat(0, "CRW_STAT1", wins.wins, wins.wins+wins.losses, bIsUpdate, whichColumn);
	}

	if( EXTRACONTENT.IsContentItemLocked(ATSTRINGHASH("DLC_CLANHEAD2HEAD",0x15d071e1)))
	{
		if( !rStatus.GetAndSet(CrewCard_Stat2)
			&& CScaleformMenuHelper::SET_DATA_SLOT(whichColumn, 1, 0, 999, OPTION_DISPLAY_NONE, 0, false, "", false, bIsUpdate) )
		{
			CScaleformMgr::AddParamInt(-1); // icon
			CScaleformMgr::AddParamInt(-1); // value
			CScaleformMgr::EndMethod();
		}
	}
	else if( bItsSafeToFetch && CHECK_IT( dataMgr.GetClanH2HWinLoss(clanToFetchFor, wins), "Head 2 Head" ) )
	{
		if( !rStatus.GetAndSet(CrewCard_Stat2) )
			AddCrewStat(1, "CRW_STAT2", wins.wins, wins.wins+wins.losses, bIsUpdate, whichColumn);
	}

	// if we hit a failure somewhere above... I SUPPOSE let's just cancel the whole display and show a warning?
	if( totalSuccess == netStatus::NET_STATUS_FAILED )
	{
		pOutStatus->Set(CrewCard_Errors);

		rbShowColumn = false;
		if( pOutStatus )
			pOutStatus->GetAndSet(CrewCard_ColumnHidden);
	}
	
	// if we've set something from above, and the bare minimum
	if( rStatus.IsBareMinimumSet() )
	{
		rbShowColumn = !bIsContextMenuOpen;
		if( !rStatus.GetAndSet(CrewCard_ColumnShown) )
		{
			bDataNeedsDisplaying = true;
		}
	}
	else 
	{
		rStatus.Set(CrewCard_ColumnHidden);
		rbShowColumn = false;
	}

	if( bDataNeedsDisplaying )
		CScaleformMenuHelper::DISPLAY_DATA_SLOT(whichColumn);

	return rStatus.IsSet(CrewCard_ColumnShown);
}

void CCrewMenu::SET_DESCRIPTION( const char* pszDescription, int FriendCount, bool bIsPrivate, bool bIsRockstar, const char* pClanTag, PM_COLUMNS whichColumn /*= PM_COLUMN_RIGHT*/ )
{
	if( CScaleformMgr::BeginMethod(CPauseMenu::GetContentMovieId(), SF_BASE_CLASS_PAUSEMENU, "SET_DESCRIPTION") )
	{
		CScaleformMgr::AddParamInt(whichColumn);
		CScaleformMgr::AddParamString(pszDescription);
		CScaleformMgr::AddParamString("",false);//TheText.Get("CRW_FRIENDCOUNT"));
		CScaleformMgr::AddParamInt(FriendCount);
		// clan tag stuff riding piggyback on description
		CScaleformMgr::AddParamBool(bIsPrivate);
		CScaleformMgr::AddParamBool(bIsRockstar);
		CScaleformMgr::AddParamString(pClanTag);
		CScaleformMgr::AddParamInt(0); // Rank?

		CScaleformMgr::EndMethod();
	}
}

void CCrewMenu::AddCrewWidget(int iIndex, const char* pszHeader, const char* pszData /* = NULL */, bool bIsUpdate /* = false */, PM_COLUMNS whichColumn /* = PM_COLUMN_RIGHT*/)
{
	const char* pHeaderText = TheText.Get(pszHeader);//(whichColumn == CREW_COLUMN_MAIN_CARD) ?  : "";
	if( CScaleformMenuHelper::SET_DATA_SLOT(whichColumn, iIndex, -1, 999, OPTION_DISPLAY_NONE, 0, false, pHeaderText, !pszData, bIsUpdate)
		&& pszData )
	{
		CScaleformMgr::AddParamString(pszData);
		CScaleformMgr::EndMethod();
	}
}

void CCrewMenu::AddCrewWidget(int iIndex, const char* pszHeader, int R, int G, int B, bool bIsUpdate /* = false */, PM_COLUMNS whichColumn /* = PM_COLUMN_RIGHT*/)
{
	const char* pHeaderText = TheText.Get(pszHeader);//(whichColumn == CREW_COLUMN_MAIN_CARD) ?  : "";
	if( CScaleformMenuHelper::SET_DATA_SLOT(whichColumn, iIndex, -1, 999, OPTION_DISPLAY_NONE, 0, false, pHeaderText, false, bIsUpdate))
	{
		CScaleformMgr::AddParamString("",false);
		CScaleformMgr::AddParamInt(R);
		CScaleformMgr::AddParamInt(G);
		CScaleformMgr::AddParamInt(B);
		CScaleformMgr::EndMethod();
	}
}

void CCrewMenu::AddCrewWidget(int iIndex, const char* pszHeader, s64 iValue, bool bIsUpdate, PM_COLUMNS whichColumn /* = PM_COLUMN_RIGHT*/, eOPTION_DISPLAY_STYLE type /*= OPTION_DISPLAY_NONE*/ )
{
	const char* pHeaderText = TheText.Get(pszHeader);//(whichColumn == CREW_COLUMN_MAIN_CARD) ?  : "";
	if( CScaleformMenuHelper::SET_DATA_SLOT(whichColumn, iIndex, -1, 999, type, 0, false, pHeaderText, false, bIsUpdate) )
	{
		CScaleformMgr::AddParamFormattedInt(iValue);
		CScaleformMgr::EndMethod();
	}
}

void CCrewMenu::AddCrewStat(int iIndex, const char* pszHeader, int numerator, int denominator, bool bIsUpdate, PM_COLUMNS whichColumn /* = PM_COLUMN_RIGHT*/)
{
	const char* pHeaderText = TheText.Get(pszHeader);//(whichColumn == CREW_COLUMN_MAIN_CARD) ?  : "";
	if( CScaleformMenuHelper::SET_DATA_SLOT(whichColumn, iIndex, 0, 999, OPTION_DISPLAY_NONE, 0, false, pHeaderText, false, bIsUpdate) )
	{
		char szValue[32]; // MAX_INT is only ~10x2 +1 and some extra
		formatf(szValue, "%i/%i", numerator, denominator);
		CScaleformMgr::AddParamString(szValue);
		CScaleformMgr::AddParamInt( rage::round( (float(numerator)/denominator)*100.0f) );
		CScaleformMgr::EndMethod();
	}
}

#if __BANK

void CCrewMenu::AddWidgets(bkBank* pBank)
{
	if(m_pDebugWidget)
		return;

	atString name("CrewMenu: ");
	name += m_Owner.MenuScreen.GetParserName();

	m_pDebugWidget = pBank->AddGroup(name);

	m_pDebugWidget->AddSlider("Initial Populate Cooldown", &CLEAR_COOLDOWN, -1000, 100000, 25);
	m_pDebugWidget->AddToggle("Hit an Error", &m_bLastFetchHadErrors);
	m_pDebugWidget->AddToggle("Fake Network Loss", &s_gFakeNetworkLoss);
	m_pDebugWidget->AddButton("Fake Refresh Event", datCallback(MFA1(CSystemTimer::Start),(datBase*)&m_EventCooldown, 0));
	m_pDebugWidget->AddSlider("Card First Timeout", &CARD_FIRST_TIME_OFFSET, -1000, 1000, 25);
	m_pDebugWidget->AddSlider("Card Timeout", &CARD_MOVEMENT_INTERVAL, 0, 1000,25);
	m_pDebugWidget->AddSlider("Net Event cooldown", &EVENT_COOLDOWN, 0, 10000,50);
	m_pDebugWidget->AddToggle("Append Number Values", &sm_bDbgAppendNumbers);

	m_pDebugWidget->AddSlider("Selected Crew Index", &m_iSelectedCrewIndex, -1, MAX_INT32, 1);
}
#endif // __BANK


