#ifndef __CFRIENDSMENU_H__
#define __CFRIENDSMENU_H__

// rage
#include "atl/array.h"
#include "fwnet/nettypes.h"
#include "rline/rlpresence.h"
#include "rline/presence/rlpresencequery.h"

// game
#include "Frontend/CScriptMenu.h"
#include "Frontend/ScaleformMenuHelper.h"
#include "Frontend/CCrewMenu.h"
#include "Frontend/SimpleTimer.h"
#include "Frontend/UIPaginator.h"
#include "Network/Live/FriendClanData.h"
#include "Network/Live/NetworkClan.h"
#include "Network/Live/PlayerCardDataManager.h"
#include "Text/TextFile.h"

#define PLAYER_CARD_STAT_NOT_FOUND (-1)
#define PLAYER_CARD_MAX_COLUMNS (6)

class CPlayerListMenu;
class CNetGamePlayer;
class CNetObjPlayer;

struct PlayerDataSlot
{
	enum eActiveState
	{
		AS_OFFLINE,
		AS_ONLINE,
		AS_ONLINE_IN_GTA,
		//AS_ONLINE_DIFFERENT_MATCH
	};

	#if RSG_PC
	int m_usingKeyboard;
	#endif

	int m_color;
	int m_inviteColor;
	int m_headsetIcon;
	int m_onlineIcon;
	int m_rank;
	u32 m_nameHash;
	int m_kickCount;
	int m_activeState;
	int m_crewRank;
	bool m_isHost;
	bool m_isClanPrivate;
	bool m_isClanRockstar;
	u32 m_pInviteState;
	const char* m_pText;
	char m_crewTag[NetworkClan::FORMATTED_CLAN_TAG_LEN];

	PlayerDataSlot()
	{
		Reset();
	}

	void Reset()
	{
		#if RSG_PC
		m_usingKeyboard = 0;
		#endif

		m_color = 0;
		m_inviteColor = 0;
		m_headsetIcon = 0;
		m_onlineIcon = 0;
		m_rank = 0;
		m_nameHash = 0;
		m_kickCount = 0;
		m_activeState = AS_OFFLINE;
		m_isHost = false;
		m_isClanPrivate = false;
		m_isClanRockstar = false;
		m_crewTag[0] = '\0';
		m_crewRank = 0;
		m_pText = NULL;
		m_pInviteState = 0;
	}

	bool operator==(const PlayerDataSlot& slot) const
	{
		return m_color == slot.m_color &&
			m_inviteColor == slot.m_inviteColor &&
			m_headsetIcon == slot.m_headsetIcon &&
			m_onlineIcon == slot.m_onlineIcon &&
			m_rank == slot.m_rank &&
			m_nameHash == slot.m_nameHash &&
			m_kickCount == slot.m_kickCount &&
			m_activeState == slot.m_activeState &&
			m_isHost == slot.m_isHost &&
			m_isClanPrivate == slot.m_isClanPrivate &&
			m_isClanRockstar == slot.m_isClanRockstar &&
			(strcmp(m_crewTag, slot.m_crewTag) == 0) &&
			m_crewRank == slot.m_crewRank &&
			m_pText == slot.m_pText &&
			m_pInviteState == slot.m_pInviteState
			#if RSG_PC
			&& m_usingKeyboard == slot.m_usingKeyboard
			#endif
			;
	}

	bool operator!=(const PlayerDataSlot& slot) const
	{
		return !((*this) == slot);
	}
};

class CPlayerListMenuPage : public UIDataPageBase 
{
public:
	DECLARE_UI_DATA_PAGE(CPlayerListMenuPage, 112);//16); // or whatever data size you want

	CPlayerListMenuPage(const UIPaginatorBase* const pOwner, PageIndex pageIndex);
	void UpdateEntry(int iSlotIndex, DataIndex playerIndex);

	// whatever information each row needs to collect its data
	bool SetData(CPlayerListMenu* pPlayerListMenu);

	// see UIDataPageBase for more information on these
	virtual bool IsReady() const;
	virtual int GetSize() const;
	virtual int GetTotalSize() const;

	virtual bool FillScaleformEntry( LocalDataIndex iItemIndex, int iUIIndex, MenuScreenId menuId, DataIndex playerIndex, int iColumn, bool bIsUpdate );
	virtual bool FillEmptyScaleformSlots( LocalDataIndex iItemIndex, int iSlotIndex, MenuScreenId menuId, DataIndex playerIndex, int iColumn );

	virtual void SetSelectedItem(int iColumn, int iSlotIndex, DataIndex playerIndex);
	virtual bool CanHighlightSlot() const;

private:
	bool m_isPopulated;
	CPlayerListMenu* m_pPlayerListMenu;
};

class CBaseMenuPaginator : public UIPaginator<CPlayerListMenuPage>
{
public:
	CBaseMenuPaginator(const CMenuScreen* const pOwner): UIPaginator<CPlayerListMenuPage>(pOwner)
	{
		m_pPlayerListMenu = NULL;
	}

	virtual bool UpdateInput(s32 eInput)		{ return UIPaginatorBase::UpdateInput(eInput); }
	virtual void Update(bool bAllowedToDraw = true) = 0;

