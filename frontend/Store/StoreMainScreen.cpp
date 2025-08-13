#include "Frontend/Store/StoreMainScreen.h"

#include "frontend/hud_colour.h"
#include "frontend/PauseMenu.h"
#include "frontend/BusySpinner.h"
#include "Frontend/FrontendStatsMgr.h"
#include "frontend/HudTools.h"
#include "frontend/NewHud.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "Frontend/Store/StoreScreenMgr.h"
#include "Frontend/Store/StoreUIChannel.h"
#include "Frontend/Store/StoreTextureManager.h"
#include "FrontEnd/WarningScreen.h"
#include "game/Clock.h"
#include "Network/Cloud/Tunables.h"
#include "Network/Live/livemanager.h"
#include "Network/Live/NetworkTransactionTelemetry.h"
#include "Network/Live/NetworkTelemetry.h"
#include "Network/commerce/CommerceManager.h"
#include "Network/Stats//NetworkLeaderboardSessionMgr.h"
#include "Peds/rendering/PedHeadshotManager.h"
#include "SaveLoad/savegame_users_and_devices.h"
#include "scene/ExtraContent.h"
#include "stats/StatsInterface.h"
#include "system/controlMgr.h"
#include "system/pad.h"
#include "Stats/MoneyInterface.h"
#include "text/TextConversion.h"

#include "fwcommerce/CommerceUtil.h"
#include "fwcommerce/CommerceChannel.h"
#include "fwcommerce/CommerceConsumable.h"
#include "fwnet/netscgameconfigparser.h"
#include "rline/rlgamerinfo.h"
#include "rline/rlpresence.h"
#include "rline/scmembership/rlscmembership.h"

#if RSG_PC
#include "rline/rlpc.h"
#endif // RSG_PC

FRONTEND_STORE_OPTIMISATIONS()

PARAM(commerceAlwaysShowUpsell, "[commerce] For products with an upsell, always run the upsell flow");

#define STOREMENU_FILENAME	"pause_menu_store_content"  
#define BUTTONMENU_FILENAME "store_instructional_buttons"
const char* BLACKFADE_FILENAME = "STORE_BLACKOUT";
const char* BACKGROUNDMOVIE_FILENAME = "store_background";

const char* CONSUMABLE_AMOUNT_SUB_TAG = "[cash pack amount]";
const int CONSUMABLE_AMOUNT_STR_LEN = 32;

#if __XENON
const char* PRICE_STRING_SUFFIX = "";
#else
const char* PRICE_STRING_SUFFIX = "";
#endif

const s32 DEFAULT_TOP_CATAGORY = 0;
const s32 NUM_COLUMNS = 3;

const s32 CAT_COLUMN_INDEX = 0;
const s32 ITEM_COLUMN_INDEX = 1;
const s32 DETAIL_COLUMN_INDEX = 2;
const int MAX_NUM_ITEMS_IN_MIDDLE_COLUMN = 4;

const int MAX_REQUESTED_IMAGES = 6;

const int INVALID_COLUMN_INDEX = -1;
const int INVALID_ITEM_INDEX = -1;

const int RIGHT_STICK_DEADZONE = 64;
const int SIZE_OF_PRICE_BUFFER = 64;
const int STORE_FADE_TIME = 1000;

const int OFFLINE_FRAMES_TO_WAIT_FOR_SIGNOUT_MESSAGE = 12;

bool cStoreMainScreen::sm_bPlayerHasJustSignedOut = false;

static rlPresence::Delegate sPresenceDlgt;

cStoreMainScreen::cStoreMainScreen() :
    m_LastSelectionTransition(NO_SELECTION_TRANSITION),
    m_StoreMovieId(INVALID_MOVIE_ID),
    m_ButtonMovieId(INVALID_MOVIE_ID),
    m_StoreBgMovieId(INVALID_MOVIE_ID),
    m_BlackFadeMovieId(INVALID_MOVIE_ID),
    m_Initialised(false),
    m_PendingListSetup(false),
    m_ColumnReturnId(0),
    m_ItemReturnId(0),
    m_LastColumnIndex(CAT_COLUMN_INDEX),
    m_LastItemIndex(INVALID_ITEM_INDEX),
    m_LastCenterItemIndex(INVALID_ITEM_INDEX),
    m_PendingSelection(false),
    m_PendingHighlightChange(false),
    m_PendingLongDescriptionItem(NULL),
    m_PendingCheckoutProduct(NULL),
    m_CurrentlySelectedCategory(INVALID_ITEM_INDEX),
    m_WasInCheckout(false),
    m_RefetchingPlatformData(false),
    m_CurrentStartReturnId(0),
    m_CurrentEndReturnId(0),
    m_LastStartIndex(INVALID_ITEM_INDEX),
    m_LastEndIndex(INVALID_ITEM_INDEX),
    m_bLoadingAssets(false),
    m_ConsumableProductCheckLevel(0),
    mp_CurrentLeaderboardRead(NULL),
    mp_PendingNumFriendsWhoOwnProduct(NULL),
	m_CurrentNonCriticalWarningScreen(STORE_NON_CRIT_WARNING_NONE),
    m_PendingNonCriticalWarningScreen(STORE_NON_CRIT_WARNING_NONE),
	m_CurrentDCWarningScreen(STORE_WARNING_NONE),
    m_FramesOffline(0),
	m_InNoProductsMode(false),
	m_PedHeadShotHandle(0),
	m_PendingCash(0),
	m_WasConsumableDataDirtyLastFrame(false),
	m_StoreListsSetup(false),
	m_QuickCheckout(false),
	m_HasBeenInCheckout(false),
	m_WasSignedOnlineOnEnter(false)
{
    m_CurrentState = STATE_INVALID;

    m_ProductIndexArray.Reset();
    m_CategoryIndexArray.Reset();

    m_RequestedImageIndices.Reset();
    m_RequestedImageIndices.Reserve( MAX_REQUESTED_IMAGES );
    
    m_LoadedImageIndices.Reset();
    m_LoadedImageIndices.Reserve( MAX_REQUESTED_IMAGES );

    m_NumFriendsWhoOwnProductMap.Reset();

	sm_bPlayerHasJustSignedOut = false;
	if (!sPresenceDlgt.IsBound())
	{
		sPresenceDlgt.Bind(cStoreMainScreen::OnPresenceEvent);
		rlPresence::AddDelegate(&sPresenceDlgt);
	}

#if RSG_NP
	m_shouldCommerceLogoDisplay = false;
	m_isCommerceLogoDisplaying = false;
#endif
}

cStoreMainScreen::~cStoreMainScreen()
{
    Shutdown();

	if (sPresenceDlgt.IsBound())
		sPresenceDlgt.Unbind();

	rlPresence::RemoveDelegate(&sPresenceDlgt);
}

void cStoreMainScreen::Init( int a_StoreMovieId, int a_ButtonMovieId )
{
    m_StoreMovieId = a_StoreMovieId;
    m_ButtonMovieId = a_ButtonMovieId;

    InitInternal();

    m_CurrentState = STATE_PENDING_INITIAL_DATA;

    m_PurchaseCheckProductID.Reset();
	m_WasConsumableDataDirtyLastFrame = false;

	m_TimerSinceInputEnabled.Zero();
}

void cStoreMainScreen::Init()
{
    InitInternal();

    RequestScaleformMovies();
}

void cStoreMainScreen::InitInternal()
{
    m_LastSelectionTransition = NO_SELECTION_TRANSITION;

    m_Initialised = true;

    m_ColumnReturnId = 0;
    m_ItemReturnId = 0;
    m_LastColumnIndex = CAT_COLUMN_INDEX;
    m_LastItemIndex = INVALID_ITEM_INDEX;
    m_LastCenterItemIndex = INVALID_ITEM_INDEX;
    m_PendingSelection = false;
    m_PendingHighlightChange = false;
    m_PendingLongDescriptionItem = NULL;
    m_PendingCheckoutProduct = NULL;
    m_CurrentlySelectedCategory = INVALID_ITEM_INDEX;
    m_WasInCheckout = false;
    m_RefetchingPlatformData = false;
    m_WasSignedOnlineOnEnter = CLiveManager::IsOnline();

    m_ProductIndexArray.Reset();
    m_CategoryIndexArray.Reset();

    m_RequestedImageIndices.Reset();
    m_RequestedImageIndices.Reserve( MAX_REQUESTED_IMAGES );

    m_LoadedImageIndices.Reset();
    m_LoadedImageIndices.Reserve( MAX_REQUESTED_IMAGES );

    m_CurrentStartReturnId = 0;
    m_CurrentEndReturnId = 0;

    m_NumFriendsWhoOwnProductMap.Reset();
    m_CurrentLeaderboardDataFetchProductId.Reset();
    mp_PendingNumFriendsWhoOwnProduct = NULL;

	m_CurrentDCWarningScreen = STORE_WARNING_NONE;

    m_bLoadingAssets = false;

	sm_bPlayerHasJustSignedOut = false;

	m_ConsumableProductCheckId.Reset();
	m_ConsumableProductCheckLevel = 0;
	m_FramesOffline = 0;
	m_InNoProductsMode = false;

	m_CurrentNonCriticalWarningScreen = STORE_NON_CRIT_WARNING_NONE;
	m_PendingNonCriticalWarningScreen = STORE_NON_CRIT_WARNING_NONE;

	m_WasConsumableDataDirtyLastFrame = false;
	m_PendingCash = 0;
	m_StoreListsSetup = false;
	m_QuickCheckout = false;

	m_SubscriptionUpsellItemId.Clear();
	m_SubscriptionUpsellTargetItemId.Clear(); 
    m_PendingImageRequest = false;

	InitialiseConsumablesLevel();
// 	if ( m_PedHeadShotHandle == 0 )
// 	{
// 		m_PedHeadShotHandle = (s32)PEDHEADSHOTMANAGER.RegisterPed(CGameWorld::FindLocalPlayer());
// 	}
}

void cStoreMainScreen::Shutdown()
{
#if RSG_NP
	g_rlNp.GetCommonDialog().HideCommerceLogo();
	m_shouldCommerceLogoDisplay = false;
	m_isCommerceLogoDisplaying = false;
#endif

    m_SubscriptionUpsellItemId.Clear();
    m_SubscriptionUpsellTargetItemId.Clear();
    m_PendingImageRequest = false;

    // switch this off when we exit the store
    MEMBERSHIP_ONLY(CLiveManager::SetSuppressMembershipForScript(false));

    if ( m_StoreMovieId != INVALID_MOVIE_ID )
    {
        CScaleformMgr::RequestRemoveMovie(m_StoreMovieId);
        m_StoreMovieId = INVALID_MOVIE_ID;
    }

    if ( m_ButtonMovieId != INVALID_MOVIE_ID )
    {
		CBusySpinner::UnregisterInstructionalButtonMovie(m_ButtonMovieId);  // register this "instructional button" movie with the spinner system
        CScaleformMgr::RequestRemoveMovie(m_ButtonMovieId);
        m_ButtonMovieId = INVALID_MOVIE_ID;
    }

    if ( m_StoreBgMovieId != INVALID_MOVIE_ID )
    {
        CScaleformMgr::RequestRemoveMovie(m_StoreBgMovieId);
        m_StoreBgMovieId = INVALID_MOVIE_ID;
    }

    if ( m_BlackFadeMovieId != INVALID_MOVIE_ID )
    {
        CScaleformMgr::RequestRemoveMovie(m_BlackFadeMovieId);
        m_BlackFadeMovieId = INVALID_MOVIE_ID;  
    }

    const rage::cCommerceUtil *pCommerceUtil = CLiveManager::GetCommerceMgr().GetCommerceUtil();

    for ( s32 i = 0; i < m_LoadedImageIndices.GetCount(); i++ )
    {
        const cCommerceItemData *itemData = pCommerceUtil->GetItemData(m_LoadedImageIndices[i]);

        if ( itemData && itemData->GetImagePaths().GetCount() > 0 )
        {
            const atString path(itemData->GetImagePaths()[0]);
            cStoreScreenMgr::GetTextureManager()->ReleaseTexture( path );
        }
    }
    m_LoadedImageIndices.Reset();

    CLiveManager::GetCommerceMgr().SetAutoConsumePaused(false);

    if (mp_CurrentLeaderboardRead != NULL)
    {
        CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr().ClearLeaderboardRead(mp_CurrentLeaderboardRead);
        mp_CurrentLeaderboardRead = NULL;
        m_CurrentLeaderboardDataFetchProductId.Reset();
    }
    
	if (m_PedHeadShotHandle != 0)
	{
		PEDHEADSHOTMANAGER.UnregisterPed(m_PedHeadShotHandle);
		m_PedHeadShotHandle = 0;
	}

    mp_PendingNumFriendsWhoOwnProduct = NULL;

    m_Initialised = false;
    m_CurrentState = STATE_INVALID;

    //Take this opportunity to kick off an data enumeration.
    if (!CLiveManager::GetCommerceMgr().GetConsumableManager()->IsOwnershipDataDirty())
    {
        CLiveManager::GetCommerceMgr().GetConsumableManager()->SetOwnershipDataDirty();
    }

#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
	// refresh membership status - defaulted to false
	if(Tunables::IsInstantiated() && Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_MEMBERSHIP_REFRESH_ON_STORE_SHUTDOWN", 0x8f865fd1), false))
	{
		storeUIDebugf1("Shutdown :: Requesting Membership Status");
		rlScMembership::RequestMembershipStatus(NetworkInterface::GetLocalGamerIndex());
	}
#endif

	//We should redisplay cash limit warnings at this point.
	CLiveManager::GetCommerceMgr().ResetAutoConsumeMessage();
}

void cStoreMainScreen::UpdateStoreLists()
{
    if (!CLiveManager::GetCommerceMgr().HasValidData())
    {
        return;
    }

    ClearImageRequests();
    m_ProductIndexArray.Reset();
    m_CategoryIndexArray.Reset();
	
	m_StoreListsSetup =  true;

    if (m_InNoProductsMode)
    {
        if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "MENU_STATE"))
        {
            CScaleformMgr::AddParamInt(1);
            CScaleformMgr::EndMethod();
        }
    }

    //Clear up previous item column
 
    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "REMOVE_COLUMN"))
    {
        CScaleformMgr::AddParamInt(ITEM_COLUMN_INDEX);
        CScaleformMgr::EndMethod();
    }


    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "ADD_COLUMN"))
    {
        //Plus one because this function seems to count the columns up from one. Charming.
        CScaleformMgr::AddParamInt(ITEM_COLUMN_INDEX);
        CScaleformMgr::AddParamString("contentImageList");
        CScaleformMgr::EndMethod();
    }

    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_HEADER_COLOUR"))
    {
        CRGBA headerColour = CHudColour::GetRGBA(HUD_COLOUR_BLUE);
        CScaleformMgr::AddParamInt(headerColour.GetRed());
        CScaleformMgr::AddParamInt(headerColour.GetGreen());
        CScaleformMgr::AddParamInt(headerColour.GetBlue());
        CScaleformMgr::EndMethod();   
    }

    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_COLUMN_TITLE"))
    {
        CScaleformMgr::AddParamInt(0);
        CScaleformMgr::AddParamString(TheText.Get("CMRC_CATEGS"));
        CScaleformMgr::EndMethod();   
    }

    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_COLUMN_TITLE"))
    {
        CScaleformMgr::AddParamInt(1);
        CScaleformMgr::AddParamString(TheText.Get("CMRC_CONTENT"));
        CScaleformMgr::EndMethod();   
    }

#if __PPU
    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SHOW_PLAYSTATION_LOGO"))
    {
        CScaleformMgr::AddParamBool(true);
        CScaleformMgr::EndMethod();   
    }  
