#ifndef COMMERCEMANAGER_H
#define COMMERCEMANAGER_H

#include "atl/string.h"
#include "atl/array.h"
#include "rline/entitlement/rlrosentitlement.h"
#include "rline/scmembership/rlscmembershipcommon.h"
#include "system/memory.h"
#include "system/memmanager.h"

#if RSG_PC
#include "Network/Shop/NetworkGameTransactions.h"
#endif

#if __BANK
#include "bank/bank.h"
#endif

//IMPORTANT NOTE! EJAs comments about the container functions are at the bottom of this file.

namespace rage
{
    class cCommerceUtil;
    class cCommerceConsumableManager;
    class rlPresenceEvent;
}

class cCommerceManager
{

#if RSG_PC
private:
	enum eWarningScreens {WARNING_NONE, WARNING_FAILED_CONSUMPTION, WARNING_OVER_DAILY_PURCHASE, WARNING_OVER_VC_LIMIT};
#endif // RSG_PC

public:
    cCommerceManager();
    virtual ~cCommerceManager();

    void Init();
    void Shutdown();

    void Update();

#if RSG_PC
	void UpdateWarningMessage();
#endif // RSG_PC

    void CreateCommerceUtil( bool moduleOnly = false );
    bool InitialiseCommerceUtil();
    void DestroyCommerceUtil();

    void DoPlatformLevelDebugDump();
   
    void FetchData();

    static void GenerateCommerceFolderString( atString &folderString );

    bool HasValidData();

    cCommerceUtil* GetCommerceUtil() { return mp_CommerceUtil; }

    cCommerceConsumableManager* GetConsumableManager() { return mp_ConsumableManager; }

//Consumables functions
    bool TriggerConsumableOwnershipLevelFetch();
	void SetAutoConsumePaused(bool pauseAutoConsume);

	bool CreditBankCashStats( const char* consumableId, int numPacks = 1 );

	void ResetAutoConsumeMessage() { m_CurrentAutoConsumePreventMessage = AC_NEUTRAL; }

//Thin data functions.
    bool HasThinDataPopulationBeenCompleted() { return (m_ThinDataFetchEncounteredError || m_ThinDataPopulated); }
    bool IsThinDataInErrorState() { return m_ThinDataFetchEncounteredError; }
    bool IsThinDataPopulated() { return m_ThinDataPopulated; }
    bool IsThinDataPopulationInProgress();
    void ClearThinData();
    void PopulateThinData( bool clearIfPresent = false );
    void DoThinDataDump();
    int GetNumThinDataRecords() { return m_ThinDataArray.GetCount(); }
    
    struct sThinData
    {
        atString m_Id;
        atString m_Name;
        atString m_ShortDesc;
        atString m_ImagePath;
        atString m_TextureName;
        atString m_Category;
        atString m_Price;
        bool m_Purchasable;
        bool m_IsPurchased;
        bool m_IsVirtual;
    };
    
    const sThinData* GetThinDataByIndex( int requestIndex );
    void CancelPendingConsumptions();

	const char* GetCommerceConsumableIdPrefix();
#if __BANK
    void InitWidgets( bkBank& bank );
    void UpdateWidgets();
#endif

	//*******************************************************
	// Generic UI support 
	//******************************************************
	void OnEnterUIStore();  //Called by the UI code when we 'enter' the in-game store

	const int GetNumberTimeEnteredUIStore() const { return m_NumTimesStoreEntered; }

    enum AccessPackType
    {
        NONE = 0,
        STANDALONE,
        BUNDLED_WITH_GREAT_WHITE,
        BUNDLED_WITH_WHALE,
        BUNDLED_WITH_MEGALODON        
    };

    AccessPackType IsEntitledToPremiumPack();
    AccessPackType IsEntitledToStarterPack();


#if RL_SC_MEMBERSHIP_ENABLED
	bool CanConsumeBenefits();
#endif

private:

    void UpdateThinDataPopulation();
    void UpdateLeaderboardWrites();
    void AddItemToThinData( int itemIndex, bool addIfVirtual = true );
    
	bool CanAutoConsume();
	void AutoConsumeUpdate();
	void AutoConsumeUpdateCesp();

#if RL_SC_MEMBERSHIP_ENABLED
	void AddPendingBenefits(const rlScSubscriptionBenefits& benefits);
	void AutoConsumeSubscriptionBenefits();
	bool AwardCashForBenefits();
#endif

