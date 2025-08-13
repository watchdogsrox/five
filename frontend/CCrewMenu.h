#ifndef __CCREWMENU_H__
#define __CCREWMENU_H__

// rage
#include "atl/string.h"
#include "atl/bitset.h"
#include "rline/clan/rlclancommon.h"


// game
#include "Network/Live/ClanNetStatusArray.h"
#include "Network/Live/NetworkClan.h"
#include "Text/TextFile.h"
#include "Frontend/UIPaginator.h"
#include "Frontend/CMenuBase.h"
#include "Frontend/WarningScreen.h"
#include "Frontend/SimpleTimer.h"

//rage
#include "rline/rlfriendsmanager.h"

// forward declarations
struct Leaderboard;
class NetworkCrewMetadata;
class CCrewMenu;

namespace rage
{
	class netLeaderboardRead;
}

#define VISIBLE_CREW_ITEMS 16

// one cached page of 112 entries (>100 friends)
#define FRIENDS_UI_PAGE_COUNT (rlFriendsPage::MAX_FRIEND_PAGE_SIZE/16)+1
#define FRIENDS_CACHED_PAGES 1 

#define CREW_UI_PAGE_COUNT 2
#define CREW_CACHED_PAGES 1

#define OPEN_CREW_UI_PAGE_COUNT 2
#define OPEN_CREW_CACHED_PAGES 3

#define REQUEST_UI_PAGE_COUNT 1
#define REQUEST_CACHED_PAGES 1

#define INVITE_UI_PAGE_COUNT 1
#define INVITE_CACHED_PAGES 1
#define UNSORTED_LUT 255
#define INVALID_LUT 254

class CrewCardStatus : public atFixedBitSet32
{
public:
	CrewCardStatus() {};
	bool IsDone() const;
	bool IsBareMinimumSet() const;
};

class UICrewLBPage : public UIDataPageBase
{
public:
	DECLARE_UI_DATA_PAGE(UICrewLBPage, VISIBLE_CREW_ITEMS * CREW_UI_PAGE_COUNT);

	UICrewLBPage(const UIPaginatorBase* const pOwner, PageIndex pageIndex) 
		: UIDataPageBase(pOwner, pageIndex)
		, m_pDynamicOwner(NULL)
	{
		m_LocalDataIndex_to_DescIndex[0] = UNSORTED_LUT;
	};

	~UICrewLBPage() { Shutdown(); }

	bool SetData(CCrewMenu* pOwner);
	const rlClanDesc& GetClanDesc(LocalDataIndex iItemIndex) const;

	virtual bool IsReady() const;
	virtual bool HasFailed() const;
	virtual int GetResultCode() const;
	virtual int GetSize() const;
	virtual int GetTotalSize() const;
	virtual void Update();
	void Shutdown();

	virtual bool FillScaleformEntry(	  LocalDataIndex iItemIndex, int iUIIndex, MenuScreenId ColumnId, DataIndex iUniqueId, int iColumn, bool bIsUpdate );
	virtual bool FillEmptyScaleformSlots( LocalDataIndex iItemIndex, int iUIIndex, MenuScreenId ColumnId, DataIndex iUniqueId, int iColumn );
	virtual void FillScaleformBase(int iColumn, MenuScreenId ColumnId, DataIndex iStartingIndex, int iMaxPerPage, bool bHasFocus);
	virtual void SetSelectedItem(int iColumn, int iSlotIndex, DataIndex playerIndex);

private:

	bool IsSCAvailable();

	fwRegdRef<class netLeaderboardRead> m_LeaderboardRead;
	ClanNetStatusArray<rlClanDesc, DATA_PAGE_SIZE> m_Descs;
	atRangeArray<u8, DATA_PAGE_SIZE>				m_LocalDataIndex_to_DescIndex; // because the net services team is a dick and returns the clan ids in a different order than what I asked for
	CCrewMenu* m_pDynamicOwner;

	static const Leaderboard* sm_pLBData;

};

class UICrewLBPaginator : public UIPaginator<UICrewLBPage>
{
public:
	UICrewLBPaginator(const CMenuScreen* const pOwner) : UIPaginator<UICrewLBPage>(pOwner) 
		, m_pDynamicOwner(NULL)
	{};