#endif
    
    const rage::cCommerceUtil *pCommerceUtil = CLiveManager::GetCommerceMgr().GetCommerceUtil();

    storeUIAssert( pCommerceUtil );

    if ( pCommerceUtil == NULL )
    {
        return;
    }

    //Build the category list
    const rage::cCommerceItemData* pTopLevelItemData = NULL;
    pTopLevelItemData = pCommerceUtil->GetItemData( cStoreScreenMgr::GetBaseCategory(), cCommerceItemData::ITEM_TYPE_CATEGORY );
    const cCommerceCategoryData* pTopLevelCategoryData = NULL;
    if(pTopLevelItemData && pTopLevelItemData->GetItemType() == cCommerceItemData::ITEM_TYPE_CATEGORY)
    {
        pTopLevelCategoryData = static_cast<const cCommerceCategoryData*>(pTopLevelItemData);
    }

    if (pTopLevelCategoryData==NULL)
    {
        //Handle the case where a top level category is not found for whatever reason
        pTopLevelItemData = pCommerceUtil->GetItemData( "TOP_DEFAULT", cCommerceItemData::ITEM_TYPE_CATEGORY );
        if(pTopLevelItemData && pTopLevelItemData->GetItemType() == cCommerceItemData::ITEM_TYPE_CATEGORY)
        {
            pTopLevelCategoryData = static_cast<const cCommerceCategoryData*>(pTopLevelItemData);
        }
    }
    
    int numberOfCategories = 0;

    bool customTopLevelCategory = false;

    if(pTopLevelCategoryData)
    {
        for(int i= 0; i < pTopLevelCategoryData->GetNumProducts(); i++)
        {
            int itemIndex = pTopLevelCategoryData->GetItemIndex(i);
            const cCommerceItemData* pCatItemData = pCommerceUtil->GetItemData(itemIndex);

            if (pCatItemData)
            {
                //Check if we are a category or an item.
				if (pCatItemData->GetItemType() == cCommerceItemData::ITEM_TYPE_CATEGORY && (static_cast<const cCommerceCategoryData*>(pCatItemData))->GetNumProducts() > 0 )
                {
                    m_CategoryIndexArray.PushAndGrow(itemIndex);
                    //Category, add to appropriate column
                    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_DATA_SLOT"))
                    {
                        atString tempString = pCatItemData->GetName();
                        storeUIDebugf3("Setting the string name to %s",tempString.c_str());
                        CScaleformMgr::AddParamInt(CAT_COLUMN_INDEX);
                        CScaleformMgr::AddParamInt(numberOfCategories);
                        CScaleformMgr::AddParamString(tempString.c_str());
                        CScaleformMgr::EndMethod();   

                        numberOfCategories++;
                    }

                }
            }
        }
    }

    int initialCategoryIndex = INVALID_ITEM_INDEX;
    int currentCatagory = m_CurrentlySelectedCategory;

    if ( m_CurrentlySelectedCategory == INVALID_ITEM_INDEX )
    {
        //We are in the initial state of the store. Display the first category found.
        if ( m_CategoryIndexArray.GetCount() == 0 )
        {
            //Check if we are looking at an over-ridden base category and see if that category is valid
            if (pTopLevelCategoryData )
            {
                //Special case, a custom category with items but no sub category
               
                //Category, add to appropriate column
                if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_DATA_SLOT"))
                {
                    atString tempString = pTopLevelCategoryData->GetName();
                    storeUIDebugf3("Setting the string name to %s",tempString.c_str());
                    CScaleformMgr::AddParamInt(CAT_COLUMN_INDEX);
                    CScaleformMgr::AddParamInt(numberOfCategories);
                    CScaleformMgr::AddParamString(tempString.c_str());
                    CScaleformMgr::EndMethod();   

                    numberOfCategories++;
                }

                //Find the ID of this category
                for (int i=0; i < pCommerceUtil->GetNumItems(); i++)
                {
                    if ( pCommerceUtil->GetItemData(i)->GetId() == pTopLevelCategoryData->GetId() )
                    {
                         m_CategoryIndexArray.PushAndGrow(i);
                         break;
                    }
                }

                customTopLevelCategory = true;
            }
            else
            {
                //No categories. Call function to setup no data notification here.
                return;
            }
        }
        else
        {
			storeUIDebugf1("UpdateStoreLists :: InitialItemId: %s, SubscriptionUpsellItemId: %s, SubscriptionUpsellTargetItemId: %s", 
                cStoreScreenMgr::GetInitialItemID().c_str(),
                m_SubscriptionUpsellItemId.c_str(),
                m_SubscriptionUpsellTargetItemId.c_str());
			
            // tracking whether we have an initial item or not
			const cCommerceItemData* pInitialItemData = nullptr;
			
            int validInitialCategoryIndex = INVALID_ITEM_INDEX;
            if ( cStoreScreenMgr::GetInitialItemID().GetLength() > 0 )
            {
                pInitialItemData = pCommerceUtil->GetItemDataByItemId(cStoreScreenMgr::GetInitialItemID().c_str());
			}
            // if we have a target for post-upsell, use that
            else if(m_SubscriptionUpsellTargetItemId.GetLength() > 0)
                {
				pInitialItemData = pCommerceUtil->GetItemDataByItemId(m_SubscriptionUpsellTargetItemId.c_str());

                // our subscription changed, request new images
                m_PendingImageRequest = true;
                }

			// tracking whether we have an initial item or not
                if ( pInitialItemData )
                {
                    //We found our initial item. Now check if there is a category that it is a member of in our category list
				storeUIDebugf1("UpdateStoreLists :: Found InitialItem: %s", pInitialItemData->GetName().c_str());
                    
                    for ( int iItemsCats = 0; iItemsCats < pInitialItemData->GetCategoryMemberships().GetCount(); iItemsCats++ )
                    {
                        for ( int iValidCats = 0; iValidCats < m_CategoryIndexArray.GetCount(); iValidCats++ )
                        {
                            if ( pInitialItemData->GetCategoryMemberships()[iItemsCats] == pCommerceUtil->GetItemData( m_CategoryIndexArray[iValidCats] )->GetId() )
                            {
                                //We have a found a category which 
                                //a) the item belongs to 
                                //b) is in our base category list
                            validInitialCategoryIndex = iValidCats;
                        }
                    }
                }
            }

            if (validInitialCategoryIndex == INVALID_ITEM_INDEX )
            {
                 currentCatagory = m_CategoryIndexArray[0];
            }
            else
            {
                 currentCatagory = m_CategoryIndexArray[validInitialCategoryIndex];
                 initialCategoryIndex = validInitialCategoryIndex;
            }
        }
    }
    else if ( m_CurrentlySelectedCategory < 0 || m_CurrentlySelectedCategory >= m_CategoryIndexArray.GetCount() )
	{
		commerceErrorf("We have hit the commerce list safety fall through with a category list size of %d and an index of %d", m_CategoryIndexArray.GetCount(), m_CurrentlySelectedCategory);
		DoEmptyStoreCheck();
		return;
	}
	else
    {
        //We have a valid slot.
        storeUIAssert(m_CurrentlySelectedCategory >= 0 && m_CurrentlySelectedCategory < m_CategoryIndexArray.GetCount() );

        currentCatagory = m_CategoryIndexArray[ m_CurrentlySelectedCategory ];
    }

    const cCommerceCategoryData* pCategoryData = NULL;

    if (customTopLevelCategory)
    {
        pCategoryData = pTopLevelCategoryData;
    }
    else
    {
        //Get the list of items in this category to display them.
        const rage::cCommerceItemData* pItemData = NULL; 
        pItemData = pCommerceUtil->GetItemData( currentCatagory );

        if(storeUIVerifyf( pItemData && pItemData->GetItemType() == cCommerceItemData::ITEM_TYPE_CATEGORY, "pItem not retrieved") )
        {
            pCategoryData = static_cast<const cCommerceCategoryData*>(pItemData);
        }
    }
    
    storeUIDebugf1("UpdateStoreLists :: Category data - NumItems: %d, Name: %s", pCategoryData->GetNumProducts(), pCategoryData->GetName().c_str());

    //Clear the existing center data slot of items.
    if ( CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_DATA_SLOT_EMPTY"))
    {
        CScaleformMgr::AddParamInt(ITEM_COLUMN_INDEX);
        CScaleformMgr::EndMethod();
    }

    //Clear the columns.
    for (s32 iClearCols=1; iClearCols < NUM_COLUMNS; iClearCols++)
    {
        if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_DATA_SLOT_EMPTY"))
        {
            CScaleformMgr::AddParamInt(iClearCols);
            CScaleformMgr::EndMethod();
        }
    }

    //Clear the existing center data slot of items.
    if ( CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "DISPLAY_DATA_SLOT"))
    {
        CScaleformMgr::AddParamInt(ITEM_COLUMN_INDEX);
        CScaleformMgr::EndMethod();
    }

    int initialProductIndex = INVALID_ITEM_INDEX;

    int numberOfItems = 0;
   
    int numberOfProducts = 0;
    if(pCategoryData)
    {
        numberOfItems = pCategoryData->GetNumProducts();

        for(int i= 0; i < numberOfItems; i++)
        {
            int itemIndex = pCategoryData->GetItemIndex(i);
            const cCommerceItemData* pCatItemData = pCommerceUtil->GetItemData(itemIndex);

            if (pCatItemData)
            {
                //Check if we are a category or an item.
                if (pCatItemData->GetItemType() == cCommerceItemData::ITEM_TYPE_CATEGORY && (static_cast<const cCommerceCategoryData*>(pCatItemData))->GetNumProducts() > 0 )
                {
                    m_CategoryIndexArray.PushAndGrow(itemIndex);
                    //Category, add to appropriate column
                    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_DATA_SLOT"))
                    {
                        atString tempString = pCatItemData->GetName();
                        storeUIDebugf3("Setting the string name to %s",tempString.c_str());
                        CScaleformMgr::AddParamInt(0);
                        CScaleformMgr::AddParamInt(numberOfCategories);
                        CScaleformMgr::AddParamString(tempString.c_str());
                        CScaleformMgr::EndMethod();   

                        numberOfCategories++;
                    }
                }

                if (pCatItemData->GetItemType() == cCommerceItemData::ITEM_TYPE_PRODUCT)
                {
                    const cCommerceProductData *pProductData = static_cast<const cCommerceProductData*>(pCatItemData);

                    const bool showProduct =
                        (pCommerceUtil->HasProductDetails(pProductData) || pCommerceUtil->ShouldAllowWithoutProduct(pProductData)) &&
                            !pCommerceUtil->ShouldHideProduct(pProductData);

                    storeUIDebugf1("UpdateStoreLists :: %s: %s - %s, HasProduct: %s, AllowWithoutProduct: %s, Hide: %s",
                        showProduct ? "Adding" : "Skipping",
						pProductData ? pProductData->GetId().c_str() : "[InvalidProduct]",
						pProductData ? pProductData->GetName().c_str() : "[InvalidProduct]",
						pCommerceUtil->HasProductDetails(pProductData) ? "True" : "False",
						pCommerceUtil->ShouldAllowWithoutProduct(pProductData) ? "True" : "False",
						pCommerceUtil->ShouldHideProduct(pProductData) ? "True" : "False");

                    // exit if we can't show this product
                    if(!showProduct)
                        continue; 

					m_ProductIndexArray.PushAndGrow(itemIndex);

                    //Item, add to appropriate column
                    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_DATA_SLOT"))
                    {                 
                        CScaleformMgr::AddParamInt(ITEM_COLUMN_INDEX);
                        CScaleformMgr::AddParamInt(numberOfProducts);
                        
                        //No string, so that the images start off as a spinner.
                        CScaleformMgr::AddParamString("");
                        CScaleformMgr::EndMethod();   

                        //Show the first product.
                        if ( numberOfProducts == 0 )
                        {
                            storeUIDebugf1("UpdateStoreLists :: Displaying First Item");
                            DisplayItemDetails( pProductData );
                        }
                        
                        if (pProductData->GetId() == cStoreScreenMgr::GetInitialItemID() || pProductData->GetId() == m_SubscriptionUpsellTargetItemId)
                        {
							storeUIDebugf1("UpdateStoreLists :: Displaying Initial Item (Index: %d, Product: %s)", numberOfProducts, pProductData->GetName().c_str());
							
                            initialProductIndex = numberOfProducts;
                            
                            //Set the initial string to empty so this does not occur each time.
                            atString emptyString("");
                            cStoreScreenMgr::SetInitialItemID(emptyString);

                            //We want these details displayed on initial store entry
                            DisplayItemDetails( pProductData );
                        }

                        numberOfProducts++;
                    }
                }
            }
        }
    }

    if ( numberOfProducts == 0 )
    {
        //We have no products in this category, so clear the details column.
        ClearItemDetails();
    }

    if ( initialCategoryIndex != INVALID_ITEM_INDEX )
    {
        //We have a non zero initial category set
        if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "HIGHLIGHT_ITEM"))
        {
            CScaleformMgr::AddParamInt(CAT_COLUMN_INDEX);
            CScaleformMgr::AddParamInt(initialCategoryIndex);
            CScaleformMgr::EndMethod();
        }

        m_LastItemIndex = initialCategoryIndex;
    }

    if ( initialProductIndex != INVALID_ITEM_INDEX )
    {
        //We have found a requested initial product
        m_LastColumnIndex = ITEM_COLUMN_INDEX;

        if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "HIGHLIGHT_ITEM"))
        {
            CScaleformMgr::AddParamInt(ITEM_COLUMN_INDEX);
            CScaleformMgr::AddParamInt(initialProductIndex);
            CScaleformMgr::EndMethod();
        }

        m_LastCenterItemIndex = initialProductIndex;
        m_LastItemIndex = initialProductIndex;
    }
    else
    {
        if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "HIGHLIGHT_COLUMN"))
        {
            CScaleformMgr::AddParamInt(CAT_COLUMN_INDEX);
            CScaleformMgr::EndMethod();

            m_LastCenterItemIndex = 0;
        }
    }

    for (s32 iCols=0; iCols < NUM_COLUMNS; iCols++)
    {
        if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "DISPLAY_DATA_SLOT"))
        {
            CScaleformMgr::AddParamInt(iCols);
            CScaleformMgr::EndMethod();
        }
    }

    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "GET_CURRENT_START_INDEX"))
    {
        CScaleformMgr::AddParamInt(ITEM_COLUMN_INDEX);
        m_CurrentStartReturnId = CScaleformMgr::EndMethodReturnValue(m_CurrentStartReturnId);
    }

    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "GET_CURRENT_END_INDEX"))
    {
        CScaleformMgr::AddParamInt(ITEM_COLUMN_INDEX);
        m_CurrentEndReturnId = CScaleformMgr::EndMethodReturnValue(m_CurrentEndReturnId);
    }

	//Check for empty store
	DoEmptyStoreCheck();
}

