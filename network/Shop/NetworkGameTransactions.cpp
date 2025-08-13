//
// NetworkGameTransactions.h
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#include "Network/NetworkTypes.h"

#if __NET_SHOP_ACTIVE

#include "NetworkGameTransactions.h"

#include "diag/channel.h"
#include "diag/seh.h"
#include "event/EventNetwork.h"
#include "event/EventGroup.h"
#include "rline/ros/rlros.h"
#include "rline/rltitleid.h"
#include "streaming/streamingengine.h"
#include "system/param.h"
#include "bank/bank.h"

#include "fwnet/netchannel.h"

#include "Network/Shop/GameTransactionsSessionMgr.h"
#include "Network/NetworkInterface.h"
#include "network/Shop/Inventory.h"
#include "network/Shop/Catalog.h"
#include "Network/Shop/NetworkShopping.h"	
#include "Stats/MoneyInterface.h"
#include "Stats/StatsTypes.h"

NETWORK_OPTIMISATIONS();
NETWORK_SHOP_OPTIMISATIONS();

RAGE_DEFINE_SUBCHANNEL(net, shop_gameTransactions, DIAG_SEVERITY_DEBUG3);

#undef __net_channel
#define __net_channel net_shop_gameTransactions

XPARAM(netshoplatency);
PARAM(netGameTransactionsNoToken, "Don't use the access tokens");
PARAM(nocatalog, "Don't fail catalog retrieve issues");
NOTFINAL_ONLY( PARAM(netGameTransactionsNoRateLimit, "Ignore rate limit values."); )

#if !__FINAL
PARAM(catalogVersion,"catalog version to use");
#endif //!__FINAL


namespace rage
{
	extern sysMemAllocator* g_rlAllocator;
}


//////////////////////////////////////////////////////////////////////////
//
//	NetworkGameTransactions
//
//////////////////////////////////////////////////////////////////////////

bool NetworkGameTransactions::InventoryItem::Serialize( RsonWriter& wr ) const
{
	return gnetVerify(wr.WriteInt("itemId", static_cast<int>(m_itemId)) 
		&& wr.WriteInt("value", static_cast<int>(m_quantity)) 
		&& wr.WriteInt("price", m_price));
}

bool NetworkGameTransactions::InventoryItem::Deserialize( const RsonReader& rr )
{
	int itemId = 0;

	if (rr.ReadInt("itemId", itemId)
		&& rr.ReadInt("value", m_quantity))
	{
		m_itemId = static_cast<s32>(itemId);
		return true;
	}

	return false;
}

bool NetworkGameTransactions::InventoryItemSet::Deserialize( const RsonReader& rr )
{	
	u32 numinventoryitems = 0;
	RsonReader itemListReader;

	//TODO remove 'Items' when we update the server
	if(!gnetVerifyf(rr.GetMember("items", &itemListReader) || rr.GetMember("Items", &itemListReader), "Missing 'items' object") )
	{
		return false;
	}

	rr.ReadInt("slot", m_slot);	
	gnetAssertf(m_slot > -1, "InventoryItemSet::Deserialize - m_slot=%d", m_slot);

	int itemCount = -1;
	if(rr.ReadInt("count", itemCount) && itemCount > 0
		 && gnetVerifyf( itemCount < 100000, "InventoryItemSet::Deserialize - itemCount=%d", itemCount)) //Make sure it's not a stupid crazy number
	{
		m_items.Grow(itemCount);
	}

	RsonReader iterItemReader;
	bool success = itemListReader.GetFirstMember(&iterItemReader);
	while(success)
	{
		InventoryItem iterItem;
		if(gnetVerify(iterItem.Deserialize(iterItemReader)))
		{
			//Add it to the list.
			m_items.PushAndGrow(iterItem);
			numinventoryitems++;
		}
		success = iterItemReader.GetNextSibling(&iterItemReader);
	}

	m_deserialized = true;

	return true;
}

void NetworkGameTransactions::InventoryItemSet::Clear()
{
	m_slot = -1;
	m_items.Reset(true);
	m_deserialized = false;
	m_finished = false;
	m_applyDataToStatsInfo.Clear();
}

const NetworkGameTransactions::InventoryItemSet& NetworkGameTransactions::InventoryItemSet::operator=(const InventoryItemSet& other)
{
	m_slot = other.m_slot;

	m_items.Reset(true);
	for(int i=0; i<other.m_items.GetCount(); ++i)
	{
		const InventoryItem& item = other.m_items[i];
		m_items.PushAndGrow(item);
	}
	gnetAssert(m_items.GetCount() == other.m_items.GetCount());

	m_applyDataToStatsInfo = other.m_applyDataToStatsInfo;

	return *this;
}

bool NetworkGameTransactions::InventoryItemSet::ApplyDataToStats(InventoryDataApplicationInfo& outInfo) 
{
#if !__FINAL
	GameTransactionSessionMgr& rSession = GameTransactionSessionMgr::Get();
	gnetAssertf(rSession.HaveStatsFinishedLoading(), "Failed HaveStatsFinishedLoading()");
	if (!rSession.HaveStatsFinishedLoading())
	{
		gnetError("Failed HaveStatsFinishedLoading()");
	}
#endif // !__FINAL

	if (m_finished)
	{
		gnetDebug1("InventoryItemSet::ApplyDataToStats - ALREADY DONE- slot:%d, numItems:%d", m_slot, m_items.GetCount());
		outInfo = m_applyDataToStatsInfo;
		return true;
	}

	gnetDebug1("InventoryItemSet::ApplyDataToStats - slot:%d, numItems:%d", m_slot, m_items.GetCount());
	
	m_finished = true;

	const netCatalog& catalog = netCatalog::GetInstance();

	//Fill out the application info.
	m_applyDataToStatsInfo.m_dataReceived = m_deserialized;
	m_applyDataToStatsInfo.m_numItems = m_items.GetCount();

	//Iterate over each item we're received.
	for(int i = 0; i < m_items.GetCount(); ++i)
	{
		const InventoryItem& item = m_items[i];

		//Find this item in the catalog data
		atHashString itemIdHashStr((u32)item.m_itemId);
		const netCatalogBaseItem* catItemBase = catalog.Find(itemIdHashStr);

		if (gnetVerifyf(catItemBase, "No catalog items found for '%d'", (int)item.m_itemId))
		{
			int category = (int)catItemBase->GetCategory();
			OUTPUT_ONLY(const char* catItemName = catItemBase->GetKeyHash().TryGetCStr());
			gnetDebug2("Catalog Item Name: key=<%s, %d>.", catItemName ? catItemName : "UNKNOWN", catItemBase->GetKeyHash().GetHash());

			//Get the static stat value
			s64 statvalue = item.m_inventorySlotItemId;

			atHashString itemIdCatOnlyHashStr((u32)item.m_inventorySlotItemId);
			const netCatalogBaseItem* catOnlyItemBase = catalog.Find( itemIdCatOnlyHashStr );
			if (catOnlyItemBase && catOnlyItemBase->HasStatValue())
			{
				statvalue = (s64)catOnlyItemBase->GetStatValue();
			}

			if(category == CATEGORY_INVENTORY_VEHICLE_MOD
				|| category == CATEGORY_INVENTORY_BEARD
				|| category == CATEGORY_INVENTORY_MKUP
				|| category == CATEGORY_INVENTORY_EYEBROWS
				|| category == CATEGORY_INVENTORY_CHEST_HAIR
				|| category == CATEGORY_INVENTORY_FACEPAINT
				|| category == CATEGORY_INVENTORY_BLUSHER
				|| category == CATEGORY_INVENTORY_LIPSTICK)
			{
				if (statvalue >= 0 && statvalue <= 255)
				{
					gnetDebug1("Applying netInventoryPackedStatsItem - slot:%d, itemId: %d, value:%d.", m_slot, itemIdHashStr.GetHash(), (s32)item.m_inventorySlotItemId);
					netInventoryPackedStatsItem vehModInvItem(itemIdHashStr, 1, statvalue, -1, m_slot);
					catItemBase->SetStatValue(&vehModInvItem);
				}
				else
				{
					gnetWarning("Failed to apply netInventoryPackedStatsItem - slot:'%d', itemId:'%d', inventorySlotItemId:'%d', value:'%" I64FMT "d'.", m_slot, itemIdHashStr.GetHash(), (s32)item.m_inventorySlotItemId, statvalue);
				}
			}
			else if(gnetVerify(NETWORK_SHOPPING_MGR.GetCategoryIsValid((NetShopCategory)category)))
			{
				gnetDebug1("Applying General Item - category:%s, categoryHash:%d, slot:%d, itemId:%d, value:%d.", NETWORK_SHOPPING_MGR.GetCategoryName((NetShopCategory)category), category, m_slot, itemIdHashStr.GetHash(), (s32)item.m_inventorySlotItemId);
				netInventoryBaseItem baseInvItem(itemIdHashStr, 1, statvalue, -1);
				catItemBase->SetStatValue(&baseInvItem);
			}
			else
			{
				gnetError("Unhandled category for received item - id:%d, category:%s",(int)item.m_itemId, catItemBase->GetCategoryName());
				m_applyDataToStatsInfo.m_numItems--;
			}
		}
	}

	outInfo = m_applyDataToStatsInfo;

	return true;
}

bool NetworkGameTransactions::TelemetryNonce::Deserialize( const RsonReader& rr )
{
	s64 nonce = 0;
	rr.AsInt64(nonce);	
	gnetAssert(nonce != 0);
	if (nonce != 0)
	{
		m_nonce = nonce;
	}

	gnetDebug1("Deserialize 'TransactionId' '%" I64FMT "d'.", m_nonce);

	return true;
}

