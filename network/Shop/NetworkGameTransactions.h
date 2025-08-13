//
// NetworkGameTransactions.h
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
#ifndef NETWORKGAMETRANSACTIONS_H
#define NETWORKGAMETRANSACTIONS_H

#include "network/NetworkTypes.h"
#if __NET_SHOP_ACTIVE

#include "atl/array.h"
#include "atl/string.h"
#include "data/base.h"

//Forward definitions
namespace rage 
{ 
	class RsonWriter;
	class RsonReader;
	class netStatus;
	class bkBank;
};

class GameTransactionHttpTask;

//Queue id to use for game transactions.  300 is arbitrary (not 0,1, or a likely pointer address).
#define GAME_TRANSACTION_TASK_QUEUE_ID 300

//Buffer size for the rson writer.
#define GAME_TRANSACTION_SIZE_OF_RSONWRITER 4096

namespace NetworkGameTransactions
{
	BANK_ONLY( void Bank_InitWidgets(bkBank& bank); )

	//Struct holding info associated with what happened when we apply data from the server.
	class PlayerBalanceApplicationInfo
	{
	public:
		PlayerBalanceApplicationInfo()
			: m_bankCashDifference(0)
			, m_walletCashDifference(0)
			, m_applicationSuccessful(false)
			, m_dataReceived(false)
		{}

		void Clear()
		{
			m_applicationSuccessful = false;
			m_dataReceived          = false;
			m_bankCashDifference     = 0;
			m_walletCashDifference   = 0;
		}

		const PlayerBalanceApplicationInfo& operator=(const PlayerBalanceApplicationInfo& other)
		{
			m_applicationSuccessful = other.m_applicationSuccessful;
			m_dataReceived          = other.m_dataReceived;
			m_bankCashDifference    = other.m_bankCashDifference;
			m_walletCashDifference  = other.m_walletCashDifference;

			return *this;
		}

	public:
		bool  m_applicationSuccessful;
		bool  m_dataReceived;
		s64   m_bankCashDifference;
		s64   m_walletCashDifference;
	};

	//
	// Player Balance
	//
	class PlayerBalance
	{
	public:
		PlayerBalance() 
			: m_evc(0)
			, m_pvc(0)
			, m_bank(0)
			, m_pxr(0.0)
			, m_usde(0.0)
			, m_finished(false)
			, m_deserialized(false)
		{
			Clear();
		}

		PlayerBalance(const PlayerBalance& pb)
			: m_evc(pb.m_evc)
			, m_pvc(pb.m_pvc)
			, m_bank(pb.m_bank)
			, m_pxr(pb.m_pxr)
			, m_usde(pb.m_usde)
			, m_finished(false)
			, m_deserialized(false)
		{
			m_applyDataToStatsInfo = pb.m_applyDataToStatsInfo;

			for(int i = 0; i < MAX_NUM_WALLETS; i++)
				m_Wallets[i] = pb.m_Wallets[i];
		}

		void Clear()
		{
			m_finished = false;
			m_deserialized = false;
			m_evc  = 0;
			m_pvc  = 0;
			m_bank = 0;
			m_pxr  = 0.0;
			m_usde = 0.0;

			for(int i = 0; i < MAX_NUM_WALLETS; i++)
				m_Wallets[i] = 0;

			m_applyDataToStatsInfo.Clear();
		}



		bool Deserialize(const RsonReader& rr);
		bool Serialize(RsonWriter& wr) const;
		bool ApplyDataToStats(PlayerBalanceApplicationInfo& info);
		bool ApplyDataToStats(const int slot, PlayerBalanceApplicationInfo& info);

		s64 GetBankDifference() {return m_applyDataToStatsInfo.m_bankCashDifference;}

#if !__NO_OUTPUT
		const char* ToString() const;
#endif

