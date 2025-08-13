 
//rage
#include "fwdecorator/decoratorExtension.h"
#include "fwsys/timer.h"
#include "net/task.h"
#include "rline/rlfriend.h"
#include "net/nethardware.h"

// game
#include "Frontend/CFriendsMenu.h"
#include "Frontend/WarningScreen.h"
#include "Frontend/ButtonEnum.h"
#include "Frontend/PauseMenu.h"
#include "Frontend/ProfileSettings.h"
#include "Frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/ScaleformMenuHelper.h"
#include "Frontend/ui_channel.h"
#include "Frontend/CCrewMenu.h"
#include "Frontend/SocialClubMenu.h"


#if RSG_PC
#include "rline/rlsystemui.h"
#else
#include "frontend/SocialClubMenu.h"
#endif

#include "event/EventGroup.h"
#include "event/EventNetwork.h"
#include "game/ModelIndices.h"
#include "Network/Cloud/Tunables.h"
#include "Network/Live/livemanager.h"
#include "Network/Live/PlayerCardDataManager.h"
#include "Network/Network.h"
#include "Network/Voice/NetworkVoice.h"
#include "Network/Sessions/NetworkSession.h"
#include "Network/Sessions/NetworkSessionUtils.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Peds/ped.h"
#include "Scene/World/GameWorld.h"
#include "Script/commands_player.h"
#include "Script/script_channel.h"
#include "Stats/StatsInterface.h"
#include "Stats/StatsTypes.h"
#include "system/controlMgr.h"

#if __BANK
#include "bank/bank.h"
#endif

#define PLAYER_LIST_COLUMN PM_COLUMN_LEFT
#define PLAYER_AVATAR_COLUMN PM_COLUMN_MIDDLE
#define PLAYER_CARD_COLUMN PM_COLUMN_RIGHT
#define PLAYER_CARD_LOCAL_PLAYER_COMP_COLUMN static_cast<PM_COLUMNS>(4)
#define PLAYER_CARD_OTHER_PLAYER_COMP_COLUMN static_cast<PM_COLUMNS>(3)

#if RSG_ORBIS || RSG_WIN32 || RSG_DURANGO
#define PLAYER_LIST_NUMBER_OF_PAGES (5)
#else
#define PLAYER_LIST_NUMBER_OF_PAGES (1)
#endif

#define PLAYER_CARD_SLOT_FIX(slot) (slot+1)
#define PLAYER_CARD_INVALID_PLAYER_ID (-1)
#define PLAYER_CARD_LOCAL_PLAYER_ID (-2)
#define PLAYER_CARD_IVALID_STAT_TYPE (0)
#define SPAM_REFRESH_FRAME_COUNT (3)
#define PLAYER_CARD_MAX_RANK (999.0f)
#define PLAYER_CARD_SIGNIN_DELAY (30)
#define PLAYER_CARD_FRIEND_CHANGE_DELAY (0)
#define PLAYER_CARD_INVITE_DELAY (1500)

#define AVATAR_BUSY_SPINNER_COLUMN (12)
#define AVATAR_BUSY_SPINNER_INDEX (2)

#define PLAYER_CARD_FRAMES_TO_UPDATE_SCRIPTED_PLAYERS (15)
#define PLAYER_CARD_FRAMES_TO_CHECK_PLAYER_LIST_CHANGE (5)
#define PLAYER_CARD_SCRIPTED_ENTRY_DISPLAY_DELAY (5)

#define FRIENDS_MENU_PAGE_SIZE ( CLiveManager::FRIEND_CACHE_PAGE_SIZE )
#define FRIEND_CACHE_STATS_SIZE ( MAX_FRIEND_STAT_QUERY )


#define LOCAL_PLAYER_CONTEXT "IsLocalPlayer"
#define ACTIVE_PLAYER_CONTEXT "InLocalSession"
#define HAS_PLAYERS_CONTEXT "HasPlayers"
#define CONTEXT_MENU_CONTEXT ATSTRINGHASH("HasContextMenu",0x52536cbf)
#define OFFLINE_CONTEXT "Offline"
#define TRANSITION_CONTEXT "IsInTransition"
#define INVITED_TRANSITION_CONTEXT "IsInvitedToTransition"

const u32 DOWNLOAD_TIMEOUT = 30000;

FRONTEND_MENU_OPTIMISATIONS()

IMPLEMENT_UI_DATA_PAGE(CPlayerListMenuPage);

#if !__FINAL
XPARAM(scriptCodeDev);
PARAM(earlyInvites, "Allows invites/jipping before the MP tutorial has been completed.");
#endif

int RoundDownToNearest2(int iValue, int iRoundingValue)
{
	return (iValue/iRoundingValue)*iRoundingValue;
}

CPlayerListMenuPage::CPlayerListMenuPage(const UIPaginatorBase* const pOwner, PageIndex pageIndex) : UIDataPageBase(pOwner, pageIndex)
{
	m_pPlayerListMenu = NULL;
	m_isPopulated = false;
}

bool CPlayerListMenuPage::SetData(CPlayerListMenu* pPlayerListMenu)
{
	m_pPlayerListMenu = pPlayerListMenu;

	BasePlayerCardDataManager* pManager = m_pPlayerListMenu ? m_pPlayerListMenu->GetDataMgr() : NULL;
	if(pManager && pManager->GetCurrPage() != GetPagedIndex())
	{
		pManager->LoadPage(GetPagedIndex());
		m_pPlayerListMenu->SetReinitPaginator(false);
		m_pPlayerListMenu->TriggerFullReset(1);
	}

	return true;
}

bool CPlayerListMenuPage::IsReady() const
{
	BasePlayerCardDataManager* pManager = m_pPlayerListMenu ? m_pPlayerListMenu->GetDataMgr() : NULL;
	if(pManager)
	{
		if( pManager->IsValid() &&
			(pManager->GetCurrPage() == pManager->GetQueuedPage())
			)
		{
			return !m_pPlayerListMenu || (m_pPlayerListMenu->GetGroupClanData().GetStatus().Succeeded() || m_pPlayerListMenu->GetGroupClanData().GetStatus().Failed());
		}
	}
	return false;
}

int CPlayerListMenuPage::GetSize() const
{
	if(m_pPlayerListMenu && m_pPlayerListMenu->GetDataMgr())
	{
		return m_pPlayerListMenu->GetDataMgr()->GetReadGamerCount();
	}
	return 0;
}

int CPlayerListMenuPage::GetTotalSize() const
{
	if(m_pPlayerListMenu && m_pPlayerListMenu->GetDataMgr() && m_pPageOwner)
	{
		MenuScreenId eMenuId = m_pPlayerListMenu->GetMenuScreenId();

		if (eMenuId == MENU_UNIQUE_ID_FRIENDS_LIST || eMenuId == MENU_UNIQUE_ID_FRIENDS_OPTIONS || eMenuId == MENU_UNIQUE_ID_FRIENDS_MP ||
			eMenuId == MENU_UNIQUE_ID_HEADER_CORONA_INVITE_FRIENDS || eMenuId == MENU_UNIQUE_ID_FRIENDS || eMenuId == MENU_UNIQUE_ID_CORONA_INVITE_FRIENDS)
			return rlFriendsManager::GetTotalNumFriends(NetworkInterface::GetLocalGamerIndex() );
		else
			return m_pPlayerListMenu->GetDataMgr()->GetTotalUncachedGamerCount();

	}
	return 0;
}


void CPlayerListMenuPage::UpdateEntry(int iSlotIndex, DataIndex playerIndex)
{
	m_pPlayerListMenu->SetPlayerDataSlot(playerIndex, iSlotIndex, m_isPopulated);
	
	if(IsReady())
	{
		m_isPopulated = true;
	}
}

bool CPlayerListMenuPage::FillEmptyScaleformSlots( LocalDataIndex UNUSED_PARAM(iItemIndex), int iSlotIndex, MenuScreenId UNUSED_PARAM(menuId), DataIndex UNUSED_PARAM(playerIndex), int UNUSED_PARAM(iColumn) )
{
	PlayerDataSlot slotData;
	slotData.m_pText = "";
	slotData.m_color = m_pPlayerListMenu->GetMiscData().m_emptySlotColor;
	slotData.m_activeState = PlayerDataSlot::AS_OFFLINE;
	return m_pPlayerListMenu->AddOrUpdateSlot(iSlotIndex, PLAYER_CARD_INVALID_PLAYER_ID, false, slotData);
}

// playerIndex and iItemIndex should be the same for these menus.
// The Player list menu knows the ColumnId and column
bool CPlayerListMenuPage::FillScaleformEntry( LocalDataIndex iItemIndex, int iSlotIndex, MenuScreenId UNUSED_PARAM(menuId), DataIndex UNUSED_PARAM(playerIndex), int UNUSED_PARAM(iColumn), bool bIsUpdate )
{
	return m_pPlayerListMenu->SetPlayerDataSlot(iItemIndex, iSlotIndex, bIsUpdate);
}

void CPlayerListMenuPage::SetSelectedItem(int iColumn, int iSlotIndex, DataIndex playerIndex)
{
	BasePlayerCardDataManager* pManager = m_pPlayerListMenu ? m_pPlayerListMenu->GetDataMgr() : NULL;
	if(pManager && pManager->IsValid())
	{
		int currPage = pManager->GetCurrPage();
		int correctedPlayerIndex = playerIndex - (currPage*pManager->GetPageSize());
			
		// We don't need the avatar to flicker.
		if(correctedPlayerIndex != m_pPlayerListMenu->GetCurrentPlayerIndex())
		{
            m_pPlayerListMenu->SetPlayerCard(correctedPlayerIndex);
		}

		UIDataPageBase::SetSelectedItem(iColumn, iSlotIndex, playerIndex);

		CPauseMenu::GetCurrentScreenData().DrawInstructionalButtons(m_pPlayerListMenu->GetMenuScreenId(), iSlotIndex);
	}
}

bool CPlayerListMenuPage::CanHighlightSlot() const
{
	return m_pPlayerListMenu && m_pPlayerListMenu->GetIsInPlayerList();
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
CPlayerListMenuDataPaginator::CPlayerListMenuDataPaginator(const CMenuScreen* const pOwner) : 
	CBaseMenuPaginator(pOwner)
{
	m_pPlayerListMenu = NULL;
}

void CPlayerListMenuDataPaginator::Update(bool bAllowedToDraw /* = true */)
{
	if(m_pPlayerListMenu)
	{
		BasePlayerCardDataManager* pManager = m_pPlayerListMenu->GetDataMgr();
		CPlayerListMenuPage* pActivePage = dynamic_cast<CPlayerListMenuPage*>(GetActivePage());

		if( uiVerify(pManager) && uiVerifyf(pActivePage,"How do we STILL not have a valid active page!?") )
		{
			int bottom = pActivePage->GetLastShownIndex();
			if(bottom != -1)
			{
				CScaleformMovieWrapper& pauseContent = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);


				if( pauseContent.BeginMethod("SET_MENU_LEVEL") )
				{
					pauseContent.AddParam(1);
					pauseContent.EndMethod();
				}

				DataIndex pageBase = pActivePage->GetLowestEntry();
				LocalDataIndex LocalIndex = bottom - pageBase;

				playerlistDebugf1("[CPlayerListMenuDataPaginator] CPlayerListMenuDataPaginator::Update - LocalIndex = %d, pActivePage->GetLastShownItemCount() = %d", LocalIndex, pActivePage->GetLastShownItemCount());
				for(LocalDataIndex i=0; i < pActivePage->GetLastShownItemCount() && i < pManager->GetReadGamerCount(); ++i)
				{
					pActivePage->UpdateEntry(i, i+LocalIndex);
				}
			}
		}
	}

	UIPaginator<CPlayerListMenuPage>::Update(bAllowedToDraw);
}

void CPlayerListMenuDataPaginator::Init(CPlayerListMenu* pPlayerListMenu, int VisibleItemsPerPage, MenuScreenId ColumnId, int iColumnIndex)
{
	if(!m_pPlayerListMenu)
	{
		// IMPORTANT: set our data FIRST before calling InitInternal
		m_pPlayerListMenu = pPlayerListMenu;

		InitInternal(VisibleItemsPerPage, ColumnId, iColumnIndex, PLAYER_LIST_NUMBER_OF_PAGES, PLAYER_AVATAR_COLUMN);
	}
}

// imparts the UniqueDataPerColumn onto a given datapage
bool CPlayerListMenuDataPaginator::SetDataOnPage(UIDataPageBase* thisPage)
{
	CPlayerListMenuPage* pPage = verify_cast<CPlayerListMenuPage*>(thisPage);
	if( pPage )
	{
		return pPage->SetData(m_pPlayerListMenu);
	}
	return false;
}

void CPlayerListMenuDataPaginator::ItemUp()
{
	CPauseMenu::PlayInputSound(FRONTEND_INPUT_UP);
	UIPaginatorBase::ItemUp();
}

void CPlayerListMenuDataPaginator::ItemDown()
{
	CPauseMenu::PlayInputSound(FRONTEND_INPUT_DOWN);
	UIPaginatorBase::ItemDown();
}



////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
CFriendListMenuDataPaginator::CFriendListMenuDataPaginator(const CMenuScreen* const pOwner) : 
	CBaseMenuPaginator(pOwner),
	m_uSystemFriendIndex(0),
	m_bTransitioningFriends(false)
{
	m_pPlayerListMenu = NULL;
}

void CFriendListMenuDataPaginator::Update(bool bAllowedToDraw /* = true */)
{
	if(m_pPlayerListMenu)
	{
		BasePlayerCardDataManager* pManager = m_pPlayerListMenu->GetDataMgr();
		CPlayerListMenuPage* pActivePage = dynamic_cast<CPlayerListMenuPage*>(GetActivePage());

		if( uiVerify(pManager) && uiVerifyf(pActivePage,"How do we STILL not have a valid active page!?") )
		{
			int bottom = pActivePage->GetLastShownIndex();
			if(bottom != -1)
			{
				DataIndex pageBase = pActivePage->GetLowestEntry();
				LocalDataIndex LocalIndex = bottom - pageBase;

				for(LocalDataIndex i=0; i < pActivePage->GetLastShownItemCount(); ++i)
				{
					pActivePage->UpdateEntry(i, i+LocalIndex);
				}
			}
		}
	}

	UIPaginator<CPlayerListMenuPage>::Update(bAllowedToDraw);
}

void CFriendListMenuDataPaginator::SetTransitioningPlayers(bool b)
{
	m_bTransitioningFriends = b;
}

void CFriendListMenuDataPaginator::UpdateItemSelection(DataIndex iNewIndex, bool bHasFocus )
{
	if(!bHasFocus)
	{
		UIPaginator::UpdateItemSelection(iNewIndex, false);
		PM_COLUMNS pmc = static_cast<PM_COLUMNS>(m_iColumnIndex);
		CScaleformMenuHelper::HIDE_COLUMN_SCROLL(pmc);
		return;
	}

	if (m_bTransitioningFriends)
	{
		UIPaginator::UpdateItemSelection(iNewIndex, true);
		PM_COLUMNS pmc = static_cast<PM_COLUMNS>(m_iColumnIndex);
		CScaleformMenuHelper::SET_COLUMN_SCROLL(pmc, m_uSystemFriendIndex, GetTotalSize() );
	}
	else
	{
		UIPaginator::UpdateItemSelection(iNewIndex, bHasFocus);
		PM_COLUMNS pmc = static_cast<PM_COLUMNS>(m_iColumnIndex);
		CScaleformMenuHelper::SET_COLUMN_SCROLL(pmc, m_uSystemFriendIndex, GetTotalSize() );
	}
}

void CFriendListMenuDataPaginator::Init(CPlayerListMenu* pPlayerListMenu, int VisibleItemsPerPage, MenuScreenId ColumnId, int iColumnIndex)
{
	if(!m_pPlayerListMenu)
	{
		// IMPORTANT: set our data FIRST before calling InitInternal
		m_pPlayerListMenu = pPlayerListMenu;
		if (!m_bTransitioningFriends)
			m_uSystemFriendIndex = 0;

		InitInternal(VisibleItemsPerPage, ColumnId, iColumnIndex, PLAYER_LIST_NUMBER_OF_PAGES, PLAYER_AVATAR_COLUMN);
	}
}

// imparts the UniqueDataPerColumn onto a given datapage
bool CFriendListMenuDataPaginator::SetDataOnPage(UIDataPageBase* thisPage)
{
	CPlayerListMenuPage* pPage = verify_cast<CPlayerListMenuPage*>(thisPage);
	if( pPage )
	{
		return pPage->SetData(m_pPlayerListMenu);
	}
	return false;
}

//-----------------------------------------------------------------
// Override these from UIPaginator so we can modify input behaviour 
// slightly to account for streaming in new friend requests.
// ----------------------------------------------------------------
void CFriendListMenuDataPaginator::PageUp()
{	
	if (  IsInputAllowed() )
	{	
		bool const c_supportFriendsPaging = ShouldSupportFriendsPaging();
		bool const c_needsToPageFriends = NeedsToPageFriends( m_uSystemFriendIndex );

		bool const c_supportStatsPaging = ShouldSupportStatsPaging();
		bool const c_needsToPageStats = NeedsToPageStats( m_uSystemFriendIndex );

		if ( c_needsToPageFriends && c_supportFriendsPaging )
		{
			bool const c_atPageBoundry = m_uSystemFriendIndex == 0;
			int const c_maxPageSize = GetActivePage()->GetTotalSize() - 1;
			int const c_startIndex = c_atPageBoundry ? c_maxPageSize - ( c_maxPageSize % FRIENDS_MENU_PAGE_SIZE) : m_uSystemFriendIndex - FRIENDS_MENU_PAGE_SIZE;

			m_uSystemFriendIndex = c_atPageBoundry ? c_maxPageSize : --m_uSystemFriendIndex;

			TransitionToNewPage( c_startIndex );
		}
		
		if ( c_needsToPageStats && c_supportStatsPaging )
		{
			bool const c_needsStatsRefresh = !( c_needsToPageFriends && c_supportFriendsPaging );
			int const c_newIndex = ( m_iLastHighlight - 1 )  <= 0 ? GetActivePage()->GetTotalSize()-1 :  m_iLastHighlight - 1;
			int const c_startIndex = c_newIndex - ( FRIEND_CACHE_STATS_SIZE - 1 );

			RequestNewStatsBlob( c_startIndex, c_needsStatsRefresh );
			
			if ( c_needsStatsRefresh )
			{
				if ( m_uSystemFriendIndex > 0 )
					m_uSystemFriendIndex--;
				else if ( m_uSystemFriendIndex == 0 )
					m_uSystemFriendIndex = GetActivePage()->GetTotalSize() -1;
			}
		}
		else
		{
			m_uSystemFriendIndex = RoundDownToNearest2( m_uSystemFriendIndex-1, m_VisibleItemsPerPage );
		}

		int const c_updatedIndex = RoundDownToNearest2(m_iLastHighlight-1, m_VisibleItemsPerPage);

		if( c_updatedIndex != m_iLastHighlight )
			CPauseMenu::PlayInputSound( FRONTEND_INPUT_UP );

		UpdateItemSelection( c_updatedIndex, true );
	}
}

void CFriendListMenuDataPaginator::PageDown()
{
	if( IsInputAllowed() )
	{
		bool const c_supportFriendsPaging = ShouldSupportFriendsPaging();
		bool const c_needsToPageFriends = NeedsToPageFriends( m_uSystemFriendIndex + 1 );

		bool const c_supportStatsPaging = ShouldSupportStatsPaging();
		bool const c_needsToPageStats = NeedsToPageStats( m_uSystemFriendIndex + 1 );

		if (c_supportFriendsPaging && ( c_needsToPageFriends || (int)m_uSystemFriendIndex == GetActivePage()->GetTotalSize()-1))
		{
			bool const c_needsToWrap = (int)m_uSystemFriendIndex == GetActivePage()->GetTotalSize()-1;
			int const c_startIndex =  c_needsToWrap ? m_uSystemFriendIndex = 0 : m_uSystemFriendIndex +1;
			
			m_uSystemFriendIndex = c_needsToWrap ? 0 : m_uSystemFriendIndex +1;
			
			TransitionToNewPage( c_startIndex );
		}
		else
		{
			m_uSystemFriendIndex = RoundDownToNearest2( m_uSystemFriendIndex + 1 + m_VisibleItemsPerPage, m_VisibleItemsPerPage ) - 1;
		}

		int newIndex = 0;
		if ( m_uSystemFriendIndex > GetActivePage()->GetTotalSize() )
		{
			m_uSystemFriendIndex = GetActivePage()->GetTotalSize() - 1;
			newIndex = m_uSystemFriendIndex % FRIENDS_MENU_PAGE_SIZE;
		}
		else
		{
			newIndex = RoundDownToNearest2( m_iLastHighlight + 1 + m_VisibleItemsPerPage, m_VisibleItemsPerPage ) - 1;
			newIndex = Min( newIndex, GetActivePage()->GetTotalSize()-1 );
		}

		if ( c_needsToPageStats && c_supportStatsPaging )
		{
			bool const c_needsStatsRefresh = !( c_needsToPageFriends && c_supportFriendsPaging );
			RequestNewStatsBlob( newIndex , c_needsStatsRefresh );			
		}


		if( newIndex != m_iLastHighlight )
			CPauseMenu::PlayInputSound( FRONTEND_INPUT_DOWN );
		UpdateItemSelection( newIndex, true );
	}

}

void CFriendListMenuDataPaginator::ItemUp()
{
	if( IsInputAllowed() )
	{
		bool const c_supportFriendsPaging = ShouldSupportFriendsPaging();
		bool const c_needsToPageFriends = NeedsToPageFriends( m_uSystemFriendIndex );

		bool const c_supportStatsPaging = ShouldSupportStatsPaging();
		bool const c_needsToPageStats = NeedsToPageStats( m_uSystemFriendIndex );

		if (c_needsToPageStats && c_supportStatsPaging)
		{
			bool const c_shouldWrap = m_iLastHighlight <= 0;
			bool const c_shouldRefresh = !(c_needsToPageFriends && c_supportFriendsPaging);
			 
			int const c_newIndex = c_shouldWrap ? GetActivePage()->GetTotalSize() - 1 : m_iLastHighlight - 1;
			int const c_newStatsStartIndex = c_newIndex - (FRIEND_CACHE_STATS_SIZE - 1);

			RequestNewStatsBlob( c_newStatsStartIndex, c_shouldRefresh );
		}

		if ( c_needsToPageFriends && c_supportFriendsPaging )
		{
			bool const c_shouldWrap = m_uSystemFriendIndex == 0;
			int const c_pageTotalSize = GetActivePage()->GetTotalSize() - 1;
			int const c_pageOffset = c_pageTotalSize % FRIENDS_MENU_PAGE_SIZE;
			int const c_startValue = c_shouldWrap ? 
				c_pageTotalSize - c_pageOffset :
				m_uSystemFriendIndex - FRIENDS_MENU_PAGE_SIZE;
			
			m_uSystemFriendIndex = c_shouldWrap ? c_pageTotalSize : --m_uSystemFriendIndex;
			
			TransitionToNewPage( c_startValue );
		}
		else
		{
			if ( m_uSystemFriendIndex > 0 )
				m_uSystemFriendIndex--;
			else if ( m_uSystemFriendIndex == 0 )
				m_uSystemFriendIndex = GetActivePage()->GetTotalSize() -1;
		}
		
		int newIndex = m_iLastHighlight - 1;
		if( m_iLastHighlight <= 0 )
			newIndex = GetActivePage()->GetTotalSize() - 1;
		UpdateItemSelection( newIndex, true );
	}
}

void CFriendListMenuDataPaginator::ItemDown()
{
	if( IsInputAllowed() )
	{
		int const c_systemIndexAsInt = (int)m_uSystemFriendIndex;
		bool const c_isIndexAtPageBounds = c_systemIndexAsInt == GetActivePage()->GetTotalSize()-1;
		int const c_nextIndex = c_isIndexAtPageBounds ? 0 : m_uSystemFriendIndex + 1; 

		bool const c_supportFriendsPaging = ShouldSupportFriendsPaging();
		bool const c_needsToPageFriends = NeedsToPageFriends( c_nextIndex );

		bool const c_supportStatsPaging = ShouldSupportStatsPaging();		
		bool const c_needsToPageStats = NeedsToPageStats( c_nextIndex );

		if ( c_supportFriendsPaging && ( c_needsToPageFriends || c_isIndexAtPageBounds ) )
		{
			m_uSystemFriendIndex = c_isIndexAtPageBounds ? 0 : m_uSystemFriendIndex + 1;			
			TransitionToNewPage( m_uSystemFriendIndex );
		}
		else
		{
			m_uSystemFriendIndex = c_isIndexAtPageBounds ? 0 : ++m_uSystemFriendIndex ;
			m_iLastHighlight = c_isIndexAtPageBounds ? -1 : m_iLastHighlight;
			
			if ( c_needsToPageStats && c_supportStatsPaging )
			{
				int const c_statsStartIndex = Min(m_iLastHighlight + 1, GetActivePage()->GetTotalSize()-1);
				bool const c_shouldRefresh = !( c_needsToPageFriends && c_supportFriendsPaging );

				RequestNewStatsBlob(  c_statsStartIndex, c_shouldRefresh );
			}
		}
		
		int newIndex = m_iLastHighlight + 1;

		if (m_uSystemFriendIndex != 0)
		{
			newIndex = Min(newIndex, GetActivePage()->GetTotalSize()-1);
		}

		UpdateItemSelection( newIndex, true );
	}
}


// Utility methods for input with friends paginator
void CFriendListMenuDataPaginator::TransitionToNewPage( int const startIndex )
{
	CLiveManager::RequestNewFriendsPage(startIndex, FRIENDS_MENU_PAGE_SIZE);
	
	CFriendPlayerCardDataManager* pMgr = dynamic_cast<CFriendPlayerCardDataManager*>(m_pPlayerListMenu->GetDataMgr());	
	pMgr->RequestStatsQuery( startIndex );

	SetTransitioningPlayers(true);
}

void CFriendListMenuDataPaginator::RequestNewStatsBlob( int const startIndex, bool const refreshStats )
{
	CFriendPlayerCardDataManager* pMgr = dynamic_cast<CFriendPlayerCardDataManager*>(m_pPlayerListMenu->GetDataMgr());	
	pMgr->RequestStatsQuery( startIndex );
	
	if ( refreshStats )
	{
		pMgr->RefreshGamerStats();
	}

	m_pPlayerListMenu->SetPagingStats();
}

bool CFriendListMenuDataPaginator::IsInputAllowed() const
{
	bool const c_isActivePageReady = IsReady();
	bool const c_isReadingFriends = CLiveManager::IsReadingFriends();
	bool const c_hasDataMgr = m_pPlayerListMenu && m_pPlayerListMenu->GetDataMgr();
	bool const c_isPlayerMgrReady = m_pPlayerListMenu  && m_pPlayerListMenu->IsPopulated();
	bool const c_result = c_isActivePageReady && !c_isReadingFriends && c_hasDataMgr && c_isPlayerMgrReady;
	return c_result;
}

bool CFriendListMenuDataPaginator::ShouldSupportStatsPaging() const
{
	bool const c_result = GetActivePage()->GetTotalSize() > FRIEND_CACHE_STATS_SIZE;
	return c_result;
}

bool CFriendListMenuDataPaginator::ShouldSupportFriendsPaging()  const
{
	bool const c_result = GetActivePage()->GetTotalSize() > FRIENDS_MENU_PAGE_SIZE;
	return c_result;
}

bool CFriendListMenuDataPaginator::NeedsToPageFriends( int const index ) const
{
	bool const c_result = index % FRIENDS_MENU_PAGE_SIZE == 0;
	return c_result;
}

bool CFriendListMenuDataPaginator::NeedsToPageStats( int const index ) const
{
	bool const c_result = index  % FRIEND_CACHE_STATS_SIZE == 0;
	return c_result;
}

void CFriendListMenuDataPaginator::MoveToIndex(s32 iIndex)
{
	// Used for mouse input 
	if (iIndex == GetCurrentHighlight())
	{
		return;
	}
	else if (iIndex > GetCurrentHighlight())
	{
		m_uSystemFriendIndex += (iIndex - GetCurrentHighlight());
	}
	else
	{
		m_uSystemFriendIndex -= (GetCurrentHighlight() - iIndex);
	}
}


