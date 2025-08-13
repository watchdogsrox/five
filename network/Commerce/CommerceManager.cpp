#include "CommerceManager.h"

#include "atl/hashstring.h"

#include "fwcommerce/CommerceUtil.h"
#include "fwcommerce/CommerceData.h"
#include "fwcommerce/CommerceChannel.h"
#include "fwcommerce/CommerceConsumable.h"
#include "fwcommerce/SCS/SCSCommerceConsumable.h"
#include "fwnet/netscgameconfigparser.h"

#include "frontend/PauseMenu.h"
#include "frontend/GameStreamMgr.h"
#include "frontend/Store/StoreTextureManager.h"

#include "Network/NetworkInterface.h"

#include "Network/Live/NetworkTransactionTelemetry.h"
#include "Network/Stats/NetworkLeaderboardSessionMgr.h"
#include "Network/General/NetworkUtil.h"
#include "scene/ExtraContent.h"
#include "stats/StatsInterface.h"
#include "stats/MoneyInterface.h"
#include "network/Live/livemanager.h"
#include "event/EventGroup.h"
#include "event/EventNetwork.h"
#include "SaveLoad/savegame_autoload.h"
#include "SaveLoad/savegame_slow_ps3_scan.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "system/criticalsection.h"
#include "system/appcontent.h"

#if RSG_PC
#include "Network/NetworkTypes.h"
#include "Network/Shop/GameTransactionsSessionMgr.h"
#include "Network/Shop/NetworkShopping.h"
#include "frontend/WarningScreen.h"
#endif

#include "rline/rl.h"
#include "rline/rlpc.h"

// EJ: Memory Optimization
#if COMMERCE_CONTAINER
#include <sys/memory.h>
#include <cell/sysmodule.h>
#include "streaming/streaming.h"
#include "system/memory.h"
#endif

NETWORK_OPTIMISATIONS()

#define NO_AUTO_CONSUME_IDS_FOR_XBOX 0

const int THINDATA_IMAGE_INDEX = 1;
const int AUTOCONSUME_FRAME_PERIOD = 120;
const int AUTO_CONSUME_ID_ARRAY_SIZE = 10; 

static sysCriticalSectionToken s_thinDataCritSecToken;

//Prefixes for commerce
const char* SCEA_CONSUMABLE_PREFIX = "UP1004-BLUS31156_00-";
const char* SCEJ_CONSUMABLE_PREFIX = "JP0230-BLJM61019_00-";
const char* SCEE_CONSUMABLE_PREFIX = "EP1004-BLES01807_00-";
const char* OTHER_PLATFORM_CONSUMABLE_PREFIX = "";

#if RSG_DURANGO
const char* ROOT_PRODUCT_IDENTIFIER_JA = "c41e0227-5a28-4847-b1c4-3b175996827d";
const char* ROOT_PRODUCT_IDENTIFIER = "4f0a3089-ba2c-4f3d-9e38-102a41cbd885";
#else
const char* ROOT_PRODUCT_IDENTIFIER = "";
#endif


//Start/premium pack setups

#if RSG_ORBIS
const char* STARTERPACK_IDENTIFIER = "STAR01";
const char* PREMIUMPACK_IDENTIFIER = "PREM01";
#elif RSG_DURANGO
const char* STARTERPACK_IDENTIFIER = "36783e62-ec1c-4d6d-bb9a-60aab2d076e5";
const char* PREMIUMPACK_IDENTIFIER = "UNIMPLEMENTED";
#elif RSG_PC
const char* STARTERPACK_IDENTIFIER = "CF2CA16589D1308191159D6B5DCD1568";
const char* STARTERPACK_EPIC_IDENTIFIER = "52A0C11305A7C7D81158AD245C4A73D5";

//Premium pack is currently unused
const char* PREMIUMPACK_IDENTIFIER = "1E31CA686715B992C485024838BC4169";
#else
const char* STARTERPACK_IDENTIFIER = "UNIMPLEMENTED";
const char* PREMIUMPACK_IDENTIFIER = "UNIMPLEMENTED";
#endif

static bool s_PremiumAloneFakeEntitlement = false;
static bool s_PremiumBundledWithGreatWhite = false;
static bool s_PremiumBundledWithWhale = false;
static bool s_PremiumBundledWithMegalodon = false;
static bool s_StarterAloneFakeEntitlement = false;
static bool s_StarterBundledWithGreatWhite = false;
static bool s_StarterBundledWithWhale = false;
static bool s_StarterBundledWithMegalodon = false;

#if __BANK
static const unsigned MAX_COMMERCE_STATUS_WIDGET_STRING_SIZE = 64;
static char s_CommerceStatus[MAX_COMMERCE_STATUS_WIDGET_STRING_SIZE];
// static char s_CommerceConsumableTest[MAX_COMMERCE_STATUS_WIDGET_STRING_SIZE];
//static char s_VirtualCashLevel[MAX_COMMERCE_STATUS_WIDGET_STRING_SIZE];

static char s_StarterStatus[MAX_COMMERCE_STATUS_WIDGET_STRING_SIZE];
static char s_BankProductId[64] = {0};
static char s_BankProductCategory[64] = {0};
static bool s_BankProductCheckout = false;

const char* TEST_CONSUMABLE_ID = "EP1004-BLES01807_00-CASH01";
#endif 

bool cCommerceManager::m_EntitlementRefreshEventRegistered = false;

static rlPresence::Delegate sCommercePresenceDlgt;
#if RL_SC_MEMBERSHIP_ENABLED
static rlScMembership::Delegate sCommerceMembershipDlgt;
#endif

#if __STEAM_BUILD
//Callback to check if the installation mask of a piece of content matches what the game has.
void steamTransactionAuthorisedFunc( bool /*authorised*/ )
{
    if (g_rlPc.GetCommerceManager())
    {
        g_rlPc.GetCommerceManager()->ShowCommerceStore(NULL,NULL);
    }
}
#endif

//Callback to check if the installation mask of a piece of content matches what the game has.
bool commerceInstalledCheckFunc( const rage::cCommerceProductData *productData )
{
    if ( productData->GetContainedPackageNames().GetCount() == 0 )
    {
        return false;
    }

    bool allContentInstalled = true;

    for ( int s = 0; s < productData->GetContainedPackageNames().GetCount(); s++ )
    {
        if ( !EXTRACONTENT.IsContentPresent(atStringHash(productData->GetContainedPackageNames()[s])))
        {
            allContentInstalled = false;
        }
    }

    return allContentInstalled;  
}

cCommerceManager::cCommerceManager() :
    mp_CommerceUtil( NULL ),
    mp_ConsumableManager( NULL ),
    m_PendingAutoTransaction(rage::TRANSACTION_ID_UNASSIGNED),
    m_PendingTransferQuantity(0),
	m_PendingAutoconsumePresave(false),
    m_ThinDataPopulated( false ),
    m_ThinDataPopulationInProgress( false ),
    m_ThinDataErrorValue( -1 ),
    m_ThinDataFetchEncounteredError( false ),
    m_UtilCreator( UTIL_CREATOR_NONE ),
    m_WhitelistingErrorDisplayed(false),
    m_PreviousBankTransactionResult(false),
    m_AutoConsumePaused(false),
    m_AutoConsumeFrameDelayCount(0),
	m_AutoConsumeForceUpdateOnNetGameStart(false),
	m_ForceAutoconsumeUpdate(false),
	m_AutoconsumeUpdateCesp(false),
	m_MoneyStatSaveRequired(false),
	m_CurrentAutoConsumePreventMessage(AC_NEUTRAL),
	m_NumTimesStoreEntered(0)
#if __STEAM_BUILD
    , m_SteamEntitlementCallbackObject(0, &steamTransactionAuthorisedFunc)
#endif
     , m_NumComsumptionFailures(0)
#if COMMERCE_CONTAINER
		, m_ContainerLoaded(false)
		, m_ContainerMode(CONTAINER_MODE_NONE)
#endif
#if RSG_PC
		, m_currentWarningScreen(WARNING_NONE)
		, m_canConsumeProducts(true)
		, m_doneWarningScreenDailyLimit(false)
		, m_doneWarningScreenVcLimit(false)
#endif // RSG_PC
{
    m_ThinDataArray.Reset();
}

void cCommerceManager::Init()
{
    m_ThinDataArray.Reset();
    m_ThinDataPopulated = false;
    m_ThinDataPopulationInProgress = false;
    m_ThinDataFetchEncounteredError = false;
    m_ThinDataErrorValue = -1;
    m_UtilCreator = UTIL_CREATOR_NONE;
    m_WhitelistingErrorDisplayed = false;
	m_NumTimesStoreEntered = 0;


    if ( mp_ConsumableManager == NULL )
    {
        mp_ConsumableManager = rage::CommerceConsumablesInstance(false);
    }

    m_PreviousBankTransactionResult = false;

    m_PendingAutoTransaction = rage::TRANSACTION_ID_UNASSIGNED;
    m_PendingTransferQuantity = 0;
	m_PendingTransferId.Reset();
	m_PendingAutoconsumePresave = false;
    m_AutoConsumePaused = false;
    m_AutoConsumeFrameDelayCount = 0;

	m_AutoConsumeForceUpdateOnNetGameStart = false;
    m_ForceAutoconsumeUpdate = false;
	m_MoneyStatSaveRequired = false;

    if (!sCommercePresenceDlgt.IsBound())
    {
        sCommercePresenceDlgt.Bind(cCommerceManager::OnPresenceEvent);
        rlPresence::AddDelegate(&sCommercePresenceDlgt);
    }

#if RL_SC_MEMBERSHIP_ENABLED
	if (!sCommerceMembershipDlgt.IsBound())
	{
		sCommerceMembershipDlgt.Bind(cCommerceManager::OnMembershipEvent);
		rlScMembership::AddDelegate(&sCommerceMembershipDlgt);
	}
#endif

    mp_ConsumableManager->Init();
}