		const PlayerBalance& operator=(const PlayerBalance& other)
		{
			m_evc  = other.m_evc;
			m_pvc  = other.m_pvc;
			m_bank = other.m_bank;
			m_bank = other.m_bank;
			m_pxr  = other.m_pxr;
			m_usde = other.m_usde;
			m_applyDataToStatsInfo = other.m_applyDataToStatsInfo;

			for(int i = 0; i < MAX_NUM_WALLETS; i++)
				m_Wallets[i] = other.m_Wallets[i];

			return *this;
		}

		bool IsFinished() const {return m_finished;}

	public:
		static const int MAX_NUM_WALLETS = 5;

	public:
		s64    m_evc;
		s64    m_pvc;
		s64    m_bank;
		s64    m_Wallets[MAX_NUM_WALLETS];
		double m_pxr;
		double m_usde;
		PlayerBalanceApplicationInfo m_applyDataToStatsInfo;

	protected:
		bool m_deserialized;
		bool m_finished;
	};

	//
	//	InventoryItem 
	//
	class InventoryItem
	{
	public:
		InventoryItem() : m_itemId(0), m_inventorySlotItemId(0), m_price(0)
		{
		}

		InventoryItem(int id, int value) 
			: m_itemId(id)
			, m_inventorySlotItemId(value)
		{
		}

	public:
		bool Serialize(RsonWriter& wr) const;
		bool Deserialize(const RsonReader& rr);

	public:
		s32 m_itemId;
		union 
		{
			s32 m_inventorySlotItemId;
			s32 m_quantity;
		};
		int m_price;
	};

	class InventoryDataApplicationInfo
	{
	public:
		InventoryDataApplicationInfo()
			: m_numItems(0)
			, m_dataReceived(false)
		{}

		void Clear()
		{
			m_numItems = 0;
			m_dataReceived = false;
		}

		const InventoryDataApplicationInfo& operator=(const InventoryDataApplicationInfo& other)
		{
			m_dataReceived = other.m_dataReceived;
			m_numItems     = other.m_numItems;

			return *this;
		}

	public:
		bool m_dataReceived;
		int  m_numItems;
	};

	class InventoryItemSet
	{
	public:
		InventoryItemSet() : m_slot(-1), m_deserialized(false), m_finished(false) {Clear();}
		InventoryItemSet(const InventoryItemSet& other) 
			: m_slot(other.m_slot)
			, m_items(other.m_items)
			, m_deserialized(other.m_deserialized)
			, m_finished(other.m_finished) 
		{
			m_applyDataToStatsInfo = other.m_applyDataToStatsInfo;
		}

		~InventoryItemSet(){ Clear(); }

		bool Deserialize(const RsonReader& rr);
		bool ApplyDataToStats(InventoryDataApplicationInfo& outInfo);
		void Clear();

		const InventoryItemSet& operator=(const InventoryItemSet& other);

		bool IsFinished() const {return m_finished;}

	public:
		bool                   m_deserialized;
		int                    m_slot;
		atArray<InventoryItem> m_items;

	private:
		bool                          m_finished;
		InventoryDataApplicationInfo  m_applyDataToStatsInfo;
	};

	class TelemetryNonce
	{
	public:
		TelemetryNonce() : m_nonce(0) {}

		void       Clear( ) {m_nonce = 0;}

		void    SetNonce(s64 nonce)       {m_nonce = nonce;}
		s64     GetNonce( )         const { return m_nonce;}

		bool Deserialize(const RsonReader& rr);

		const TelemetryNonce& operator=(const TelemetryNonce& other)
		{
			m_nonce = other.m_nonce;
			return *this;
		}

	public:
		s64 m_nonce;
	};

	//////////////////////////////////////////////////////////////////////////
	//
	//  Transactions
	//
	//////////////////////////////////////////////////////////////////////////

	// Base class for transactions
	class GameTransactionBase : public datBase
	{
	public:
		GameTransactionBase() : m_nounce(0) {}

		virtual bool    Serialize(RsonWriter& wr) const;
		virtual bool	SetNonce();
		u64		        GetNonce() const {return m_nounce;}

	protected:
		u64 m_nounce;

