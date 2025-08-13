//
// MoneyInterface.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef MONEYINTERFACE_H
#define MONEYINTERFACE_H

#include "atl/hashstring.h"
#include "atl/array.h"
#include "atl/singleton.h"
#include "net/status.h"
#include "net/time.h"
#include "rline/socialclub/rlsocialclubcommon.h"
#include "network/NetworkTypes.h"

class sStatData;
namespace rage
{
	class atString;
}

namespace NetworkGameTransactions
{
	class PlayerBalance;
	class PlayerBalanceApplicationInfo;
}

//Class that loads and stores cash products information.
class CCashProductsData
{
	static const unsigned MAX_NUM_PRODUCTS = 70; 

public:
	struct SkuCashPackInfo : public rlCashPackUSDEInfo
	{
		bool GetSKUPackName(atString& outString) const;
	};

	CCashProductsData() 
		: m_received(false)
		, m_countReceived(0)
		, m_numRetries(0)
	{
	}
	~CCashProductsData() 
	{
		Shutdown();
	}

	void  Shutdown( );
	void  Update();
	typedef atFixedArray<SkuCashPackInfo, MAX_NUM_PRODUCTS> CCashProductsList;
	const CCashProductsList& GetProductsList() const {return m_productIds;}

private:
	CCashProductsList m_productIds;
	unsigned m_countReceived;
	netStatus m_netStatus;
	bool m_received;

	static const int RETRY_TIME_MS = 15 * 1000;
	static const unsigned MAX_NUM_RETRIES = 3;
	netStopWatch m_retryTimer;
	unsigned m_numRetries;

};
typedef atSingleton< CCashProductsData > CashPackInfo;

//PURPOSE
//  Interface to access in game money - All virtual cash stuff.
namespace MoneyInterface
{
	// PURPOSE: Retrieve Per-Character wallet Cash.
	s64  GetVCWalletBalance(const int character = -1);
	// PURPOSE: Retrieve Banked Cash..
	s64  GetVCBankBalance( );
	// PURPOSE: Retrieve Total Virtual cash - GetEVCBalance() + GetPVCBalance().
	s64  GetVCBalance();
	// PURPOSE: Retrieve Total Virtual cash player has EARN, either in game or via other mechanism.
	s64  GetEVCBalance();
	// PURPOSE: Retrieve Total Virtual cash player has PAID for with real money.
	s64  GetPVCBalance();

	// PURPOSE: amount of PVC added (through purchase or PVC gift receipt) by a player in 1 day.
	s64  GetPVCDailyAdditionsBalance();

	// PURPOSE: amount of PVC transferred out by a player to other players in 1 day.
	s64  GetPVCDailyTransfersBalance();

	// PURPOSE: amount of VC (non-typed) transferred out by a player to other players in 1 day.
	s64  GetVCDailyTransferBalance();

	// PURPOSE: sum of $USD value of cash packs purchased plus USDE value of gifts received.
	float GetUSDEDailyAdditionsBalance();

	// PURPOSE: Get Personal Exchange Rate for that player (PXR)
	float  GetPXRValue( );
	// PURPOSE: New PXR = (old PVC balance + new PVC addition) / (old USDE + new USD addition).
	void   UpdatePXRValue( const float usdeValue );

	// PURPOSE: USDE Daily Transfers = PVC Daily Transfers out during the day / Personal Exchange Rate for that player (PXR)
	float  GetUSDEDailyTransfersBalance();

	// PURPOSE: USDE Balance = PVC Balance / PXR
	float  GetUSDEBalance( );

	// PURPOSE: Decrement/Increment PVC stat.
	bool  DecrementStatPVCBalance(const s64 amount, const bool changeValue = true);
	bool  IncrementStatPVCBalance(const s64 amount, const bool changeValue = true);

	// PURPOSE: Decrement/Increment EVC stat.
	bool  DecrementStatEVCBalance(const s64 amount, const bool changeValue = true);
	bool  IncrementStatEVCBalance(const s64 amount, const bool changeValue = true);

	// PURPOSE: Decrement/Increment Bank stat.
	bool  DecrementStatBankBalance(const s64 amount, const bool changeValue = true);
	bool  IncrementStatBankBalance(const s64 amount, const bool changeValue = true);
	void  DecrementStatBankBalance64(const s64 amount);