cCommerceManager::~cCommerceManager()
{
    Shutdown();
}

void cCommerceManager::Shutdown()
{
    if ( mp_CommerceUtil )
    {
        DestroyCommerceUtil();
    }

	mp_ConsumableManager->Shutdown();

    if (sCommercePresenceDlgt.IsBound())
        sCommercePresenceDlgt.Unbind();

    rlPresence::RemoveDelegate(&sCommercePresenceDlgt);

#if RL_SC_MEMBERSHIP_ENABLED
	if (sCommerceMembershipDlgt.IsBound())
		sCommerceMembershipDlgt.Unbind();
	rlScMembership::RemoveDelegate(&sCommerceMembershipDlgt);
#endif
}

#if RSG_PC
void cCommerceManager::UpdateWarningMessage()
{
	int numberOfButtons = 1;

	eWarningButtonFlags buttonFlags = FE_WARNING_OK;

	static const unsigned MAX_STRING = 64;
	char bodyTextLabel[MAX_STRING];
	bodyTextLabel[0] = '\0';

	switch (m_currentWarningScreen)
	{
	case cCommerceManager::WARNING_NONE:
		numberOfButtons = 0;
		break;

	case cCommerceManager::WARNING_FAILED_CONSUMPTION:
		safecpy(bodyTextLabel, "PURCH_FAILED");
		break;

	case cCommerceManager::WARNING_OVER_DAILY_PURCHASE:
		safecpy(bodyTextLabel, "CASH_LMT_DAILY");
		break;

	case cCommerceManager::WARNING_OVER_VC_LIMIT:
		safecpy(bodyTextLabel, "CASH_LMT_DAILY");
		break;

	default:
		break;
	}

	//We currently have a warning message set.
	if (m_currentWarningScreen != cCommerceManager::WARNING_NONE && numberOfButtons > 0)
	{
		CWarningScreen::SetMessage(WARNING_MESSAGE_STANDARD, bodyTextLabel, buttonFlags, false, -1, NULL, NULL, true, false);
	}
	//Make sure sure we always clear this for unknown bugs...
	else
	{
		m_currentWarningScreen = cCommerceManager::WARNING_NONE;
	}

	if(numberOfButtons > 0)
	{
		eWarningButtonFlags result = CWarningScreen::CheckAllInput();

		if(result == FE_WARNING_OK)
		{
			commerceDebugf1("UpdateWarningMessage - Ok");
			m_currentWarningScreen = cCommerceManager::WARNING_NONE;
			CWarningScreen::Remove();
		}
	}
}
#endif // RSG_PC

void cCommerceManager::Update()
{
    if ( mp_CommerceUtil )
    {
        mp_CommerceUtil->Update();
    }

    //Handle thin data population.
    if ( m_ThinDataPopulationInProgress )
    {
        UpdateThinDataPopulation();
    }

    //Check for a situation where the commerce util has errored but thin data population is still in progress
    if ( mp_CommerceUtil && mp_CommerceUtil->IsInErrorState() && !mp_CommerceUtil->IsInDataFetchState() && m_ThinDataPopulationInProgress )
    {
        SYS_CS_SYNC(s_thinDataCritSecToken);
  

        //Destroy our errored commerce util
        DestroyCommerceUtil();

		//Stop thin data population
		m_ThinDataPopulationInProgress = false;


        commerceErrorf("Thin commerce population being stopped due to util error. Destroying mp_CommerceUtil");
        m_ThinDataFetchEncounteredError = true;
    }

	const rlGamerInfo* localGamer = NetworkInterface::GetActiveGamerInfo();
    if (mp_ConsumableManager)
    {
		if (localGamer && localGamer->IsValid() && localGamer->IsOnline())
		{
			GetConsumableManager()->SetCurrentConsumableUser(localGamer->GetLocalIndex());
		}
		else
		{
			GetConsumableManager()->SetCurrentConsumableUser(-1);
		}

        mp_ConsumableManager->Update();

		// update any pending auto consume actions
		UpdatePendingAutoConsumePresave();
		UpdatePendingAutoConsumeTransaction();
		UpdatePendingAutoConsumeMultiplayerSave();

        int numberWaitPeriods = static_cast<int>(pow(2, m_NumComsumptionFailures));

        if ( (m_AutoConsumeFrameDelayCount++ % (numberWaitPeriods * AUTOCONSUME_FRAME_PERIOD)) == 0  
            || (m_AutoConsumeForceUpdateOnNetGameStart && NetworkInterface::IsGameInProgress())  
            || m_ForceAutoconsumeUpdate )
        {
            m_ForceAutoconsumeUpdate = false;
			m_AutoConsumeForceUpdateOnNetGameStart = false;

			//We want to repeatedly poll in this case.
			m_AutoconsumeUpdateCesp = true;

			//Important note: This ifdef should be replaced with the correct method of distinguishing between a Gameserver and a non Gameserver using platform
			//				  as soon as one becomes available.
#if RSG_PC
			AutoConsumeUpdateGameServer();
#else
            AutoConsumeUpdate();
#endif
        }

#if RL_SC_MEMBERSHIP_ENABLED
		// check if we have any pending subscription benefits
		AutoConsumeSubscriptionBenefits();
#endif

		//Auto consume and update CESP
		AutoConsumeUpdateCesp();
	}

	UpdateAutoConsumeMessage();

#if RSG_PC
	UpdateWarningMessage();
#endif

	//This code is not relevant to the NG consoles as they do not use whitelisting. I should remove this.
#if 0//!__FINAL
    if ( mp_CommerceUtil && !m_WhitelistingErrorDisplayed && mp_CommerceUtil->HasWhitelistingFailed() )
    {
        CGameStreamMgr::GetGameStream()->PostTicker("WARNING: Commerce white listing failed. If you need DLC access please request whitelisting.", false);
        m_WhitelistingErrorDisplayed = true;
    }
#endif

    //We now trigger the thin data fetch here.
	//To me: This is approaching the threshold of unwieldiness. Consider refactor.

	// 	commerceDisplayf("Thin Data Test: %d | %d | %d | %d | %d \n",
	// 		!IsThinDataPopulationInProgress() ,!IsThinDataInErrorState(), (localGamer && localGamer->IsValid() && localGamer->IsOnline()), !ShouldThinDataWaitForSaveGames(), ContainerIsNone());


	// 	if (localGamer)
	// 	{
	// 		commerceDisplayf("Online Test: %d %d %d",
	// 			localGamer != NULL , localGamer->IsValid() , localGamer->IsOnline());
	// 	}

	if (!IsThinDataPopulated() 
		&& !IsThinDataPopulationInProgress() 
		&& !IsThinDataInErrorState() 
		&& localGamer && localGamer->IsValid() && localGamer->IsOnline()
		&& !ShouldThinDataWaitForSaveGames()
		&& ContainerIsNone()
		&& mp_CommerceUtil == nullptr)
    {
        PopulateThinData(false);
    }

    //Handle the SC entitlement system entitlement refresh message here
    if (m_EntitlementRefreshEventRegistered && GetConsumableManager())
    {
		//We want to repeatedly poll in this case.
		m_AutoconsumeUpdateCesp = true;

        m_EntitlementRefreshEventRegistered = false;
        GetConsumableManager()->SetOwnershipDataDirty();
    }

    UpdateLeaderboardWrites();
#if __BANK
    UpdateWidgets();
#endif
}

void cCommerceManager::CreateCommerceUtil( bool moduleOnly )
{
#if RSG_PC
	//Make sure that when a new entitlement is registered we try to consume again.
	if (m_EntitlementRefreshEventRegistered && GetConsumableManager())
	{
		m_canConsumeProducts = true;
		m_NumComsumptionFailures = 0;
	}
#endif //RSG_PC

    if ( moduleOnly )
    {
        COMMERCE_CONTAINER_ONLY(ContainerLoadModule();)
    }
    else
    {
        COMMERCE_CONTAINER_ONLY(ContainerLoadStore();)
    }
    
    commerceAssert( mp_CommerceUtil == NULL );
    if ( mp_CommerceUtil == NULL )
    {
        //Currently forcing base version as per platform data setup is not ready.
        mp_CommerceUtil = rage::CommerceUtilInstance( /*true*/ );
        commerceDebugf3("Commerce Util Created");
    }  

    mp_CommerceUtil->SetIsInstalledCallback(&commerceInstalledCheckFunc);
	mp_CommerceUtil->SetAllocator(!HasThinDataPopulationBeenCompleted() ? CNetwork::GetNetworkHeap() : sysMemManager::GetInstance().GetFragCacheAllocator());

#if RSG_DURANGO
	if(sysAppContent::IsJapaneseBuild())
	{
		mp_CommerceUtil->SetRootProductIdentifier(ROOT_PRODUCT_IDENTIFIER_JA);
	}
	else
#endif	// RSG_DURANGO
	{
	    mp_CommerceUtil->SetRootProductIdentifier(ROOT_PRODUCT_IDENTIFIER);
	}

    m_UtilCreator = UTIL_CREATOR_MANUAL;
}

void cCommerceManager::DestroyCommerceUtil()
{

    commerceAssert( mp_CommerceUtil );
    if ( mp_CommerceUtil )
    {
        delete mp_CommerceUtil;
    }
    
    mp_CommerceUtil = NULL;

#if COMMERCE_CONTAINER    
    if ( ContainerGetMode() == CONTAINER_MODE_MODULE )
    {
        COMMERCE_CONTAINER_ONLY(ContainerUnloadModule();)
    }
    else
    {
        COMMERCE_CONTAINER_ONLY(ContainerUnloadStore();)
    }
#endif

    m_UtilCreator = UTIL_CREATOR_NONE;
}


