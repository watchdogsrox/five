
#include "Network/NetworkTypes.h"

#if __NET_SHOP_ACTIVE

#include "GameTransactionsSessionMgr.h"

//Rage includes
#include "diag/seh.h"
#include "data/rson.h"
#include "system/param.h"
#include "system/system.h"
#include "system/threadtype.h"
#include "rline/ros/rlros.h"
#include "rline/ros/rlroscommon.h"

//Framework includes
#include "fwnet/netchannel.h"

//Game includes
#include "Network/Shop/Inventory.h"
#include "Network/Shop/Catalog.h"
#include "Network/Shop/NetworkShopping.h"
#include "Network/Live/Livemanager.h"
#include "Network/Live/NetworkProfileStats.h"
#include "Network/NetworkInterface.h"
#include "Network/NetworkTypes.h"
#include "Network/Sessions/NetworkSession.h"
#include "Event/EventGroup.h"
#include "Event/EventNetwork.h"
#include "Stats/StatsMgr.h"
#include "Stats/StatsDataMgr.h"
#include "Stats/StatsInterface.h"
#include "Stats/StatsTypes.h"
#include "streaming/streamingengine.h"

NETWORK_OPTIMISATIONS();
NETWORK_SHOP_OPTIMISATIONS();

RAGE_DECLARE_SUBCHANNEL(net, shop_gameTransactions);

#undef __net_channel
#define __net_channel net_shop_gameTransactions

XPARAM(netGameTransactionsNoToken);
PARAM(inventoryForceApplyData, "Force application of the data from the server");
XPARAM(catalogVersion);

BANK_ONLY(ForceErrorHelper SessionStartGameTransactionHttpTask::sm_errors;)
BANK_ONLY(ForceErrorHelper SessionRestartGameTransactionHttpTask::sm_errors;)
BANK_ONLY(ForceErrorHelper GetBalanceGameTransactionHttpTask::sm_errors;)
BANK_ONLY(ForceErrorHelper HeartBeatGameTransactionHttpTask::sm_errors;)

//////////////////////////////////////////////////////////////////////////
// GetAccessTokenHelper
//////////////////////////////////////////////////////////////////////////

void GetAccessTokenHelper::Init(int localGamerIndex)
{
	m_localGamerIndex = localGamerIndex;
	m_retryCount = 0;

#if RLROS_USE_SERVICE_ACCESS_TOKENS
	//We need to make sure we have a valid token before we start.
	const rlRosServiceAccessInfo& tokenInfo = rlRos::GetServiceAccessInfo(m_localGamerIndex);
	if (!tokenInfo.IsValid() && !PARAM_netGameTransactionsNoToken.Get())
	{
		//Request the token
		if(gnetVerify(rlRos::RequestServiceAccessInfo(m_localGamerIndex)))
		{
			m_state = WAITING_FOR_TOKEN;
		}
		else
		{
			gnetError("Failed to request token for index %d", m_localGamerIndex);
			m_state = TOKEN_FAILED;
		}
	}
	else
#endif // RLROS_USE_SERVICE_ACCESS_TOKENS
	{
		gnetDebug1("GetAccessTokenHelper::Init - Calling task immediately");
		m_state = TOKEN_RECEIVED;
	}
}

void GetAccessTokenHelper::Shutdown()
{
	m_localGamerIndex = -1;
	m_state = IDLE;
	m_retryCount = 0;
}


void GetAccessTokenHelper::Update()
{
#if RLROS_USE_SERVICE_ACCESS_TOKENS	
	switch (m_state)
	{
	case IDLE:
		break;
	case WAITING_FOR_TOKEN:
		{
			//We need to make sure we have a valid token
			const rlRosServiceAccessInfo& tokenInfo = rlRos::GetServiceAccessInfo(m_localGamerIndex);

			//Make sure it's still pending
			int retryCount = 0;
			int retryTimeMs = 0;
			if(tokenInfo.IsValid())
			{
				gnetDebug1("Token received");
				m_state = TOKEN_RECEIVED;
			}
			else if(rlRos::IsServiceAccessRequestPending(m_localGamerIndex, retryCount, retryTimeMs))
			{
				//Check to see if our retry count changed
				if(m_retryCount != retryCount)
				{
					gnetDebug1("Retry token %d fired for %d ms", retryCount, retryTimeMs);
					//Send an event that we did a retry.
					GetEventScriptNetworkGroup()->Add(CEventNetworkRequestDelay(retryTimeMs));
					m_retryCount = retryCount;
				}
			}
			else //token not valid and not pending...boo.
			{
				gnetDebug1("Failed because token request failed");
				m_state = TOKEN_FAILED;
			}
		}
		break;
	case TOKEN_RECEIVED:
		{
			//We need to make sure we still have a valid token
			const rlRosServiceAccessInfo& tokenInfo = rlRos::GetServiceAccessInfo(m_localGamerIndex);

			//If it's not valid anymore, kick it off again.
			if(!tokenInfo.IsValid() && !PARAM_netGameTransactionsNoToken.Get())
			{
				gnetDebug1("Token found invalid.  Calling GetAccessTokenHelper::Init()");
				Init(m_localGamerIndex);
			}
		}
		break;
	case TOKEN_FAILED:
		break;
	}
#else	//RLROS_USE_SERVICE_ACCESS_TOKENS
	gnetDebug1("GetAccessTokenHelper::Update - Calling task immediately");
	m_state = TOKEN_RECEIVED;
#endif 
}

const rlRosServiceAccessInfo* GetAccessTokenHelper::GetAccessToken() const
{
#if RLROS_USE_SERVICE_ACCESS_TOKENS
	return &rlRos::GetServiceAccessInfo(m_localGamerIndex);
#else
	return NULL;
#endif
}


//////////////////////////////////////////////////////////////////////////
// SessionStartResponseData::sSlotCreateTimestamp
//////////////////////////////////////////////////////////////////////////

bool SessionStartResponseData::sSlotCreateTimestamp::Deserialize( const RsonReader& rr )
{
	rtry
	{
		RsonReader createTimes;

		if (gnetVerifyf(rr.GetMember("createTimes", &createTimes), "SessionStartResponseData: failed to find 'createTimes' array"))
		{
			rverify(createTimes.AsArray(m_posixtime, MAX_NUM_MP_CHARS), catchall, gnetError("SessionStartResponseData: faile to parse 'createTimes' array"));
		}

		for (int i=0; i<MAX_NUM_MP_CHARS; i++)
			gnetDebug1("Character creation times <slot %d='%" I64FMT "d'>", i, m_posixtime[i]);

		return true;
	}
	rcatchall
	{
	}

	return false;
}