void cStoreMainScreen::Update()
{
    rage::cCommerceUtil *pCommerceUtil = CLiveManager::GetCommerceMgr().GetCommerceUtil();

    if ( pCommerceUtil == NULL )
    {
        return;
    }

    if (!m_Initialised)
    {
        return;
    }

#if RSG_NP
	if (CWarningScreen::IsActive())
	{
		if (m_isCommerceLogoDisplaying)
		{
			g_rlNp.GetCommonDialog().HideCommerceLogo();
			m_isCommerceLogoDisplaying = false;
		}
	}
	else
	{
		if (!m_isCommerceLogoDisplaying && m_shouldCommerceLogoDisplay)
		{
			m_isCommerceLogoDisplaying = g_rlNp.GetCommonDialog().ShowCommerceLogo(rlNpCommonDialog::COMMERCE_LOGO_POS_LEFT);
		}
	}
#endif

    if (!UpdateWarningScreen())
    {
        return;
    }
    
	if ( !UpdateNonCriticalWarningScreen() )
	{
		return;
	}

    if (m_PendingListSetup && !CLiveManager::GetCommerceMgr().GetCommerceUtil()->IsInErrorState() )
    {
        UpdateStoreLists();
        m_PendingListSetup = false;
    }

    if (m_PendingLongDescriptionItem != NULL && pCommerceUtil->GetItemLongDescription(m_PendingLongDescriptionItem) != NULL)
    {
        DisplayItemDetails(m_PendingLongDescriptionItem);
        m_PendingLongDescriptionItem = NULL;
    }
        
    switch( m_CurrentState )
    {
    case(STATE_INITIALISING):
        UpdateInitialising();
        break;
    case(STATE_NEUTRAL):
        UpdateInput();
        break;
    case(STATE_PENDING_INITIAL_DATA):
        UpdatePendingInitialData();
        break;
    case(STATE_LOADING_ASSETS):
        UpdateLoadingAssets();
        break;
    case(STATE_IN_CHECKOUT):
        UpdateCheckout();
        break;
    default:
        break;
    }

    if ( m_ColumnReturnId != 0 )
    {
        if (CScaleformMgr::IsReturnValueSet(m_ColumnReturnId))
        {
            m_LastColumnIndex = CScaleformMgr::GetReturnValueInt(m_ColumnReturnId);
            m_ColumnReturnId = 0;
        }
    }

    if ( m_ItemReturnId != 0 )
    {
        if (CScaleformMgr::IsReturnValueSet(m_ItemReturnId))
        {
            m_LastItemIndex = CScaleformMgr::GetReturnValueInt(m_ItemReturnId);

			if ( m_LastItemIndex < 0  )
			{
				commerceErrorf("m_LastItemIndex set to %d. Almost certainly bad.", m_LastItemIndex);
			}

            m_ItemReturnId = 0;
        }
    }

    bool imageUpdateRequired = false;

    if ( m_CurrentStartReturnId != 0 )
    {
        if (CScaleformMgr::IsReturnValueSet(m_CurrentStartReturnId))
        {
            m_LastStartIndex = CScaleformMgr::GetReturnValueInt(m_CurrentStartReturnId);
            m_CurrentStartReturnId = 0;

            if ( m_CurrentEndReturnId == 0 )
            {
                imageUpdateRequired = true;
            }

//             if (m_LastStartIndex < 0 || m_LastStartIndex > m_ProductIndexArray.GetCount() )
//             {
//                 m_LastStartIndex = 0;
//             }
        }
    }

    if ( m_CurrentEndReturnId != 0 )
    {
        if (CScaleformMgr::IsReturnValueSet(m_CurrentEndReturnId))
        {
            m_LastEndIndex = CScaleformMgr::GetReturnValueInt(m_CurrentEndReturnId);
            m_CurrentEndReturnId = 0;

            if ( m_CurrentStartReturnId == 0 )
            {
                imageUpdateRequired = true;
            }

//             if (m_LastEndIndex < 0 || m_LastEndIndex > m_ProductIndexArray.GetCount() )
//             {
//                 m_LastEndIndex = 0;
//             }
        }
    }

    //We have our new start and end IDs after a move, sort out the images
    if (imageUpdateRequired || m_PendingImageRequest)
    {
        m_PendingImageRequest = false;
        PopulateImageRequests();
    }

    UpdateImageRequests();

	if ( pCommerceUtil->IsCategoryInfoPopulated() && m_CurrentState == STATE_NEUTRAL && CBusySpinner::IsOn() )
	{
		CBusySpinner::Off( SPINNER_SOURCE_STORE_LOADING );
	}

    if ( pCommerceUtil->IsContentInfoPopulated() && m_PendingHighlightChange && m_ItemReturnId == 0 && m_ColumnReturnId == 0 )
    {
        ProcessHighlightChange();
        m_PendingHighlightChange = false;
        UpdateContextHelp();
    }

    UpdateLeaderBoardDataFetch();

    //If we were in the checkout we need to detect when that is no longer the case and correctly update the product states
    if ( m_WasInCheckout && pCommerceUtil->IsInCheckout() == false && CLiveManager::IsOnline() && CLiveManager::IsOnlineRos() 
         && pCommerceUtil->DidLastCheckoutSucceed() )
    {
        storeUIDebugf1("Update :: Now out of store");
        //SEND AN UPDATE TO THE CONTENT SYSTEM TO RESCAN HERE

		//Send the metric containing the completed checkout product here
		const int TEMP_PRICE_STRING_LEN = 128;
		char tempPriceString[TEMP_PRICE_STRING_LEN];
		atString priceString;
		if (m_PendingCheckoutProduct != nullptr && pCommerceUtil->GetProductPrice(m_PendingCheckoutProduct, tempPriceString, TEMP_PRICE_STRING_LEN) >= 0.0f)
		{			
			CNetworkTelemetry::AppendMetric(MetricIngameStoreCheckoutComplete(tempPriceString, (m_PendingCheckoutProduct ? m_PendingCheckoutProduct->GetPlatformId().c_str() : "")));
		}


// PS3 only xbox should process event for this
#if __PS3
        EXTRACONTENT.OnContentDownloadCompleted();
#endif // __PS3 

#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
				// refresh membership status - defaulted to true
				if(Tunables::IsInstantiated() && Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_MEMBERSHIP_REFRESH_ON_PURCHASED_MEMBERSHIP", 0x2be302ce), true))
				{
					storeUIDebugf1("Update :: Requesting Membership Status");
					rlScMembership::RequestMembershipStatus(NetworkInterface::GetLocalGamerIndex());
				}

				// Add relevant subscription Ids
				if(rlScMembership::HasMembership(NetworkInterface::GetLocalGamerIndex()))
				{
					storeUIDebugf1("Update :: AddSubscriptionId: %s", rlScMembership::GetSubscriptionId());
					CLiveManager::GetCommerceMgr().GetCommerceUtil()->AddSubscriptionId(rlScMembership::GetSubscriptionId());
				}

                // disable membership notifications for script
                // don't reset the upsell item here - we need it to reinsert ourselves in the selection window
                if(!m_SubscriptionUpsellItemId.empty())
                {
					storeUIDebugf1("Update :: Upsell Path, suppressing membership");
					CLiveManager::SetSuppressMembershipForScript(true);

                    // we also want to show the welcome here
					m_PendingNonCriticalWarningScreen = STORE_SUBSCRIPTION_WELCOME;
                    CLiveManager::SetHasShownMembershipWelcome();
                }
#endif

        pCommerceUtil->DoDataRequest(false);
        m_RefetchingPlatformData = true;   
    }

    if ( m_RefetchingPlatformData == true && !pCommerceUtil->IsInDataFetchState() && pCommerceUtil->IsContentInfoPopulated() )
    {
        m_RefetchingPlatformData = false;
        //ClearLoadedImages();
        //m_PendingListSetup = true;
        TriggerHighlightChange();
        
        UpdateContextHelp();

        //Check to see if a previously requested product is now owned, increment the money spent stat if so.
        if ( m_PurchaseCheckProductID.GetLength() > 0 )
        {
            rage::cCommerceUtil *pCommerceUtil = CLiveManager::GetCommerceMgr().GetCommerceUtil();
            storeUIAssert( pCommerceUtil );
            if ( pCommerceUtil != NULL )
            {
                const cCommerceItemData *purchasedItem = pCommerceUtil->GetItemData(m_PurchaseCheckProductID);

                if ( purchasedItem && purchasedItem->GetItemType() == cCommerceItemData::ITEM_TYPE_PRODUCT )
                {
                    const cCommerceProductData *pProductData = static_cast<const cCommerceProductData*>(purchasedItem);

                    //Check to see if the item is now owned
                    if ( pProductData->IsConsumable() )
                    {
                        //Its a consumable. Store its inventory level 
                        m_ConsumableProductCheckId = m_PurchaseCheckProductID;
                        m_ConsumableProductCheckLevel = CLiveManager::GetCommerceMgr().GetConsumableManager()->GetConsumableLevel(pProductData->GetConsumableId());
                        
                        //then refetch data to compare.
                        CLiveManager::GetCommerceMgr().GetConsumableManager()->SetOwnershipDataDirty();

                        //Pause auto consume to avoid potential timing issues
                        CLiveManager::GetCommerceMgr().SetAutoConsumePaused(true);

                    }
                    else if ( pCommerceUtil->IsProductPurchased(pProductData) )
                    {
						//Really not appropriate, due to currency differences
                        //AddProductsPriceToCashSpentStat(pProductData);
                    }
                    else
                    { 
                        StatsInterface::IncrementStat(STAT_MPPLY_STORE_CHECKOUTS_CANCELLED, 1);
                    }
                }
            }

            m_PurchaseCheckProductID.Reset();
        }
		else
		{
			//Probably a code entry. Rescan consumables in case it was a cash entry.
			CLiveManager::GetCommerceMgr().GetConsumableManager()->SetOwnershipDataDirty();
		}
    }

    //Do we have a pending consumables level check and valid consumables data since the last refresh?
    if ( m_ConsumableProductCheckId.GetLength() > 0 && CLiveManager::GetCommerceMgr().GetConsumableManager()->IsOwnershipDataPopulated() )
    {
        //Yes, compare the ownership level
        rage::cCommerceUtil *pCommerceUtil = CLiveManager::GetCommerceMgr().GetCommerceUtil();
        storeUIAssert( pCommerceUtil );
        if ( pCommerceUtil != NULL )
        {
            const cCommerceItemData *purchasedItem = pCommerceUtil->GetItemData(m_ConsumableProductCheckId);

            if ( purchasedItem && purchasedItem->GetItemType() == cCommerceItemData::ITEM_TYPE_PRODUCT )
            {
                const cCommerceProductData *pProductData = static_cast<const cCommerceProductData*>(purchasedItem);

                if (CLiveManager::GetCommerceMgr().GetConsumableManager()->GetConsumableLevel(pProductData->GetConsumableId()) > m_ConsumableProductCheckLevel)
                {
                    AddProductsPriceToCashSpentStat(pProductData);
                }
                else
                {
                    StatsInterface::IncrementStat(STAT_MPPLY_STORE_CHECKOUTS_CANCELLED, 1);
                }
            }
        }

        m_ConsumableProductCheckId.Reset();
    }

	if ( m_WasConsumableDataDirtyLastFrame && CLiveManager::GetCommerceMgr().GetConsumableManager()->IsOwnershipDataPopulated() )
	{
		m_WasConsumableDataDirtyLastFrame = false;
		UpdateChangeInConsumableLevelCheck();
		UpdatePendingCash();
	}

	UpdateTopOfScreenTextInfo();

	if ( CLiveManager::GetCommerceMgr().GetConsumableManager()->IsOwnershipDataDirty() )
	{
		m_WasConsumableDataDirtyLastFrame = true;
	}

	if ( pCommerceUtil->IsContentInfoPopulated() && m_PendingSelection )
	{
		ProcessSelection();
		m_PendingSelection = false;
		UpdateContextHelp();
	}

	CBusySpinner::Update();
    m_WasInCheckout = pCommerceUtil->IsInCheckout();
}

void cStoreMainScreen::Render()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP();

	float fTimeStep = fwTimer::GetSystemTimeStep();
    if (m_StoreBgMovieId >= 0)
    {
        CScaleformMgr::RenderMovie( m_StoreBgMovieId, fTimeStep, true );                    
    }

	if (m_CurrentState == STATE_LOADING_ASSETS ||
		m_CurrentState == STATE_INITIALISING ||
		m_CurrentState == STATE_PENDING_INITIAL_DATA)
	{
		CPauseMenu::RenderAnimatedSpinner(-1.0f, true, false);  // fix for 1495684
	}

    if ( /*m_CurrentState == STATE_LOADING_ASSETS  ||*/ m_CurrentState == STATE_PENDING_INITIAL_DATA && !cStoreScreenMgr::IsExitRequested() )
    {
		//char cHtmlFormattedBusyString[256];
		//char* html = CTextConversion::TextToHtml(TheText.Get("STORE_LOADING"),cHtmlFormattedBusyString,sizeof(cHtmlFormattedBusyString));
		//CBusySpinner::On( html, SPINNER_ICON_LOADING, SPINNER_SOURCE_STORE_LOADING );
		//CPauseMenu::RenderAnimatedSpinner(-1.0f,true);
    }
	
    if (!m_Initialised)
    {
        return;
    }

#if RSG_PC
	// hide instructional buttons if SCUI is showing ...hints are confusing
	if (!g_rlPc.IsUiShowing())
#endif
	{
		if ( m_ButtonMovieId >= 0)
		{
			CScaleformMgr::RenderMovie( m_ButtonMovieId, fTimeStep, true );
		}
		else 
		{
			int buttonMovieIndex = CScaleformMgr::FindMovieByFilename(BUTTONMENU_FILENAME);
			if (  buttonMovieIndex >= 0 )
			{
				CScaleformMgr::RenderMovie( buttonMovieIndex, fTimeStep, true );
			}
		}
	}

    if ( m_StoreMovieId >= 0)
    {
        CScaleformMgr::RenderMovie( m_StoreMovieId, fTimeStep, true );
    }

    if (CNewHud::GetHudComponentMovieId(NEW_HUD_SAVING_GAME) >= 0)
    {
        CScaleformMgr::RenderMovie( CNewHud::GetHudComponentMovieId(NEW_HUD_SAVING_GAME), fTimeStep, true );
    }

    if ( m_BlackFadeMovieId >= 0 )
    {
        CScaleformMgr::RenderMovie( m_BlackFadeMovieId, fTimeStep, true );   
    }

}

void cStoreMainScreen::UpdateInitialising()
{
    //They really, really should be if we are in here.
    storeUIAssert(AreAllAssetsActive());
    if (AreAllAssetsActive())
    {
        if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "MENU_STATE"))
        {
            s32 menuStateParam = 1; //1 is the value we pass for the store screen.
            CScaleformMgr::AddParamInt(menuStateParam);
            CScaleformMgr::EndMethod();
        }


        if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_CONTENTIMAGE_SIZE"))
        {
            CScaleformMgr::AddParamInt( static_cast<int>(pow(2.0f, cStoreScreenMgr::GetStoreItemNumPerColumnExp() ) ));
            CScaleformMgr::EndMethod();   
        }

        if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_SHOP_LOGO"))
        {
            CScaleformMgr::AddParamInt(cStoreScreenMgr::GetBranding());
            CScaleformMgr::EndMethod();   
        }

        if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SHOW_SHOP_LOGO"))
        {
            CScaleformMgr::AddParamBool( cStoreScreenMgr::GetBranding() != 0 );
            CScaleformMgr::EndMethod();   
        }

		UpdateTopOfScreenTextInfo();

// 		if (m_PedHeadShotHandle != 0 && CScaleformMgr::BeginMethod( m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_CHAR_IMG"))
// 		{
// 			const char* pedHeadShotTextureName = PEDHEADSHOTMANAGER.GetTextureName((s32)m_PedHeadShotHandle);
// 			CScaleformMgr::AddParamString( pedHeadShotTextureName, false);
// 			CScaleformMgr::AddParamString( pedHeadShotTextureName, false);
// 			bool 
// 			CScaleformMgr::AddParamBool( pedHeadShotTextureName[0] != '\0');  // fixes 1212873
// 			CScaleformMgr::EndMethod();
// 		}
    }
    else
    {
        storeUIErrorf("Assets were not loaded upon reaching the store menu. Extreme badness alert.");
        return;
    }

    m_CurrentState = STATE_NEUTRAL;
    m_Initialised = true;
	CLiveManager::GetCommerceMgr().OnEnterUIStore();
	
	if ( !m_TimerSinceInputEnabled.IsStarted() )
	{
		m_TimerSinceInputEnabled.Start();
	}
	

    UpdateContextHelp();

    //Take the loading spinner off screen
    //CBusySpinner::Off( SPINNER_SOURCE_STORE_LOADING );
}