	// Called on creation to set your instance apart from the others
	void Init(MenuScreenId ColumnId, MenuScreenId DelayedOwner, datCallback showHideCB, datCallback noResultsCB, CCrewMenu* pOwner)
	{
		SetDelayedOwner( DelayedOwner );
		InitInternal(VISIBLE_CREW_ITEMS, ColumnId, PM_COLUMN_MIDDLE, CREW_CACHED_PAGES, PM_COLUMN_MIDDLE_RIGHT);

		SetShowHideCB(showHideCB);
		SetNoResultsCB(noResultsCB);
		m_pDynamicOwner = pOwner;
	}

	const rlClanDesc& GetClanDesc(DataIndex indexToGet) const;

protected:
	// imparts the UniqueDataPerColumn onto a given datapage
	virtual bool SetDataOnPage(UIDataPageBase* thisPage)
	{
		UICrewLBPage* pPage = verify_cast<UICrewLBPage*>(thisPage);
		if( pPage )
		{
			return pPage->SetData(m_pDynamicOwner);
		}
		return false;
	}
	
	CCrewMenu* m_pDynamicOwner;
};

//////////////////////////////////////////////////////////////////////////

class UIFriendsClanPage : public UIDataPageBase
{
public:
	DECLARE_UI_DATA_PAGE(UIFriendsClanPage, FRIENDS_UI_PAGE_COUNT); // nearest factor of 16

	UIFriendsClanPage(const UIPaginatorBase* const pOwner, PageIndex pageIndex) 
		: UIDataPageBase(pOwner, pageIndex)
		, m_pDynamicOwner(NULL)
	{};

	virtual bool IsReady() const;
	virtual int GetTotalSize() const;
	virtual int GetSize() const { return GetTotalSize();}

	virtual bool FillScaleformEntry(	  LocalDataIndex iItemIndex, int iUIIndex, MenuScreenId ColumnId, DataIndex iUniqueId, int iColumn, bool bIsUpdate );
	virtual bool FillEmptyScaleformSlots( LocalDataIndex iItemIndex, int iUIIndex, MenuScreenId ColumnId, DataIndex iUniqueId, int iColumn );
	virtual void SetSelectedItem(int iColumn, int iSlotIndex, DataIndex playerIndex);

	void SetData(CCrewMenu* pOwner) { m_pDynamicOwner = pOwner; }
protected:

	CCrewMenu* m_pDynamicOwner;
};

class UIFriendsClanPaginator : public UIPaginator<UIFriendsClanPage>
{
public:
	UIFriendsClanPaginator(const CMenuScreen* const pOwner) : UIPaginator<UIFriendsClanPage>(pOwner)
		, m_pDynamicOwner(NULL)
	{};

	void Init(MenuScreenId ColumnId, MenuScreenId DelayedOwner, datCallback showHideCB, datCallback noResultsCB, CCrewMenu* owner)
	{
		UIPaginatorBase::InitInternal(VISIBLE_CREW_ITEMS, ColumnId, PM_COLUMN_MIDDLE, FRIENDS_CACHED_PAGES, PM_COLUMN_MIDDLE_RIGHT);

		SetDelayedOwner(DelayedOwner);
		SetShowHideCB(showHideCB);
		SetNoResultsCB(noResultsCB);
		m_pDynamicOwner = owner;
	}

	const rlClanDesc& GetClanDesc(DataIndex indexToGet) const;

protected:

	CCrewMenu* m_pDynamicOwner;


	virtual bool SetDataOnPage(UIDataPageBase* thisPage) 
	{
		UIFriendsClanPage* pPage = verify_cast<UIFriendsClanPage*>(thisPage);
		if( pPage )
		{
			pPage->SetData(m_pDynamicOwner);
			return true;
		}
		return false;
	}
};


//////////////////////////////////////////////////////////////////////////

class UIGeneralCrewPageBase : public UIDataPageBase
{
public:
	UIGeneralCrewPageBase(const UIPaginatorBase* const pOwner, PageIndex pageIndex) 
		: UIDataPageBase(pOwner, pageIndex)
		, m_pDynamicOwner(NULL)
	{};

	// THESE NEED TO BE DEFINED
	virtual bool SetData(rlClanId PrimaryClan, CCrewMenu* pOwner) = 0;

	virtual bool IsReady() const;
	virtual void SetSelectedItem(int iColumn, int iSlotIndex, DataIndex playerIndex);
	virtual bool FillEmptyScaleformSlots( LocalDataIndex UNUSED_PARAM(iItemIndex), int iUIIndex, MenuScreenId ColumnId, DataIndex UNUSED_PARAM(iUniqueId), int iColumn );

protected:
	CCrewMenu* m_pDynamicOwner;
};