// Misc
bool NetworkGameTransactions::GameTransactionBase::SetNonce()
{
	m_nounce = GameTransactionSessionMgr::Get().GetNextTransactionId();
	return true;
}

bool NetworkGameTransactions::GameTransactionBase::Serialize( RsonWriter& wr ) const
{
	bool retVal = true;

#if !__FINAL
	if (PARAM_netGameTransactionsNoRateLimit.Get())
	{
		wr.WriteBool("ignoreLimits", true);
	}
#endif // !__FINAL

	if(netCatalog::GetInstance().HasReadSucceeded() && IncludeCatalogVersion())
	{
		int catalogVersion = (int)netCatalog::GetInstance().GetVersion();
		retVal &= wr.WriteInt("catalogVersion", catalogVersion);
	}

	if(GameTransactionSessionMgr::Get().IsReady())
	{
		if (gnetVerify(m_nounce > 0))
		{
			retVal &= wr.WriteInt64("TransactionNonce", m_nounce);
		}
		else
		{
			retVal = false;
		}
	}

	return retVal;
}

bool NetworkGameTransactions::PlayerBalance::Deserialize( const RsonReader& rr )
{
#define PB_PROCESS_VFY(x) rverify(rr.ReadValue(#x, m_##x), catchall, gnetError("PlayerBalance: failed %s", #x))

	rtry
	{
		PB_PROCESS_VFY(evc);
		PB_PROCESS_VFY(pvc);
		PB_PROCESS_VFY(bank);
		PB_PROCESS_VFY(pxr);
		PB_PROCESS_VFY(usde);

		RsonReader walletList;
		rverify(rr.GetMember("wallets", &walletList ), catchall, gnetError("PlayerBalance: failed to find 'wallets' array"));

		rverify(walletList.AsArray(m_Wallets, MAX_NUM_WALLETS), catchall, gnetError("PlayerBalance: faile to parse 'wallets' array"));
		
		m_deserialized = true;

		return true;
	}
	rcatchall
	{

	}
	return false;
}

bool NetworkGameTransactions::PlayerBalance::Serialize( RsonWriter& wr ) const
{
	bool success =
	wr.WriteValue("evc", m_evc) &&
	wr.WriteValue("pvc", m_pvc) &&
	wr.WriteValue("bank",m_bank ) &&
	wr.WriteValue("pxr", m_pxr ) &&
	wr.WriteValue("usde",m_usde );

	char walletName[16];
	for (int i = 0; i < MAX_NUM_WALLETS; i++)
	{
		formatf(walletName, "wallet%d", i);
		success &= wr.WriteValue(walletName, m_Wallets[i]);
	}

	return success;
}

#if !__NO_OUTPUT
const char* NetworkGameTransactions::PlayerBalance::ToString() const
{
	static char s_debugString[512];
	RsonWriter wr(s_debugString, RSON_FORMAT_JSON);
	Serialize(wr);
	return s_debugString;
}
#endif

bool NetworkGameTransactions::PlayerBalance::ApplyDataToStats(PlayerBalanceApplicationInfo& applicationInfo)
{
#if !__FINAL
	GameTransactionSessionMgr& rSession = GameTransactionSessionMgr::Get();
	gnetAssertf(rSession.HaveStatsFinishedLoading(), "Failed HaveStatsFinishedLoading()");
	if (!rSession.HaveStatsFinishedLoading())
	{
		gnetError("Failed HaveStatsFinishedLoading()");
	}
#endif

	if (m_finished)
	{
		gnetDebug1("PlayerBalance::ApplyDataToStats - ALREADY DONE: all slots, pb: %s, desereialized[%s]", ToString(), m_deserialized ? "TRUE": "FALSE");
		applicationInfo = m_applyDataToStatsInfo;
		return true;
	}

	gnetDebug1("PlayerBalance::ApplyDataToStats: all slots, pb: %s, desereialized[%s]", ToString(), m_deserialized ? "TRUE": "FALSE");
	if (!m_deserialized)
	{
		return true;	
	}

	m_finished = true;

	m_applyDataToStatsInfo.m_dataReceived = m_deserialized;

	const bool result = MoneyInterface::ApplyVirtualCashBalance(-1, *this, m_applyDataToStatsInfo);

	applicationInfo = m_applyDataToStatsInfo;

	return result;
}

bool NetworkGameTransactions::PlayerBalance::ApplyDataToStats(const int slot, PlayerBalanceApplicationInfo& applicationInfo)
{
#if !__FINAL
	GameTransactionSessionMgr& rSession = GameTransactionSessionMgr::Get();
	gnetAssertf(rSession.HaveStatsFinishedLoading(), "Failed HaveStatsFinishedLoading()");
	if (!rSession.HaveStatsFinishedLoading())
	{
		gnetError("Failed HaveStatsFinishedLoading()");
	}
#endif

	if (m_finished)
	{
		gnetDebug1("PlayerBalance::ApplyDataToStats - ALREADY DONE: slot:%d, pb: %s, desereialized[%s]", slot, ToString(), m_deserialized ? "TRUE": "FALSE");
		applicationInfo = m_applyDataToStatsInfo;
		return true;
	}

	gnetDebug1("PlayerBalance::ApplyDataToStats: slot:%d, pb: %s, desereialized[%s]", slot, ToString(), m_deserialized ? "TRUE": "FALSE");
	if (!m_deserialized)
	{
		return true;	
	}

	m_finished = true;

	m_applyDataToStatsInfo.m_dataReceived = m_deserialized;

	const bool result = MoneyInterface::ApplyVirtualCashBalance(slot, *this, m_applyDataToStatsInfo);

	applicationInfo = m_applyDataToStatsInfo;

	return result;
}

#if !__FINAL
void NetworkGameTransactions::ApplyUserInfoHack(const int localGamerIndex, RsonWriter& wr)
{
	if(PARAM_netGameTransactionsNoToken.Get())
	{
		//Until we get ticketing fully working, we need to put the 'AccountId' in the message
		const rlRosCredentials& cred = rlRos::GetCredentials( localGamerIndex );
		if(gnetVerify(cred.IsValid()))
		{
			PlayerAccountId acctId = cred.GetPlayerAccountId();
			RockstarId rid = cred.GetRockstarId();

			wr.WriteInt("AccountId", acctId);
			wr.WriteInt64("RockId", rid);
			wr.WriteString("Title", g_rlTitleId->m_RosTitleId.GetTitleName());
			wr.WriteString("Plat", g_rlTitleId->m_RosTitleId.GetPlatformName());
			wr.WriteInt("Ver", g_rlTitleId->m_RosTitleId.GetTitleVersion());
		}
	} 
}
#endif


bool NetworkGameTransactions::DoGameTransactionEvent(const int localGamerIndex
													 ,GameTransactionBase& trans
													 ,GameTransactionHttpTask* task
													 ,PlayerBalance* outPB
													 ,InventoryItemSet* outItemSet
													 ,TelemetryNonce* outTelemetryNonce
													 ,netStatus* status
													 ,const u32 sessioncri)
{
	bool nonceSet = false;

	rtry																				
	{
		rverify(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, gnetError("INVALID GAMER INDEX"));

		rcheck(outTelemetryNonce, catchall, gnetError("Failed Telemetry Nonce is NULL"));

		char dataBuffer[GAME_TRANSACTION_SIZE_OF_RSONWRITER];
		RsonWriter wr(dataBuffer, RSON_FORMAT_JSON);

		wr.Begin(NULL, NULL);

#if !__FINAL
		ApplyUserInfoHack(localGamerIndex, wr);
#endif //!__FINAL

		rcheck(trans.SetNonce()
			,catchall
			,gnetError("Failed to setup nonce for trasaction."));

		nonceSet = true;

		if (outTelemetryNonce)
			outTelemetryNonce->SetNonce(trans.GetNonce());

#if __BANK
		rcheck(task->BankSerializeForceError(wr), catchall, 
			gnetError("Failed to serialize 'forceerror'"));
#endif // __BANK

		rcheck(trans.Serialize(wr), catchall, 
			gnetError("Failed to serialize SpendTransaction"));

		rcheck(rlTaskBase::Configure(task, localGamerIndex, sessioncri, wr, outPB, outItemSet, outTelemetryNonce, status), 
			catchall,
			gnetError("GameTransactionHttpTask<%s>: Failed to Configure", task->GetServiceEndpoint()));

		rcheck(rlGetTaskManager()->AppendSerialTask(GAME_TRANSACTION_TASK_QUEUE_ID, task), catchall,);

		task->SetHasNonce();

		return true;
	}
	rcatchall
	{
		if (nonceSet)
		{
			GameTransactionSessionMgr::Get().RollbackNounce();
		}

		if(task)
		{
			if(task->IsPending())
			{
				task->Finish(rlTaskBase::FINISH_FAILED, RLSC_ERROR_UNEXPECTED_RESULT);
			}
			else
			{
				status->ForceFailed();
			}

			rlGetTaskManager()->DestroyTask(task);
		}
	}

	return false;
}



//////////////////////////////////////////////////////////////////////////
//
// Spend
//
//////////////////////////////////////////////////////////////////////////
class SpendGameTransactionHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("Spend");
public:
	virtual const char* GetServiceEndpoint() const { return "Spend"; }
};
BANK_ONLY(ForceErrorHelper SpendGameTransactionHttpTask::sm_errors;)

class EarnGameTransactionHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("Earn");
public:
	virtual const char* GetServiceEndpoint() const { return "Earn"; }
};
BANK_ONLY(ForceErrorHelper EarnGameTransactionHttpTask::sm_errors;)

class BuyItemGameTransactionHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("BuyItem");
public:
	virtual const char* GetServiceEndpoint() const { return "BuyItem"; }
};
BANK_ONLY(ForceErrorHelper BuyItemGameTransactionHttpTask::sm_errors;)

class BuyPropertyGameTransactionHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("BuyProperty");
public:
	virtual const char* GetServiceEndpoint() const { return "BuyProperty"; }
};
BANK_ONLY(ForceErrorHelper BuyPropertyGameTransactionHttpTask::sm_errors;)

class BuyVehicleGameTransactionHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("BuyVehicle");
public:
	virtual const char* GetServiceEndpoint() const { return "BuyVehicle"; }
};
BANK_ONLY(ForceErrorHelper BuyVehicleGameTransactionHttpTask::sm_errors;)

class SellVehicleGameTransactionHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("SellVehicle");
public:
	virtual const char* GetServiceEndpoint() const { return "SellVehicle"; }
};
BANK_ONLY(ForceErrorHelper SellVehicleGameTransactionHttpTask::sm_errors;)

class SpendVehicleModTransactionHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("SpendVehicleMod");
public:
	virtual const char* GetServiceEndpoint() const { return "SpendVehicleMod"; }
};
BANK_ONLY(ForceErrorHelper SpendVehicleModTransactionHttpTask::sm_errors;)

class CreatePlayerAppearanceTransactionHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("CreatePlayerAppearance");
public:
	virtual const char* GetServiceEndpoint() const { return "CreatePlayerAppearance"; }
};
BANK_ONLY(ForceErrorHelper CreatePlayerAppearanceTransactionHttpTask::sm_errors;)

class SpendLimitedServiceGameTransactionHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("SpendLimitedService");
public:
	virtual const char* GetServiceEndpoint() const { return "SpendLimitedService"; }
};
BANK_ONLY(ForceErrorHelper SpendLimitedServiceGameTransactionHttpTask::sm_errors;)

class EarnLimitedServiceGameTransactionHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("EarnLimitedService");
public:
	virtual const char* GetServiceEndpoint() const { return "EarnLimitedService"; }
};
BANK_ONLY(ForceErrorHelper EarnLimitedServiceGameTransactionHttpTask::sm_errors;)

class BuyWarehouseGameTransactionHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("BuyWarehouse");
public:
	virtual const char* GetServiceEndpoint() const { return "BuyWarehouse"; }
};
BANK_ONLY(ForceErrorHelper BuyWarehouseGameTransactionHttpTask::sm_errors;)

class BuyContrabandMissionGameTransactionHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("BuyContrabandMission");
public:
	virtual const char* GetServiceEndpoint() const { return "BuyContrabandMission"; }
};
BANK_ONLY(ForceErrorHelper BuyContrabandMissionGameTransactionHttpTask::sm_errors;)

class AddContrabandGameTransactionHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("AddContraband");
public:
	virtual const char* GetServiceEndpoint() const { return "AddContraband"; }
};
BANK_ONLY(ForceErrorHelper AddContrabandGameTransactionHttpTask::sm_errors;)

class RemoveContrabandGameTransactionHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("RemoveContraband");
public:
	virtual const char* GetServiceEndpoint() const { return "RemoveContraband"; }
};
BANK_ONLY(ForceErrorHelper RemoveContrabandGameTransactionHttpTask::sm_errors;)

class ResetBusinessProgressHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("ResetBusinessProgress");
public:
	virtual const char* GetServiceEndpoint() const { return "ResetBusinessProgress"; }
};
BANK_ONLY(ForceErrorHelper ResetBusinessProgressHttpTask::sm_errors;)

class UpdateBusinessGoodsHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("UpdateBusinessGoods");
public:
	virtual const char* GetServiceEndpoint() const { return "UpdateBusinessGoods"; }
};
BANK_ONLY(ForceErrorHelper UpdateBusinessGoodsHttpTask::sm_errors;)

class UpdateWarehouseVehicleHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("UpdateWarehouseVehicle");
public:
	virtual const char* GetServiceEndpoint() const { return "UpdateWarehouseVehicle"; }
};
BANK_ONLY(ForceErrorHelper UpdateWarehouseVehicleHttpTask::sm_errors;);

class BonusEventHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("Bonus");
public:
	virtual const char* GetServiceEndpoint() const { return "Bonus"; }
};
BANK_ONLY(ForceErrorHelper BonusEventHttpTask::sm_errors;);

class BuyCasinoChipsHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("BuyCasinoChips");
public:
	virtual const char* GetServiceEndpoint() const { return "BuyCasinoChips"; }
};
BANK_ONLY(ForceErrorHelper BuyCasinoChipsHttpTask::sm_errors;);

class BuyUnlockGameTransactionHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("BuyUnlock");
public:
	virtual const char* GetServiceEndpoint() const { return "BuyUnlock"; }
};
BANK_ONLY(ForceErrorHelper BuyUnlockGameTransactionHttpTask::sm_errors;);

class SellCasinoChipsHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("SellCasinoChips");
public:
	virtual const char* GetServiceEndpoint() const { return "SellCasinoChips"; }
};
BANK_ONLY(ForceErrorHelper SellCasinoChipsHttpTask::sm_errors;);

class UpdateStorageDataHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("UpdateStorageData");
public:
	virtual const char* GetServiceEndpoint() const { return "UpdateStorageData"; }
};
BANK_ONLY(ForceErrorHelper UpdateStorageDataHttpTask::sm_errors;);

bool NetworkGameTransactions::SpendEarnTransaction::Serialize( RsonWriter& wr ) const
{
	bool success = gnetVerify(GameTransactionItems::Serialize(wr));

	success &= gnetVerify(wr.WriteInt64("bank", m_bank))
			&& gnetVerify(wr.WriteInt64("wallet", m_wallet));

	if (IsEVCOnly())
		success &= gnetVerify(wr.WriteString("evconly", "true"));

	return success;
}

void NetworkGameTransactions::GameTransactionItems::PushItem(long id, long value, int price, bool evconly)
{
	if (evconly)
	{
		m_evconly = true;
	}
	else
	{
		CheckEvcOnlyItem(id);
	}

	InventoryItem &newItem = m_items.Grow();
	newItem.m_itemId = id;
	newItem.m_quantity = value;
	newItem.m_price = price;
}

void NetworkGameTransactions::GameTransactionItems::CheckEvcOnlyItem(long id)
{
	if (id == (long)ATSTRINGHASH("SERVICE_SPEND_PAY_BOSS", 0x7a31f111)
		|| id == (long)ATSTRINGHASH("SERVICE_SPEND_PAY_GOON", 0x978b277b)
		|| id == (long)ATSTRINGHASH("SERVICE_SPEND_WAGER", 0x2c41a631))
	{
		m_evconly = true;
	}
}

bool NetworkGameTransactions::GameTransactionItems::Exists(long id) const
{
	for (int i = 0; i < m_items.GetCount();  i++)
	{
		if (id == m_items[i].m_itemId)
		{
			return true;
		}
	}

	return false;
}


bool NetworkGameTransactions::GameTransactionItems::Serialize(RsonWriter& wr) const
{
	bool success = gnetVerify(GameTransactionBase::Serialize(wr));

	if (success)
	{
		success &= gnetVerify(m_slot >= 0);

		success &= gnetVerify(wr.WriteInt("slot", m_slot));

		success &= gnetVerify(wr.BeginArray("items", NULL));
		for (int i = 0; i < m_items.GetCount() && success;  i++)
		{
			if(gnetVerify(wr.Begin(NULL, NULL)))
			{
				success &= m_items[i].Serialize(wr);
				success &= wr.End();
			}
			else
			{
				success = false;
			}
			
		}

		gnetVerify(wr.End());
	}

	return success;
}

//////////////////////////////////////////////////////////////////////////
//
// Earn
//
//////////////////////////////////////////////////////////////////////////