bool CFriendListMenuDataPaginator::UpdateInput(s32 eInput)
{
#if ALLOW_MANUAL_FRIEND_REFRESH
	switch(eInput)
	{
	case PAD_SELECT:
		{
			if (rlFriendsManager::CanQueueFriendsRefresh(NetworkInterface::GetLocalGamerIndex()))
			{
				CPauseMenu::PlaySound("SELECT");

				int startIndex = m_uSystemFriendIndex - m_uSystemFriendIndex % FRIEND_UI_PAGE_SIZE;
				rlFriendsManager::RequestRefreshFriendsPage(NetworkInterface::GetLocalGamerIndex(), CLiveManager::GetFriendsPage(), startIndex, FRIEND_UI_PAGE_SIZE, true);

#if RSG_PC
				// Trigger a reset
				if (m_pPlayerListMenu)
				{
					m_pPlayerListMenu->TriggerFullReset(0);
				}
#endif // #if RSG_PC

				return true;
			}
		}
	}
#endif // #if ALLOW_MANUAL_FRIEND_REFRESH

	return UIPaginatorBase::UpdateInput(eInput);
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
CPlayerListMenu::CPlayerListMenu(CMenuScreen& owner, bool bDontInitPaginator)
: CScriptMenu(owner)
, m_pPaginator(NULL)
, m_transitionIndex(-1)
, m_transitionSystemIndex(-1)
, m_IsWaitingForPlayersRead(false)
{

	if (!bDontInitPaginator)
	{
		m_pPaginator = rage_new CPlayerListMenuDataPaginator(&owner);
	}

	m_bContinualUpdateMode = false;
	m_shouldReinit = true;

	if( m_ScriptPath.length() == 0 )
		m_ScriptPath = "MP_MENUPED";

	m_listRules.m_hideLocalPlayer = false;
	m_listRules.m_hidePlayersInTutorial = false;

	m_emblemTxdSlot = -1;
	m_localEmblemTxdSlot = -1;

	Clear();
	if( uiVerifyf(GetContextMenu(), "PlayerListMenu(%d) expected to have a ContextMenu!", GetMenuScreenId().GetValue()) )
		GetContextMenu()->SetShowHideCB(this);
}

CPlayerListMenu::~CPlayerListMenu()
{
	if ( m_pPaginator )
	{
		delete m_pPaginator;
		m_pPaginator = NULL;
	}

	CancelJIPSessionStatusRequest();
}

int CPlayerListMenu::GetListMenuId()
{
	uiAssertf(m_Owner.m_pContextMenu->GetTriggerMenuId() != MENU_UNIQUE_ID_INVALID, "Trying to GetListMenuId when the contextMenu TriggerMenuId hasn't been set up in XML!");
	return m_Owner.m_pContextMenu->GetTriggerMenuId().GetValue();
}

void CPlayerListMenu::Clear()
{
	m_showAvatar = false;
	m_isEmpty = false;
	m_isPopulated = false;
	m_isPopulatingStats = false;
	m_currPlayerIndex = 0;
	m_currPlayerGamerHandle = RL_INVALID_GAMER_HANDLE;
	m_playerTeamId = PLAYER_CARD_INVALID_TEAM_ID;
	m_justGainedFocus = 0;
	m_fullResetDelay = 0;
	m_isInPlayerList = false;
	m_wasInPlayerListUponRefresh = false;
	m_isComparing = false;
	m_isComparingVisible = false;
	m_hasClanData = false;
	m_hasCrewInfo = false;
	m_localPlayerHasCrewInfo = false;
	m_hasHint = false;
	m_hadHint = false;
	m_hasSentCrewInvite = false;
	m_hadSentCrewInvite = false;
	m_killScriptPending = false;
	m_mgrSucceeded = false;
	m_bPaginatorPagingStats = false;
	
	m_listRefreshDelay = 0;
	m_currentStatVersion = 0;
	m_WasSystemError = false;

	m_pPreviousStatus = NULL;
	m_inviteTimer.Zero();

	RemoveEmblemRef(m_emblemTxdSlot);
	RemoveEmblemRef(m_localEmblemTxdSlot);

	ClearColumnVisibilities();
	ClearColumnDisplayed();
}

void CPlayerListMenu::SetCurrentlyViewedPlayer(s32 currPlayerIndex)
{
	m_currPlayerIndex = currPlayerIndex;
	m_currPlayerGamerHandle = GetGamerHandle(currPlayerIndex);
}

bool CPlayerListMenu::ShouldFadeInPed() const
{
#if RSG_PC
	return true;
#else
	return false;
#endif //RSG_PC
}

void CPlayerListMenu::Update()
{
	if(m_killScriptPending)
	{
		KillScript();
		m_killScriptPending = false;
	}

	CScriptMenu::Update();

	Paginator* pPaginator = GetPaginator();
	BasePlayerCardDataManager* pManager = GetDataMgr();

	if(uiVerify(pManager) && uiVerify(pPaginator))
	{
		bool hasError = pManager->HadError();

#if RSG_ORBIS
		bool IsPS4Error = pManager->GetError() == BasePlayerCardDataManager::ERROR_NEEDS_GAME_UPDATE || pManager->GetError() == BasePlayerCardDataManager::ERROR_PROFILE_CHANGE || 
						  pManager->GetError() == BasePlayerCardDataManager::ERROR_PROFILE_SYSTEM_UPDATE || pManager->GetError() == BasePlayerCardDataManager::ERROR_REASON_AGE;
#endif //RSG_ORBIS
		bool bIsSystemError = CheckSystemError() && HasSystemError();

		// Only populate the warning screen if we're not already populated
		if (bIsSystemError)
		{
			if (m_WasSystemError != bIsSystemError)
			{
				HandleSystemError();
			}

			m_WasSystemError = bIsSystemError;
		}		
		else if(IsInEntryDelay())
		{
			CheckListChanges();
		}
#if RSG_ORBIS
		else if (IsPS4Error)
		{
			CScaleformMenuHelper::SET_BUSY_SPINNER(PLAYER_AVATAR_COLUMN, false);
			CScaleformMenuHelper::SET_BUSY_SPINNER(DownloadingMessageColumns(), false);
			CPauseMenu::GetDisplayPed()->SetVisible(false);

			ShowColumnWarning_EX(PM_COLUMN_MAX, CPauseMenu::GetOfflineReasonAsLocKey(), "CWS_WARNING");
		}
#endif //RSG_ORBIS
		else if ( pPaginator->IsTransitioningPlayers() || m_IsWaitingForPlayersRead)
		{
			if (!m_IsWaitingForPlayersRead)
			{
				CScaleformMenuHelper::SET_BUSY_SPINNER(PLAYER_AVATAR_COLUMN, false);
				CScaleformMenuHelper::SET_BUSY_SPINNER(DownloadingMessageColumns(), true);

				if (CUIMenuPed* pMenuPed = CPauseMenu::GetDisplayPed())
				{
					pMenuPed->SetVisible(false);
				}

				ShowColumnWarning_EX(DownloadingMessageColumns(), NULL, "SC_LB_DL3");
				m_IsWaitingForPlayersRead = true;

				m_transitionIndex = m_currPlayerIndex;
				m_transitionSystemIndex = pPaginator->GetRawPlayerIndex();
			}
			else if (!CLiveManager::IsReadingFriends())
			{
				m_IsWaitingForPlayersRead = false;

				playerlistDebugf1("[%s] CPlayerListMenu::Update - !LiveManager::IsReadingFriends() resetting datamgr", GetClassNameIdentifier());
				pManager->Reset();

				LoseFocus();
				Populate(GetUncorrectedMenuScreenId());

				m_currPlayerIndex = m_transitionSystemIndex % FRIENDS_MENU_PAGE_SIZE;

				OnNavigatingContent(true);
				pPaginator->SetRawPlayerIndex(m_transitionSystemIndex);
				pManager->RefreshGamerData();

				m_transitionIndex = -1;
				m_transitionSystemIndex = -1;

				pPaginator->SetTransitioningPlayers(false);
				SetIsInPlayerList(true);
			}
		}
		else if (m_bPaginatorPagingStats )
		{
			if ( pManager->IsValid() && !pManager->IsStatsQueryPending() )
			{
				m_transitionSystemIndex = pPaginator->GetRawPlayerIndex();
				m_currPlayerIndex = m_transitionSystemIndex % FRIENDS_MENU_PAGE_SIZE;

				OnNavigatingContent(true);
				pManager->RefreshGamerData();

				m_transitionIndex = -1;
				m_transitionSystemIndex = -1;
				m_bPaginatorPagingStats = false;

				SetIsInPlayerList(true);
			}
		}
		else if(pManager->AreGamerTagsPopulated() || hasError)
		{
			playerlistDebugf1("[%s] CPlayerListMenu::Update - Populated gamertags || hasError", GetClassNameIdentifier());

			if(!ValidateManagerForClanDataCheck() || pManager->GetReadGamerCount())
			{
				CheckForClanData();
			}

			int numGamers = pManager->GetReadGamerCount();
			bool hasDataToPopulate = (pManager->IsValid() && m_hasClanData) || hasError;
			bool becameValid = (!m_isPopulated && hasDataToPopulate) || (m_isPopulated && !m_mgrSucceeded && pManager->HasSucceeded());
			m_mgrSucceeded = pManager->HasSucceeded();

			if(m_isPopulatingStats && hasDataToPopulate)
			{
				playerlistDebugf1("[%s] CPlayerListMenu::Update - m_isPopulatingStats && hasDataToPopulate", GetClassNameIdentifier());
				CScaleformMenuHelper::SET_BUSY_SPINNER(PLAYER_AVATAR_COLUMN, false);
				CScaleformMenuHelper::SET_BUSY_SPINNER(DownloadingMessageColumns(), false);
				ClearWarningColumn_EX(DownloadingMessageColumns());
				m_isPopulatingStats = false;
			}
			else if(!m_isPopulatingStats && !hasDataToPopulate)
			{
				playerlistDebugf1("[%s] CPlayerListMenu::Update - !m_isPopulatingStats && !hasDataToPopulate", GetClassNameIdentifier());

				if( !GetPaginator() || !GetPaginator()->IsReady())
				{
					playerlistDebugf1("[%s] CPlayerListMenu::Update - !GetPaginator() || !GetPaginator()->IsReady()", GetClassNameIdentifier());

					CScaleformMenuHelper::SET_BUSY_SPINNER(PLAYER_AVATAR_COLUMN, false);
					CScaleformMenuHelper::SET_BUSY_SPINNER(DownloadingMessageColumns(), true);

					if (CUIMenuPed* pMenuPed = CPauseMenu::GetDisplayPed())
					{
						pMenuPed->SetVisible(false);
					}

					ShowColumnWarning_EX(DownloadingMessageColumns(), NULL, "SC_LB_DL3");

					m_isPopulatingStats = true;
				}
			}

			if(hasError || bIsSystemError)
			{
				if(becameValid)
				{
					playerlistDebugf1("[%s] CPlayerListMenu::Update - hasError && becomeValid", GetClassNameIdentifier());
					if (CheckNetworkTransitions() && CNetwork::GetNetworkSession().IsTransitionLeaving())
					{
						HandleLaunchingGame();
					}
					else
					{
						playerlistDebugf1("[%s] CPlayerListMenu::Update - No Players", GetClassNameIdentifier());
						HandleNoPlayers();
					}
					
					CScaleformMenuHelper::SET_BUSY_SPINNER(PLAYER_AVATAR_COLUMN, false);
					CScaleformMenuHelper::SET_BUSY_SPINNER(DownloadingMessageColumns(), false);
					m_isPopulated = true;
				}
			}
			else if (!m_isPopulated && AllowTimeout() && HasTimedOut())
			{
				playerlistDebugf1("[%s] CPlayerListMenu::Update - Handling Timeout", GetClassNameIdentifier());
				HandleTimeout();
			}
			else if(becameValid || m_isEmpty)
			{
				playerlistDebugf1("[%s] CPlayerListMenu::Update - becameValid || m_isEmpty", GetClassNameIdentifier());

				if(numGamers > 0)
				{
					playerlistDebugf1("[%s] CPlayerListMenu::Update - numGamers > 0", GetClassNameIdentifier());

                    SetPlayerCard(m_currPlayerIndex); // Fill out the card for the player at the top of the list.

                    if(CheckManagerBufferOnPopulate() && pManager->HasBufferedReadData() && pManager->HasSucceeded())
                    {
                        // Re-initialize the paginator, because we've swapped buffers and the list has changed
                        InitPaginator(GetUncorrectedMenuScreenId());
                        if( GetPaginator() )
                            GetPaginator()->UpdateItemSelection(m_currPlayerIndex,true);

                        HighlightGamerHandle(m_currPlayerGamerHandle);
                    }
				}

				if (CheckNetworkTransitions() && CNetwork::GetNetworkSession().IsTransitionLeaving())
				{
					HandleLaunchingGame();
				}
				else
				{
					RefreshPlayers();
				}
				
				m_isPopulated = true;
			}
			else if(m_isPopulated)
			{
				playerlistDebugf1("[%s] CPlayerListMenu::Update - m_isPopulated", GetClassNameIdentifier());
				
				// If the PCDM needs to be refreshed, let's trigger a refresh of the players before fetching player data
				// as that player may not be available anymore (i.e. Active Player List)
				if(pManager->NeedsRefresh())
				{
					playerlistDebugf3("[%s] CPlayerListMenu::Update - NeedsRefresh", GetClassNameIdentifier());
					RefreshPlayers();
				}
				else
				{
					bool needsRefresh = DoesPlayerCardNeedRefresh(m_currPlayerIndex, m_hasCrewInfo);
				
					if(m_isComparing)
					{
						needsRefresh |= DoesPlayerCardNeedRefresh(PLAYER_CARD_LOCAL_PLAYER_ID, m_localPlayerHasCrewInfo);
					}

					if(needsRefresh)
					{
						playerlistDebugf1("[%s] CPlayerListMenu::Update - needsRefresh", GetClassNameIdentifier());
						SetPlayerCard(m_currPlayerIndex, false);
					}
					else if(!GetContextMenu() || !GetContextMenu()->IsOpen())
					{
						SetupAvatarColumn(m_currPlayerIndex, m_pPreviousStatus);
					}

					RefreshPlayers();

					UpdatePedState(m_currPlayerIndex);

					if(m_showAvatar &&
						!CPauseMenu::IsClosingDown() &&
						(GetContextMenu()==NULL || !GetContextMenu()->IsOpen()) &&
						(!m_inviteTimer.IsStarted() || m_inviteTimer.IsComplete(PLAYER_CARD_INVITE_DELAY)) &&
						m_currPlayerIndex != PLAYER_CARD_INVALID_PLAYER_ID)
					{
						CUIMenuPed* pMenuPed = CPauseMenu::GetDisplayPed();
						if(pMenuPed && (!pMenuPed->HasPed() || pMenuPed->IsWaitingForNewPed()) && 
							pManager->ArePlayerStatsAccessible(GetGamerHandle(m_currPlayerIndex)))
						{
							bool isLocal = false;
							bool isActive = false;

							if(NetworkInterface::IsNetworkOpen())
							{
								isLocal = IsLocalPlayer(m_currPlayerIndex);
								isActive = isLocal;
						
								if(AllowTimeout())
								{
									ResetTimeout();
								}

								if(!isActive)
								{
									CNetObjPlayer* pObjPlayer = GetNetObjPlayer(m_currPlayerIndex);
									if (pObjPlayer && pObjPlayer->GetStatVersion() != 0)
									{
										isActive = true;
									}
								}
							}

							if(isActive)
							{
								playerlistDebugf1("[%s] CPlayerListMenu::Update - isActive and will set up avatar.", GetClassNameIdentifier());

								PM_COLUMNS column = PM_COLUMN_MIDDLE;
								if(!isLocal && m_isComparing)
								{
									column = PM_COLUMN_EXTRA3;
								}

								if(ShouldFadeInPed())
								{
									const u32 numFramesToDelayFade = 0;
									const u32 fadeDurationMS = 175;
									pMenuPed->SetFadeIn(fadeDurationMS, fwTimer::GetTimeStepInMilliseconds() * numFramesToDelayFade);
								}
								else
								{
									pMenuPed->SetVisible(true);
								}

								pMenuPed->SetPedModel(static_cast<int>(GetActivePlayerIndex(m_currPlayerIndex)), column);
							}
							else
							{
								LaunchAvatarScript();
							}
						}
					}
				}
			}
			else if(++m_justGainedFocus < SPAM_REFRESH_FRAME_COUNT)
			{
				RefreshPlayers();
			}


			if(m_wasInPlayerListUponRefresh && m_isPopulated)
			{
				SetIsInPlayerList(true);
				if(GetPaginator())
				{
					if(m_currPlayerIndex > 0)
					{
						GetPaginator()->ItemUp();
						GetPaginator()->ItemDown();
					}
					else if(numGamers > 0)
					{
						CScaleformMenuHelper::SET_COLUMN_HIGHLIGHT(PLAYER_LIST_COLUMN, 0);
					}
				}

				if(numGamers > 0)
				{
					SetPlayerCard(m_currPlayerIndex); // Fill out the card for the player at the top of the list.
				}
				

				m_wasInPlayerListUponRefresh = false;
			}
		}
		else if(pManager->NeedsRefresh())
		{
			TriggerFullReset(1);
		}

		if(m_fullResetDelay > 0)
		{
			if(!m_inviteTimer.IsStarted())
			{
				if(--m_fullResetDelay == 0)
				{
					playerlistDebugf1("[%s] CPlayerListMenu::Update - m_fullResetDelay = 0, reset data manager.", GetClassNameIdentifier());
					
					bool wasInPlayerListUponRefresh = GetIsInPlayerList();

					if(pManager->CanReset())
					{
						playerlistDebugf1("[%s] CPlayerListMenu::Update - Reset", GetClassNameIdentifier());
						pManager->Reset();
					}

					const rlGamerHandle cachedCurrGamerHandle = m_currPlayerGamerHandle;
					HandleUIReset(cachedCurrGamerHandle, *pManager);

					m_wasInPlayerListUponRefresh = wasInPlayerListUponRefresh;
				}
			}
		}

		if(pManager && pManager->IsValid())
		{
			Paginator* pPaginator = GetPaginator();
			if(pPaginator)
			{
				playerlistDebugf1("[%s] CPlayerListMenu::Update - update the paginator.", GetClassNameIdentifier());

				FillOutPlayerMissionInfo();
				pPaginator->Update();
			}
		}
	}
}

bool CPlayerListMenu::HasSystemError() const
{
	return !netHardware::IsLinkConnected() || !CLiveManager::IsOnline() || CLiveManager::CheckIsAgeRestricted(SCE_ONLY(GAMER_INDEX_ANYONE));
}

void CPlayerListMenu::HandleUIReset( const rlGamerHandle& cachedCurrGamerHandle, const BasePlayerCardDataManager& UNUSED_PARAM(rManager) )
{
	if (m_isComparing == false)
	{
		LoseFocus();
	}

	Populate(GetUncorrectedMenuScreenId());
	HighlightGamerHandle(cachedCurrGamerHandle);
}

void CPlayerListMenu::UpdatePedState(int playerIndex)
{
	bool isLit = false;
	bool isAwake = false;

	if(playerIndex == PLAYER_CARD_LOCAL_PLAYER_ID)
	{
		isAwake = true;
		isLit = true;
	}
	else if(playerIndex != PLAYER_CARD_INVALID_PLAYER_ID)
	{
		BasePlayerCardDataManager* pManager = GetDataMgr();
		if(uiVerify(pManager) && 0 <= playerIndex && playerIndex < pManager->GetReadGamerCount())
		{
			const GamerData& rGamerData = pManager->GetGamerData(playerIndex);
			if(rGamerData.m_isOnline || IsLocalPlayer(playerIndex))
			{
				isAwake = true;
				isLit = true;
			}
			else
			{
				isAwake = false;
				isLit = false;
			}
		}
	}

	if(!m_isInPlayerList)
	{
		isLit = false;
	}

	CPauseMenu::GetDisplayPed()->SetIsAsleep(!isAwake);
	CPauseMenu::GetDisplayPed()->SetIsLit(isLit);
	CPauseMenu::GetLocalPlayerDisplayPed()->SetIsAsleep(false);
	CPauseMenu::GetLocalPlayerDisplayPed()->SetIsLit(m_isInPlayerList);
}

bool CPlayerListMenu::ShouldCheckStatsForInviteOrJIP()
{
    const bool bCheckTutorialStat = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ENABLE_TUTORIAL_STAT_CHECK", 0x9EE4F4E4), false); 
    return bCheckTutorialStat;
}

void CPlayerListMenu::RefreshPlayers()
{
	BasePlayerCardDataManager* pManager = GetDataMgr();
	if(uiVerify(pManager))
	{
		int numGamers = pManager->HasSucceeded() ? pManager->GetReadGamerCount() : pManager->GetWriteGamerCount();
		if(numGamers == 0)
		{
			if(!m_isEmpty)
			{
				if (CNetwork::GetNetworkSession().IsTransitionLeaving())
				{
					HandleLaunchingGame();
				}
				else
				{
					playerlistDebugf1("[CPlayerListMenu] CPlayerListMenu::RefreshPlayers - No Players");
					HandleNoPlayers();
				}
				m_isEmpty = true;
			}
		}
		else
		{
			m_isEmpty = false;
		}

		pManager->RefreshGamerData();

		if(m_isPopulated && !pManager->HadError())
		{
			if(pManager->NeedsRefresh())
			{
				TriggerFullReset(PLAYER_CARD_FRIEND_CHANGE_DELAY);
				pManager->ClearRefreshFlag();
			}

			if(--m_listRefreshDelay <= 0)
			{
				CheckListChanges();
				m_listRefreshDelay = PLAYER_CARD_FRAMES_TO_CHECK_PLAYER_LIST_CHANGE;
			}
		}
	}
}

void CPlayerListMenu::HandleSystemError()
{
	
	CContextMenu* pContextMenu = GetContextMenu();

	if ( pContextMenu )
	{
		pContextMenu->CloseMenu();
		SUIContexts::Activate(UIATSTRINGHASH("HasContextMenu",0x52536cbf));
		CControlMgr::GetMainFrontendControl(false).DisableInputsOnBackButton(100);
	}

	SET_DATA_SLOT_EMPTY(PLAYER_LIST_COLUMN);
	SET_DATA_SLOT_EMPTY(PLAYER_CARD_COLUMN);
	SET_DATA_SLOT_EMPTY(PLAYER_CARD_LOCAL_PLAYER_COMP_COLUMN);
	SET_DATA_SLOT_EMPTY(PLAYER_CARD_OTHER_PLAYER_COMP_COLUMN);


	if(!netHardware::IsLinkConnected())
	{
		if ( GetContextMenu() && GetContextMenu()->IsOpen() )
		{
			GetContextMenu()->CloseMenu();
		}

		ShowColumnWarning_EX(NoPlayersColumns(), "HUD_NOCONNECT", "CWS_WARNING");
	}
	else if(!CLiveManager::IsOnline())
	{
#if RSG_NP
		if( !g_rlNp.IsNpAvailable( NetworkInterface::GetLocalGamerIndex() ) )
		{
			const rlNpAuth::NpUnavailabilityReason c_npReason = g_rlNp.GetNpAuth().GetNpUnavailabilityReason( NetworkInterface::GetLocalGamerIndex() );
			uiDisplayf("CPlayerListMenu::HandleNoPlayers - Network is not available. Reason code %d", c_npReason );

			if( c_npReason != rlNpAuth::NP_REASON_INVALID )
			{
				ShowColumnWarning_EX(NoPlayersColumns(),  CPauseMenu::GetOfflineReasonAsLocKey(), "CWS_WARNING" );
			}
		}
		else
#endif
		{
			if ( GetContextMenu() && GetContextMenu()->IsOpen() )
			{
				GetContextMenu()->CloseMenu();
			}

			ShowColumnWarning_EX(NoPlayersColumns(), "NOT_CONNECTED", "WARNING_NOT_CONNECTED_TITLE");
		}
	}
	else if(CLiveManager::CheckIsAgeRestricted())
	{
		ShowColumnWarning_EX(NoPlayersColumns(), "HUD_AGERES", "CWS_WARNING");
	}
	else
	{
		SUIContexts::Activate(ATSTRINGHASH("HIDE_ACCEPTBUTTON",0x14211b54));
		BasePlayerCardDataManager* pManager = GetDataMgr();
		
		if (rlFriendsManager::ActivationFailed())
		{
			ShowColumnWarning_EX(NoPlayersColumns(), GetFriendErrorText(), GetNoResultsTitle());
		}
		else if(uiVerify(pManager) && pManager->HadError())
		{
			ShowColumnWarning_EX(NoPlayersColumns(), "PCARD_SYNC_ERROR", "PCARD_SYNC_ERROR_TITLE");
		}

		else
		{
			ShowColumnWarning_EX(NoPlayersColumns(), GetNoResultsText(), GetNoResultsTitle());
		}
	}

	CScaleformMenuHelper::SET_BUSY_SPINNER(PLAYER_AVATAR_COLUMN, false);
	CPauseMenu::GetDisplayPed()->SetVisible(false);

	CPauseMenu::RefreshSocialClubContext();
	CPauseMenu::RedrawInstructionalButtons();
}

void CPlayerListMenu::GoBack()
{
    CPauseMenu::PlayInputSound(FRONTEND_INPUT_ACCEPT);
    CPauseMenu::MENU_SHIFT_DEPTH(kMENUCEPT_SHALLOWER,false,true);
}

void CPlayerListMenu::OfferAccountUpgrade()
{
	CPauseMenu::PlayInputSound(FRONTEND_INPUT_ACCEPT);
	CPauseMenu::MENU_SHIFT_DEPTH(kMENUCEPT_SHALLOWER,false,true);
	CLiveManager::ShowAccountUpgradeUI();
}

void CPlayerListMenu::HandleNoPlayers()
{
	SET_DATA_SLOT_EMPTY(PLAYER_LIST_COLUMN);
	SET_DATA_SLOT_EMPTY(PLAYER_CARD_COLUMN);
	SET_DATA_SLOT_EMPTY(PLAYER_CARD_LOCAL_PLAYER_COMP_COLUMN);
	SET_DATA_SLOT_EMPTY(PLAYER_CARD_OTHER_PLAYER_COMP_COLUMN);


	if(!netHardware::IsLinkConnected())
	{
		ShowColumnWarning_EX(NoPlayersColumns(), "HUD_NOCONNECT", "CWS_WARNING");
	}
	else if(!CLiveManager::IsOnline())
	{
#if RSG_NP
		if( !g_rlNp.IsNpAvailable( NetworkInterface::GetLocalGamerIndex() ) )
		{
			const rlNpAuth::NpUnavailabilityReason c_npReason = g_rlNp.GetNpAuth().GetNpUnavailabilityReason( NetworkInterface::GetLocalGamerIndex() );
			uiDisplayf("CPlayerListMenu::HandleNoPlayers - Network is not available. Reason code %d", c_npReason );

			if( c_npReason != rlNpAuth::NP_REASON_INVALID )
			{
				ShowColumnWarning_EX(NoPlayersColumns(),  CPauseMenu::GetOfflineReasonAsLocKey(), "CWS_WARNING" );
			}
		}
		else
#endif
		if (NetworkInterface::IsLocalPlayerSignedIn())
		{
			ShowColumnWarning_EX(NoPlayersColumns(), "NOT_IN_OFFLINE_MODE", "WARNING_NOT_CONNECTED_TITLE");
		}
		else
		{
			ShowColumnWarning_EX(NoPlayersColumns(), "NOT_CONNECTED", "WARNING_NOT_CONNECTED_TITLE");
		}
	}
	else if(CLiveManager::CheckIsAgeRestricted())
	{
		ShowColumnWarning_EX(NoPlayersColumns(), "HUD_AGERES", "CWS_WARNING");
	}
	else
	{
		SUIContexts::Activate(ATSTRINGHASH("HIDE_ACCEPTBUTTON",0x14211b54));
		BasePlayerCardDataManager* pManager = GetDataMgr();
		
		if (rlFriendsManager::ActivationFailed())
		{
			ShowColumnWarning_EX(NoPlayersColumns(), GetFriendErrorText(), GetNoResultsTitle());
		}
		else if(uiVerify(pManager) && pManager->HadError())
		{
			ShowColumnWarning_EX(NoPlayersColumns(), "PCARD_SYNC_ERROR", "PCARD_SYNC_ERROR_TITLE");
		}

		else
		{
			ShowColumnWarning_EX(NoPlayersColumns(), GetNoResultsText(), GetNoResultsTitle());
		}
	}

	CPauseMenu::GetDisplayPed()->SetVisible(false);

	CPauseMenu::RefreshSocialClubContext();
	CPauseMenu::RedrawInstructionalButtons();
}

void CPlayerListMenu::HandleLaunchingGame()
{
	SET_DATA_SLOT_EMPTY(PLAYER_LIST_COLUMN);
	SET_DATA_SLOT_EMPTY(PLAYER_CARD_COLUMN);
	SET_DATA_SLOT_EMPTY(PLAYER_CARD_LOCAL_PLAYER_COMP_COLUMN);
	SET_DATA_SLOT_EMPTY(PLAYER_CARD_OTHER_PLAYER_COMP_COLUMN);

	SUIContexts::Activate(ATSTRINGHASH("HIDE_ACCEPTBUTTON",0x14211b54));
	ShowColumnWarning_EX(NoPlayersColumns(), "FM_COR_AUTOD", "PCARD_SYNC_ERROR_TITLE");
	
	CPauseMenu::GetDisplayPed()->SetVisible(false);

	CPauseMenu::RefreshSocialClubContext();
	CPauseMenu::RedrawInstructionalButtons();
}
const rlGamerHandle& CPlayerListMenu::GetGamerHandle(int playerIndex)
{
	if(playerIndex == PLAYER_CARD_LOCAL_PLAYER_ID && NetworkInterface::GetActiveGamerInfo())
	{
		return NetworkInterface::GetActiveGamerInfo()->GetGamerHandle();
	}
	else
	{
		BasePlayerCardDataManager* pManager = GetDataMgr();
		if(uiVerify(pManager) &&
			uiVerifyf(0 <= playerIndex && playerIndex < pManager->GetReadGamerCount(), "CPlayerListMenu::GetGamerHandle - playerIndex=%d, GetReadGamerCount=%d", playerIndex, pManager->GetReadGamerCount()))
		{
			return pManager->GetGamerHandle(playerIndex);
		}
	}

	return RL_INVALID_GAMER_HANDLE;
}

ActivePlayerIndex CPlayerListMenu::GetActivePlayerIndex(int playerIndex)
{
	return GetActivePlayerIndex(GetGamerHandle(playerIndex));
}

ActivePlayerIndex CPlayerListMenu::GetActivePlayerIndex(const rlGamerHandle& gamerHandle)
{
	ActivePlayerIndex api = MAX_NUM_ACTIVE_PLAYERS;
	if(NetworkInterface::IsNetworkOpen())
	{
		api = NetworkInterface::GetActivePlayerIndexFromGamerHandle(gamerHandle);
	}

	return api;
}

bool CPlayerListMenu::IsActivePlayer(int playerIndex)
{
	return IsLocalPlayer(playerIndex) || (GetActivePlayerIndex(playerIndex) != MAX_NUM_ACTIVE_PLAYERS);
}

bool CPlayerListMenu::IsActivePlayer(const rlGamerHandle& gamerHandle)
{
	return IsLocalPlayer(gamerHandle) || (GetActivePlayerIndex(gamerHandle) != MAX_NUM_ACTIVE_PLAYERS);
}

bool CPlayerListMenu::IsLocalPlayer(int playerIndex)
{
	return playerIndex == PLAYER_CARD_LOCAL_PLAYER_ID || IsLocalPlayer(GetGamerHandle(playerIndex));
}

bool CPlayerListMenu::IsLocalPlayer(const rlGamerHandle& gamerHandle)
{
	return CContextMenuHelper::IsLocalPlayer(gamerHandle);
}

const CNetGamePlayer* CPlayerListMenu::GetLocalPlayer()
{
	const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();
	return pInfo ? NetworkInterface::GetPlayerFromGamerHandle(pInfo->GetGamerHandle()) : NULL;
}

CNetGamePlayer* CPlayerListMenu::GetNetGamePlayer(int playerIndex)
{
	if(playerIndex == PLAYER_CARD_LOCAL_PLAYER_ID)
	{
		if(uiVerify(NetworkInterface::GetActiveGamerInfo()))
		{
			return NetworkInterface::GetPlayerFromGamerHandle(NetworkInterface::GetActiveGamerInfo()->GetGamerHandle());
		}
	}
	else
	{
		BasePlayerCardDataManager* pManager = GetDataMgr();
		if(pManager && uiVerifyf(0 <= playerIndex && playerIndex < pManager->GetReadGamerCount(), "CPlayerListMenu::GetNetGamePlayer - playerIndex(%d) is out of range, max=%d", playerIndex, pManager->GetReadGamerCount()))
		{
			rlGamerHandle gamerHandle = pManager->GetGamerHandle(playerIndex);
			return NetworkInterface::GetPlayerFromGamerHandle(gamerHandle);
		}
	}

	return NULL;
}

CNetObjPlayer* CPlayerListMenu::GetNetObjPlayer(int playerIndex)
{
	CNetGamePlayer* pNetPlayer = GetNetGamePlayer(playerIndex);
	return (pNetPlayer && pNetPlayer->GetPlayerPed()) ? static_cast<CNetObjPlayer*>(pNetPlayer->GetPlayerPed()->GetNetworkObject()) : NULL;
}

CNetObjPlayer* CPlayerListMenu::GetNetObjPlayer(const rlGamerHandle& gamerHandle)
{
	CNetGamePlayer* pNetPlayer = NetworkInterface::GetPlayerFromGamerHandle(gamerHandle);
	return (pNetPlayer && pNetPlayer->GetPlayerPed()) ? static_cast<CNetObjPlayer*>(pNetPlayer->GetPlayerPed()->GetNetworkObject()) : NULL;

}

void CPlayerListMenu::BuildContexts()
{
	BasePlayerCardDataManager* pManager = GetDataMgr();
	
	// if no results, just bail
	if( pManager->GetReadGamerCount() <= 0 )
	{
		SUIContexts::Activate("NO_RESULTS");
		return;
	}
	
	if ( m_WasSystemError )
	{
		// Not the most ideal solution.  Better would be to not build the context menus at all if dynamic pages are in a "system error" or otherwise "error" state.  However, this appears to be the desired
		// path in the existing set up - bailing without populating the context list is tantimount do denying opening the list.
		return;	
	}

	if(uiVerify(pManager) && uiVerifyf(m_currPlayerIndex < pManager->GetReadGamerCount(), "CPlayerListMenu::BuildContexts - m_currPlayerIndex=%d gamerCount=%d", m_currPlayerIndex, pManager->GetReadGamerCount()))
	{
		const rlGamerHandle& gamerHandle = pManager->GetGamerHandle(m_currPlayerIndex);
		const GamerData& gamerData = pManager->GetGamerData(m_currPlayerIndex);

		if(gamerData.m_isInSameTitleOnline)
		{
			SUIContexts::Activate("PlayerIsSameTitleOnline");
		}

        
		if( !gamerData.m_isInSameSession
#if !__FINAL
			&& (!PARAM_scriptCodeDev.Get() && !PARAM_earlyInvites.Get())
#endif
			)
		{
            bool jipBlocked = false;

            // url:bugstar:6859775 - Stat check is on a tunable to ease load for SCS
            const bool bCheckTutorialStat = ShouldCheckStatsForInviteOrJIP(); 
            if( bCheckTutorialStat )
            {
                // Local Tut check
                {
                    bool hasLocalBeatTut = false;

                    for(int charSlot = CHAR_MP_START; charSlot<=CHAR_MP_END; ++charSlot)
                    {
                        int hash = GetMiscData().m_jipUnlockStat.GetStat(charSlot-CHAR_MP_START);
                        if(StatsInterface::IsKeyValid(hash) && StatsInterface::GetBooleanStat(hash))
                        {
                            hasLocalBeatTut = true;
                            break;
                        }
                    }

                    if (!hasLocalBeatTut)
                    {
                        scriptDebugf3("[JIPLOCK TEST] Local player has not completed tutorial.   Failed on GetMiscData().m_jipUnlockStat.GetStat()");
                    }

                    if(!hasLocalBeatTut)
                    {
                        CProfileSettings& settings = CProfileSettings::GetInstance();
                        if(settings.AreSettingsValid())
                        {
                            const int maxCharSlots = 5;
                            for(int charSlot = 0; charSlot<maxCharSlots; ++charSlot)
                            {
                                CProfileSettings::ProfileSettingId id = static_cast<CProfileSettings::ProfileSettingId>(CProfileSettings::FREEMODE_PROLOGUE_COMPLETE_CHAR0 + charSlot);
                                if(settings.Exists(id) && (settings.GetInt(id) & InviteMgr::PSFP_FM_INTRO_CAN_ACCEPT_INVITES))
                                {
                                    hasLocalBeatTut = true;
                                    break;
                                }
                            }
                        }
                    }

                    if(!hasLocalBeatTut)
                    {
                        scriptDebugf3("[JIPLOCK TEST] Local player has not completed tutorial.   Activating JIPLocked context.");
                        jipBlocked = true;
                        SUIContexts::Activate("LocalJIPLocked");
                    }
                }

                // Remote Tut check
                {
                    bool hasRemoteBeatTut = false;


                    for(int charSlot = CHAR_MP_START; charSlot < (CHAR_MP_START + MAX_NUM_MP_ACTIVE_CHARS); ++charSlot)
                    {
                        int hash = GetMiscData().m_jipUnlockStat.GetStat(charSlot-CHAR_MP_START);
                        if(GetBoolStat(m_currPlayerIndex, hash, false))
                        {
                            hasRemoteBeatTut = true;
                            break;
                        }
                    }

                    if(!hasRemoteBeatTut)
                    {
                        scriptDebugf3("[JIPLOCK TEST] Remote player has not completed tutorial.   Activating JIPLocked context.");
                        jipBlocked = true;
                        SUIContexts::Activate("RemoteJIPLocked");
                    }
                }
            }

#if RSG_ORBIS
			// Lock JIP if you have no PS+ subscription
			if (!CLiveManager::HasPlatformSubscription())
			{
				jipBlocked = true;
			}

			rlFriendsPage* p = CLiveManager::GetFriendsPage();
			rlFriend* f = p->GetFriend(gamerHandle);
			if (f && f->IsPlayingLastGen())
			{
				jipBlocked = true;
			}
#endif

			if(jipBlocked)
			{
				SUIContexts::Activate("JIPLocked");
			}
		}

		// Remote player in tutorial session check (this differs than the check above, which determines if AT LEAST ONE character has beaten the tutorial)
		CNetGamePlayer* pRemotePlayer = NetworkInterface::GetPlayerFromGamerHandle(gamerHandle);
		SUIContexts::SetActive("RemoteInTutorialSession", pRemotePlayer && NetworkInterface::IsPlayerInTutorialSession(*pRemotePlayer));

		if (NetworkInterface::GetActivePlayerIndexFromGamerHandle(gamerHandle) != MAX_NUM_ACTIVE_PLAYERS)
		{

			CNetObjPlayer* pObjPlayer = GetNetObjPlayer(gamerHandle);

			if (pObjPlayer && pObjPlayer->GetStatVersion() != 0)
			{
				// Remote player allows others to spectate them ?
				SUIContexts::SetActive("RemoteAllowsSpectating", GetBoolStat(m_currPlayerIndex, GetMiscData().m_allowsSpectatingStat, false));
				// Remote player is already spectating someone ?
				SUIContexts::SetActive("RemoteIsSpectating", GetBoolStat(m_currPlayerIndex, GetMiscData().m_isSpectatingStat, false));
			}
		}

		bool bDisableFriendSpectate = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("DISABLE_SCTV_FRIENDS_SPECTATE", 0x74afe8f8), false);
		bool bSCTVGTAOTV = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("DISABLE_SCTV_GTAO_TV", 0xb715b652), false);

		if ( NetworkInterface::IsRockstarDev() && bSCTVGTAOTV)
		{
			SUIContexts::Activate("DisableSpectate");
		}
		else if (!NetworkInterface::IsRockstarDev() && bDisableFriendSpectate)
		{
			SUIContexts::Activate("DisableSpectate");
		}
		else
		{
			// Remote player allows others to spectate them ?
			SUIContexts::SetActive("RemoteAllowsSpectating", GetBoolStat(m_currPlayerIndex, GetMiscData().m_allowsSpectatingStat, false));
		}

        //@@: location CPLAYERLISTMENU_BUILDCONTEXTS_SET_REMOTE_SPECTATING
		// Remote player is already spectating someone ?
		SUIContexts::SetActive("RemoteIsSpectating", GetBoolStat(m_currPlayerIndex, GetMiscData().m_isSpectatingStat, false));

		// Local player is in an "Activity" session
		SUIContexts::SetActive("LocalInActivitySession", NetworkInterface::IsActivitySession());
		
		if(NetworkInterface::IsNetworkOpen() && (StatsInterface::IsBadSport() || CNetwork::IsCheater()))
		{
			SUIContexts::Activate("LocalIsBadSport");
		}

		BuildContexts(gamerHandle, pManager->GetIsPrimaryClan());
	}
}