bool cCommerceManager::InitialiseCommerceUtil()
{
    atString folderString;
    GenerateCommerceFolderString( folderString );

    // We used to assert but changed it so we can keep the error handing internally
    // for when the store is opened on the landing page outside of MP.
    int localUserIndex = 0;
    const rlGamerInfo* localGamer = NetworkInterface::GetActiveGamerInfo();
    if (!localGamer )
    {
        commerceWarningf("cCommerceManager::InitialiseCommerceUtil(): !localGamer ");
        return false;
    }
    else if ( !localGamer->IsValid() )
    {
        commerceWarningf("cCommerceManager::InitialiseCommerceUtil(): !localGamer->IsValid() ");
        return false;
    }
    else if ( !localGamer->IsOnline())
    {
        commerceWarningf("cCommerceManager::InitialiseCommerceUtil(): !localGamer->IsOnline() ");
        return false;
    }
    else
    {
        localUserIndex = localGamer->GetLocalIndex();
    }

    commerceAssert( mp_CommerceUtil );
    if ( mp_CommerceUtil != NULL )
    {

        mp_CommerceUtil->Init( localUserIndex, folderString );
        commerceDebugf3("Commerce Util Initialised. Index %d. Folder string: %s", localUserIndex, folderString.c_str());

        return true;
    }

    return false;
}

void cCommerceManager::FetchData()
{
    const rlGamerInfo* localGamer = NetworkInterface::GetActiveGamerInfo();
    if (!localGamer )
    {
        commerceAssertf(false,"cCommerceManager::FetchData(): !localGamer ");
        return;
    }
    else if ( !localGamer->IsValid() )
    {
        commerceAssertf(false,"cCommerceManager::FetchData(): !localGamer->IsValid() ");
        return;
    }
    else if ( !localGamer->IsOnline())
    {
        commerceAssertf(false,"cCommerceManager::FetchData(): !localGamer->IsOnline() ");
        return;
    }

	commerceDisplayf("Performing commerce data fetch.");  
    commerceAssert( mp_CommerceUtil );
    if ( mp_CommerceUtil != NULL )
    {
        mp_CommerceUtil->DoDataRequest();
    }
}

void cCommerceManager::GenerateCommerceFolderString( atString &folderString )
{
    //Really not sure this function should be in this file, if better locations are available
    //please let me know and I will relocate.

    //Tried to find a way to do this without ifdefs, but the SKU strings are per platform. Boo.
#if RSG_ORBIS
	if(sysAppContent::IsAmericanBuild())
		folderString = "SCEA";
	else if(sysAppContent::IsJapaneseBuild())
		folderString = "SCEJ";
	else	// if(sysAppContent::IsEuropeanBuild())
		folderString = "SCEE";
    folderString += "/";
#endif

    //NOTICE! If you want to split to a seperate JP config file for Xbox, here is where you do it. Just the JP language split will not cut it.
    // ------->

	folderString += NetworkUtils::GetLanguageCode();
}

void cCommerceManager::DoPlatformLevelDebugDump()
{
    if ( mp_CommerceUtil != NULL )
    {
        mp_CommerceUtil->DoPlatformLevelDebugDump();
    }
}

#if __BANK

static void CommerceMgrWidget_PlatformDataDump()
{
    CLiveManager::GetCommerceMgr().DoPlatformLevelDebugDump();
}

static void CommerceMgrWidget_CreateUtil()
{
    CLiveManager::GetCommerceMgr().CreateCommerceUtil();

    CLiveManager::GetCommerceMgr().InitialiseCommerceUtil();
}

static void CommerceMgrWidget_DeleteUtil()
{
    CLiveManager::GetCommerceMgr().DestroyCommerceUtil();
}

static void CommerceMgrWidget_DoRoSDataFetch()
{
    CLiveManager::GetCommerceMgr().FetchData();
}

static void CommerceMgrWidget_ThinDataFetch()
{
    CLiveManager::GetCommerceMgr().PopulateThinData();
}

static void CommerceMgrWidget_DumpThinData()
{
    CLiveManager::GetCommerceMgr().DoThinDataDump();
}

static void CommerceMgrWidget_TriggerOwnershipLevelFetch()
{
    CLiveManager::GetCommerceMgr().TriggerConsumableOwnershipLevelFetch();
}

static void CommerceMgrTestConsumableConsumption()
{
    //Local dev testing purposes only.
    CLiveManager::GetCommerceMgr().GetConsumableManager()->Consume("0x1",50);
} 

static void CommerceMgrTestStarterPackOwnershipCheck()
{
    CLiveManager::GetCommerceMgr().IsEntitledToStarterPack();
}

static void CommerceMgrTestPremiumPackOwnershipCheck()
{
    CLiveManager::GetCommerceMgr().IsEntitledToPremiumPack();
}

static void CommerceMgrAddBankTestAmount()
{
    if (!StatsInterface::SavePending())
    {
        const int TEST_BANK_INCREMENT_AMOUNT = 10000;
        MoneyInterface::CreditBankCashStats(TEST_BANK_INCREMENT_AMOUNT);
    }
}

static void CommerceMgrWidgetDumpEntitlementInfo()
{
    CLiveManager::GetCommerceMgr().GetConsumableManager()->DumpCurrentOwnershipData();
}

namespace network_commands
{
extern void CommandOpenCommerceStore(const char* productID, const char* category, int location GEN9_STANDALONE_ONLY(, bool launchLandingPageOnClose));
extern void CommandCheckoutCommerceProduct(const char* productID, int location GEN9_STANDALONE_ONLY(, bool launchLandingPageOnClose));
}

static void CommerceMgrOpenStore()
{
	if (s_BankProductCheckout)
	{
		network_commands::CommandCheckoutCommerceProduct(s_BankProductId, 0 GEN9_STANDALONE_ONLY(, false));
	}
	else
	{
		network_commands::CommandOpenCommerceStore(s_BankProductId, s_BankProductCategory, 0 GEN9_STANDALONE_ONLY(, false));
	}
}

void cCommerceManager::UpdateWidgets()
{
    if ( mp_CommerceUtil == NULL )
    {
        strncpy( s_CommerceStatus, "No Commerce Instance.", MAX_COMMERCE_STATUS_WIDGET_STRING_SIZE );
    }
    else if ( mp_CommerceUtil && mp_CommerceUtil->IsCategoryInfoPopulated() )
    {
        strncpy( s_CommerceStatus, "Commerce Data Populated.", MAX_COMMERCE_STATUS_WIDGET_STRING_SIZE );
    }
    else
    {
        strncpy( s_CommerceStatus, "Commerce Instance present, No data.", MAX_COMMERCE_STATUS_WIDGET_STRING_SIZE );
    }

    if (IsEntitledToStarterPack())
    {
        strncpy( s_StarterStatus, "true", MAX_COMMERCE_STATUS_WIDGET_STRING_SIZE);
    }
    else
    {
        strncpy( s_StarterStatus, "false", MAX_COMMERCE_STATUS_WIDGET_STRING_SIZE);
    }

    //formatf(s_VirtualCashLevel, "%d", GetCurrentVirtualCashAmount());
// 
//     if ( mp_ConsumableManager->GetConsumableNum(TEST_CONSUMABLE_ID) == -1 )
//     {
//         strncpy(s_CommerceConsumableTest, "Entitlement not found", MAX_COMMERCE_STATUS_WIDGET_STRING_SIZE);
//     }
//     else
//     {
//         formatf(s_CommerceConsumableTest, "%d", mp_ConsumableManager->GetConsumableNum(TEST_CONSUMABLE_ID), MAX_COMMERCE_STATUS_WIDGET_STRING_SIZE);
//     }
//     
}

void cCommerceManager::InitWidgets( bkBank& bank )
{
    bank.PushGroup( "Commerce Manager" );
    
    bank.AddSeparator();

    bank.AddText("Status:", s_CommerceStatus, MAX_COMMERCE_STATUS_WIDGET_STRING_SIZE-1,true);

    bank.AddSeparator();
    
    bank.AddButton("Create Rage Commerce Util", datCallback(CommerceMgrWidget_CreateUtil) );
   
    bank.AddButton("Do data fetch", datCallback(CommerceMgrWidget_DoRoSDataFetch));

    bank.AddButton("Destroy Rage Commerce Util", datCallback(CommerceMgrWidget_DeleteUtil) );

    bank.AddButton("Trigger thin data fetch", datCallback(CommerceMgrWidget_ThinDataFetch) );

    bank.AddButton("Dump thin data", datCallback(CommerceMgrWidget_DumpThinData) );


    bank.AddButton("Dump platform level commerce data", datCallback(CommerceMgrWidget_PlatformDataDump));

    bank.AddSeparator();

    bank.AddTitle("Thar be dragons above thee line. Press ye not thee buttonz of doom.");

    bank.AddTitle("(Everything down from here is fine. You probably just want the add bucks button though.)");



    bank.AddSeparator();

    bank.AddButton("Add 10000 test virtua bucks.", datCallback(CommerceMgrAddBankTestAmount));   


    bank.AddButton("Trigger consumables ownership level fetch", datCallback(CommerceMgrWidget_TriggerOwnershipLevelFetch));
    //bank.AddText("Virtual cash level:", s_VirtualCashLevel, MAX_COMMERCE_STATUS_WIDGET_STRING_SIZE-1,true);

    //bank.AddText("Currency consumable level:", s_CommerceConsumableTest, MAX_COMMERCE_STATUS_WIDGET_STRING_SIZE-1,true);


    bank.AddButton("Consume 50 units from platform backend", datCallback(CommerceMgrTestConsumableConsumption));
    
    bank.AddButton("Dump current consumables ownership info to tty", datCallback(CommerceMgrWidgetDumpEntitlementInfo));

    bank.AddSeparator();

    bank.AddToggle("Test premium pack alone entitlement", &s_PremiumAloneFakeEntitlement);
    bank.AddToggle("Test premium pack with great white entitlement", &s_PremiumBundledWithGreatWhite);
    bank.AddToggle("Test premium pack with whale shark entitlement", &s_PremiumBundledWithWhale);
	bank.AddToggle("Test premium pack with megalodon shark entitlement", &s_PremiumBundledWithMegalodon);

    bank.AddToggle("Test starter pack alone entitlement", &s_StarterAloneFakeEntitlement);
    bank.AddToggle("Test starter pack with great white entitlement", &s_StarterBundledWithGreatWhite);
    bank.AddToggle("Test starter pack with whale shark entitlement", &s_StarterBundledWithWhale);
	bank.AddToggle("Test starter pack with megalodon shark entitlement", &s_StarterBundledWithMegalodon);

    bank.AddButton("Test starter pack check", datCallback(CommerceMgrTestStarterPackOwnershipCheck));
    bank.AddButton("Test premium pack check", datCallback(CommerceMgrTestPremiumPackOwnershipCheck));

    bank.AddText("Status:", s_StarterStatus, MAX_COMMERCE_STATUS_WIDGET_STRING_SIZE-1,true);

	bank.AddSeparator();
	bank.AddButton("Open Commerce Store", datCallback(CommerceMgrOpenStore));
	bank.AddText("Product Id", s_BankProductId, sizeof(s_BankProductId), false);
	bank.AddText("Product Category", s_BankProductCategory, sizeof(s_BankProductCategory), false);
	bank.AddToggle("Checkout Product", &s_BankProductCheckout);

    bank.PopGroup();
}