		//Used to specify if we include the 'catalogVersion'
		virtual bool	IncludeCatalogVersion() const { return true; };
	};

	class GameTransactionItems : public GameTransactionBase
	{
	public:
		GameTransactionItems() : GameTransactionBase(), m_slot(-1), m_evconly(false) {;}
		GameTransactionItems(int slot) : GameTransactionBase(), m_slot(slot), m_evconly(false) {;}

		~GameTransactionItems()
		{
			m_items.Reset();
		}

		void  Clear()
		{
			m_items.Reset();
			m_slot = -1;
		}

		void     SetSlot(const int slot) {m_slot=slot;}
		bool SlotIsValid( ) const {return (m_slot>-1);}

		virtual bool Serialize(RsonWriter& wr) const;

		void PushItem(long id, long value, int price, bool evconly);
		bool   Exists(long id) const;

		int GetNumItems() const { return m_items.GetCount(); }

		bool IsEVCOnly() const { return m_evconly; }

	private:
		void CheckEvcOnlyItem(long id);

	protected:

		//True if this is a EVC only operation.
		bool m_evconly;

		//Player slot
		int m_slot;

		//Items being transacted
		atArray<InventoryItem> m_items;
	};

	//////////////////////////////////////////////////////////////////////////
	//
	//	Spend/Earn Events
	//
	class SpendEarnTransaction : public GameTransactionItems
	{
	public:
		SpendEarnTransaction() 
			: GameTransactionItems()
			, m_bank(0)
			, m_wallet(0)
		{
		}

		SpendEarnTransaction(int slot, long bank, long wallet) 
			: GameTransactionItems(slot)
			, m_bank(bank)
			, m_wallet(wallet)
		{
		}

		void Reset(int slot, long bank, long wallet)
		{
			SetSlot(slot);
			m_bank   = bank;
			m_wallet = wallet;
		}

		virtual bool Serialize(RsonWriter& wr) const;

		long   GetBank( ) const {return m_bank;}
		long GetWallet( ) const {return m_wallet;}

	protected:
		long  m_bank;
		long  m_wallet;
	};

	//PURPOSE
	//  Start a player session - slot must be valid.
	bool SessionStart(const int localGamerIndex, const int slot);

	//PURPOSE
	//  Cancel an operation.
	void Cancel(netStatus* status);

	//PURPOSE
	//  Retrieve current catalog.
	bool GetCatalog(const int localGamerIndex, netStatus* status);

	//PURPOSE
	//  Retrieve current catalog version.
	bool GetCatalogVersion(const int localGamerIndex, netStatus* status);

	//
	// Spend Events
	//
	bool                   SendSpendEvent(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status, const u32 sessioncri);
	bool                 SendBuyItemEvent(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status);
	bool             SendBuyPropertyEvent(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status, const u32 sessioncri);
	bool              SendBuyVehicleEvent(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status, const u32 sessioncri);
	bool         SendSpendVehicleModEvent(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, TelemetryNonce* outTelemetryNonce, netStatus* status);
	bool          SendSpendLimitedService(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status);
	bool            SendBuyWarehouseEvent(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status, const u32 sessioncri);
	bool    SendBuyContrabandMissionEvent(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status, const u32 sessioncri);
	bool           SendAddContrabandEvent(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status, const u32 sessioncri);
	bool   SendResetBusinessProgressEvent(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status, const u32 sessioncri);
	bool     SendUpdateBusinessGoodsEvent(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status, const u32 sessioncri);
    bool SendUpdateUpdateWarehouseVehicle(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status, const u32 sessioncri);
	bool               SendBuyCasinoChips(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status, const u32 sessioncri);
	bool            SendUpdateStorageData(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status, const u32 sessioncri);
	bool                    SendBuyUnlock(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status, const u32 sessioncri);

	//
	// Earn Events
	//
	bool             SendEarnEvent(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status, const u32 sessioncri);
	bool      SendSellVehicleEvent(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status, const u32 sessioncri);
	bool    SendEarnLimitedService(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status);
	bool SendRemoveContrabandEvent(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status, const u32 sessioncri);
	bool       SendSellCasinoChips(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status, const u32 sessioncri);