	virtual void Init(CPlayerListMenu* pPlayerListMenu, int VisibleItemsPerPage, MenuScreenId ColumnId, int iColumnIndex) = 0;
	virtual bool SetDataOnPage(UIDataPageBase* thisPage) =0;

	virtual bool IsTransitioningPlayers() const { return false; }
	virtual void SetTransitioningPlayers(bool UNUSED_PARAM(b)) { uiAssertf(false, "CBaseMenuPaginator::SetTransitioningPlayers -- Paginator does not support this feature"); } 
	virtual int GetRawPlayerIndex() { uiAssertf(false, "CBaseMenuPaginator::GetRawPlayerIndex -- Paginator does not support this feature"); return 0; }
	virtual void SetRawPlayerIndex(unsigned UNUSED_PARAM(index)) { uiAssertf(false, "CBaseMenuPaginator::GetRawPlayerIndex -- Paginator does not support this feature"); }
	
	virtual void Shutdown()
	{
		UIPaginator<CPlayerListMenuPage>::Shutdown();
		m_pPlayerListMenu = NULL;
	}
	
protected:
	CPlayerListMenu*	m_pPlayerListMenu;

};

class CPlayerListMenuDataPaginator : public CBaseMenuPaginator
{
public:
	CPlayerListMenuDataPaginator(const CMenuScreen* const pOwner);

	virtual bool UpdateInput(s32 eInput) { return UIPaginatorBase::UpdateInput(eInput); }
	virtual void Update(bool bAllowedToDraw = true);

	// Called on creation to set your instance apart from the others
	void Init(CPlayerListMenu* pPlayerListMenu, int VisibleItemsPerPage, MenuScreenId ColumnId, int iColumnIndex);

	// imparts the UniqueDataPerColumn onto a given datapage
	virtual bool SetDataOnPage(UIDataPageBase* thisPage);
	
	void ItemUp();
	void ItemDown();

	// The size of a friend's page
	static const int FRIEND_UI_PAGE_SIZE = 16;


};


class CFriendListMenuDataPaginator : public CBaseMenuPaginator
{
public:
	CFriendListMenuDataPaginator(const CMenuScreen* const pOwner);


	virtual bool UpdateInput(s32 eInput);
	virtual void Update(bool bAllowedToDraw = true);

	// Called on creation to set your instance apart from the others
	void Init(CPlayerListMenu* pPlayerListMenu, int VisibleItemsPerPage, MenuScreenId ColumnId, int iColumnIndex);

	// imparts the UniqueDataPerColumn onto a given datapage
	virtual bool SetDataOnPage(UIDataPageBase* thisPage);
	
	void SetTransitioningPlayers(bool b) override;
	bool IsTransitioningPlayers() const override	{ return m_bTransitioningFriends; }
	int GetCurrentPlayerIndex()						{ return m_uSystemFriendIndex % FRIEND_UI_PAGE_SIZE; }
	int GetRawPlayerIndex() override				{ return m_uSystemFriendIndex; }
	void SetRawPlayerIndex(unsigned index) override	{ m_uSystemFriendIndex = index; }

	void UpdateItemSelection(DataIndex iNewIndex, bool bHasFocus);

	virtual void Shutdown()
	{
		UIPaginator<CPlayerListMenuPage>::Shutdown();
		m_pPlayerListMenu = NULL;
	}

	void PageUp();
	void PageDown();
	void ItemUp();
	void ItemDown();

	// The size of a friend's page
	static const int FRIEND_UI_PAGE_SIZE = 16;

private:
	
	// Utility methods to simplify input logic.
	bool IsInputAllowed() const;
	void TransitionToNewPage( int const startIndex );
	void RequestNewStatsBlob( int const startIndex, bool const refreshStats );

	bool ShouldSupportStatsPaging() const;
	bool ShouldSupportFriendsPaging()  const;

	bool NeedsToPageFriends( int const index ) const;
	bool NeedsToPageStats( int const index ) const;

	void MoveToIndex(s32 iIndex);
	
	u32						m_uSystemFriendIndex;
	bool					m_bTransitioningFriends;

};

class CPlayerListMenu : public CScriptMenu
{
	enum eStatMode
	{
		STATMODE_DEFAULT,
		STATMODE_SP,
		STATMODE_MP
	};

public:

	// Should match the values in MP_PLAYER_CARD.as
	enum eDescType
	{
		DESC_TYPE_TXT,
		DESC_TYPE_SC,
		DESC_TYPE_INV_SENT
	};

	CPlayerListMenu(CMenuScreen& owner, bool bDontInitPaginator = false);
	~CPlayerListMenu();

	virtual void Update();
	//virtual void Render(const PauseMenuRenderDataExtra* renderData);
	virtual bool UpdateInput(s32 sInput);
	virtual bool Populate(MenuScreenId newScreenId);
	virtual bool TriggerEvent(MenuScreenId MenuId, s32 iUniqueId);
	virtual void LayoutChanged( MenuScreenId iPreviousLayout, MenuScreenId iNewLayout, s32 iUniqueId, eLAYOUT_CHANGED_DIR eDir );
	virtual void LoseFocus();
	virtual void BuildContexts();
	virtual bool HandleContextOption(atHashWithStringBank whichOption);
	virtual void OnNavigatingContent(bool bAreWe);