#endif //__BANK

bool cCommerceManager::HasValidData()
{
    if ( mp_CommerceUtil != NULL )
    {
        return mp_CommerceUtil->IsCategoryInfoPopulated();
    }
    else
    {
        return false;
    }

}

void cCommerceManager::ClearThinData()
{
    m_ThinDataArray.Reset();
    m_ThinDataPopulated = false;
}

void cCommerceManager::PopulateThinData( bool clearIfPresent )
{
    bool notSupported = false;
#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
    //No PC implementation for thing commerce yet
    notSupported = false;
#endif

    if ( notSupported )
    {
        return;
    }

    commerceDebugf3("cCommerceManager::PopulateThinData called.");
    if ( m_ThinDataPopulated && !clearIfPresent )
    {
        return;
    }

    const rlGamerInfo* localGamer = NetworkInterface::GetActiveGamerInfo();
    if (!localGamer )
    {
        commerceErrorf("NetworkInterface::GetActiveGamerInfo() returned NULL");
        return;
    }
        
    if ( !localGamer->IsValid() )
    {
        commerceErrorf("NetworkInterface::GetActiveGamerInfo()->IsValid() is false");
        return;
    }

    if (!localGamer->IsOnline())
    {
        commerceErrorf("NetworkInterface::GetActiveGamerInfo()->IsOnline() is false");
        return;
    }

    commerceAssertf( mp_CommerceUtil == NULL, "Call to cCommerceManager::PopulateThinData when commerce util is already created. Either you have made it manually, or already called cCommerceManager::PopulateThinData" );

    if ( mp_CommerceUtil != NULL )
    {
        return;
    }

    ClearThinData();
  
    SYS_CS_SYNC(s_thinDataCritSecToken);
    m_ThinDataPopulationInProgress = true;
    CreateCommerceUtil( true );
    
    InitialiseCommerceUtil();
    //Set the creator to be our thin system so that we can keep track.
    m_UtilCreator = UTIL_CREATOR_THIN;

    //Kick off the data fetch
    FetchData();

   
}

void cCommerceManager::UpdateThinDataPopulation()
{
    commerceAssertf( mp_CommerceUtil != NULL, "Call to cCommerceManager::UpdateThinDataPopulation when commerce util is not created." );

    if ( mp_CommerceUtil && mp_CommerceUtil->IsCategoryInfoPopulated() )
    {
        //Huzzah, we have our data. Populate the thin array
        for (s32 iItems=0; iItems < mp_CommerceUtil->GetNumItems(); iItems++ )
        {
            AddItemToThinData( iItems );
        }

        
        //Now shut down the commerce util, freeing up its memory.
        SYS_CS_SYNC(s_thinDataCritSecToken);
        DestroyCommerceUtil();
        m_ThinDataPopulationInProgress = false;
        m_ThinDataFetchEncounteredError = false;
        m_ThinDataPopulated = true;

        //Use this point to update the ROS side records of what users own.
        StartUserOwnedContentRecordUpdate();
    }
}

void cCommerceManager::AddItemToThinData( int itemIndex, bool addIfVirtual )
{
    commerceAssertf( mp_CommerceUtil != NULL, "Call to cCommerceManager::AddItemToThinData when commerce util is not created." );
    if ( mp_CommerceUtil == NULL )
    {
        return;
    }

    sThinData newData;
    const cCommerceItemData *itemData = mp_CommerceUtil->GetItemData(itemIndex);

    if ( itemData == NULL )
    {
        return;
    }

    //We do not want to add categories into the thin data
    if ( itemData->GetItemType() != cCommerceItemData::ITEM_TYPE_PRODUCT )
    {
        return;
    }

    const cCommerceProductData *productData = static_cast<const cCommerceProductData*>(itemData);


    newData.m_Id = productData->GetId().c_str();
    newData.m_Name = productData->GetName().c_str();
    newData.m_ShortDesc = productData->GetDescription().c_str(); 

    //For now we only support the first category for thin data
    if ( itemData->GetCategoryMemberships().GetCount() > 0 )
    {
        newData.m_Category = itemData->GetCategoryMemberships()[0];
    }

    if ( THINDATA_IMAGE_INDEX < itemData->GetImagePaths().GetCount() )
    {
        newData.m_ImagePath = itemData->GetImagePaths()[THINDATA_IMAGE_INDEX];
    }
    else if ( itemData->GetImagePaths().GetCount() > 0 )
    {
        newData.m_ImagePath = itemData->GetImagePaths()[0];
    }
    else
    {
        newData.m_ImagePath.Reset();
    }

    if ( newData.m_ImagePath.GetLength() > 0 )
    {
        newData.m_TextureName = newData.m_ImagePath;
        cStoreTextureManager::ConvertPathToTextureName( newData.m_TextureName );
    }
    else
    {
        newData.m_TextureName.Reset();
    }

    newData.m_IsVirtual = !productData->GetEnumeratedFlag();
    newData.m_Purchasable = mp_CommerceUtil->IsProductPurchasable(productData);
    newData.m_IsPurchased = mp_CommerceUtil->IsProductPurchased(productData);


    const int TEMP_PRICE_BUFFER_SIZE = 128;
    char tempStringBuffer[TEMP_PRICE_BUFFER_SIZE];
    mp_CommerceUtil->GetProductPrice(productData, tempStringBuffer, TEMP_PRICE_BUFFER_SIZE);
    newData.m_Price = tempStringBuffer;
        
    bool addProduct = true;

    if( newData.m_IsVirtual )
    {
        //We probably dont want to have virtual products in our thin data in the long run, but it can be decided later.
        addProduct = addIfVirtual;
    }

    if ( addProduct )
    {
        m_ThinDataArray.Grow() = newData;
    }
}





void cCommerceManager::DoThinDataDump()
{
    commerceDebugf3("Thin data dump\n");

    if ( !IsThinDataPopulated() )
    {
        commerceDebugf3("Thin data not populated.\n");
        return;
    }
    for (s32 i = 0; i < m_ThinDataArray.GetCount(); i++)
    {
        commerceDebugf3("-----------------------------------------------------------------\nId: %s\nName: %s\nDesc: %s\nCategory: %s\nPrice: %s\n",
                        m_ThinDataArray[i].m_Id.c_str(),
                        m_ThinDataArray[i].m_Name.c_str(),
                        m_ThinDataArray[i].m_ShortDesc.c_str(),
                        m_ThinDataArray[i].m_Category.c_str(),
                        m_ThinDataArray[i].m_Price.c_str());
    }

    commerceDebugf3("-----------------------------------------------------------------");
}

const cCommerceManager::sThinData* cCommerceManager::GetThinDataByIndex( int requestIndex )
{
    if (!IsThinDataPopulated())
    {
        return NULL;
    }

    if ( requestIndex < 0 || requestIndex >= m_ThinDataArray.GetCount() )
    {
        return NULL;
    }

    return &m_ThinDataArray[requestIndex];
}

bool cCommerceManager::TriggerConsumableOwnershipLevelFetch()
{
    if ( mp_ConsumableManager )
    {
        int localUserIndex = 0;
        const rlGamerInfo* localGamer = NetworkInterface::GetActiveGamerInfo();
        if (!localGamer )
        {
            commerceAssertf(false,"cCommerceManager::InitialiseCommerceUtil(): !localGamer ");
            return false;
        }
        else if ( !localGamer->IsValid() )
        {
            commerceAssertf(false,"cCommerceManager::InitialiseCommerceUtil(): !localGamer->IsValid() ");
            return false;
        }
        else if ( !localGamer->IsOnline())
        {
            commerceAssertf(false,"cCommerceManager::InitialiseCommerceUtil(): !localGamer->IsOnline() ");
            return false;
        }
        else
        {
            localUserIndex = localGamer->GetLocalIndex();
            mp_ConsumableManager->SetCurrentConsumableUser(localUserIndex);
        }

         return mp_ConsumableManager->StartOwnershipDataFetch();
    }

    return false;
}