bool CPlayerListMenu::CanInviteToCrew() const
{
	// this is KIND of lazy, but hey, it'll work!
	// push contexts, then check if we've passed the checks for invites
	SUICONTEXT_AUTO_PUSH();
	// warning! CONST-KLUDGE INBOUND
	const_cast<CPlayerListMenu*>(this)->BuildContexts();

#if RSG_XBOX || RSG_XENON || RSG_PC
	// Xbox TRC TCR 094 - use every gamer index
	if( CLiveManager::CheckUserContentPrivileges(GAMER_INDEX_EVERYONE))
#elif RSG_SCE || RSG_PS3
	// PlayStation R4063 | R5063 - use the local gamer index
	if( CLiveManager::CheckUserContentPrivileges(GAMER_INDEX_LOCAL))
#endif
	{
		return SUIContexts::IsActive("CanInviteToCrew");
	}

	return false;
	
}


void CPlayerListMenu::InitPaginator(MenuScreenId newScreenId)
{
	playerlistDebugf1("[CPlayerListMenu] CPlayerListMenu::InitPaginator");
	Paginator* pPaginator = GetPaginator();
	if(pPaginator)
	{
		if(pPaginator->HasBeenInitialized())
		{
			pPaginator->Shutdown();
		}
		
		int const c_maxPlayerSlotCount = GetMaxPlayerSlotCount();

		playerlistDebugf1("[CPlayerListMenu] CPlayerListMenu::InitPaginator - Init the paginator and disable focus on first item.");
		pPaginator->Init(this, c_maxPlayerSlotCount, newScreenId, PLAYER_LIST_COLUMN);
		pPaginator->UpdateItemSelection(0, false);
	}
}

int CPlayerListMenu::GetMaxPlayerSlotCount() const 
{
	return GetMiscData().m_maxMenuSlots;
}

bool CPlayerListMenu::IsPaginatorValid() const
{
	return !m_isEmpty && m_isPopulated && (GetPaginator() != NULL);
}

void CPlayerListMenu::BuildContexts(const rlGamerHandle& gamerHandle, bool bSuppressCrewInvite)
{
	CContextMenuHelper::BuildPlayerContexts(gamerHandle, bSuppressCrewInvite, GetClanData(gamerHandle));
}