	bool IsPopulated() const { return m_isPopulated; }
	void SetPagingStats()	{ m_bPaginatorPagingStats = true; }
	// Grabs the character slot stat for the given player
	static int GetCharacterSlot(int playerIndex, eStatMode mode = STATMODE_DEFAULT);

	void LoseFocusLite();
	virtual void ShutdownPaginator();

#if __BANK
	virtual void AddWidgets(bkBank* pBank);
#endif

	virtual bool ShouldPlayNavigationSound(bool navUp);
	s32 GetCurrentPlayerIndex() const {return m_currPlayerIndex;}

	void GoBack();
	void OfferAccountUpgrade();

protected:
	typedef CPlayerCardXMLDataManager::CardSlot CardSlot;
	typedef CPlayerCardXMLDataManager::PlayerCard PlayerCard;
	typedef CPlayerCardXMLDataManager::MiscData MiscData;
	typedef BasePlayerCardDataManager::GamerData GamerData;
	typedef CBaseMenuPaginator Paginator;
	friend CPlayerListMenuPage;
	friend CPlayerListMenuDataPaginator;
	friend CFriendListMenuDataPaginator;

	inline static const MiscData& GetMiscData() {return SPlayerCardManager::GetInstance().GetXMLDataManager().GetMiscData();}
	inline static const CPlayerCardXMLDataManager& GetStatListMgr() {return SPlayerCardManager::GetInstance().GetXMLDataManager();}
	inline static BasePlayerCardDataManager* GetDataMgr() {return SPlayerCardManager::GetInstance().GetDataManager();}
	inline static const atArray<int>* GetStatIds() {return SPlayerCardManager::GetInstance().GetStatIds();}
	inline static const bool GetPackedStatValue(int playerIndex, u32 statHash, int& outVal) {return SPlayerCardManager::GetInstance().GetPackedStatValue(playerIndex, statHash, outVal);}
#if !__NO_OUTPUT
	inline static const char* GetStatName(int statHash) {return SPlayerCardManager::GetInstance().GetStatName(statHash);}
#endif

    static bool ShouldCheckStatsForInviteOrJIP();

	void RefreshPlayers();
	
	virtual bool CheckManagerBufferOnPopulate()		const { return true; }
	virtual bool CheckNetworkTransitions()			const { return true; }
	virtual bool ValidateManagerForClanDataCheck()	const { return false; }

	// Error handling
	virtual bool CheckSystemError() const { return false; }
	bool HasSystemError() const;
	
	// Timeout
	virtual bool AllowTimeout()	const { return false; }
	virtual bool HasTimedOut()	const { return false; }
	virtual void HandleTimeout()	{ uiAssertf(false, "CPlayerListMenu::HandleTimeout -- Feature not supported!"); }
	virtual void ResetTimeout()		{ uiAssertf(false, "CPlayerListMenu::HandleTimeout -- Feature not supported!"); }

	virtual void HandleUIReset(const rlGamerHandle& cachedCurrGamerHandle, const BasePlayerCardDataManager& rManager);
	virtual void HandleNoPlayers();
	virtual void HandleLaunchingGame();
	virtual void HandleSystemError();
	virtual PM_COLUMNS NoPlayersColumns() const {return PM_COLUMN_MAX;}

	// Gets the correct player card to fill out.
	static const PlayerCard& GetCardType(int playerIndex, int characterSlot);
	const Paginator* GetPaginator() const {return m_pPaginator;}
	Paginator* GetPaginator() {return m_pPaginator;}
	bool IsPaginatorValid() const;

	// Fills in the names on the left of the screen, with the status icons.
	virtual bool SetPlayerDataSlot(int playerIndex, int slotIndex, bool isUpdate);

	bool AddOrUpdateSlot( int menuIndex, int playerIndex, bool isUpdate, const PlayerDataSlot& playerSlot );

	// Fills in the player card
	virtual void SetPlayerCard(int playerIndex, bool refreshAvatar = true);
	virtual bool SetupPlayerCardColumns(int playerIndex);
	virtual bool SetupPlayerCardColumn(PM_COLUMNS column, int playerIndex);
	virtual void SetupAvatarColumn(int playerIndex, const char* pPreviousStatus = NULL);
	virtual void SetupCardTitle(PM_COLUMNS column, int playerIndex, int characterSlot, const PlayerCard& rCard);
	virtual bool SetCardDataSlot(PM_COLUMNS column, int playerIndex, int characterSlot, int slot, const PlayerCard& rCard, bool bCallEndMethod = true);
	virtual void SetupHelpSlot(int UNUSED_PARAM(playerIndex)) {}
	virtual void SetupAvatar(int playerIndex);
	void LaunchAvatarScript();
	void LaunchUpdateInputScript(s32 sInput);

	void SetPlayerCardForInviteMenu(int playerIndex, bool refreshAvatar = true);

	virtual bool DoesPlayerCardNeedRefresh(int playerIndex, bool hasCrewInfo);

	// Gets the string from the text file given the hash of a string label
	static const char* GetString(int hash); 

	static bool GetBoolStat(int playerIndex, int statHash, int defaultValue = PLAYER_CARD_STAT_NOT_FOUND);