void cCommerceManager::SetAutoConsumePaused(bool pauseAutoConsume)
{
	if(m_AutoConsumePaused != pauseAutoConsume)
	{
		// auto consume can be paused (for situations where we want to check platform ownership state before it is instantly eaten.
		commerceDebugf1("SetAutoConsumePaused :: %s -> %s", m_AutoConsumePaused ? "True" : "False", pauseAutoConsume ? "True" : "False");
		m_AutoConsumePaused = pauseAutoConsume;
	}
}

void cCommerceManager::AutoConsumeUpdateCesp()
{
	//We don't need to poll anymore.
	if (!m_AutoconsumeUpdateCesp)
	{
		return;
	}

	if (!GetConsumableManager())
	{
		return;
	}

	if ( !NetworkInterface::IsGameInProgress() || !NetworkInterface::IsNetworkOpen() || StatsInterface::GetBlockSaveRequests() || !MoneyInterface::SavegameCanBuyCashProducts() )
	{
		return;
	}

	//If the data is not present or is out of date we want to get out of here.
	if (!GetConsumableManager()->IsOwnershipDataPopulated() || GetConsumableManager()->IsOwnershipDataDirty())
	{
		//We want to repeatedly poll in this case.
		m_AutoconsumeUpdateCesp = true;
		return;
	}

	//Stop any repeatedly poll in this case.
	m_AutoconsumeUpdateCesp = false;

	const bool isEntitledToCesp = StatsInterface::GetBooleanStat( STAT_MPPLY_SCADMIN_CESP );
	if (isEntitledToCesp)
	{
		return;
	}

	if (IsEntitledToPremiumPack() != AccessPackType::NONE || IsEntitledToStarterPack() != AccessPackType::NONE)
	{
        //spew helpful info
        commerceDebugf1("Setting MPPLY_SCADMIN_CESP to true. IsEntitledTo='%s'", IsEntitledToPremiumPack() != AccessPackType::NONE ? "PremiumPack" : "StarterPack");

		//Set entitlement.
		StatsInterface::SetStatData(STAT_MPPLY_SCADMIN_CESP.GetHash(), true);

		// Savegame.
		StatsInterface::TryMultiplayerSave( STAT_SAVETYPE_COMMERCE_DEPOSIT, 0 );
	}
}

void cCommerceManager::AutoConsumeUpdate()
{
	// currently we want certain back end assets to be immediately consumed and the relevant assets added to cash

	// wait until we are not paused
	if(m_AutoConsumePaused)
		return;

	if(!m_AutoConsumeForceUpdateOnNetGameStart && (!NetworkInterface::IsGameInProgress() || !NetworkInterface::IsNetworkOpen()))
	{
		m_AutoConsumeForceUpdateOnNetGameStart = true;
	}

	if(StatsInterface::GetBlockSaveRequests())
	{
		commerceDebugf2("AutoConsumeUpdate :: Returning from Autoconsume update loop due to StatsInterface::GetBlockSaveRequests");
	}

	if(!CanAutoConsume())
		return;

	// wait until the pre-save completes
	if(m_PendingAutoconsumePresave)
		return;

	// wait until any pending transaction completes
	if(m_PendingAutoTransaction != rage::TRANSACTION_ID_UNASSIGNED)
		return;

	// if the data is not present or is out of date we want to get out of here.
	if(!GetConsumableManager()->IsOwnershipDataPopulated() || GetConsumableManager()->IsOwnershipDataDirty())
	{
		m_ForceAutoconsumeUpdate = true;
		return;
	}

	const CCashProductsData::CCashProductsList& productsList = CashPackInfo::GetInstance().GetProductsList();

	atString skuPackName;
	for (int i=0; i < productsList.GetCount(); i++)
	{
		//Check if the user has any of our auto commodity

        skuPackName.Reset();
		if (!productsList[i].GetSKUPackName(skuPackName))
		{
			continue;
		}

		int amountOfAutoItem = GetConsumableManager()->GetConsumableLevel(skuPackName);

		if (amountOfAutoItem == 0)
		{
			continue;
		}

		float cashProductValue = MoneyInterface::GetCashPackUSDEValue(skuPackName.c_str());
		
		eAutoConsumePreventedMessageTypes lastLimitFailType = AC_NEUTRAL;
		//Check if we are allowed to redeem this cash product

		//These whiles are here to try and redeem a smaller number of cash packs if the users total quantity on entitled packs exceeds a limit.
		while ( amountOfAutoItem > 0 && MoneyInterface::IsOverDailyUSDELimit(static_cast<float>(amountOfAutoItem * cashProductValue ) ) )
		{
			lastLimitFailType = AC_OVER_DAILY_PURCHASE;
			amountOfAutoItem--;
		}

		if (lastLimitFailType == AC_OVER_DAILY_PURCHASE)    
		{
			commerceDebugf1("AutoConsumeUpdate :: Reduced autoconsume amount due to exceeded Daily USDE limit");
		}

		while ( amountOfAutoItem > 0 && MoneyInterface::IsOverUSDEBalanceLimit(static_cast<float>(amountOfAutoItem * cashProductValue ) ) )
		{
			lastLimitFailType = AC_OVER_VC_LIMIT;
			amountOfAutoItem--;
		}

		if (lastLimitFailType == AC_OVER_VC_LIMIT)
		{
			commerceDebugf1("AutoConsumeUpdate :: Reducing autoconsume amount due to exceeded USDE balance limit");
		}

		if (lastLimitFailType != AC_NEUTRAL && m_CurrentAutoConsumePreventMessage != AC_DISPLAYED)
		{
			commerceDebugf1("AutoConsumeUpdate :: Setting autoconsume prevent message");
			m_CurrentAutoConsumePreventMessage = lastLimitFailType;
		}

		if ( amountOfAutoItem == 0 )
		{
			continue;
		}

		if (amountOfAutoItem > 0 && MoneyInterface::CheckPreConsumeSaveDone() )
		{
			bool preConsumeSuccess = true;

			//Start the presave
			if (!MoneyInterface::PreConsumeCashProduct(skuPackName.c_str(), amountOfAutoItem))
			{
				preConsumeSuccess = false;
			}

			if (!MoneyInterface::StartPreConsumeSave())
			{
				preConsumeSuccess = false;
			}

			if (preConsumeSuccess)
			{
				m_PendingTransferQuantity = amountOfAutoItem;
				m_PendingTransferId = skuPackName;
				commerceDebugf1("AutoConsumeUpdate :: Auto consumption of asset now pending. Amount: %d. ID: %s", m_PendingTransferQuantity, m_PendingTransferId.c_str());
				m_ForceAutoconsumeUpdate = true;			
				m_PendingAutoconsumePresave = true;
			}

			//We break out here to ensure only one consumable is auto consumed at once.
			break;
		}
	}

    // we have got here without anything happening. For clarity, explicitly set force update to false
    m_ForceAutoconsumeUpdate = false;
}

bool cCommerceManager::CanAutoConsume()
{
	if(!NetworkInterface::IsLocalPlayerOnline())
		return false;

	const bool isInMultiplayer = NetworkInterface::IsGameInProgress() && NetworkInterface::IsNetworkOpen();
	const bool isBlockSaveRequests = StatsInterface::GetBlockSaveRequests();
	const bool canBuyCashProducts = MoneyInterface::SavegameCanBuyCashProducts();

	return isInMultiplayer && !isBlockSaveRequests && canBuyCashProducts;
}

void cCommerceManager::UpdatePendingAutoConsumePresave()
{
	// paused, bail out
	if(m_AutoConsumePaused)
		return;

	// nothing pending, bail out
	if(!m_PendingAutoconsumePresave)
		return;

	// check if we can't auto consume, eject 
	if(!CanAutoConsume())
	{
		AutoConsumePendingPresaveDebugOutput();

		m_PendingAutoconsumePresave = false;

		MoneyInterface::ClearConsumeCashProduct(m_PendingTransferId.c_str(), m_PendingTransferQuantity);

		m_PendingAutoTransaction = rage::TRANSACTION_ID_UNASSIGNED;
		m_PendingTransferQuantity = 0;
		m_PendingTransferId.Reset();

		return;
	}

	// we have a pending auto consumption, check if the save has completed.
	if(!MoneyInterface::CheckPreConsumeSaveDone())
		return;

	// reset pending flag
	m_PendingAutoconsumePresave = false;

	if(MoneyInterface::CheckPreConsumeSaveSuccessful())
	{
		// start the actual consume...
		commerceDebugf1("UpdatePendingAutoConsumePresave :: Completed PreSave - Amount: %d, Id: %s", m_PendingTransferQuantity, m_PendingTransferId.c_str());
		m_PendingAutoTransaction = GetConsumableManager()->Consume(m_PendingTransferId, m_PendingTransferQuantity);
	}
	else
	{
		commerceErrorf("UpdatePendingAutoConsumePresave :: Failed Presave - Aborting auto consume transaction");

		// decrement the presave stats
		MoneyInterface::ClearConsumeCashProduct(m_PendingTransferId.c_str(), m_PendingTransferQuantity);
		MoneyInterface::StartPreConsumeSave();

		m_PendingAutoTransaction = rage::TRANSACTION_ID_UNASSIGNED;
		m_PendingTransferQuantity = 0;
		m_PendingTransferId.Reset();
		return;
	}
}