	void UpdatePendingAutoConsumePresave();
	void UpdatePendingAutoConsumeTransaction();
	void UpdatePendingAutoConsumeMultiplayerSave(); 

#if RSG_PC
    void AutoConsumeUpdateGameServer();
    static bool GameserverAutoconsumeCallback(int localGamerIndex, const char* codeToConsume, s64 instanceIdToConsume, int numToConsume, netStatus* status);
    void SendPurchEventForCompletedTransfer();
#endif
	
	void AutoConsumePendingPresaveDebugOutput();
	void StartUserOwnedContentRecordUpdate();
    void SetUserOwnedRecord( bool owned, const char* productId );

	bool ShouldThinDataWaitForSaveGames();
    
	void UpdateAutoConsumeMessage();

    static void OnPresenceEvent(const rlPresenceEvent* evt);

#if RL_SC_MEMBERSHIP_ENABLED
	static void OnMembershipEvent(const rlScMembershipEvent* evt);
#endif

    cCommerceUtil *mp_CommerceUtil;
    cCommerceConsumableManager *mp_ConsumableManager;

#if RSG_PC
	static NetworkGameTransactions::TelemetryNonce  sm_TelemetryNonceResponse;
#endif

    int m_PendingAutoTransaction;
    int m_PendingTransferQuantity;
	atString m_PendingTransferId;
	bool m_PendingAutoconsumePresave;

    enum eUtilCreator
    {
        UTIL_CREATOR_THIN,
        UTIL_CREATOR_MANUAL,
        UTIL_CREATOR_NONE
    };

    eUtilCreator m_UtilCreator;
    
    bool m_ThinDataPopulated;
    bool m_ThinDataPopulationInProgress;
    bool m_ThinDataFetchEncounteredError;
    int m_ThinDataErrorValue;
    atArray<sThinData> m_ThinDataArray;

    bool m_WhitelistingErrorDisplayed;

    bool m_PreviousBankTransactionResult;

    bool m_AutoConsumePaused;
    int m_AutoConsumeFrameDelayCount;
	bool m_AutoConsumeForceUpdateOnNetGameStart;
	bool m_ForceAutoconsumeUpdate;
	bool m_AutoconsumeUpdateCesp;

#if RL_SC_MEMBERSHIP_ENABLED
	rlScSubscriptionBenefits m_PendingBenefits;
#endif

	bool m_MoneyStatSaveRequired;
	int m_NumTimesStoreEntered;

	enum eAutoConsumePreventedMessageTypes
	{
		AC_NEUTRAL = 0,
		AC_DISPLAYED,
		AC_OVER_VC_LIMIT,
		AC_OVER_DAILY_PURCHASE,
		AC_NUM_PREVENT_TYPES
	};

	eAutoConsumePreventedMessageTypes m_CurrentAutoConsumePreventMessage;

    static bool m_EntitlementRefreshEventRegistered;

    int m_NumComsumptionFailures;

#if RSG_PC
    NetworkGameTransactions::PlayerBalance m_AutoConsumePlayerBalance;
	eWarningScreens  m_currentWarningScreen;
	bool             m_canConsumeProducts;
	bool             m_doneWarningScreenDailyLimit;
	bool             m_doneWarningScreenVcLimit;
	static const int MAX_FAILED_RETRIES = 5;
#endif

#if __STEAM_BUILD
    cSteamEntitlementCallbackObject m_SteamEntitlementCallbackObject;
#endif

#if COMMERCE_CONTAINER
//Moved the container defines down here so that they are nicely blocked.
//I may move these to a seperate header 
public:
    enum {CONTAINER_MODE_NONE, CONTAINER_MODE_MODULE, CONTAINER_MODE_STORE, CONTAINER_MODE_GENERIC };

	// EJ: Page out the container memory (false means PRX memory only)
	inline bool PageOutModuleMemory() {return sysMemManager::GetInstance().PageOutContainerMemory(false);}
	inline bool PageInModuleMemory() {return sysMemManager::GetInstance().PageInContainerMemory(false);}

	// EJ: Page out the container memory (true means all memory)
	inline bool PageOutContainerMemory() {return sysMemManager::GetInstance().PageOutContainerMemory(true);}
	inline bool PageInContainerMemory() {return sysMemManager::GetInstance().PageInContainerMemory(true);}