bool CPlayerListMenu::HandleContextOption(atHashWithStringBank contextOption)
{
	bool isValid = false;

	BasePlayerCardDataManager* pManager = GetDataMgr();
	if(m_isEmpty ||
		(uiVerify(pManager) && uiVerifyf(m_currPlayerIndex < pManager->GetReadGamerCount(), "CPlayerListMenu::HandleContextOption - m_currPlayerIndex=%d gamerCount=%d", m_currPlayerIndex, pManager->GetReadGamerCount())))
	{
		isValid = true;
		const rlGamerHandle& gamerHandle = m_isEmpty ? RL_INVALID_GAMER_HANDLE : pManager->GetGamerHandle(m_currPlayerIndex);

		const char* pName = m_isEmpty ? "Player" : pManager->GetName(m_currPlayerIndex);
		if(!uiVerify(pName))
		{
			pName = "";
		}


#if __ASSERT || !__NO_OUTPUT
		const char* pszDebugConfirmedName = atHashString(contextOption).TryGetCStr() ? atHashString(contextOption).TryGetCStr() : "-no hash-";
#endif

		if( CContextMenuHelper::HandlePlayerContextOptions(contextOption, gamerHandle))
			return true;

		//////////////////////////////////////////////////////////////////////////
		if( contextOption == ATSTRINGHASH("JOIN_GAME",0x64536f9)
			|| contextOption == ATSTRINGHASH("JOIN_PARTY",0x644fa1bd))
		{
			InviteMgr& inviteMgr = CLiveManager::GetInviteMgr();
			const int inviteIndex = inviteMgr.GetUnacceptedInviteIndex(gamerHandle);
			if(inviteIndex < inviteMgr.GetNumUnacceptedInvites())
			{
				inviteMgr.AcceptInvite(inviteIndex);
				SetupCenterColumnWarning("PCARD_JOIN_GAME", "PCARD_JIP_TITLE");
			}
			//			else
			//			{
			//				// Add warning saying the invite has expired.
			//			}
		}
		//////////////////////////////////////////////////////////////////////////
		else if( contextOption == ATSTRINGHASH("JIP_GAME",0xda4858c1))
		{
			#if RSG_ORBIS
			CWarningMessage::Data newMessage;
			newMessage.m_TextLabelHeading = "CWS_WARNING";
			if (!CLiveManager::CheckOnlinePrivileges())
			{
				newMessage.m_TextLabelBody = "HUD_PERM";  
				newMessage.m_iFlags = FE_WARNING_OK;
				newMessage.m_bCloseAfterPress = true;
				newMessage.m_acceptPressed = datCallback( MFA(CPlayerListMenu::GoBack), this);

				if (uiVerify(CPauseMenu::DynamicMenuExists()))
				{
					CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().SetMessage(newMessage);
				}

				return false;
			}
			else if(!CLiveManager::HasPlatformSubscription())
			{
				CLiveManager::ShowAccountUpgradeUI();

				newMessage.m_TextLabelBody = "HUD_PSPLUS2";
				newMessage.m_iFlags = FE_WARNING_OK;
				newMessage.m_bCloseAfterPress = true;
				newMessage.m_acceptPressed = datCallback( MFA(CPlayerListMenu::GoBack), this);

				if (uiVerify(CPauseMenu::DynamicMenuExists()))
				{
					CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().SetMessage(newMessage);
				}

				return false;
			}
			else
			#endif
			{
				InitJIPSyncing(false);
			}
		}
		else if( contextOption == ATSTRINGHASH("JIP_GAME_DIS",0x265c0624) )
		{
			CWarningMessage::Data newMessage;
			newMessage.m_TextLabelHeading = "CWS_WARNING";

			#if RSG_ORBIS
			if(!CLiveManager::HasPlatformSubscription())
			{
                CLiveManager::ShowAccountUpgradeUI();

				newMessage.m_TextLabelBody = "HUD_PSPLUS2";
				newMessage.m_iFlags = FE_WARNING_OK;
				newMessage.m_bCloseAfterPress = true;
				newMessage.m_acceptPressed = datCallback( MFA(CPlayerListMenu::GoBack), this);
				CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().SetMessage(newMessage);
	
				return false;
			}
			else
			#endif
			if (!CLiveManager::CheckOnlinePrivileges())
			{
				newMessage.m_TextLabelBody = "HUD_PERM";  // fix for 1686784
			}
			else
			{
				newMessage.m_TextLabelBody = "CWS_JIP_DIS";
			}

			newMessage.m_iFlags = FE_WARNING_OK;
			newMessage.m_acceptPressed = datCallback(MFA(CWarningMessage::Clear), &CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage());
			CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().SetMessage(newMessage);
		}
		//////////////////////////////////////////////////////////////////////////
		else if( contextOption == ATSTRINGHASH("JIP_GAME_TRANSITION",0xcc6ad7da))
		{
			const CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(gamerHandle);
			if(pPlayer && uiVerify(pPlayer->HasStartedTransition()) && uiVerify(pPlayer->GetTransitionSessionInfo().IsValid()) && uiVerify(NetworkInterface::GetActiveGamerInfo()))
			{
				CLiveManager::GetInviteMgr().HandleJoinRequest(pPlayer->GetTransitionSessionInfo(), *NetworkInterface::GetActiveGamerInfo(), gamerHandle, InviteFlags::IF_None);
				SetupCenterColumnWarning("PCARD_JOIN_GAME", "PCARD_JIP_TITLE");
			}
			else
			{
				InitJIPSyncing(true);
			}
		}
		//////////////////////////////////////////////////////////////////////////
		else if( contextOption == ATSTRINGHASH("SPECTATE_OTHER_SESSION",0xbfcca007) )
		{
            // if this is a local player (same session) we should just inform script
            if(NetworkInterface::GetPlayerFromGamerHandle(gamerHandle))
            {
                CEventNetworkSpectateLocal scrEvent(gamerHandle);
                GetEventScriptNetworkGroup()->Add(scrEvent);
            }
            else
            {
				InitJIPSyncing(true);
			}
		}
		
		//////////////////////////////////////////////////////////////////////////
		else if( contextOption == ATSTRINGHASH("SEND_PARTY_INVITE",0x8e32528e) )
		{
			/*if(!NetworkInterface::IsInParty())
			{
				NetworkInterface::HostParty();
			}

			m_inviteTimer.Start();

			CWarningMessage::Data newMessage;
			newMessage.m_TextLabelHeading = "CWS_WARNING";
			newMessage.m_TextLabelBody = "CWS_PARTY_INVITE";
			newMessage.m_iFlags = FE_WARNING_CANCEL;
			newMessage.m_onUpdate = datCallback(MFA(CPlayerListMenu::CreatingPartyCallback), this);
			newMessage.m_declinePressed = datCallback(MFA(CWarningMessage::Clear), &CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage());
			CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().SetMessage(newMessage);*/
		}

		//////////////////////////////////////////////////////////////////////////
		else if( contextOption == ATSTRINGHASH("KICK_PARTY",0x9c6fd013) )
		{
			/*if(uiVerify(NetworkInterface::IsInParty()) &&
				uiVerify(NetworkInterface::IsPartyMember(&gamerHandle)))
			{
				m_inviteTimer.Start();

				CWarningMessage::Data newMessage;
				newMessage.m_TextLabelHeading = "CWS_WARNING";
				newMessage.m_TextLabelBody = "CWS_PARTY_KICK";
				newMessage.m_iFlags = FE_WARNING_CANCEL;
				newMessage.m_onUpdate = datCallback(MFA(CPlayerListMenu::KickPartyCallback), this);
				newMessage.m_declinePressed = datCallback(MFA(CWarningMessage::Clear), &CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage());
				CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().SetMessage(newMessage);
			}*/
		}

		//////////////////////////////////////////////////////////////////////////
		else if( contextOption == ATSTRINGHASH("SEND_GAME_INVITE",0xe1e8d5dc) )
		{
			if(!CNetwork::GetNetworkSession().IsSoloSession(SessionType::ST_Physical) &&
				(CNetwork::GetNetworkSession().GetSnSession().GetEmptySlots(RL_SLOT_PRIVATE) > 0 ||
				 CNetwork::GetNetworkSession().GetSnSession().GetEmptySlots(RL_SLOT_PUBLIC) > 0))
			{
				// track whether we are using a presence invite
				bool usingPresenceInvite = false;

#define NET_SUPPORT_FREEROAM_PRESENCE_INVITES (0)
#if NET_SUPPORT_FREEROAM_PRESENCE_INVITES
				// check if this friend is online and in title, if so, send a presence invite
				const rlFriend* pFriend = CLiveManager::GetFriendsPage()->GetFriend(gamerHandle);
				if(pFriend && pFriend->IsInSameTitle() && pFriend->IsOnline())
				{
					uiDebugf1("CPlayerListMenu::HandleContextOption - Attempting presence invite to %s", gamerHandle.ToString());
					const bool isImportant = true;
					usingPresenceInvite = CNetwork::GetNetworkSession().SendPresenceInvites(&gamerHandle, &isImportant, 1, nullptr, nullptr, 0, 0, 0, 0);
				}
#endif
				if(!usingPresenceInvite)
				{
					uiDebugf1("CPlayerListMenu::HandleContextOption - Sending platform invite to %s", gamerHandle.ToString());
					CNetwork::GetNetworkSession().SendInvites(&gamerHandle, 1);
				}

				SetupCenterColumnWarning("PCARD_SEND_INVT_TEXT", "PCARD_SEND_INVT_TITLE");
			}
			else
			{
				CWarningMessage::Data newMessage;
				newMessage.m_TextLabelHeading = "CWS_WARNING";
				newMessage.m_TextLabelBody = CNetwork::GetNetworkSession().IsSoloSession(SessionType::ST_Physical) ? "CWS_SOLO_SESSION" : "CWS_FULL_SESSION";
				newMessage.m_iFlags = FE_WARNING_OK;
				newMessage.m_acceptPressed = datCallback(MFA(CWarningMessage::Clear), &CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage());
				CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().SetMessage(newMessage);
			}
		}
		//////////////////////////////////////////////////////////////////////////
		else if( contextOption == ATSTRINGHASH("SEND_GAME_INVITE_DIS",0xf806e810) )
		{
			CWarningMessage::Data newMessage;
			newMessage.m_TextLabelHeading = "CWS_WARNING";
			newMessage.m_TextLabelBody = "CWS_INVITE_DIS";
			newMessage.m_iFlags = FE_WARNING_OK;
			newMessage.m_acceptPressed = datCallback(MFA(CWarningMessage::Clear), &CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage());
			CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().SetMessage(newMessage);
		}
		//////////////////////////////////////////////////////////////////////////
		else if( contextOption == ATSTRINGHASH("SEND_CREW_INVITE",0xd6110a7b) )
		{
			isValid = false;
			int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
			if(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
			{
				if(CLiveManager::GetNetworkClan().CanSendInvite(gamerHandle))
				{
					rlClanDesc primClan = rlClan::GetPrimaryClan(localGamerIndex);
					if(primClan.IsValid())
					{
						NetAction& rAction = CPauseMenu::GetDynamicPauseMenu()->GetNetAction();

						isValid = rlClan::Invite(localGamerIndex, primClan.m_Id, -1, true, gamerHandle,
							rAction.Set("CRW_INVITE", pManager->GetName(m_currPlayerIndex), primClan.m_ClanName, NullCB, RL_CLAN_ERR_ALREADY_IN_CLAN)
						);

						if(isValid)
						{
							NETWORK_CLAN_INST.RefreshJoinRequestsReceived();
							m_hasSentCrewInvite = true;
							SetPlayerCard(m_currPlayerIndex);
						}
					}
				}
			}
		}
		//////////////////////////////////////////////////////////////////////////
		else if( contextOption == ATSTRINGHASH("CORONA_KICK",0xfbfd3d10) )
		{
			OnInputCancleInvite(PAD_SQUARE);
		}
		else
		{
			uiAssertf(0, "CPlayerListMenu::HandleContextOption - Unknown Context Option %s (0x%08x)", pszDebugConfirmedName, contextOption.GetHash());
			isValid = false;
		}

#if !__NO_OUTPUT
		uiDisplayf("CPlayerListMenu::HandleContextOption - %s (0x%08x) with %s", pszDebugConfirmedName, contextOption.GetHash(), pName);
#endif
	}

	return isValid;
}

void CPlayerListMenu::InitJIPSyncing(bool isSpectating)
{
	bool success = false;

	BasePlayerCardDataManager* pManager = GetDataMgr();
	if(uiVerify(pManager) && uiVerify(!m_isEmpty) &&
		uiVerify(m_currPlayerIndex != PLAYER_CARD_INVALID_PLAYER_ID) &&
		uiVerifyf(m_currPlayerIndex < pManager->GetReadGamerCount(), "CPlayerListMenu::InitJIPSyncing - m_currPlayerIndex=%d, GetReadGamerCount=%d", m_currPlayerIndex, pManager->GetReadGamerCount()) &&
		uiVerify(!m_jipSessionStatus.Pending()))
	{
		rlGamerHandle gamerHandle = GetGamerHandle(m_currPlayerIndex);

		m_jipSessionStatus.Reset();
		m_sessionQuery.Clear();
		m_numSessionFound = 0;

		if(rlPresenceQuery::GetSessionByGamerHandle(NetworkInterface::GetActiveGamerInfo()->GetLocalIndex(),
			&gamerHandle,
			1,
			&m_sessionQuery,
			1,
			&m_numSessionFound,
			&m_jipSessionStatus))
		{
			m_inviteTimer.Start();

			CWarningMessage::Data newMessage;
			newMessage.m_TextLabelHeading = "CWS_WARNING";
			newMessage.m_TextLabelBody = "NT_INV_CONFIG";
			newMessage.m_iFlags = FE_WARNING_CANCEL | FE_WARNING_SPINNER;
			newMessage.m_declinePressed = datCallback(MFA(CPlayerListMenu::CancelJIP), this);
			newMessage.m_onUpdate = datCallback(MFA1(CPlayerListMenu::SyncingJIPWarningCallback), this, reinterpret_cast<CallbackData>(isSpectating), false);
			CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().SetMessage(newMessage);

			success = true;
		}
	}

	if(!success)
	{
		SetupJIPFailedWarning(isSpectating);
	}
}

void CPlayerListMenu::SyncingJIPWarningCallback(CallbackData cdIsSpectating)
{
	if(!m_jipSessionStatus.Pending() && m_inviteTimer.IsComplete(PLAYER_CARD_INVITE_DELAY))
	{
		bool isSpectating = cdIsSpectating ? true : false;
		InviteFlags nInviteFlags = isSpectating ? InviteFlags::IF_Spectator : InviteFlags::IF_None;
	
		m_inviteTimer.Zero();

		if(m_jipSessionStatus.Succeeded() && m_numSessionFound && NetworkInterface::GetActiveGamerInfo())
		{
			CLiveManager::GetInviteMgr().HandleJoinRequest(m_sessionQuery.m_SessionInfo, *NetworkInterface::GetActiveGamerInfo(), m_sessionQuery.m_GamerHandle, nInviteFlags);
            
			if(!CPauseMenu::IsClosingDown())
            {
			    CWarningMessage::Data newMessage;
			    newMessage.m_TextLabelHeading = "CWS_WARNING";
			    newMessage.m_TextLabelBody = "NT_INV_CONFIG";
			    newMessage.m_iFlags = FE_WARNING_CANCEL | FE_WARNING_SPINNER;
			    newMessage.m_declinePressed = datCallback(MFA(CPlayerListMenu::CancelJIP), this);
			    newMessage.m_onUpdate = datCallback(MFA(CPlayerListMenu::InitingInviteMsgJIPWarningCallback), this);
			    CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().SetMessage(newMessage);
            }
            else
            {
                CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().Clear();
            }
		}
		else
		{
			uiErrorf("CPlayerListMenu::SyncingJIPWarningCallback - Failed to sync session data. Succeeded=%d, ResultCode=%d, m_numSessionFound=%d, hasActiveGamerInfo=%d", m_jipSessionStatus.Succeeded(), m_jipSessionStatus.GetResultCode(), m_numSessionFound, NetworkInterface::GetActiveGamerInfo() ? 0 : 1);
			SetupJIPFailedWarning(isSpectating);
		}
	}
}

void CPlayerListMenu::InitingInviteMsgJIPWarningCallback()
{
	if(CLiveManager::GetInviteMgr().GetAcceptedInvite() && !CLiveManager::GetInviteMgr().GetAcceptedInvite()->IsProcessing())
	{
		CLiveManager::GetInviteMgr().SetShowingConfirmationExternally();
		CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().Clear();
	}
}

void CPlayerListMenu::CancelJIPSessionStatusRequest()
{
	if(m_jipSessionStatus.Pending())
	{
		netTask::Cancel(&m_jipSessionStatus);
	}
	m_jipSessionStatus.Reset();
}

void CPlayerListMenu::SetupJIPFailedWarning(bool isSpectating)
{
	CancelJIPSessionStatusRequest();

	CWarningMessage::Data newMessage;
	newMessage.m_TextLabelHeading = "CWS_WARNING";
	newMessage.m_TextLabelBody = isSpectating ? "CWS_SPEC_FAILED" : "CWS_JIP_FAILED";
	newMessage.m_iFlags = FE_WARNING_OK;
	newMessage.m_acceptPressed = datCallback(MFA(CWarningMessage::Clear), &CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage());
	CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().SetMessage(newMessage);
}

void CPlayerListMenu::CancelJIP()
{
	CancelJIPSessionStatusRequest();

	if(CLiveManager::GetInviteMgr().HasPendingAcceptedInvite())
	{
		CLiveManager::GetInviteMgr().CancelInvite();
	}

	CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().Clear();
}

void CPlayerListMenu::CreatingPartyCallback()
{
	/*if(!NetworkInterface::IsPartyStatusPending())
	{
		bool inviteSent = false;

		BasePlayerCardDataManager* pManager = GetDataMgr();
		if(m_currPlayerIndex < pManager->GetReadGamerCount())
		{
			inviteSent = CNetwork::GetNetworkSession().SendPartyInvites(&pManager->GetGamerHandle(m_currPlayerIndex), 1);
		}

		if(inviteSent)
		{
			CWarningMessage::Data newMessage;
			newMessage.m_TextLabelHeading = "CWS_WARNING";
			newMessage.m_TextLabelBody = "CWS_PARTY_INVITE";
			newMessage.m_iFlags = FE_WARNING_CANCEL;
			newMessage.m_onUpdate = datCallback(MFA(CPlayerListMenu::DisplayingMessageCallback), this);
			newMessage.m_declinePressed = datCallback(MFA(CWarningMessage::Clear), &CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage());
			CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().SetMessage(newMessage);
		}
		else
		{
			m_inviteTimer.Zero();

			CWarningMessage::Data newMessage;
			newMessage.m_TextLabelHeading = "CWS_WARNING";
			newMessage.m_TextLabelBody = "PRTY_INVT_FAIL";
			newMessage.m_iFlags = FE_WARNING_OK;
			newMessage.m_acceptPressed = datCallback(MFA(CWarningMessage::Clear), &CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage());
			CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().SetMessage(newMessage);
		}
	}*/
}

void CPlayerListMenu::KickPartyCallback()
{
	/*if(!NetworkInterface::IsPartyStatusPending())
	{
		bool kickSent = false;

		BasePlayerCardDataManager* pManager = GetDataMgr();
		if(m_currPlayerIndex < pManager->GetReadGamerCount())
		{
			kickSent = CNetwork::GetNetworkSession().KickPartyMembers(&pManager->GetGamerHandle(m_currPlayerIndex), 1);
		}

		if(kickSent)
		{
			CWarningMessage::Data newMessage;
			newMessage.m_TextLabelHeading = "CWS_WARNING";
			newMessage.m_TextLabelBody = "CWS_PARTY_KICK";
			newMessage.m_iFlags = FE_WARNING_CANCEL;
			newMessage.m_onUpdate = datCallback(MFA(CPlayerListMenu::DisplayingMessageCallback), this);
			newMessage.m_declinePressed = datCallback(MFA(CWarningMessage::Clear), &CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage());
			CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().SetMessage(newMessage);
		}
		else
		{
			m_inviteTimer.Zero();

			CWarningMessage::Data newMessage;
			newMessage.m_TextLabelHeading = "CWS_WARNING";
			newMessage.m_TextLabelBody = "PRTY_KICK_FAIL";
			newMessage.m_iFlags = FE_WARNING_OK;
			newMessage.m_acceptPressed = datCallback(MFA(CWarningMessage::Clear), &CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage());
			CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().SetMessage(newMessage);
		}
	}*/
}

void CPlayerListMenu::DisplayingMessageCallback()
{
	if(m_inviteTimer.IsComplete(PLAYER_CARD_INVITE_DELAY))
	{
		m_inviteTimer.Zero();
		CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().Clear();
	}
}

bool CPlayerListMenu::TriggerEvent(MenuScreenId OUTPUT_ONLY(MenuId), s32 OUTPUT_ONLY(iUniqueId))
{
	uiDisplayf("CPlayerListMenu::TriggerEvent - MenuId=%d, iUniqueId=%d", MenuId.GetValue(), iUniqueId);
	SetIsInPlayerList(true);

	//CScriptMenu::TriggerEvent(MenuId, iUniqueId); // We don't want to call this version of the function.
	return false;
}

void CPlayerListMenu::SetPlayerCard(int playerIndex, bool refreshAvatar)
{
	BasePlayerCardDataManager* pManager = GetDataMgr();
	if(pManager && !m_isEmpty && playerIndex != PLAYER_CARD_INVALID_PLAYER_ID && playerIndex < pManager->GetReadGamerCount())
	{
		if(m_currPlayerIndex != playerIndex)
		{
			refreshAvatar = true;

			//KillScript();
			m_killScriptPending = true;
			ClearAvatarSpinner();
			CPauseMenu::GetDisplayPed()->ClearPed(false);


			m_hasClanData = false;
			m_hasSentCrewInvite = false;
			SetCurrentlyViewedPlayer(playerIndex);
			SPlayerCardManager::GetInstance().SetCurrentGamer(GetGamerHandle(playerIndex));
			SPlayerCardManager::GetInstance().SetCrewStatus(CPlayerCardManager::CREWSTATUS_PENDING);
			SPlayerCardManager::GetInstance().SetClanId(RL_INVALID_CLAN_ID);
		}

		if(pManager->IsValid() || pManager->HadError())
		{
			RefreshButtonPrompts();

			SetupHelpSlot(playerIndex);
			if(SetupPlayerCardColumns(playerIndex))
			{
				SetupAvatarColumn(playerIndex);

				if(refreshAvatar)
				{
					SetupAvatar(playerIndex);
				}
			}
		}
	}
}

void CPlayerListMenu::RefreshButtonPrompts()
{
	BasePlayerCardDataManager* pManager = GetDataMgr();
	if(pManager && !m_isEmpty && m_currPlayerIndex != PLAYER_CARD_INVALID_PLAYER_ID && m_currPlayerIndex < pManager->GetReadGamerCount())
	{
		if(pManager->IsValid() || pManager->HadError())
		{
			const rlGamerHandle& rGamerHandle = GetGamerHandle(m_currPlayerIndex);
			const GamerData& rGamerData = pManager->GetGamerData(m_currPlayerIndex);
			int charSlot = GetCharacterSlot(m_currPlayerIndex);
			bool isInvitedToTransition = false;
			bool isInTransition = false;

			SUIContexts::SetActive( CONTEXT_MENU_CONTEXT, GetIsInPlayerList() );
			SUIContexts::SetActive(LOCAL_PLAYER_CONTEXT, IsLocalPlayer(m_currPlayerIndex));
			SUIContexts::SetActive(ACTIVE_PLAYER_CONTEXT, IsActivePlayer(m_currPlayerIndex));
			SUIContexts::SetActive("SoloSession",CNetwork::GetNetworkSession().IsSoloSession(SessionType::ST_Physical));
			SUIContexts::Activate(HAS_PLAYERS_CONTEXT);
			

			SUIContexts::SetActive("PlayerIsOnline", rGamerData.m_isOnline);

#if RSG_PC
			// Cross-title invites not supported on PC
			SUIContexts::SetActive("PlayerIsInvitable", rGamerData.m_isOnline && rGamerData.m_isInSameTitle);
#else
			SUIContexts::SetActive("PlayerIsInvitable", rGamerData.m_isOnline);
#endif

			SUIContexts::SetActive("HasPlayedMode", HasPlayed(m_currPlayerIndex));
			SUIContexts::SetActive("SyncingStats", IsSyncingStats(m_currPlayerIndex, false));
			SUIContexts::SetActive("Comparing", m_isComparing);

#if __XENON
            // On 360, the app and Xbox mute are tied together. We always want the mute option. 
            SUIContexts::SetActive("MutedByOS", false);
#else	
			const VoiceGamerSettings* pVoiceSettings = NetworkInterface::GetVoice().FindGamer(rGamerHandle);
			SUIContexts::SetActive("MutedByOS", pVoiceSettings && (pVoiceSettings->GetLocalVoiceFlags() & VoiceGamerSettings::VOICE_LOCAL_MUTED) == VoiceGamerSettings::VOICE_LOCAL_MUTED);
#endif

			if(CNetwork::GetNetworkSession().IsTransitionActive())
			{
				if(CNetwork::GetNetworkSession().IsTransitionMember(rGamerHandle))
				{
					isInTransition = true;
				}

				const CNetworkInvitedPlayers::sInvitedGamer& invitedPlayer = CNetwork::GetNetworkSession().GetTransitionInvitedPlayers().GetInvitedPlayer(rGamerHandle);
				if(invitedPlayer.IsValid() &&
					(invitedPlayer.m_bHasAck || invitedPlayer.IsWithinAckThreshold()))
				{
					isInvitedToTransition = true;
				}
			}

            // url:bugstar:6859775 - Stat check is on a tunable to ease load for SCS
            const bool bCheckTutorialStat = ShouldCheckStatsForInviteOrJIP(); 
            if( bCheckTutorialStat &&
#if __FINAL
                true 
#else
			    !PARAM_scriptCodeDev.Get() && !PARAM_earlyInvites.Get() 
#endif
                )
			{
				if(IsSyncingStats(m_currPlayerIndex, false))
				{
					SUIContexts::SetActive("TutLock", true);
				}
				else
				{
					SUIContexts::SetActive("TutLock", !GetBoolStat(m_currPlayerIndex, GetMiscData().m_jipUnlockStat.GetStat(charSlot), false));
				}
			}

			SUIContexts::SetActive(TRANSITION_CONTEXT, isInTransition);
			SUIContexts::SetActive(INVITED_TRANSITION_CONTEXT, isInvitedToTransition);


			CPauseMenu::RedrawInstructionalButtons();
		}
	}
}

void CPlayerListMenu::SetPlayerCardForInviteMenu(int playerIndex, bool refreshAvatar /*= true*/)
{
	BasePlayerCardDataManager* pManager = GetDataMgr();
	if(pManager && playerIndex != PLAYER_CARD_INVALID_PLAYER_ID && playerIndex < pManager->GetReadGamerCount())
	{
		CPlayerListMenu::SetPlayerCard(playerIndex, refreshAvatar);

		CScaleformMenuHelper::SET_COLUMN_HIGHLIGHT(PLAYER_LIST_COLUMN, m_currPlayerIndex % GetMaxPlayerSlotCount() );
	}
}

bool CPlayerListMenu::SetupPlayerCardColumns(int playerIndex)
{
	bool showingColumn = false;
	bool showingLocalPlayer = false;

	if(!IsSyncingStats(playerIndex, true))
	{
		CScaleformMenuHelper::SET_BUSY_SPINNER(PM_COLUMN_MIDDLE_RIGHT, false);

		if(m_isComparing && GetIsInPlayerList() && !IsLocalPlayer(playerIndex))
		{
			SHOW_COLUMN(PLAYER_CARD_COLUMN, false);
			SET_DATA_SLOT_EMPTY(PLAYER_CARD_COLUMN);

			SetComparingVisible(true);
			if(SetupPlayerCardColumn(PLAYER_CARD_OTHER_PLAYER_COMP_COLUMN, playerIndex))
			{
				showingColumn = true;
				showingLocalPlayer = true;
				SetupPlayerCardColumn(PLAYER_CARD_LOCAL_PLAYER_COMP_COLUMN, PLAYER_CARD_LOCAL_PLAYER_ID);

				CPauseMenu::GetLocalPlayerDisplayPed()->SetIsAsleep(false);
				CPauseMenu::GetLocalPlayerDisplayPed()->SetIsLit(m_isInPlayerList);
			}

			CPauseMenu::GetDisplayPed()->SetCurrColumn(PM_COLUMN_EXTRA3);
		}
		else
		{
			SHOW_COLUMN(PLAYER_CARD_LOCAL_PLAYER_COMP_COLUMN, false);
			SHOW_COLUMN(PLAYER_CARD_OTHER_PLAYER_COMP_COLUMN, false);
			SHOW_COLUMN(PLAYER_CARD_COLUMN,false);
			SET_DATA_SLOT_EMPTY(PLAYER_CARD_LOCAL_PLAYER_COMP_COLUMN);
			SET_DATA_SLOT_EMPTY(PLAYER_CARD_OTHER_PLAYER_COMP_COLUMN);

			SetComparingVisible(false);
			if(SetupPlayerCardColumn(PLAYER_CARD_COLUMN, playerIndex))
			{
				showingColumn = true;
			}
			CPauseMenu::GetDisplayPed()->SetCurrColumn(PM_COLUMN_MIDDLE);
		}
	}
	else
	{
		CScaleformMenuHelper::SET_BUSY_SPINNER(PM_COLUMN_MIDDLE_RIGHT, true);
	}

	m_showAvatar = showingColumn;
	if(!showingColumn)
	{

		
		CPauseMenu::GetDisplayPed()->SetVisible(false);

		SHOW_COLUMN(PLAYER_AVATAR_COLUMN,false);
		SHOW_COLUMN(PLAYER_CARD_COLUMN,false);
		SHOW_COLUMN(PLAYER_CARD_LOCAL_PLAYER_COMP_COLUMN,false);
		SHOW_COLUMN(PLAYER_CARD_OTHER_PLAYER_COMP_COLUMN,false);
		CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(PLAYER_AVATAR_COLUMN);
		if (CScaleformMenuHelper::SET_COLUMN_TITLE(PLAYER_AVATAR_COLUMN, ""	,false))
		{
			CScaleformMgr::AddParamString("");
			CScaleformMgr::AddParamBool(false);
			CScaleformMgr::EndMethod();

		}

		SetupNeverPlayedWarning(playerIndex);
	}
	else
	{
		ClearWarningColumn();
	}

	CPauseMenu::GetDynamicPauseMenu()->SetLocalPedShowing(showingLocalPlayer);
	
	return showingColumn;
}

bool CPlayerListMenu::IsSyncingStats(int playerIndex, bool updateStatVersion)
{
	if(updateStatVersion && IsActivePlayer(playerIndex) && !IsLocalPlayer(playerIndex))
	{
		CNetObjPlayer* pNetPlayer = GetNetObjPlayer(playerIndex);
		if(pNetPlayer)
		{
			m_currentStatVersion = pNetPlayer->GetStatVersion();
		}
	}

	return false;
}

void CPlayerListMenu::SetupNeverPlayedWarning(int playerIndex)
{
	SetupNeverPlayedWarningHelper(playerIndex, IsActivePlayer(playerIndex) ? "PCARD_SYNCING_STATS" : "PCARD_NEVER_PLAYED_MP", PLAYER_AVATAR_COLUMN, 2);
}

void CPlayerListMenu::SetupNeverPlayedWarningHelper(int UNUSED_PARAM(playerIndex), const char* pMessage, PM_COLUMNS column, int numColumns)
{
	ShowColumnWarning_Loc(column, numColumns, pMessage, "NO_DATA");
}

const char* CPlayerListMenu::GetName(int playerIndex) const
{
	const char* pName = NULL;
	if(IsLocalPlayer(playerIndex))
	{
		#if RSG_DURANGO

		if (NetworkInterface::GetActiveGamerInfo())
		{
			if (NetworkInterface::GetActiveGamerInfo()->HasDisplayName())
				pName = NetworkInterface::GetActiveGamerInfo()->GetDisplayName();
			else
				pName = NetworkInterface::GetActiveGamerInfo()->GetName();
		}

		#else
		pName = NetworkInterface::GetActiveGamerInfo() ? NetworkInterface::GetActiveGamerInfo()->GetName() : NULL;
		#endif
		
	}

	if(!pName)
	{
		BasePlayerCardDataManager* pManager = GetDataMgr();
		if(pManager &&
			(uiVerifyf(0 <= playerIndex && playerIndex < pManager->GetReadGamerCount(), "CPlayerListMenu::SetupNeverPlayedWarning - playerIndex=%d, gamerCount=%d", playerIndex, pManager->GetReadGamerCount())))
		{
			pName = pManager->GetName(playerIndex);
		}
	}

	return pName;
}

void CPlayerListMenu::SetComparingVisible(bool visible)
{
	if(m_isComparingVisible != visible ||
		m_hadHint != m_hasHint)
	{
		m_isComparingVisible = visible;
		m_hadHint = m_hasHint;

		if(visible)
		{
			SHOW_COLUMN(PLAYER_CARD_COLUMN, false);
			SHOW_COLUMN(PLAYER_CARD_LOCAL_PLAYER_COMP_COLUMN, true);
			SHOW_COLUMN(PLAYER_CARD_OTHER_PLAYER_COMP_COLUMN, true);
		}
		else
		{
			SHOW_COLUMN(PLAYER_CARD_COLUMN, true);
			SHOW_COLUMN(PLAYER_CARD_LOCAL_PLAYER_COMP_COLUMN, false);
			SHOW_COLUMN(PLAYER_CARD_OTHER_PLAYER_COMP_COLUMN, false);
		}
	}
}

bool CPlayerListMenu::SetupPlayerCardColumn(PM_COLUMNS column, int playerIndex)
{
	BasePlayerCardDataManager* pManager = GetDataMgr();
	if(pManager && playerIndex < pManager->GetReadGamerCount())
	{
		int characterSlot = GetCharacterSlot(playerIndex);
		const PlayerCard& rCard = GetCardType(playerIndex, characterSlot);

		if(!HasPlayed(playerIndex))
		{
			SHOW_COLUMN(column, false);
		}
		else
		{
			SHOW_COLUMN(column, true);

			if(!rCard.m_slots.empty())
			{
				SetupCardTitle(column, playerIndex, characterSlot, rCard);

				for(int i=0; i<rCard.m_slots.size(); ++i)
				{
					SetCardDataSlot(column, playerIndex, characterSlot, i, rCard);
				}
			}

			DISPLAY_DATA_SLOT(column);
			return true;
		}
	}

	return false;
}

bool CPlayerListMenu::HasPlayed(int playerIndex) const
{
	BasePlayerCardDataManager* pManager = GetDataMgr();
	if(pManager && playerIndex != -1 && playerIndex < pManager->GetReadGamerCount())
	{
		if(IsActivePlayer(playerIndex))
		{
			return true;
		}

		return pManager->HasPlayedMode(pManager->GetGamerHandle(playerIndex));
	}

	return false;
}

bool CPlayerListMenu::SetCardDataSlot(PM_COLUMNS column, int playerIndex, int characterSlot, int slot, const PlayerCard& rCard, bool bCallEndMethod /*= true*/)
{
	BasePlayerCardDataManager* pManager = GetDataMgr();
	if(pManager && uiVerify(0 <= slot && slot < rCard.m_slots.GetCount()))
	{
		const CardSlot& rCardSlot = rCard.m_slots[slot];
		if((m_hasHint || !IsClanDataReady() || !m_hasCrewInfo) && rCardSlot.m_isCrew)
		{
			return false;
		}

		//freemode params:
		//columnIndex = always 2 in this case
		//menuIndex = 0….X amount
		//menuID = this is the menu enum can be anything not really used 
		//menuUniqueID = this is a number you put in that gets returned to you
		//menuType = always 1 for freemode (0 cops & crooks)
		//initial Value = always 0
		//menuItemGreyed = always 1
		//statString = e.g Kill/death
		//statIcon = use the icons enums
		//statValue = number 
		//statDesciption = String
		
		#if !__NO_OUTPUT
		uiDisplayf("CPlayerListMenu::SetCardDataSlot - Column - %d, Slot - %d, String - %s ", column, PLAYER_CARD_SLOT_FIX(slot), GetString(rCardSlot.m_textHash));
		#endif
		if(CScaleformMenuHelper::SET_DATA_SLOT(column, PLAYER_CARD_SLOT_FIX(slot), 0, PLAYER_CARD_SLOT_FIX(slot),
			(eOPTION_DISPLAY_STYLE)(rCardSlot.m_showPercentBar ? 0 : 1), 0, 1, GetString(rCardSlot.m_textHash), false))
		{
			//if(rCardSlot.m_descHash != 0)
			//{
			//	char descBuffer[MAX_CHARS_IN_MESSAGE];
			//	const char* pDesc = GetString(rCardSlot.m_descHash);

			//	if(rCardSlot.m_isRatio)
			//	{
			//		CNumberWithinMessage data[2];
			//		data[0].Set(GetIntStat(playerIndex, rCardSlot.m_stat.GetStat(characterSlot)));
			//		data[1].Set(GetIntStat(playerIndex, rCardSlot.m_extraStat.GetStat(characterSlot)));

			//		CMessages::InsertNumbersAndSubStringsIntoString(pDesc,
			//			data, NELEM(data),
			//			NULL, 0,
			//			descBuffer, NELEM(descBuffer) );
			//	}
			//	else
			//	{
			//		CNumberWithinMessage data[1];
			//		data[0].Set(GetIntStat(playerIndex, rCardSlot.m_stat.GetStat(characterSlot)));

			//		CMessages::InsertNumbersAndSubStringsIntoString(pDesc,
			//			data, NELEM(data),
			//			NULL, 0,
			//			descBuffer, NELEM(descBuffer) );
			//	}

			//	CScaleformMgr::AddParamString(descBuffer);
			//}

			AddStatParam(playerIndex, characterSlot, rCardSlot);

			if(bCallEndMethod)
			{
				CScaleformMgr::EndMethod(false);
			}

			return true;
		}

	}

	return false;
}

void CPlayerListMenu::SetupAvatar(int playerIndex)
{
	const rlGamerHandle& gamerHandle = GetGamerHandle(playerIndex);
	SPlayerCardManager::GetInstance().SetCurrentGamer(gamerHandle);
	
	UpdatePedState(playerIndex);

	CPauseMenu::GetDisplayPed()->SetWaitingForNewPed();
}

void CPlayerListMenu::LaunchAvatarScript()
{
	uiAssertf(GetDataMgr()->ArePlayerStatsAccessible(GetGamerHandle(m_currPlayerIndex)), "Attempted to call LaunchAvatarScript() before all the stats were ready");
	CPauseMenu::GetDisplayPed()->SetVisible(true);
	SPauseMenuScriptStruct newArgs(kPopulatePeds, GetMenuId(), -1, m_currPlayerIndex);
	LaunchScript(newArgs);
}

void CPlayerListMenu::LaunchUpdateInputScript(s32 sInput)
{
	KillScript();

	SPauseMenuScriptStruct newArgs(kUpdateInput, GetMenuId(), CPauseMenu::ConvertPadButtonNumberToInputType(sInput), m_currPlayerIndex);
	LaunchScript(newArgs);
}

bool CPlayerListMenu::SetPlayerDataSlot(int playerIndex, int slotIndex, bool isUpdate)
{
	bool wasSlotSet = false;
	
	BasePlayerCardDataManager* pManager = GetDataMgr();
	if(pManager && uiVerifyf(0 <= playerIndex && playerIndex < pManager->GetReadGamerCount(), "CPlayerListMenu::SetPlayerDataSlot - playerIndex=%d, gamerCount=%d", playerIndex, pManager->GetReadGamerCount()))
	{
		rlGamerHandle gamerHandle = pManager->GetGamerHandle(playerIndex);
		const GamerData& rGamerData = pManager->GetGamerData(playerIndex);
		CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(gamerHandle);

		PlayerDataSlot playerSlot;
		playerSlot.m_activeState = rGamerData.m_isOnline ? PlayerDataSlot::AS_ONLINE : PlayerDataSlot::AS_OFFLINE;
		playerSlot.m_onlineIcon = 0;
		playerSlot.m_headsetIcon = 0;
		playerSlot.m_rank = 0;
		playerSlot.m_isHost = rGamerData.m_isHost;
		playerSlot.m_color = GetPlayerColor(playerIndex, slotIndex);
		playerSlot.m_nameHash = rGamerData.m_nameHash;

		const rlClanMembershipData* pClanData = GetClanData(gamerHandle);
		if(pClanData && pClanData->IsValid())
		{
			#if RSG_DURANGO || RSG_XENON || RSG_PC
			// Xbox TRC TCR 094 - use every gamer index
			if( CLiveManager::CheckUserContentPrivileges(GAMER_INDEX_EVERYONE))
			#elif RSG_ORBIS || RSG_PS3
			// Playstation R4063 - use the local gamer index
			if( CLiveManager::CheckUserContentPrivileges(GAMER_INDEX_LOCAL))
			#endif
			{
				NetworkClan::GetUIFormattedClanTag(pClanData, playerSlot.m_crewTag, COUNTOF(playerSlot.m_crewTag));
			}
		}

		if(rGamerData.m_isInSameTitleOnline)
		{
			if(CPauseMenu::IsSP())
			{
				playerSlot.m_onlineIcon = GetMiscData().m_playingGTAMPIcon;
			}
			else
			{
				if(!IsSyncingStats(playerIndex, false))
				{
					int characterSlot = GetCharacterSlot(playerIndex);
					const PlayerCard& rCard = GetCardType(playerIndex, characterSlot);
					playerSlot.m_rank = GetIntStat(playerIndex, rCard.m_rankStat.GetStat(characterSlot), 0);
				}

				playerSlot.m_onlineIcon = GetMiscData().m_playingGTAMPWithRankIcon;
			}
			playerSlot.m_activeState = PlayerDataSlot::AS_ONLINE_IN_GTA;
		}
		else if(rGamerData.m_isInSameTitle)
		{
			playerSlot.m_onlineIcon = GetMiscData().m_playingGTASPIcon;
			playerSlot.m_activeState = PlayerDataSlot::AS_ONLINE_IN_GTA;
		}

		CNetworkVoice& voice = NetworkInterface::GetVoice();

		if(pPlayer)
		{
			int mpBitset = 0;

			playerSlot.m_activeState = PlayerDataSlot::AS_ONLINE_IN_GTA;

			if(fwDecoratorExtension::Get(pPlayer->GetPlayerInfo(), GetMiscData().m_bitTag, mpBitset) && mpBitset)
			{
				if(mpBitset & GetMiscData().m_bountyBit)
				{
					playerSlot.m_onlineIcon = GetMiscData().m_bountyIcon;
				}

				if(mpBitset & GetMiscData().m_spectatingBit)
				{
					playerSlot.m_onlineIcon = GetMiscData().m_spectatingIcon;
				}
			}

            if(voice.GamerHasHeadset(gamerHandle) && (IsLocalPlayer(gamerHandle) || voice.CanCommunicateVoiceWithGamer(gamerHandle)))
			{
				const rlGamerId& rGamerId = pPlayer->GetRlGamerId();

				if(voice.IsGamerTalking(rGamerId))
				{
					playerSlot.m_headsetIcon = GetMiscData().m_activeHeadsetIcon;
				}

//				playerSlot.m_headsetIcon = voice.IsGamerTalking(rGamerId) ? GetMiscData().m_activeHeadsetIcon : GetMiscData().m_inactiveHeadsetIcon;
			}

			playerSlot.m_kickCount = pPlayer->GetNumKickVotes();

#if RSG_NP
			if(g_rlNp.GetChatRestrictionFlag(ORBIS_ONLY(playerIndex)))
				playerSlot.m_headsetIcon = 0;
#endif
		}
		
		if(voice.IsGamerMutedByMe(gamerHandle))
		{
			playerSlot.m_headsetIcon = GetMiscData().m_mutedHeadsetIcon;
		}
			

		if(pPlayer && pPlayer->IsPhysical() && NetworkInterface::IsGameInProgress() && NetworkInterface::IsSpectatingLocalPlayer(pPlayer->GetPhysicalPlayerIndex()))
		{
			// Do nothing.
		}
		else if(rGamerData.m_inviteBlockedLabelHash)
		{
			playerSlot.m_pInviteState = rGamerData.m_inviteBlockedLabelHash;
			playerSlot.m_inviteColor = HUD_COLOUR_RED;
		}
		else if(rGamerData.m_isBusy)
		{
			playerSlot.m_pInviteState = ATSTRINGHASH("FM_PLY_BUSY", 0xB90FE62E);
			playerSlot.m_inviteColor = HUD_COLOUR_RED;
		}
		else if(rGamerData.m_hasBeenInvited || rGamerData.m_isWaitingForInviteAck || rGamerData.m_isOnCall )
		{
			if(rGamerData.m_hasFullyJoined)
			{
				playerSlot.m_pInviteState = ATSTRINGHASH("FM_PLY_JOINED",0xc5c40e3a);
				playerSlot.m_inviteColor = HUD_COLOUR_GREEN;
			}
			else if (rGamerData.m_isOnCall)
			{
				playerSlot.m_pInviteState = ATSTRINGHASH("FM_PLY_ON_CALL",0xEE06BF80);
				playerSlot.m_inviteColor = HUD_COLOUR_ORANGE;
			}
			else if(rGamerData.m_isInTransition)
			{
				playerSlot.m_pInviteState = ATSTRINGHASH("FM_PLY_JOINING",0xa8349664);
				playerSlot.m_inviteColor = HUD_COLOUR_RED;
			}
			else if(rGamerData.m_hasBeenInvited)
			{
				playerSlot.m_pInviteState = ATSTRINGHASH("FM_PLY_INVITED",0x1d6762b6);
				playerSlot.m_inviteColor = HUD_COLOUR_RED;
			}
			else if(rGamerData.m_isWaitingForInviteAck)
			{
				playerSlot.m_pInviteState = ATSTRINGHASH("FM_PLY_INVITING",0x7cde8fba);
				playerSlot.m_inviteColor = HUD_COLOUR_RED;
			}
			
		}

		playerSlot.m_pText = pManager->GetName(playerIndex);

		// If the data changed, or is being initialized, then set the slot.
		if(playerSlot != m_previousPlayerDataSlots[slotIndex] || !isUpdate)
		{
			m_previousPlayerDataSlots[slotIndex] = playerSlot;

			AddOrUpdateSlot( slotIndex, playerIndex, isUpdate, playerSlot);
			wasSlotSet = true;

		}
	}

	return wasSlotSet;
}

const CPlayerListMenu::PlayerCard& CPlayerListMenu::GetCardType(int UNUSED_PARAM(playerIndex), int UNUSED_PARAM(characterSlot))
{
	const CPlayerCardXMLDataManager& rStatListManager = GetStatListMgr();

	if(CPauseMenu::IsSP())
	{
		return rStatListManager.GetPlayerCard(CPlayerCardXMLDataManager::PCTYPE_SP);
	}

	// Assuming Freemode until CnC is hookedup again.
	return rStatListManager.GetPlayerCard(CPlayerCardXMLDataManager::PCTYPE_FREEMODE);
}

int CPlayerListMenu::GetCharacterSlot(int playerIndex, eStatMode mode /*= STATMODE_DEFAULT*/)
{
	int characterId = 0;

	if(IsSP(mode))
	{
		characterId = (int)StatsInterface::GetCharacterIndex();
		if(CHAR_SP_START <= characterId && characterId <= CHAR_SP_END)
		{
			characterId -= CHAR_SP_START;
		}
		else
		{
			characterId = 0;
		}
	}
	else if(IsLocalPlayer(playerIndex) || !IsActivePlayer(playerIndex))
	{
		characterId = GetIntStat(playerIndex, GetMiscData().m_characterSlotStat);
		if(characterId < 0 || MAX_NUM_MP_CHARS < characterId)
		{
			characterId = 0;
		}
	}

	return characterId;
}

const char* CPlayerListMenu::GetString(int hash)
{
	return hash ? TheText.Get(hash, NULL) : "";
}

bool CPlayerListMenu::GetBoolStat(int playerIndex, int statHash, int defaultValue /*= PLAYER_CARD_STAT_NOT_FOUND*/)
{
	BasePlayerCardDataManager* pManager = GetDataMgr();
	if(pManager)
	{
		if(pManager->HadError() || !pManager->ArePlayerStatsAccessible(GetGamerHandle(playerIndex)))
		{
			return defaultValue > 0 ? true : false;
		}

		return GetStatAsFloat(playerIndex, statHash, (double)defaultValue) > 0.0f ? true : false;
	}

	uiAssertf(0, "CPlayerListMenu::GetBoolStat - Couldn't find stat %s(0x%08x)", GetStatName(statHash), statHash);
	return false;
}

int CPlayerListMenu::GetIntStat(int playerIndex, int statHash, int defaultValue /*=PLAYER_CARD_STAT_NOT_FOUND*/)
{
	BasePlayerCardDataManager* pManager = GetDataMgr();
	if(pManager)
	{
		if(pManager->HadError() || !pManager->ArePlayerStatsAccessible(GetGamerHandle(playerIndex)))
		{
			return defaultValue;
		}

		return (int)GetStatAsFloat(playerIndex, statHash, (double)defaultValue);
	}

	uiAssertf(0, "CPlayerListMenu::GetIntStat - Couldn't find stat %s(0x%08x)", GetStatName(statHash), statHash);
	return 0;
}


int CPlayerListMenu::CalcStatHash(const char* pStat)
{
	return atStringHash(pStat);
}

int CPlayerListMenu::CalcStatHash(const char* pStat, int characterSlot, eStatMode mode /*=STATMODE_DEFAULT*/)
{
	const int bufferSize = 50;
	char buffer[bufferSize] = {0};

	const char* pMode = IsSP(mode) ? "SP" : "MP";

	formatf(buffer, "%s%d_%s", pMode, characterSlot, pStat);
	return CalcStatHash(buffer);
}

bool CPlayerListMenu::IsSP(eStatMode mode)
{
	return (mode == STATMODE_DEFAULT) ? CPauseMenu::IsSP() : (mode == STATMODE_SP);
}

void CPlayerListMenu::AddStatParam(int playerIndex, int statHash)
{
	CScaleformMgr::AddParamFloat(GetStatAsFloat(playerIndex, statHash));
}

void CPlayerListMenu::AddStatParam(int playerIndex, int characterSlot, const CardSlot& rCardSlot)
{
	int statHash = rCardSlot.m_stat.GetStat(characterSlot);

	double val = 0.0;
	double num = 0.0;
	double denom = 0.0;

	if(rCardSlot.m_isRatio || rCardSlot.m_isRatioStr || rCardSlot.m_isSumPercent)
	{
		num = GetStatAsFloat(playerIndex, statHash, 0.0);
		denom = GetStatAsFloat(playerIndex, rCardSlot.m_extraStat.GetStat(characterSlot), 0.0);

		if(rCardSlot.m_isSumPercent)
		{
			denom += num;
		}

		if(num < 0.001)
		{
			val = 0.0;
		}
		else if(denom < 0.001)
		{
			val = rCardSlot.m_isSumPercent ? 100.0 : num;
		}
		else
		{
			val = num / denom;

			if(rCardSlot.m_isSumPercent)
			{
				val *= 100.0;
			}
		}
	}
	else
	{
		val = GetStatAsFloat(playerIndex, statHash, 0.0);
	}

	if(rCardSlot.m_titleListHash)
	{
		u32 titleHash = SPlayerCardManager::GetInstance().GetXMLDataManager().GetTitleInfo().GetTitle(rCardSlot.m_titleListHash, (float)val);
		CScaleformMgr::AddParamString(GetString(titleHash));
	}

	if(rCardSlot.m_isCash)
	{
		AddCashParam(val);
	}
	else if(rCardSlot.m_isPercent)
	{
		AddPercentParam(val);
	}
	else if(rCardSlot.m_isRatioStr)
	{
		AddRatioStrParam(num, denom);
		CScaleformMgr::AddParamFloat(val);
	}
	else
	{
		CScaleformMgr::AddParamFloat(val);
	}
}

void CPlayerListMenu::AddCashParam(double cash)
{
	const int bufferSize = 20;
	char buffer[bufferSize] = {0};
	formatf(buffer, bufferSize, "$%d", (s32)cash);
	CScaleformMgr::AddParamString(buffer);
}

void CPlayerListMenu::AddPercentParam(double percent)
{
	const int bufferSize = 20;
	char buffer[bufferSize] = {0};
	formatf(buffer, bufferSize, "%.2f%%", percent);
	CScaleformMgr::AddParamString(buffer);
}

void CPlayerListMenu::AddRatioStrParam(double num, double denom)
{
	const int bufferSize = 20;
	char buffer[bufferSize] = {0};
	formatf(buffer, bufferSize, "%d/%d", (int)num, (int)denom);
	CScaleformMgr::AddParamString(buffer);
}


double CPlayerListMenu::GetStatAsFloat(int playerIndex, int statHash, double defaultVal /*=-1.0*/)
{
	double retVal = 0.0;
	bool found = false;
	BasePlayerCardDataManager* pManager = GetDataMgr();

	if(uiVerify(pManager) && (IsLocalPlayer(playerIndex) || pManager->ArePlayerStatsAccessible(GetGamerHandle(playerIndex))))
	{
		if(IsLocalPlayer(playerIndex))
		{
			StatId statId(statHash);

			if(uiVerifyf(StatsInterface::IsKeyValid(statId), "CPlayerListMenu::GetStatAsFloat - Trying to use a stat that doesn't exist, %s(0x%08x).", GetStatName(statHash), statHash))
			{
				found = true;
				if(StatsInterface::GetIsBaseType(statId, STAT_TYPE_INT))
				{
					retVal = static_cast<double>(StatsInterface::GetIntStat(statId));
				}
				else if(StatsInterface::GetIsBaseType(statId, STAT_TYPE_FLOAT))
				{
					retVal = static_cast<double>(StatsInterface::GetFloatStat(statId));
				}
				else if(StatsInterface::GetIsBaseType(statId, STAT_TYPE_BOOLEAN))
				{
					retVal = StatsInterface::GetBooleanStat(statId) ? 1.0 : 0.0;
				}
				else
				{
					found = false;
				}

				uiAssertf(found, "CPlayerListMenu::GetStatAsFloat - Stat %s(0x%08x) has an unhandled stat type %d.", GetStatName(statHash), statHash, StatsInterface::GetStatType(statId));
			}

			// Active players should only get their data from here.
			if(!found)
			{
				retVal = defaultVal;
				found = true;
			}
		}

		if(!found && NetworkInterface::IsGameInProgress() && IsActivePlayer(playerIndex))
		{
			CNetObjPlayer* pObjPlayer = GetNetObjPlayer(playerIndex);
			if(pObjPlayer && pObjPlayer->GetStatVersion() != 0)
			{
				const PlayerCard& rCard = GetStatListMgr().GetPlayerCard(CPlayerCardXMLDataManager::PCTYPE_FREEMODE);
				PlayerCard::eNetworkedStatIds netStatId = rCard.GetNetIdByStatHash(statHash);

				if(uiVerifyf(0 <= netStatId && netStatId < PlayerCard::NETSTAT_TOTAL_STATS, "CPlayerListMenu::GetStatAsFloat - networked stat %s(0x%08x) out of range. %d with max of %d", GetStatName(statHash), statHash, netStatId, PlayerCard::NETSTAT_TOTAL_STATS))
				{
					retVal = static_cast<double>(pObjPlayer->GetStatValue(netStatId));
					found = true;
				}

				// Active players should only get their data from here.
				if(!found)
				{
					retVal = defaultVal;
					found = true;
				}
			}
		}

		if(!found)
		{
			const atArray<int>* pStats = GetStatIds();
			int statIndex = pStats->Find(statHash);
			if(statIndex != -1)
			{
				const rlProfileStatsValue* pStat = pManager->GetStat(playerIndex, statIndex);
				if(pStat)
				{
					switch(pStat->GetType())
					{
					case RL_PROFILESTATS_TYPE_INT32:
						retVal = (double)pStat->GetInt32();
						break;
					case RL_PROFILESTATS_TYPE_INT64:
						retVal = (double)pStat->GetInt64();
						break;
					case RL_PROFILESTATS_TYPE_FLOAT:
						retVal = (double)pStat->GetFloat();
						break;
					default:
						uiDisplayf("CPlayerListMenu::GetStatAsFloat - %s(0x%08x) is an unknown stat type (%d).", GetStatName(statHash), statHash, (int)pStat->GetType());
						retVal = PLAYER_CARD_IVALID_STAT_TYPE;
						break;
					}

					found = true;
				}
			}
			else
			{
				uiWarningf("CPlayerListMenu::GetStatAsFloat - Trying to get a stat %s(0x%08x, %d) value, but the PlayerCardManager doesn't know about it - likelly the stat has not been updated yet.", StatsInterface::GetKeyName(statHash), statHash, statHash);
			}
		}

		if(!found)
		{
			int packValue = 0;
			if(GetPackedStatValue(playerIndex, statHash, packValue))
			{
				retVal = static_cast<double>(packValue);
				found = true;
			}
		}
	}

	if(!found)
	{
		retVal = defaultVal;
	}

	return retVal;
}

bool CPlayerListMenu::UpdateInput(s32 sInput)
{
	if(UpdateInviteTimer())
	{
		return true;
	}

	if(CheckPaginatorInput(sInput))
	{
		uiDisplayf("CPlayerListMenu::UpdateInput- if(CheckPaginatorInput(sInput))");

		return true;
	}

	if(sInput == PAD_CIRCLE)
	{
		return OnInputBack();
	}
	else if(sInput == PAD_SQUARE)
	{
		return OnInputCompare();
	}
	else if(sInput == PAD_TRIANGLE && CanHandleGoToSCInput())
	{
		return OnInputSC();
	}
	else if(sInput == PAD_CROSS)
	{
		return OnInputOpenContext();
	}

	return false;
}

bool CPlayerListMenu::OnInputBack()
{
	if(GetIsInPlayerList())
	{
		m_isComparing = false;
		SetIsInPlayerList(false);

		BasePlayerCardDataManager* pManager = GetDataMgr();
		if(pManager && 0 < pManager->GetReadGamerCount())
		{
			SetupPlayerCardColumns(m_currPlayerIndex);
		}
	}

	return false;
}

bool CPlayerListMenu::OnInputOpenContext()
{

	#if !RSG_ORBIS // B*1817634 - Cannot show Sign-in UI on PS4
	if( NetworkInterface::GetActiveGamerInfo() == NULL || !NetworkInterface::GetActiveGamerInfo()->IsOnline())
	{
		if(GetIsInPlayerList())
		{
			SetIsInPlayerList(false);
			CPauseMenu::MENU_SHIFT_DEPTH(kMENUCEPT_SHALLOWER,false,true);
		}

		CLiveManager::ShowSigninUi();
		return true;
	}
		else if( NetworkInterface::GetActiveGamerInfo() == NULL || !NetworkInterface::GetActiveGamerInfo()->IsOnline())
	{
		if(GetIsInPlayerList())
		{
			SetIsInPlayerList(false);
			CPauseMenu::MENU_SHIFT_DEPTH(kMENUCEPT_SHALLOWER,false,true);
		}

		CLiveManager::ShowAccountUpgradeUI();

		return true;
	} else
#endif
		// Don't allow the player to enter the menu before it's populated.
	if(!m_isPopulated)
	{
		return true;
	}
	else
	{
		BasePlayerCardDataManager* pManager = GetDataMgr();
		if(!pManager ||
			pManager->GetReadGamerCount() == 0)
		{
			return true;
		}
	}

	return false;
}

bool CPlayerListMenu::OnInputCompare()
{
	if(GetIsInPlayerList() && (!GetContextMenu() || !GetContextMenu()->IsOpen()))
	{
		BasePlayerCardDataManager* pManager = GetDataMgr();
		if(pManager && pManager->IsValid() && pManager->GetReadGamerCount() > 0 && m_currPlayerIndex != PLAYER_CARD_LOCAL_PLAYER_ID && !IsLocalPlayer(m_currPlayerIndex))
		{
			CPauseMenu::PlaySound("NAV_LEFT_RIGHT");

			m_isComparing = !m_isComparing;
			SetupHelpSlot(m_currPlayerIndex);
			SetupPlayerCardColumns(m_currPlayerIndex);

			RefreshButtonPrompts();
		}
	}

	return false;
}

bool CPlayerListMenu::OnInputSC()
{
	if(GetIsInPlayerList() && (!GetContextMenu() || !GetContextMenu()->IsOpen()))
	{
		if((!CLiveManager::GetSocialClubMgr().IsConnectedToSocialClub() || !CLiveManager::GetSocialClubMgr().IsOnlinePolicyUpToDate()) &&
			NetworkInterface::GetActiveGamerInfo() != NULL && NetworkInterface::GetActiveGamerInfo()->IsOnline())
		{
			SocialClubMenu::SetTourHash(ATSTRINGHASH("General",0x6b34fe60));
			CPauseMenu::EnterSocialClub();
		}
	}

	return false;
}

bool CPlayerListMenu::OnInputInvite(s32 sInput)
{
	if (SUIContexts::IsActive("TutLock"))
	{
		return true;
	}

	if(!m_isEmpty && !m_inviteTimer.IsStarted() && m_currPlayerIndex != PLAYER_CARD_LOCAL_PLAYER_ID)
	{
		BasePlayerCardDataManager* pManager = GetDataMgr();
		if(pManager && pManager->IsValid() && pManager->GetReadGamerCount() > 0)
		{
			const rlGamerHandle& rCurrGamerHandle = GetGamerHandle(m_currPlayerIndex);
			GamerData& rGamerData = pManager->GetGamerData(m_currPlayerIndex);
			rGamerData.m_inviteBlockedLabelHash = 0;
			scriptDebugf3("m_inviteBlockedLabelHash cleared.");

			if(rGamerData.m_isOnline && !CNetwork::GetNetworkSession().IsTransitionMember(rCurrGamerHandle))
			{
				const CNetworkInvitedPlayers::sInvitedGamer& invitedPlayer = CNetwork::GetNetworkSession().GetTransitionInvitedPlayers().GetInvitedPlayer(rCurrGamerHandle);
				if(!invitedPlayer.IsValid() ||
					(!invitedPlayer.m_bHasAck && !invitedPlayer.IsWithinAckThreshold()))
				{
					LaunchUpdateInputScript(sInput);

					SetupCenterColumnWarning("PCARD_SEND_INVT_TEXT", "PCARD_SEND_INVT_TITLE");

					SUIContexts::Activate(INVITED_TRANSITION_CONTEXT);
					CPauseMenu::RedrawInstructionalButtons();

					return true;
				}
			}
		}
	}

	return false;
}

bool CPlayerListMenu::UpdateInviteTimer()
{
	if(m_inviteTimer.IsStarted())
	{
		if(m_inviteTimer.IsComplete(PLAYER_CARD_INVITE_DELAY))
		{
			m_inviteTimer.Zero();
			CScaleformMenuHelper::SET_BUSY_SPINNER(PLAYER_AVATAR_COLUMN, false);
			
			// Needs to refresh everything.
			int temp = m_currPlayerIndex;
			m_currPlayerIndex = -1;
			SetPlayerCard(temp);
		}
		else
		{
			return true;
		}
	}

	return false;
}

void CPlayerListMenu::SetupCenterColumnWarning(const char* message, const char* title)
{
	ClearAvatarSpinner();
	CScaleformMenuHelper::SET_BUSY_SPINNER(PLAYER_AVATAR_COLUMN, true);
	m_inviteTimer.Start();

	m_showAvatar = false;
	CPauseMenu::GetDisplayPed()->SetVisible(false);
	SHOW_COLUMN(PLAYER_AVATAR_COLUMN,false);

	ShowColumnWarning(PLAYER_AVATAR_COLUMN, 1, TheText.Get(message), TheText.Get(title));
}

void CPlayerListMenu::ClearAvatarSpinner()
{
	CPauseMenu::SetBusySpinner(false, AVATAR_BUSY_SPINNER_COLUMN, AVATAR_BUSY_SPINNER_INDEX);
}

bool CPlayerListMenu::OnInputMute()
{
	if(!m_isEmpty && m_currPlayerIndex != PLAYER_CARD_LOCAL_PLAYER_ID)
	{
		BasePlayerCardDataManager* pManager = GetDataMgr();
		if(pManager && pManager->IsValid() && pManager->GetReadGamerCount() > 0)
		{
			const rlGamerHandle& rCurrGamerHandle = GetGamerHandle(m_currPlayerIndex);
			
#if !__XENON
			const VoiceGamerSettings* pVoiceSettings = NetworkInterface::GetVoice().FindGamer(rCurrGamerHandle);
			if(pVoiceSettings &&
				((pVoiceSettings->GetLocalVoiceFlags() & VoiceGamerSettings::VOICE_LOCAL_MUTED) != VoiceGamerSettings::VOICE_LOCAL_MUTED))
#endif
			{
				CPauseMenu::PlayInputSound(FRONTEND_INPUT_ACCEPT);
				NetworkInterface::GetVoice().ToggleMute(rCurrGamerHandle);
			}
		}
	}

	return false;
}

bool CPlayerListMenu::OnInputCancleInvite(s32 sInput)
{
	if(!m_isEmpty && m_currPlayerIndex != PLAYER_CARD_LOCAL_PLAYER_ID && m_currPlayerIndex != PLAYER_CARD_INVALID_PLAYER_ID)
	{
		BasePlayerCardDataManager* pManager = GetDataMgr();
		if(pManager && pManager->IsValid() && pManager->GetReadGamerCount() > 0)
		{
			const rlGamerHandle& rCurrGamerHandle = pManager->GetGamerHandle(m_currPlayerIndex);
			GamerData& rGamerData = pManager->GetGamerData(m_currPlayerIndex);
			rGamerData.m_inviteBlockedLabelHash = 0;
			scriptDebugf3("m_inviteBlockedLabelHash cleared.");


			CNetwork::GetNetworkSession().CancelTransitionInvites(&rCurrGamerHandle, 1);
			CNetwork::GetNetworkSession().RemoveTransitionInvites(&rCurrGamerHandle, 1);

			SUIContexts::Deactivate(INVITED_TRANSITION_CONTEXT);
			SUIContexts::Deactivate(TRANSITION_CONTEXT);
			CPauseMenu::RedrawInstructionalButtons();

			LaunchUpdateInputScript(sInput);

			return true;
		}
	}

	return false;
}

bool CPlayerListMenu::CheckPaginatorInput(s32 sInput)
{
	if(IsPaginatorValid() && GetIsInPlayerList())
	{
		CContextMenu* pContextMenu = GetContextMenu();

		if(pContextMenu && !pContextMenu->IsOpen())
		{
			// checks if LT/RT has been pressed
			if( GetPaginator()->UpdateInput(sInput) )
				return true; // handled!
		}
	}
	return false;
}

bool CPlayerListMenu::UpdateInputForInviteMenus(s32 sInput)
{
	if(UpdateInviteTimer())
	{
		return true;
	}

	if(sInput == PAD_DPADLEFT
		|| sInput == PAD_DPADRIGHT
		|| sInput == PAD_LEFTSHOULDER1
		|| sInput == PAD_RIGHTSHOULDER1
		)
	{
		return true;
	}

	bool inputHandled = false;

	if(CheckPaginatorInput(sInput))
	{
		inputHandled = true;
	}
	if(sInput == PAD_CIRCLE)
	{
		inputHandled = OnInputBack();
	}
	else if(sInput == PAD_CROSS)
	{
		inputHandled = OnInputInvite(sInput);
		if(inputHandled)
		{
			CPauseMenu::PlayInputSound(FRONTEND_INPUT_ACCEPT);
		}
	}
	else if(sInput == PAD_SQUARE)
	{
		inputHandled = OnInputCancleInvite(sInput);
	}
	else if(sInput == PAD_TRIANGLE)
	{
		inputHandled = OnInputMute();
	}

	if(inputHandled)
	{
		
		if (!CPauseMenu::IsNavigatingContent())
		{
			SetIsInPlayerList(true);
			CPauseMenu::SetNavigatingContent(true);
		}
		return true;
	}

	return false;
}

bool CPlayerListMenu::ShouldPlayNavigationSound(bool UNUSED_PARAM(navUp))
{
	bool retVal = false;

	// Don't play sounds when at the end of the list.
	if(!m_isEmpty)
	{
		BasePlayerCardDataManager* pManager = GetDataMgr();
		if(pManager && pManager->GetReadGamerCount() > 1)
		{
			retVal = !m_inviteTimer.IsStarted() || m_inviteTimer.IsComplete(PLAYER_CARD_INVITE_DELAY);
		}
	}

	return retVal;
}

void CPlayerListMenu::FillOutPlayerMissionInfo()
{
	if(NetworkInterface::IsNetworkOpen())
	{
		const CNetGamePlayer* pPlayer = GetLocalPlayer();
		if(pPlayer)
		{
			m_playerTeamId = PLAYER_CARD_INVALID_TEAM_ID;

			fwDecoratorExtension::Get(pPlayer->GetPlayerInfo(), GetMiscData().m_teamIdTag, m_playerTeamId);
		}
	}

}

int CPlayerListMenu::GetPlayerColor(int playerIndex, int OUTPUT_ONLY(debugSlotIndexForOutput) /*= -1*/)
{
	int retColor = GetMiscData().m_offlineSlotColor;

	if(IsLocalPlayer(playerIndex))
	{
		if(m_playerTeamId != PLAYER_CARD_INVALID_TEAM_ID)
		{
			retColor = GetMiscData().m_localPlayerSlotColor;
		}
		else
		{
			retColor = GetMiscData().m_freemodeSlotColor;
		}

#if !__NO_OUTPUT
		if(0 <= debugSlotIndexForOutput && debugSlotIndexForOutput < m_previousPlayerDataSlots.size() &&
			retColor != m_previousPlayerDataSlots[debugSlotIndexForOutput].m_color)
		{
			uiDisplayf("CPlayerListMenu::GetPlayerColor - isLocalPlayer=true, m_playerTeamId=%d, retColor=%d", m_playerTeamId, retColor);
		}
#endif
	}
	else
	{
		BasePlayerCardDataManager* pManager = GetDataMgr();
		if(pManager && playerIndex < pManager->GetReadGamerCount())
		{
			rlGamerHandle gamerHandle = pManager->GetGamerHandle(playerIndex);
			const GamerData& rGamerData = pManager->GetGamerData(playerIndex);

			if(CPauseMenu::IsSP())
			{
				if(rGamerData.m_isOnline)
				{
					retColor = GetMiscData().m_spSlotColor;
				}
			}
			else // MP + Lobby/Prelobby
			{
				if(rGamerData.m_isOnline)
				{
					retColor = GetMiscData().m_freemodeSlotColor;
				}

				CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(gamerHandle);
				if(pPlayer)
				{
					if(m_playerTeamId != PLAYER_CARD_INVALID_TEAM_ID)
					{
						if(rGamerData.m_teamId == m_playerTeamId && m_playerTeamId != PLAYER_CARD_SOLO_TEAM_ID)
						{
							retColor = GetMiscData().m_localPlayerSlotColor;
						}
						else
						{
							retColor = GetMiscData().m_inMatchSlotColor;
						}
					}
				}
			}

#if !__NO_OUTPUT
			if(0 <= debugSlotIndexForOutput && debugSlotIndexForOutput < m_previousPlayerDataSlots.size() &&
				retColor != m_previousPlayerDataSlots[debugSlotIndexForOutput].m_color)
			{
				uiDisplayf("CPlayerListMenu::GetPlayerColor - isSp=%d, isMP=%d, isLobby=%d, isPreLobby=%d, rGamerData.m_isOnline=%d, m_playerTeamId=%d, rGamerData.m_teamId=%d, retColor=%d",
					CPauseMenu::IsSP(), CPauseMenu::IsMP(), CPauseMenu::IsLobby(), CPauseMenu::IsPreLobby(), rGamerData.m_isOnline, m_playerTeamId, rGamerData.m_teamId, retColor);
			}
#endif
		}
	}

	return retColor;
}

void CPlayerListMenu::SetupAvatarColumn(int playerIndex, const char* pPreviousStatus /*= NULL*/)
{
	if(!m_showAvatar)
		return;

	rlGamerHandle gamerHandle = RL_INVALID_GAMER_HANDLE;
	const char* onlineStatus = "PCARD_OFFLINE";

	BasePlayerCardDataManager* pManager = GetDataMgr();
	if(IsLocalPlayer(playerIndex))
	{
		if(NetworkInterface::GetActiveGamerInfo())
		{
			gamerHandle = NetworkInterface::GetActiveGamerInfo()->GetGamerHandle();
			onlineStatus = NetworkInterface::IsNetworkOpen() ? "PCARD_ONLINE_SES" : "PCARD_ONLINE_SP";
		}
	}
	else if(pManager && playerIndex != PLAYER_CARD_INVALID_PLAYER_ID)
	{
		if(uiVerifyf(playerIndex < pManager->GetReadGamerCount(), "CPlayerListMenu::SetupAvatarColumn - playerIndex = %d, GetReadGamerCount=%d", playerIndex, pManager->GetReadGamerCount()))
		{
			gamerHandle = pManager->GetGamerHandle(playerIndex);
			const GamerData& rGamerData = pManager->GetGamerData(playerIndex);

			if(rGamerData.m_isInSameSession)
			{
				onlineStatus = "PCARD_ONLINE_SES";
			}
			else if(rGamerData.m_isInSameTitleOnline)
			{
				onlineStatus = "PCARD_ONLINE_MP";
			}
			else if(rGamerData.m_isInSameTitle)
			{
				onlineStatus = "PCARD_ONLINE_SP";
			}
			else if(rGamerData.m_isOnline)
			{
				onlineStatus = "PCARD_ONLINE_OTHER";
			}
		}
	}

	if(uiVerify(gamerHandle != RL_INVALID_GAMER_HANDLE) && pPreviousStatus != onlineStatus)
	{
		SHOW_COLUMN(PLAYER_AVATAR_COLUMN, true);
		CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(PLAYER_AVATAR_COLUMN);
		if(CScaleformMenuHelper::SET_COLUMN_TITLE(PLAYER_AVATAR_COLUMN, CanShowAvatarTitle() ? TheText.Get(onlineStatus) : "", false))
		{
			bool hasBounty = false;
			m_pPreviousStatus = onlineStatus;
	
			if(NetworkInterface::IsNetworkOpen())
			{
				CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(gamerHandle);
				if(pPlayer)
				{
					int mpBitset = 0;
					fwDecoratorExtension::Get(pPlayer->GetPlayerInfo(), GetMiscData().m_bitTag, mpBitset);
					hasBounty = (mpBitset & GetMiscData().m_bountyBit) != 0;
				}
			}
	
			uiDisplayf("CPlayerListMenu::SetupAvatarColumn - playerIndex = %d, bounty = %d, onlineStatus = '%s'", playerIndex, hasBounty, onlineStatus);
			CScaleformMgr::AddParamString("");
			CScaleformMgr::AddParamBool(hasBounty);
			
			CScaleformMgr::EndMethod();
		}
		DISPLAY_DATA_SLOT(PLAYER_AVATAR_COLUMN);
	}
}

void CPlayerListMenu::HandleContextMenuVisibility(bool closingContext)
{
	if(closingContext)
	{
		if(m_showAvatar)
		{
			RefreshButtonPrompts();

			BasePlayerCardDataManager* pManager = GetDataMgr();
			if(pManager && 0 <= m_currPlayerIndex && m_currPlayerIndex < pManager->GetReadGamerCount())
			{
				SetupAvatarColumn(m_currPlayerIndex);
			}

			CPauseMenu::GetDisplayPed()->SetVisible(true);
		}
	}
	else
	{
		m_killScriptPending = true;
		SHOW_COLUMN(PLAYER_AVATAR_COLUMN, false);
		CPauseMenu::GetDisplayPed()->SetVisible(false);
		ClearAvatarSpinner();
	}
}

const char* CPlayerListMenu::GetCrewEmblem(const rlGamerHandle& gamerHandle) const
{
	CNetworkCrewDataMgr& dataMgr = CLiveManager::GetCrewDataMgr();
	const rlClanMembershipData* pClanData = GetClanData(gamerHandle);
	if(pClanData && pClanData->IsValid())
	{
		const char* pClanEmblem = NULL;
		return dataMgr.GetClanEmblem(pClanData->m_Clan.m_Id, pClanEmblem, false) ? pClanEmblem : NULL;
	}

	return NULL;
}

bool CPlayerListMenu::GetCrewRankTitle(const rlGamerHandle& gamerHandle, char* crewRankTitle, int crewRankTextSize, int crewRank) const
{
	const rlClanMembershipData* pClanData = GetClanData(gamerHandle);
	if(pClanData && pClanData->IsValid())
	{
		const NetworkCrewMetadata* pMeta = NULL;
		CNetworkCrewDataMgr& dataMgr = CLiveManager::GetCrewDataMgr();
		if(dataMgr.GetClanMetadata(pClanData->m_Clan.m_Id, pMeta) && pMeta)
		{
			return  pMeta->GetCrewInfoCrewRankTitle(crewRank, crewRankTitle, crewRankTextSize) || pMeta->Succeeded();
		}
	}

	return false;
}

bool CPlayerListMenu::GetCrewColor(const rlGamerHandle& gamerHandle, Color32& color) const
{
	const rlClanMembershipData* pClanData = GetClanData(gamerHandle);
	if(pClanData && pClanData->IsValid())
	{
		color.Set(pClanData->m_Clan.m_clanColor);
		return true;
	}

	return false;
}

int CPlayerListMenu::GetCrewRank(int playerIndex) const
{
	int retVal = GetIntStat(playerIndex, GetMiscData().m_crewRankStat, 1);
	return MAX(1, retVal);
}

bool CPlayerListMenu::DoesPlayerCardNeedRefresh(int playerIndex, bool hasCrewInfo)
{
	if(!GetContextMenu() || !GetContextMenu()->IsOpen())
	{
		Color32 color;
		char crewRankTitle[RL_CLAN_NAME_MAX_CHARS] = {0};

		BasePlayerCardDataManager* pManager = GetDataMgr();

		//Player Card flagged that needs to refresh since 1 or more player 
		//  have joined or left.
		if (pManager 
			&& pManager->IsValid() 
			&& pManager->NeedsRefresh())
		{
			return true;
		}
		// Has Crew stuff changed.
		else if(!hasCrewInfo && pManager && pManager->IsValid() &&
			GetCrewEmblem(playerIndex) != NULL &&
			GetCrewRankTitle(playerIndex, crewRankTitle, RL_CLAN_NAME_MAX_CHARS, GetCrewRank(playerIndex)) &&
			GetCrewColor(playerIndex, color))
		{
			return true;
		}
		else if(IsActivePlayer(playerIndex) && !IsLocalPlayer(playerIndex)) // If active player has newer stats.
		{
			CNetObjPlayer* pObjPlayer = GetNetObjPlayer(playerIndex);
			if(pObjPlayer && pObjPlayer->GetStatVersion() != m_currentStatVersion)
			{
				return true;
			}
		}
	}

	return false;
}

void CPlayerListMenu::SetIsInPlayerList(bool isIn)
{
	CPauseMenu::RefreshSocialClubContext();

	m_isInPlayerList = isIn;
	UpdatePedState(m_currPlayerIndex);
}

bool CPlayerListMenu::Populate(MenuScreenId newScreenId)
{
	playerlistDebugf1("[CPlayerListMenu] CPlayerListMenu::Populate");

	if( newScreenId == GetMenuScreenId() )
	{
		if (!m_isComparing)
		{

			playerlistDebugf1("[CPlayerListMenu] CPlayerListMenu::Populate - Resetting scaleform columns.");

			Clear();
			m_hasClanData = GetGroupClanData().Init();

			CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(PLAYER_LIST_COLUMN);
			CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(PLAYER_AVATAR_COLUMN);
			CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(PLAYER_CARD_COLUMN);
			CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(PLAYER_CARD_LOCAL_PLAYER_COMP_COLUMN);
			CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(PLAYER_CARD_OTHER_PLAYER_COMP_COLUMN);

			// Using CScaleformMenuHelper to make sure these are called, as the Clear resets the flags to false.
			CScaleformMenuHelper::SHOW_COLUMN(PLAYER_AVATAR_COLUMN, false);
			CScaleformMenuHelper::SHOW_COLUMN(PLAYER_CARD_COLUMN, false);
			CScaleformMenuHelper::SHOW_COLUMN(PLAYER_CARD_LOCAL_PLAYER_COMP_COLUMN, false);
			CScaleformMenuHelper::SHOW_COLUMN(PLAYER_CARD_OTHER_PLAYER_COMP_COLUMN, false);
			SHOW_COLUMN(PLAYER_LIST_COLUMN,true);// Using this version to make the flag get set to the correct value.

			m_hasClanData = true;
			SPlayerCardManager::GetInstance().SetCrewStatus(CPlayerCardManager::CREWSTATUS_PENDING);
			SPlayerCardManager::GetInstance().SetClanId(RL_INVALID_CLAN_ID);

			CPauseMenu::GetDisplayPed()->ClearPed();
			CPauseMenu::GetDynamicPauseMenu()->SetAvatarBGMode(true);

			if(!CPauseMenu::GetLocalPlayerDisplayPed()->HasPed())
			{
				CPauseMenu::GetLocalPlayerDisplayPed()->SetPedModelLocalPlayer(PM_COLUMN_EXTRA4);
			}
		}

		m_previousPlayerDataSlots.clear();
		m_previousPlayerDataSlots.ResizeGrow( GetMaxPlayerSlotCount() );

		FillOutPlayerMissionInfo();

		if(NetworkInterface::GetActiveGamerInfo() == NULL || !NetworkInterface::GetActiveGamerInfo()->IsOnline())
		{
			SUIContexts::Activate(OFFLINE_CONTEXT);
		}

		InitDataManagerAndPaginator(newScreenId);

		return true;
	}

	return false;
}

void CPlayerListMenu::LayoutChanged( MenuScreenId iPreviousLayout, MenuScreenId iNewLayout, s32 iUniqueId, eLAYOUT_CHANGED_DIR eDir )
{
	uiDebugf3("LAYOUT CHANGED ON FRIENDS MENU. Prev: %i, New: %i, Unique: %i", iPreviousLayout.GetValue(), iNewLayout.GetValue(), iUniqueId);

	bool isThisMenu = false;

	if(iUniqueId != PLAYER_CARD_INVALID_PLAYER_ID)
	{
		if(iNewLayout.GetValue() == GetListMenuId() || iNewLayout.GetValue() == (GetListMenuId() + PREF_OPTIONS_THRESHOLD))
		{
			BasePlayerCardDataManager* pManager = GetDataMgr();
			if(pManager && pManager->IsValid())
			{
				SetPlayerCard(iUniqueId);
			}
			
			if(IsPaginatorValid()) 
			{
				#if RSG_PC
				GetPaginator()->MoveToIndex(iUniqueId);
				#endif
				GetPaginator()->UpdateItemSelection(iUniqueId, true);
			}

			CScriptMenu::LayoutChanged(iPreviousLayout, iNewLayout, iUniqueId, eDir);
			isThisMenu = true;
		}
	}

	if(!isThisMenu && IsPaginatorValid())
	{
		GetPaginator()->LoseFocus();
	}
}

void CPlayerListMenu::LoseFocusLite()
{
	ClearWarningColumn();
	SUIContexts::Deactivate(ATSTRINGHASH("HIDE_ACCEPTBUTTON",0x14211b54));

	if(GetContextMenu() && GetContextMenu()->IsOpen())
	{
		GetContextMenu()->CloseMenu();
	}

	RemoveEmblemRef(m_emblemTxdSlot);
	RemoveEmblemRef(m_localEmblemTxdSlot);

	m_currPlayerIndex = PLAYER_CARD_INVALID_PLAYER_ID;
	m_currPlayerGamerHandle = RL_INVALID_GAMER_HANDLE;

	m_isPopulatingStats = false;

	CPauseMenu::GetDisplayPed()->SetVisible(false);
	CPauseMenu::GetDisplayPed()->ClearPed();
	CPauseMenu::GetDynamicPauseMenu()->SetAvatarBGMode(false);
	SPlayerCardManager::GetInstance().ClearCurrentGamer();

	// Ensure to also reset the crew data so other pages don't accidentally use the wrong crew/clan id
	SPlayerCardManager::GetInstance().SetCrewStatus(CPlayerCardManager::CREWSTATUS_PENDING);
	SPlayerCardManager::GetInstance().SetClanId(RL_INVALID_CLAN_ID);

	CScriptMenu::LoseFocus();

	CScaleformMenuHelper::SET_BUSY_SPINNER(PLAYER_AVATAR_COLUMN, false);
	CScaleformMenuHelper::SET_BUSY_SPINNER(PM_COLUMN_MIDDLE_RIGHT, false);
	ClearAvatarSpinner();
}

void CPlayerListMenu::LoseFocus()
{
	uiDebugf3("CPlayerListMenu::LoseFocus");

	LoseFocusLite();

	m_isComparing = false;
	m_isComparingVisible = false;
	SetIsInPlayerList(false);

	CPauseMenu::GetDynamicPauseMenu()->SetAvatarBGOn(false);

	SUIContexts::Deactivate(LOCAL_PLAYER_CONTEXT);
	SUIContexts::Deactivate(HAS_PLAYERS_CONTEXT);
	SUIContexts::Deactivate(OFFLINE_CONTEXT);
	SUIContexts::Deactivate( CONTEXT_MENU_CONTEXT );

	CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(PLAYER_LIST_COLUMN);
	CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(PLAYER_AVATAR_COLUMN);
	CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(PLAYER_CARD_COLUMN);
	CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(PLAYER_CARD_LOCAL_PLAYER_COMP_COLUMN);
	CScaleformMenuHelper::SET_DATA_SLOT_EMPTY(PLAYER_CARD_OTHER_PLAYER_COMP_COLUMN);
}

void CPlayerListMenu::OnNavigatingContent(bool bAreWe)
{
	if(bAreWe)
	{
		RefreshButtonPrompts();
		if( GetPaginator() )
			GetPaginator()->UpdateItemSelection(m_currPlayerIndex,true);
	}
	else
	{
		SUIContexts::Deactivate( CONTEXT_MENU_CONTEXT );
	}
}

void CPlayerListMenu::ShutdownPaginator()
{
	if( GetPaginator() )
	{
		GetPaginator()->Shutdown();
	}
	Clear();
}

bool CPlayerListMenu::AddOrUpdateSlot( int menuIndex, int playerIndex, bool isUpdate, const PlayerDataSlot& playerSlot )
{
	//columnIndex = always 0 in this case
	//menuIndex = ?
	//menuID = this is the menu enum Friends List 
	//menuUniqueID = this is a number you put in that gets returned to you
	//menuType = always 0
	//rankValue = the rank value that goes in the emblem
	//menuItemGreyed = 0 or 1
	//menuUserString =Username
	//menu colour =  6 = cops blue, 7 = vagos orange, 8 = bikers grey, 4 = white
	//host, 1 = host 0 = not host
	//column1Icon = use the icons enums
	//column2Icon = use the icons enums
	//column3Icon = use the icons enums

	if(playerIndex == m_currPlayerIndex)
	{
		RefreshButtonPrompts();
	}

	CScaleformMovieWrapper& pauseContent = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);
	if( pauseContent.BeginMethod(isUpdate ? "UPDATE_SLOT" : "SET_DATA_SLOT") )
	{
		pauseContent.AddParam(static_cast<int>(PLAYER_LIST_COLUMN));
		pauseContent.AddParam(menuIndex);
		pauseContent.AddParam(GetListMenuId()+PREF_OPTIONS_THRESHOLD);
		pauseContent.AddParam(playerIndex);
		pauseContent.AddParam(playerSlot.m_activeState);
		pauseContent.AddParam(playerSlot.m_rank);
		pauseContent.AddParam(true);
		pauseContent.AddParam(playerSlot.m_pText ? playerSlot.m_pText : "");

		pauseContent.AddParam(playerSlot.m_color);
		pauseContent.AddParam(0);
		pauseContent.AddParam(0);
		pauseContent.AddParam(playerSlot.m_headsetIcon ? playerSlot.m_headsetIcon : playerSlot.m_onlineIcon);
		pauseContent.AddParam(0); // Rank Icon was removed
		pauseContent.AddParam(CScaleformMovieWrapper::Param(playerSlot.m_crewTag, false));
		pauseContent.AddParam(playerSlot.m_kickCount);
		
		if(playerSlot.m_pInviteState)
		{
			pauseContent.AddParam(TheText.Get(playerSlot.m_pInviteState, NULL));
			pauseContent.AddParam(playerSlot.m_inviteColor);
		}
		else
		{
			pauseContent.AddParam("");
			pauseContent.AddParam("");
		}

		#if RSG_PC
		int hash = GetMiscData().m_usingKeyboard.GetHash();
		
		if(StatsInterface::IsKeyValid(hash) && playerSlot.m_activeState > 0 && GetDataManagerType() == CPlayerCardManager::CARDTYPE_ACTIVE_PLAYER)
		{
			
			bool bUsingKeyboard = GetBoolStat(playerIndex, GetMiscData().m_usingKeyboard.GetHash(),true);
			
			if (bUsingKeyboard)
				pauseContent.AddParam("IS_PC_PLAYER");
			else
				pauseContent.AddParam("IS_CONSOLE_PLAYER");
		}

		#else	
		pauseContent.AddParam("NONE");
		#endif
		
		pauseContent.EndMethod();

		return true;
	}

	return false;
}