void cCommerceManager::UpdatePendingAutoConsumeTransaction()
{
	// paused, bail out
	if(m_AutoConsumePaused)
		return;

	// no pending transaction, bail out
	if(m_PendingAutoTransaction == rage::TRANSACTION_ID_UNASSIGNED)
		return;

	// shouldn't happen but check this...
	cCommerceConsumableManager* consumableManager = GetConsumableManager();
	if(consumableManager == nullptr)
		return;

	// not completed, bail out
	if(!consumableManager->HasTransactionCompleted(m_PendingAutoTransaction))
		return;

	// finished, check if it succeeded
	if(consumableManager->WasTransactionSuccessful(m_PendingAutoTransaction))
	{
		commerceDebugf1("UpdatePendingAutoConsumeTransaction :: Successful - Asset: %s, Credited: %d", m_PendingTransferId.c_str(), m_PendingTransferQuantity);
		CreditBankCashStats(m_PendingTransferId.c_str(), m_PendingTransferQuantity);

		// we have consumed something, mark the fact that we need to save stats once all consumptions are complete.
		m_MoneyStatSaveRequired = true;
	}
	else
	{
		commerceErrorf("UpdatePendingAutoConsumeTransaction :: Failed - Asset: %s", m_PendingTransferId.c_str());
		MoneyInterface::ClearConsumeCashProduct(m_PendingTransferId.c_str(), m_PendingTransferQuantity);
		MoneyInterface::StartPreConsumeSave();
	}

	// clear out transaction details
	m_PendingAutoTransaction = rage::TRANSACTION_ID_UNASSIGNED;
	m_PendingTransferQuantity = 0;
	m_PendingTransferId.Reset();

	// flag dirty data
	consumableManager->SetOwnershipDataDirty();

	// force an auto-consume update
	m_ForceAutoconsumeUpdate = true;
}

void cCommerceManager::UpdatePendingAutoConsumeMultiplayerSave()
{
	if(m_PendingAutoTransaction == rage::TRANSACTION_ID_UNASSIGNED
		&& m_MoneyStatSaveRequired)
	{
		//Here we can be sure that:
		// - There is no transaction pending (since we have checked all our IDs at this point)
		// - At least one cash pack has been consumed recently
		// 
		// So we should save here to guarantee the one save which catches all.
		m_MoneyStatSaveRequired = false;

		StatsInterface::TryMultiplayerSave(STAT_SAVETYPE_COMMERCE_DEPOSIT, 0);
	}
}

#if RL_SC_MEMBERSHIP_ENABLED

bool cCommerceManager::CanConsumeBenefits()
{
	return CanAutoConsume();
}

void cCommerceManager::AddPendingBenefits(const rlScSubscriptionBenefits& benefits)
{
#if !__NO_OUTPUT
	const int numBenefits = benefits.GetCount();
	for (int i = 0; i < numBenefits; i++)
	{
		commerceDebugf1("AddPendingBenefits :: [%i] %s", i, benefits.m_Benefits[i].m_BenefitCode);
	}
#endif

	// should possibly consider adding these in, unlikely but possible
	m_PendingBenefits = benefits;
}

void cCommerceManager::AutoConsumeSubscriptionBenefits()
{
	// do we have any benefits pending consumption
	const int benefitsPending = m_PendingBenefits.GetCount();
	if(benefitsPending <= 0)
		return;

	// wait until we are not paused
	if(m_AutoConsumePaused)
		return;

	if(!CanAutoConsume())
		return;

	// wait until the pre-save completes
	if(m_PendingAutoconsumePresave)
		return;

	// wait until any pending transaction completes
	if(m_PendingAutoTransaction != rage::TRANSACTION_ID_UNASSIGNED)
		return;

	// if the data is not present or is out of date we want to get out of here.
	cCommerceConsumableManager* consumableManager = GetConsumableManager();
	if(consumableManager && (!consumableManager->IsOwnershipDataPopulated() || consumableManager->IsOwnershipDataDirty()))
		return;

	// award all pending cash - just loop until we have no more actions to take
	bool awardedCash = false;
	do
	{
		awardedCash = AwardCashForBenefits();
	} 
	while(awardedCash);
}

bool cCommerceManager::AwardCashForBenefits()
{
	// check our pending subscription benefits - we don't need to check for any daily limits
	const int numBenefits = m_PendingBenefits.GetCount();
	for(int b = 0; b < numBenefits; b++)
	{
		// get the benefit code
		const char* benefitCode = m_PendingBenefits.m_Benefits[b].m_BenefitCode;

		// check if this code matches any of our products
		const CCashProductsData::CCashProductsList& productsList = CashPackInfo::GetInstance().GetProductsList();

		// number of benefits to cash-in of this type
		unsigned numBenefitsToCashIn = 0;

		atString skuPackName;
		for(int p = 0; p < productsList.GetCount(); p++)
		{
			skuPackName.Reset();
			if (!productsList[p].GetSKUPackName(skuPackName))
				continue;

			if (strcmp(benefitCode, skuPackName) != 0)
				continue;

			// we know we have at least one
			numBenefitsToCashIn = 1;

			// we have a viable product - we can now look for more of the same one
			for(int b2 = 0; b2 < numBenefits; b2++)
			{
				// don't count this benefit twice
				if(b == b2)
					continue;

				// if this benefit doesn't match, skip...
				if(strcmp(benefitCode, m_PendingBenefits.m_Benefits[b2].m_BenefitCode) != 0)
					continue;

				// increment counter, another benefit with the same type
				numBenefitsToCashIn++;
			}

			break;
		}

		// if we have come benefits to cash-in...
		if(numBenefitsToCashIn > 0)
		{
			// remove all benefits with this product name
			OUTPUT_ONLY(const unsigned numBenefitsRemoved =) m_PendingBenefits.RemoveBenefits(skuPackName.c_str());

			commerceDebugf1("AutoConsumeSubscriptionBenefits :: Pending - Id: %s, Value: %f, Num: %u, Removed: %u", 
				skuPackName.c_str(), 
				MoneyInterface::GetCashPackUSDEValue(skuPackName.c_str()), 
				numBenefitsToCashIn,
				numBenefitsRemoved);

			CreditBankCashStats(skuPackName.c_str(), numBenefitsToCashIn);

			// we have consumed something, mark the fact that we need to save stats once all consumptions are complete.
			m_MoneyStatSaveRequired = true;

			// we cashed-in some benefits
			return true; 
		}
	}

	// nothing cashed-in
	return false;
}

void cCommerceManager::OnMembershipEvent(const rlScMembershipEvent* evt)
{
	switch (evt->GetId())
	{
		case RLMEMBERSHIP_EVENT_UNCONSUMED_SUBSCRIPTION_BENEFITS:
		{
			// just use an INVALID info for the previous state
			rlScUnconsumedSubscriptionBenefitsEvent* e = (rlScUnconsumedSubscriptionBenefitsEvent*)evt;
			CLiveManager::GetCommerceMgr().AddPendingBenefits(e->m_Benefits);
		}
		break;
	default:
		break;
	}
}
#endif

bool cCommerceManager::CreditBankCashStats(const char* NOTPC_ONLY(consumableId), int NOTPC_ONLY(numPacks))
{
#if !RSG_PC
	atHashString consumableIdHash(consumableId);
	return MoneyInterface::ConsumeCashProduct(consumableIdHash, numPacks, GetNumberTimeEnteredUIStore());
#else
	return true;
#endif
}

void cCommerceManager::StartUserOwnedContentRecordUpdate()
{
    //This function will need to loop through the products we have in thin data. If a product is owned, we 
    //will set a boolean to true on a leaderboard with that products ID as a category
    for (s32 i=0; i<GetNumThinDataRecords(); i++)
    {
        if ( GetThinDataByIndex(i)->m_IsPurchased )
        {
            //Set user record for this product
            SetUserOwnedRecord(true,GetThinDataByIndex(i)->m_Id.c_str());
        }
    }
}

void cCommerceManager::SetUserOwnedRecord( bool owned, const char* productId )
{
    if (!owned)
    {
        return;
    }

    const Leaderboard* lbConf = GAME_CONFIG_PARSER.GetLeaderboard("COMMERCE");
    if (lbConf == NULL)
    {
        return;
    }
    commerceDebugf3("Name of LB is %s", lbConf->GetName());

    //Build our group selector
    rlLeaderboard2GroupSelector groupSelector;
    groupSelector.m_NumGroups = 1;
    strncpy(groupSelector.m_Group[0].m_Category,"ProductId",RL_LEADERBOARD2_CATEGORY_MAX_CHARS);
    strncpy(groupSelector.m_Group[0].m_Id,productId,RL_LEADERBOARD2_GROUP_ID_MAX_CHARS);
    
    //Get our write manager
    CNetworkWriteLeaderboards& writemgr = CNetwork::GetLeaderboardMgr().GetLeaderboardWriteMgr();
    netLeaderboardWrite *leaderboardWrite = writemgr.AddLeaderboard(lbConf->m_id, groupSelector, RL_INVALID_CLAN_ID);

    if ( leaderboardWrite == NULL )
    {
        commerceErrorf("Null returned by AddLeaderboard in SetUserOwnedRecord. This is bad(tm).");
        return;
    }
    
    rlStatValue valueToPush;
    valueToPush.Int64Val = 1;

    const unsigned inputId = lbConf->m_columns[0].m_aggregation.m_gameInput.m_inputId;

    leaderboardWrite->AddColumn(valueToPush, inputId); 
}


void cCommerceManager::UpdateLeaderboardWrites()
{
    //We will probably need to cope with a queue of LB writes. This is where we will do it.
}

bool cCommerceManager::IsThinDataPopulationInProgress()
{
    SYS_CS_SYNC(s_thinDataCritSecToken);
    return m_ThinDataPopulationInProgress;
}

bool cCommerceManager::ShouldThinDataWaitForSaveGames()
{
	return false;
}

//*******************************************************************************************************
// CONTAINER CODE FUNCTIONS HERE
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#if COMMERCE_CONTAINER
void cCommerceManager::ContainerUpdateWrapper()
{
	CLiveManager::GetCommerceMgr().Update();
}

bool cCommerceManager::ContainerLoadPRX()
{
    // EJ: Load the commerce PRX
    if (CELL_OK != cellSysmoduleIsLoaded(CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2))
    { 
        int result = CELL_OK;
        if ((result = cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2)) != CELL_OK)
        {
            Errorf("Unable to load PS3 Commerce Module (see sysmodule.h, code = %x)\n", result);
            return false;
        }
    }

    return true;
}