bool SessionStartResponseData::sSlotCreateTimestamp::IsValid(const int slot) const
{
	if (gnetVerify(0 <= slot) && gnetVerify(slot < MAX_NUM_MP_CHARS))
	{
		return (m_posixtime[slot] > 0);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
// BaseSessionGameTransactionHttpTask
//////////////////////////////////////////////////////////////////////////

bool BaseSessionGameTransactionHttpTask::Configure(const int localGamerIndex
												   ,const u32 challengeResponseInfo
												   ,RsonWriter& wr
												   ,SessionStartResponseData* outResponseData
												   ,NetworkGameTransactions::TelemetryNonce* outTelemetryNonce)
{
	rtry
	{
		rverify(GameTransactionHttpTask::Configure(localGamerIndex, challengeResponseInfo, wr, outTelemetryNonce)
			,catchall
			,gnetError("Failed to GameTransactionHttpTask::Configure()."));

		m_pResponseData = outResponseData;

		return true;
	}
	rcatchall
	{

	}

	return false;
}

bool BaseSessionGameTransactionHttpTask::DeserializePlayerInfo(const RsonReader& rr)
{
	rtry
	{
		RsonReader respReader;

		rcheck(rr.ReadReader("playerBalance", respReader)
			,catchall
			,rlTaskError("No 'playerBalance' object found"));

		rcheck(m_pResponseData->m_playerBalance.Deserialize(respReader)
			,catchall
			,rlTaskError("Failed to deserialize 'playerBalance'"));

		rcheck(rr.ReadReader("inventoryItems", respReader)
			,catchall
			,rlTaskError("No 'InventoryItems' object found"));

		rcheck(m_pResponseData->m_inventory.Deserialize(respReader)
			,catchall
			,rlTaskError("Failed to deserialize 'InventoryItems'"));

		rcheck(m_pResponseData->m_slotCreateTimestamp.Deserialize(rr)
			,catchall
			,rlTaskError("Failed to deserialize 'createTime'"));

		rcheck(rr.ReadValue("timeIntervalSec", m_pResponseData->m_heartBeatSec)
			,catchall
			,rlTaskError("Failed to parse 'timeIntervalSec'"));

		return true;
	}
	rcatchall
	{
		gnetError("Failed to DeserializePlayerInfo");
	}

	return false;
}

bool BaseSessionGameTransactionHttpTask::DeserializeSessionInfo(const RsonReader& rr)
{
	rtry
	{
		rcheck(rr.ReadString("gsToken",m_pResponseData->m_gsToken.m_token)
			,catchall
			,rlTaskError("No 'gsToken' found"));

		rcheck(rr.ReadUns64("transactionNonceSeed", m_pResponseData->m_nonceSeed)
			,catchall
			,rlTaskError("No 'transactionNonceSeed' found"));

		u32 currentCatalogCRC = 0;
		u32 currentCatalogVersion = 0;
		if(gnetVerify(rr.ReadUns("CurrentCatalogVersion", currentCatalogVersion))
			&& gnetVerify(rr.ReadUns("CatalogCRC", currentCatalogCRC)))
		{
#if !__FINAL
			int catalogVersion = 0;
			if(!PARAM_catalogVersion.Get(catalogVersion))
				catalogVersion = 0;

			if(catalogVersion > 0)
			{
				//Ignore 'CurrentCatalogVersion' when we want to override it.
			}
			else
#endif //!__FINAL
			{
				netCatalog::GetInstance().SetLatestVersion(currentCatalogVersion, currentCatalogCRC);
			}
		}

		return true;
	}
	rcatchall
	{
		gnetError("Failed on DeserializeSessionInfo");
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
// HeartBeatGameTransactionHttpTask
//////////////////////////////////////////////////////////////////////////

bool HeartBeatGameTransactionHttpTask::Configure(const int localGamerIndex
												 ,const u32 challengeResponseInfo
												 ,RsonWriter& wr
												 ,SessionStartResponseData* outResponseData
												 ,NetworkGameTransactions::TelemetryNonce* outTelemetryNonce
												 ,u32* timeIntervalSec
												 ,u32* failedAttempts)
{
	rtry
	{
		rverify(BaseSessionGameTransactionHttpTask::Configure(localGamerIndex, challengeResponseInfo, wr, outResponseData, outTelemetryNonce)
				,catchall
				,gnetError("Failed to configure BaseSessionGameTransactionHttpTask::Configure."));

		m_timeIntervalMs = timeIntervalSec;
		m_failedAttempts = failedAttempts;

		return true;
	}
	rcatchall
	{

	}

	return false;
}

bool HeartBeatGameTransactionHttpTask::DeserializePlayerInfo(const RsonReader& rr)
{
	rtry
	{
		RsonReader respReader;
		rcheck(rr.ReadReader("playerBalance", respReader), catchall, rlTaskError("No 'playerBalance' object found"));
		rcheck(m_pResponseData->m_playerBalance.Deserialize(respReader), catchall, rlTaskError("Failed to deserialize 'playerBalance'"));

		//No 'InventoryItems' object found
		if (rr.ReadReader("inventoryItems", respReader))
		{
			rcheck(m_pResponseData->m_inventory.Deserialize(respReader), catchall, rlTaskError("Failed to deserialize 'InventoryItems'"));
		}

		return true;
	}
	rcatchall
	{
		gnetError("Failed to DeserializePlayerInfo");
		CheckFailure(rlTaskBase::FINISH_FAILED);
	}

	return false;
}

bool HeartBeatGameTransactionHttpTask::ProcessSuccess(const RsonReader &rr)
{
	rtry
	{
		rcheck(DeserializePlayerInfo(rr), catchall, );

		//If this is NULL we want to crash it.
		*m_failedAttempts = 0;

		RsonReader respReader;
		if(rr.ReadReader("timeIntervalSec", respReader) && gnetVerify(m_timeIntervalMs))
		{
			u32 value = 0;
			if (gnetVerify(respReader.AsUns(value)))
			{
				*m_timeIntervalMs = (value*1000);
			}
		}
		
		return true;
	}
	rcatchall
	{
		gnetError("HeartBeatGameTransactionHttpTask::ProcessSuccess - FAILED.");
		CheckFailure(rlTaskBase::FINISH_FAILED);
	}

	return false;
}

void HeartBeatGameTransactionHttpTask::Finish(const FinishType finishType, const int resultCode /*= 0*/)
{
	BaseSessionGameTransactionHttpTask::Finish(finishType, resultCode);
	CheckFailure(finishType);
	GameTransactionSessionMgr::Get().HandleHeartBeatFinish();
}

void  HeartBeatGameTransactionHttpTask::CheckFailure(const FinishType finishType)
{
	//If this is NULL we want to crash it.
	if (rlTaskBase::FINISH_SUCCEEDED != finishType)
	{
		*m_failedAttempts += 1;
		if (*m_failedAttempts > MAX_ATTEMPTS)
		{
			bool bail = true;
			const bool tuneExists = Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("BAIL_GAME_SERVER_HEART_BAIL", 0xe472729a), bail);
			if(bail || !tuneExists)
			{
				gnetDebug1("HeartBeatGameTransactionHttpTask::CheckFailure - FAILURE TO COMMUNICATE WITH SERVER.");
				GameTransactionSessionMgr::Get().Bail(GameTransactionSessionMgr::FAILED_ACCESS_TOKEN, BAIL_GAME_SERVER_HEART_BAIL, BAIL_CTX_NONE);
			}

			*m_failedAttempts = 0;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// SessionStartGameTransactionHttpTask
//////////////////////////////////////////////////////////////////////////

// Use the Streaming memory, if we don't have enough we can try using the network heap.
SessionStartGameTransactionHttpTask::SessionStartGameTransactionHttpTask() 
	: BaseSessionGameTransactionHttpTask(NULL
							, Tunables::GetInstance().CheckExists(CD_GLOBAL_HASH, ATSTRINGHASH("START_SESSION_PROBLEM", 0x52a63a2e)) ? NULL : GameTransactionSessionMgr::Get().GetBounceBuffer()
							, Tunables::GetInstance().CheckExists(CD_GLOBAL_HASH, ATSTRINGHASH("START_SESSION_PROBLEM", 0x52a63a2e)) ? 0 : GameTransactionSessionMgr::Get().GetBounceBufferSize())
{
	if (!Tunables::GetInstance().CheckExists(CD_GLOBAL_HASH, ATSTRINGHASH("START_SESSION_PROBLEM", 0x52a63a2e)))
	{
		m_GrowBuffer.Init(GameTransactionSessionMgr::Get().GetGrowBuffer(), GameTransactionSessionMgr::Get().GetGrowBufferSize(), datGrowBuffer::NULL_TERMINATE);
		gnetDebug1("Stearming memory available '%d'.", strStreamingEngine::GetAllocator().GetVirtualMemoryFree());
	}
}

void SessionStartGameTransactionHttpTask::OnFinish(const int resultCode)
{
	BaseSessionGameTransactionHttpTask::OnFinish(resultCode);
	GameTransactionSessionMgr::Get().HandleSessionStartFinish();
}

bool SessionStartGameTransactionHttpTask::ProcessSuccess(const RsonReader &rr)
{
	rtry
	{
		//Clear all inventory data from the stats.
		CStatsMgr::GetStatsDataMgr().ClearPlayerInventory();

		//Now we can apply all inventory data and the player cash.
		rcheck(DeserializePlayerInfo(rr)
			,catchall
			,gnetDebug1("Failed to DeserializePlayerInfo()."));

		//Now we can apply all session info.
		rcheck(DeserializeSessionInfo(rr)
			,catchall
			,gnetDebug1("Failed to DeserializeSessionInfo()."));

		return true;
	}
	rcatchall
	{
		gnetError("SessionStartGameTransactionHttpTask::ProcessSuccess - FAILED.");
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
// SessionRestartGameTransactionHttpTask
//////////////////////////////////////////////////////////////////////////


void SessionRestartGameTransactionHttpTask::OnFinish(const int resultCode)
{
	BaseSessionGameTransactionHttpTask::OnFinish(resultCode);
	GameTransactionSessionMgr::Get().HandleSessionRefreshFinish();
}

bool SessionRestartGameTransactionHttpTask::ProcessSuccess(const RsonReader &rr)
{
	rtry
	{
		rcheck(DeserializeSessionInfo(rr), catchall, );
		return true;
	}
	rcatchall
	{

	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
// GetBalanceGameTransactionHttpTask
//////////////////////////////////////////////////////////////////////////

void GetBalanceGameTransactionHttpTask::OnFinish(const int resultCode)
{
	BaseSessionGameTransactionHttpTask::OnFinish(resultCode);
	GameTransactionSessionMgr::Get().HandleGetBalanceFinish();
}

bool GetBalanceGameTransactionHttpTask::ProcessSuccess(const RsonReader &rr)
{
	rtry
	{
		RsonReader respReader;

		if (rr.ReadReader("playerBalance", respReader))
		{
			rcheck(m_pResponseData->m_playerBalance.Deserialize(respReader), catchall, rlTaskError("Failed to deserialize 'playerBalance'"));
		}
		else
		{
			gnetDebug1("GetBalanceGameTransactionHttpTask:: No 'playerBalance' object found");
		}

		if (rr.ReadReader("inventoryItems", respReader))
		{
			rcheck(m_pResponseData->m_inventory.Deserialize(respReader), catchall, rlTaskError("Failed to deserialize 'InventoryItems'"));
		}
		else
		{
			gnetDebug1("GetBalanceGameTransactionHttpTask:: No 'InventoryItems' object found");
		}

		return true;
	}
	rcatchall
	{

	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
// SessionStartTransaction
//////////////////////////////////////////////////////////////////////////

class SessionStartTransaction : public NetworkGameTransactions::GameTransactionBase
{
public:
	SessionStartTransaction() 
		: GameTransactionBase()
		, m_slot(-1)
		, m_inventory(false)
		, m_playerbalance(false)
	{
	}

	SessionStartTransaction(const int slot) 
		: GameTransactionBase()
		, m_slot(slot)
		, m_inventory(false)
		, m_playerbalance(false)
	{
	}

	SessionStartTransaction(const int slot, const bool inventory, const bool playerbalance) 
		: GameTransactionBase()
		, m_slot(slot)
		, m_inventory(inventory)
		, m_playerbalance(playerbalance)
	{
	}

	virtual bool Serialize(RsonWriter& wr) const
	{
		return GameTransactionBase::Serialize(wr)
			&& gnetVerify(m_slot >= 0 && m_slot < MAX_NUM_MP_CHARS)
			&& wr.WriteInt("slot", m_slot)
			&& m_inventory ? wr.WriteBool("inventory", m_inventory) : true
			&& m_playerbalance ? wr.WriteBool("playerbalance", m_playerbalance) : true;
	}

protected:
	//Used to specify if we include the 'catalogVersion'.  For the SessionStart transaction, we don't want to include the catalog version with the transaction data.
	virtual bool	IncludeCatalogVersion() const { return false; };

private:
	int  m_slot;
	bool m_inventory;
	bool m_playerbalance;
};


//////////////////////////////////////////////////////////////////////////
// HeartBeatHelper
//////////////////////////////////////////////////////////////////////////

HeartBeatHelper::~HeartBeatHelper()
{
	if (m_status.Pending())
			Cancel();
	m_status.Reset();
}

void HeartBeatHelper::Init( const u32 intervalSeconds )
{
	m_timeExpiredIntervalMs = intervalSeconds > 0 ? (intervalSeconds*1000) : DEFAULT_MAX_INTERVAL;
	Reset(sysTimer::GetSystemMsTime());
}

void HeartBeatHelper::Reset(const unsigned curTimeMs)
{
	m_nextExpireTimeMs = curTimeMs + m_timeExpiredIntervalMs;
}

void HeartBeatHelper::Clear()
{
	if(m_status.Pending())
		Cancel( );
	m_status.Reset();

	m_data.Clear();
	m_nextExpireTimeMs = 0;
	m_failedAttempts = 0;
}

void HeartBeatHelper::Cancel()
{
	if(gnetVerify(m_status.Pending()))
	{
		NetworkGameTransactions::Cancel(&m_status);
	}
}

bool HeartBeatHelper::CanSend(const unsigned curTime) const
{
	return m_nextExpireTimeMs > 0 && curTime > m_nextExpireTimeMs;
}

void HeartBeatHelper::AddItem(const int itemId, const int value, const int price)
{
	if (!Exists(itemId))
	{
		m_data.PushItem(itemId, value == 0 ? 1 : value, price, false);
	}
}

bool HeartBeatHelper::Exists(const int itemId) const
{
	return m_data.Exists(itemId);
}


//////////////////////////////////////////////////////////////////////////
// GameTransactionSessionMgr
//////////////////////////////////////////////////////////////////////////

rage::sysIpcMutex GameTransactionSessionMgr::sm_Mutex = NULL;

void GameTransactionSessionMgr::Init()
{
	//Register to handle presence events.
	m_presDelegate.Bind(this, &GameTransactionSessionMgr::OnPresenceEvent);
	rlPresence::AddDelegate(&m_presDelegate);

	m_bounceBuffer = (u8*) strStreamingEngine::GetAllocator().Allocate(GetBounceBufferSize(), 16, MEMTYPE_RESOURCE_VIRTUAL);
	gnetFatalAssertf(m_bounceBuffer, "Failed to create the bounce buffer %d", GetBounceBufferSize());

	m_growBuffer = (u8*) strStreamingEngine::GetAllocator().Allocate(GetGrowBufferSize(), 16, MEMTYPE_RESOURCE_VIRTUAL);
	gnetFatalAssertf(m_growBuffer, "Failed to create the grow buffer %d", GetGrowBufferSize());

	sm_Mutex = rage::sysIpcCreateMutex();
}

void GameTransactionSessionMgr::Shutdown()
{
	sysIpcDeleteMutex(sm_Mutex);

	netCatalog::GetInstance().Shutdown( );
	rlPresence::RemoveDelegate(&m_presDelegate);

	if (gnetVerify(m_bounceBuffer))
	{
		strStreamingEngine::GetAllocator().Free(m_bounceBuffer);
		m_bounceBuffer = NULL;
	}

	if (gnetVerify(m_growBuffer))
	{
		strStreamingEngine::GetAllocator().Free(m_growBuffer);
		m_growBuffer = NULL;
	}
}

bool GameTransactionSessionMgr::InitManager(const int localGamerIndex)
{
	bool retVal = false;

	rtry
	{
		rverify(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, gnetError("INVALID GAMER INDEX, localGamerIndex=%d", localGamerIndex));

		if(HasFailed() ||
			(m_state != INVALID && m_localGamerIndex != localGamerIndex))
		{
			ShutdownManager();
		}

		if(m_state == INVALID)
		{
			if(m_deleteStatus.Pending())
				NetworkGameTransactions::Cancel(&m_deleteStatus);
			m_deleteStatus.Reset();

			if(m_cashtransferStatus.Pending())
				NetworkGameTransactions::Cancel(&m_cashtransferStatus);
			m_cashtransferStatus.Reset();

			m_localGamerIndex = localGamerIndex;

			SetState(PENDING_ACCESS_TOKEN);
			m_accessToken.Init(m_localGamerIndex);
		}

		gnetDebug1("GameTransactionSessionMgr::InitManager()");

		retVal = true;
	}
	rcatchall
	{
	}

	return retVal;
}

void GameTransactionSessionMgr::ShutdownManager()
{
	gnetDebug1("GameTransactionSessionMgr::ShutdownManager()");

	m_localGamerIndex = -1;
	m_slot = -1;
	m_sessionTransactionCount = 0;
	m_sessionStartData.Clear();
	m_applyDataOnSuccess = false;

	m_heartBeatMgr.Clear();

	if(m_status.Pending())
		NetworkGameTransactions::Cancel(&m_status);
	m_status.Reset();

	if(m_deleteStatus.Pending())
		NetworkGameTransactions::Cancel(&m_deleteStatus);
	m_deleteStatus.Reset();

	if(m_cashtransferStatus.Pending())
		NetworkGameTransactions::Cancel(&m_cashtransferStatus);
	m_cashtransferStatus.Reset();

	NETWORK_SHOPPING_MGR.Shutdown( SHUTDOWN_SESSION );

	SetState(INVALID);

	rlRos::ClearServiceAccessRequest();

	m_usingGameTransactionToken = false;
	m_accessToken.Shutdown();

    //Cancel any pending commerce consumes, and therefore purch calls.
    CLiveManager::GetCommerceMgr().CancelPendingConsumptions();
}

bool GameTransactionSessionMgr::SessionStart(const int localGamerIndex, const int slot, bool applyDataOnSuccess /*= false*/)
{
	bool retVal = false;

	//Session Start is in progress already and we are not changing the slot.
	if (m_state == PENDING_SESSION_START && m_slot == slot)
	{
		gnetWarning("GameTransactionSessionMgr::SessionStart - VOID CALL - Start is in progress already and we are not changing the slot. current slot='%d'", m_slot);
		return true;
	}

	if(gnetVerifyf(0 <= slot && slot < MAX_NUM_MP_CHARS, "GameTransactionSessionMgr::SessionStart - Invalid slot=%d", slot)
		&& gnetVerifyf(InitManager(localGamerIndex), "GameTransactionSessionMgr::SessionStart - Failed InitManager()"))
	{
		// Changing slots requires calling SessionStart again, but the access token and catalog should still be good.
		if(m_slot != slot && (m_state == PENDING_SESSION_START || m_state == READY))
		{
			if(m_status.Pending())
			{
				NetworkGameTransactions::Cancel(&m_status);
			}
			m_status.Reset();

			SetState( WAIT_FOR_START_SESSION );
		}

		m_applyDataOnSuccess = applyDataOnSuccess || PARAM_inventoryForceApplyData.Get();

		m_sessionStartData.m_inventory.m_slot = slot;
		m_slot = slot;

		retVal = true;
	}

	return retVal;
}

bool GameTransactionSessionMgr::GetBalance(const bool inventory, const bool playerbalance)
{
	if (gnetVerify(IsReady()))
	{
		return TriggerGetBalance(inventory, playerbalance);
	}

	return false;
}

void GameTransactionSessionMgr::Update(const unsigned curTime)
{
	m_isAnySessionActive = NetworkInterface::IsAnySessionActive();

	m_accessToken.Update();

	//Don't process the state if we cant 
	if ( !rage::sysIpcTryLockMutex( sm_Mutex ) )
		return;

	if(m_delayedBailReason != BAIL_INVALID)
	{
		Bail(m_state, m_delayedBailReason, m_delayedcontext, false);
		m_delayedBailReason = BAIL_INVALID;
		m_delayedcontext = 0;
	}
	gnetVerify( rage::sysIpcUnlockMutex(sm_Mutex) );

	switch(m_state)
	{
	//case INVALID:
	//case PENDING_SESSION_START:
	//case PENDING_SESSION_RESTART:
	//case FAILED_ACCESS_TOKEN:
	//case FAILED_CATALOG:
	//case FAILED_SESSION_START:
	//case FAILED_SESSION_RESTART:
	//case FAILED_GAME_VERSION:
	case PENDING_ACCESS_TOKEN:
		{
			if(!m_accessToken.IsPending())
			{
				if(m_accessToken.IsSuccess())
				{
					m_triggerCrcCheck = true;

					if(netCatalog::GetInstance().HasReadSucceeded())
					{
						SetState( WAIT_FOR_START_SESSION );
					}
					else
					{
						TriggerCatalogRefresh();
					}
				}
				else
				{
					gnetDebug1("GameTransactionSessionMgr::Update - Failed to retrieve the access token.");
					Bail(FAILED_ACCESS_TOKEN, BAIL_TOKEN_REFRESH_FAILED, BAIL_CTX_NONE);
				}
			}
		}
		break;

	case WAIT_FOR_START_SESSION:
		{
			if(m_slot != -1)
			{
				if (netCatalog::GetInstance().GetLoadWorkerInProgress())
				{
					SetState(PENDING_CATALOG);
				}
				else if(TriggerCatalogVersionRefresh())
				{
					SetState(PENDING_CATALOG_VERSION);
				}
				else
				{
					gnetDebug1("GameTransactionSessionMgr::Update - TriggerCatalogVersionRefresh failed.");
					Bail(FAILED_CATALOG, BAIL_CATALOG_REFRESH_FAILED, BAIL_CTX_CATALOG_FAIL_TRIGGER_VERSION_CHECK);
				}
			}
			else
			{
				TriggerCatalogRefreshIfRequested();
			}
		}
		break;

	case PENDING_CATALOG_VERSION:
		{
			if(!m_status.Pending())
			{
				if(m_status.Succeeded())
				{
					if(m_refreshCatalogRequested)
					{
						if (TriggerCatalogRefresh())
						{
							//catalog download started... or error... 
						}
						else
						{
							gnetDebug1("GameTransactionSessionMgr::Update - TriggerCatalogVersionRefresh failed.");
							Bail(FAILED_CATALOG, BAIL_CATALOG_REFRESH_FAILED, BAIL_CTX_CATALOG_FAIL_TRIGGER_CATALOG_REFRESH);
						}
					}
					else if(netCatalog::GetInstance().GetStatus().Pending() || netCatalog::GetInstance().GetLoadWorkerInProgress())
					{
						SetState(PENDING_CATALOG);
					}
					else
					{
						//We have the latest catalog.
						SetState(CATALOG_COMPLETE);
					}

				}
				else
				{
					gnetDebug1("GameTransactionSessionMgr::Update - TriggerCatalogVersionRefresh failed.");

					Bail(FAILED_CATALOG, BAIL_CATALOGVERSION_REFRESH_FAILED, m_status.GetResultCode());
				}

				m_status.Reset();
			}
		}
		break;

	case PENDING_CATALOG:
		{
			if(!netCatalog::GetInstance().GetStatus().Pending() && !netCatalog::GetInstance().GetLoadWorkerInProgress())
			{
				const int resultCode = netCatalog::GetInstance().GetStatus().GetResultCode();

				netCatalog::GetInstance().CatalogDownloadDone();

				if(netCatalog::GetInstance().GetStatus().Succeeded())
				{
					m_triggerCrcCheck = true;
					SetState( CATALOG_COMPLETE );
				}
				else
				{
					gnetDebug1("GameTransactionSessionMgr::Update - GetCatalog failed.");

					Bail(FAILED_CATALOG, BAIL_CATALOG_REFRESH_FAILED, resultCode);
				}

				m_status.Reset();
			}
		}
		break;

	case CATALOG_COMPLETE:
		{
			if(m_slot != -1)
			{
				if(m_sessionStartData.m_gsToken.IsValid())
				{
					SetState(READY);
				}
				else
				{
					TriggerSessionStart();
				}
			}
			else
			{
				TriggerCatalogRefreshIfRequested();
			}
		}
		break;

	case READY:
		{
			if (m_accessToken.IsPending())
			{
				gnetDebug1("Access Token found invalid.  Waiting for new token");

				SetState(PENDING_ACCESS_TOKEN);
			}
			else
			{
#if RSG_PC
				if (NetworkInterface::IsGameInProgress()
					&& m_triggerCrcCheck 
					&& netCatalog::GetInstance().IsLatestVersion())
				{
					m_triggerCrcCheck = false;
					CNetworkCheckCatalogCrc::Trigger(false);
				}
#endif //RSG_PC

				TriggerCatalogRefreshIfRequested();
				TriggerSessionRefreshIfRequested();
				TriggerHeartBeat(curTime);
			}
			
			break;
		}
		
	default:
		break;
	}

	//clear m_telemetryNonceSeed
	m_telemetryNonceSeed = 0;
}

void GameTransactionSessionMgr::FlagForSessionRestart()
{
	if (gnetVerify(HasInitSucceeded()))
	{
		gnetDebug1("FlagForSessionRestart()");
		m_refreshSessionRequested = true; 
	}
}

const char* GameTransactionSessionMgr::GetStateString(const eState state) const
{
	const char* result = "UNKNOWN";

	switch (state)
	{
	case INVALID:                 result = "INVALID"; break;
	case PENDING_ACCESS_TOKEN:    result = "PENDING_ACCESS_TOKEN"; break;
	case WAIT_FOR_START_SESSION:  result = "WAIT_FOR_START_SESSION"; break;
	case PENDING_CATALOG_VERSION: result = "PENDING_CATALOG_VERSION"; break;
	case PENDING_CATALOG:         result = "PENDING_CATALOG"; break;
	case CATALOG_COMPLETE:        result = "CATALOG_COMPLETE"; break;
	case PENDING_SESSION_START:   result = "PENDING_SESSION_START"; break;
	case PENDING_SESSION_RESTART: result = "PENDING_SESSION_RESTART"; break;
	case READY:                   result = "READY"; break;
	case FAILED_ACCESS_TOKEN:     result = "FAILED_ACCESS_TOKEN"; break;
	case FAILED_CATALOG:          result = "FAILED_CATALOG"; break;
	case FAILED_SESSION_START:    result = "FAILED_SESSION_START"; break;
	case FAILED_SESSION_RESTART:  result = "FAILED_SESSION_RESTART"; break;
	case FAILED_GAME_VERSION:     result = "FAILED_GAME_VERSION"; break;

	default:
		break;
	}

	return result;
}

bool GameTransactionSessionMgr::GetIsSlotValid(const int slot)
{
	return (HasInitSucceeded() 
				&& m_sessionStartData.m_slotCreateTimestamp.IsValid(slot));
}


void GameTransactionSessionMgr::SetState(const eState newState)
{
#if !__NO_OUTPUT
	static unsigned s_StartTime = 0;
	const unsigned curTime = sysTimer::GetSystemMsTime();

	//Restart clock.
	if ((m_state == INVALID || m_state == READY) && newState != INVALID)
	{
		s_StartTime = curTime;
	}

	gnetWarning("GameTransactionSessionMgr::SetState - Changing state from '%s' to '%s', elapsed time = %u ms", GetStateString(m_state), GetStateString(newState), s_StartTime > 0 ? curTime - s_StartTime : 0);

	//Reset clock.
	if (newState == READY)
	{
		s_StartTime = 0;
	}
#endif //__NO_OUTPUT

	m_state = newState;

	//We nned to cleanup.
	if (newState == WAIT_FOR_START_SESSION)
	{
		NETWORK_SHOPPING_MGR.DeleteBasket( );
	}

	//Make sure we clear any pending refreshes.
	if (HasFailed())
	{
		m_refreshCatalogRequested = false;
		m_refreshSessionRequested = false;
	}
	//Flag that our 1st transition to multiplayer is done.
	else if (IsReady())
	{
		m_isAnySessionActive = true;
	}
}

void GameTransactionSessionMgr::Bail(const eState newState, const eBailReason nBailReason, const int context, const bool delayed)
{
	gnetAssertf(newState > READY, "Invalid state='%s' for a Bail event.", GetStateString(newState));

	SetState(newState);

	if (delayed)
	{
		gnetVerify( rage::sysIpcLockMutex(sm_Mutex) );

		m_delayedBailReason = nBailReason;
		m_delayedcontext = context;
		gnetWarning("GameTransactionSessionMgr::Bail - delayed - newState='%s', nBailReason='%s'.", GetStateString(newState), NetworkUtils::GetBailReasonAsString(nBailReason));

		gnetVerify( rage::sysIpcUnlockMutex(sm_Mutex) );
		return;
	}

	gnetAssertf(sysThreadType::IsUpdateThread(), "GameTransactionSessionMgr::Bail() used from another thread than the main thread, this may not be safe.");
	if (sysThreadType::IsUpdateThread())
	{
		gnetWarning("GameTransactionSessionMgr::Bail - newState='%s', nBailReason='%s'.", GetStateString(newState), NetworkUtils::GetBailReasonAsString(nBailReason));
		NetworkInterface::Bail(nBailReason, context);
	}

	//Clear any pending operations
	m_refreshCatalogRequested = false;
	m_refreshSessionRequested = false;

	//Clear the session if we are in a valid one
	if (m_slot >= 0)
		ClearStartSession( m_slot );

	switch (newState)
	{
		//Clear session start Data.
	case FAILED_ACCESS_TOKEN:
	case FAILED_SESSION_START:
	case FAILED_SESSION_RESTART:
	case FAILED_GAME_VERSION:
		m_sessionStartData.Clear();
		break;

		//Clear current catalog status
	case FAILED_CATALOG:
		netCatalog::GetInstance().ClearDownloadStatus( );
		break;

	default:
		break;
	}
}

void GameTransactionSessionMgr::HandleSessionRefreshFinish()
{
	if (m_status.Succeeded())
	{
		gnetDebug1("SessionRefresh Succeeded");
		gnetDebug1("    gsToken:%s", m_sessionStartData.m_gsToken.m_token);
		gnetDebug1("    nounce: %" I64FMT "d", m_sessionStartData.m_nonceSeed);

		m_heartBeatMgr.Reset(sysTimer::GetSystemMsTime());

		SetState(READY);

		//Reset our transaction count
		m_sessionTransactionCount = 0;
	}
	else //Failed or cancelled.
	{
		gnetDebug1("SessionRefresh Failed - code: %d", m_status.GetResultCode());
		Bail(FAILED_SESSION_RESTART, BAIL_SESSION_REFRESH_FAILED, BAIL_CTX_NONE);
	}
}

void GameTransactionSessionMgr::HandleSessionStartFinish()
{
	if (m_status.Succeeded())
	{
		gnetDebug1("SessionStart Succeeded");
		gnetDebug1("    gsToken:%s", m_sessionStartData.m_gsToken.m_token);
		gnetDebug1("    nounce: %" I64FMT "d", m_sessionStartData.m_nonceSeed);

		gnetAssert(m_slot > -1 && m_slot == m_sessionStartData.m_inventory.m_slot);

		m_heartBeatMgr.Init(m_sessionStartData.m_heartBeatSec);

		SetState(READY);

		//Reset our transaction count
		m_sessionTransactionCount = 0;

		if (m_applyDataOnSuccess)
		{
			ApplyData();
		}
	}
	else //Failed or cancelled.
	{
		gnetDebug1("SessionStart Failed - code: %d", m_status.GetResultCode());
		gnetAssert(!m_status.Pending());

		SetState(FAILED_SESSION_START);
		m_sessionStartData.Clear();
	}
}

void GameTransactionSessionMgr::HandleGetBalanceFinish()
{
	if (m_status.Succeeded())
	{
		gnetDebug1("GetBalance Succeeded");
		gnetDebug1("    gsToken:%s", m_sessionStartData.m_gsToken.m_token);
		gnetDebug1("    nounce: %" I64FMT "d", m_sessionStartData.m_nonceSeed);

		gnetAssert(m_slot > -1 && m_slot == m_sessionStartData.m_inventory.m_slot);

		m_heartBeatMgr.Reset(sysTimer::GetSystemMsTime());

		SetState(READY);

		if (m_applyDataOnSuccess)
		{
			ApplyData();
		}
	}
	else //Failed or cancelled.
	{
		gnetDebug1("GetBalance FAILED - code: %d", m_status.GetResultCode());
		gnetAssert(!m_status.Pending());

		Bail(FAILED_SESSION_RESTART, BAIL_SESSION_RESTART_FAILED, BAIL_CTX_NONE);
	}
}

void GameTransactionSessionMgr::HandleHeartBeatFinish()
{
	if (m_heartBeatMgr.m_status.Succeeded())
	{
		rtry																				
		{
			//////////////////////////////////////////////////////////////////////////
			//PLAYER BALANCE
			//////////////////////////////////////////////////////////////////////////

			NetworkGameTransactions::PlayerBalanceApplicationInfo pbInfo;
			rverify(m_heartBeatMgr.m_responseData.m_playerBalance.ApplyDataToStats(pbInfo)
						,catchall
						,gnetError("HandleHeartBeatFinish() - Failed to Apply balance."));

			m_heartBeatMgr.Reset(sysTimer::GetSystemMsTime());

#if RSG_PC
			if (NetworkInterface::IsGameInProgress())
			{
				if (pbInfo.m_bankCashDifference > 0)
				{
					gnetError("Cash differences during heartBeat check. bank='%" I64FMT "u'.", pbInfo.m_bankCashDifference);
					CReportMyselfEvent::Trigger(NetworkRemoteCheaterDetector::GAME_SERVER_CASH_BANK, (u32)pbInfo.m_bankCashDifference);
				}

				if (pbInfo.m_walletCashDifference > 0)
				{
					gnetError("Cash differences during heartBeat check. wallet='%" I64FMT "u'.", pbInfo.m_walletCashDifference);
					CReportMyselfEvent::Trigger(NetworkRemoteCheaterDetector::GAME_SERVER_CASH_WALLET, (u32)pbInfo.m_walletCashDifference);
				}
			}
#endif // RSG_PC

			//////////////////////////////////////////////////////////////////////////
			//INVENTORY
			//////////////////////////////////////////////////////////////////////////

			//////////////////////////////////////////////////////////////////////////
			//SERVER INTEGRITY
			//////////////////////////////////////////////////////////////////////////
		}
		rcatchall
		{
			//////////////////////////////////////////////////////////////////////////
		}
	}

	m_heartBeatMgr.m_status.Reset();
}

void GameTransactionSessionMgr::TriggerSessionStart()
{
	SessionStartGameTransactionHttpTask* pSessionStartTask = NULL;

	rtry																				
	{
		rverify(RL_IS_VALID_LOCAL_GAMER_INDEX(m_localGamerIndex), catchall, gnetError("INVALID GAMER INDEX"));

		char dataBuffer[GAME_TRANSACTION_SIZE_OF_RSONWRITER];
		RsonWriter wr(dataBuffer, RSON_FORMAT_JSON);

		wr.Begin(NULL, NULL);

#if !__FINAL
		NetworkGameTransactions::ApplyUserInfoHack(m_localGamerIndex, wr);
#endif

		m_sessionStartNonceResponse.Clear();

		rcheck(rlGetTaskManager()->CreateTask(&pSessionStartTask)
			,catchall
			,gnetError("GameTransactionSessionMgr::TriggerSessionStart() - NetworkGameTransactions::SessionStart: Failed to create"));

#if __BANK
		rcheck(pSessionStartTask->BankSerializeForceError(wr)
			,catchall
			,gnetError("Failed to serialize 'forceerror'"));
#endif // __BANK

		SessionStartTransaction sessionStartTransaction(m_slot);

		rcheck(sessionStartTransaction.Serialize(wr)
			,catchall
			,gnetError("GameTransactionSessionMgr::TriggerSessionStart() - Failed to serialize Transaction"));

		rcheck(rlTaskBase::Configure(pSessionStartTask
										,m_localGamerIndex
										,netCatalog::GetInstance().GetRandomItem()
										,wr
										,&m_sessionStartData
										,&m_sessionStartNonceResponse
										,&m_status)
			,catchall
			,gnetError("GameTransactionSessionMgr::TriggerSessionStart() - SessionStartGameTransactionHttpTask: Failed to Configure"));

		rcheck(rlGetTaskManager()->AddParallelTask(pSessionStartTask)
			,catchall
			,gnetError("GameTransactionSessionMgr::TriggerSessionStart() - Failed to Configure AddParallelTask."));

		SetState(PENDING_SESSION_START);
	}
	rcatchall
	{
		if(pSessionStartTask)							
		{																		
			if(pSessionStartTask->IsPending())													
			{																		
				pSessionStartTask->Finish(rlTaskBase::FINISH_FAILED, RLSC_ERROR_UNEXPECTED_RESULT);						                    
			}																		
			else																	
			{																		
				m_status.ForceFailed();													
			}

			rlGetTaskManager()->DestroyTask(pSessionStartTask);								    
		}

		gnetDebug1("GameTransactionSessionMgr::Update - TriggerSessionStart failed.");
		SetState(FAILED_SESSION_START);
	}
}

void GameTransactionSessionMgr::SendHeartBeat(const unsigned curTime)
{
	HeartBeatGameTransactionHttpTask* pHeartBeatTask = NULL;

	bool nonceSet = false;

	rtry
	{
		rverify(m_heartBeatMgr.CanSend(curTime), catchall, gnetError("Cannot send heartBeat."));

		rverify(RL_IS_VALID_LOCAL_GAMER_INDEX(m_localGamerIndex), catchall, gnetError("INVALID GAMER INDEX"));

		char dataBuffer[GAME_TRANSACTION_SIZE_OF_RSONWRITER];
		RsonWriter wr(dataBuffer, RSON_FORMAT_JSON);

		wr.Begin(NULL, NULL);

#if !__FINAL
		NetworkGameTransactions::ApplyUserInfoHack(m_localGamerIndex, wr);
#endif // !__FINAL

		m_heartBeatMgr.m_data.SetSlot(m_slot);

		rcheck(m_heartBeatMgr.m_data.SlotIsValid()
			,catchall
			,gnetError("heartBeat does not have a valid slot."));

		rcheck(!m_heartBeatMgr.m_status.Pending()
			,catchall
			,gnetError("heartBeat Transaction is in progress."));

		//Setup nonce for the heartbeat
		m_heartBeatMgr.m_data.SetNonce();
		nonceSet = true;

		rcheck(m_heartBeatMgr.m_data.Serialize(wr)
			,catchall
			,gnetError("Failed to serialize Transaction"));

		rcheck(rlGetTaskManager()->CreateTask(&pHeartBeatTask)
			,catchall
			,gnetError("HeartbeatGameTransactionHttpTask: Failed to create"));

		rcheck(rlTaskBase::Configure(pHeartBeatTask
										,m_localGamerIndex
										,netCatalog::GetInstance().GetRandomItem()
										,wr
										,&m_heartBeatMgr.m_responseData
										,&m_heartBeatMgr.m_responseNonce
										,&m_heartBeatMgr.m_timeExpiredIntervalMs
										,&m_heartBeatMgr.m_failedAttempts
										,&m_heartBeatMgr.m_status)
			,catchall
			,gnetError("HeartbeatGameTransactionHttpTask: Failed to Configure"));

		rcheck(rlGetTaskManager()->AppendSerialTask(GAME_TRANSACTION_TASK_QUEUE_ID, pHeartBeatTask)
			,catchall
			,gnetError("HeartbeatGameTransactionHttpTask: Failed to AppendSerialTask."));

		//All done and set
		m_heartBeatMgr.Reset(curTime);

		pHeartBeatTask->SetHasNonce();
	}
	rcatchall
	{
		//Nonce must be rolled back,
		if (nonceSet)
		{
			GameTransactionSessionMgr::Get().RollbackNounce();
		}

		if(pHeartBeatTask)							
		{																		
			if(pHeartBeatTask->IsPending())													
			{																		
				pHeartBeatTask->Finish(rlTaskBase::FINISH_FAILED, RLSC_ERROR_UNEXPECTED_RESULT);						                    
			}																		
			else																	
			{																		
				m_heartBeatMgr.m_status.ForceFailed();													
			}

			rlGetTaskManager()->DestroyTask( pHeartBeatTask );								    
		}
	}

}

void GameTransactionSessionMgr::TriggerSessionRefresh( )
{
	m_refreshSessionRequested = false;

	SessionRestartGameTransactionHttpTask* pSessionRestartTask = NULL;

	bool nonceSet = false;

	rtry
	{
		rverify(RL_IS_VALID_LOCAL_GAMER_INDEX(m_localGamerIndex), catchall, gnetError("INVALID GAMER INDEX"));

		char dataBuffer[GAME_TRANSACTION_SIZE_OF_RSONWRITER];
		RsonWriter wr(dataBuffer, RSON_FORMAT_JSON);

		wr.Begin(NULL, NULL);

#if !__FINAL
		NetworkGameTransactions::ApplyUserInfoHack(m_localGamerIndex, wr);
#endif

		m_sessionStartNonceResponse.Clear();

		SessionStartTransaction sessionStartTransaction(m_slot);

		rcheck(sessionStartTransaction.SetNonce()
			,catchall
			,gnetError("Failed to setup nonce for trasaction."));
		nonceSet = true;

		rcheck(rlGetTaskManager()->CreateTask(&pSessionRestartTask)
			,catchall
			,gnetError("SessionRestartGameTransactionHttpTask: Failed to create"));

#if __BANK
		rcheck(pSessionRestartTask->BankSerializeForceError(wr), catchall, 
			gnetError("Failed to serialize 'forceerror'"));
#endif // __BANK

		rcheck(sessionStartTransaction.Serialize(wr)
			,catchall
			,gnetError("Failed to serialize Transaction"));

		rcheck(rlTaskBase::Configure(pSessionRestartTask
										,m_localGamerIndex
										,netCatalog::GetInstance().GetRandomItem()
										,wr
										,&m_sessionStartData
										,&m_sessionStartNonceResponse
										,&m_status)
			,catchall
			,gnetError("SessionStartGameTransactionHttpTask: Failed to Configure"));

		rcheck(rlGetTaskManager()->AddParallelTask(pSessionRestartTask)
			,catchall
			,gnetError("SessionStartGameTransactionHttpTask: Failed to AddParallelTask."));

		SetState(PENDING_SESSION_RESTART);

		pSessionRestartTask->SetHasNonce();
	}
	rcatchall
	{
		//Nonce must be rolled back,
		if (nonceSet)
		{
			GameTransactionSessionMgr::Get().RollbackNounce();
		}

		if(pSessionRestartTask)							
		{																		
			if(pSessionRestartTask->IsPending())													
			{																		
				pSessionRestartTask->Finish(rlTaskBase::FINISH_FAILED, RLSC_ERROR_UNEXPECTED_RESULT);						                    
			}																		
			else																	
			{																		
				m_status.ForceFailed();													
			}

			rlGetTaskManager()->DestroyTask(pSessionRestartTask);								    
		}

		gnetDebug1("GameTransactionSessionMgr::Update - TriggerSessionRefresh failed.");
		SetState(FAILED_SESSION_RESTART);
	}
}

bool GameTransactionSessionMgr::TriggerGetBalance(const bool inventory, const bool playerbalance)
{
	m_refreshSessionRequested = false;

	GetBalanceGameTransactionHttpTask* pSessionRestartTask = NULL;

	bool nonceSet = false;

	rtry																				
	{
		rverify(RL_IS_VALID_LOCAL_GAMER_INDEX(m_localGamerIndex), catchall, gnetError("INVALID GAMER INDEX"));

		char dataBuffer[GAME_TRANSACTION_SIZE_OF_RSONWRITER];
		RsonWriter wr(dataBuffer, RSON_FORMAT_JSON);

		wr.Begin(NULL, NULL);

#if !__FINAL
		NetworkGameTransactions::ApplyUserInfoHack(m_localGamerIndex, wr);
#endif //!__FINAL

		m_status.Reset();
		m_sessionStartNonceResponse.Clear();

		SessionStartTransaction sessionStartTransaction(m_slot, inventory, playerbalance);

		rcheck(sessionStartTransaction.SetNonce()
			,catchall
			,gnetError("Failed to setup nonce for trasaction."));
		nonceSet = true;

		rcheck(sessionStartTransaction.Serialize(wr)
			,catchall
			,gnetError("Failed to serialize Transaction"));

		rcheck(rlGetTaskManager()->CreateTask(&pSessionRestartTask)
			,catchall
			,gnetError("GetBalanceGameTransactionHttpTask: Failed to create"));

		rcheck(rlTaskBase::Configure(pSessionRestartTask
										,m_localGamerIndex
										,netCatalog::GetInstance().GetRandomItem()
										,wr
										,&m_sessionStartData
										,&m_sessionStartNonceResponse
										,&m_status)
			,catchall
			,gnetError("GetBalanceGameTransactionHttpTask: Failed to Configure"));

		rcheck(rlGetTaskManager()->AddParallelTask(pSessionRestartTask)
			,catchall
			,gnetError("GetBalanceGameTransactionHttpTask: Failed to AddParallelTask."));

		SetState(PENDING_SESSION_RESTART);

		pSessionRestartTask->SetHasNonce();

		return true;
	}
	rcatchall
	{
		//Nonce must be rolled back,
		if (nonceSet)
		{
			GameTransactionSessionMgr::Get().RollbackNounce();
		}

		if(pSessionRestartTask)
		{
			if(pSessionRestartTask->IsPending())
			{
				pSessionRestartTask->Finish(rlTaskBase::FINISH_FAILED, RLSC_ERROR_UNEXPECTED_RESULT);
			}
			else
			{
				m_status.ForceFailed();
			}

			rlGetTaskManager()->DestroyTask(pSessionRestartTask);
		}

		gnetDebug1("GameTransactionSessionMgr:: TriggerGetBalance - FAILED.");
		SetState(FAILED_SESSION_RESTART);
	}

	return false;
}

void GameTransactionSessionMgr::TriggerHeartBeat(const unsigned curTime)
{
	//Make sure no other transactions in progress.
	if (IsQueueBusy())
	{
		m_heartBeatMgr.Reset(curTime);
		return;
	}

	//Make sure we are ready.
	if (!IsReady())
	{
		m_heartBeatMgr.Reset(curTime);
		return;
	}

	//Make sure helper class allows it.
	if (!m_heartBeatMgr.CanSend(curTime))
		return;

	//fucking send it
	SendHeartBeat(curTime);
}

bool GameTransactionSessionMgr::IsQueueBusy() const
{
	if (NETWORK_SHOPPING_MGR.TransactionInProgress())
		return true;

	if (IsRefreshPending())
		return true;

	if (IsRefreshCatalogPending())
		return true;

	if (IsHeartBeatInProgress())
		return true;

	return false;
}

bool GameTransactionSessionMgr::HaveStatsFinishedLoading() const
{
	CProfileStats& profileStatsMgr = CLiveManager::GetProfileStatsMgr();
	if (!profileStatsMgr.GetMPSynchIsDone( ))
	{
		gnetWarning("HaveStatsFinishedLoading() - Profile Stats not Synchronized.");
		return false;
	}

	if ( StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) )
	{
		gnetWarning("HaveStatsFinishedLoading() - Pending load slot='STAT_MP_CATEGORY_DEFAULT'.");
		return false;
	}

	if ( StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT) )
	{
		gnetWarning("HaveStatsFinishedLoading() - Load Failed for slot='STAT_MP_CATEGORY_DEFAULT'.");
		return false;
	}

	if (m_slot != -1)
	{
		if ( StatsInterface::CloudFileLoadPending(m_slot+STAT_MP_CATEGORY_CHAR0) )
		{
			gnetWarning("HaveStatsFinishedLoading() - Pending load slot='%d'.", m_slot+STAT_MP_CATEGORY_CHAR0);
			return false;
		}

		if ( StatsInterface::CloudFileLoadFailed(m_slot+STAT_MP_CATEGORY_CHAR0) )
		{
			gnetWarning("HaveStatsFinishedLoading() - Load Failed for slot='%d'.", m_slot+STAT_MP_CATEGORY_CHAR0);
			return false;
		}
	}

	return true;
}

u32 GameTransactionSessionMgr::GetCharacterSlot() const
{
	if (gnetVerify(!IsSlotInvalid()))
	{
		return static_cast<u32>(m_slot+CHAR_MP0);
	}

	return 0;
}

const GSToken& GameTransactionSessionMgr::GetGsToken() const
{
	return m_sessionStartData.m_gsToken;
}


BANK_ONLY(PARAM(netgametransactionsNonce, "Nonce"););
u64 GameTransactionSessionMgr::GetNextTransactionId()
{
#if __BANK
	if (PARAM_netgametransactionsNonce.Get())
	{
		return 72;
	}
#endif //__BANK
	
	return m_sessionStartData.m_nonceSeed + m_sessionTransactionCount++;
}


bool GameTransactionSessionMgr::RollbackNounce()
{
	if(m_sessionTransactionCount > 0)
	{
		--m_sessionTransactionCount;
	}

	gnetDebug1("RollbackNounce: %" I64FMT "u", m_sessionTransactionCount);

	return true;
}

bool GameTransactionSessionMgr::ApplyData()
{
	bool retVal = false;

	rtry
	{
		rcheck(HaveStatsFinishedLoading(), catchall, gnetWarning("Stats are not loaded."));

		rverify(IsReady(), catchall, );

		rverify(m_sessionStartData.m_inventory.m_slot == m_slot, catchall, );

		NetworkGameTransactions::PlayerBalanceApplicationInfo pbInfo;
		rverify(m_sessionStartData.m_playerBalance.ApplyDataToStats(pbInfo)
				,catchall
				,);

		NetworkGameTransactions::InventoryDataApplicationInfo info;
		rverify(m_sessionStartData.m_inventory.ApplyDataToStats(info)
				,catchall
				,);

		retVal = true;
	}
	rcatchall
	{

	}

	return retVal;
}

bool GameTransactionSessionMgr::ClearStartSession(int slot)
{
	if (!gnetVerify(slot>=0 || HasFailed()))
		return false;

	if (!gnetVerify(slot<MAX_NUM_MP_CHARS))
		return false;

	if (!gnetVerify(m_slot == slot || HasFailed()))
		return false;

	if (!gnetVerify(!IsStartPending()))
		return false;

	gnetDebug1("GameTransactionSessionMgr::ClearStartSession() - m_slot='%d', slot='%d', state='%s'", m_slot, slot, GetStateString(m_state));

	switch (m_state)
	{
	case GameTransactionSessionMgr::READY:
	case GameTransactionSessionMgr::FAILED_SESSION_START:
	case GameTransactionSessionMgr::FAILED_SESSION_RESTART:
		SetState( WAIT_FOR_START_SESSION );
		break;

	default:
		break;
	}

	m_slot = -1;
	m_sessionTransactionCount = 0;
	m_sessionStartData.Clear();

	return true;
}

bool GameTransactionSessionMgr::StartSessionApplyDataPending( ) const
{
	if (m_slot > -1 && m_sessionTransactionCount > 0 && m_state == READY)
	{
		return (!m_sessionStartData.m_playerBalance.IsFinished() 
					&& !m_sessionStartData.m_inventory.IsFinished());
	}

	return false;
}

bool GameTransactionSessionMgr::TriggerCatalogVersionRefresh( )
{
	gnetAssertf( !m_status.Pending(), "TriggerCatalogVersionRefresh( ) - Invalid m_status='%d'", m_status.GetStatus() );
	gnetAssertf( !IsRefreshPending(), "TriggerCatalogVersionRefresh( ) - IsRefreshPending() is TRUE.");
	gnetAssertf( NetworkInterface::GetActiveGamerInfo(), "TriggerCatalogVersionRefresh( ) - NetworkInterface::GetActiveGamerInfo().");

	if (!m_status.Pending() && !IsRefreshPending() && NetworkInterface::GetActiveGamerInfo())
	{
		m_status.Reset();

		return gnetVerify(NetworkGameTransactions::GetCatalogVersion(NetworkInterface::GetActiveGamerInfo()->GetLocalIndex(), &m_status));
	}

	return false;
}

bool GameTransactionSessionMgr::TriggerCatalogRefresh()
{
	if (!netCatalog::GetInstance().GetStatus().Pending() && !IsRefreshPending())
	{
		m_refreshCatalogRequested = false;
		if(CATALOG_SERVER_INST.RequestServerFile())
		{
			gnetDebug1("TriggerCatalogRefresh().");
			SetState(PENDING_CATALOG);
		}
		else
		{
			gnetDebug1("GameTransactionSessionMgr::TriggerCatalogRefreshIfRequested - GetCatalog failed to create task.");
			SetState(FAILED_CATALOG);
			netCatalog::GetInstance().ClearDownloadStatus( );
		}

		return true;
	}

	return false;
}

void GameTransactionSessionMgr::Cancel()
{
	if(m_status.Pending())
	{
		rlGetTaskManager()->CancelTask(&m_status);
	}
}

void GameTransactionSessionMgr::TriggerSessionRefreshIfRequested()
{
	if (m_refreshSessionRequested)
	{
		TriggerSessionRefresh();
	}
}
#if __BANK
void GameTransactionSessionMgr::AddWidgets(bkBank& bank)
{
	bank.PushGroup("GameTransactionSessionMgr");
	{
		bank.AddButton("Invalidate Catalog",  datCallback(MFA(GameTransactionSessionMgr::BankInvalidateCatalog), this));
		bank.AddButton("Invalidate Token",    datCallback(MFA(GameTransactionSessionMgr::BankInvalidateToken), this));
		bank.AddButton("Invalidate Nonce",    datCallback(MFA(GameTransactionSessionMgr::BankInvalidateNonce), this));
		bank.AddButton("Flag Catalog Refresh.",  datCallback(MFA(GameTransactionSessionMgr::FlagCatalogForRefresh), this));
		bank.AddButton("Flag Session Restart.",  datCallback(MFA(GameTransactionSessionMgr::FlagForSessionRestart), this));
		bank.AddButton("Flag Get Balance.",  datCallback(MFA(GameTransactionSessionMgr::BankGetBalance), this));
	}
	bank.PopGroup();

	bank.PushGroup("Fail server Transaction");
	{
		NetworkGameTransactions::Bank_InitWidgets(bank);
	}
	bank.PopGroup();
}

void GameTransactionSessionMgr::BankInvalidateCatalog()
{
	netCatalog::GetInstance().InvalidateCatalog();
	gnetDebug1("BankInvalidateCatalog now");
}

void GameTransactionSessionMgr::BankInvalidateToken()
{
	rlRos::InvalidateOrRequestToken(m_localGamerIndex);

	//m_sessionStartData.m_gsToken.Clear();
	m_sessionStartData.m_nonceSeed = 72;
	m_sessionTransactionCount = 300;
	gnetDebug1("BankInvalidateToken now %" I64FMT "d + %" I64FMT "d = %" I64FMT "d",m_sessionStartData.m_nonceSeed, m_sessionTransactionCount, m_sessionStartData.m_nonceSeed + m_sessionTransactionCount );
}

void GameTransactionSessionMgr::BankInvalidateNonce()
{
	m_sessionStartData.m_nonceSeed = 72;
	m_sessionTransactionCount = 300;
	gnetDebug1("BankInvalidateNonce now %" I64FMT "d + %" I64FMT "d = %" I64FMT "d",m_sessionStartData.m_nonceSeed, m_sessionTransactionCount, m_sessionStartData.m_nonceSeed + m_sessionTransactionCount );
}

void GameTransactionSessionMgr::BankGetBalance( )
{
	GameTransactionSessionMgr::Get().GetBalance(true, true);
}

#endif //__BANK

///PURPOSE
///  Class responsible to gather Presence info about
///   scAdmin updates to the player inventory/balance.
class NetworkGameServerUpdatePresenceMessage
{
public:
	RLPRESENCE_MESSAGE_DECL("gameserver.update");

	NetworkGameServerUpdatePresenceMessage() 
		: m_fullRefreshRequested(false)
		, m_updatePlayerBalance(false)
		, m_awardTypeHash(0)
		, m_awardAmount(0)
	{
	}

	bool Export(RsonWriter& rw) const;
	bool Import(const RsonReader& rr);

public:
	bool m_fullRefreshRequested;
	bool m_updatePlayerBalance;
	atArray<int> m_items;
	u32 m_awardTypeHash;
	s32 m_awardAmount;
};

bool NetworkGameServerUpdatePresenceMessage::Export(RsonWriter& /*rw*/) const
{
	return gnetVerify(false);  //Not sent from clients
}

bool NetworkGameServerUpdatePresenceMessage::Import(const RsonReader& rr)
{
	bool success = false;

	if (gnetVerifyf(rlScPresenceMessage::IsA<NetworkGameServerUpdatePresenceMessage>(rr), "%s: Invalid", MSG_ID()))
	{
		rr.ReadBool("balance", m_updatePlayerBalance);
		rr.ReadBool("fullRefresh", m_fullRefreshRequested);

		//If we're given item, we get a listing of the items we were given to request updates.
		RsonReader itemsListRR;
		if(rr.GetMember("items", &itemsListRR))
		{
			bool itemListSuccess = false;
			RsonReader itemIter;
			for (itemListSuccess = itemsListRR.GetFirstMember(&itemIter); itemListSuccess; itemListSuccess = itemIter.GetNextSibling(&itemIter))
			{
				int tempInt = 0;
				if (itemIter.AsInt(tempInt))
				{
					m_items.PushAndGrow(tempInt);
				}
			}
		}

		rr.ReadUns("awardType", m_awardTypeHash);
		rr.ReadInt("awardAmount", m_awardAmount);

		success = true;
	}

	return success;
}

void GameTransactionSessionMgr::OnPresenceEvent(const rlPresenceEvent* evt)
{
	if ( evt->GetId() == rlPresenceEventScMessage::EVENT_ID() )
	{
		const rlPresenceEventScMessage* pSCEvent = static_cast<const rlPresenceEventScMessage*>(evt);

		if(evt->m_ScMessage->m_Message.IsA<NetworkGameServerUpdatePresenceMessage>())
		{
			NetworkGameServerUpdatePresenceMessage gsUpdateMsg;

			if (gnetVerify(gsUpdateMsg.Import(pSCEvent->m_Message)))
			{
				gnetDebug1("rlPresenceEventScMessage:: event received.");

				sNetworkScAdminPlayerUpdate eventData;

				eventData.m_fullRefreshRequested.Int = gsUpdateMsg.m_fullRefreshRequested;
				eventData.m_updatePlayerBalance.Int  = gsUpdateMsg.m_updatePlayerBalance;
				eventData.m_awardTypeHash.Int        = gsUpdateMsg.m_awardTypeHash;
				eventData.m_awardAmount.Int          = gsUpdateMsg.m_awardAmount;

				u32 numItems = 0;

				for (int i=0; i<gsUpdateMsg.m_items.GetCount(); i++, numItems++)
					eventData.m_items[i].Int = gsUpdateMsg.m_items[i];

				for (int i=numItems; i<sNetworkScAdminPlayerUpdate::MAX_NUM_ITEMS; i++)
					eventData.m_items[i].Int = 0;

				GetEventScriptNetworkGroup()->Add(CEventNetworkScAdminPlayerUpdated(&eventData));
			}
		}
	}
}

#undef __net_channel

#endif // __NET_SHOP_ACTIVE

//eof 