bool cStoreMainScreen::AreAllAssetsActive()
{
    bool retVal = ( m_StoreMovieId != INVALID_MOVIE_ID && CScaleformMgr::IsMovieActive(m_StoreMovieId) && 
                    m_StoreBgMovieId != INVALID_MOVIE_ID && CScaleformMgr::IsMovieActive(m_StoreBgMovieId) &&
                    m_BlackFadeMovieId != INVALID_MOVIE_ID);

    if ( m_ButtonMovieId != INVALID_MOVIE_ID && !CScaleformMgr::IsMovieActive(m_ButtonMovieId) )
    {
        retVal = false;
    }



    return retVal;

}

void cStoreMainScreen::CheckIncomingFunctions( atHashWithStringBank methodName, const GFxValue* /*args*/ )
{
    if (!m_Initialised)
    {
        return;
    }

    if (methodName == ATSTRINGHASH("LOAD_PAUSE_MENU_STORE_CONTENT",0x879bc589))
    {
        storeUIDebugf3("Store Advert screen callback recieved by main menu.\n");

        if (!DoQuickCheckout())
        {
            m_PendingListSetup = true;

#if RSG_NP
            m_isCommerceLogoDisplaying = g_rlNp.GetCommonDialog().ShowCommerceLogo(rlNpCommonDialog::COMMERCE_LOGO_POS_LEFT);
            m_shouldCommerceLogoDisplay = true;
#endif
        }

        return;
    }

     if ( methodName == ATSTRINGHASH("FADE_TO_BLACK_COMPLETED",0x8d27a5a7) )
     {
        if ( m_CurrentState == STATE_STORE_FADE_TO_BLACK )
        {
            rage::cCommerceUtil *pCommerceUtil = CLiveManager::GetCommerceMgr().GetCommerceUtil();

            if (m_PendingCheckoutProduct != NULL)
            {
                pCommerceUtil->CheckoutProduct(m_PendingCheckoutProduct);
            }
            else
            {
                pCommerceUtil->ProductCodeEntry();        
            }
            
			m_HasBeenInCheckout = true;
            m_CurrentState = STATE_IN_CHECKOUT;

			// NOTE: good alternative position to HideCommerceLogo at end of fade, rather than start. 50/50 what looks nicer. 
        }
     }
 
     if ( methodName == ATSTRINGHASH("FADE_TO_TRANSPARENT_COMPLETED",0x351af647) )
     {
         if ( m_CurrentState == STATE_STORE_FADE_TO_TRANS )
         {
            m_CurrentState = STATE_NEUTRAL;
         }
     }
}

void cStoreMainScreen::UpdateInput()
{
	if (m_ColumnReturnId != 0 || m_ItemReturnId != 0)  // fix for 1912443 - dont request multiple return value requests
	{
		return;
	}

    if (!AreAllAssetsActive())
	{ 
		return;
	}

	if (cStoreScreenMgr::IsExitRequested())
	{
		return;
	}

	if ( m_InNoProductsMode || !m_StoreListsSetup )
	{
		//This input update allows the user access to exit and code entry.
		UpdateNoContentInput();
		return;
	}

    bool highlightChangeRequired = false;
    bool currentSlotRangeCheckRequired = false;

    int axisR = static_cast<s32>(CControlMgr::GetMainFrontendControl().GetFrontendRStickUpDown().GetNorm() * 128.0f);

    if ( RIGHT_STICK_DEADZONE < Abs(axisR) )
    {
        if ( CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_ANALOG_STICK_INPUT") )
        {
            CScaleformMgr::AddParamBool(false);
            CScaleformMgr::AddParamInt(0);
            CScaleformMgr::AddParamInt(axisR);
            CScaleformMgr::EndMethod();
        }
    }
       
    if ( m_CurrentStartReturnId == 0 && CPauseMenu::CheckInput(FRONTEND_INPUT_UP,false,CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE) )
    {
        if ( CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_INPUT_EVENT") )
        {
            CScaleformMgr::AddParamInt(DPADUP);
            CScaleformMgr::EndMethod();
        }

        if ( m_LastColumnIndex == ITEM_COLUMN_INDEX )
        {	
			currentSlotRangeCheckRequired = true;	
            m_LastSelectionTransition = ITEM_TO_ITEM;
        }
        else if ( m_LastColumnIndex == CAT_COLUMN_INDEX )
        {
            m_LastSelectionTransition = CAT_TO_CAT;
        }
        else
        {
            AssertMsg(false,"invalid column index");
        }

        highlightChangeRequired = true;
    }

    if ( m_CurrentStartReturnId == 0 && CPauseMenu::CheckInput(FRONTEND_INPUT_DOWN,false,CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE) )
    {
        if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_INPUT_EVENT"))
        {
            CScaleformMgr::AddParamInt(DPADDOWN);
            CScaleformMgr::EndMethod();
        }

        if ( m_LastColumnIndex == ITEM_COLUMN_INDEX )
        {
			if (m_LastItemIndex < m_ProductIndexArray.GetCount())
			{
				currentSlotRangeCheckRequired = true;
			}
            m_LastSelectionTransition = ITEM_TO_ITEM;
        }
        else if ( m_LastColumnIndex == CAT_COLUMN_INDEX )
        {
            m_LastSelectionTransition = CAT_TO_CAT;
        }
        else
        {
            AssertMsg(false,"invalid column index");
        }

        highlightChangeRequired = true;
    }

    if ( CPauseMenu::CheckInput(FRONTEND_INPUT_BACK,false,CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE) && CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_INPUT_EVENT_CIRCLE"))
    {
        CScaleformMgr::EndMethod();
        
        if ( m_LastColumnIndex == CAT_COLUMN_INDEX )
        {
            if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SHUTDOWN_MOVIE"))
            {
                CScaleformMgr::EndMethod();
            }

			storeUIDebugf1("UpdateInput :: User requested exit");

            cStoreScreenMgr::RequestExit();
			CPauseMenu::PlaySound("BACK");
        }
        else
        {
            //We are on the item column.
            if ( CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_INPUT_EVENT") )
            {
                CScaleformMgr::AddParamInt(DPADLEFT);
                CScaleformMgr::EndMethod();
            }

            m_LastSelectionTransition = ITEM_TO_CAT;

            highlightChangeRequired = true;
        }
    }

    if ( !m_InNoProductsMode && CPauseMenu::CheckInput(FRONTEND_INPUT_ACCEPT,false,CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE) && !cStoreScreenMgr::IsExitRequested() )
    {
        if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_INPUT_EVENT_CROSS"))
        {
            CScaleformMgr::EndMethod();
        }

        if ( m_LastColumnIndex == CAT_COLUMN_INDEX )
        {
            m_LastSelectionTransition = CAT_TO_ITEM;

            //Set the selected item to be the top
            if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "HIGHLIGHT_ITEM"))
            {
                CScaleformMgr::AddParamInt(ITEM_COLUMN_INDEX);
                CScaleformMgr::AddParamInt(0);
                CScaleformMgr::EndMethod();
            }

            highlightChangeRequired = true;
        }
        else
        {
            //We are on an item
            m_PendingSelection = true;
        }
    }

    if (CPauseMenu::CheckInput(FRONTEND_INPUT_Y,false,CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE) && !cStoreScreenMgr::IsExitRequested() )
    {
//         rage::cCommerceUtil *pCommerceUtil = CLiveManager::GetCommerceMgr().GetCommerceUtil();
// 
//         if (pCommerceUtil)
//         {
//             pCommerceUtil->ProductCodeEntry();        
//         }

        //Start the fade down
        if (m_BlackFadeMovieId != INVALID_MOVIE_ID && CScaleformMgr::BeginMethod( m_BlackFadeMovieId, SF_BASE_CLASS_STORE, "FADE_TO_BLACK" ))
        {
            CScaleformMgr::AddParamInt(STORE_FADE_TIME);
            CScaleformMgr::EndMethod();
        }

        m_PendingCheckoutProduct = NULL;
        m_CurrentState = STATE_STORE_FADE_TO_BLACK;

        if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_INPUT_EVENT_TRIANGLE"))
        {
            CScaleformMgr::EndMethod();
        }

		CPauseMenu::PlaySound("SELECT");

#if RSG_NP
		g_rlNp.GetCommonDialog().HideCommerceLogo();
		m_shouldCommerceLogoDisplay = false;
		m_isCommerceLogoDisplaying = false;
#endif
    }

    if (highlightChangeRequired)
    {
        TriggerHighlightChange();
    }

    if (currentSlotRangeCheckRequired)
    {
        if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "GET_CURRENT_START_INDEX"))
        {
            CScaleformMgr::AddParamInt(ITEM_COLUMN_INDEX);

            m_CurrentStartReturnId = CScaleformMgr::EndMethodReturnValue(m_CurrentStartReturnId);
        }

        if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "GET_CURRENT_END_INDEX"))
        {
            CScaleformMgr::AddParamInt(ITEM_COLUMN_INDEX);

            m_CurrentEndReturnId = CScaleformMgr::EndMethodReturnValue(m_CurrentEndReturnId);
        }
    }
}