bool cCommerceManager::ContainerUnloadPRX()
{
    // EJ: Unload the commerce PRX
    if (CELL_OK == cellSysmoduleIsLoaded(CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2))
    { 
        int result = CELL_OK;
        if ((result = cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_NP_COMMERCE2)) != CELL_OK)
        {
            Errorf("Unable to unload PS3 Commerce Module (see sysmodule.h, code = %x)\n", result);
            return false;
        }
    }

    return true;
}

bool cCommerceManager::ContainerLoadModule()
{
    if (ContainerIsEnabled())
    {
        Errorf("Attempting to call LoadModule without first calling UnloadModule!");
        return false;
    }
    
	if (PageOutModuleMemory())
	{
		if (ContainerLoadPRX())
		{
			m_ContainerMode = CONTAINER_MODE_MODULE;
			m_ContainerLoaded = true;
		}
		else
			PageInModuleMemory();
	}

	return (CONTAINER_MODE_MODULE == m_ContainerMode);
}

bool cCommerceManager::ContainerUnloadModule()
{
    if (!ContainerIsModuleMode())
    {
        Errorf("Attempting to call UnloadModule without first calling LoadModule!");
        return false;
    }

	ContainerUnloadPRX();

	if (PageInModuleMemory())
	{
		m_ContainerMode = CONTAINER_MODE_NONE;
		m_ContainerLoaded = false;
	}

	return (CONTAINER_MODE_NONE == m_ContainerMode);
}

bool cCommerceManager::ContainerLoadStore()
{
	commerceAssertf(!ContainerIsEnabled(),"Trying to use the container memory when it is already in use. Container mode is %d", ContainerGetMode() );
    if (ContainerIsEnabled())
    {
        Errorf("Attempting to call LoadStore without first calling UnloadStore!");
        return false;
    }

	if (PageOutContainerMemory())
	{
		if (ContainerLoadPRX())
		{
			m_ContainerMode = CONTAINER_MODE_STORE;
			m_ContainerLoaded = true;
		}
		else
			PageInContainerMemory();
	}

    return (CONTAINER_MODE_STORE == m_ContainerMode);
}

bool cCommerceManager::ContainerUnloadStore()
{
    if (!ContainerIsStoreMode())
    {
        Errorf("Attempting to call UnloadStore without first calling LoadStore!");
        return false;
    }

	ContainerUnloadPRX();

	if (PageInContainerMemory())
	{
		m_ContainerMode = CONTAINER_MODE_NONE;
		m_ContainerLoaded = false;
	}

	return (CONTAINER_MODE_NONE == m_ContainerMode);
}

bool cCommerceManager::ContainerReserveStore()
{
	commerceAssertf(!ContainerIsEnabled(),"Trying to use the container memory when it is already in use. Container mode is %d", ContainerGetMode() );
	if (ContainerIsEnabled())
	{
		Errorf("Attempting to call LoadStore without first calling UnloadStore!");
		return false;
	}

	m_ContainerMode = CONTAINER_MODE_STORE;
	return true;
}

bool cCommerceManager::ContainerLoadGeneric()
{
	if (ContainerIsEnabled())
	{
		Errorf("Attempting to call LoadGeneric without first calling UnloadGeneric!");
		return false;
	}

	if (PageOutModuleMemory())
	{
		m_ContainerMode = CONTAINER_MODE_GENERIC;
		m_ContainerLoaded = true;
	}

	return (CONTAINER_MODE_GENERIC == m_ContainerMode);
}

bool cCommerceManager::ContainerUnloadGeneric()
{
	if (!ContainerIsGenericMode())
	{
		Errorf("Attempting to call UnloadGeneric without first calling LoadGeneric!");
		return false;
	}

	if (PageInModuleMemory())
	{
		m_ContainerMode = CONTAINER_MODE_NONE;
		m_ContainerLoaded = false;
	}

	return (CONTAINER_MODE_NONE == m_ContainerMode);
}

bool cCommerceManager::ContainerLoadGeneric(const size_t prx, const char* OUTPUT_ONLY(name /*= NULL*/))
{
	if (ContainerLoadGeneric())
	{
		// Load the Facebook PRX
		if (CELL_OK != cellSysmoduleIsLoaded(prx))
		{ 
			int result = CELL_OK;
			if ((result = cellSysmoduleLoadModule(prx)) != CELL_OK)
			{
				Errorf("Unable to load %s Module (see sysmodule.h, code = %x)\n", name, result);
				ContainerUnloadGeneric();
				return false;
			}
		}

		return true;
	}

	return false;
}

bool cCommerceManager::ContainerUnloadGeneric(const size_t prx, const char* OUTPUT_ONLY(name /*= NULL*/))
{	
	// Unload the Facebook PRX
	if (CELL_OK == cellSysmoduleIsLoaded(prx))
	{ 
		int result = CELL_OK;
		if ((result = cellSysmoduleUnloadModule(prx)) != CELL_OK)
		{
			Errorf("Unable to unload %s Module (see sysmodule.h, code = %x)\n", name, result);
			//return false;
		}
	}

	return ContainerUnloadGeneric();
}

#endif
//*******************************************************************************************************
// END CONTAINER CODE FUNCTIONS 
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


const char* cCommerceManager::GetCommerceConsumableIdPrefix()
{
	return OTHER_PLATFORM_CONSUMABLE_PREFIX;
}

void cCommerceManager::AutoConsumePendingPresaveDebugOutput()
{
#if !__NO_OUTPUT
	commerceWarningf(" m_PendingAutoconsumePresave was true but I need to cleanup because I cant progress with the consume, reason: ");

	if ( !NetworkInterface::IsGameInProgress() || !NetworkInterface::IsNetworkOpen() )
	{
		commerceWarningf(" .... !NetworkInterface::IsGameInProgress() || !NetworkInterface::IsNetworkOpen() ");
	}

	if ( StatsInterface::GetBlockSaveRequests() )
	{
		commerceWarningf(" .... StatsInterface::GetBlockSaveRequests() ");
	}

	if ( !MoneyInterface::SavegameCanBuyCashProducts() )
	{			
		commerceWarningf(" .... !MoneyInterface::SavegameCanBuyCashProducts() ");
	}
#endif // !__NO_OUTPUT
}

void cCommerceManager::UpdateAutoConsumeMessage()
{
	if (m_CurrentAutoConsumePreventMessage == AC_DISPLAYED)
	{
		//Early out.
		return;
	}

	if ( CGameStreamMgr::GetGameStream() == NULL )
	{
		return;
	}

	if ( CGameStreamMgr::GetGameStream() && (CGameStreamMgr::GetGameStream()->IsPaused() || CGameStreamMgr::GetGameStream()->IsScriptHidingThisFrame()))
	{
		return;
	}

	if (!NetworkInterface::IsGameInProgress())
	{
		return;
	}

	//This seems to be the only way to check that the MP session start has truly completed.
	if (g_PlayerSwitch.IsActive())
	{
		return;
	}

	switch(m_CurrentAutoConsumePreventMessage)
	{
	case(AC_OVER_DAILY_PURCHASE):
		CGameStreamMgr::GetGameStream()->PostTicker(TheText.Get("CASH_LMT_DAILY"), true);
		m_CurrentAutoConsumePreventMessage = AC_DISPLAYED;
		break;
	case(AC_OVER_VC_LIMIT):
		CGameStreamMgr::GetGameStream()->PostTicker(TheText.Get("CASH_LMT_BALANCE"), true);
		m_CurrentAutoConsumePreventMessage = AC_DISPLAYED;
		break;
	default:
		break;
	}
}

void cCommerceManager::OnEnterUIStore()
{
	commerceDebugf1("cCommerceUtil::OnEnterUIStore()");
	m_NumTimesStoreEntered++;
}

cCommerceManager::AccessPackType cCommerceManager::IsEntitledToStarterPack()
{
    if (GetConsumableManager()->IsOwnershipDataPopulated())
    {
        atString starterPackName;
        starterPackName.Reset();
        starterPackName += STARTERPACK_IDENTIFIER;
#if RSG_ORBIS
        starterPackName += GetCommerceConsumableIdPrefix();         
#endif

        if (GetConsumableManager()->IsConsumableKnown(starterPackName))
        {
			return STANDALONE;
        }

#if RSG_PC
		//PC has an additional check here, specifically for Epic support of the starter pack
		starterPackName.Reset();
		starterPackName += STARTERPACK_EPIC_IDENTIFIER;

		if (GetConsumableManager()->IsConsumableKnown(starterPackName))
		{
			return STANDALONE;
		}
#endif
    }

    //Currently driven by bank variables while durable entitlements are completed on all platforms
    if (s_StarterBundledWithMegalodon)
    {
        return BUNDLED_WITH_MEGALODON;
    }
    else if (s_StarterBundledWithWhale == true)
    {
        return BUNDLED_WITH_WHALE;
    }
    else if (s_StarterBundledWithGreatWhite)
    {
        return BUNDLED_WITH_GREAT_WHITE;
    }
    else if (s_StarterAloneFakeEntitlement == true)
    {
        return STANDALONE;
    }

    return AccessPackType::NONE;
}

cCommerceManager::AccessPackType cCommerceManager::IsEntitledToPremiumPack()
{
    if (GetConsumableManager()->IsOwnershipDataPopulated())
    {
        atString premiumPackName;
        premiumPackName.Reset();
        premiumPackName+= PREMIUMPACK_IDENTIFIER;
#if RSG_ORBIS
        premiumPackName += GetCommerceConsumableIdPrefix();         
#endif

        if (GetConsumableManager()->GetConsumableLevel(premiumPackName) > 0)
        {

            return STANDALONE;
        }
    }

    //Currently driven by bank variables while durable entitlements are completed on all platforms
    if (s_PremiumBundledWithMegalodon)
    {
        return BUNDLED_WITH_MEGALODON;
    }
    else if (s_PremiumBundledWithWhale == true)
    {
        return BUNDLED_WITH_WHALE;
    }
    else if (s_PremiumBundledWithGreatWhite)
    {
        return BUNDLED_WITH_GREAT_WHITE;
    }
    else if (s_PremiumAloneFakeEntitlement == true)
    {
        return STANDALONE;
    }

    return AccessPackType::NONE;
}