	// Returns the stat for int values.  Returns 0 if the stat isn't an int.
	static int GetIntStat(int playerIndex, int statHash, int defaultValue = PLAYER_CARD_STAT_NOT_FOUND);
	static int GetIntStat(int playerIndex, const char* pStat, int characterSlot, eStatMode mode = STATMODE_DEFAULT) {return GetIntStat(playerIndex, CalcStatHash(pStat, characterSlot, mode));}

	static double GetStatAsFloat(int playerIndex, int statHash, double defaultVal = PLAYER_CARD_STAT_NOT_FOUND);

	// creates a hash for the stat
	static int CalcStatHash(const char* pStat);
	static int CalcStatHash(const char* pStat, int characterSlot, eStatMode mode = STATMODE_DEFAULT);

	static bool IsSP(eStatMode mode);

	// Add's the stat as a scaleform param
	static void AddStatParam(int playerIndex, int statHash);
	static void AddStatParam(int playerIndex, int characterSlot, const CardSlot& rCardSlot);

	static void AddCashParam(double cash);
	static void AddPercentParam(double percent);
	static void AddRatioStrParam(double num, double denom);

	static const rlGamerHandle& GetGamerHandle(int playerIndex);
	static ActivePlayerIndex GetActivePlayerIndex(int playerIndex);
	static ActivePlayerIndex GetActivePlayerIndex(const rlGamerHandle& gamerHandle);
	static bool IsActivePlayer(int playerIndex);
	static bool IsActivePlayer(const rlGamerHandle& gamerHandle);
	static bool IsLocalPlayer(int playerIndex);
	static bool IsLocalPlayer(const rlGamerHandle& gamerHandle);
	static const CNetGamePlayer* GetLocalPlayer();
	static CNetGamePlayer* GetNetGamePlayer(int playerIndex);
	static CNetObjPlayer* GetNetObjPlayer(int playerIndex);
	static CNetObjPlayer* GetNetObjPlayer(const rlGamerHandle& gamerHandle);

	void BuildContexts(const rlGamerHandle& gamerHandle, bool bSuppressCrewInvite = false);

	const rlClanMembershipData* GetClanData(const rlGamerHandle& gamerHandle) const;

	void InitPaginator(MenuScreenId newScreenId);

	virtual int GetMaxPlayerSlotCount() const;

	// Maintenance functions
	void Clear();
	void SetCurrentlyViewedPlayer(s32 playerIndex);

	int GetMenuId() {return GetMenuScreenId().GetValue();}
	virtual int GetListMenuId();
	virtual const char* GetNoResultsText() const { return "NORESULTS_TEXT"; }
	virtual const char* GetNoResultsTitle() const { return "NORESULTS_TITLE"; }
	virtual const char* GetFriendErrorText() const { return "NORESULTS_TEXT"; }

	void SetComparingVisible(bool visible);
	
	static void OnPresenceEvent(const rlPresenceEvent* evt);

	virtual bool IsInEntryDelay() const {return false;}
	void TriggerFullReset(u32 delayTimer) {m_fullResetDelay = MAX(1, MAX(m_fullResetDelay, delayTimer));}
	void SetReinitPaginator(bool shouldReinit) {m_shouldReinit = shouldReinit;}

	virtual void CheckListChanges() {}
	virtual void CheckFriendListChanges();
	virtual void CheckPlayerListChanges();
	bool IsInValidContextMenu();

	virtual CPlayerCardManager::CardTypes GetDataManagerType() const = 0; 
	virtual void InitDataManagerAndPaginator(MenuScreenId newScreenId);

	virtual bool IsClanDataReady() const {return m_hasClanData;}
	void CheckForClanData();
	virtual const CGroupClanData& GetGroupClanData() const;
	// non-const, non-virtual version defined in terms of the above so we don't have to rewrite it as well every time
	CGroupClanData& GetGroupClanData() { return const_cast<CGroupClanData&>( const_cast<const CPlayerListMenu*>(this)->GetGroupClanData() ); }

	bool HasPlayed(int playerIndex) const;
	virtual void SetupNeverPlayedWarning(int playerIndex);
	void SetupNeverPlayedWarningHelper(int playerIndex, const char* pMessage, PM_COLUMNS column, int numColumns);
	const char* GetName(int playerIndex) const;

	virtual bool UpdateInputForInviteMenus(s32 sInput);
	void FillOutPlayerMissionInfo();
	int GetPlayerColor(int playerIndex, int debugSlotIndexForOutput = -1);

	const char* GetCrewEmblem(int playerIndex) const {return GetCrewEmblem(GetGamerHandle(playerIndex));}
	const char* GetCrewEmblem(const rlGamerHandle& gamerHandle) const;

	bool GetCrewRankTitle(int playerIndex, char* crewRankTitle, int crewRankTextSize, int crewRank) const {return GetCrewRankTitle(GetGamerHandle(playerIndex), crewRankTitle, crewRankTextSize, crewRank);}
	bool GetCrewRankTitle(const rlGamerHandle& gamerHandle, char* crewRankTitle, int crewRankTextSize, int crewRank) const;

	bool GetCrewColor(int playerIndex, Color32& color) const {return GetCrewColor(GetGamerHandle(playerIndex), color);}
	bool GetCrewColor(const rlGamerHandle& gamerHandle, Color32& color) const;