void CPlayerListMenu::CheckFriendListChanges()
{
#if __BANK
	if(CFriendPlayerCardDataManager::ms_bDebugHideAllFriends)
		return;
#endif

	BasePlayerCardDataManager* pManager = GetDataMgr();
	if(pManager)
	{
		rlFriendsPage* rFriends = CLiveManager::GetFriendsPage();

		//If we're currently looking at the context menu of a player, and the player is still in the list, then don't refresh the menu.
		if(IsInValidContextMenu() && rFriends->GetFriend(GetGamerHandle(m_currPlayerIndex)))
		{
			return;
		}
		
		const atArray<rlGamerHandle>& rAllGamerHandles = pManager->GetAllGamerHandles();


		// If a friend has either been added or removed, the sizes will be different.
		if((unsigned)rAllGamerHandles.size() != (unsigned)rFriends->m_NumFriends)
		{
			TriggerFullReset(PLAYER_CARD_FRIEND_CHANGE_DELAY);
			return;
		}

		// If a friend was added and another removed in the same time window, the sizes will be the same,
		// so let's makes sure both lists contain the same set of gamers.
		for(unsigned i=0; i<(unsigned)rAllGamerHandles.size(); ++i)
		{
			if(rFriends->GetFriend(rAllGamerHandles[i]) == NULL)
			{
				TriggerFullReset(PLAYER_CARD_FRIEND_CHANGE_DELAY);
				return;
			}
		}

#if ALLOW_MANUAL_FRIEND_REFRESH
		if (SUIContexts::IsActive("CanRefreshFriends"))
		{
			if (!rlFriendsManager::CanQueueFriendsRefresh(NetworkInterface::GetLocalGamerIndex()))
			{
				SUIContexts::Deactivate("CanRefreshFriends");
				CPauseMenu::RedrawInstructionalButtons();
			}
		}
		else if (rlFriendsManager::CanQueueFriendsRefresh(NetworkInterface::GetLocalGamerIndex()))
		{
			SUIContexts::Activate("CanRefreshFriends");
			CPauseMenu::RedrawInstructionalButtons();
		}
#endif // #if ALLOW_MANUAL_FRIEND_REFRESH
	}
}