void cCommerceManager::OnPresenceEvent(const rlPresenceEvent* evt)
{
    switch(evt->GetId())
    {
    case PRESENCE_EVENT_SC_MESSAGE:
        if (evt->m_ScMessage)
        {
            const rlScPresenceMessage& msgBuf = evt->m_ScMessage->m_Message;
            if(msgBuf.IsA<rlScPresenceMessageEntitlementsChanged>())
            {
                m_EntitlementRefreshEventRegistered = true;
            }
        }
        break;
    case PRESENCE_EVENT_SIGNIN_STATUS_CHANGED:
        m_EntitlementRefreshEventRegistered = true;
        break;
    }
}

#if RSG_PC

NetworkGameTransactions::TelemetryNonce  cCommerceManager::sm_TelemetryNonceResponse;

bool cCommerceManager::GameserverAutoconsumeCallback(int localGamerIndex, const char* codeToConsume, s64 instanceIdToConsume, int numToConsume, netStatus* status)
{
	GameTransactionSessionMgr& rSession = GameTransactionSessionMgr::Get();
	if (!rSession.IsQueueBusy() && rSession.IsReady())
	{
		sm_TelemetryNonceResponse.Clear();

		return NetworkGameTransactions::Purch(localGamerIndex
												,codeToConsume
												,instanceIdToConsume
												,numToConsume
												,&(CLiveManager::GetCommerceMgr().m_AutoConsumePlayerBalance)
												,&sm_TelemetryNonceResponse
												,status);
	}

	return false;
}

void cCommerceManager::AutoConsumeUpdateGameServer()
{
    if(m_AutoConsumePaused)
		return;

    if ( !NetworkInterface::IsGameInProgress() || !NetworkInterface::IsNetworkOpen() )
    {
        m_AutoConsumeForceUpdateOnNetGameStart = true;
    }

    if ( StatsInterface::GetBlockSaveRequests())
    {
        commerceDebugf2("Returning from AutoConsumeUpdateGameServer update loop due to StatsInterface::GetBlockSaveRequests()");
    }

    if ( !NetworkInterface::IsGameInProgress() || !NetworkInterface::IsNetworkOpen() )
    {
        return;		
    }

    //Do GameServer magic here. This is likely to comprise of an entitlement level check via the consumable manager, followed
    //by a request to consume the items in the new shiny game server.

    //We have a pending auto consumption, check if it has completed then return
    if ( m_PendingAutoTransaction != rage::TRANSACTION_ID_UNASSIGNED && GetConsumableManager()->HasTransactionCompleted(m_PendingAutoTransaction))
    {
        if ( GetConsumableManager()->WasTransactionSuccessful(m_PendingAutoTransaction) )
        {          
            commerceDisplayf("Game server version auto consumption of asset [%s] amount %d succeeded.", m_PendingTransferId.c_str(),m_PendingTransferQuantity);  

            SendPurchEventForCompletedTransfer();
            m_NumComsumptionFailures = 0;
        }
        else
        {
            commerceErrorf("Game server auto consumption of consumable asset failed.");
            m_NumComsumptionFailures++;
        }

        m_PendingAutoTransaction = rage::TRANSACTION_ID_UNASSIGNED;
        m_PendingTransferQuantity = 0;
        m_PendingTransferId.Reset();

        if (GetConsumableManager() )
        {
            GetConsumableManager()->SetOwnershipDataDirty();
        }

        m_ForceAutoconsumeUpdate = true;
        return;
    }

    if ( m_PendingAutoTransaction != rage::TRANSACTION_ID_UNASSIGNED )
    {
        m_ForceAutoconsumeUpdate = true;
        return;
    }

    //If the data is not present or is out of date we want to get out of here.
    if (!GetConsumableManager()->IsOwnershipDataPopulated() || GetConsumableManager()->IsOwnershipDataDirty())
    {
        //We want to repeatedly poll in this case.
        m_ForceAutoconsumeUpdate = true;
        return;
    }

	//set warning failed to PURCH
	if (m_canConsumeProducts && m_NumComsumptionFailures >= MAX_FAILED_RETRIES && m_currentWarningScreen == WARNING_NONE)
	{
		m_currentWarningScreen = WARNING_FAILED_CONSUMPTION;

		//Make sure we don't retry this again.
		m_canConsumeProducts = false;
	}

	if (m_canConsumeProducts)
	{
		const CCashProductsData::CCashProductsList& productsList = CashPackInfo::GetInstance().GetProductsList();

		atString skuPackName;
		for (int i=0; i < productsList.GetCount(); i++)
		{
			//Check if the user has any of our auto commodity

			if (!productsList[i].GetSKUPackName(skuPackName))
			{
				continue;
			}

			int amountOfAutoItem = GetConsumableManager()->GetConsumableLevel(skuPackName);

			float cashProductValue = MoneyInterface::GetCashPackUSDEValue(skuPackName.c_str());

			eAutoConsumePreventedMessageTypes lastLimitFailType = AC_NEUTRAL;
			//Check if we are allowed to redeem this cash product

			//These whiles are here to try and redeem a smaller number of cash packs if the users total quantity on entitled packs exceeds a limit.
			while ( amountOfAutoItem > 0 && MoneyInterface::IsOverDailyUSDELimit(static_cast<float>(amountOfAutoItem * cashProductValue ) ) )
			{
				if (m_currentWarningScreen == WARNING_NONE && !m_doneWarningScreenDailyLimit)
				{
					m_doneWarningScreenDailyLimit = true;
					m_currentWarningScreen = WARNING_OVER_DAILY_PURCHASE;
				}

				lastLimitFailType = AC_OVER_DAILY_PURCHASE;
				amountOfAutoItem--;
			}

			if (lastLimitFailType == AC_OVER_DAILY_PURCHASE)
			{
				commerceDisplayf("Reduced autoconsume amount due to exceeded Daily USDE limit");
			}

			while ( amountOfAutoItem > 0 && MoneyInterface::IsOverUSDEBalanceLimit(static_cast<float>(amountOfAutoItem * cashProductValue ) ) )
			{
				lastLimitFailType = AC_OVER_VC_LIMIT;
				amountOfAutoItem--;

				if (m_currentWarningScreen == WARNING_NONE && !m_doneWarningScreenVcLimit)
				{
					m_doneWarningScreenVcLimit = true;
					m_currentWarningScreen     = WARNING_OVER_VC_LIMIT;
				}
			}

			if (lastLimitFailType == AC_OVER_VC_LIMIT)
			{
				commerceDisplayf("Reducing autoconsume amount due to exceeded USDE balance limit");
			}


			if (lastLimitFailType != AC_NEUTRAL && m_CurrentAutoConsumePreventMessage != AC_DISPLAYED)
			{
				commerceDisplayf("Setting autoconsume prevent message");
				m_CurrentAutoConsumePreventMessage = lastLimitFailType;
			}

			if (amountOfAutoItem == 0)
			{
				continue;
			}

			m_PendingTransferQuantity = amountOfAutoItem;
			m_PendingTransferId = skuPackName;

			NET_SHOP_ONLY(commerceAssert(dynamic_cast<cSCSCommerceConsumableManager*>(GetConsumableManager()));)
			NET_SHOP_ONLY(static_cast<cSCSCommerceConsumableManager*>(GetConsumableManager())->SetConsumptionCallback(cCommerceManager::GameserverAutoconsumeCallback);)

			m_PendingAutoTransaction = GetConsumableManager()->Consume(m_PendingTransferId,m_PendingTransferQuantity);
			commerceDisplayf("Complete request for Gameserver to consume cash pack. Amount: %d. ID: %s",m_PendingTransferQuantity, m_PendingTransferId.c_str());
			break;
		}
	}
}

void cCommerceManager::SendPurchEventForCompletedTransfer( )
{
    const CCashProductsData::CCashProductsList& productsList = CashPackInfo::GetInstance().GetProductsList();

    s64       amount = 0;
    float usdeAmount = 0;
	const char* packName = NULL;

    atString skuPackName;
    for (int i=0; i<productsList.GetCount(); i++)
    {
        skuPackName.Reset();
        if (productsList[i].GetSKUPackName(skuPackName) && skuPackName == m_PendingTransferId)
        {
            amount     = productsList[i].m_Value * m_PendingTransferQuantity;
            usdeAmount = productsList[i].m_USDE * m_PendingTransferQuantity;
			packName   = productsList[i].m_PackName;
            break;
        }
    }

    if (amount != 0 && usdeAmount != 0 && packName != NULL && commerceVerify(sm_TelemetryNonceResponse.GetNonce() > 0))
    {
		MoneyInterface::ConsumeCashProduct(amount);
		GameTransactionSessionMgr::Get().SetNonceForTelemetry(sm_TelemetryNonceResponse.GetNonce());
        CNetworkTelemetry::AppendMetric(MetricPurchaseVC(static_cast<int>(amount), usdeAmount, packName, false));
		sm_TelemetryNonceResponse.Clear();
    }
    else
    {
        commerceAssertf(false,"Cash pack %s was consumed but not found in the CCashProductsList", m_PendingTransferId.c_str());
    }
};



#endif

void cCommerceManager::CancelPendingConsumptions()
{
	if (mp_ConsumableManager) mp_ConsumableManager->CancelAllTransactions();
}