template<class dataType, int uiPageCount>
class UIGeneralCrewPage : public UIGeneralCrewPageBase
{
public:
	enum
	{
		UI_PAGE_SIZE = VISIBLE_CREW_ITEMS*uiPageCount
	};

	UIGeneralCrewPage(const UIPaginatorBase* const pOwner, PageIndex pageIndex) 
		: UIGeneralCrewPageBase(pOwner, pageIndex)
	{};

	~UIGeneralCrewPage() { Shutdown(); }

	virtual bool IsReady() const
	{ 
		if( !UIGeneralCrewPageBase::IsReady() )
			return false;

		return m_Data.m_RequestStatus.Succeeded() || HasFailed(); 
	}
	virtual bool HasFailed() const	 { return m_Data.m_RequestStatus.Failed(); }
	virtual int GetResultCode() const { return m_Data.m_RequestStatus.GetResultCode(); }
	virtual int GetSize() const		 { return IsReady() ? m_Data.m_iResultSize : 0; }
	virtual int GetTotalSize() const { return IsReady() ? m_Data.m_iTotalResultSize : 0; }

	void Shutdown()
	{
		m_Data.Clear();
	}

	// DOESN'T check for ready, so be sure to do so before you invoke it
	const dataType& GetData(LocalDataIndex index) const { return m_Data[index]; }

protected:
	typedef ClanNetStatusArray<dataType, UI_PAGE_SIZE>	aDataType;
	aDataType	m_Data;
	
};

template<class PageType, size_t _pageCount = UIPaginatorBase::DEFAULT_MAX_PAGES>
class UIGeneralCrewPaginator : public UIPaginator<PageType>
{
public:
	//	have to have an ODD page size
	CompileTimeAssert((_pageCount&1)==1);

	UIGeneralCrewPaginator(const CMenuScreen* const pOwner) : UIPaginator<PageType>(pOwner)
		, m_PrimaryClan(RL_INVALID_CLAN_ID)
		, m_pDynamicOwner(NULL)
	{};

	// Called on creation to set your instance apart from the others
	void Init(MenuScreenId ColumnId, MenuScreenId DelayedOwner, datCallback showHideCB, datCallback noResultsCB, CCrewMenu* pOwner)
	{
		UIPaginatorBase::InitInternal(VISIBLE_CREW_ITEMS, ColumnId, PM_COLUMN_MIDDLE, _pageCount, PM_COLUMN_MIDDLE_RIGHT);
		UIPaginatorBase::SetDelayedOwner( DelayedOwner );
		UIPaginatorBase::SetShowHideCB(showHideCB);
		UIPaginatorBase::SetNoResultsCB(noResultsCB);
		m_pDynamicOwner = pOwner;
	}

	void SetData(rlClanId newClanId)
	{
		if( m_PrimaryClan != newClanId )
		{
			m_PrimaryClan = newClanId;

			UIPaginatorBase::ResetPages();
		}
	}

	const rlClanDesc& GetClanDesc(DataIndex whichClan) const
	{
		// unfortunately, you may have to use template specialization because of our different types
		if( const PageType* pRightType = GetPageSafe(whichClan) )
		{
			LocalDataIndex ldi = whichClan - pRightType->GetLowestEntry();
			return pRightType->GetData( ldi ); // probably right here
		}

		return RL_INVALID_CLAN_DESC;
	}

protected:
	const PageType* GetPageSafe(DataIndex index) const
	{
		const UIDataPageBase* pPage = UIPaginatorBase::FindPageContaining(index);
		if( pPage && pPage->IsReady())
			return verify_cast<const PageType*>(pPage);

		return NULL;
	}

	PageType* GetPageSafe(DataIndex index)
	{
		return const_cast<PageType*>(const_cast<const UIGeneralCrewPaginator<PageType, _pageCount>* >(this)->GetPageSafe(index));
	}

	// imparts the UniqueDataPerColumn onto a given datapage
	virtual bool SetDataOnPage(UIDataPageBase* thisPage)
	{
		PageType* pPage = verify_cast<PageType*>(thisPage);
		if( pPage )
		{
			return pPage->SetData(m_PrimaryClan, m_pDynamicOwner);
		}
		return false;
	}

	CCrewMenu*	m_pDynamicOwner;
	rlClanId m_PrimaryClan;
};

//////////////////////////////////////////////////////////////////////////