	inline bool ContainerIsNone() const {return m_ContainerMode == CONTAINER_MODE_NONE;}
	inline bool ContainerIsLoaded() const {return m_ContainerLoaded;}

    bool ContainerLoadModule();
    bool ContainerUnloadModule();
    inline bool ContainerIsModuleMode() const {return CONTAINER_MODE_MODULE == m_ContainerMode;}

    bool ContainerLoadStore();
    bool ContainerUnloadStore();	
    inline bool ContainerIsStoreMode() const {return CONTAINER_MODE_STORE == m_ContainerMode;}
	bool ContainerReserveStore();

	bool ContainerLoadGeneric();
	bool ContainerLoadGeneric(const size_t id, const char* name = NULL);
	bool ContainerUnloadGeneric();
	bool ContainerUnloadGeneric(const size_t id, const char* name = NULL);
	inline bool ContainerIsGenericMode() const {return CONTAINER_MODE_GENERIC == m_ContainerMode;}			

    int ContainerGetMode() const {return m_ContainerMode;}	
    inline void ContainerSetMode(int mode)
    {
        gnetAssert(CONTAINER_MODE_MODULE == mode || CONTAINER_MODE_STORE == mode);
        m_ContainerMode = mode;
    }

    static void ContainerUpdateWrapper();

private:
    bool ContainerIsEnabled() const {return !ContainerIsNone() && ContainerIsLoaded();}
    bool ContainerLoadPRX();
    bool ContainerUnloadPRX();

private:
    int m_ContainerMode;
	bool m_ContainerLoaded;
#else
    //To avoid preprocessor contagion.
    public:
        inline bool ContainerIsNone() const {return true;}
#endif
};

#endif

/*
	EJ: PS3 Marketplace requires 16MB of isolated container memory + another 1 MB of commerce module (PRX) memory.
	In order to get this to work, I had to create a separate 18MB "flex allocator" memory container. When not in 
	the marketplace, we use this 18 MB for the Network heap, MovePed heap, and the remaining ~13.75 MB for pool 
	allocations (both atPool and fwPool).

	When the player enters the Marketplace, I page out 17MB of the 18MB to the hard drive. Sony imposes an N - 1MB
	limit on the amount of memory that can be paged out. 1MB is used to load the commerce PRX module, while the 
	remaining 16MB is to be used for code redemption and marketplace checkout.

	During the marketplace I pause the game (fwTimer.cpp) and stop scene streaming (streaming.cpp). I only update
	the areas of the game that are needed by the commerce module (game.cpp). In order to avoid unnecessary page
	faults, I have moved the following pools from the flex allocator to the main game heap (MEMTYPE_GAME_VIRTUAL).
	This was done only for pools that had their memory accessed during the marketplace. It's not a perfect solution,
	but it does a good job of minimizing page faults so that the game can run at 25-30fps during the marketplace.

	There are 2 types of pools, and how we handle memory allocations is different for each pool type.
	1)	atPool 
	2)	fwPool

	[fwPool]
	The following pools ARE NOT allocated from flex allocator.  All other fwPools ARE allocated from flex allocator.
	•	CVehicleStructure
	•	naEnvironmentGroup
	•	CMovePedPooledObject
	•	audVehicleAudioEntity
	•	CObject
	•	fwDecoratorExtension
	•	fwAnimDirectorComponent (many pools)
	•	fwAttachmentEntityExtension
	•	CTaskInfo
	•	CPed
	•	CPedIntelligence

	[atPool]
	The following pools ARE allocated from flex allocator.  All other atPools ARE NOT allocated from flex allocator.
	•	CarGeneratorPool
	•	crCreatureComponentPhysical
	•	fwArchetypeManager
	•	phInst
	•	crmtNodeFactory
	•	phGlassInst
	•	bgDrawable
	•	fragCachePoolManager
	•	m_ClothsPool
	•	m_CharClothsPool
	•	crCreatureComponent
	•	crCommonPool

	[New Gamestate Update (game.cpp)]
	I created a new game state (COMMERCE_MAIN_UPDATE) to minimize page faults while in the marketplace.
	•	RegisterCommerceMainUpdateFunctions

	[Do not call debug functions]
	To further minimize page faults, I try to avoid calling debug functions while in the marketplate.
	•	CDebugScene
	•	CStreamingDebug
	•	CDebug
	•	CStreaming
	•	CStreamingRequestList
	•	WorldProbe::CShapeTestTaskData
	•	WorldProbe::CShapeTestManager
*/