void CPlayerListMenu::CheckPlayerListChanges()
{
	BasePlayerCardDataManager* pManager = GetDataMgr();
	if(pManager)
	{
		const atArray<rlGamerHandle>& gamers = pManager->GetAllGamerHandles();

		//If we're currently looking at the context menu of a player, and the player is still in the list, then don't refresh the menu.
		if(IsInValidContextMenu() && gamers.Find(GetGamerHandle(m_currPlayerIndex)) != -1)
		{
			return;
		}

		if(pManager->IsPlayerListDirty())
		{
			TriggerFullReset(PLAYER_CARD_FRIEND_CHANGE_DELAY);
		}
	}
}

bool CPlayerListMenu::IsInValidContextMenu()
{
	BasePlayerCardDataManager* pManager = GetDataMgr();
	if(pManager)
	{
		//If we're currently looking at the context menu of a player, and we're still in range, then the context menu is valid.
		return m_isInPlayerList &&
			0 <= m_currPlayerIndex &&
			m_currPlayerIndex < pManager->GetReadGamerCount() &&
			GetContextMenu() && GetContextMenu()->IsOpen();
	}

	return false;
}

void CPlayerListMenu::CheckForClanData()
{
	if(!m_hasClanData)
	{
		if(!CLiveManager::GetSocialClubMgr().IsConnectedToSocialClub())
		{
			m_hasClanData = true;
		}
		else if( GetGroupClanData().GetStatus().None() )
		{
			m_hasClanData = GetGroupClanData().Init();
		}
		else if(IsActivePlayer(m_currPlayerIndex))
		{
			m_hasClanData = true;
		}
		else
		{
			m_hasClanData = GetGroupClanData().GetStatus().Succeeded();
		}
	}
}

const CGroupClanData& CPlayerListMenu::GetGroupClanData() const
{
	return CPauseMenu::GetDynamicPauseMenu()->GetFriendClanData();
}

void CPlayerListMenu::InitDataManagerAndPaginator(MenuScreenId newScreenId)
{
	playerlistDebugf1("[%s] CPlayerListMenu::InitDataManagerAndPaginator", GetClassNameIdentifier());
	
	SPlayerCardManager::GetInstance().Init(GetDataManagerType(), m_listRules);

	if(m_shouldReinit)
	{
		playerlistDebugf1("[%s] CPlayerListMenu::InitDataManagerAndPaginator - Should re-init paginator.", GetClassNameIdentifier());
		InitPaginator(newScreenId);
	}
	m_shouldReinit = true;
}

const rlClanMembershipData* CPlayerListMenu::GetClanData(const rlGamerHandle& gamerHandle) const
{
	const rlClanMembershipData* pRetVal = NULL;

	CNetGamePlayer* pNetPlayer = NetworkInterface::GetPlayerFromGamerHandle(gamerHandle);
	if (pNetPlayer)
	{
		pRetVal = &pNetPlayer->GetClanMembershipInfo();
	}

	if(!pRetVal)
	{
		const CGroupClanData& rData = GetGroupClanData();

		if(rData.GetStatus().Succeeded())
		{
			for(int i=0; i<rData.GetResultSize(); ++i)
			{
				if(gamerHandle == rData[i].m_MemberInfo.m_GamerHandle)
				{
					pRetVal = &rData[i].m_MemberClanInfo;
				}
			}
		}
	}

	return pRetVal;
}