class UIRequestPage : public UIGeneralCrewPage<rlClanRequestId,REQUEST_UI_PAGE_COUNT>
{
public:
	DECLARE_UI_DATA_PAGE(UIRequestPage, UI_PAGE_SIZE);

	UIRequestPage(const UIPaginatorBase* const pOwner, PageIndex pageIndex) 
		: UIGeneralCrewPage<rlClanRequestId,REQUEST_UI_PAGE_COUNT>(pOwner, pageIndex)
	{};

	virtual bool SetData(rlClanId PrimaryClan, CCrewMenu* pOwner);
	virtual bool FillScaleformEntry( LocalDataIndex iItemIndex, int iUIIndex, MenuScreenId ColumnId, DataIndex iUniqueId, int iColumn, bool bIsUpdate );

	atFixedBitSet<UI_PAGE_SIZE, u32>	m_Completed;
	atFixedBitSet<UI_PAGE_SIZE, u32>	m_AcceptedOrRejected;
};

class UIRequestPaginator : public UIGeneralCrewPaginator<UIRequestPage,REQUEST_CACHED_PAGES>
{
public:
	UIRequestPaginator(const CMenuScreen* const pOwner) : UIGeneralCrewPaginator<UIRequestPage,REQUEST_CACHED_PAGES>(pOwner) {};

	void HandleRequest(DataIndex entry, bool bAcceptedOrRejected);
	bool IsHandled(DataIndex entry) const;

	const rlGamerHandle&				GetGamerHandle(DataIndex entry) const;
	const rlClanJoinRequestRecieved&	GetRequest(DataIndex entry) const;
};

//////////////////////////////////////////////////////////////////////////

class UIInvitePage : public UIGeneralCrewPage<rlClanInviteId,INVITE_UI_PAGE_COUNT>
{
public:
	DECLARE_UI_DATA_PAGE(UIInvitePage, UI_PAGE_SIZE);

	UIInvitePage(const UIPaginatorBase* const pOwner, PageIndex pageIndex) 
		: UIGeneralCrewPage<rlClanInviteId,INVITE_UI_PAGE_COUNT>(pOwner,pageIndex)
	{};


	virtual bool SetData(rlClanId PrimaryClan, CCrewMenu* pOwner);
	virtual bool FillScaleformEntry( LocalDataIndex iItemIndex, int iUIIndex, MenuScreenId ColumnId, DataIndex iUniqueId, int iColumn, bool bIsUpdate );

	atFixedBitSet<UI_PAGE_SIZE, u32>	m_Completed;
	atFixedBitSet<UI_PAGE_SIZE, u32>	m_AcceptedOrRejected;
};

class UIInvitePaginator : public UIGeneralCrewPaginator<UIInvitePage,INVITE_CACHED_PAGES>
{
public:
	UIInvitePaginator(const CMenuScreen* const pOwner) : UIGeneralCrewPaginator<UIInvitePage,INVITE_CACHED_PAGES>(pOwner) {};

	const rlClanInvite* GetInvite(DataIndex thisClan);
	void HandleRequest(DataIndex entry, bool bAcceptedOrRejected);
	bool IsHandled(DataIndex entry) const;
};




//////////////////////////////////////////////////////////////////////////

class UIOpenClanPage : public UIGeneralCrewPage<rlClanDesc,OPEN_CREW_UI_PAGE_COUNT>
{
public:
	DECLARE_UI_DATA_PAGE(UIOpenClanPage, UI_PAGE_SIZE);

	UIOpenClanPage(const UIPaginatorBase* const pOwner, PageIndex pageIndex) 
		: UIGeneralCrewPage<rlClanDesc,OPEN_CREW_UI_PAGE_COUNT>(pOwner, pageIndex)
	{};

	virtual bool SetData(rlClanId PrimaryClan, CCrewMenu* pOwner);
	virtual bool FillScaleformEntry( LocalDataIndex iItemIndex, int iUIIndex, MenuScreenId ColumnId, DataIndex iUniqueId, int iColumn, bool bIsUpdate );
};

typedef UIGeneralCrewPaginator<UIOpenClanPage,OPEN_CREW_CACHED_PAGES> UIOpenClanPaginator;

//////////////////////////////////////////////////////////////////////////

class CCrewMenu : public CMenuBase
{
public:
	CCrewMenu(CMenuScreen& owner);
	~CCrewMenu();
	virtual void Update();

	