	int GetCrewRank(int playerIndex) const;

	bool GetIsInPlayerList() const {return m_isInPlayerList;}
	void SetIsInPlayerList(bool isIn);

	bool OnInputBack();
	bool OnInputOpenContext();
	bool OnInputSC();
	bool OnInputCompare();
	bool OnInputMute();
	bool OnInputCancleInvite(s32 sInput);
	bool OnInputInvite(s32 sInput);
	bool CheckPaginatorInput(s32 sInput);

	void UpdatePedState(int playerIndex);

	void SET_DESCRIPTION(PM_COLUMNS column, const char* descStr, int descType, const char* crewTagStr);

	bool UpdateInviteTimer();
	void SetupCenterColumnWarning(const char* message, const char* title);

	void InitJIPSyncing(bool isSpectating);
	void SyncingJIPWarningCallback(CallbackData cdIsSpectating);
	void InitingInviteMsgJIPWarningCallback();
	void SetupJIPFailedWarning(bool isSpectating);
	void CancelJIP();
	void CancelJIPSessionStatusRequest();

	void CreatingPartyCallback();
	void KickPartyCallback();
	void DisplayingMessageCallback();

	void RefreshButtonPrompts();
	virtual void HandleContextMenuVisibility(bool closingContext);

	virtual bool CanInviteToCrew() const;
	virtual bool CanHandleGoToSCInput() const {return true;}
	virtual bool CanShowAvatarTitle() const {return true;}
	virtual PM_COLUMNS DownloadingMessageColumns() const {return PM_COLUMN_MIDDLE;}

	virtual bool IsSyncingStats(int playerIndex, bool updateStatVersion);

	void ClearAvatarSpinner();

	void AddEmblemRef(int& refSlot, const char* pCrewEmblemName);
	void RemoveEmblemRef(int& refSlot);

	void HighlightGamerHandle(const rlGamerHandle& gamerHandle);

private: // methods
	OUTPUT_ONLY( virtual const char* GetClassNameIdentifier() const { return "PlayerListMenu"; } )
	virtual bool ShouldFadeInPed() const;
protected:
	rlGamerHandle m_currPlayerGamerHandle;
	CrewCardStatus m_crewCardCompletion;

	bool m_wasInPlayerListUponRefresh;

	
private:
	bool m_isInPlayerList;
	
protected:
	bool m_isEmpty;
	bool m_isPopulated;
	bool m_isPopulatingStats;
	bool m_IsWaitingForPlayersRead;
	bool m_isComparing;
	bool m_isComparingVisible;
	bool m_hasHint;
	bool m_hadHint;
	bool m_showAvatar;
	bool m_hasClanData;
	bool m_hasCrewInfo;
	bool m_localPlayerHasCrewInfo;
	bool m_hasSentCrewInvite;
	bool m_hadSentCrewInvite;
	bool m_shouldReinit;
	bool m_killScriptPending;
	bool m_mgrSucceeded;
	bool m_WasSystemError;
	bool m_bPaginatorPagingStats;

	u8 m_currentStatVersion;
	s8 m_listRefreshDelay;
	s32 m_currPlayerIndex;
	s32 m_transitionIndex;
	s32 m_transitionSystemIndex;
	s32 m_playerTeamId;
	int m_emblemTxdSlot;
	int m_localEmblemTxdSlot;
	u32 m_justGainedFocus;
	u32 m_fullResetDelay;
	const char* m_pPreviousStatus;
	
	CSystemTimer m_inviteTimer;

	BasePlayerCardDataManager::ValidityRules m_listRules;

	atArray<PlayerDataSlot> m_previousPlayerDataSlots;

protected:
	Paginator* m_pPaginator;

private:

	netStatus m_jipSessionStatus;
	rlSessionQueryData m_sessionQuery;
	unsigned m_numSessionFound;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CSPPlayerListMenu : public CPlayerListMenu
{
public:
	CSPPlayerListMenu(CMenuScreen& owner, bool bDontInitPaginator = false);
	~CSPPlayerListMenu();
	virtual bool Populate(MenuScreenId newScreenId);

protected:
	virtual void SetupCardTitle(PM_COLUMNS column, int playerIndex, int characterSlot, const PlayerCard& rCard);
	virtual bool SetCardDataSlot(PM_COLUMNS column, int playerIndex, int characterSlot, int slot, const PlayerCard& rCard, bool bCallEndMethod = true);
	virtual void SetupAvatar(int playerIndex);

	virtual bool DoesPlayerCardNeedRefresh(int playerIndex, bool hasCrewInfo);

	virtual void SetupNeverPlayedWarning(int playerIndex);
	virtual void CheckListChanges() {CheckFriendListChanges();}

	virtual const char* GetNoResultsTitle() const { return "NOFRIENDS_TITLE"; }
	virtual const char* GetFriendErrorText() const { return "FRIEND_SYNC_ERROR"; }

	virtual CPlayerCardManager::CardTypes GetDataManagerType() const {return CPlayerCardManager::CARDTYPE_SP_FRIEND;}

private: // methods
	OUTPUT_ONLY( const char* GetClassNameIdentifier() const override { return "CSPlayerListMenu"; } )

private:
	bool m_isLocalPlayerOnline;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CMPPlayerListMenu : public CPlayerListMenu
{
protected:
	CMPPlayerListMenu(CMenuScreen& owner, bool bDontInitPaginator = false);
	~CMPPlayerListMenu();

	virtual void SetupHelpSlot(int playerIndex);
	virtual const char* GetFriendErrorText() const { return "FRIEND_SYNC_ERROR"; }

private: // methods
	OUTPUT_ONLY( const char* GetClassNameIdentifier() const override { return "CMPPlayerListMenu"; } )

};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CFriendsMenuMP : public CMPPlayerListMenu
{
public:
	CFriendsMenuMP(CMenuScreen& owner);
	~CFriendsMenuMP();

	void InitPaginator(MenuScreenId newScreenId);
	bool Populate(MenuScreenId newScreenId) override;

	bool UpdateInput(s32 sInput) override;
protected:

	bool CheckManagerBufferOnPopulate()		const override { return false; }
	bool CheckNetworkTransitions()			const override { return false; }
	bool ValidateManagerForClanDataCheck()	const override { return true; }
	bool CheckSystemError()					const override { return true; }

	int GetMaxPlayerSlotCount() const override;

	// Timeout
	bool AllowTimeout()	const override { return true; }
	bool HasTimedOut()	const override;
	void HandleTimeout() override;
	void ResetTimeout() override { m_uDownloadTimerMS = 0; }

	void CheckListChanges() override { CheckFriendListChanges(); }
	void HandleUIReset(const rlGamerHandle& cachedCurrGamerHandle, const BasePlayerCardDataManager& rManager) override;

	const char* GetNoResultsTitle() const override { return "NOFRIENDS_TITLE"; }

	CPlayerCardManager::CardTypes GetDataManagerType() const override { return CPlayerCardManager::CARDTYPE_MP_FRIEND;}

private: // methods
	OUTPUT_ONLY( const char* GetClassNameIdentifier() const override { return "CFriendsMenuMP"; } )
	bool ShouldFadeInPed() const override { return false; };

private:
	u32 m_uDownloadTimerMS;
};

class CFriendsMenuSP : public CSPPlayerListMenu
{
public:
	CFriendsMenuSP(CMenuScreen& owner);
	~CFriendsMenuSP();

	void InitPaginator(MenuScreenId newScreenId);
	bool Populate(MenuScreenId newScreenId) override;

	bool UpdateInput(s32 sInput) override;
protected:

	bool CheckManagerBufferOnPopulate()		const override { return false; }
	bool CheckNetworkTransitions()			const override { return false; }
	bool ValidateManagerForClanDataCheck()	const override { return true; }
	bool CheckSystemError()					const override { return true; }

	int GetMaxPlayerSlotCount() const override;

	void CheckListChanges() override { CheckFriendListChanges(); }
	void HandleUIReset(const rlGamerHandle& cachedCurrGamerHandle, const BasePlayerCardDataManager& rManager) override;

	const char* GetNoResultsTitle() const override { return "NOFRIENDS_TITLE"; }

	CPlayerCardManager::CardTypes GetDataManagerType() const override {return CPlayerCardManager::CARDTYPE_SP_FRIEND;}

private: // methods
	OUTPUT_ONLY( const char* GetClassNameIdentifier() const override { return "CFriendsMenuSP"; } )
	bool ShouldFadeInPed() const override { return false; };
};
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CPlayersMenu : public CMPPlayerListMenu
{
public:
	CPlayersMenu(CMenuScreen& owner);
	~CPlayersMenu();

	virtual bool Populate(MenuScreenId newScreenId);

protected:

	virtual bool IsSyncingStats(int playerIndex, bool updateStatVersion);

	virtual void CheckListChanges() {CheckPlayerListChanges();}

	virtual CPlayerCardManager::CardTypes GetDataManagerType() const {return CPlayerCardManager::CARDTYPE_ACTIVE_PLAYER;}

	void SetupNeverPlayedWarning(int playerIndex);
	virtual bool CanShowAvatarTitle() const {return false;}

private: // methods
	OUTPUT_ONLY( const char* GetClassNameIdentifier() const override { return "CPlayersMenu"; } )

};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CLobbyMenu : public CPlayersMenu
{
public:
	CLobbyMenu(CMenuScreen& owner);
	~CLobbyMenu();

private: // methods
	OUTPUT_ONLY( const char* GetClassNameIdentifier() const override { return "CLobbyMenu"; } )
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CPartyMenu : public CMPPlayerListMenu
{
public:
	CPartyMenu(CMenuScreen& owner);
	~CPartyMenu();

	bool Populate(MenuScreenId newScreenId);
	virtual void CheckListChanges() {CheckPlayerListChanges();}
	virtual bool UpdateInput(s32 sInput);

	virtual bool CanHandleGoToSCInput() const;

protected:

	virtual CPlayerCardManager::CardTypes GetDataManagerType() const {return CPlayerCardManager::CARDTYPE_PARTY;}
	virtual const CGroupClanData& GetGroupClanData() const {return m_partyData;}

	CPartyClanData m_partyData;

private: // methods
	OUTPUT_ONLY( const char* GetClassNameIdentifier() const override { return "CPartyMenu"; } )
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CCrewDetailMenu : public CPlayerListMenu
{
public:
	CCrewDetailMenu(CMenuScreen& owner);
	//~CCrewDetailMenu();
	virtual bool Populate(MenuScreenId newScreenId);
	virtual void Update();
	virtual bool InitScrollbar();
	virtual void BuildContexts();

protected:
	virtual void SetPlayerCard(int playerIndex, bool refreshAvatar = true);
	virtual void SetupCardTitle(PM_COLUMNS UNUSED_PARAM(column), int UNUSED_PARAM(playerIndex), int UNUSED_PARAM(characterSlot), const PlayerCard& UNUSED_PARAM(rCard)) {};
	virtual bool SetCardDataSlot(PM_COLUMNS UNUSED_PARAM(column), int UNUSED_PARAM(playerIndex), int UNUSED_PARAM(characterSlot), int UNUSED_PARAM(slot), const PlayerCard& UNUSED_PARAM(rCard), bool UNUSED_PARAM(bCallEndMethod) = true) {return false; }
	virtual atString& GetScriptPath();

	virtual bool DoesPlayerCardNeedRefresh(int playerIndex, bool hasCrewInfo);
	virtual MenuScreenId GetUncorrectedMenuScreenId() const {return m_uncorrectScreenId;}

	void SetupNeverPlayedWarning(int playerIndex);
	virtual const char* GetNoResultsText() const { return "CRW_NO_MEMBERS"; }
	virtual PM_COLUMNS NoPlayersColumns() const {return PM_COLUMN_LEFT_MIDDLE;}
	
	virtual CPlayerCardManager::CardTypes GetDataManagerType() const {return CPlayerCardManager::CARDTYPE_CLAN;}
	virtual const CGroupClanData& GetGroupClanData() const { return m_crewMemberData; }

	virtual void InitDataManagerAndPaginator(MenuScreenId UNUSED_PARAM(newScreenId)) {}

	bool		m_IsFriendsWith;
	atString m_SPPath;
	MenuScreenId m_uncorrectScreenId;
	CCrewMembersClanData m_crewMemberData;

private: // methods
	OUTPUT_ONLY( const char* GetClassNameIdentifier() const override { return "CCrewDetailMenu"; } )
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CScriptedPlayersMenu : public CMPPlayerListMenu
{
public:
	CScriptedPlayersMenu(CMenuScreen& owner, bool popRefreshOnUpdate);
	virtual bool Populate(MenuScreenId newScreenId);
	virtual void Update();
	virtual int GetListMenuId() {return MENU_UNIQUE_ID_PLAYERS_LIST;}
	virtual bool SetPlayerDataSlot(int playerIndex, int slotIndex, bool isUpdate);
	virtual GamerData& GetGamerData(int index) { return m_gamerDatas[index]; }


protected:
	void UpdateGamerData(GamerData& rData, const rlGamerHandle& rGamerHandle);
	virtual void CheckListChanges() {CheckPlayerListChanges();}
	virtual bool IsInEntryDelay() const {return m_entryDelay > 0;}
	virtual void HandleNoPlayers();
	
	atArray< GamerData >      m_gamerDatas;

	bool m_popRefreshOnUpdate;
	s8 m_popRefreshTimer;
	u8 m_entryDelay;

private: // methods
	OUTPUT_ONLY( const char* GetClassNameIdentifier() const override { return "CScriptedPlayersMenu"; } )
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CCoronaJoinedPlayersMenu : public CScriptedPlayersMenu
{
public:
	CCoronaJoinedPlayersMenu(CMenuScreen& owner);
	virtual bool UpdateInput(s32 sInput);

protected:
	virtual CPlayerCardManager::CardTypes GetDataManagerType() const {return CPlayerCardManager::CARDTYPE_JOINED;}
	virtual const char* GetNoResultsText() const { return "PCARD_NO_JOINED"; }
	virtual void SetPlayerCard(int playerIndex, bool refreshAvatar = true) {SetPlayerCardForInviteMenu(playerIndex, refreshAvatar);}
	virtual void SetupHelpSlot(int UNUSED_PARAM(playerIndex)) {}

	virtual bool CanInviteToCrew() const {return false;}
	virtual bool CanHandleGoToSCInput() const {return false;}
	virtual bool CanShowAvatarTitle() const {return false;}

private: // methods
	OUTPUT_ONLY( const char* GetClassNameIdentifier() const override { return "CCoronaJoinedPlayersMenu"; } )
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CCoronaFriendsInviteMenu : public CFriendsMenuMP
{
public:
	CCoronaFriendsInviteMenu(CMenuScreen& owner);
	virtual bool Populate(MenuScreenId newScreenId);
	virtual bool UpdateInput(s32 sInput) {return UpdateInputForInviteMenus(sInput);}
	virtual int GetListMenuId() {return MENU_UNIQUE_ID_FRIENDS_LIST;}
	virtual bool UpdateInputForInviteMenus(s32 sInput);

protected:

	void OnPlayerCardInput();
	virtual void SetPlayerCard(int playerIndex, bool refreshAvatar = true) {SetPlayerCardForInviteMenu(playerIndex, refreshAvatar);}
	virtual void SetupHelpSlot(int UNUSED_PARAM(playerIndex)) {}

	virtual bool CanInviteToCrew() const {return false;}
	virtual bool CanHandleGoToSCInput() const {return false;}

private: // methods
	OUTPUT_ONLY( const char* GetClassNameIdentifier() const override { return "CCoronaFriendsInviteMenu"; } )
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CCoronaPlayersInviteMenu : public CPlayersMenu
{
public:
	CCoronaPlayersInviteMenu(CMenuScreen& owner);
	virtual bool Populate(MenuScreenId newScreenId);
	virtual void Update();


	virtual bool UpdateInput(s32 sInput) {return UpdateInputForInviteMenus(sInput);}
	virtual bool UpdateInputForInviteMenus(s32 sInput);

	virtual int GetListMenuId() {return MENU_UNIQUE_ID_PLAYERS_LIST;}
	virtual CPlayerCardManager::CardTypes GetDataManagerType() const {return CPlayerCardManager::CARDTYPE_INVITABLE_SESSION_PLAYERS;}

protected:
	virtual const char* GetNoResultsText() const { return "PCARD_SOLO_SES"; }
	virtual void SetupHelpSlot(int UNUSED_PARAM(playerIndex)) {}

	virtual bool CanInviteToCrew() const {return false;}
	virtual bool CanHandleGoToSCInput() const {return false;}
	virtual bool CanShowAvatarTitle() const {return false;}

	s8 m_popRefreshTimer;

private: // methods
	OUTPUT_ONLY( const char* GetClassNameIdentifier() const override { return "CCoronaPlayersInviteMenu"; } )
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CCoronaCrewsInviteMenu : public CMPPlayerListMenu
{
public:
	CCoronaCrewsInviteMenu(CMenuScreen& owner);
	virtual bool Populate(MenuScreenId newScreenId);
	virtual bool UpdateInput(s32 sInput) {return UpdateInputForInviteMenus(sInput);}
	virtual bool UpdateInputForInviteMenus(s32 sInput);
	virtual int GetListMenuId() {return MENU_UNIQUE_ID_CREW_OPTIONS;}
	virtual CPlayerCardManager::CardTypes GetDataManagerType() const {return CPlayerCardManager::CARDTYPE_CLAN;}

protected:
	virtual void SetPlayerCard(int playerIndex, bool refreshAvatar = true) {SetPlayerCardForInviteMenu(playerIndex, refreshAvatar);}
	virtual void SetupHelpSlot(int UNUSED_PARAM(playerIndex)) {}

	virtual void InitDataManagerAndPaginator(MenuScreenId UNUSED_PARAM(newScreenId)) {}

	virtual bool CanInviteToCrew() const {return false;}
	virtual bool CanHandleGoToSCInput() const {return false;}
	virtual const char* GetNoResultsText() const { return "NOCREW_TEXT"; }

private: // methods
	OUTPUT_ONLY( const char* GetClassNameIdentifier() const override { return "CCoronaCrewsInviteMenu"; } )
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CCoronaLastJobInviteMenu : public CScriptedPlayersMenu
{
public:
	CCoronaLastJobInviteMenu(CMenuScreen& owner);
	virtual bool UpdateInput(s32	 sInput) {return UpdateInputForInviteMenus(sInput);}
	virtual bool UpdateInputForInviteMenus(s32 sInput);
protected:
	virtual CPlayerCardManager::CardTypes GetDataManagerType() const {return CPlayerCardManager::CARDTYPE_LAST_JOB;}
	virtual const char* GetNoResultsText() const { return "PCARD_NO_LAST_JOB"; }
	virtual void SetPlayerCard(int playerIndex, bool refreshAvatar = true) {SetPlayerCardForInviteMenu(playerIndex, refreshAvatar);}
	virtual void SetupHelpSlot(int UNUSED_PARAM(playerIndex)) {}

	virtual bool CanInviteToCrew() const {return false;}
	virtual bool CanHandleGoToSCInput() const {return false;}

private: // methods
	OUTPUT_ONLY( const char* GetClassNameIdentifier() const override { return "CCoronaLastJobInviteMenu"; } )
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CCoronaMatchedPlayersInviteMenu : public CScriptedPlayersMenu
{
public:
	CCoronaMatchedPlayersInviteMenu(CMenuScreen& owner);
	virtual bool UpdateInput(s32 sInput) {return UpdateInputForInviteMenus(sInput);}
	virtual bool UpdateInputForInviteMenus(s32 sInput);
	virtual bool Populate(MenuScreenId newScreenId);

protected:
	virtual CPlayerCardManager::CardTypes GetDataManagerType() const {return CPlayerCardManager::CARDTYPE_MATCHED;}
	virtual const char* GetNoResultsText() const { return "PCARD_NO_MATCHED"; }
	virtual void SetPlayerCard(int playerIndex, bool refreshAvatar = true) {SetPlayerCardForInviteMenu(playerIndex, refreshAvatar);}
	virtual void SetupHelpSlot(int UNUSED_PARAM(playerIndex)) {}

	virtual bool CanInviteToCrew() const {return false;}
	virtual bool CanHandleGoToSCInput() const {return false;}

private: // methods
	OUTPUT_ONLY( const char* GetClassNameIdentifier() const override { return "CCoronaMatchedPlayersInviteMenu"; } )
};

#endif // __CFRIENDSMENU_H__