void CPlayerListMenu::SetupCardTitle(PM_COLUMNS column, int playerIndex, int characterSlot, const PlayerCard& rCard)
{
	const char* pPlayerName = "";
	rlGamerHandle gamerHandle = RL_INVALID_GAMER_HANDLE;
	BasePlayerCardDataManager* pManager = GetDataMgr();

	uiDebugf3("[CPlayerListMenu] SetupCardTitle");

	if(playerIndex == PLAYER_CARD_LOCAL_PLAYER_ID)
	{
		if(NetworkInterface::GetActiveGamerInfo() != NULL)
		{
			#if RSG_DURANGO
			if (NetworkInterface::GetActiveGamerInfo()->HasDisplayName())
				pPlayerName = NetworkInterface::GetActiveGamerInfo()->GetDisplayName();
			else
				pPlayerName = NetworkInterface::GetActiveGamerInfo()->GetName();
			#else
			pPlayerName = NetworkInterface::GetActiveGamerInfo()->GetName();
			#endif

			gamerHandle = NetworkInterface::GetActiveGamerInfo()->GetGamerHandle();
		}
	}
	else if(pManager)
	{
		#if RSG_DURANGO
		
		pPlayerName = pManager->GetName(playerIndex);
		#else
		pPlayerName = pManager->GetName(playerIndex);
		#endif
		
		gamerHandle = pManager->GetGamerHandle(playerIndex);
	}

	bool isLocalPlayer = IsLocalPlayer(gamerHandle);
	int rankColor = rCard.m_color;
	char crewName[30] = {0};
	int crewRank = GetCrewRank(playerIndex);
	int crewSize = 0;
	char crewTag[NetworkClan::FORMATTED_CLAN_TAG_LEN] = {0};
	char crewRankTitle[RL_CLAN_NAME_MAX_CHARS] = {0};

	if(NetworkInterface::IsNetworkOpen())
	{
		CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(gamerHandle);
		if(pPlayer)
		{
			rankColor = GetPlayerColor(playerIndex);
			safecpy( crewName, pPlayer->GetClanDesc().m_ClanName );
			crewSize = pPlayer->GetClanDesc().m_MemberCount;
		}
	}

	const rlClanMembershipData* pClanData = GetClanData(gamerHandle);
	if( pClanData && pClanData->IsValid())
	{
		NetworkClan::GetUIFormattedClanTag(pClanData, crewTag, COUNTOF(crewTag));

		if(crewName[0] == '\0')
		{
			safecpy( crewName, pClanData->m_Clan.m_ClanName );
		}

		crewSize = pClanData->m_Clan.m_MemberCount;
	}

	if(CScaleformMenuHelper::SET_COLUMN_TITLE(column, pPlayerName, false))
	{
		const char* pClanEmblem = GetCrewEmblem(gamerHandle);

		Color32 color;
		bool hasColor = GetCrewColor(playerIndex, color);
		if(hasColor)
		{
			SPlayerCardManager::GetInstance().SetCrewStatus(CPlayerCardManager::CREWSTATUS_HASDATA);
			SPlayerCardManager::GetInstance().SetCurrentCrewColor(color);
		}
		else if(IsClanDataReady() || crewName[0] == '\0')
		{
			SPlayerCardManager::GetInstance().SetCrewStatus(CPlayerCardManager::CREWSTATUS_NODATA);
		}

		const rlClanMembershipData* pClanData = GetClanData(gamerHandle);
		if(pClanData && pClanData->IsValid())
		{
			SPlayerCardManager::GetInstance().SetClanId(pClanData->m_Clan.m_Id);
		}
		else
		{
			SPlayerCardManager::GetInstance().SetClanId(RL_INVALID_CLAN_ID);
		}

		if(pClanEmblem &&
			hasColor &&
			GetCrewRankTitle(playerIndex, crewRankTitle, RL_CLAN_NAME_MAX_CHARS, crewRank))
		{
			if(playerIndex == PLAYER_CARD_LOCAL_PLAYER_ID)
			{
				m_localPlayerHasCrewInfo = true;
				AddEmblemRef(m_localEmblemTxdSlot, pClanEmblem);
			}
			else
			{
				m_hasCrewInfo = true;
				AddEmblemRef(m_emblemTxdSlot, pClanEmblem);
			}

			m_hadHint = false;

			CScaleformMgr::AddParamString(crewName);
			CScaleformMgr::AddParamInt(rankColor);
			CScaleformMgr::AddParamInt(rCard.m_rankIcon);

			CScaleformMgr::AddParamString(crewRankTitle); // Crew Rank
			CScaleformMgr::AddParamString(crewTag, false);
	
			CScaleformMgr::AddParamString(pClanEmblem, false);
			CScaleformMgr::AddParamString(pClanEmblem, false);
	
			CScaleformMgr::AddParamInt(crewRank);
	
			CScaleformMgr::AddParamInt(color.GetRed()); // Crew Color R
			CScaleformMgr::AddParamInt(color.GetGreen()); // Crew Color G
			CScaleformMgr::AddParamInt(color.GetBlue()); // Crew Color B

			CScaleformMgr::AddParamInt(crewSize);
		}
		else
		{
			if(playerIndex == PLAYER_CARD_LOCAL_PLAYER_ID)
			{
				m_localPlayerHasCrewInfo = false;
			}
			else
			{
				m_hasCrewInfo = false;
			}

			CScaleformMgr::AddParamString("");
			CScaleformMgr::AddParamInt(rankColor);
			CScaleformMgr::AddParamInt(rCard.m_rankIcon);

			CScaleformMgr::AddParamString(""); // Crew Rank
			CScaleformMgr::AddParamString(crewTag, false);

			m_hadHint = false;
		}
		
		CScaleformMgr::EndMethod(false);
	}

	CScaleformMovieWrapper& pauseContent = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);
	if( pauseContent.BeginMethod("SET_DATA_SLOT") )
	{
		float rank = (float)GetStatAsFloat(playerIndex, rCard.m_rankStat.GetStat(characterSlot));

		pauseContent.AddParam(static_cast<int>(column));  // The View the slot is to be added to
		pauseContent.AddParam(0);  // The numbered slot the information is to be added to
		pauseContent.AddParam(0);  // The menu ID
		pauseContent.AddParam(0);  // The unique ID
		pauseContent.AddParam(1);  // The Menu item type (see below)
		pauseContent.AddParam(0);  // The initial index of the slot (0 default, can be 0,1,2...x in a multiple selection)

		pauseContent.AddParam(GetBoolStat(playerIndex, rCard.m_planeStat.GetStat(characterSlot)));
		pauseContent.AddParam(GetBoolStat(playerIndex, rCard.m_heliStat.GetStat(characterSlot)));
		pauseContent.AddParam(GetBoolStat(playerIndex, rCard.m_boatStat.GetStat(characterSlot)));
		pauseContent.AddParam(GetBoolStat(playerIndex, rCard.m_carStat.GetStat(characterSlot))); // Car
		pauseContent.AddParam(rank); // rank

		// Rank Title
		{
			u32 titleHash = SPlayerCardManager::GetInstance().GetXMLDataManager().GetTitleInfo().GetTitle(ATSTRINGHASH("RankTitles",0x776db9ec), (float)rank);

			pauseContent.AddParam(GetString(titleHash)); // Text/Number
			pauseContent.AddParam(0); // Icon
			pauseContent.AddParam(0); // Color
		}

		// Bad Sport
		{
			float currBadSportValue = (float)GetStatAsFloat(playerIndex, GetMiscData().m_badSportStat);
			float badSportLimit = GetMiscData().m_defaultBadSportLimit;
			float dirtyPlayerLimit = GetMiscData().m_defaultDirtyPlayerLimit;
			const char* pBadSportText = "PCARD_CLEAN_PLAYER";

			if(Tunables::IsInstantiated())
			{
				Tunables::GetInstance().Access(GetMiscData().m_badSportTunable, badSportLimit);
				Tunables::GetInstance().Access(GetMiscData().m_dirtyPlayerTunable, dirtyPlayerLimit);
			}

			if(isLocalPlayer && (StatsInterface::IsBadSport() || CNetwork::IsCheater()))
			{
				currBadSportValue = badSportLimit;
			}

			MenuScreenId eMenuId = GetMenuScreenId();

			if (!isLocalPlayer && (eMenuId == MENU_UNIQUE_ID_PLAYERS ||eMenuId == MENU_UNIQUE_ID_PLAYERS_LIST ) )
			{
				CNetGamePlayer* pPlayer =  NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(playerIndex));
				if( pPlayer && pPlayer->IsCheater())
				{
					currBadSportValue = badSportLimit;
				}
			}

			if(badSportLimit <= currBadSportValue)
			{
				pBadSportText = "PCARD_BAD_SPORT";
			}
			else if(dirtyPlayerLimit <= currBadSportValue)
			{
				pBadSportText = "PCARD_DIRTY_PLAYER";
			}

			pauseContent.AddParam(TheText.Get(pBadSportText)); // Text/Number
			pauseContent.AddParam(0); // Icon
			pauseContent.AddParam(0); // Color
		}

		for(int i=0; i<rCard.m_titleSlots.size(); ++i)
		{
			char descBuffer[MAX_CHARS_IN_MESSAGE];
			const char* pDesc = TheText.Get(rCard.m_titleSlots[i].m_textHash, NULL);
			float val = 0.0f;

			if(IsActivePlayer(playerIndex) && !IsLocalPlayer(playerIndex))
			{
				CNetObjPlayer* pObjPlayer = GetNetObjPlayer(playerIndex);
				if(pObjPlayer)
				{
					val = pObjPlayer->GetStatValue(static_cast<PlayerCard::eNetworkedStatIds>(PlayerCard::NETSTAT_START_TITLE_RATIO_STATS + i));
				}
			}
			else
			{
				float num = (float)GetStatAsFloat(playerIndex, rCard.m_titleSlots[i].m_stat.GetStat(characterSlot), 0.0);
				float denom = 0.0f;
				u32 extraStatHash = rCard.m_titleSlots[i].m_extraStat.GetStat(characterSlot);
				
				if(extraStatHash)
				{
					denom = (float)GetStatAsFloat(playerIndex, extraStatHash, 0.0);

					// HACK:
					// When calculating the K/D ratio stat make sure to count suicides
					// in the denom since it's tracked outside of deaths.
					if(rCard.m_titleSlots[i].m_textHash == ATSTRINGHASH("PCARD_KD_RATIO", 0xB6E8D58E))
					{
						float extraDenom = (float)GetStatAsFloat(playerIndex, STAT_MP_SUICIDES_PLAYER);
						denom += extraDenom;
					}
				}

				if(denom > 0.0f)
				{
					val = num / denom;
				}
				else
				{
					val = num;
				}
			}

			CNumberWithinMessage data[1];
			data[0].Set(val, 2);

			CMessages::InsertNumbersAndSubStringsIntoString(pDesc,
				data, NELEM(data),
				NULL, 0,
				descBuffer, NELEM(descBuffer) );

			pauseContent.AddParam(descBuffer); // Text/Number
			pauseContent.AddParam(0); // Icon
			pauseContent.AddParam(0); // Color
		}

		pauseContent.EndMethod();
	}
}

void CPlayerListMenu::AddEmblemRef(int& refSlot, const char* pClanEmblem)
{
	RemoveEmblemRef(refSlot);

	strLocalIndex txdSlot = strLocalIndex(g_TxdStore.FindSlot(pClanEmblem));
	if (g_TxdStore.IsValidSlot(txdSlot))
	{
		g_TxdStore.AddRef(txdSlot, REF_OTHER);
		refSlot = txdSlot.Get();
	}
#if !__NO_OUTPUT
	else
	{
		uiErrorf("CPlayerListMenu::SetupCardTitle - failed to get valid txd for clan emblem %s at slot %d", pClanEmblem, txdSlot.Get());
	}
#endif
}

void CPlayerListMenu::RemoveEmblemRef(int& refSlot)
{
	if(g_TxdStore.IsValidSlot(strLocalIndex(refSlot)))
	{
		g_TxdStore.RemoveRef(strLocalIndex(refSlot), REF_OTHER);
	}

	refSlot = -1;
}


void CPlayerListMenu::SET_DESCRIPTION(PM_COLUMNS column, const char* descStr, int descType, const char* crewTagStr)
{
	if( CScaleformMgr::BeginMethod(CPauseMenu::GetContentMovieId(), SF_BASE_CLASS_PAUSEMENU, "SET_DESCRIPTION") )
	{
		CScaleformMgr::AddParamInt(column);
		CScaleformMgr::AddParamString(descStr, false);
		CScaleformMgr::AddParamInt(descType);
		CScaleformMgr::AddParamString(crewTagStr);

		CScaleformMgr::EndMethod();
	}
}

void CPlayerListMenu::HighlightGamerHandle( const rlGamerHandle& gamerHandle )
{
	Paginator* pPaginator = GetPaginator();
	BasePlayerCardDataManager* pManager = GetDataMgr();

	if(pPaginator && pManager && gamerHandle != RL_INVALID_GAMER_HANDLE)
	{
		const atArray<rlGamerHandle>& gamerHandles = pManager->GetAllGamerHandles();

		for(int i = 0; i < gamerHandles.size(); ++i)
		{
			if(gamerHandles[i] == gamerHandle)
			{
				// The last selected gamer is in our new list of gamers so keep them selected
				pPaginator->UpdateItemSelection(i, false);
				break;
			}
		}
	}
}

#if __BANK

void CPlayerListMenu::AddWidgets(bkBank* pBank)
{
	CScriptMenu::AddWidgets(pBank);
	if( m_pMyGroup )
	{
		m_pMyGroup->AddToggle("Hide All friends (requires unpausing to take effect)", &CFriendPlayerCardDataManager::ms_bDebugHideAllFriends);
		m_pMyGroup->AddButton("Broadcast local player's stats", CFA(CNetObjPlayer::TriggerStatUpdate));
	}
}

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CSPPlayerListMenu::CSPPlayerListMenu(CMenuScreen& owner, bool bDontInitPaginator) : CPlayerListMenu(owner, bDontInitPaginator)
{
	const rlGamerInfo* pGamer = NetworkInterface::GetActiveGamerInfo();
	m_isLocalPlayerOnline = pGamer ? pGamer->IsOnline() : false;
	if(!bDontInitPaginator)
		m_pPaginator = rage_new CFriendListMenuDataPaginator(&owner);
}

CSPPlayerListMenu::~CSPPlayerListMenu()
{
	if ( m_pPaginator )
	{
		delete m_pPaginator;
		m_pPaginator = NULL;
	}
}

bool CSPPlayerListMenu::Populate(MenuScreenId newScreenId)
{
	if( CPlayerListMenu::Populate(newScreenId) )
	{
		m_hasClanData = GetGroupClanData().Init();
		return true;
	}

	return false;
}

void CSPPlayerListMenu::SetupCardTitle(PM_COLUMNS column, int playerIndex, int UNUSED_PARAM(characterSlot), const PlayerCard& rCard)
{
	BasePlayerCardDataManager* pManager = GetDataMgr();
	if(pManager)
	{
		const char* pPlayerName = "";
		rlGamerHandle gamerHandle = RL_INVALID_GAMER_HANDLE;

		if(playerIndex == PLAYER_CARD_LOCAL_PLAYER_ID)
		{
			if(NetworkInterface::GetActiveGamerInfo() != NULL)
			{
				pPlayerName = NetworkInterface::GetActiveGamerInfo()->GetName();
				gamerHandle = NetworkInterface::GetActiveGamerInfo()->GetGamerHandle();
			}
		}
		else
		{
			pPlayerName = pManager->GetName(playerIndex);
			gamerHandle = pManager->GetGamerHandle(playerIndex);
		}

		char crewTag[NetworkClan::FORMATTED_CLAN_TAG_LEN] = {0};
		const rlClanMembershipData* pClanData = GetClanData(gamerHandle);
		if( pClanData && pClanData->IsValid())
		{
			NetworkClan::GetUIFormattedClanTag(pClanData, crewTag, COUNTOF(crewTag));
		}

		SET_DATA_SLOT_EMPTY(PLAYER_CARD_COLUMN);

		CScaleformMovieWrapper& pauseContent = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);
		if( pauseContent.BeginMethod("SET_COLUMN_TITLE") )
		{
			pauseContent.AddParam(static_cast<int>(column));
			pauseContent.AddParam(pPlayerName); // gamer name
			pauseContent.AddParam(crewTag); // crew tag string
			pauseContent.AddParam(rCard.m_color); // color

			pauseContent.EndMethod();
		}

		const int bufferSize = 20;
		char buffer[bufferSize] = {0};

		double percentComplete = GetStatAsFloat(playerIndex, GetMiscData().m_gameCompletionStat.GetHash(), 0.0);
		formatf(buffer, bufferSize, "%d%%", (int)percentComplete);

		if( pauseContent.BeginMethod("SET_DATA_SLOT") )
		{
			pauseContent.AddParam(static_cast<int>(column));
			pauseContent.AddParam(0);
			pauseContent.AddParam(0);
			pauseContent.AddParam(buffer);
			pauseContent.AddParam("");
			pauseContent.AddParam("");
			pauseContent.AddParam("");
			pauseContent.AddParam("");

			pauseContent.EndMethod();
		}
	}
}

bool CSPPlayerListMenu::SetCardDataSlot(PM_COLUMNS column, int playerIndex, int characterSlot, int slot, const PlayerCard& rCard, bool bCallEndMethod /*= true*/)
{
	BasePlayerCardDataManager* pManager = GetDataMgr();
	if(pManager && uiVerify(0 <= slot && slot < rCard.m_slots.GetCount()))
	{
		u32 textHash = rCard.m_slots[slot].m_textHash;

		if(m_isComparingVisible && rCard.m_slots[slot].m_compareTextHash)
		{
			textHash = rCard.m_slots[slot].m_compareTextHash;
		}

		//columnIndex = always 2 in this case
		//menuIndex = 0?.X amount
		//menuID = this is the menu enum can be anything not really used 
		//menuUniqueID = this is a number you put in that gets returned to you
		//menuType = always 1 for freemode (0 cops & crooks) sp=?
		//initial Value = always 0
		//menuItemGreyed = always 1
		//statString = e.g Kill/death
		//statValue = string
		if(CScaleformMenuHelper::SET_DATA_SLOT(column, PLAYER_CARD_SLOT_FIX(slot), 0, PLAYER_CARD_SLOT_FIX(slot),
			(eOPTION_DISPLAY_STYLE)0, 0, 1, GetString(textHash), false))
		{
			AddStatParam(playerIndex, characterSlot, rCard.m_slots[slot]);
			CScaleformMgr::AddParamInt(rCard.m_slots[slot].m_color);

			if(bCallEndMethod)
			{
				CScaleformMgr::EndMethod(false);
			}

			return true;
		}

	}

	return false;
}

void CSPPlayerListMenu::SetupAvatar(int playerIndex)
{
	const rlGamerHandle& gamerHandle = GetGamerHandle(playerIndex);
	SPlayerCardManager::GetInstance().SetCurrentGamer(gamerHandle);

	CPauseMenu::GetDisplayPed()->ClearPed(false);
	UpdatePedState(playerIndex);
}

void CSPPlayerListMenu::SetupNeverPlayedWarning(int playerIndex)
{
	SetupNeverPlayedWarningHelper(playerIndex, "PCARD_NEVER_PLAYED_SP", PLAYER_AVATAR_COLUMN, 2);
}

bool CSPPlayerListMenu::DoesPlayerCardNeedRefresh(int UNUSED_PARAM(playerIndex), bool UNUSED_PARAM(hasCrewInfo))
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CMPPlayerListMenu::CMPPlayerListMenu(CMenuScreen& owner, bool bDontInitPaginator) : CPlayerListMenu(owner, bDontInitPaginator)
{
}

CMPPlayerListMenu::~CMPPlayerListMenu()
{
}