	virtual bool Populate(MenuScreenId newScreenId);
	virtual void LayoutChanged( MenuScreenId iPreviousLayout, MenuScreenId iNewLayout, s32 iUniqueId, eLAYOUT_CHANGED_DIR UNUSED_PARAM(eDir) );
	virtual void PrepareInstructionalButtons(MenuScreenId MenuId, s32 iUniqueId);
	virtual bool InitScrollbar();
	virtual bool ShouldBlockEntrance( MenuScreenId iMenuScreenBeingFilledOut);
	virtual bool ShouldPlayNavigationSound(bool bNavUp);

	virtual void LoseFocus();
	virtual void Render(const PauseMenuRenderDataExtra* renderData);
	virtual bool UpdateInput(s32 eInput);
	virtual bool TriggerEvent(MenuScreenId MenuId, s32 iUniqueId );

	virtual bool HandleContextOption(atHashWithStringBank whichOption);
	virtual void BuildContexts();
	virtual CContextMenu* GetContextMenu();
	virtual const CContextMenu* GetContextMenu() const;

	virtual bool IsShowingWarningColumn() const		{return CMenuBase::IsShowingWarningColumn()		|| (m_pCrewDetails && m_pCrewDetails->IsShowingWarningColumn());}
	virtual bool IsShowingFullWarningColumn() const {return CMenuBase::IsShowingFullWarningColumn() || (m_pCrewDetails && m_pCrewDetails->IsShowingFullWarningColumn());}

	bool CheckNetworkLoss();
	void HandleNetworkLoss();
	void LeaveCrewDetails();

#if __BANK
	virtual void AddWidgets(bkBank* ToThisBank);
	static bool sm_bDbgAppendNumbers;
#endif

protected:
	typedef ClanNetStatusArray<rlClanMembershipData, 5>			aMyMemberships;


	// definitions and enums
	enum Variant
	{
		CrewVariant_Mine,
		CrewVariant_Friends,
		CrewVariant_Invites,
		// note that anything fetched on startup has to be adjacent
		CrewVariant_OpenClans,
		CrewVariant_Requests,
		CrewVariant_Leaderboards,
		
		CrewVariant_MAX
	};

	enum State
	{
		CrewState_MembershipFetch,
		CrewState_WaitingForAllData,
		CrewState_WaitingForMembership,
		CrewState_WaitingForClanData,
		CrewState_Done,
		// Any states after Done count as special, 'done-like' states.
		CrewState_NeedToJoinSC,
		CrewState_InsideCrewMenu,
		CrewState_ErrorFetching,
		CrewState_ErrorOffline,
		CrewState_ErrorNoCloud,
		CrewState_Refreshing,
		CrewState_NoPrivileges,
		CrewState_NoSubscription
	};

	enum ColumnMode
	{
		Col_SingleCard,
		Col_Compare,
		Col_LBStats,
		Col_MAX_MODES
	};


	// Maintenance functions
public:
	bool ShouldShowCrewCards() const { return m_eRightColumnMode != Col_LBStats; }
	bool IsReady() const;
protected:
	void UpdateColumnModeContexts(ColumnMode newMode = Col_MAX_MODES);
	void Clear(bool bDontClearEverything=false); // have this rubbish backwards thing to handle callbacks
	void ReinitPaginators();
	void DoDataFixup();

	void UpdateItemCount(MenuScreenId itemToFind, u32 iCount);
	bool HandleCrewCard();
	void SetStates(bool bBaseVis, bool& bMainCardVis, bool& bLeftCompare, bool& bRightCompare, bool& bLBRanks);
	void SetThirdColumnVisibleOnly(bool bVisibility);
	void SetThirdColumnVisible(bool bVisibility);
	virtual void HandleContextMenuVisibility(bool bVisibility);
	void ShowNoResultsCB(bool bVisibility);