bool NetworkGameTransactions::SendSpendEvent(const int localGamerIndex 
											 ,SpendEarnTransaction& trans
											 ,PlayerBalance* outPB
											 ,InventoryItemSet* outItemSet
											 ,TelemetryNonce* outTelemetryNonce
											 ,netStatus* status
											 ,const u32 sessioncri)
{
	SpendGameTransactionHttpTask* task = NULL;

	rtry
	{
		rcheck(trans.GetNumItems() > 0, 
			catchall,
			gnetError("SpendGameTransactionHttpTask::Item list is empty"));

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("SpendGameTransactionHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, trans, task, outPB, outItemSet, outTelemetryNonce, status, sessioncri);
	}
	rcatchall
	{

	}
	
	return false;
}

bool NetworkGameTransactions::SendSpendVehicleModEvent(const int localGamerIndex
													   ,SpendEarnTransaction& trans
													   ,PlayerBalance* outPB
													   ,TelemetryNonce* outTelemetryNonce
													   ,netStatus* status )
{
	SpendVehicleModTransactionHttpTask* task = NULL;

	rtry
	{
		rcheck(trans.GetNumItems() > 0, 
			catchall,
			gnetError("SpendVehicleModTransactionHttpTask::Item list is empty"));

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("SpendVehicleModTransactionHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, trans, task, outPB, NULL, outTelemetryNonce, status, 0);
	}
	rcatchall
	{

	}

	return false;
}


bool NetworkGameTransactions::SendBuyItemEvent(const int localGamerIndex
											   ,SpendEarnTransaction& trans
											   ,PlayerBalance* outPB
											   ,InventoryItemSet* outItemSet
											   ,TelemetryNonce* outTelemetryNonce
											   ,netStatus* status )
{
	BuyItemGameTransactionHttpTask* task = NULL;

	rtry
	{
		rcheck(trans.GetNumItems() > 0, 
			catchall,
			gnetError("BuyItemGameTransactionHttpTask::Item list is empty"));

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("BuyItemGameTransactionHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, trans, task, outPB, outItemSet, outTelemetryNonce, status, 0);
	}
	rcatchall
	{

	}

	return false;
}

bool NetworkGameTransactions::SendBuyPropertyEvent(const int localGamerIndex
												   ,SpendEarnTransaction& trans
												   ,PlayerBalance* outPB
												   ,InventoryItemSet* outItemSet
												   ,TelemetryNonce* outTelemetryNonce
												   ,netStatus* status
												   ,const u32 sessioncri)
{
	BuyPropertyGameTransactionHttpTask* task = NULL;

	rtry
	{
		rcheck(trans.GetNumItems() > 0, 
			catchall,
			gnetError("BuyPropertyGameTransactionHttpTask::Item list is empty"));

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("BuyPropertyGameTransactionHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, trans, task, outPB, outItemSet, outTelemetryNonce, status, sessioncri);
	}
	rcatchall
	{

	}

	return false;
}

bool NetworkGameTransactions::SendBuyVehicleEvent(const int localGamerIndex
												  ,SpendEarnTransaction& trans
												  ,PlayerBalance* outPB
												  ,InventoryItemSet* outItemSet
												  ,TelemetryNonce* outTelemetryNonce
												  ,netStatus* status
												  ,const u32 sessioncri)
{
	BuyVehicleGameTransactionHttpTask* task = NULL;

	rtry
	{
		rcheck(trans.GetNumItems() > 0, 
			catchall,
			gnetError("BuyVehicleGameTransactionHttpTask::Item list is empty"));

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("BuyVehicleGameTransactionHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, trans, task, outPB, outItemSet, outTelemetryNonce, status, sessioncri);
	}
	rcatchall
	{

	}

	return false;
}

bool NetworkGameTransactions::SendBuyWarehouseEvent(const int localGamerIndex
												  ,SpendEarnTransaction& trans
												  ,PlayerBalance* outPB
												  ,InventoryItemSet* outItemSet
												  ,TelemetryNonce* outTelemetryNonce
												  ,netStatus* status
												  ,const u32 sessioncri)
{
	BuyWarehouseGameTransactionHttpTask* task = NULL;

	rtry
	{
		rcheck(trans.GetNumItems() > 0, 
			catchall,
			gnetError("BuyWarehouseGameTransactionHttpTask::Item list is empty"));

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("BuyWarehouseGameTransactionHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, trans, task, outPB, outItemSet, outTelemetryNonce, status, sessioncri);
	}
	rcatchall
	{

	}

	return false;
}

bool NetworkGameTransactions::SendBuyContrabandMissionEvent(const int localGamerIndex
												  ,SpendEarnTransaction& trans
												  ,PlayerBalance* outPB
												  ,InventoryItemSet* outItemSet
												  ,TelemetryNonce* outTelemetryNonce
												  ,netStatus* status
												  ,const u32 sessioncri)
{
	BuyContrabandMissionGameTransactionHttpTask* task = NULL;

	rtry
	{
		rcheck(trans.GetNumItems() > 0, 
			catchall,
			gnetError("BuyContrabandGameTransactionHttpTask::Item list is empty"));

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("BuyContrabandGameTransactionHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, trans, task, outPB, outItemSet, outTelemetryNonce, status, sessioncri);
	}
	rcatchall
	{

	}

	return false;
}

bool NetworkGameTransactions::SendAddContrabandEvent(const int localGamerIndex
													,SpendEarnTransaction& trans
													,PlayerBalance* outPB
													,InventoryItemSet* outItemSet
													,TelemetryNonce* outTelemetryNonce
													,netStatus* status
													,const u32 sessioncri)
{
	AddContrabandGameTransactionHttpTask* task = NULL;

	rtry
	{
		rcheck(trans.GetNumItems() > 0, 
			catchall,
			gnetError("AddContrabandGameTransactionHttpTask::Item list is empty"));

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("AddContrabandGameTransactionHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, trans, task, outPB, outItemSet, outTelemetryNonce, status, sessioncri);
	}
	rcatchall
	{

	}

	return false;
}

bool NetworkGameTransactions::SendResetBusinessProgressEvent(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status, const u32 sessioncri)
{
	ResetBusinessProgressHttpTask* task = nullptr;

	rtry
	{
		rcheck(trans.GetNumItems() > 0, 
			catchall,
			gnetError("ResetBusinessProgressHttpTask::Item list is empty"));

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("ResetBusinessProgressHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, trans, task, outPB, outItemSet, outTelemetryNonce, status, sessioncri);
	}
	rcatchall
	{

	}

	return false;
}

bool NetworkGameTransactions::SendUpdateBusinessGoodsEvent(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status, const u32 sessioncri)
{
	UpdateBusinessGoodsHttpTask* task = nullptr;

	rtry
	{
		rcheck(trans.GetNumItems() > 0, 
			catchall,
			gnetError("UpdateBusinessGoodsHttpTask::Item list is empty"));

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("UpdateBusinessGoodsHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, trans, task, outPB, outItemSet, outTelemetryNonce, status, sessioncri);
	}
	rcatchall
	{

	}

	return false;
}

bool NetworkGameTransactions::SendUpdateUpdateWarehouseVehicle(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status, const u32 sessioncri)
{
	UpdateWarehouseVehicleHttpTask* task = nullptr;

	rtry
	{
		rcheck(trans.GetNumItems() > 0, 
			catchall,
			gnetError("UpdateBusinessGoodsHttpTask::Item list is empty"));

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("UpdateBusinessGoodsHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, trans, task, outPB, outItemSet, outTelemetryNonce, status, sessioncri);
	}
	rcatchall
	{

	}

	return false;
}

bool NetworkGameTransactions::SendBonusEvent(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status, const u32 sessioncri)
{
	BonusEventHttpTask* task = nullptr;

	rtry
	{
		rcheck(trans.GetNumItems() > 0, 
			catchall,
			gnetError("BonusEventHttpTask::Item list is empty"));

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("BonusEventHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, trans, task, outPB, outItemSet, outTelemetryNonce, status, sessioncri);
	}
	rcatchall
	{

	}

	return false;
}


bool NetworkGameTransactions::SendSpendLimitedService(const int localGamerIndex
														,SpendEarnTransaction& trans
														,PlayerBalance* outPB
														,InventoryItemSet* outItemSet
														,TelemetryNonce* outTelemetryNonce
														,netStatus* status)
{
	SpendLimitedServiceGameTransactionHttpTask* task = NULL;

	rtry
	{
		rcheck(trans.GetNumItems() > 0, 
			catchall,
			gnetError("SpendLimitedServiceGameTransactionHttpTask::Item list is empty"));

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("SpendLimitedServiceGameTransactionHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, trans, task, outPB, outItemSet, outTelemetryNonce, status, 0);
	}
	rcatchall
	{

	}

	return false;
}

bool NetworkGameTransactions::SendEarnLimitedService(const int localGamerIndex
													,SpendEarnTransaction& trans
													,PlayerBalance* outPB
													,InventoryItemSet* outItemSet
													,TelemetryNonce* outTelemetryNonce
													,netStatus* status)
{
	EarnLimitedServiceGameTransactionHttpTask* task = NULL;

	rtry
	{
		rcheck(trans.GetNumItems() > 0, 
			catchall,
			gnetError("EarnLimitedServiceGameTransactionHttpTask::Item list is empty"));

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("EarnLimitedServiceGameTransactionHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, trans, task, outPB, outItemSet, outTelemetryNonce, status, 0);
	}
	rcatchall
	{

	}

	return false;
}

bool NetworkGameTransactions::SendSellVehicleEvent(const int localGamerIndex
												   ,SpendEarnTransaction& trans
												   ,PlayerBalance* outPB
												   ,InventoryItemSet* outItemSet
												   ,TelemetryNonce* outTelemetryNonce
												   ,netStatus* status
												   ,const u32 sessioncri)
{
	SellVehicleGameTransactionHttpTask* task = NULL;

	rtry
	{
		rcheck(trans.GetNumItems() > 0, 
			catchall,
			gnetError("SellVehicleGameTransactionHttpTask:: Item list is empty"));

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("SellVehicleGameTransactionHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, trans, task, outPB, outItemSet, outTelemetryNonce, status, sessioncri);
	}
	rcatchall
	{

	}

	return false;
}

bool NetworkGameTransactions::SendRemoveContrabandEvent(const int localGamerIndex
												        ,SpendEarnTransaction& trans
												        ,PlayerBalance* outPB
												        ,InventoryItemSet* outItemSet
												        ,TelemetryNonce* outTelemetryNonce
												        ,netStatus* status
												        ,const u32 sessioncri)
{
	RemoveContrabandGameTransactionHttpTask* task = NULL;

	rtry
	{
		rcheck(trans.GetNumItems() > 0, 
			catchall,
			gnetError("RemoveContrabandGameTransactionHttpTask:: Item list is empty"));

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("RemoveContrabandGameTransactionHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, trans, task, outPB, outItemSet, outTelemetryNonce, status, sessioncri);
	}
	rcatchall
	{

	}

	return false;
}

bool NetworkGameTransactions::SendEarnEvent(const int localGamerIndex
											,SpendEarnTransaction& trans
											,PlayerBalance* outPB
											,InventoryItemSet* outItemSet
											,TelemetryNonce* outTelemetryNonce
											,netStatus* status
											,const u32 sessioncri)
{
	EarnGameTransactionHttpTask* task = NULL;

	rtry
	{
		rcheck(trans.GetNumItems() > 0, 
			catchall,
			gnetError("EarnGameTransactionHttpTask - Item list is empty"));

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("EarnGameTransactionHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, trans, task, outPB, outItemSet, outTelemetryNonce, status, sessioncri);
	}
	rcatchall
	{

	}

	return false;
}


bool NetworkGameTransactions::SendUpdateStorageData(const int localGamerIndex, SpendEarnTransaction& trans, PlayerBalance* outPB, InventoryItemSet* outItemSet, TelemetryNonce* outTelemetryNonce, netStatus* status, const u32 sessioncri)
{
	UpdateStorageDataHttpTask* task = nullptr;

	rtry
	{
		rcheck(trans.GetNumItems() > 0, 
			catchall,
			gnetError("UpdateBusinessGoodsHttpTask::Item list is empty"));

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("UpdateBusinessGoodsHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, trans, task, outPB, outItemSet, outTelemetryNonce, status, sessioncri);
	}
	rcatchall
	{

	}

	return false;
}

bool NetworkGameTransactions::SendBuyCasinoChips(const int localGamerIndex
												  ,SpendEarnTransaction& trans
												  ,PlayerBalance* outPB
												  ,InventoryItemSet* outItemSet
												  ,TelemetryNonce* outTelemetryNonce
												  ,netStatus* status
												  ,const u32 sessioncri)
{
	BuyCasinoChipsHttpTask* task = NULL;

	rtry
	{
		rcheck(trans.GetNumItems() > 0, 
			catchall,
			gnetError("BuyVehicleGameTransactionHttpTask::Item list is empty"));

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("BuyVehicleGameTransactionHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, trans, task, outPB, outItemSet, outTelemetryNonce, status, sessioncri);
	}
	rcatchall
	{

	}

	return false;
}

bool NetworkGameTransactions::SendBuyUnlock(const int localGamerIndex
											, SpendEarnTransaction& trans
											, PlayerBalance* outPB
											, InventoryItemSet* outItemSet
											, TelemetryNonce* outTelemetryNonce
											, netStatus* status
											, const u32 sessioncri)

{
	BuyUnlockGameTransactionHttpTask* task = NULL;

	rtry
	{
		rcheck(trans.GetNumItems() > 0, 
			catchall,
			gnetError("BuyUnlockGameTransactionHttpTask::Item list is empty"));

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("BuyUnlockGameTransactionHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, trans, task, outPB, outItemSet, outTelemetryNonce, status, sessioncri);
	}
	rcatchall
	{

	}

	return false;
}


bool NetworkGameTransactions::SendSellCasinoChips(const int localGamerIndex
														,SpendEarnTransaction& trans
														,PlayerBalance* outPB
														,InventoryItemSet* outItemSet
														,TelemetryNonce* outTelemetryNonce
														,netStatus* status
														,const u32 sessioncri)
{
	SellCasinoChipsHttpTask* task = NULL;

	rtry
	{
		rcheck(trans.GetNumItems() > 0, 
			catchall,
			gnetError("RemoveContrabandGameTransactionHttpTask:: Item list is empty"));

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("RemoveContrabandGameTransactionHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, trans, task, outPB, outItemSet, outTelemetryNonce, status, sessioncri);
	}
	rcatchall
	{

	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//
//	DeleteSlot
//
//
//

bool NetworkGameTransactions::CreatePlayerAppearance(const int localGamerIndex
													 ,SpendEarnTransaction& trans
													 ,PlayerBalance* outPB
													 ,InventoryItemSet* outItemSet
													 ,TelemetryNonce* outTelemetryNonce
													 ,netStatus* status)
{
	CreatePlayerAppearanceTransactionHttpTask* task = NULL;

	rtry
	{
		rcheck(trans.GetNumItems() > 0, 
			catchall,
			gnetError("CreatePlayerAppearanceTransactionHttpTask:: Item list is empty"));

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("CreatePlayerAppearanceTransactionHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, trans, task, outPB, outItemSet, outTelemetryNonce, status, 0);
	}
	rcatchall
	{

	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//
//	DeleteSlot
//
//
//
class DeleteSlotResponseData
{
public:
	void Clear() {}
};

class DeleteSlotTransaction : public NetworkGameTransactions::GameTransactionBase
{
public:

	DeleteSlotTransaction()
		: GameTransactionBase()
		, m_slot(-1)
		, m_transfer(true)
		, m_clearbank(false)
		, m_reason(-1)
	{
	}

	DeleteSlotTransaction(int slot, bool transfer, const bool clearBank, const int reason)
		: GameTransactionBase()
		, m_slot(slot)
		, m_transfer(transfer)
		, m_clearbank(clearBank)
		, m_reason(reason)
	{
	}

	virtual bool SetNonce()
	{
		m_nounce = 1;
		gnetDebug1("DeleteSlotTransaction setting fake nounce of '%" I64FMT "d'", m_nounce);
		return true;
	}

	virtual bool Serialize(RsonWriter& wr) const
	{
		return GameTransactionBase::Serialize(wr)
			&& wr.WriteInt("slot", m_slot)
			&& wr.WriteBool("transfer", m_transfer)
			&& wr.WriteInt("reason", m_reason)

#if __FINAL
			&& wr.WriteBool("clearbank", false)
			&& gnetVerify(m_slot >= 0);
#else
			&& wr.WriteBool("clearbank", m_clearbank);
#endif //!__FINAL
	}

protected: 
	//For DeleteSlot, we don't want to include the 'catalogVersion' field when we send the transaction
	virtual bool	IncludeCatalogVersion() const { return false; };

	int  m_reason;
	int  m_slot;
	bool m_transfer;
	bool m_clearbank;
};

class GameTransactionDeleteSlotTask : public GetTokenAndRunHTTPTask<DeleteSlotTransactionHttpTask, DeleteSlotResponseData, DeleteSlotTransaction, NetworkGameTransactions::TelemetryNonce>
{
public:
	RL_TASK_DECL(GameTransactionDeleteSlotTask);
	RL_TASK_USE_CHANNEL(net_shop_gameTransactions);

	virtual bool ApplyServerData() const {return true;}
}; 

bool NetworkGameTransactions::DeleteSlot(const int localGamerIndex
										 ,const int slot
										 ,const bool clearbank
										 ,const bool bTransferFundsToBank
										 ,const int reason
										 ,TelemetryNonce* outTelemetryNonce
										 ,netStatus* status)
{
	GameTransactionDeleteSlotTask* task = NULL;

	rtry
	{
		gnetDebug1("NetworkGameTransactions::DeleteSlot - slot=%d, reason=%d", slot, reason);

		DeleteSlotTransaction deleteSlotTrans(slot, bTransferFundsToBank, clearbank, reason);

		rcheck(outTelemetryNonce, 
			catchall, 
			gnetError("outTelemetryNonce: Is NULL."));

		deleteSlotTrans.SetNonce();  //Virtualized version that doesn't burn a nonce.
		outTelemetryNonce->SetNonce(deleteSlotTrans.GetNonce());

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("DeleteSlotTransactionHttpTask: Failed to create"));

		rcheck(rlTaskBase::Configure(task, localGamerIndex, &deleteSlotTrans, (DeleteSlotResponseData*)NULL, outTelemetryNonce, status), 
			catchall,
			gnetError("GameTransactionDeleteSlotTask: Failed to Configure"));		                

		rcheck(rlGetTaskManager()->AppendSerialTask(GAME_TRANSACTION_TASK_QUEUE_ID, task), catchall,);

		return true;
	}
	rcatchall
	{
		if(task)							
		{																		
			if(task->IsPending())													
			{																		
				task->Cancel();						                    
			}

			if(status)
			{
				status->ForceFailed();
			}

			rlGetTaskManager()->DestroyTask(task);								    
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//
//  Allot/Recoup
//
//////////////////////////////////////////////////////////////////////////
class AllotGameTransactionHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("Allot");
public:
	virtual const char* GetServiceEndpoint() const { return "Allot"; }
	virtual bool ApplyServerData() const {return true;}

	virtual bool ProcessSuccess(const RsonReader &rr)
	{
		const bool result = GameTransactionHttpTask::ProcessSuccess(rr);

		if (m_pPlayerBalanceResponse && m_pPlayerBalanceResponse->GetBankDifference() > 0)
		{
			//Generate script event
			CEventNetworkCashTransactionLog tEvent(0, CEventNetworkCashTransactionLog::CASH_TRANSACTION_ATM, CEventNetworkCashTransactionLog::CASH_TRANSACTION_WITHDRAW, m_pPlayerBalanceResponse->GetBankDifference(), rlGamerHandle());
			GetEventScriptNetworkGroup()->Add(tEvent);
		}

		return result;
	}
};
BANK_ONLY(ForceErrorHelper AllotGameTransactionHttpTask::sm_errors;)

class RecoupGameTransactionHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("Recoup");
public:
	virtual const char* GetServiceEndpoint() const { return "Recoup"; }
	virtual bool ApplyServerData() const {return true;}

	virtual bool ProcessSuccess(const RsonReader &rr)
	{
		const bool result = GameTransactionHttpTask::ProcessSuccess(rr);

		if (m_pPlayerBalanceResponse && m_pPlayerBalanceResponse->GetBankDifference() < 0)
		{
			//Generate script event
			CEventNetworkCashTransactionLog tEvent(0, CEventNetworkCashTransactionLog::CASH_TRANSACTION_ATM, CEventNetworkCashTransactionLog::CASH_TRANSACTION_DEPOSIT, m_pPlayerBalanceResponse->GetBankDifference()*-1, rlGamerHandle());
			GetEventScriptNetworkGroup()->Add(tEvent);
		}

		return result;
	}
};
BANK_ONLY(ForceErrorHelper RecoupGameTransactionHttpTask::sm_errors;)

class AllotRecoupTransaction : public NetworkGameTransactions::GameTransactionBase
{
public:
	AllotRecoupTransaction(int slot, int amount)
		: GameTransactionBase()
		, m_slot(slot)
		, m_amount(amount)
	{
	}

	virtual bool Serialize(RsonWriter& wr) const
	{
		return GameTransactionBase::Serialize(wr)
			&& wr.WriteInt("slot", m_slot)
			&& gnetVerify(m_slot >= 0)
			&& wr.WriteInt("amt", m_amount);
	}

	int m_slot;
	int m_amount;
};

bool NetworkGameTransactions::Allot(const int localGamerIndex
									,const int slot
									,const int amount
									,PlayerBalance* outPB
									,TelemetryNonce* outTelemetryNonce
									,netStatus* status)
{
	AllotGameTransactionHttpTask* task = NULL;

	rtry
	{
		AllotRecoupTransaction trans(slot, amount);

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("EarnGameTransactionHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, trans, task, outPB, NULL, outTelemetryNonce, status, 0);
	}
	rcatchall
	{

	}

	return false;
}

bool NetworkGameTransactions::Recoup(const int localGamerIndex
									 ,const int slot
									 ,const int amount
									 ,PlayerBalance* outPB
									 ,TelemetryNonce* outTelemetryNonce
									 ,netStatus* status)
{
	RecoupGameTransactionHttpTask* task = NULL;

	rtry
	{
		AllotRecoupTransaction trans(slot, amount);

		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("EarnGameTransactionHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, trans, task, outPB, NULL, outTelemetryNonce, status, 0);
	}
	rcatchall
	{

	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//
//  Use/Acquire
//
//////////////////////////////////////////////////////////////////////////
class UseGameTransactionHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("Use");
public:
	virtual const char* GetServiceEndpoint() const { return "Use"; }
	virtual bool ApplyServerData() const {return true;}
};
BANK_ONLY(ForceErrorHelper UseGameTransactionHttpTask::sm_errors;)

class AcquireGameTransactionHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("Acquire");
public:
	virtual const char* GetServiceEndpoint() const { return "Acquire"; }
	virtual bool ApplyServerData() const {return true;}
};
BANK_ONLY(ForceErrorHelper AcquireGameTransactionHttpTask::sm_errors;)

bool NetworkGameTransactions::UseAcquireTransaction::Serialize( RsonWriter& wr ) const
{
	bool success = gnetVerify(m_slot >= 0)
		&& GameTransactionBase::Serialize(wr)
		&& wr.WriteInt("slot", m_slot);

	if (success)
	{
		wr.BeginArray("items", NULL);
		for (int i = 0; i < m_items.GetCount(); i++)
		{
			wr.Begin(NULL, NULL);
			m_items[i].Serialize(wr);
			wr.End();
		}
		wr.End();
	}

	return success;
}

bool NetworkGameTransactions::Use(const int localGamerIndex
								  ,UseAcquireTransaction& transactionData
								  ,TelemetryNonce* outTelemetryNonce
								  ,netStatus* status)
{
	UseGameTransactionHttpTask* task = NULL;
	rtry
	{
		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("EarnGameTransactionHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, transactionData, task, NULL, NULL, outTelemetryNonce, status, 0);
	}
	rcatchall
	{

	}

	return false;
}

bool NetworkGameTransactions::Acquire(const int localGamerIndex
									  ,UseAcquireTransaction& transactionData
									  ,TelemetryNonce* outTelemetryNonce
									  ,netStatus* status)
{
	AcquireGameTransactionHttpTask* task = NULL;
	rtry
	{
		rcheck(rlGetTaskManager()->CreateTask(&task), 
			catchall, 
			gnetError("EarnGameTransactionHttpTask: Failed to create"));

		return DoGameTransactionEvent(localGamerIndex, transactionData, task, NULL, NULL, outTelemetryNonce, status, 0);
	}
	rcatchall
	{

	}

	return false;
}


///////////////////////////////////////////////////////////////////////////
// 
//  NULL transaction
//
//////////////////////////////////////////////////////////////////////////
bool NetworkGameTransactions::SendNULLEvent( const int localGamerIndex, netStatus* status )
{
	NULLGameTransactionHttpTask* task = NULL;

#if __BANK
	int latency = 0;
	PARAM_netshoplatency.Get(latency);
	if( NETWORK_SHOPPING_MGR.Bank_GetOverrideLatency( ) ) 
		latency = NETWORK_SHOPPING_MGR.Bank_GetLatency();
#else
	int latency = 0;
#endif

	rtry
	{
		rverify(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex)
			,catchall
			,gnetError("INVALID GAMER INDEX"));

		rcheck(rlGetTaskManager()->CreateTask(&task)
			,catchall 
			,gnetError("NULLGameTransactionHttpTask: Failed to create"));

		rcheck(rlTaskBase::Configure(task, latency, status)
			,catchall
			,gnetError("NULLGameTransactionHttpTask: Failed to Configure"));		                

		rcheck(rlGetTaskManager()->AppendSerialTask(GAME_TRANSACTION_TASK_QUEUE_ID, task)
			,catchall
			,);			            

		return true;
	}
	rcatchall
	{

	}

	return false;
}

///////////////////////////////////////////////////////////////////////////
// 
//  Purch transaction
//
//////////////////////////////////////////////////////////////////////////

class PurchGameTransactionHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("Purch");
public:
    virtual const char* GetServiceEndpoint() const { return "Purch"; }
	virtual bool ApplyServerData() const {return true;}
};
BANK_ONLY(ForceErrorHelper PurchGameTransactionHttpTask::sm_errors;)

bool NetworkGameTransactions::PurchTransaction::Serialize( RsonWriter& wr ) const
{
    bool success = GameTransactionBase::Serialize(wr);

	success &= wr.WriteString("entitlementCode", m_EntitlementCode)
        && wr.WriteInt("quantity", m_Amount)
        && wr.WriteInt64("instanceId", m_EntitlementInstanceId);

    return success;
}

bool NetworkGameTransactions::Purch(int localGamerIndex
									,const char* codeToConsume
									,s64 instanceIdToConsume
									,int numToConsume
									,PlayerBalance* outPB
									,TelemetryNonce* outTelemetryNonce
									,netStatus* status)
{
    PurchTransaction purchTransaction(codeToConsume,instanceIdToConsume,numToConsume);
    return Purch(localGamerIndex,purchTransaction, outPB, outTelemetryNonce, status);
}

bool NetworkGameTransactions::Purch(const int localGamerIndex
									,PurchTransaction& transactionData
									,PlayerBalance* outPB
									,TelemetryNonce* outTelemetryNonce
									,netStatus* status)
{
    PurchGameTransactionHttpTask * task = NULL;
    rtry
    {
        rcheck(rlGetTaskManager()->CreateTask(&task), 
            catchall, 
            gnetError("PurchGameTransactionHttpTask: Failed to create"));

        return DoGameTransactionEvent(localGamerIndex, transactionData, task, outPB, NULL, outTelemetryNonce, status, 0);
    }
    rcatchall
    {
            
    }

    return false;
}

#if __BANK
NetworkGameTransactions::PlayerBalance    s_bankPlayerBalance;
NetworkGameTransactions::InventoryItemSet s_bankItemSet;
NetworkGameTransactions::TelemetryNonce   s_bankTelemetryNonce;

char networkTransactionPurchId[128];
char networkTransactionPurchEntitlementCode[128];
long bankSpendItemId = -176143638; //WP_WT_PARA_t4_v0
long bankSpendBankAmount = 0;
long bankSpendWalletAmount = 500;
long bankEarnItemId = -1186079845; //SERVICE_EARN_ARMORED_TRUCKS
long bankEarnBankAmount = 0;
long bankEarnWalletAmount = 500;
void NetworkGameTransactions::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Game Server Transactions");
		bank.AddButton("Request Service Token", datCallback(CFA(BankRequestToken)));
		bank.AddButton("Request Catalog", datCallback(CFA(BankRequestCatalog)));
		bank.PushGroup("Events");
			bank.AddButton("Send SessionStart", datCallback(CFA(BankSessionStart)));
			bank.PushGroup("Transactions");
				bank.AddText("Spend ItemId", &bankSpendItemId, false);
				bank.AddText("Spend Bank Amount", &bankSpendBankAmount, false);
				bank.AddText("Spend Wallet Amount", &bankSpendWalletAmount, false);
				bank.AddButton("Send Spend", datCallback(CFA(BankSpend)));
				bank.AddText("Earn ItemId", &bankEarnItemId, false);
				bank.AddText("Earn Bank Amount", &bankEarnBankAmount, false);
				bank.AddText("Earn Wallet Amount", &bankEarnWalletAmount, false);
				bank.AddButton("Send Earn", datCallback(CFA(BankEarn)));
				bank.AddButton("Delete Slot", datCallback(CFA(BankDeleteSlot)));
				bank.PushGroup("Purch");
					bank.AddText("Purch Instance Id", networkTransactionPurchId, sizeof(networkTransactionPurchId), false);
					bank.AddText("Purch Entitlement Code", networkTransactionPurchEntitlementCode, sizeof(networkTransactionPurchEntitlementCode), false);
					bank.AddButton("Send Purch", datCallback(CFA(BankPurch)));
				bank.PopGroup();
			bank.PopGroup();
		bank.PopGroup();
	bank.PopGroup();
}


void NetworkGameTransactions::BankRequestToken()
{
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	rlRos::RequestServiceAccessInfo(localGamerIndex);
}

void NetworkGameTransactions::BankSpend()
{
	static netStatus spendNetStatus;

	if (spendNetStatus.Pending())
	{
		return;
	}
	else
	{
		spendNetStatus.Reset();
	}

	long bank = bankSpendBankAmount;
	long wallet = bankSpendWalletAmount;
	int slot = 0;
	int price = bankSpendBankAmount+bankSpendWalletAmount;

	//Find this item in the catalog data
	const netCatalog& catalog = netCatalog::GetInstance();
	atHashString itemIdHashStr((u32)bankSpendItemId);
	const netCatalogBaseItem* catItemBase = catalog.Find(itemIdHashStr);
	if (catItemBase)
	{
		price = catItemBase->GetPrice();
	}


	SpendEarnTransaction sp(slot, bank, wallet);
	sp.PushItem(bankSpendItemId, 1, price, false);
	
	gnetVerify(NetworkGameTransactions::SendSpendEvent(0, sp, &s_bankPlayerBalance, &s_bankItemSet, &s_bankTelemetryNonce, &spendNetStatus, 0));
}

void NetworkGameTransactions::BankEarn()
{
	static netStatus earnNetStatus;

	if (earnNetStatus.Pending())
	{
		return;
	}
	else
	{
		earnNetStatus.Reset();
	}

	long bank = bankEarnBankAmount;
	long wallet = bankEarnWalletAmount;
	int slot = 0;
	int price = bankEarnWalletAmount+bankEarnBankAmount;


	SpendEarnTransaction sp(slot, bank, wallet);
	sp.PushItem(bankEarnItemId, 1, price, false);

	gnetVerify(NetworkGameTransactions::SendEarnEvent(0, sp, &s_bankPlayerBalance, &s_bankItemSet, &s_bankTelemetryNonce, &earnNetStatus, 0));
}

void NetworkGameTransactions::BankDeleteSlot()
{
	static netStatus deleteNetStatus;
	gnetVerify(NetworkGameTransactions::DeleteSlot(0, 0, false, true, 930488515, &s_bankTelemetryNonce, &deleteNetStatus));
}

void NetworkGameTransactions::BankRequestCatalog()
{
	if(netCatalog::GetInstance().GetStatus().Pending())
		CATALOG_SERVER_INST.Cancel();
	
	CATALOG_SERVER_INST.RequestServerFile();
}


void NetworkGameTransactions::BankSessionStart()
{
	int slot = 0;
	GameTransactionSessionMgr::Get().SessionStart(NetworkInterface::GetLocalGamerIndex(), slot, true);
}


void NetworkGameTransactions::BankSessionRestart()
{
	GameTransactionSessionMgr::Get().FlagForSessionRestart();
}

void NetworkGameTransactions::BankPurch()
{
    static netStatus status;
    static PurchTransaction purchTransaction(networkTransactionPurchEntitlementCode,atoi(networkTransactionPurchId),1);
    NetworkGameTransactions::Purch(0,purchTransaction, &s_bankPlayerBalance, &s_bankTelemetryNonce, &status);
}


#endif

#if !__FINAL
class InitBalanceGameTransactionHttpTask : public GameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("SessionInitBalance");
public:
	virtual const char* GetServiceEndpoint() const { return "SessionInitBalance"; }
	virtual bool UseChallengeResponseInfo() const { return false; }
};
BANK_ONLY(ForceErrorHelper InitBalanceGameTransactionHttpTask::sm_errors;)

namespace NetworkGameTransactions
{
class InitBalanceTransaction : public GameTransactionBase
{
public:
	InitBalanceTransaction(const PlayerBalance& pb, const int slot)
		: GameTransactionBase()
		, m_playerBalance(pb)
		, m_slot(slot)
	{
	}

	virtual bool Serialize( RsonWriter& wr ) const
	{
		return	gnetVerify(m_slot >= 0)
				&& wr.WriteInt("slot", m_slot)
				&& m_playerBalance.Serialize(wr);
	}
protected:
	int m_slot;
	PlayerBalance m_playerBalance;
};


} //NetworkGameTransactions

#endif //__FINAL

//////////////////////////////////////////////////////////////////////////
//
//	CatalogVersion
//
//////////////////////////////////////////////////////////////////////////

class GetCatalogVersionHttpTask : public GameTransactionBaseHttpTask
{
	typedef GameTransactionBaseHttpTask Base;

	GAME_SERVER_ERROR_BANK("GetCatalogVersion");

public:	
	RL_TASK_DECL(GetCatalogVersionHttpTask);
	RL_TASK_USE_CHANNEL(net_shop_gameTransactions);

	GetCatalogVersionHttpTask() 
		: Base()
	{
	}

	virtual const char* GetServiceEndpoint() const { return "GetCatalogVersion"; }

	virtual bool UseChallengeResponseInfo() const { return false; }
	virtual bool       HasTransactionData() const { return false; }

	bool Configure( const int localGamerIndex)
	{
		RsonWriter wr;
		wr.InitNull(RSON_FORMAT_JSON);
		return Base::Configure(localGamerIndex, 0, wr);
	}

protected:
	//We want to call this before we get a GSToken
	virtual bool UsesGSToken() const { return false; } 

	//Custom handling of catalog info
	virtual bool ProcessResponseCatalogInfo(const RsonReader& rr);
};
BANK_ONLY(ForceErrorHelper GetCatalogVersionHttpTask::sm_errors;)

bool GetCatalogVersionHttpTask::ProcessResponseCatalogInfo(const RsonReader& rr)
{
	bool success = false;

	rtry
	{
		rlTaskDebug("GetCatalogVersionHttpTask::ProcessResponseCatalogInfo()");

		u32 latestCatalogVersion = 0;

		rcheck(rr.ReadUns("CurrentCatalogVersion", latestCatalogVersion)
			,catchall
			,rlTaskError("No 'CurrentCatalogVersion' element"));

		rcheck(latestCatalogVersion > 0
			,catchall
			,rlTaskError("Invalid catalog version='%d',", latestCatalogVersion));

		u32 currentCatalogCRC = 0;

		rcheck(rr.ReadUns("CatalogCRC", currentCatalogCRC)
			,catchall
			,rlTaskError("No 'CatalogCRC' element"));

		rcheck(currentCatalogCRC > 0
			,catchall
			,rlTaskError("Invalid catalog crc='%d',", currentCatalogCRC));

		netCatalog::GetInstance().SetLatestVersion(latestCatalogVersion, currentCatalogCRC);

		success = true;
	}
	rcatchall
	{
	}

	return success;
}

//////////////////////////////////////////////////////////////////////////
//
//	Catalog/Start
//
//////////////////////////////////////////////////////////////////////////
class GetCatalogGameTransactionHttpTask : public GameTransactionBaseHttpTask
{
	typedef GameTransactionBaseHttpTask Base;

	GAME_SERVER_ERROR_BANK("GetCatalog");

private:
	static const u32 MEMORY_NEEEDED = 1024*1024; //Kb

public:	
	RL_TASK_DECL(GetCatalogGameTransactionHttpTask);
	RL_TASK_USE_CHANNEL(net_shop_gameTransactions);

	// Use the Streaming memory, if we don't have enough we can try using the network heap.
	GetCatalogGameTransactionHttpTask() : Base(NULL, GameTransactionSessionMgr::Get().GetBounceBuffer(), GameTransactionSessionMgr::Get().GetBounceBufferSize())
	{
		m_GrowBuffer.Init(GameTransactionSessionMgr::Get().GetGrowBuffer(), GameTransactionSessionMgr::Get().GetGrowBufferSize(), datGrowBuffer::NULL_TERMINATE);
		gnetDebug1("Stearming memory available '%d'.", strStreamingEngine::GetAllocator().GetVirtualMemoryFree());
	}

	virtual const char* GetServiceEndpoint() const { return "GetCatalog"; }
	virtual const char* GetServerAppName() const {return "GamePlayServices";}
	virtual const char* ContentType() const {return NULL;}

	virtual netHttpVerb GetHttpVerb() const 
	{
#if !__FINAL
		int catalogVersion = 0;
		if (PARAM_catalogVersion.Get(catalogVersion) && catalogVersion > 0)
		{
			return NET_HTTP_VERB_POST;
		}
#endif //!__FINAL

		return NET_HTTP_VERB_GET;
	}

	virtual bool UseChallengeResponseInfo() const { return false; }
	virtual bool       HasTransactionData() const { return false; }

	bool Configure( const int localGamerIndex)
	{
		rtry
		{
#if !__FINAL
			int catalogVersion = 0;
			if (PARAM_catalogVersion.Get(catalogVersion) && catalogVersion > 0)
			{
				char dataBuffer[GAME_TRANSACTION_SIZE_OF_RSONWRITER];
				RsonWriter wr(dataBuffer, RSON_FORMAT_JSON);

				rverify(wr.Begin(NULL, NULL)
					,catchall
					,rlTaskError("GetCatalogGameTransactionHttpTask - Failed to wr.Begin()."));

				rverify(wr.WriteInt("catalogVersion", catalogVersion)
					,catchall
					,rlTaskError("GetCatalogGameTransactionHttpTask - Failed to 'catalogVersion' - '%d'.", catalogVersion));

				rverify(wr.End()
					,catchall
					,rlTaskError("GetCatalogGameTransactionHttpTask - Failed to wr.End() - %s", wr.ToString()));

				rverify(Base::Configure(localGamerIndex, 0, wr)
					,catchall
					,rlTaskError("GetCatalogGameTransactionHttpTask - Failed to configure."));
			}
			else
#endif //!__FINAL
			{
				RsonWriter wr;
				wr.InitNull(RSON_FORMAT_INVALID);

				rverify(Base::Configure(localGamerIndex, 0, wr)
					,catchall
					,rlTaskError("GetCatalogGameTransactionHttpTask - Failed to configure."));
			}

			return true;
		}
		rcatchall
		{

		}

		return false;
	}

protected:

	virtual bool UsesGSToken() const { return false; } //The session Transactions don't use the GSToken.

	virtual bool ProcessResponse(const char* response, int& resultCode);

	virtual bool ProcessSuccess(const RsonReader &UNUSED_PARAM(rr))
	{
		gnetAssertf(0, "GetCatalogGameTransactionHttpTask::ProcessSuccess is unused due to needing different paramaters.");
		return false;
	}

	bool ProcessError( const RsonReader &UNUSED_PARAM(rr) )
	{
		gnetAssertf(0, "GetCatalogGameTransactionHttpTask::ProcessSuccess is unused due to needing different paramaters.");
		return false;
	}

	//PURPOSE
	//  Called when the base class finishes this finish can be delayed due to Latency testing.
	virtual void OnFinish(const int resultCode);
};
BANK_ONLY(ForceErrorHelper GetCatalogGameTransactionHttpTask::sm_errors;)

void GetCatalogGameTransactionHttpTask::OnFinish(const int resultCode)
{
	if (resultCode >= 400)
	{
		const u32 inContentLen = GetResponseContentLength();
		if (inContentLen > GameTransactionSessionMgr::Get().GetGrowBufferSize())
		{
			gnetAssertf(0, "GetCatalogGameTransactionHttpTask::OnFinish - Grow Buffer is too small. needed='%u', current size='%u'.", inContentLen, GameTransactionSessionMgr::Get().GetGrowBufferSize());
			GameTransactionSessionMgr::Get().IncreaseGrowBufferSize();
		}
	}
#if !__NO_OUTPUT
	else 
	{
		const u32 inContentLen = GetResponseContentLength() + (1024*64);
		if (inContentLen > GameTransactionSessionMgr::Get().GetGrowBufferSize())
		{
			gnetAssertf(0, "GetCatalogGameTransactionHttpTask::OnFinish - Grow Buffer is soon to be too small. needed='%u', current size='%u'.", GetResponseContentLength(), GameTransactionSessionMgr::Get().GetGrowBufferSize());
		}
	}
#endif // !__NO_OUTPUT

	GameTransactionBaseHttpTask::OnFinish(resultCode);

}

bool GetCatalogGameTransactionHttpTask::ProcessResponse(const char* response, int& resultCode)
{
	bool success = false;
	bool foundZipped = false;
	resultCode = 0;

	//We need the length of the response, which is contained in the grow buffer.
	unsigned int responseLength = m_GrowBuffer.Length();

	if(response && responseLength)
	{
		rtry
		{
			foundZipped = true;
			rverify(CATALOG_SERVER_INST.OnServerSuccess(response, responseLength), catchall, rlTaskError("Failed to deserialize responce responseLength=%d", responseLength));

			success = true;
			resultCode = 0;
		}
		rcatchall
		{
			success = false;
			resultCode = BAIL_CTX_CATALOG_DESERIALIZE_FAILED;
		}
	}

	gnetAssertf(foundZipped, "GetCatalogGameTransactionHttpTask::ProcessResponse unable to find the zipped data in the responce (%s).", response);

#if !__FINAL

	//Don't fail on catalog download failures.
	if (PARAM_nocatalog.Get())
	{
		rlTaskError("GetCatalogGameTransactionHttpTask::ProcessResponse - Failed to retrieve catalog bypass this fuck (%s).", response);
		success = true;
		resultCode = 0;
	}

#endif // !__FINAL

	return success;
}


//////////////////////////////////////////////////////////////////////////
//
//	Session/Start
//
//////////////////////////////////////////////////////////////////////////

void NetworkGameTransactions::Cancel(netStatus* status)
{
	rlGetTaskManager()->CancelTask(status);
}

bool NetworkGameTransactions::SessionStart(const int localGamerIndex, const int slot)
{
	return GameTransactionSessionMgr::Get().SessionStart(localGamerIndex, slot, true);
}

bool NetworkGameTransactions::GetCatalogVersion(const int localGamerIndex, netStatus* status)
{
	GetCatalogVersionHttpTask* task = NULL;

	rtry																				
	{
		rverify(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex)
			,catchall
			,gnetError("INVALID GAMER INDEX"));

		rcheck(rlGetTaskManager()->CreateTask(&task)
			,catchall
			,gnetError("NetworkGameTransactions::GetCatalogVersion: Failed to create"));

		rcheck(rlTaskBase::Configure(task, localGamerIndex, status)
			,catchall
			,gnetError("GetCatalogVersionHttpTask<%s>: Failed to Configure", task->GetServiceEndpoint()));

		rcheck(rlGetTaskManager()->AddParallelTask(task)
			,catchall
			,gnetError("GetCatalogVersionHttpTask: Failed to AddParallelTask"));

		return true;
	}
	rcatchall
	{
		if(task)
		{
			if(task->IsPending())
			{
				task->Finish(rlTaskBase::FINISH_FAILED, BAIL_CTX_CATALOG_VERSION_TASKCONFIGURE_FAILED);
			}
			else
			{											
				status->ForceFailed(BAIL_CTX_CATALOG_VERSION_TASKCONFIGURE_FAILED);
			}

			rlGetTaskManager()->DestroyTask(task);								    
		}	
	}

	return false;
}

bool NetworkGameTransactions::GetCatalog(const int localGamerIndex, netStatus* status)
{
	GetCatalogGameTransactionHttpTask* task = NULL;

	rtry																				
	{
		rverify(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex)
			,catchall
			,gnetError("INVALID GAMER INDEX"));

		rcheck(rlGetTaskManager()->CreateTask(&task)
			,catchall 
			,gnetError("NetworkGameTransactions::GetCatalog: Failed to create"));

		rcheck(rlTaskBase::Configure(task, localGamerIndex, status)
			,catchall
			,gnetError("GetCatalogGameTransactionHttpTask<%s>: Failed to Configure", task->GetServiceEndpoint()));		                

		rcheck(rlGetTaskManager()->AddParallelTask(task), catchall,);

		return true;																	
	}
	rcatchall
	{
		if(task)							
		{																		
			if(task->IsPending())
			{
				task->Finish(rlTaskBase::FINISH_FAILED, BAIL_CTX_CATALOG_TASKCONFIGURE_FAILED);
			}
			else																	
			{																		
				status->ForceFailed(BAIL_CTX_CATALOG_TASKCONFIGURE_FAILED);													
			}

			rlGetTaskManager()->DestroyTask(task);								    
		}	
	}

	return false;
}

namespace NetworkGameTransactions
{
#if __BANK
	void  Bank_InitWidgets(bkBank& bank)
	{
		DeleteSlotTransactionHttpTask::Bank_InitWidgets(bank);
		SpendGameTransactionHttpTask::Bank_InitWidgets(bank);
		SpendLimitedServiceGameTransactionHttpTask::Bank_InitWidgets(bank);
		EarnGameTransactionHttpTask::Bank_InitWidgets(bank);
		EarnLimitedServiceGameTransactionHttpTask::Bank_InitWidgets(bank);
		BuyItemGameTransactionHttpTask::Bank_InitWidgets(bank);
		BuyPropertyGameTransactionHttpTask::Bank_InitWidgets(bank);
		BuyVehicleGameTransactionHttpTask::Bank_InitWidgets(bank);
		SellVehicleGameTransactionHttpTask::Bank_InitWidgets(bank);
		SpendVehicleModTransactionHttpTask::Bank_InitWidgets(bank);
		CreatePlayerAppearanceTransactionHttpTask::Bank_InitWidgets(bank);
		AllotGameTransactionHttpTask::Bank_InitWidgets(bank);
		RecoupGameTransactionHttpTask::Bank_InitWidgets(bank);
		UseGameTransactionHttpTask::Bank_InitWidgets(bank);
		AcquireGameTransactionHttpTask::Bank_InitWidgets(bank);
		PurchGameTransactionHttpTask::Bank_InitWidgets(bank);
		InitBalanceGameTransactionHttpTask::Bank_InitWidgets(bank);
		HeartBeatGameTransactionHttpTask::Bank_InitWidgets(bank);
		SessionStartGameTransactionHttpTask::Bank_InitWidgets(bank);
		SessionRestartGameTransactionHttpTask::Bank_InitWidgets(bank);
		GetBalanceGameTransactionHttpTask::Bank_InitWidgets(bank);
		GetCatalogVersionHttpTask::Bank_InitWidgets(bank);
		GetCatalogGameTransactionHttpTask::Bank_InitWidgets(bank);
		BuyWarehouseGameTransactionHttpTask::Bank_InitWidgets(bank);
		BuyContrabandMissionGameTransactionHttpTask::Bank_InitWidgets(bank);
		AddContrabandGameTransactionHttpTask::Bank_InitWidgets(bank);
		RemoveContrabandGameTransactionHttpTask::Bank_InitWidgets(bank);
	}
#endif // __BANK
} // NetworkGameTransactions

///Clear out our channel.
#undef __net_channel

#endif // __NET_SHOP_ACTIVE

//eof 