void cStoreMainScreen::UpdateNoContentInput()
{
	if ( CPauseMenu::CheckInput(FRONTEND_INPUT_BACK,false,CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE) && CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_INPUT_EVENT_CIRCLE"))
    {
        CScaleformMgr::EndMethod();
        
        if ( m_LastColumnIndex == CAT_COLUMN_INDEX )
        {
            if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SHUTDOWN_MOVIE"))
            {
                CScaleformMgr::EndMethod();
            }

			storeUIDebugf1("UpdateNoContentInput :: User requested exit");
			
            cStoreScreenMgr::RequestExit();
        }
    }

	if (CPauseMenu::CheckInput(FRONTEND_INPUT_Y,false,CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE) && !cStoreScreenMgr::IsExitRequested() )
	{
		//Start the fade down
		if (m_BlackFadeMovieId != INVALID_MOVIE_ID && CScaleformMgr::BeginMethod( m_BlackFadeMovieId, SF_BASE_CLASS_STORE, "FADE_TO_BLACK" ))
		{
			CScaleformMgr::AddParamInt(STORE_FADE_TIME);
			CScaleformMgr::EndMethod();
		}

		m_PendingCheckoutProduct = NULL;
		m_CurrentState = STATE_STORE_FADE_TO_BLACK;

		if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_INPUT_EVENT_TRIANGLE"))
		{
			CScaleformMgr::EndMethod();
		}

		CPauseMenu::PlaySound("SELECT");
	}
}

void cStoreMainScreen::ProcessSelection()
{
    storeUIDebugf3("Selected column %d selection %d",m_LastColumnIndex,m_LastItemIndex);

    rage::cCommerceUtil *pCommerceUtil = CLiveManager::GetCommerceMgr().GetCommerceUtil();

    storeUIAssert( pCommerceUtil );

    if ( pCommerceUtil == NULL )
    {
        return;
    }

    if ( m_LastColumnIndex == DETAIL_COLUMN_INDEX || m_LastColumnIndex == ITEM_COLUMN_INDEX )
    {
        if ( m_LastCenterItemIndex == INVALID_ITEM_INDEX || m_LastCenterItemIndex >= m_ProductIndexArray.GetCount() )
        {
            return;
        }
        const cCommerceItemData* pItemData = pCommerceUtil->GetItemData(m_ProductIndexArray[ m_LastCenterItemIndex ]);

        if ( pItemData != NULL && pItemData->GetItemType() == cCommerceItemData::ITEM_TYPE_PRODUCT )
        {
            const cCommerceProductData *pProductData = static_cast<const cCommerceProductData*>(pItemData);
            ProcessCheckoutSelected(pProductData);
        }            
    }
    else
    {
        //We are on the category column, so fire a dpad right message
        if ( CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_INPUT_EVENT"))
        {
            CScaleformMgr::AddParamInt(DPADRIGHT);
            CScaleformMgr::EndMethod();
            TriggerHighlightChange();
        }
    }
}

void cStoreMainScreen::ProcessHighlightChange()
{
    storeUIDebugf3("ProcessHighlightChange :: Column: %d, Selection: %d", m_LastColumnIndex, m_LastItemIndex);

    if ( m_LastColumnIndex == ITEM_COLUMN_INDEX )
    {
        //Store the item index for later
        m_LastCenterItemIndex = m_LastItemIndex;

        if ( m_LastItemIndex == INVALID_ITEM_INDEX || m_LastItemIndex >= m_ProductIndexArray.GetCount() )
        {
            return;
        }

        const rage::cCommerceUtil *pCommerceUtil = CLiveManager::GetCommerceMgr().GetCommerceUtil();

        storeUIAssert( pCommerceUtil );

        if ( pCommerceUtil == NULL )
        {
            return;
        }

        const cCommerceItemData* pItemData = pCommerceUtil->GetItemData(m_ProductIndexArray[ m_LastItemIndex ]);

        if ( pItemData != NULL && pItemData->GetItemType() == cCommerceItemData::ITEM_TYPE_PRODUCT )
        {
            const cCommerceProductData *pProductData = static_cast<const cCommerceProductData*>(pItemData);
            DisplayItemDetails( pProductData );
        }

        //Set slots with no loaded texture to display a blank.
        for ( s32 iSlots = 0; iSlots < (Min(m_ProductIndexArray.GetCount(), MAX_NUM_ITEMS_IN_MIDDLE_COLUMN) ); iSlots++ )
        {
            int requestedSlotIndex = (m_LastStartIndex + iSlots) % m_ProductIndexArray.GetCount();
            if ( m_LoadedImageIndices.Find( requestedSlotIndex ) == -1 )
            {
                const rage::cCommerceUtil *pCommerceUtil = CLiveManager::GetCommerceMgr().GetCommerceUtil();
                if ( pCommerceUtil )
                {
                    const cCommerceItemData *itemData = pCommerceUtil->GetItemData( m_ProductIndexArray[ requestedSlotIndex ] );
                    PopulateItemSlotWithData((m_LastStartIndex + iSlots) % m_ProductIndexArray.GetCount(), itemData, false);
                }             
            }
        }
    }

    if ( m_LastColumnIndex == CAT_COLUMN_INDEX)
    {
        if ( m_LastItemIndex == INVALID_ITEM_INDEX || m_LastItemIndex >= m_CategoryIndexArray.GetCount() || m_CategoryIndexArray.GetCount() == 1 )
        {
            return;
        }
        
        m_CurrentlySelectedCategory = m_LastItemIndex;

        if ( m_LastSelectionTransition != ITEM_TO_CAT )
        {
            ClearLoadedImages();

            m_PendingListSetup = true;
        }
    }
}

void cStoreMainScreen::DisplayItemDetails( const cCommerceProductData* pProductData  )
{
    rage::cCommerceUtil *pCommerceUtil = CLiveManager::GetCommerceMgr().GetCommerceUtil();

    storeUIAssert( pCommerceUtil );

    if ( pCommerceUtil == NULL )
    {
        return;
    }

    storeUIAssert( pProductData != NULL );
    if ( pProductData == NULL )
    {
        return;
    }

    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_DATA_SLOT_EMPTY"))
    {
        //Plus one because this function seems to count the columns up from one. Charming.
        CScaleformMgr::AddParamInt(DETAIL_COLUMN_INDEX);
        CScaleformMgr::EndMethod();
    }

    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "ADD_COLUMN"))
    {
        //Plus one because this function seems to count the columns up from one. Charming.
        CScaleformMgr::AddParamInt(DETAIL_COLUMN_INDEX);
        CScaleformMgr::AddParamString("DetailsList");
        CScaleformMgr::EndMethod();
    }

    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_COLUMN_TITLE"))
    {
        CScaleformMgr::AddParamInt(2);
        CScaleformMgr::AddParamString(TheText.Get("CMRC_DETAILS"));
        CScaleformMgr::EndMethod();   
    }

	char cashAmountString[CONSUMABLE_AMOUNT_STR_LEN];
	cashAmountString[0] = 0;

	if ( pProductData->IsConsumable() && MoneyInterface::GetCashPackValue(pProductData->GetConsumableId() ) != -1 )
	{		
		CTextConversion::FormatIntForHumans(MoneyInterface::GetCashPackValue(pProductData->GetConsumableId()), cashAmountString, CONSUMABLE_AMOUNT_STR_LEN, "%s");
	}

    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_DATA_SLOT"))
    {
        atString tempString = pProductData->GetName();
		
		if ( pProductData->IsConsumable() && MoneyInterface::GetCashPackValue(pProductData->GetConsumableId() ) != -1 )
		{
			tempString.Replace(CONSUMABLE_AMOUNT_SUB_TAG, cashAmountString);
		}

        CScaleformMgr::AddParamInt(DETAIL_COLUMN_INDEX);
        CScaleformMgr::AddParamInt(0);
        CScaleformMgr::AddParamString(tempString.c_str());
        CScaleformMgr::EndMethod();
    }	

    if ( pCommerceUtil->GetItemLongDescription(pProductData) != NULL )
    {
		atString longDescString(pCommerceUtil->GetItemLongDescription(pProductData));

		if ( pProductData->IsConsumable() && MoneyInterface::GetCashPackValue(pProductData->GetConsumableId() ) != -1 )
		{
			longDescString.Replace(CONSUMABLE_AMOUNT_SUB_TAG, cashAmountString);
		}

        if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_DATA_SLOT"))
        {
            CScaleformMgr::AddParamInt(DETAIL_COLUMN_INDEX);
            CScaleformMgr::AddParamInt(1);
            CScaleformMgr::AddParamString(longDescString);
            CScaleformMgr::EndMethod();
        }

		//We have our long description.
		m_PendingLongDescriptionItem = NULL;
    }
    else
    {
        m_PendingLongDescriptionItem = pProductData;
    }

    if ( pProductData->GetEnumeratedFlag() )
    {
        if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_DESCRIPTION"))
        {
            const int TEMP_PRICE_STRING_LEN = 128;
            char tempPriceString[TEMP_PRICE_STRING_LEN]; 
            atString priceString;
            if ( pCommerceUtil->GetProductPrice(pProductData,tempPriceString,TEMP_PRICE_STRING_LEN) != 0.0f)
            {
                priceString = tempPriceString;
                priceString += PRICE_STRING_SUFFIX;
            }
            
            //char* string = TheText.Get("MKT_AIM_SPD");
             
            char* statusString = NULL;
			bool showPrice = true;

            if ( pCommerceUtil->IsProductInstalled( pProductData ) )
            {
                statusString = TheText.Get("CMRC_INSTALLED");
				showPrice = false;
            }
            else if ( pCommerceUtil->IsProductPurchased( pProductData ) && !pProductData->IsConsumable()  )
            {
                statusString = TheText.Get("CMRC_PRCHSD");
				showPrice = false;
            }
            else
            {
                statusString = TheText.Get("CMRC_AVAIL");
            }

			if ( pProductData->IsConsumable() )
			{
				showPrice = true;
			}

            //Check if we have the data for how many of our friends own this product
            const int *pNumFriendsWhoOwnProduct = m_NumFriendsWhoOwnProductMap.Access(pProductData->GetId() );
            
            if ( pNumFriendsWhoOwnProduct ==  NULL )
            {
                //We dont have the data for this product. Set it as our next request.
                mp_PendingNumFriendsWhoOwnProduct = pProductData;
            }
            else
            {
                //Clear a pending request.
                mp_PendingNumFriendsWhoOwnProduct = NULL;
            }

			if (showPrice)
			{
				CScaleformMgr::AddParamInt(2);
				CScaleformMgr::AddParamString(TheText.Get("CMRC_PRICE"));
            
				if ( priceString.GetLength() == 0 )
				{
					CScaleformMgr::AddParamString(TheText.Get("STORE_FREE_PRICE"));
				}
				else
				{
					CScaleformMgr::AddParamString(priceString.c_str());
				}
			}
			else
			{
				CScaleformMgr::AddParamInt(2);
				priceString.Clear();
				CScaleformMgr::AddParamString(priceString.c_str());
				CScaleformMgr::AddParamString(priceString.c_str());
				
			}

            if ( pNumFriendsWhoOwnProduct )
            {
                CScaleformMgr::AddParamInt(*pNumFriendsWhoOwnProduct);
            }
            else
            {

                CScaleformMgr::AddParamInt(0);
            }

            CScaleformMgr::AddParamString(statusString);

            CScaleformMgr::AddParamInt(0);
            CScaleformMgr::AddParamBool(true);
            CScaleformMgr::AddParamBool(pNumFriendsWhoOwnProduct != NULL);
            CScaleformMgr::AddParamBool(true);
            CScaleformMgr::EndMethod();
        }
    }
    else
    {
        if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_DESCRIPTION"))
        {
            //char* string = TheText.Get("MKT_AIM_SPD");
            CScaleformMgr::AddParamInt(2);
            CScaleformMgr::AddParamString(TheText.Get("CMRC_PRICE"));
            CScaleformMgr::AddParamString("");
            CScaleformMgr::AddParamInt(0);
            CScaleformMgr::AddParamString(TheText.Get("CMRC_AVAIL"));
            CScaleformMgr::AddParamInt(15);
            CScaleformMgr::AddParamBool(false);
            CScaleformMgr::AddParamBool(false);
            CScaleformMgr::AddParamBool(false);
            CScaleformMgr::EndMethod();
        }
    }

    //Setup the status colour
    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_STATUS_COLOURS"))
    {
#if __XENON
        CRGBA bgColour = CHudColour::GetRGBA(HUD_COLOUR_PLATFORM_GREEN);
        CRGBA fgColour = CHudColour::GetRGBA(HUD_COLOUR_BLACK);
#else
        CRGBA bgColour = CHudColour::GetRGBA(HUD_COLOUR_PLATFORM_BLUE);
        CRGBA fgColour = CHudColour::GetRGBA(HUD_COLOUR_WHITE);
#endif  
        CScaleformMgr::AddParamInt(bgColour.GetRed());
        CScaleformMgr::AddParamInt(bgColour.GetGreen());
        CScaleformMgr::AddParamInt(bgColour.GetBlue());

        CScaleformMgr::AddParamInt(fgColour.GetRed());
        CScaleformMgr::AddParamInt(fgColour.GetGreen());
        CScaleformMgr::AddParamInt(fgColour.GetBlue());

        CScaleformMgr::EndMethod();
    }

    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "DISPLAY_DATA_SLOT"))
    {
        CScaleformMgr::AddParamInt(DETAIL_COLUMN_INDEX);
        CScaleformMgr::EndMethod();
    }
}

void cStoreMainScreen::ClearItemDetails( )
{
    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "REMOVE_COLUMN"))
    {
        //Plus one because this function seems to count the columns up from one. Charming.
        CScaleformMgr::AddParamInt(DETAIL_COLUMN_INDEX);
        CScaleformMgr::EndMethod();
    }

    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "ADD_COLUMN"))
    {
        //Plus one because this function seems to count the columns up from one. Charming.
        CScaleformMgr::AddParamInt(DETAIL_COLUMN_INDEX);
        CScaleformMgr::AddParamString("DetailsList");
        CScaleformMgr::EndMethod();
    }

    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_COLUMN_TITLE"))
    {
        CScaleformMgr::AddParamInt(2);
        CScaleformMgr::AddParamString(TheText.Get("CMRC_DETAILS"));
        CScaleformMgr::EndMethod();   
    }

    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_DATA_SLOT"))
    {
        CScaleformMgr::AddParamInt(DETAIL_COLUMN_INDEX);
        CScaleformMgr::AddParamInt(0);
        CScaleformMgr::AddParamString("");
        CScaleformMgr::EndMethod();
    }

    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_DATA_SLOT"))
    {

        CScaleformMgr::AddParamInt(DETAIL_COLUMN_INDEX);
        CScaleformMgr::AddParamInt(1);
        CScaleformMgr::AddParamString("");
        CScaleformMgr::EndMethod();
    }

    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_DESCRIPTION"))
    {
        CScaleformMgr::AddParamInt(2);
        CScaleformMgr::AddParamString("");
        CScaleformMgr::AddParamString("");
        CScaleformMgr::AddParamInt(0);
        CScaleformMgr::AddParamString("");
        CScaleformMgr::AddParamInt(15);
        CScaleformMgr::AddParamBool(false);
        CScaleformMgr::AddParamBool(false);
        CScaleformMgr::AddParamBool(false);
        CScaleformMgr::EndMethod();
    }

    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "DISPLAY_DATA_SLOT"))
    {
        CScaleformMgr::AddParamInt(DETAIL_COLUMN_INDEX);
        CScaleformMgr::EndMethod();
    }
}

void cStoreMainScreen::UpdateContextHelp()
{
	//Because of the various paths into the store, we do not always own the instructional button movie.
	int buttonMovieId = m_ButtonMovieId;

	if ( buttonMovieId == INVALID_MOVIE_ID )
	{
		buttonMovieId = CScaleformMgr::FindMovieByFilename(BUTTONMENU_FILENAME);
	}

	//We couldn't get the movie ID from either source. Leave.
	if ( buttonMovieId == INVALID_MOVIE_ID )
	{
		return;
	}

    if ( buttonMovieId >= 0 )
    {
        if ( CScaleformMgr::BeginMethod(buttonMovieId, SF_BASE_CLASS_GENERIC, "SET_DATA_SLOT_EMPTY"))
        {
            CScaleformMgr::EndMethod();
        }

        if ( CScaleformMgr::BeginMethod(buttonMovieId, SF_BASE_CLASS_GENERIC, "SET_CLEAR_SPACE"))
        {
            CScaleformMgr::AddParamInt(200);
            CScaleformMgr::EndMethod();
        }

		if ( m_InNoProductsMode || cStoreScreenMgr::GetCheckoutInitialItem())
		{
			//We don't want any additional buttons in this case.
		}
        else if ( m_LastColumnIndex == CAT_COLUMN_INDEX )
        {
            if ( CScaleformMgr::BeginMethod(buttonMovieId, SF_BASE_CLASS_GENERIC, "SET_DATA_SLOT"))
            {
                CScaleformMgr::AddParamInt(0);
                CScaleformMgr::AddParamInt(30);
                CScaleformMgr::AddParamString(TheText.Get("CMRC_SELECT"));
                CScaleformMgr::EndMethod();
            }
        }
        else if ( m_LastCenterItemIndex != INVALID_ITEM_INDEX  && m_LastCenterItemIndex < m_ProductIndexArray.GetCount() )
        {
            const rage::cCommerceUtil *pCommerceUtil = CLiveManager::GetCommerceMgr().GetCommerceUtil();

            storeUIAssertf( pCommerceUtil, "Commerce utility was not initialised or present" );

            if ( pCommerceUtil == NULL )
            {
                return;
            }

            const cCommerceItemData* pItemData = pCommerceUtil->GetItemData(m_ProductIndexArray[ m_LastCenterItemIndex ]);

            if ( pItemData != NULL && pItemData->GetItemType() == cCommerceItemData::ITEM_TYPE_PRODUCT )
            {
                const cCommerceProductData *pProductData = static_cast<const cCommerceProductData*>(pItemData);

                if ( pCommerceUtil->IsProductInstalled( pProductData ) )
                {
					if ( CScaleformMgr::BeginMethod(buttonMovieId, SF_BASE_CLASS_GENERIC, "SET_DATA_SLOT"))
					{
						CScaleformMgr::AddParamInt(0);
						CScaleformMgr::AddParamInt(30);
						CScaleformMgr::AddParamString(TheText.Get("CMRC_DLOAD"));
						CScaleformMgr::EndMethod();
					}
                }
                else if ( pCommerceUtil->IsProductPurchased( pProductData )  && !pProductData->IsConsumable() )
                {
                    if ( CScaleformMgr::BeginMethod(buttonMovieId, SF_BASE_CLASS_GENERIC, "SET_DATA_SLOT"))
                    {
                        CScaleformMgr::AddParamInt(0);
                        CScaleformMgr::AddParamInt(30);
                        CScaleformMgr::AddParamString(TheText.Get("CMRC_DLOAD"));
                        CScaleformMgr::EndMethod();
                    }
                }
                else if ( pCommerceUtil->IsProductPurchasable( pProductData ) )
                {
                    if ( CScaleformMgr::BeginMethod(buttonMovieId, SF_BASE_CLASS_GENERIC, "SET_DATA_SLOT"))
                    {
                        CScaleformMgr::AddParamInt(0);
                        CScaleformMgr::AddParamInt(30);
                        CScaleformMgr::AddParamString(TheText.Get("CMRC_PURCHASE"));
                        CScaleformMgr::EndMethod();
                    }
                }
            }
        }
        
		if ( CScaleformMgr::BeginMethod(buttonMovieId, SF_BASE_CLASS_GENERIC, "SET_DATA_SLOT"))
		{
			CScaleformMgr::AddParamInt(1);
			CScaleformMgr::AddParamInt(31);
			CScaleformMgr::AddParamString(TheText.Get("CMRC_BACK"));
			CScaleformMgr::EndMethod();
		}

        // Don't show the "Redeem Code"-button during a quick checkout
        if ( !cStoreScreenMgr::GetCheckoutInitialItem()
            && CScaleformMgr::BeginMethod(buttonMovieId, SF_BASE_CLASS_GENERIC, "SET_DATA_SLOT") )
        {
            CScaleformMgr::AddParamInt(2);
            CScaleformMgr::AddParamInt(33);
            CScaleformMgr::AddParamString(TheText.Get("CMRC_CODE"));
            CScaleformMgr::EndMethod();
        }

        if ( CScaleformMgr::BeginMethod(buttonMovieId, SF_BASE_CLASS_GENERIC, "DRAW_INSTRUCTIONAL_BUTTONS"))
        {            
            CScaleformMgr::EndMethod();
        }

        if ( CScaleformMgr::BeginMethod(buttonMovieId, SF_BASE_CLASS_GENERIC, "SET_BACKGROUND_COLOUR"))
        {
            CScaleformMgr::AddParamInt(0);
            CScaleformMgr::AddParamInt(0);
            CScaleformMgr::AddParamInt(0);
            CScaleformMgr::AddParamInt(80);
            CScaleformMgr::EndMethod();
        }
    }
}

