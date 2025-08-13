#ifndef __STORE_MAIN_SCREEN_H__
#define __STORE_MAIN_SCREEN_H__

#include "frontend/SimpleTimer.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "fwtl/regdrefs.h"


namespace rage
{
    class cCommerceItemData;
    class cCommerceProductData;
	class netLeaderboardRead;
};




class cStoreMainScreen
{
public:
    cStoreMainScreen();
    virtual ~cStoreMainScreen();

    void Init(int a_StoreMovieId, int a_ButtonMovieId);
    void Init();
    void Shutdown();

    void UpdateStoreLists();
    void Update();
    void Render();

    void CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args);

    void SetBgScrollSpeed(int speed);

	bool HasUserBeenInCheckout() { return m_HasBeenInCheckout; }

	int	 GetTimeSinceInputEnabled() { return m_TimerSinceInputEnabled.GetTimeFunc() - m_TimerSinceInputEnabled.GetTime(); }
	bool IsLoadingAssets() const { return m_CurrentState == STATE_LOADING_ASSETS; }
	bool IsInQuickCheckout() const { return m_QuickCheckout; }

    void OnMembershipGained();

private:
    
	static void OnPresenceEvent(const rlPresenceEvent* evt);

    void InitInternal();

//Update functions
    void UpdateInitialising();
    void UpdatePendingInitialData();
    void UpdateLoadingAssets();
    void UpdateInput();
    
    void UpdateLeaderBoardDataFetch();

    bool UpdateWarningScreen();
	bool UpdateNonCriticalWarningScreen();

    void UpdateContextHelp();

    void ProcessSelection();

    void TriggerHighlightChange();
    void ProcessHighlightChange();

	void DoEmptyStoreCheck();
	void DisplayNoProductsWarning();

    void UpdateCheckout();
    bool DoQuickCheckout();
    void ExitQuickCheckout();
    bool ProcessCheckoutSelected(const cCommerceProductData *pProductData);
    bool StartCheckout(const cCommerceProductData* pProductData);

    bool AreAllAssetsActive();
    void DisplayItemDetails( const cCommerceProductData* pProductData );
    void ClearItemDetails( );
    void PopulateImageRequests();
    void UpdateImageRequests();
    void ClearImageRequests();
    void PopulateItemSlotWithData( int a_Slot, const cCommerceItemData* a_ItemData, bool showImage = true );
    void ClearLoadedImages();

    void RequestScaleformMovies();

    void AddProductsPriceToCashSpentStat( const cCommerceProductData* pProductData );

	bool IsUserAllowedToPurchaseCash(const char* currentConsumableId = NULL);

	void UpdateNoContentInput();

	void UpdateTopOfScreenTextInfo();

	void UpdatePendingCash();
	void InitialiseConsumablesLevel();
	void UpdateChangeInConsumableLevelCheck();

    enum eState
    {
        STATE_INITIALISING=0,
        STATE_NEUTRAL,
        STATE_INVALID,
        STATE_PENDING_INITIAL_DATA,
        STATE_LOADING_ASSETS,
        STATE_STORE_FADE_TO_BLACK,
        STATE_IN_CHECKOUT,
        STATE_STORE_FADE_TO_TRANS,
        STATE_NUM_STATES
    };

    enum eAdScreenInputVals
    {
        DPADUP = 8,
        DPADDOWN = 9,
        DPADLEFT = 10,
        DPADRIGHT = 11
    };

    enum eSelectionTransition
    {
        CAT_TO_CAT,
        ITEM_TO_ITEM,
        CAT_TO_ITEM,
        ITEM_TO_CAT,
        NO_SELECTION_TRANSITION,
        NUM_SELECTION_TRANSITIONS
    };

    eSelectionTransition m_LastSelectionTransition;

    eState m_CurrentState;

    int m_StoreMovieId;
    int m_ButtonMovieId;
    int m_StoreBgMovieId;
    int m_BlackFadeMovieId;

    int m_CurrentlySelectedCategory;
    
    bool m_Initialised;
    bool m_PendingListSetup;

    s32 m_ColumnReturnId;
    s32 m_ItemReturnId;
    s32 m_LastColumnIndex;
    s32 m_LastItemIndex;
    
    //This is stored for use when the highlight has moved on
    s32 m_LastCenterItemIndex;

    bool m_PendingSelection;
    bool m_PendingHighlightChange;
    const cCommerceProductData *m_PendingLongDescriptionItem;
    const cCommerceProductData *m_PendingCheckoutProduct;

    atArray<int> m_ProductIndexArray;
    atArray<int> m_CategoryIndexArray;

    bool m_WasInCheckout;
    bool m_RefetchingPlatformData;

    atArray<int> m_RequestedImageIndices;
    atArray<int> m_LoadedImageIndices;
    s32 m_CurrentStartReturnId;
    s32 m_CurrentEndReturnId;
    s32 m_LastStartIndex;
    s32 m_LastEndIndex;

    bool m_bLoadingAssets;

	static bool sm_bPlayerHasJustSignedOut;

    atString m_PurchaseCheckProductID;
    atString m_ConsumableProductCheckId;
    int m_ConsumableProductCheckLevel;

    atMap<atString,int> m_NumFriendsWhoOwnProductMap;
    fwRegdRef<netLeaderboardRead> m_LeaderboardRead;
    const netLeaderboardRead *mp_CurrentLeaderboardRead;

    //The currently displayed product which needs leaderboard data
    const cCommerceProductData *mp_PendingNumFriendsWhoOwnProduct;

    //The ID for the product currently being fetched from leaderboard data (not necessarily the same as m_PendingNumFriendsWhoOwnProduct)
    atString m_CurrentLeaderboardDataFetchProductId;

	enum eStoreWarningScreen
	{
		STORE_WARNING_NONE = 0,
		STORE_WARNING_PLATFORM_OFFLINE,
		STORE_WARNING_PLATFORM_LOST,
		STORE_WARNING_ROS,
		STORE_WARNING_AGE,
		STORE_WARNING_NUM
	};

	enum eStoreNonCriticalMessageScreen
	{
		STORE_NON_CRIT_WARNING_NONE = 0,
		STORE_CANNOT_PURCHASE_CASH,
        STORE_SUBSCRIPTION_UPSELL,
		STORE_SUBSCRIPTION_WELCOME,
	};

	eStoreNonCriticalMessageScreen m_CurrentNonCriticalWarningScreen;
    eStoreNonCriticalMessageScreen m_PendingNonCriticalWarningScreen;
	eStoreWarningScreen m_CurrentDCWarningScreen;
    int m_FramesOffline;

	bool m_InNoProductsMode;

	int m_PedHeadShotHandle;
	s64 m_PendingCash;
	bool m_WasConsumableDataDirtyLastFrame;

	bool m_StoreListsSetup;
	bool m_QuickCheckout;
	bool m_HasBeenInCheckout;

	atString m_SubscriptionUpsellItemId;
	atString m_SubscriptionUpsellTargetItemId;
    bool m_PendingImageRequest;

	CSystemTimer m_TimerSinceInputEnabled;

	atArray<int> m_InitialConsumableLevels;

#if RSG_ORBIS
	bool m_shouldCommerceLogoDisplay;
	bool m_isCommerceLogoDisplaying;
#endif

	bool m_WasSignedOnlineOnEnter;
};

#endif