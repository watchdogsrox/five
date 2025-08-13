#ifndef __STORE_SCREEN_MGR_H__
#define __STORE_SCREEN_MGR_H__

#include "frontend/Scaleform/ScaleFormMgr.h"
#include "renderer/sprite2d.h"
#include "ATL/String.h"

class cStoreAdvertScreen;
class cStoreAdvertData;
class cStoreMainScreen;
class cStoreTextureManager;

//A delay which we introduce to alloy scaleform shutdown commands to be processed.
const int EXIT_REQUEST_FRAME_DELAY = 3;

class cStoreScreenMgr
{
public:
    static void Init(unsigned initMode);
    static void Shutdown(unsigned shutdownMode);
    static void Update();
    static void Render();

    static void LoadXmlData( bool forceReload = false );

	static void HandleXML( parTreeNode* pCommerceStoreNode );

	static void Open(bool closeToPauseMenu = true GEN9_STANDALONE_ONLY(, bool launchLandingPageOnClose = false));
	static bool IsLoadingAssets();
    static bool Close();

    static void DisplayAdScreen();

    //static bool FetchAdvertData();

    static const cStoreAdvertData* GetAdvertData() { return NULL;/*mp_StoreAdvertData;*/ }
    
    static cStoreMainScreen* GetStoreMainScreen() { return mp_StoreMainScreen; }

    static void SetStoreScreenEnabled(bool enabled) { m_StoreEnabled = enabled; }
    static bool IsStoreScreenEnabled() { return m_StoreEnabled; };
	static bool IsPendingNetworkShutdownToOpen() { return m_PendingSessionShutdownToOpen; }

#if RSG_PC
	static void OpenPC();
#endif

#if __BANK
    static void InitWidgets();
    static void CreateBankWidgets();
    static void SwitchOnOff();
    static int ms_StoreAnimatedBgSpeed;
#endif // __BANK
    
     static void CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args);

     static void RequestExit();
     static bool IsExitRequested() { return (m_ExitRequestedFrameCount > 0); }

	 static void OnSignOffline();
	 static void OnSignOut();
     static void OnMembershipGained(); 
       
     //This covers all store screens (ads etc)
     static bool IsStoreMenuOpen();

     static atString &GetBaseCategory();
     static void SetBaseCategory( atString &newBaseCat );

     static atString &GetInitialItemID();
     static void SetInitialItemID( const atString &newInitialItemID );

     static bool GetCheckoutInitialItem();
     static void SetCheckoutInitialItem( const bool checkoutInitialItem );

     static float GetMovieXPos() { return m_MovieXPos; }
     static float GetMovieYPos() { return m_MovieYPos; }
     static float GetMovieHeight() { return m_MovieHeight; }
     static float GetMovieWidth() { return m_MovieWidth; }

     static void SetBranding( int aStoreBranding ) { m_CurrentStoreBranding = aStoreBranding; }
     static int GetBranding() { return m_CurrentStoreBranding; }

     static void ResetStoreTextureManager();
     static cStoreTextureManager *GetTextureManager() { return &m_StoreTextureManager; }

     //It's applied as a power of two. Don't blame me, blame scaleform people!
     static int GetStoreItemNumPerColumnExp() {return m_StoreItemNumPerColumn;}

     static void SetStoreBgScrollSpeed( int speed );

	 static bool RequestMPStore();

	 static void SetCashProductsAllowed( bool cashProductsAllowed ) { m_PermitCashProducts = cashProductsAllowed; }
	 static bool AreCashProductsAllowed() { return m_PermitCashProducts; }

	 static void DelayStoreOpen();

	 static void SetReturnToPauseMenu( bool returnToPauseMenu ) { m_ReturnToPauseMenu = returnToPauseMenu; }
	 static bool IsTransitioningToPause() { return m_TogglePauseRenderPhasesFrameCount || m_ResetPauseRenderPhasesFrameCount; };
#if GEN9_STANDALONE_ENABLED
	 static bool GetLaunchLandingPageOnClose() { return m_LaunchLandingPageOnClose; }
#endif
	 static bool WasOpenedFromNetworkGame() { return m_WasOpenedFromNetworkGame; }
     static void ResetNetworkGameTracking();

	 //Functions needed to pass info into the store where many systems are shut down.
	 static const char* GetPlayerNameToDisplay() { return m_PlayerNameForDisplay.c_str(); }
	 static s64 GetWalletCashForDisplay() { return m_WalletCashAmountForDisplay; }
	 static s64 GetBankCashForDisplay() { return m_BankCashAmountForDisplay; }
	static void PopulateDisplayValues();

	static void CheckoutCommerceProduct(const char* productID, int location GEN9_STANDALONE_ONLY(, bool launchLandingPageOnClose));

	COMMERCE_CONTAINER_ONLY(static void OpenInternalPhase2();)

private:
    static void OpenInternal();

    static bool m_bStoreIsOpen;

    static void DumpDebugInfo();

    //Functions to handle the gradiated backgound in code.
    static void RemoveSprites();
    static void SetupSprites();
    static void RequestSprites();
    static void RenderBackground();

    static bool IsBgSpriteLoaded();
	static bool IsMPGameReadyToOpenStore();

	

    static bool m_LoadingBgSprite;

    //static cStoreAdvertScreen* mp_StoreAdvertScreen;
    static cStoreMainScreen* mp_StoreMainScreen;
    static bool m_IsInitialised; 

    //This is in the manager so that the XML for ad data can be fetched and parsed prior to the screen itself being active.
    //static cStoreAdvertData* mp_StoreAdvertData;
    static int m_ExitRequestedFrameCount;

    static CSprite2d sm_BackgroundGradient;

    static atString m_BaseCategory;
    static atString m_InitialProductID;
    static bool m_InitialProductCheckout;
    
    static int m_CurrentStoreBranding;

    //Coordinates for the scaleform movie, grabbed from frontend.xml
    static float m_MovieXPos;
    static float m_MovieYPos;
    static float m_MovieWidth;
    static float m_MovieHeight;

    static bool m_HasXMLBeenRead;

    static grcBlendStateHandle ms_StandardSpriteBlendStateHandle;

    static bool m_ReturnToPauseMenu;
#if GEN9_STANDALONE_ENABLED
	static bool m_LaunchLandingPageOnClose;
#endif

    static cStoreTextureManager m_StoreTextureManager;

    static int m_StoreItemNumPerColumn;
    static bool m_WasOpenedFromNetworkGame;
    static bool m_PendingSessionShutdownToOpen;

    static bool m_StoreEnabled;
	static int m_ResetPauseRenderPhasesFrameCount;
	static int m_TogglePauseRenderPhasesFrameCount;

	static bool m_PermitCashProducts;
	static bool m_ShouldShowCashProducts;

	static int m_StoreOpenFrameDelay;

	static atString m_PlayerNameForDisplay;
	static s64 m_WalletCashAmountForDisplay;
	static s64 m_BankCashAmountForDisplay;
};

#endif //__STORE_SCREEN_MGR_H__