void CMPPlayerListMenu::SetupHelpSlot(int playerIndex)
{
	BasePlayerCardDataManager* pManager = GetDataMgr();
	if(pManager && (playerIndex != PLAYER_CARD_INVALID_PLAYER_ID && playerIndex < pManager->GetReadGamerCount()) && HasPlayed(playerIndex))
	{
		CheckForClanData();

		rlGamerHandle gamerHandle = RL_INVALID_GAMER_HANDLE;
		if(IsLocalPlayer(playerIndex))
		{
			if(NetworkInterface::GetActiveGamerInfo() != NULL)
			{
				gamerHandle = NetworkInterface::GetActiveGamerInfo()->GetGamerHandle();
			}
		}
		else
		{
			gamerHandle = pManager->GetGamerHandle(playerIndex);
		}

		bool isInCrew = false;
		bool isLocalInCrew = false;
		bool isLocalPlayer = IsLocalPlayer(playerIndex);
		char localCrewTag[NetworkClan::FORMATTED_CLAN_TAG_LEN] = {0};
		const char* pLocalClanName = NULL;
		const char* pMsg = NULL;

		if(NetworkInterface::IsNetworkOpen())
		{
			CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(gamerHandle);
			if(pPlayer)
			{
				isInCrew = pPlayer->GetClanDesc().IsValid();
			}

			CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();
			if(pLocalPlayer)
			{
				isLocalInCrew = pLocalPlayer->GetClanDesc().IsValid();
				if(isLocalInCrew)
				{
					pLocalClanName = pLocalPlayer->GetClanDesc().m_ClanName;
					NetworkClan::GetUIFormattedClanTag(&pLocalPlayer->GetClanMembershipInfo(), localCrewTag, COUNTOF(localCrewTag));
				}
			}
		}

		const rlClanMembershipData* pClanData = GetClanData(gamerHandle);
		if(pClanData && pClanData->IsValid())
		{
			isInCrew = true;
		}

		if(isInCrew && isLocalPlayer)
		{
			isLocalInCrew = true;
		}

		m_hasHint = false;
		eDescType descType = DESC_TYPE_TXT;

		if(m_hasSentCrewInvite)
		{
			pMsg = TheText.Get("PCARD_CREW_INVITE");
			m_hadSentCrewInvite = m_hasSentCrewInvite;
			descType = DESC_TYPE_INV_SENT;
		}
		else if(!isInCrew)
		{
			if(isLocalInCrew)
			{
				if(!m_isComparing)
				{
					if(!CanInviteToCrew())
					{
						pMsg = TheText.Get("PCARD_NO_CREW");
					}
				}
			}
			else
			{
				descType = DESC_TYPE_SC;

				if(!CLiveManager::GetSocialClubMgr().IsConnectedToSocialClub())
				{
					pMsg = TheText.Get("PCARD_JOIN_SC");
				}
				else if(!CLiveManager::GetSocialClubMgr().IsOnlinePolicyUpToDate())
				{
					pMsg = TheText.Get("PCARD_UPDATE_SC");
				}
				else
				{
					pMsg = TheText.Get("PCARD_JOIN_CREW");
				}
			}
		}

		if(pMsg)
		{
			m_hasHint = true;
		}

		if(m_hasHint != m_hadHint)
		{
			SET_DATA_SLOT_EMPTY(PLAYER_CARD_COLUMN);
			SET_DATA_SLOT_EMPTY(PLAYER_CARD_LOCAL_PLAYER_COMP_COLUMN);
			SET_DATA_SLOT_EMPTY(PLAYER_CARD_OTHER_PLAYER_COMP_COLUMN);

			SHOW_COLUMN(PLAYER_CARD_COLUMN, false);
			SHOW_COLUMN(PLAYER_CARD_LOCAL_PLAYER_COMP_COLUMN, false);
			SHOW_COLUMN(PLAYER_CARD_OTHER_PLAYER_COMP_COLUMN, false);
		}


		PM_COLUMNS column = m_isComparing ? PLAYER_CARD_OTHER_PLAYER_COMP_COLUMN : PLAYER_CARD_COLUMN;

		if(m_hasHint)
		{
			uiDisplayf("CMPPlayerListMenu::SetupHelpSlot - Setting Hint, '%s'.", pMsg);
			SET_DESCRIPTION(column, pMsg, descType, localCrewTag);
		}
		else if(m_hadHint)
		{
			SET_DESCRIPTION(column, "", descType, localCrewTag);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CFriendsMenuMP::CFriendsMenuMP(CMenuScreen& owner):
	CMPPlayerListMenu(owner, true),
	m_uDownloadTimerMS(0)
{
	m_pPaginator = rage_new CFriendListMenuDataPaginator(&owner);
}

CFriendsMenuMP::~CFriendsMenuMP()
{
	if ( m_pPaginator )
	{
		delete m_pPaginator;
		m_pPaginator = NULL;
	}

}

void CFriendsMenuMP::InitPaginator(MenuScreenId newScreenId)
{
	Paginator* pPaginator = GetPaginator();
	if(pPaginator) 
	{
		if(pPaginator->HasBeenInitialized())
		{
			pPaginator->Shutdown();
		}

		pPaginator->Init(this, GetMaxPlayerSlotCount() , newScreenId, PLAYER_LIST_COLUMN);

		pPaginator->UpdateItemSelection(0, false);
	}
}

bool CFriendsMenuMP::Populate(MenuScreenId newScreenId)
{
	if( CMPPlayerListMenu::Populate(newScreenId))
	{
		m_uDownloadTimerMS = fwTimer::GetSystemTimeInMilliseconds();
		m_hasClanData = GetGroupClanData().Init();

		return true;
	}

	return false;
}

bool CFriendsMenuMP::UpdateInput(s32 sInput)
{
#if RSG_PC
	if (rlFriendsManager::GetTotalNumFriends(NetworkInterface::GetLocalGamerIndex()) == 0)
	{
		if(sInput == PAD_CROSS)
		{
			g_SystemUi.ShowFriendSearchUi(NetworkInterface::GetLocalGamerIndex());
			return true;
		}
	}

#endif // RSG_PC
	return CPlayerListMenu::UpdateInput(sInput);
}

int CFriendsMenuMP::GetMaxPlayerSlotCount() const 
{
	int tunable = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("FRIEND_MENU_MAX_STATS_QUERY", 0x222DADB0), GetMiscData().m_maxMenuSlots); 
	int result =  std::min<int>( tunable, GetMiscData().m_maxMenuSlots );

	return result;
}


bool CFriendsMenuMP::HasTimedOut() const
{
	// url:bugstar:5580619 - JRM
	// RageNet HTTP tasks time out just fine on their own at 30s.
	// Timing out CFriendsMenuMP after getting the gamer handle list causes this state machine
	// to reset infinitely due to the extreme spaghetti-ness of this code. This is the smallest
	// change I feel comfortable making that fixes the infinite resets. Closing the pause menu and 
	// opening up the friends menu again works just fine after encountering an error.
	if (m_isPopulatingStats)
		return false;

	return fwTimer::GetSystemTimeInMilliseconds() - m_uDownloadTimerMS > DOWNLOAD_TIMEOUT;
}

void CFriendsMenuMP::HandleTimeout()
{
	SET_DATA_SLOT_EMPTY(PLAYER_LIST_COLUMN);
	SET_DATA_SLOT_EMPTY(PLAYER_CARD_COLUMN);
	SET_DATA_SLOT_EMPTY(PLAYER_CARD_LOCAL_PLAYER_COMP_COLUMN);
	SET_DATA_SLOT_EMPTY(PLAYER_CARD_OTHER_PLAYER_COMP_COLUMN);

	ShowColumnWarning_EX(NoPlayersColumns(), "PCARD_SYNC_ERROR", "CWS_WARNING");

	CScaleformMenuHelper::SET_BUSY_SPINNER(PLAYER_AVATAR_COLUMN, false);
	CScaleformMenuHelper::SET_BUSY_SPINNER(DownloadingMessageColumns(), false);
	m_uDownloadTimerMS = 0;
	m_isPopulated = true;
}

void CFriendsMenuMP::HandleUIReset( const rlGamerHandle& cachedCurrGamerHandle, const BasePlayerCardDataManager& rManager )
{
	LoseFocus();
	Populate(GetUncorrectedMenuScreenId());

	Paginator* pPaginator = GetPaginator();
	if(pPaginator && cachedCurrGamerHandle != RL_INVALID_GAMER_HANDLE)
	{
		const atArray<rlGamerHandle>& gamerHandles = rManager.GetAllGamerHandles();

		for(int i = 0; i < gamerHandles.size(); ++i)
		{
			if(gamerHandles[i] == cachedCurrGamerHandle)
			{
				// The last selected gamer is in our new list of gamers so keep them selected
				pPaginator->UpdateItemSelection(i, false);
				break;
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CFriendsMenuSP::CFriendsMenuSP(CMenuScreen& owner):
	CSPPlayerListMenu(owner, true)
{
	m_pPaginator = rage_new CFriendListMenuDataPaginator(&owner);
}

CFriendsMenuSP::~CFriendsMenuSP()
{
	if ( m_pPaginator )
	{
		delete m_pPaginator;
		m_pPaginator = NULL;
	}

}

void CFriendsMenuSP::InitPaginator(MenuScreenId newScreenId)
{
	Paginator* pPaginator = GetPaginator();
	if(pPaginator) 
	{
		if(pPaginator->HasBeenInitialized())
		{
			pPaginator->Shutdown();
		}


		pPaginator->Init(this, GetMaxPlayerSlotCount(), newScreenId, PLAYER_LIST_COLUMN);
		pPaginator->UpdateItemSelection(0, false);
		m_WasSystemError = false;
	}
}

bool CFriendsMenuSP::Populate(MenuScreenId newScreenId)
{
	if( CSPPlayerListMenu::Populate(newScreenId))
	{
		m_hasClanData = GetGroupClanData().Init();

		return true;
	}

	return false;
}

bool CFriendsMenuSP::UpdateInput(s32 sInput)
{
#if RSG_PC
	if (rlFriendsManager::GetTotalNumFriends(NetworkInterface::GetLocalGamerIndex()) == 0)
	{
		if(sInput == PAD_CROSS)
		{
			g_SystemUi.ShowFriendSearchUi(NetworkInterface::GetLocalGamerIndex());
			return true;
		}
	}

#endif
	return CPlayerListMenu::UpdateInput(sInput);

}

int CFriendsMenuSP::GetMaxPlayerSlotCount() const 
{
	int tunable = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("FRIEND_MENU_MAX_STATS_QUERY", 0x222DADB0), GetMiscData().m_maxMenuSlots); 
	int result =  std::min<int>( tunable, GetMiscData().m_maxMenuSlots );
	return result;
}

void CFriendsMenuSP::HandleUIReset(const rlGamerHandle& rCachedCurrGamerHandle, const BasePlayerCardDataManager& rManager)
{
	LoseFocus();
	Populate(GetUncorrectedMenuScreenId());

	Paginator* pPaginator = GetPaginator();
	if(pPaginator && rCachedCurrGamerHandle != RL_INVALID_GAMER_HANDLE)
	{
		const atArray<rlGamerHandle>& gamerHandles = rManager.GetAllGamerHandles();

		for(int i = 0; i < gamerHandles.size(); ++i)
		{
			if(gamerHandles[i] == rCachedCurrGamerHandle)
			{
				// The last selected gamer is in our new list of gamers so keep them selected
				pPaginator->UpdateItemSelection(i, false);
				break;
			}
		}
	}}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CPlayersMenu::CPlayersMenu(CMenuScreen& owner)
: CMPPlayerListMenu(owner)
{
	m_listRules.m_hidePlayersInTutorial = true;
}

CPlayersMenu::~CPlayersMenu()
{
}

bool CPlayersMenu::Populate(MenuScreenId newScreenId)
{
	if( CMPPlayerListMenu::Populate(newScreenId) )
	{
		SetIsInPlayerList(true);
		return true;
	}

	return false;
}

void CPlayersMenu::SetupNeverPlayedWarning(int playerIndex)
{
	SetupNeverPlayedWarningHelper(playerIndex, "PCARD_SYNCING_STATS", PLAYER_AVATAR_COLUMN, 2);
}

bool CPlayersMenu::IsSyncingStats(int playerIndex, bool updateStatVersion)
{
	bool isSyncingStats = true;

	if(IsLocalPlayer(playerIndex))
	{
		isSyncingStats = false;
	}
	else if(IsActivePlayer(playerIndex))
	{
		CNetObjPlayer* pNetPlayer = GetNetObjPlayer(playerIndex);
		if(pNetPlayer)
		{
			isSyncingStats = (pNetPlayer->GetStatVersion() == 0);

			if(updateStatVersion)
			{
				m_currentStatVersion = pNetPlayer->GetStatVersion();
			}
		}
	}

	return isSyncingStats;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CLobbyMenu::CLobbyMenu(CMenuScreen& owner) : CPlayersMenu(owner)
{
}

CLobbyMenu::~CLobbyMenu()
{
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CPartyMenu::CPartyMenu(CMenuScreen& owner): CMPPlayerListMenu(owner)
{
}

CPartyMenu::~CPartyMenu()
{
}

bool CPartyMenu::Populate(MenuScreenId newScreenId)
{
	if(CMPPlayerListMenu::Populate(newScreenId))
	{
		SetIsInPlayerList(true);

		m_hasClanData = m_partyData.Init();
		return true;
	}

	return false;
}

bool CPartyMenu::CanHandleGoToSCInput() const
{
#if __XENON
	return false;
#else
	return true;
#endif
}

bool CPartyMenu::UpdateInput(s32 sInput)
{
	bool retVal = CMPPlayerListMenu::UpdateInput(sInput);

#if PARTY_PLATFORM
	if(sInput == PAD_TRIANGLE && m_isPopulated && !m_inviteTimer.IsStarted() && !CNetwork::GetNetworkSession().IsSoloSession(SessionType::ST_Physical))
	{
		BasePlayerCardDataManager* pManager = GetDataMgr();
		if(pManager)
		{
			atArray<rlGamerHandle> gamersToInvite;
			const atArray<rlGamerHandle>& rGamerHandles = pManager->GetAllGamerHandles();
			for(int i=0; i<rGamerHandles.size(); ++i)
			{
				if(NetworkInterface::GetPlayerFromGamerHandle(rGamerHandles[i]) == NULL)
				{
					gamersToInvite.PushAndGrow(rGamerHandles[i]);
				}
			}

			if(!gamersToInvite.empty())
			{
				CNetwork::GetNetworkSession().SendInvites(gamersToInvite.GetElements(), gamersToInvite.size(), NULL, NULL);
				SetupCenterColumnWarning("PCARD_SEND_PART_INVT", "PCARD_SEND_INVT_TITLE");
			}

			retVal = true;
		}
	}
#endif

	return retVal;
}

//////////////////////////////////////////////////////////////////////////
CCrewDetailMenu::CCrewDetailMenu(CMenuScreen& owner) : 
	CPlayerListMenu(owner),
	m_IsFriendsWith(false)

{
	m_listRules.m_hideLocalPlayer = false;
	owner.FindDynamicData("sp_path", m_SPPath);
}

atString& CCrewDetailMenu::GetScriptPath()
{
	if(NetworkInterface::IsNetworkOpen())
		return m_ScriptPath;
	return m_SPPath;
}

bool CCrewDetailMenu::InitScrollbar()
{
	if( CPauseMenu::IsScreenDataValid(MENU_UNIQUE_ID_CREW_LIST) )
	{
		CMenuScreen& rData = CPauseMenu::GetScreenData(MENU_UNIQUE_ID_CREW_LIST);
		rData.INIT_SCROLL_BAR();
	}
	return false;
}

void CCrewDetailMenu::BuildContexts()
{
	BasePlayerCardDataManager* pManager = GetDataMgr();

	// if no results, just bail
	if( !pManager || pManager->GetReadGamerCount() <= 0 )
	{
		SUIContexts::Activate("NO_RESULTS");
		return;
	}

	if(uiVerify(pManager) && uiVerifyf(m_currPlayerIndex >= 0 && m_currPlayerIndex < pManager->GetReadGamerCount(), "CPlayerListMenu::BuildContexts - m_currPlayerIndex=%d gamerCount=%d", m_currPlayerIndex, pManager->GetReadGamerCount()))
	{
		const GamerData& gamerData = pManager->GetGamerData(m_currPlayerIndex);

		if(gamerData.m_isDefaultOnline)
		{
			SUIContexts::Activate("OnlineCrew");
		}

		// playing it safe and restricting this to just the crew detail menu (we *COULD* probably make it everywhere, but it'd be good to ask)
		SUIContexts::Activate("ShowGamerCard");

		CPlayerListMenu::BuildContexts();
	}
}

void CCrewDetailMenu::SetPlayerCard(int playerIndex, bool UNUSED_PARAM(refreshAvatar))
{

	if( m_currPlayerIndex != playerIndex )
	{
		CPauseMenu::GetDisplayPed()->ClearPed(false);
	}
	SetCurrentlyViewedPlayer(playerIndex);

	SPlayerCardManager::GetInstance().SetCurrentGamer(GetGamerHandle(playerIndex));

	//m_crewCardCompletion.Reset();
	KillScript();

	SetupAvatar(playerIndex);
	if(HasPlayed(playerIndex))
	{
		m_showAvatar = true;
		SHOW_COLUMN(PLAYER_AVATAR_COLUMN, true);
		ClearWarningColumn();
	}
	else
	{
		m_showAvatar = false;
		SHOW_COLUMN(PLAYER_AVATAR_COLUMN, false);
		CPauseMenu::GetDisplayPed()->SetVisible(false);
		SetupNeverPlayedWarning(playerIndex);
	}
}

void CCrewDetailMenu::SetupNeverPlayedWarning(int playerIndex)
{
	const char* pErrorString = IsActivePlayer(playerIndex) ? "PCARD_SYNCING_STATS" : (NetworkInterface::IsNetworkOpen() ? "PCARD_NEVER_PLAYED_MP" : "PCARD_NEVER_PLAYED_SP");

	SetupNeverPlayedWarningHelper(playerIndex, pErrorString, PLAYER_AVATAR_COLUMN, 1);
}

bool CCrewDetailMenu::Populate(MenuScreenId newScreenId)
{
	MenuScreenId Override = (newScreenId == MENU_UNIQUE_ID_CREW_OPTIONS) ? MenuScreenId(GetMenuId()) : newScreenId;

	if( CPlayerListMenu::Populate(Override) )
	{
		m_crewCardCompletion.Reset();
		SPlayerCardManager::GetInstance().SetClanId(RL_INVALID_CLAN_ID);
		
		CPlayerCardManager& cardMgr = SPlayerCardManager::GetInstance();
		CClanPlayerCardDataManager* pCard = static_cast<CClanPlayerCardDataManager*>(cardMgr.GetDataManager(GetDataManagerType()));

		if( newScreenId != MENU_UNIQUE_ID_CREW_OPTIONS )
		{

			int localIndex = NetworkInterface::GetLocalGamerIndex();
			pCard->SetShouldGetOfflineMembers(true);

			if(rlClan::HasPrimaryClan(localIndex))
			{
				const rlClanDesc& rClanDesc = rlClan::GetPrimaryClan(localIndex);
				pCard->SetClan( rClanDesc.m_Id, true );// is this branch even gettoable?
				SPlayerCardManager::GetInstance().SetClanId(rClanDesc.m_Id);
			}
			else
			{
				// set the clan as invalid so it'll fail so we can catch it later in our base update
				pCard->SetClan( RL_INVALID_CLAN_ID, false );
				ShowColumnWarning_EX(PM_COLUMN_MAX, "CRW_INCREW_TEXT", "CRW_INCREW_TITLE");
			}
		}
		else
		{
			m_hasCrewInfo = true;
		}
		
		SPlayerCardManager::GetInstance().SetCrewStatus(CPlayerCardManager::CREWSTATUS_NODATA);

		m_uncorrectScreenId = newScreenId;
		m_hasClanData = m_crewMemberData.Init();
	

		// CCrewDetailMenu overwrote this function so it wouldn't do anything in CPlayerListMenu::Populate.
		CPlayerListMenu::InitDataManagerAndPaginator(newScreenId);

		// done AFTER the above so we're ready for next time we enter this screen
		m_crewMemberData.SetLastCrew(pCard->GetClan());

		return true;
	}

	return false;
}

void CCrewDetailMenu::Update()
{
	CPlayerListMenu::Update();
	if( !m_crewCardCompletion.IsDone() )
	{
		CPlayerCardManager& cardMgr = SPlayerCardManager::GetInstance();
		CClanPlayerCardDataManager* pCard = static_cast<CClanPlayerCardDataManager*>(cardMgr.GetDataManager(GetDataManagerType()));
		rlClanId relevantClan = pCard->GetClan();

		bool bShowColumn = false;
		CCrewMenu::FillOutCrewCard(bShowColumn, relevantClan, GetContextMenu()->IsOpen(), &m_crewCardCompletion);
		SHOW_COLUMN(PM_COLUMN_RIGHT, bShowColumn);
	}
}


bool CCrewDetailMenu::DoesPlayerCardNeedRefresh(int playerIndex, bool hasCrewInfo)
{
	static int pPreviousPlayerIndex = -1;

	if (pPreviousPlayerIndex != playerIndex)
	{
		m_IsFriendsWith = rlFriendsManager::IsFriendsWith(NetworkInterface::GetLocalGamerIndex(), GetGamerHandle(playerIndex));
		pPreviousPlayerIndex = playerIndex;

	}
	if( (!GetContextMenu() || !GetContextMenu()->IsOpen()) && !m_IsFriendsWith)
	{
		Color32 color;
		char crewRankTitle[RL_CLAN_NAME_MAX_CHARS] = {0};

		BasePlayerCardDataManager* pManager = GetDataMgr();

		// Has Crew stuff changed.
		if(!hasCrewInfo && pManager && pManager->IsValid() &&
			GetCrewEmblem(playerIndex) != NULL &&
			GetCrewRankTitle(playerIndex, crewRankTitle, RL_CLAN_NAME_MAX_CHARS, GetCrewRank(playerIndex)) &&
			GetCrewColor(playerIndex, color))
		{
			return true;
		}
		else if(IsActivePlayer(playerIndex) && !IsLocalPlayer(playerIndex)) // If active player has newer stats.
		{
			CNetObjPlayer* pObjPlayer = GetNetObjPlayer(playerIndex);
			if(pObjPlayer && pObjPlayer->GetStatVersion() != m_currentStatVersion)
			{
				return true;
			}
		}
	}

	return false;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
CScriptedPlayersMenu::CScriptedPlayersMenu(CMenuScreen& owner, bool popRefreshOnUpdate)
: CMPPlayerListMenu(owner)
, m_popRefreshOnUpdate(popRefreshOnUpdate)
, m_popRefreshTimer(PLAYER_CARD_FRAMES_TO_UPDATE_SCRIPTED_PLAYERS)
, m_entryDelay(0)
{
	for (int i = 0; i < MAX_NUM_ACTIVE_PLAYERS; i++)
	{
		GamerData rData;
		m_gamerDatas.PushAndGrow(rData);
	}
}

bool CScriptedPlayersMenu::SetPlayerDataSlot(int playerIndex, int slotIndex, bool isUpdate)
{
	bool wasSlotSet = false;
	
	BasePlayerCardDataManager* pManager = GetDataMgr();
	if(pManager && uiVerifyf(0 <= playerIndex && playerIndex < pManager->GetReadGamerCount(), "CPlayerListMenu::SetPlayerDataSlot - playerIndex=%d, gamerCount=%d", playerIndex, pManager->GetReadGamerCount()))
	{
		rlGamerHandle gamerHandle = pManager->GetGamerHandle(playerIndex);
		const GamerData& rPlayerCardGamerData = pManager->GetGamerData(playerIndex);
		CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(gamerHandle);
		GamerData& rGamerData = m_gamerDatas[playerIndex];

		GetDataMgr()->CommonRefreshGamerData(rGamerData, gamerHandle);

		PlayerDataSlot playerSlot;
		playerSlot.m_activeState = rGamerData.m_isOnline ? PlayerDataSlot::AS_ONLINE : PlayerDataSlot::AS_OFFLINE;
		playerSlot.m_onlineIcon = 0;
		playerSlot.m_headsetIcon = 0;
		playerSlot.m_rank = 0;
		playerSlot.m_isHost = rGamerData.m_isHost;
		playerSlot.m_color = GetPlayerColor(playerIndex, slotIndex);
		playerSlot.m_nameHash = rPlayerCardGamerData.m_nameHash;
		#if RSG_PC
		playerSlot.m_usingKeyboard = MiscData().m_usingKeyboard;
		#endif

		const rlClanMembershipData* pClanData = GetClanData(gamerHandle);
		if(pClanData && pClanData->IsValid())
		{
			#if RSG_DURANGO || RSG_XENON || RSG_PC
			// Xbox TRC TCR 094 - use every gamer index
			if( CLiveManager::CheckUserContentPrivileges(GAMER_INDEX_EVERYONE))
			#elif RSG_ORBIS || RSG_PS3
			// Playstation R4063 - use the local gamer index
			if( CLiveManager::CheckUserContentPrivileges(GAMER_INDEX_LOCAL))
			#endif
			{
				NetworkClan::GetUIFormattedClanTag(pClanData, playerSlot.m_crewTag, COUNTOF(playerSlot.m_crewTag));
			}
		}

		if(rGamerData.m_isInSameTitleOnline)
		{
			if(CPauseMenu::IsSP())
			{
				playerSlot.m_onlineIcon = GetMiscData().m_playingGTAMPIcon;
			}
			else
			{
				if(!IsSyncingStats(playerIndex, false))
				{
					int characterSlot = GetCharacterSlot(playerIndex);
					const PlayerCard& rCard = GetCardType(playerIndex, characterSlot);
					playerSlot.m_rank = GetIntStat(playerIndex, rCard.m_rankStat.GetStat(characterSlot), 0);
				}

				playerSlot.m_onlineIcon = GetMiscData().m_playingGTAMPWithRankIcon;
			}
			playerSlot.m_activeState = PlayerDataSlot::AS_ONLINE_IN_GTA;
		}
		else if(rGamerData.m_isInSameTitle)
		{
			playerSlot.m_onlineIcon = GetMiscData().m_playingGTASPIcon;
			playerSlot.m_activeState = PlayerDataSlot::AS_ONLINE_IN_GTA;
		}

		CNetworkVoice& voice = NetworkInterface::GetVoice();

		if(pPlayer)
		{
			int mpBitset = 0;

			playerSlot.m_activeState = PlayerDataSlot::AS_ONLINE_IN_GTA;

			if(fwDecoratorExtension::Get(pPlayer->GetPlayerInfo(), GetMiscData().m_bitTag, mpBitset) && mpBitset)
			{
				if(mpBitset & GetMiscData().m_bountyBit)
				{
					playerSlot.m_onlineIcon = GetMiscData().m_bountyIcon;
				}

				if(mpBitset & GetMiscData().m_spectatingBit)
				{
					playerSlot.m_onlineIcon = GetMiscData().m_spectatingIcon;
				}
			}

            if(voice.GamerHasHeadset(gamerHandle) && (IsLocalPlayer(gamerHandle) || voice.CanCommunicateVoiceWithGamer(gamerHandle)))
			{
				const rlGamerId& rGamerId = pPlayer->GetRlGamerId();

				if(voice.IsGamerTalking(rGamerId))
				{
					playerSlot.m_headsetIcon = GetMiscData().m_activeHeadsetIcon;
				}

//				playerSlot.m_headsetIcon = voice.IsGamerTalking(rGamerId) ? GetMiscData().m_activeHeadsetIcon : GetMiscData().m_inactiveHeadsetIcon;
			}

			playerSlot.m_kickCount = pPlayer->GetNumKickVotes();

#if RSG_NP
			if(g_rlNp.GetChatRestrictionFlag(ORBIS_ONLY(playerIndex)))
				playerSlot.m_headsetIcon = 0;
#endif
		}
		
		if(voice.IsGamerMutedByMe(gamerHandle))
		{
			playerSlot.m_headsetIcon = GetMiscData().m_mutedHeadsetIcon;
		}
			

		if(pPlayer && pPlayer->IsPhysical() && NetworkInterface::IsGameInProgress() && NetworkInterface::IsSpectatingLocalPlayer(pPlayer->GetPhysicalPlayerIndex()))
		{
			// Do nothing.
		}
		else if(rGamerData.m_inviteBlockedLabelHash)
		{
			playerSlot.m_pInviteState = rGamerData.m_inviteBlockedLabelHash;
			playerSlot.m_inviteColor = HUD_COLOUR_RED;
		}
		else if(rGamerData.m_isBusy)
		{
			playerSlot.m_pInviteState = ATSTRINGHASH("FM_PLY_BUSY", 0xB90FE62E);
			playerSlot.m_inviteColor = HUD_COLOUR_RED;
		}
		else if(rGamerData.m_hasBeenInvited || rGamerData.m_isWaitingForInviteAck || rGamerData.m_isOnCall )
		{
			if(rGamerData.m_hasFullyJoined)
			{
				playerSlot.m_pInviteState = ATSTRINGHASH("FM_PLY_JOINED",0xc5c40e3a);
				playerSlot.m_inviteColor = HUD_COLOUR_GREEN;
			}
			else if (rGamerData.m_isOnCall)
			{
				playerSlot.m_pInviteState = ATSTRINGHASH("FM_PLY_ON_CALL",0xEE06BF80);
				playerSlot.m_inviteColor = HUD_COLOUR_ORANGE;
			}
			else if(rGamerData.m_isInTransition)
			{
				playerSlot.m_pInviteState = ATSTRINGHASH("FM_PLY_JOINING",0xa8349664);
				playerSlot.m_inviteColor = HUD_COLOUR_RED;
			}
			else if(rGamerData.m_hasBeenInvited)
			{
				playerSlot.m_pInviteState = ATSTRINGHASH("FM_PLY_INVITED",0x1d6762b6);
				playerSlot.m_inviteColor = HUD_COLOUR_RED;
			}
			else if(rGamerData.m_isWaitingForInviteAck)
			{
				playerSlot.m_pInviteState = ATSTRINGHASH("FM_PLY_INVITING",0x7cde8fba);
				playerSlot.m_inviteColor = HUD_COLOUR_RED;
			}
			
		}

		playerSlot.m_pText = pManager->GetName(playerIndex);

		if (!playerSlot.m_pText)
		{
			playercardAssertf(0, "m_pText is null!");
			playercardDebugf3("[UI] EMPTY NAME! Player Index: %d is empty.   playerSlot.m_pText = %s", playerIndex, playerSlot.m_pText);
		}

		// If the data changed, or is being initialized, then set the slot.
		if(playerSlot != m_previousPlayerDataSlots[slotIndex] || !isUpdate)
		{
 			m_previousPlayerDataSlots[slotIndex] = playerSlot;

			AddOrUpdateSlot( slotIndex, playerIndex, isUpdate, playerSlot);
			wasSlotSet = true;

		}
	}


	return wasSlotSet;
}

bool CScriptedPlayersMenu::Populate(MenuScreenId newScreenId)
{
	playerlistDebugf1("[CScriptedPlayersMenu] CScriptedPlayersMenu::Populate");
	if(CMPPlayerListMenu::Populate(newScreenId))
	{
		SetIsInPlayerList(true);
		SPauseMenuScriptStruct newArgs(kFill, GetMenuId(), -1,-1);
		LaunchScript(newArgs);

		m_popRefreshTimer = PLAYER_CARD_FRAMES_TO_UPDATE_SCRIPTED_PLAYERS;

		BasePlayerCardDataManager* pManager = GetDataMgr();
		if(pManager && (pManager->AreGamerTagsPopulated() || pManager->HadError()))
		{
			m_entryDelay = PLAYER_CARD_SCRIPTED_ENTRY_DISPLAY_DELAY;
		}
		else
		{
			m_entryDelay = 0;
		}

		return true;
	}

	return false;
}

void CScriptedPlayersMenu::Update()
{
	if(m_popRefreshOnUpdate && --m_popRefreshTimer <= 0)
	{
		m_popRefreshTimer = PLAYER_CARD_FRAMES_TO_UPDATE_SCRIPTED_PLAYERS;
		SPauseMenuScriptStruct newArgs(kFill, GetMenuId(), -1,-1);
		LaunchScript(newArgs);
	}

	if(m_entryDelay)
	{
		--m_entryDelay;
	}

	CMPPlayerListMenu::Update();
}

void CScriptedPlayersMenu::HandleNoPlayers()
{
	playerlistDebugf1("[CScriptedPlayersMenu] CScriptedPlayersMenu::HandleNoPlayers");

	SET_DATA_SLOT_EMPTY(PLAYER_LIST_COLUMN);
	SET_DATA_SLOT_EMPTY(PLAYER_CARD_COLUMN);
	SET_DATA_SLOT_EMPTY(PLAYER_CARD_LOCAL_PLAYER_COMP_COLUMN);
	SET_DATA_SLOT_EMPTY(PLAYER_CARD_OTHER_PLAYER_COMP_COLUMN);

	CPauseMenu::GetDisplayPed()->SetVisible(false);

	CScaleformMenuHelper::SET_BUSY_SPINNER(PLAYER_LIST_COLUMN, false);
	ClearAvatarSpinner();

	ShowColumnWarning_EX(NoPlayersColumns(), GetNoResultsText(), GetNoResultsTitle());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
CCoronaJoinedPlayersMenu::CCoronaJoinedPlayersMenu(CMenuScreen& owner)
	: CScriptedPlayersMenu(owner, true)
{
}


bool CCoronaJoinedPlayersMenu::UpdateInput(s32 sInput)
{
	if(sInput == PAD_DPADLEFT
		|| sInput == PAD_DPADRIGHT
		|| sInput == PAD_LEFTSHOULDER1
		|| sInput == PAD_RIGHTSHOULDER1
		)
	{
		return true;
	}

	bool bInputHandled = CPlayerListMenu::UpdateInput(sInput);

	if (!bInputHandled)
	{
		if (sInput == PAD_SELECT && m_isPopulated)
		{
			DataIndex index = ((Paginator*)GetPaginator())->GetCurrentHighlight();
			CLiveManager::ShowGamerProfileUi(GetGamerHandle(index));

			return true;
		}
	}

	return bInputHandled;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
CCoronaFriendsInviteMenu::CCoronaFriendsInviteMenu(CMenuScreen& owner)
: CFriendsMenuMP(owner)
{
	m_listRules.m_hidePlayersInTutorial = true;
}

bool CCoronaFriendsInviteMenu::UpdateInputForInviteMenus(s32 sInput)
{
	bool bInputHandled = CPlayerListMenu::UpdateInputForInviteMenus(sInput);
	if (!bInputHandled)
	{
		const unsigned numFriendsTotal = rlFriendsManager::GetTotalNumFriends(NetworkInterface::GetLocalGamerIndex());
		if (sInput == PAD_SELECT && m_isPopulated)
		{
			if (numFriendsTotal > 0)
			{
				OnPlayerCardInput();
			}
			return true;
		}
#if RSG_PC
		else if (sInput == PAD_CROSS && numFriendsTotal == 0) // only display this with input
		{
			g_SystemUi.ShowFriendSearchUi(NetworkInterface::GetLocalGamerIndex());
			return true;
		}
#endif // RSG_PC
	}

	return bInputHandled;
}

void CCoronaFriendsInviteMenu::OnPlayerCardInput()
{
	DataIndex index = ((Paginator*)GetPaginator())->GetCurrentHighlight();
	CLiveManager::ShowGamerProfileUi(GetGamerHandle(index));
}

bool CCoronaFriendsInviteMenu::Populate(MenuScreenId newScreenId)
{
	
	if( !rlFriendsManager::GetTotalNumFriends(NetworkInterface::GetLocalGamerIndex() ) )
	{
		SUIContexts::Activate("NO_RESULTS");
	}

	if(CFriendsMenuMP::Populate(newScreenId))
	{
		SetIsInPlayerList(true);
		return true;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
CCoronaPlayersInviteMenu::CCoronaPlayersInviteMenu(CMenuScreen& owner)
: CPlayersMenu(owner)
, m_popRefreshTimer(0)
{
	m_listRules.m_hideLocalPlayer = true;
	m_listRules.m_hidePlayersInTutorial = false;
}

bool CCoronaPlayersInviteMenu::Populate(MenuScreenId newScreenId)
{
	if(CPlayersMenu::Populate(newScreenId))
	{
		SetIsInPlayerList(true);
		SPauseMenuScriptStruct newArgs(kFill, GetMenuId(), -1,-1);

		LaunchScript(newArgs);
		return true;
	}

	return false;
}

void CCoronaPlayersInviteMenu::Update()
{
	if(--m_popRefreshTimer <= 0)
	{
		m_popRefreshTimer = PLAYER_CARD_FRAMES_TO_UPDATE_SCRIPTED_PLAYERS;
		SPauseMenuScriptStruct newArgs(kFill, GetMenuId(), -1,-1);
		LaunchScript(newArgs);
	}

	CPlayersMenu::Update();
}

bool CCoronaPlayersInviteMenu::UpdateInputForInviteMenus(s32 sInput)
{
	bool bInputHandled = CPlayerListMenu::UpdateInputForInviteMenus(sInput);
	if (!bInputHandled)
	{
		if (sInput == PAD_SELECT && m_isPopulated)
		{
			DataIndex index = ((Paginator*)GetPaginator())->GetCurrentHighlight();
			CLiveManager::ShowGamerProfileUi(GetGamerHandle(index));

			return true;
		}
	}

	return bInputHandled;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
CCoronaCrewsInviteMenu::CCoronaCrewsInviteMenu(CMenuScreen& owner)
: CMPPlayerListMenu(owner)
{
	m_listRules.m_hideLocalPlayer = true;
	m_listRules.m_hidePlayersInTutorial = true;
}

bool CCoronaCrewsInviteMenu::UpdateInputForInviteMenus(s32 sInput)
{
	bool bInputHandled = CPlayerListMenu::UpdateInputForInviteMenus(sInput);
	if (!bInputHandled)
	{
		if (sInput == PAD_SELECT && m_isPopulated)
		{
			DataIndex index = ((Paginator*)GetPaginator())->GetCurrentHighlight();
			CLiveManager::ShowGamerProfileUi(GetGamerHandle(index));

			return true;
		}
	}

	return bInputHandled;
}

bool CCoronaCrewsInviteMenu::Populate(MenuScreenId newScreenId)
{
	MenuScreenId Override = (newScreenId == MENU_UNIQUE_ID_CREW_OPTIONS) ? MenuScreenId(GetMenuId()) : newScreenId;
	playerlistDebugf1("[CCoronaCrewsInviteMenu] CCoronaCrewsInviteMenu::Populate");

	if( CPlayerListMenu::Populate(Override) )
	{
		m_crewCardCompletion.Reset();

		if( newScreenId != MENU_UNIQUE_ID_CREW_OPTIONS )
		{
			CPlayerCardManager& cardMgr = SPlayerCardManager::GetInstance();

			int localIndex = NetworkInterface::GetLocalGamerIndex();
			CClanPlayerCardDataManager* pCard = static_cast<CClanPlayerCardDataManager*>(cardMgr.GetDataManager(GetDataManagerType()));
			pCard->SetShouldGetOfflineMembers(false);

			if(rlClan::HasPrimaryClan(localIndex))
			{
				const rlClanDesc& rClanDesc = rlClan::GetPrimaryClan(localIndex);
				pCard->SetClan( rClanDesc.m_Id, true );
			}
			else
			{
				// set the clan as invalid so it'll fail so we can catch it later in our base update
				pCard->SetClan( RL_INVALID_CLAN_ID, false );
				ShowColumnWarning_EX(PM_COLUMN_MAX, "CRW_INCREW_TEXT", "CRW_INCREW_TITLE");
			}
		}

		// CCrewDetailMenu overwrote this function so it wouldn't do anything in CPlayerListMenu::Populate.
		CPlayerListMenu::InitDataManagerAndPaginator(newScreenId);


		SetIsInPlayerList(true);
		return true;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
CCoronaLastJobInviteMenu::CCoronaLastJobInviteMenu(CMenuScreen& owner)
	: CScriptedPlayersMenu(owner, false)
{
}


bool CCoronaLastJobInviteMenu::UpdateInputForInviteMenus(s32 sInput)
{
	bool bInputHandled = CPlayerListMenu::UpdateInputForInviteMenus(sInput);
	if (!bInputHandled)
	{
		if (sInput == PAD_SELECT && m_isPopulated)
		{
			DataIndex index = ((Paginator*)GetPaginator())->GetCurrentHighlight();
			CLiveManager::ShowGamerProfileUi(GetGamerHandle(index));

			return true;
		}
	}

	return bInputHandled;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
CCoronaMatchedPlayersInviteMenu::CCoronaMatchedPlayersInviteMenu(CMenuScreen& owner)
	: CScriptedPlayersMenu(owner, true)
{
}


bool CCoronaMatchedPlayersInviteMenu::UpdateInputForInviteMenus(s32 sInput)
{
	bool bInputHandled = CPlayerListMenu::UpdateInputForInviteMenus(sInput);
	if (!bInputHandled)
	{
		if (sInput == PAD_SELECT && m_isPopulated)
		{
			DataIndex index = ((Paginator*)GetPaginator())->GetCurrentHighlight();
			CLiveManager::ShowGamerProfileUi(GetGamerHandle(index));

			return true;
		}
	}

	return bInputHandled;
}

bool CCoronaMatchedPlayersInviteMenu::Populate(MenuScreenId newScreenId)
{
	playerlistDebugf1("[CCoronaMatchedPlayersInviteMenu] CCoronaMatchedPlayersInviteMenu::Populate");
	if( CScriptedPlayersMenu::Populate(newScreenId) )
	{

		return true;
	}

	return false;

}


// eof