void cStoreMainScreen::PopulateImageRequests()
{
    const rage::cCommerceUtil *pCommerceUtil = CLiveManager::GetCommerceMgr().GetCommerceUtil();

    storeUIAssert( pCommerceUtil );

    if ( pCommerceUtil == NULL )
    {
        return;
    }

    int numRequiredImages = Min( MAX_NUM_ITEMS_IN_MIDDLE_COLUMN, m_ProductIndexArray.GetCount() );

    //create an array of our currently required images
    atArray<int> requiredImageArray;

    for ( int iRequiredImageIt = 0; iRequiredImageIt < numRequiredImages; iRequiredImageIt++ )
    {
        int requiredImageIndex = (m_LastStartIndex + iRequiredImageIt ) % m_ProductIndexArray.GetCount();
        requiredImageArray.PushAndGrow(requiredImageIndex);
    }

    //Check for unloads first.
    bool deletionOccured = false;
    do 
    {
        deletionOccured = false;
        for (s32 iLoadedImageIndices = 0; iLoadedImageIndices < m_LoadedImageIndices.GetCount(); iLoadedImageIndices++)
        {
            if ( requiredImageArray.Find(m_LoadedImageIndices[ iLoadedImageIndices ]) == -1 )
            {
                //We are outside the currently requested range, unload
                const cCommerceItemData *itemData = pCommerceUtil->GetItemData(m_ProductIndexArray[ m_LoadedImageIndices[ iLoadedImageIndices ] ] );

                if ( itemData && itemData->GetImagePaths().GetCount() > 0 )
                {
                    const atString path(itemData->GetImagePaths()[0]);
                    //DO NOT CHECK IN!
                    cStoreScreenMgr::GetTextureManager()->ReleaseTexture( path );
                    PopulateItemSlotWithData(m_LoadedImageIndices[ iLoadedImageIndices ], itemData, false);
                } 
                
                deletionOccured = true;
                m_LoadedImageIndices.Delete( iLoadedImageIndices );
                break;
            }
        }
    } while (deletionOccured);


    //Now do the same check on the requested image array    
    do 
    {
        deletionOccured = false;
        for (s32 iRequestedImageIndices = 0; iRequestedImageIndices < m_RequestedImageIndices.GetCount(); iRequestedImageIndices++)
        {
            if ( requiredImageArray.Find(m_RequestedImageIndices[ iRequestedImageIndices ]) == -1 )
            {
                //We are outside the currently requested range, remove from requests
                m_RequestedImageIndices.Delete( iRequestedImageIndices );                
                deletionOccured = true;
                break;
            }
        }
    } while (deletionOccured);  
       
    for (int s = 0; s < requiredImageArray.GetCount() ; s++ )
    {
        
        bool found = false;
        //Check if the image is already in our downloaded list
        if ( m_LoadedImageIndices.Find(requiredImageArray[s]) != -1 )
        {
            found = true;
        }

        //Check if the image is already in our requested list
        if ( m_RequestedImageIndices.Find(requiredImageArray[s]) != -1 )
        {
            found = true;
        }

        if (!found)
        {
            if ( m_RequestedImageIndices.GetCount() < m_RequestedImageIndices.GetCapacity() )
            {
                //See if the product has an image to request.
                storeUIAssert( requiredImageArray[s] >= 0 && requiredImageArray[s] < m_ProductIndexArray.GetCount() );
                const cCommerceItemData *itemData = pCommerceUtil->GetItemData( m_ProductIndexArray[ requiredImageArray[s] ] );
                if ( itemData && itemData->GetImagePaths().GetCount() > 0 )
                {
                    m_RequestedImageIndices.PushAndGrow(requiredImageArray[s]);
                }               
            }
            else
            {
                for (s32 i = 0; i < m_RequestedImageIndices.GetCount(); i++)
                {
                    storeUIDebugf3("m_RequestedImageIndices[%d]: %d",i,m_RequestedImageIndices[i]);
                }
                storeUIDebugf3("Trying to add index %d",requiredImageArray[s]);
                storeUIAssertf(false,"Too many requests to download images in main store screen");
            }
        }
    }
}

void cStoreMainScreen::UpdateImageRequests()
{
    const rage::cCommerceUtil *pCommerceUtil = CLiveManager::GetCommerceMgr().GetCommerceUtil();

    storeUIAssert( pCommerceUtil );

    if ( pCommerceUtil == NULL )
    {
        return;
    }

    for (int iRequestedImageIter = 0; iRequestedImageIter < m_RequestedImageIndices.GetCount(); iRequestedImageIter++)
    {
        int requestedImageIndex = m_RequestedImageIndices[iRequestedImageIter];
        storeUIAssert( requestedImageIndex >= 0 && requestedImageIndex < m_ProductIndexArray.GetCount() );
        const cCommerceItemData *itemData = pCommerceUtil->GetItemData( m_ProductIndexArray[ requestedImageIndex ] );

        if ( itemData 
            && itemData->GetImagePaths().GetCount() > 0
            && (m_LoadedImageIndices.Find( requestedImageIndex ) == -1)
            && cStoreScreenMgr::GetTextureManager()->RequestTexture( itemData->GetImagePaths()[0] ) )
        {
            storeUIDebugf3("Requested texture %s is now available from DL Tex man. This is slot %d", itemData->GetImagePaths()[0].c_str(), requestedImageIndex );
            //We have our texture. Populate the slot.
            PopulateItemSlotWithData( requestedImageIndex , itemData );
            m_LoadedImageIndices.PushAndGrow(requestedImageIndex);
        }
    }

    //Pass to remove downloaded textures
    for ( int iLoadedImageIter = 0; iLoadedImageIter < m_LoadedImageIndices.GetCount(); iLoadedImageIter++ )
    {
        int loadedImageIndex = m_LoadedImageIndices[ iLoadedImageIter ];
        for (int iRequestedImageIter = 0; iRequestedImageIter < m_RequestedImageIndices.GetCount(); iRequestedImageIter++)
        {
            int requestedImageIndex = m_RequestedImageIndices[iRequestedImageIter];
            if ( loadedImageIndex == requestedImageIndex )
            {
                storeUIDebugf3("Removing slot %d from the requested images array", requestedImageIndex );
                //We have downloaded this slots image. Remove from the request list
                m_RequestedImageIndices.Delete( iRequestedImageIter );
                break;
            }
        }
    }
    
}

void cStoreMainScreen::PopulateItemSlotWithData( int a_Slot, const cCommerceItemData* a_ItemData, bool showImage )
{
    storeUIAssert(a_ItemData);
    if ( a_ItemData == NULL )
    {
        return;
    }

    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_DATA_SLOT"))
    {                 
        CScaleformMgr::AddParamInt(ITEM_COLUMN_INDEX);
        CScaleformMgr::AddParamInt(a_Slot);

        if ( a_ItemData->GetImagePaths().GetCount() > 0 && showImage )
        {
            atString cloudStrippedPath = a_ItemData->GetImagePaths()[0];
            cStoreTextureManager::ConvertPathToTextureName(cloudStrippedPath);
            
            CScaleformMgr::AddParamString( cloudStrippedPath );
        }
        else
        {
            CScaleformMgr::AddParamString("");
        }
        
        CScaleformMgr::EndMethod();   
    }

    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "DISPLAY_DATA_SLOT"))
    {
        CScaleformMgr::AddParamInt(ITEM_COLUMN_INDEX);
        CScaleformMgr::EndMethod();
    }
}

void cStoreMainScreen::ClearLoadedImages()
{
    const rage::cCommerceUtil *pCommerceUtil = CLiveManager::GetCommerceMgr().GetCommerceUtil();

    storeUIAssert( pCommerceUtil );

    if ( pCommerceUtil == NULL )
    {
        return;
    }

    for (s32 iLoadedImageIndices = 0; iLoadedImageIndices < m_LoadedImageIndices.GetCount(); iLoadedImageIndices++)
    {
        //We are outside the currently requested range, unload
        const cCommerceItemData *itemData = pCommerceUtil->GetItemData(m_LoadedImageIndices[iLoadedImageIndices] );

        if ( itemData && itemData->GetImagePaths().GetCount() > 0 )
        {
            const atString path(itemData->GetImagePaths()[0]);
            cStoreScreenMgr::GetTextureManager()->ReleaseTexture( path );
        }                  
    }
    m_LoadedImageIndices.Reset();

} 

void cStoreMainScreen::ClearImageRequests()
{
    storeUIAssert( cStoreScreenMgr::GetTextureManager() );
    
    const rage::cCommerceUtil *pCommerceUtil = CLiveManager::GetCommerceMgr().GetCommerceUtil();

    if ( cStoreScreenMgr::GetTextureManager() == NULL || pCommerceUtil == NULL )
    {
        return;
    }

    for ( int i = 0; i < m_RequestedImageIndices.GetCount(); i++ )
    {
        const cCommerceItemData *itemData = pCommerceUtil->GetItemData(m_ProductIndexArray[i]);

        if ( itemData && itemData->GetImagePaths().GetCount() > 0 )
        {
            const atString path(itemData->GetImagePaths()[0]);
            cStoreScreenMgr::GetTextureManager()->CancelRequest( path );
        }
    }

    m_RequestedImageIndices.Reset();
    m_RequestedImageIndices.Reserve( MAX_REQUESTED_IMAGES );
}

void cStoreMainScreen::TriggerHighlightChange()
{
    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "GET_CURRENT_COLUMN"))
    {
        m_ColumnReturnId = CScaleformMgr::EndMethodReturnValue(m_ColumnReturnId);
    }

    if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "GET_CURRENT_SELECTION"))
    {
        m_ItemReturnId = CScaleformMgr::EndMethodReturnValue(m_ItemReturnId);
    }

    m_PendingHighlightChange = true;
}

void cStoreMainScreen::UpdatePendingInitialData()
{
    if ( CLiveManager::GetCommerceMgr().HasValidData() )
    {
        m_CurrentState = STATE_INITIALISING;
    }

    //Error checking should go here for failed commerce data
}

void cStoreMainScreen::RequestScaleformMovies()
{
	int buttonMovieId = CScaleformMgr::FindMovieByFilename(BUTTONMENU_FILENAME);
    if ( buttonMovieId == -1 )
    {
        m_ButtonMovieId = CScaleformMgr::CreateMovie(BUTTONMENU_FILENAME, Vector2(0,0), Vector2(1.0f, 1.0f));
		CBusySpinner::RegisterInstructionalButtonMovie(m_ButtonMovieId);  // register this "instructional button" movie with the spinner system
    }
	else
	{
		//The instructional movie is already loaded. Take this opportunity to empty it
		if ( CScaleformMgr::BeginMethod(buttonMovieId, SF_BASE_CLASS_GENERIC, "SET_DATA_SLOT_EMPTY"))
		{
			CScaleformMgr::EndMethod();
		}

		if ( CScaleformMgr::BeginMethod(buttonMovieId, SF_BASE_CLASS_GENERIC, "CLEAR_ALL"))
		{
			CScaleformMgr::EndMethod();
		}

		
	}

    m_StoreBgMovieId = CScaleformMgr::CreateMovie(BACKGROUNDMOVIE_FILENAME, Vector2(0,0), Vector2(1.0f, 1.0f));


    m_BlackFadeMovieId = CScaleformMgr::CreateMovie(BLACKFADE_FILENAME, Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f));

    Vector2 pos(cStoreScreenMgr::GetMovieXPos(), cStoreScreenMgr::GetMovieYPos());
    Vector2 size(cStoreScreenMgr::GetMovieWidth(), cStoreScreenMgr::GetMovieHeight());

    m_StoreMovieId = CScaleformMgr::CreateMovie(STOREMENU_FILENAME, pos, size);  // this movie has seperate values for 4:3 and 16:9 as it wont fit inside safezone of 4:3
    
    m_CurrentState = STATE_LOADING_ASSETS;
}

void cStoreMainScreen::UpdateLoadingAssets()
{
    if (AreAllAssetsActive())
    {
        //DISPLAY THE "LOADING DATA" dialog here
        m_CurrentState = STATE_PENDING_INITIAL_DATA;
    }
}

void cStoreMainScreen::SetBgScrollSpeed( int speed )
{
    if ( m_StoreMovieId != INVALID_MOVIE_ID )
    {
        if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "ANIMATE_BACKGROUND"))
        {
            CScaleformMgr::AddParamInt(speed);
            CScaleformMgr::EndMethod();
        }
    }
}

void cStoreMainScreen::OnMembershipGained()
{
    storeUIDebugf1("OnMembershipGained :: Flagging List Refresh");
    m_PendingListSetup = true;
}

void cStoreMainScreen::AddProductsPriceToCashSpentStat( const cCommerceProductData* pProductData )
{
    const rage::cCommerceUtil *pCommerceUtil = CLiveManager::GetCommerceMgr().GetCommerceUtil();

    if (pCommerceUtil == NULL)
    {
        commerceAssertf(false, "AddProductsPriceToCashSpentStat :: Commerce util does not exist");
        return;
    }

    char priceBuffer[SIZE_OF_PRICE_BUFFER];
    float priceOfRequestedProduct = pCommerceUtil->GetProductPrice(pProductData,priceBuffer,SIZE_OF_PRICE_BUFFER);
    StatsInterface::IncrementStat(STAT_MPPLY_STORE_MONEY_SPENT, priceOfRequestedProduct);

	//THIS IS A VERY BAD NETWORK HEAVY FUNCTION! DO NOT CALL A LOT!!!!!
	StatsInterface::TryMultiplayerSave( STAT_SAVETYPE_STORE );
}