	//
	// Misc Events
	//
	bool SendBonusEvent(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status, const u32 sessioncri);

	//
	//CreatePlayerAppearance
	//
	bool CreatePlayerAppearance(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status);

	//
	//DeleteSlot
	//
	bool DeleteSlot(const int localGamerIndex, const int slot, const bool clearbank, const bool bTransferFundsToBank, const int reason, TelemetryNonce* outTelemetryNonce, netStatus* status);

	//
	//Allot (move from bank to wallet) 
	//Recoup (move from wallet to bank)
	// PARAMS:
	//  outPB - updated player balance.  Pass NULL if you don't care.
	//
	bool Allot(const int localGamerIndex, const int slot, const int amount, PlayerBalance* outPB, TelemetryNonce* outTelemetryNonce, netStatus* status);
	bool Recoup(const int localGamerIndex, const int slot, const int amount, PlayerBalance* outPB, TelemetryNonce* outTelemetryNonce, netStatus* status);

	//
	// Use/Acquire
	//
	class UseAcquireTransaction : public GameTransactionBase
	{
	public:
		UseAcquireTransaction(int slot) 
			: GameTransactionBase()
			, m_slot(slot)
		{

		}

		virtual bool Serialize(RsonWriter& wr) const;

		void PushItem(long id, long quant, int price);

	protected:
		atArray<InventoryItem>	m_items;
		int                     m_slot;
	};

	bool     Use(const int localGamerIndex, UseAcquireTransaction& transactionData, TelemetryNonce* outTelemetryNonce, netStatus* status);
	bool Acquire(const int localGamerIndex, UseAcquireTransaction& transactionData, TelemetryNonce* outTelemetryNonce, netStatus* status);

	
	// NULL Events
	bool SendNULLEvent(const int localGamerIndex, netStatus* status);
    
    //
    // Purch event
    //
    class PurchTransaction : public GameTransactionBase
    {
    public:
        PurchTransaction( const char* entitlementCode, s64 entitlementInstanceId, int amount ) :
            m_EntitlementCode(entitlementCode),
            m_EntitlementInstanceId(entitlementInstanceId),
            m_Amount(amount)
        {
        }

        virtual bool Serialize(RsonWriter& wr) const;

	protected:
		//For PurchTransaction, we don't want to include the 'catalogVersion' field when we send the transction
		virtual bool	IncludeCatalogVersion() const { return false; };
    private:
        atString    m_EntitlementCode;
        s64         m_EntitlementInstanceId;
        int         m_Amount;
    };

    bool Purch(const int localGamerIndex, PurchTransaction& transactionData, PlayerBalance* outPB, TelemetryNonce* outTelemetryNonce, netStatus* status);
    bool Purch(int localGamerIndex, const char* codeToConsume, s64 instanceIdToConsume, int numToConsume, PlayerBalance* outPB, TelemetryNonce* outTelemetryNonce, netStatus* status);

/*private:*/
	bool DoGameTransactionEvent(const int localGamerIndex
								,GameTransactionBase& trans
								,GameTransactionHttpTask* task
								,PlayerBalance* outPB
								,InventoryItemSet* outItemSet
								,TelemetryNonce* outTelemetryNonce
								,netStatus* status
								,const u32 sessioncri);

	
#if !__FINAL
	void ApplyUserInfoHack(const int localGamerIndex, RsonWriter& wr);
#endif

#if __BANK
/*public:*/
	void AddWidgets(bkBank& bank);

	void BankRequestToken();
	void BankSessionStart();
	void BankSessionRestart();
	void BankSpend();
	void BankEarn();
	void BankDeleteSlot();
	void BankRequestCatalog();
    void BankPurch();
#endif
};

#endif //__NET_SHOP_ACTIVE

#endif //NETWORKGAMETRANSACTIONS_H