	// PURPOSE: Decrement/Increment Wallet stat.
	bool  DecrementStatWalletBalance(const s64 amount, const int character = -1, const bool changeValue = true);
	bool  IncrementStatWalletBalance(const s64 amount, const int character = -1, const bool changeValue = true);
	void  DecrementStatWalletBalance64(const s64 amount, const int character = -1);

	// PURPOSE: amount of PVC added (through purchase or PVC gift receipt) by a player in 1 day.
	bool  IncrementStatPVCDailyAdditions(const s64 amount, const bool changeValue = true);
	// PURPOSE: amount of PVC transferred out by a player to other players in 1 day.
	bool  IncrementStatPVCDailyTransfers(const s64 amount, const bool changeValue = true);
	// PURPOSE: sum of $USD value of cash packs purchased plus USDE value of gifts received.
	bool  IncrementStatUSDEDailyAdditions(const float amount, const bool changeValue = true);
	// PURPOSE: amount of VC (non-typed) transferred out by a player to other players in 1 day.
	bool IncrementStatVCDailyTransfers(const s64 amount, const bool changeValue = true);

	// PURPOSE: Checks for changes in cheater FLAG - Must be called with the current cheater status.
	//            In-game cash wiped for cheating players, except for any purchased from us.
	void  CheckForCheaterStatsClear( );

	// PURPOSE: Can we get a way of tagging if a player is a high earner based on his Cash per hour telemetry.
	void  UpdateHighEarnerCashAmount(s64 amount);
	u32   GetHighEarnerTimePassed( );
	bool  GetPlayerIsHighEarner( );

	// PURPOSE: Update daily timestamp that checks for clearing the daily trackers.
	void  UpdateDailyStatistics( );
	bool  GetClearDailyStatistics(const sStatData* statIterDailyTimestamp, u64& currPosixTime);
	bool  CanTransferCash(const s64 amount, const float totalusde);
	bool  CanBuyCash(const s64 amount, const float totalusde);

	bool IsOverUSDEBalanceLimit( const float totalusde );

	bool IsOverDailyUSDELimit( const float totalusde );

	int   GetAmountAvailableForTransfer();

	bool  SyncAndValidateServerBalances();
	bool  SyncAndValidateBalances();
	bool  EconomyFixedCrazyNumbers( );
	void  FixBankAndWalletByTotalVC( );
	bool  FixCrazyNumbersEconomy( );
	void  MakeSureClientCashMatches();
	void  ClearFlagForResync();

#if __NET_SHOP_ACTIVE
	bool ApplyVirtualCashBalance(int slot, const NetworkGameTransactions::PlayerBalance& pb, NetworkGameTransactions::PlayerBalanceApplicationInfo& applicationInfo);
#endif

	NOTFINAL_ONLY( void SetValidateMoneyCount(); )

		// PURPOSE: Deal with store packages and values.
	struct CashProductBatch 
	{
		atHashString	m_id;
		int				m_Quantity;
	};

	typedef atArray< CashProductBatch > CashProductBatches;
	bool             CanBuyCashProducts(const CashProductBatches& productBatches);
	bool  IsCashProductInData( const atHashString& product );
	int GetCashPackValue( const char* productID );
	float GetCashPackUSDEValue( const char* productID );

	bool               IsSavegameLoaded( );
	bool     SavegameCanBuyCashProducts( );
	bool          PreConsumeCashProduct(const atHashString& product, int numPacks);
	bool        ClearConsumeCashProduct(const atHashString& product, int numPacks);
	bool            StartPreConsumeSave( );           //Start SAVE before consuming cash products.
	bool        CheckPreConsumeSaveDone( );       //Checks if the Save is finished.
	bool  CheckPreConsumeSaveSuccessful( ); //Checks if the Save was Successful.

#if RSG_PC
	bool  ConsumeCashProduct(const s64 amount);
#else
	bool  ConsumeCashProduct(const atHashString& product, int numPacks, int numStoreEntries);
#endif //RSG_PC

	int RequestBankCashWithdrawal(int requestAmount);
	bool CreditBankCashStats(int amount);

	s64 SpendEarnedCashFromAll(const s64 amount);

	void  MemoryTampering( const bool value );
	bool  IsMemoryTampered( );

};

#endif // MONEYINTERFACE_H

// EOF