bool cStoreMainScreen::UpdateWarningScreen()
{
	if (m_WasInCheckout)
	{
		//Don't want to pull the rug out from under container functions.
		return true;
	}

    if (cStoreScreenMgr::IsExitRequested())
    {
        return true;
    }

	if (sm_bPlayerHasJustSignedOut)
	{
		//We just leave for this now
		if (m_StoreMovieId != INVALID_MOVIE_ID && CScaleformMgr::IsMovieActive(m_StoreMovieId) && CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SHUTDOWN_MOVIE"))
		{
			CScaleformMgr::EndMethod();
		}

		if (m_CurrentDCWarningScreen != STORE_WARNING_NONE)
			CWarningScreen::Remove();

		storeUIDebugf1("UpdateWarningScreen :: Requesting exit after sign out");

		m_CurrentDCWarningScreen = STORE_WARNING_NONE;
		cStoreScreenMgr::RequestExit();

        return false;
	}

    //Sign out messages don't come through immediately, so we need to delay the offline check briefly.
    if (!CLiveManager::IsOnline() || !CLiveManager::IsOnlineRos() )
    {
        m_FramesOffline++;
    }
    else
    {
        m_FramesOffline = 0;
    }

	const rlGamerInfo* pGamerInfo = NetworkInterface::GetActiveGamerInfo();
    if ( (m_FramesOffline > OFFLINE_FRAMES_TO_WAIT_FOR_SIGNOUT_MESSAGE) 
		|| CLiveManager::GetCommerceMgr().GetCommerceUtil()->IsInErrorState() 	
		|| (pGamerInfo && pGamerInfo->IsOnline() && !rlGamerInfo::HasStorePrivileges(pGamerInfo->GetLocalIndex()))
		|| m_CurrentDCWarningScreen != STORE_WARNING_NONE )
    {
        CBusySpinner::Off( SPINNER_SOURCE_STORE_LOADING );
               
		if ( STORE_WARNING_NONE == m_CurrentDCWarningScreen )
		{
			//Since this is the first display, take this opportunity to show debug output identifying the cause.
			storeUIDisplayf("Displaying store DC message. Cause: CLiveManager::IsOnlineRos() [%d] CLiveManager::IsOnline() [%d], CLiveManager::GetCommerceMgr().GetCommerceUtil()->IsInErrorState() [%d], rlGamerInfo::HasStorePrivileges(pGamerInfo->GetLocalIndex() [%d]",
							CLiveManager::IsOnlineRos(), CLiveManager::IsOnline(), CLiveManager::GetCommerceMgr().GetCommerceUtil()->IsInErrorState(), rlGamerInfo::HasStorePrivileges(pGamerInfo->GetLocalIndex()) );
		
			if ( CLiveManager::IsOnline() && pGamerInfo && !rlGamerInfo::HasStorePrivileges(pGamerInfo->GetLocalIndex()) )
			{
				m_CurrentDCWarningScreen = STORE_WARNING_AGE;
			}
			else if (!CLiveManager::IsOnline() && m_WasSignedOnlineOnEnter)
			{
				m_CurrentDCWarningScreen = STORE_WARNING_PLATFORM_LOST;
			}
			else if (!CLiveManager::IsOnline())
			{
				m_CurrentDCWarningScreen = STORE_WARNING_PLATFORM_OFFLINE;
			}
			else
			{
				m_CurrentDCWarningScreen = STORE_WARNING_ROS;
			}
		}
		
		atHashWithStringBank message;
		switch(m_CurrentDCWarningScreen)
		{
		case(STORE_WARNING_PLATFORM_OFFLINE):
			message = "STORE_PLAT_OFFLINE";
			break;
		case(STORE_WARNING_PLATFORM_LOST):
			message = "STORE_PLAT_LOS";
			break;
		case(STORE_WARNING_ROS):
			message = "STORE_UNAVAIL_ROS";
			break;
		case(STORE_WARNING_AGE):
			message = "HUD_AGERES";
			break;
		default:
			message = "STORE_UNAVAIL";
		}

		CWarningScreen::SetMessage( WARNING_MESSAGE_STANDARD, message, FE_WARNING_OK);

        if(CWarningScreen::CheckAllInput() == FE_WARNING_OK)
        {
            if (m_StoreMovieId != INVALID_MOVIE_ID && CScaleformMgr::IsMovieActive(m_StoreMovieId) && CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SHUTDOWN_MOVIE"))
            {
                CScaleformMgr::EndMethod();
            }

			storeUIDebugf1("UpdateWarningScreen :: Requesting exit after error");
			
			m_CurrentDCWarningScreen = STORE_WARNING_NONE;
            cStoreScreenMgr::RequestExit();
        }

        return false;
    }

    // if we've received an invite, that's not confirmed and we are not suppressed by script
//	static bool simulateInvite = false; 
    if ( ( CLiveManager::GetInviteMgr().HasPendingAcceptedInvite() && !CLiveManager::GetInviteMgr().HasConfirmedOrIsAutoConfirm() && !CLiveManager::GetInviteMgr().IsSuppressed())/* || simulateInvite */)
	{
		if (CBusySpinner::IsOn())
		{
			CBusySpinner::Off( SPINNER_SOURCE_STORE_LOADING );
		}

		CWarningScreen::SetMessage( WARNING_MESSAGE_STANDARD, CLiveManager::GetInviteMgr().GetConfirmText(), FE_WARNING_OK_CANCEL, false, -1, CLiveManager::GetInviteMgr().GetConfirmSubstring());
		eWarningButtonFlags result = CWarningScreen::CheckAllInput();

		if (result == FE_WARNING_OK)
		{
			if (m_StoreMovieId != INVALID_MOVIE_ID && CScaleformMgr::IsMovieActive(m_StoreMovieId) && CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SHUTDOWN_MOVIE"))
			{
				CScaleformMgr::EndMethod();
			}

			storeUIDebugf1("UpdateWarningScreen :: Requesting exit after invite");
			
			cStoreScreenMgr::RequestExit();

			//Since we have accepted an invite, we don't want to leave via the pause menu
			cStoreScreenMgr::SetReturnToPauseMenu(false);

			//Confirm invite
            CLiveManager::GetInviteMgr().AutoConfirmInvite();
//			simulateInvite = false;
		}
		else if (result == FE_WARNING_CANCEL)
		{
			CLiveManager::GetInviteMgr().CancelInvite();
//			simulateInvite = false;
		}

		return false;
	}

    return true;
}


bool cStoreMainScreen::UpdateNonCriticalWarningScreen()
{
	if (cStoreScreenMgr::IsExitRequested())
	{
		return true;
	}

	if ( m_CurrentDCWarningScreen != STORE_WARNING_NONE )
	{
		return true;
	}

	//Structured like this so that we can easily add new messages later.
	if ( m_CurrentNonCriticalWarningScreen == STORE_NON_CRIT_WARNING_NONE )
	{
		switch(m_PendingNonCriticalWarningScreen)
		{
		case STORE_NON_CRIT_WARNING_NONE:
            break;

        case STORE_CANNOT_PURCHASE_CASH:
			storeUIDisplayf("Setting m_CurrentNonCriticalWarningScreen to STORE_CANNOT_PURCHASE_CASH");
			m_CurrentNonCriticalWarningScreen = STORE_CANNOT_PURCHASE_CASH;
            m_PendingNonCriticalWarningScreen = STORE_NON_CRIT_WARNING_NONE;
            break;

		case STORE_SUBSCRIPTION_UPSELL:
			storeUIDisplayf("Setting m_CurrentNonCriticalWarningScreen to STORE_SUBSCRIPTION_UPSELL");
			m_CurrentNonCriticalWarningScreen = STORE_SUBSCRIPTION_UPSELL;
			m_PendingNonCriticalWarningScreen = STORE_NON_CRIT_WARNING_NONE;
            break;

		case STORE_SUBSCRIPTION_WELCOME:
			storeUIDisplayf("Setting m_CurrentNonCriticalWarningScreen to STORE_SUBSCRIPTION_WELCOME");
			m_CurrentNonCriticalWarningScreen = STORE_SUBSCRIPTION_WELCOME;
			m_PendingNonCriticalWarningScreen = STORE_NON_CRIT_WARNING_NONE;
			break;
		}	
	}
	

	if ( m_CurrentNonCriticalWarningScreen != STORE_NON_CRIT_WARNING_NONE )
	{
		atHashWithStringBank header;
		atHashWithStringBank message;
        eWarningButtonFlags buttons = FE_WARNING_OK;
		switch( m_CurrentNonCriticalWarningScreen )
		{
		case(STORE_CANNOT_PURCHASE_CASH):
            header = 0u;
			message = "STORE_CASH_PRODS_DISABLED";	
			break;
		case(STORE_SUBSCRIPTION_UPSELL):
            header = "STORE_PS_SUBSCRIBE";
			message = "STORE_PS_UPSELL";
            buttons = FE_WARNING_YES_NO;
			break;
		case(STORE_SUBSCRIPTION_WELCOME):
            header = "ALERT_MEMBER_CONGRATS";
			message = "ALERT_MEMBER_CONGRATS_MESSAGE";
			break;
		default:
			storeUIAssertf(false,"Non implemented non critical message.");
		}

		CWarningScreen::SetMessageWithHeader( WARNING_MESSAGE_STANDARD, header, message, buttons);

        const eWarningButtonFlags buttonPressed = CWarningScreen::CheckAllInput();
		if(buttonPressed == FE_WARNING_OK || buttonPressed == FE_WARNING_YES)
		{
			switch (m_CurrentNonCriticalWarningScreen)
			{
			case(STORE_CANNOT_PURCHASE_CASH):
				break;
			case(STORE_SUBSCRIPTION_UPSELL):
                if(storeUIVerifyf(!m_SubscriptionUpsellItemId.empty(), "Invalid Upsell Item!"))
                {
					cCommerceUtil* pCommerceUtil = CLiveManager::GetCommerceMgr().GetCommerceUtil();
                    const cCommerceItemData* pUpsellItem = pCommerceUtil->GetItemDataByItemId(m_SubscriptionUpsellItemId.c_str());

                    if(pUpsellItem && pUpsellItem->GetItemType() == cCommerceItemData::ITEM_TYPE_PRODUCT)
                    {
						const cCommerceProductData* pUpsellProduct = static_cast<const cCommerceProductData*>(pUpsellItem);

						// get upsells
						const atArray<atString>& subscriptionUpsells = pUpsellProduct->GetSubscriptionUpsells();
						const unsigned numUpsells = subscriptionUpsells.GetCount();

						// need to find our subscription product
						const unsigned numItems = pCommerceUtil->GetNumItems();

						for(unsigned i = 0; i < numItems; i++)
						{
							const cCommerceItemData* pItemData = pCommerceUtil->GetItemData(i);
							if(pItemData == nullptr)
								continue;

							if(pItemData->GetItemType() != cCommerceItemData::ITEM_TYPE_PRODUCT)
								continue;

							const cCommerceProductData* pOtherProductData = static_cast<const cCommerceProductData*>(pItemData);
							if(!pOtherProductData->IsConsumable())
								continue;

							// check for our upsell in the list
							for(int d = 0; d < numUpsells; d++)
							{
								if(subscriptionUpsells[d] == pOtherProductData->GetConsumableId())
		{
									// found our subscription upsell
									storeUIDebugf1("ProcessCheckoutSelected :: SelectedProduct: %s, UpsellProduct: %s, SubscriptionId: %s",
                                        pUpsellProduct->GetName().c_str(),
										pOtherProductData->GetName().c_str(),
                                        subscriptionUpsells[d].c_str());

                                    // this starts a fade before the checkout begins
									StartCheckout(pOtherProductData);

                                    // suppress membership updates for script from here
                                    MEMBERSHIP_ONLY(CLiveManager::SetSuppressMembershipForScript(true));

                                    // found our product, bail
									break;
								}
							}
						}
                    }
                }
				break;
            case(STORE_SUBSCRIPTION_WELCOME):
			default:
                break;
			}

			m_CurrentNonCriticalWarningScreen = STORE_NON_CRIT_WARNING_NONE;
		}

		return false;
	}
	 
	return true;
}

void cStoreMainScreen::UpdateLeaderBoardDataFetch()
{
    //Do we have a fetch in progress? Is it done?
    if ( mp_CurrentLeaderboardRead && mp_CurrentLeaderboardRead->Finished())
    {
        if ( mp_CurrentLeaderboardRead->Suceeded() )
        {
            //Get the total.
            int friendsOwningThisProduct = 0;
            for ( int s = 0; s < mp_CurrentLeaderboardRead->GetNumRows(); s++ )
            {
                if ( mp_CurrentLeaderboardRead->GetRow(s)->m_ColumnValues[0].Int64Val == 1 )
                {
                    friendsOwningThisProduct++;
                }
            }

            int *existingTotal = m_NumFriendsWhoOwnProductMap.Access( m_CurrentLeaderboardDataFetchProductId );

            if ( existingTotal )
            {
                *existingTotal = friendsOwningThisProduct;
            }
            else
            {
                m_NumFriendsWhoOwnProductMap.Insert(m_CurrentLeaderboardDataFetchProductId,friendsOwningThisProduct);
            }
        }
        else
        {
            //Not sure what to do in this case.   
        }

        //Clean up.
        m_CurrentLeaderboardDataFetchProductId.Reset();
        CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr().ClearLeaderboardRead(mp_CurrentLeaderboardRead);
        mp_CurrentLeaderboardRead = NULL;

        //Check if we have fulfilled the current request
        if ( mp_PendingNumFriendsWhoOwnProduct )
        {
            const int *currentRequestFriendsOwned = m_NumFriendsWhoOwnProductMap.Access(mp_PendingNumFriendsWhoOwnProduct->GetId());

            if ( currentRequestFriendsOwned )
            {
                DisplayItemDetails(mp_PendingNumFriendsWhoOwnProduct);
                mp_PendingNumFriendsWhoOwnProduct = NULL;
            }
        }
    }

    

    //Are we not reading, and do we have an outstanding query?
    if ( mp_CurrentLeaderboardRead == NULL && mp_PendingNumFriendsWhoOwnProduct != NULL && rlFriendsManager::GetTotalNumFriends(NetworkInterface::GetLocalGamerIndex()) > 0 )
    {
        const int COMMERCE_LEADERBOARD_ID = 927;
        const Leaderboard* lbConf = GAME_CONFIG_PARSER.GetLeaderboard(COMMERCE_LEADERBOARD_ID);
        if (lbConf == NULL)
        {
            return;
        }

        //Build our group selector
        rlLeaderboard2GroupSelector groupSelector;
        groupSelector.m_NumGroups = 1;
        strncpy(groupSelector.m_Group[0].m_Category,"ProductId",RL_LEADERBOARD2_CATEGORY_MAX_CHARS);
        strncpy(groupSelector.m_Group[0].m_Id,mp_PendingNumFriendsWhoOwnProduct->GetId(),RL_LEADERBOARD2_GROUP_ID_MAX_CHARS);

        //I am currently assuming these following two are empty params. Will consult.
        rlLeaderboard2GroupSelector groupSelectors[MAX_LEADERBOARD_READ_GROUPS];
        int numberOfGroups = 0;

        const u32 LB_MAX_NUM_FRIENDS = 100;

        CNetworkReadLeaderboards& readmgr = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr();

        //For testing
        const bool includeMyself = false;

        mp_CurrentLeaderboardRead = readmgr.ReadByFriends( COMMERCE_LEADERBOARD_ID, RL_LEADERBOARD2_TYPE_GROUP_MEMBER, groupSelector,
                                                                   RL_INVALID_CLAN_ID, numberOfGroups, groupSelectors, includeMyself, 0, LB_MAX_NUM_FRIENDS);

        if ( mp_CurrentLeaderboardRead == NULL )
        {
            mp_PendingNumFriendsWhoOwnProduct = NULL;
            return;
        }

        //Fetch started correctly.
        m_CurrentLeaderboardDataFetchProductId = mp_PendingNumFriendsWhoOwnProduct->GetId();
    }
}


void cStoreMainScreen::OnPresenceEvent(const rlPresenceEvent* evt)
{
	if (evt)
	{
		if (evt->GetId() == PRESENCE_EVENT_SIGNIN_STATUS_CHANGED)
		{
			const rlPresenceEventSigninStatusChanged* s = evt->m_SigninStatusChanged;

			// check if this is a sign out event, only consider the currently active profile
			if(s && ( s->SignedOut() || s->SignedOffline() ) && ( s->m_GamerInfo.GetLocalIndex() == CControlMgr::GetMainPlayerIndex() ) )
			{
				storeUIDisplayf("Main profile (%d) signed out", s->m_GamerInfo.GetLocalIndex());
				sm_bPlayerHasJustSignedOut = true;
			}
		}
	}
}

void cStoreMainScreen::DoEmptyStoreCheck()
{
	const rage::cCommerceUtil *pCommerceUtil = CLiveManager::GetCommerceMgr().GetCommerceUtil();

	storeUIAssert(pCommerceUtil);

	if ( pCommerceUtil == NULL )
	{
		return;
	}

	int numActiveProducts = 0;
	//Loop through the categories
	for ( int iCat = 0; iCat < m_CategoryIndexArray.GetCount(); iCat++ )
	{
		//GTA only supports one level of categories
		const cCommerceItemData *pItemData = pCommerceUtil->GetItemData( m_CategoryIndexArray[iCat] );
		if(pItemData && pItemData->GetItemType() == cCommerceItemData::ITEM_TYPE_CATEGORY)
		{
			const cCommerceCategoryData *pCategoryData = static_cast<const cCommerceCategoryData*>(pItemData);

			for (int iProds = 0; iProds < pCategoryData->GetNumProducts(); iProds++ )
			{
				int itemIndex = pCategoryData->GetItemIndex(iProds);

				const cCommerceItemData *pCatItemToCheck = pCommerceUtil->GetItemData(itemIndex);

				//Check if this is a product
				if ( pCatItemToCheck->GetItemType() == cCommerceItemData::ITEM_TYPE_PRODUCT )
				{
					numActiveProducts++;
				}
			}
		}
	}


	storeUIDebugf3("Found %d valid products", numActiveProducts);

	if ( numActiveProducts == 0 )
	{
		m_InNoProductsMode = true;
		DisplayNoProductsWarning();
		UpdateContextHelp();
	}
	else
	{
		m_InNoProductsMode = false;
	}
}

void cStoreMainScreen::DisplayNoProductsWarning()
{
	if ( m_StoreMovieId == SF_INVALID_MOVIE )
	{
		return;
	}

	if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "DISPLAY_ERROR_MESSAGE"))
	{
		CScaleformMgr::AddParamString("");
		CScaleformMgr::AddParamString(TheText.Get("STORE_NO_CONTENT"));

		CScaleformMgr::EndMethod();
	}
}

bool cStoreMainScreen::DoQuickCheckout()
{
	if (!cStoreScreenMgr::GetCheckoutInitialItem())
	{
		return false;
	}

	const rage::cCommerceItemData* pInitialItemData = nullptr;
	rage::cCommerceUtil *pCommerceUtil = CLiveManager::GetCommerceMgr().GetCommerceUtil();

	if (pCommerceUtil == nullptr || pCommerceUtil->IsInErrorState())
	{
		cStoreScreenMgr::SetCheckoutInitialItem(false);
		return false;
	}

	const int num = pCommerceUtil->GetNumItems();

	for (int i = 0; i < num; i++)
	{
		if (pCommerceUtil->GetItemData(i)->GetId() == cStoreScreenMgr::GetInitialItemID())
		{
			//we have found our initial product
			pInitialItemData = pCommerceUtil->GetItemData(i);
			break;
		}
	}

	if (pInitialItemData != nullptr && pInitialItemData->GetItemType() == cCommerceItemData::ITEM_TYPE_PRODUCT)
	{
		const cCommerceProductData *pProductData = static_cast<const cCommerceProductData*>(pInitialItemData);
		if (ProcessCheckoutSelected(pProductData))
		{
			storeUIDebugf1("DoQuickCheckout: ItemId: %s", cStoreScreenMgr::GetInitialItemID().c_str());
			m_QuickCheckout = true;
			return true;
		}
	}

	storeUIWarningf("DoQuickCheckout :: Skipped - %s not found", cStoreScreenMgr::GetInitialItemID().c_str());
	cStoreScreenMgr::SetCheckoutInitialItem(false);
	return false;
}

void cStoreMainScreen::UpdateCheckout()
{
	rage::cCommerceUtil *pCommerceUtil = CLiveManager::GetCommerceMgr().GetCommerceUtil();

	if (pCommerceUtil != nullptr && pCommerceUtil->IsInCheckout())
	{
		return;
	}

	if (m_QuickCheckout)
	{
		ExitQuickCheckout();
	}
	else
	{
		//Start the fade down
		if (CScaleformMgr::BeginMethod(m_BlackFadeMovieId, SF_BASE_CLASS_STORE, "FADE_TO_TRANSPARENT"))
		{
			CScaleformMgr::AddParamInt(STORE_FADE_TIME);
			CScaleformMgr::EndMethod();
		}
		m_CurrentState = STATE_STORE_FADE_TO_TRANS;

#if RSG_NP
		m_isCommerceLogoDisplaying = g_rlNp.GetCommonDialog().ShowCommerceLogo(rlNpCommonDialog::COMMERCE_LOGO_POS_LEFT);
		m_shouldCommerceLogoDisplay = true;
#endif
	}
}

void cStoreMainScreen::ExitQuickCheckout()
{
	if (cStoreScreenMgr::IsExitRequested())
	{
		return;
	}

	cStoreScreenMgr::SetCheckoutInitialItem(false);
	m_QuickCheckout = false;

	if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SHUTDOWN_MOVIE"))
	{
		CScaleformMgr::EndMethod();
	}

	storeUIDebugf1("ExitQuickCheckout :: Requesting exit");
	cStoreScreenMgr::RequestExit();
}

bool cStoreMainScreen::ProcessCheckoutSelected(const cCommerceProductData *pProductData)
{
    // reset this here in all paths to be sure
    m_SubscriptionUpsellItemId.Clear();
    m_SubscriptionUpsellTargetItemId.Clear();

    if(pProductData == nullptr)
        return false;

	cCommerceUtil* pCommerceUtil = CLiveManager::GetCommerceMgr().GetCommerceUtil();
    if(pCommerceUtil == nullptr)
        return false;

    // check if we need to show an upsell instead of the actual product
    // - true if we have any missing upsells tagged on the product
    // - true if we have any upsells (fulfilled or not) and have -commerceAlwaysShowUpsell
    const bool showSubscriptionUpsell = pCommerceUtil->HasMissingSubscriptionUpsells(pProductData) 
        NOTFINAL_ONLY(|| (pCommerceUtil->HasSubscriptionUpsell(pProductData) && PARAM_commerceAlwaysShowUpsell.Get()));

	if(showSubscriptionUpsell)
	{		
        // show a warning screen that an upsell is required for this product and offer an upsell option
		// cache the product so that we can return to this in the menu once the purchase is complete
		m_PendingNonCriticalWarningScreen = STORE_SUBSCRIPTION_UPSELL;

        // store the upsell item
		m_SubscriptionUpsellItemId = pProductData->GetId();

		// an upsell product is a dummy entry for an existing product - get to the actual itemId by removing 
        // the upsell portion of the described itemId
		int indexOfSplit = pProductData->GetId().IndexOf('_');
		if(indexOfSplit > 0)
		{
            // step forward 
            m_SubscriptionUpsellTargetItemId = &pProductData->GetId().c_str()[++indexOfSplit];      
		}

#if RSG_OUTPUT
        const cCommerceItemData* targetItemData = pCommerceUtil->GetItemDataByItemId(m_SubscriptionUpsellTargetItemId.c_str());
        storeUIAssert(targetItemData != nullptr);
#endif

		storeUIDebugf1("ProcessCheckoutSelected :: Showing upsell - Product: %s - %s, Target: %s - %s", 
            pProductData->GetName().c_str(), 
            pProductData->GetId().c_str(),
            targetItemData ? targetItemData->GetName().c_str() : "Invalid",
			targetItemData ? targetItemData->GetId().c_str() : "Invalid");

		// indicate success
		return true;
	}
    else if(pProductData->GetEnumeratedFlag())
	{
        storeUIDebugf1("ProcessCheckoutSelected :: Calling CheckoutProduct on item %s", pProductData->GetId().c_str());

		//Check we are allowed to purchase cash.
		if (pProductData->IsConsumable() &&
			(CLiveManager::GetCommerceMgr().GetConsumableManager()->IsOwnershipDataDirty() || !CLiveManager::GetCommerceMgr().GetConsumableManager()->IsOwnershipDataPopulated()))
		{
			storeUIDisplayf("User is attempting to purchase another consumable before the value of the previous cosumable has been assessed.");
			return false;
		}
		else if (pProductData->IsConsumable() && !IsUserAllowedToPurchaseCash(pProductData->GetConsumableId()))
		{
			storeUIDisplayf("Setting m_NeedToDisplayCashProductsDisabledMessage to true.");
			m_PendingNonCriticalWarningScreen = STORE_CANNOT_PURCHASE_CASH;
			return false;
		}
		else if (m_ConsumableProductCheckId.GetLength() > 0)
		{
			//We are waiting for info on a previously purchased cash product. Don't allow another cash purchase until we have updated. 
			return false;
		}

		if (!pCommerceUtil->IsProductPurchased(pProductData) || pProductData->IsConsumable())
		{
			m_PurchaseCheckProductID = pProductData->GetId();
			CNetworkTelemetry::GetPurchaseMetricInformation().AddProductId(m_PurchaseCheckProductID);
		}

        // start the checkout process
        StartCheckout(pProductData);

        return true; 
    }

    return false;
}

bool cStoreMainScreen::StartCheckout(const cCommerceProductData* pProductData)
{
	if(pProductData != nullptr)
	{
		storeUIDebugf1("StartCheckout :: ProductName: %s, ProductId: %s", pProductData->GetName().c_str(), pProductData->GetId().c_str());
		
		m_PendingCheckoutProduct = pProductData;

		//Start the fade down
		if (CScaleformMgr::BeginMethod(m_BlackFadeMovieId, SF_BASE_CLASS_STORE, "FADE_TO_BLACK"))
		{
			CScaleformMgr::AddParamInt(STORE_FADE_TIME);
			CScaleformMgr::EndMethod();
		}

		m_CurrentState = STATE_STORE_FADE_TO_BLACK;

#if RSG_NP
		g_rlNp.GetCommonDialog().HideCommerceLogo();
		m_shouldCommerceLogoDisplay = false;
		m_isCommerceLogoDisplaying = false;
#endif
		return true;
	}
	return false;
}

bool cStoreMainScreen::IsUserAllowedToPurchaseCash(const char* currentConsumableId)
{
	if ( !cStoreScreenMgr::AreCashProductsAllowed() )
	{
		return false;
	}

	bool currentConsumableHandled = false;

	atString currentSelectedIDString(currentConsumableId);

	if (currentConsumableId == NULL)
	{
		currentConsumableHandled = true;
	}

	MoneyInterface::CashProductBatches batchesPending;
	if ( !CLiveManager::GetCommerceMgr().GetConsumableManager()->IsOwnershipDataDirty() && CLiveManager::GetCommerceMgr().GetConsumableManager()->IsOwnershipDataPopulated() )
	{	
		const CCashProductsData::CCashProductsList& productsList = CashPackInfo::GetInstance().GetProductsList();
		for (int i = 0; i < productsList.GetCount(); i++ )
		{
			atString packName;

			if ( !productsList[i].GetSKUPackName(packName) )
			{
				continue;
			}

			int consumableLevel = CLiveManager::GetCommerceMgr().GetConsumableManager()->GetConsumableLevel( packName.c_str() );
			if ( consumableLevel > 0 )
			{
				MoneyInterface::CashProductBatch batch;
				batch.m_Quantity = consumableLevel;
				
				//See if this is our currently selected ID, and if so bump the quantity by one and mark as handled.
				if ( packName == currentSelectedIDString  )
				{
					batch.m_Quantity++;
					currentConsumableHandled = true;
				}
				
				batch.m_id.SetFromString(packName);
				batchesPending.PushAndGrow(batch);
			}
		}
	}
	else
	{
		//We have no pending products, but still need space for the selected item
		batchesPending.Reserve(1);
	}

	//If the currently selected consumable has not been added to the batches, we take care of that here.
	if ( !currentConsumableHandled )
	{
		MoneyInterface::CashProductBatch batch;	
		batch.m_Quantity = 1;
		batch.m_id.SetFromString(currentSelectedIDString);
		currentConsumableHandled = true;
		batchesPending.PushAndGrow(batch);
	}

	return MoneyInterface::CanBuyCashProducts( batchesPending );
}

void cStoreMainScreen::UpdateTopOfScreenTextInfo()
{
	int hour = CClock::GetHour();
	int minute = CClock::GetMinute();

	// get the date/time:
	char const *asciiDayString = TheText.Get(CClock::GetDayOfWeekTextId());

	char dateTimeString[64] = {"\0"};

	formatf(dateTimeString, NELEM(dateTimeString), "%s %02d:%02d", asciiDayString, hour, minute);

	char cashBuff[64];
	CFrontendStatsMgr::FormatInt64ToCash(cStoreScreenMgr::GetWalletCashForDisplay(), cashBuff, NELEM(cashBuff));
	//Cash
	char CashString[128] = {0};

	if (cStoreScreenMgr::WasOpenedFromNetworkGame() || NetworkInterface::IsGameInProgress())
	{
		char bankBuff[64];
		CFrontendStatsMgr::FormatInt64ToCash(cStoreScreenMgr::GetBankCashForDisplay() + m_PendingCash, bankBuff, NELEM(bankBuff));

		// build the string out of text from the text file & also the converted cash and bank cash
		safecpy(CashString, TheText.Get("MENU_PLYR_BANK"), NELEM(CashString));
		safecat(CashString, " ", NELEM(CashString));
		safecat(CashString, (char*)bankBuff, NELEM(CashString));
		safecat(CashString, "  ", NELEM(CashString));
		safecat(CashString, TheText.Get("MENU_PLYR_CASH"), NELEM(CashString));
		safecat(CashString, " ", NELEM(CashString));
		safecat(CashString, (char*)cashBuff, NELEM(CashString));
	}
	else
	{
		safecpy(CashString, (char*)cashBuff, NELEM(CashString));  // just standard cash	
	}


	if (CScaleformMgr::IsMovieActive(m_StoreMovieId))
	{
		if (CScaleformMgr::BeginMethod(m_StoreMovieId, SF_BASE_CLASS_STORE, "SET_HEADING_DETAILS"))
		{
			CScaleformMgr::AddParamString( cStoreScreenMgr::GetPlayerNameToDisplay(), false);
			CScaleformMgr::AddParamString( dateTimeString, false);
			CScaleformMgr::AddParamString( CashString, false);
			CScaleformMgr::AddParamBool( false );  
			CScaleformMgr::EndMethod(); 
		}
	}
}

void cStoreMainScreen::InitialiseConsumablesLevel()
{
	const CCashProductsData::CCashProductsList& productsList = CashPackInfo::GetInstance().GetProductsList();

	m_InitialConsumableLevels.Reset();
	m_InitialConsumableLevels.Reserve(productsList.GetCount());
	

	for ( int i=0; i < productsList.GetCount(); i++ )
	{
		atString packName;

		if (!productsList[i].GetSKUPackName(packName))
		{
			continue;
		}

        int consumableLevel = 0;

        if (CLiveManager::GetCommerceMgr().GetConsumableManager()->IsOwnershipDataPopulated())
        {
            consumableLevel = CLiveManager::GetCommerceMgr().GetConsumableManager()->GetConsumableLevel(packName);
        }

		m_InitialConsumableLevels.PushAndGrow(consumableLevel);
	}
}

void cStoreMainScreen::UpdateChangeInConsumableLevelCheck()
{
	const CCashProductsData::CCashProductsList& productsList = CashPackInfo::GetInstance().GetProductsList();

	for ( int i=0; i < productsList.GetCount(); i++ )
	{
		atString packName;

		if (!productsList[i].GetSKUPackName(packName))
		{
			continue;
		}

		int numberOfPackOwned = CLiveManager::GetCommerceMgr().GetConsumableManager()->GetConsumableLevel(packName);

		//Take this opportunity to trigger telemetry based on level changes.
		if ( m_InitialConsumableLevels[i] != numberOfPackOwned )
		{
			int newlyObtainedConsumableNum = numberOfPackOwned - m_InitialConsumableLevels[i];
			if (newlyObtainedConsumableNum < 0)
				newlyObtainedConsumableNum *= -1;

			//DO EVENT: We just obtained newlyObtainedConsumableNum of autoConsumeArray[i] via the in game store.
			//POSSIBLE FALSE POSITIVE CASE: Simultaneous web purchase or code redemption while in store. Differentiation on that level requires platform data.

            int amount = static_cast<int>(productsList[i].m_Value * newlyObtainedConsumableNum);
            float usdeAmount = productsList[i].m_USDE * newlyObtainedConsumableNum;

			CNetworkTelemetry::AppendMetric(MetricInGamePurchaseVC(amount, usdeAmount));

			//This line just here to avoid the warning as error.
			(void)newlyObtainedConsumableNum;

			//Now we set the initial level to the new level in case of multiple purchases in the same store session.
			m_InitialConsumableLevels[i] = numberOfPackOwned;
		}
	}
}

void cStoreMainScreen::UpdatePendingCash()
{
	const CCashProductsData::CCashProductsList& productsList = CashPackInfo::GetInstance().GetProductsList();

	m_PendingCash = 0;

	for ( int i=0; i < productsList.GetCount(); i++ )
	{
		atString packName;

		if (!productsList[i].GetSKUPackName(packName))
		{
			continue;
		}

		int numberOfPackOwned = CLiveManager::GetCommerceMgr().GetConsumableManager()->GetConsumableLevel(packName);		
		int valueOfPack =  MoneyInterface::GetCashPackValue(packName);

		if ( valueOfPack != -1 )
		{
			m_PendingCash += ( numberOfPackOwned * valueOfPack );	
		}
	}
}