	// Scaleform Interaction
	static void AddCrewWidget(int iIndex, const char* pszHeader, const char* pszData = NULL, bool bIsUpdate = false, PM_COLUMNS whichColumn = PM_COLUMN_RIGHT);
	static void AddCrewWidget(int iIndex, const char* pszHeader, s64 iValue, bool bIsUpdate = false, PM_COLUMNS whichColumn = PM_COLUMN_RIGHT, eOPTION_DISPLAY_STYLE type = OPTION_DISPLAY_NONE);
	static void AddCrewWidget(int iIndex, const char* pszHeader, int R, int G, int B, bool bIsUpdate = false, PM_COLUMNS whichColumn = PM_COLUMN_RIGHT );
	static void AddCrewStat(  int iIndex, const char* pszHeader, int iIcon, int iValue, bool bIsUpdate, PM_COLUMNS whichColumn = PM_COLUMN_RIGHT);
	static void SET_DESCRIPTION(const char* pszDescription, int FriendCount, bool bIsPrivate, bool bIsRockstar, const char* pClanTag, PM_COLUMNS whichColumn = PM_COLUMN_RIGHT);


public:
	// Convenience wrappers
	bool AmIInThisClan(rlClanId clanId, bool* out_pbIsPrimary = NULL);
	static bool FillOutCrewCard(bool& bShowColumn, const rlClanId thisClan, bool bIsContextMenuOpen, CrewCardStatus* pOutStatus = NULL, PM_COLUMNS whichColumn = PM_COLUMN_RIGHT, bool bIsCompareCard = false, bool bSoonEnough = true);
	void SetCrewIndexAndPrepareCard(int iNewIndex, bool bUpdatePaginator = true);
protected:

	bool IsSCAvailable(int gamerIndex, bool bConnectionToo = true) const;
	bool IsCloudAvailable() const;

	int	 GetResultsForCurrentVariant();
	netStatus::StatusCode CheckSuccess(const netStatus& thisStatus, const char* pszTypeofCheck) const;
	netStatus::StatusCode CheckSuccessForCurrentVariant(const char* pszTypeofCheck) const;
	const netStatus& GetStatusForCurrentVariant() const { return GetStatusForVariant(m_eVariant); }
	const netStatus& GetStatusForVariant(Variant whichVariant) const;
	const rlClanId GetClanIdForCurrentVariant(int index) const;
	const rlClanDesc& GetClanDescForCurrentVariant(int index) const;
	UIContext GetVariantName(Variant forThis) const;
	const char* GetNoResultsString(Variant forThis) const;
	Variant GetVariantForMenuScreen(MenuScreenId id) const;
	MenuScreenId GetMenuScreenForVariant(Variant id) const;

	void PopulateMenuForCurrentVariant();
	void PopulateMenuForJoinSC();
	void PopulateMenuForError(Variant forThis);
	void PopulateWithMessage(const char* pError, const char* pTitle, bool bFullBlockage = true);
	void UnPopulateMessage();

	void JoinSuccessMakePrimaryCB();
	void JoinSelectedClanCB();
	void RequestSelectedClanCB();
	void SetPrimaryClanCB();
	void LeaveSelectedClanCB();

	void SetVariant( Variant newVariant );

	bool SetColumnBusy( int whichColumn, bool bForceIt = false );
	void GoBack();
	void OfferAccountUpgrade();

	void OnEvent_NetworkClan(const NetworkClan::NetworkClanEvent& evt);
	void OnEvent_rlClan(	 const rlClanEvent& evt);

protected:
	
	void ClearCrewDetails();
	void EnterViewMembers(const rlClanId& clanId);

	static const int FETCHED_ACTIONS = 1;

	// Data members
	UICrewLBPaginator	m_LBPaginator;
	UIRequestPaginator	m_RequestPaginator;
	UIOpenClanPaginator m_OpenClanPaginator;
	UIInvitePaginator	m_InvitePaginator;
	UIFriendsClanPaginator m_FriendsPaginator;
	aMyMemberships		m_MyMemberships;
	rlClanDesc			m_CachedDesc;


	atRangeArray<UIPaginatorBase*, CrewVariant_MAX>	m_Paginators;
	CSystemTimer		m_CardTimeout;
	CSystemTimer		m_ClearTimeout;
	CSystemTimer		m_EventCooldown;

	NetworkClan::Delegate	m_NetworkClanDelegate;
	rlClan::Delegate		m_rlClanDelegate;

	CrewCardStatus		m_curSlotBits;
	CrewCardStatus		m_compareSlotBits;

	rlClanId			m_myPrimaryClan;
	CMenuBase*			m_pCrewDetails;

	int					m_myPrimaryClanIndex;
	int					m_LastBusyColumn;
	DataIndex			m_iSelectedCrewIndex;
	ColumnMode			m_eRightColumnMode;
	
	Variant				m_eVariant;
	State				m_eState;
	
	bool				m_bInSubMenu;
	bool				m_bIgnoreNextLayoutChangedBecauseItIsShit;
	bool				m_bLastFetchHadErrors;
	bool				m_bVisibilityControlledByWarningScreen;

#if __BANK
	bkGroup*			m_pDebugWidget;
#endif
};



#endif // __CCREWMENU_H__

