//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
#ifndef GAMETRANSACTIONSESSIONMGR_H
#define GAMETRANSACTIONSESSIONMGR_H

#include "network/NetworkTypes.h"

#if __NET_SHOP_ACTIVE

#include "atl/singleton.h"
#include "diag/channel.h"
#include "net/status.h"
#include "rline/rlpresence.h"
#include "rline/ros/rlroscommon.h"
#include "rline/ros/rlros.h"
#include "system/ipc.h"
#include "fwnet/netchannel.h"

#include "Network/Shop/GameTransactionsTasks.h"
#include "Network/Shop/NetworkGameTransactions.h"
#include "Stats/StatsTypes.h"

RAGE_DECLARE_SUBCHANNEL(net, GameTransactions);
#undef __net_channel
#define __net_channel net_shop_gameTransactions

namespace rage
{
	class rlRosServiceAccessInfo;
}


//////////////////////////////////////////////////////////////////////////
// GSToken
//////////////////////////////////////////////////////////////////////////

class GSToken
{
public:
	GSToken()
	{
		Clear();
	}

	void Clear()
	{
		m_token[0] = '\0';
	}

	void Set(const char* token)
	{
		safecpy(m_token, token);
	}

	bool IsValid() const { return m_token[0] != '\0'; }

	operator const char*() const { return m_token; }

	char m_token[64];
};


//////////////////////////////////////////////////////////////////////////
// GetAccessTokenHelper
//////////////////////////////////////////////////////////////////////////

class GetAccessTokenHelper
{

public:
	GetAccessTokenHelper()
		: m_localGamerIndex(-1)
		, m_state(IDLE)
		, m_retryCount(0)
	{
	}

	void Init(int localGamerIndex);
	void Shutdown();
	void Update();

	const rlRosServiceAccessInfo* GetAccessToken() const;

	bool IsPending() const {return m_state == WAITING_FOR_TOKEN;}
	bool IsSuccess() const {return m_state == TOKEN_RECEIVED;}
	bool   IsError() const {return !IsPending() && !IsSuccess();}

protected:
	enum State
	{
		IDLE,
		TOKEN_FAILED,
		WAITING_FOR_TOKEN,
		TOKEN_RECEIVED,
	};

	int m_localGamerIndex;
	int m_retryCount;
	State m_state;
};


//////////////////////////////////////////////////////////////////////////
// SessionStartResponseData
//////////////////////////////////////////////////////////////////////////
class SessionStartResponseData
{
public:

	struct sSlotCreateTimestamp
	{
	public:
		sSlotCreateTimestamp() {Clear();}

		void Clear()
		{
			for (int i=0; i<MAX_NUM_MP_CHARS; i++)
				m_posixtime[i] = 0;
		}

		bool Deserialize( const RsonReader& rr );

		bool IsValid(const int slot) const;

	private:
		u64 m_posixtime[MAX_NUM_MP_CHARS];
	};

public:
	SessionStartResponseData() { Clear(); }

	void Clear() 
	{
		m_inventory.Clear();
		m_playerBalance.Clear();
		m_gsToken.Clear();
		m_nonceSeed = 0;
		m_heartBeatSec = 0;
	}

	NetworkGameTransactions::PlayerBalance     m_playerBalance;
	NetworkGameTransactions::InventoryItemSet  m_inventory;
	GSToken                                    m_gsToken;
	u64                                        m_nonceSeed;
	u32                                        m_heartBeatSec;
	sSlotCreateTimestamp                       m_slotCreateTimestamp;
};


//////////////////////////////////////////////////////////////////////////
// HeartBeatHelper
//////////////////////////////////////////////////////////////////////////

class HeartBeatHelper
{
	friend class GameTransactionSessionMgr;

private:
	static const u32 DEFAULT_MAX_INTERVAL = 1000*60*5;

public:
	HeartBeatHelper()
		: m_data(-1)
		, m_nextExpireTimeMs(0)
		, m_timeExpiredIntervalMs(DEFAULT_MAX_INTERVAL)
		, m_failedAttempts(0)
	{}
	~HeartBeatHelper();

	void       Init( const u32 intervalSeconds );
	void      Reset(const unsigned curTime);
	void     Cancel( );
	bool    CanSend(const unsigned curTime) const;
	void    AddItem(const int itemId, const int value, const int price);
	void      Clear( );
	bool     Exists(const int itemId) const;

private:
	NetworkGameTransactions::GameTransactionItems  m_data;
	SessionStartResponseData                       m_responseData;
	NetworkGameTransactions::TelemetryNonce        m_responseNonce;
	netStatus                                      m_status;
	u32											   m_nextExpireTimeMs;
	u32                                            m_timeExpiredIntervalMs;
	u32                                            m_failedAttempts;
};


//////////////////////////////////////////////////////////////////////////
// BaseSessionGameTransactionHttpTask
//////////////////////////////////////////////////////////////////////////

class BaseSessionGameTransactionHttpTask : public GameTransactionHttpTask
{
public:	
	RL_TASK_DECL(BaseSessionGameTransactionHttpTask);
	RL_TASK_USE_CHANNEL(net_shop_gameTransactions);

	BaseSessionGameTransactionHttpTask(sysMemAllocator* allocator = NULL, u8* bounceBuffer = NULL, const unsigned sizeofBounceBuffer = 0) 
		: GameTransactionHttpTask(allocator, bounceBuffer, sizeofBounceBuffer)
		, m_pResponseData(NULL)
	{
	}

	bool Configure(const int localGamerIndex
					,const u32 challengeResponseInfo
					,RsonWriter& wr
					,SessionStartResponseData* outResponseData
					,NetworkGameTransactions::TelemetryNonce* outTelemetryNonce);

protected:
	virtual bool DeserializePlayerInfo(const RsonReader& rr);
	virtual bool DeserializeSessionInfo(const RsonReader& rr);

	virtual bool UsesGSToken() const { return false; } //Most of the session Transactions don't use the GSToken.

	SessionStartResponseData* m_pResponseData;
};


//////////////////////////////////////////////////////////////////////////
// SessionStartGameTransactionHttpTask
//////////////////////////////////////////////////////////////////////////

class SessionStartGameTransactionHttpTask : public BaseSessionGameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("SessionStart");

public:	
	RL_TASK_DECL(SessionStartGameTransactionHttpTask);
	RL_TASK_USE_CHANNEL(net_shop_gameTransactions);

	// Use the Streaming memory, if we don't have enough we can try using the network heap.
	SessionStartGameTransactionHttpTask();

	virtual const char* GetServiceEndpoint() const { return "SessionStart"; }

	//PURPOSE
	//  Called when the base class finishes this finish can be delayed due to Latency testing.
	virtual void OnFinish(const int resultCode);

protected:
	virtual bool ProcessSuccess(const RsonReader &rr);
};


//////////////////////////////////////////////////////////////////////////
// SessionRestartGameTransactionHttpTask
//////////////////////////////////////////////////////////////////////////

class SessionRestartGameTransactionHttpTask : public BaseSessionGameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("SessionRestart");

public:
	RL_TASK_DECL(SessionRestartGameTransactionHttpTask);
	RL_TASK_USE_CHANNEL(net_shop_gameTransactions);

	virtual const char* GetServiceEndpoint() const { return "SessionRestart"; }

	//PURPOSE
	//  Called when the base class finishes this finish can be delayed due to Latency testing.
	virtual void OnFinish(const int resultCode);

protected:
	virtual bool CanFlagForSessionRestart() const {return false;}
	virtual bool CanTriggerCatalogRefresh() const {return false;}

	virtual bool ProcessSuccess(const RsonReader &rr);
};


//////////////////////////////////////////////////////////////////////////
// GetBalanceGameTransactionHttpTask
//////////////////////////////////////////////////////////////////////////

class GetBalanceGameTransactionHttpTask : public BaseSessionGameTransactionHttpTask
{
	GAME_SERVER_ERROR_BANK("GetBalance");

public:
	RL_TASK_DECL(GetBalanceGameTransactionHttpTask);
	RL_TASK_USE_CHANNEL(net_shop_gameTransactions);

	virtual const char* GetServiceEndpoint() const { return "GetBalance"; }

	//PURPOSE
	//  Called when the base class finishes this finish can be delayed due to Latency testing.
	virtual void OnFinish(const int resultCode);

protected:
	virtual bool UsesGSToken() const { return true; } //Most of the session Transactions don't use the GSToken.

	virtual bool CanFlagForSessionRestart() const {return false;}
	virtual bool CanTriggerCatalogRefresh() const {return false;}

	virtual bool ProcessSuccess(const RsonReader &rr);
};


//////////////////////////////////////////////////////////////////////////
// HeartBeatGameTransactionHttpTask
//////////////////////////////////////////////////////////////////////////

class HeartBeatGameTransactionHttpTask : public BaseSessionGameTransactionHttpTask
{
	static const u32 MAX_ATTEMPTS = 2;

	GAME_SERVER_ERROR_BANK("HeartBeat");

public:
	RL_TASK_DECL(HeartBeatGameTransactionHttpTask);
	RL_TASK_USE_CHANNEL(net_shop_gameTransactions);

	HeartBeatGameTransactionHttpTask() 
		: BaseSessionGameTransactionHttpTask()
		, m_timeIntervalMs(NULL)
		, m_failedAttempts(NULL)
	{
	}

	bool Configure(const int localGamerIndex
					,const u32 challengeResponseInfo
					,RsonWriter& wr
					,SessionStartResponseData* outResponseData
					,NetworkGameTransactions::TelemetryNonce* outTelemetryNonce
					,u32* timeIntervalSec
					,u32* failedAttempts);

	virtual const char* GetServiceEndpoint() const { return "UpdateData"; }

	virtual bool   UsesGSToken( ) const { return true; } //The heartbeat does use GSToken and burns a nonce.
	virtual void        Finish(const FinishType finishType, const int resultCode = 0);
	void          CheckFailure(const FinishType finishType);

protected:
	virtual bool        ProcessSuccess(const RsonReader &rr);
	virtual bool DeserializePlayerInfo(const RsonReader& rr);

private:
	u32* m_timeIntervalMs;
	u32* m_failedAttempts;
};


//////////////////////////////////////////////////////////////////////////
// GameTransactionSessionMgr
//////////////////////////////////////////////////////////////////////////

class GameTransactionSessionMgr : public datBase
{
	friend class SessionRestartGameTransactionHttpTask;
	friend class SessionStartGameTransactionHttpTask;
	friend class GetBalanceGameTransactionHttpTask;
	friend class HeartBeatGameTransactionHttpTask;

public:
	enum eState
	{
		INVALID
		,PENDING_ACCESS_TOKEN
		,WAIT_FOR_START_SESSION
		,PENDING_CATALOG_VERSION
		,PENDING_CATALOG
		,CATALOG_COMPLETE
		,PENDING_SESSION_START
		,PENDING_SESSION_RESTART
		,READY
		,FAILED_ACCESS_TOKEN
		,FAILED_CATALOG
		,FAILED_SESSION_START
		,FAILED_SESSION_RESTART
		,FAILED_GAME_VERSION
	};

public:
	typedef atSingleton<GameTransactionSessionMgr> SGameTransactionSessionMgr;

	static GameTransactionSessionMgr& Get()
	{
		if (!SGameTransactionSessionMgr::IsInstantiated())
		{
			SGameTransactionSessionMgr::Instantiate();
		}
		return SGameTransactionSessionMgr::GetInstance();
	}

	static const int MAX_GROW_BUFFER_SIZE = 1840 * 1024;

	GameTransactionSessionMgr()
		: m_localGamerIndex(-1)
		, m_slot(-1)
		, m_sessionTransactionCount(0)
		, m_state(INVALID)
		, m_applyDataOnSuccess(false)
		, m_refreshCatalogRequested(true)
		, m_refreshSessionRequested(false)
		, m_usingGameTransactionToken(false)
		, m_isAnySessionActive(false)
		, m_triggerCrcCheck(false)
		, m_delayedBailReason (BAIL_INVALID)
		, m_delayedcontext(0)
		, m_growBufferSize(MAX_GROW_BUFFER_SIZE)
	{}

	virtual ~GameTransactionSessionMgr() {}

	void Init();
	void Shutdown();

	bool InitManager(const int localGamerIndex);
	void ShutdownManager();
	bool SessionStart(const int localGamerIndex, const int slot, bool applyDataOnSuccess = true);
	bool GetBalance(const bool inventory, const bool playerbalance);
	void Update(const unsigned curTime);
	void Cancel();

	bool          IsAnySessionActive() const {return m_isAnySessionActive;}
	bool		             IsReady() const {return m_state == READY;}
	bool	        IsRefreshPending() const {return m_state == PENDING_SESSION_RESTART || m_refreshSessionRequested;}
	bool	 IsRefreshCatalogPending() const {return m_state == PENDING_CATALOG || m_refreshCatalogRequested;}
	bool                   IsInvalid() const {return m_state == INVALID;}
	bool               IsInitPending() const {return m_state != INVALID && m_state < WAIT_FOR_START_SESSION;}
	bool            HasInitSucceeded() const {return !HasFailed() && !IsInitPending();}
	bool              IsStartPending() const {return m_state != INVALID && m_state < READY && m_state > PENDING_ACCESS_TOKEN;}
	bool           IsCatalogComplete() const {return m_state == CATALOG_COMPLETE;}
	bool                   HasFailed() const {return m_state > READY;}
	bool   IsRefreshSessionRequested() const {return m_refreshSessionRequested;}
	bool	             IsQueueBusy() const;
	int                 GetErrorCode() const {return m_status.GetResultCode();}
	int 	                GetState() const {return m_state;}
	bool       IsHeartBeatInProgress() const {return m_heartBeatMgr.m_status.Pending();}

	bool  HaveStatsFinishedLoading() const;
	u32           GetCharacterSlot() const;

	bool      IsChangingSlot(const int slot) const {return (m_slot != slot && (m_state >= WAIT_FOR_START_SESSION || m_state == READY));}

	const GSToken&           GetGsToken( ) const;
	u64            GetNextTransactionId( );
	bool                 RollbackNounce( );

	int        GetSlot(        ) const {return m_slot;}
	bool IsSlotInvalid(        ) const {return (m_slot==-1);}
	bool   IsSlotValid(int slot) const {return m_slot==slot && IsReady();}
	bool     ApplyData(        );

	bool ClearStartSession(int slot);
	bool StartSessionApplyDataPending( ) const;

	void          UnFlagCatalogForRefresh( ) {m_refreshCatalogRequested = false;}
	void            FlagCatalogForRefresh( ) {m_refreshCatalogRequested = true;}
	void TriggerCatalogRefreshIfRequested( ) {if(m_refreshCatalogRequested){TriggerCatalogRefresh();}}
	bool     TriggerCatalogVersionRefresh( );
	bool            TriggerCatalogRefresh( );
	void            FlagForSessionRestart( );

	BANK_ONLY( void AddWidgets(bkBank& bank); )

	//Telemetry nonce being sent right now.
	void SetNonceForTelemetry(const u64 nonceSeed) {m_telemetryNonceSeed = nonceSeed;}
	s64  GetNonceForTelemetry( ) const {return static_cast< s64 >(m_telemetryNonceSeed);}

	HeartBeatHelper& GetHeartBeatHelper() { return m_heartBeatMgr;}

	//Return TRUE if the slot is valid
	bool GetIsSlotValid(const int slot);

	//PURPOSE
	// Called when the we need to Bail the net game when the game server is in bad state.
	void Bail(const eState newState, const eBailReason nBailReason, const int context, const bool delayed = false);

protected:
	const char* GetStateString(const eState state) const;
	void SetState(const eState newState);

	void HandleSessionStartFinish();
	void HandleSessionRefreshFinish();
	void HandleGetBalanceFinish();
	void HandleHeartBeatFinish();

	void TriggerSessionStart();

	void TriggerSessionRefreshIfRequested();
	void TriggerSessionRefresh();
	bool TriggerGetBalance(const bool inventory, const bool playerbalance);
	void TriggerHeartBeat(const unsigned curTime);

	void OnPresenceEvent(const rlPresenceEvent* event);

	void SendHeartBeat(const unsigned curTime);

public:
	const char* GetStateString() const { return GetStateString(m_state);}

	netStatus&       GetDeleteStatus() {return m_deleteStatus;}
	netStatus& GetCashTransferStatus() {return m_cashtransferStatus;}

	u8*       GetBounceBuffer()       {return m_bounceBuffer;}
	u32   GetBounceBufferSize() const {return (830*1024);}

	u8*       GetGrowBuffer()       {return m_growBuffer;}
	u32   GetGrowBufferSize() const {return m_growBufferSize;}
	void IncreaseGrowBufferSize() { m_growBufferSize += (1024*64); }

private:
	bool                                     m_isAnySessionActive;
	bool                                     m_applyDataOnSuccess;
	bool                                     m_refreshCatalogRequested;
	bool                                     m_refreshSessionRequested;
	bool					   				 m_usingGameTransactionToken;
	bool					   				 m_triggerCrcCheck;
	int                                      m_localGamerIndex;
	int                                      m_slot;
	eState                                   m_state;
	GetAccessTokenHelper                     m_accessToken;
	SessionStartResponseData                 m_sessionStartData;
	NetworkGameTransactions::TelemetryNonce  m_sessionStartNonceResponse;
	netStatus                                m_status;
	u64		                                 m_sessionTransactionCount;
	u64                                      m_telemetryNonceSeed;
	rlPresence::Delegate                     m_presDelegate;
	HeartBeatHelper                          m_heartBeatMgr;
	netStatus                                m_deleteStatus;
	netStatus                                m_cashtransferStatus;
	u8*                                      m_bounceBuffer;
	u8*                                      m_growBuffer;
	u32                                      m_growBufferSize;
	eBailReason                              m_delayedBailReason;
	int                                      m_delayedcontext;
	// Used to prevent update when we are Bailing 
	static rage::sysIpcMutex sm_Mutex;

#if __BANK
	void BankInvalidateCatalog();
	void BankInvalidateToken();
	void BankInvalidateNonce();
	void BankGetBalance();
#endif
};

//////////////////////////////////////////////////////////////////////////
/// GetTokenAndRunHTTPTask
//////////////////////////////////////////////////////////////////////////
template<typename tHttpTask, typename tResponceData, typename tTransaction, typename tTelemetryNonce>
class GetTokenAndRunHTTPTask : public rlTaskBase
{
	friend class GameTransactionSessionMgr;
public:

	RL_TASK_DECL(GetTokenAndRunHTTPTask);
	RL_TASK_USE_CHANNEL(net_shop_gameTransactions);

	GetTokenAndRunHTTPTask()
		: m_state(INVALID)
		, m_responseData(NULL)
	{

	}
	virtual ~GetTokenAndRunHTTPTask()
	{

	}

	bool Configure(const int localGamerIndex, tTransaction* pTrans, tResponceData* outResponseData, tTelemetryNonce* outResponseTelemetryNonce);

	virtual bool IsCancelable() const
	{
		return true;
	}

	virtual void Start();
	virtual void Update(const unsigned timeStep);
	virtual void DoCancel();

protected:

	bool DoTask();

	enum State
	{
		INVALID,
		PENDING_TOKEN,
		PENDING_TASK,
	};

	State m_state;

	int m_localGamerIndex;

	GetAccessTokenHelper m_accessToken;
	tTransaction m_trans;
	tResponceData* m_responseData;
	tTelemetryNonce* m_responseTelemetryNonce;

	netStatus m_httpStatus;
};

template<typename tHttpTask, typename tResponceData, typename tTransaction, typename tTelemetryNonce>
bool GetTokenAndRunHTTPTask<tHttpTask, tResponceData, tTransaction, tTelemetryNonce>::Configure(const int localGamerIndex
																								,tTransaction* pTrans
																								,tResponceData* outResponseData
																								,tTelemetryNonce* outResponseTelemetryNonce)
{
	if(!gnetVerify(outResponseTelemetryNonce))
		return false;

	if(!gnetVerify(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex)))
		return false;

	m_localGamerIndex        = localGamerIndex;
	m_responseData           = outResponseData;
	m_responseTelemetryNonce = outResponseTelemetryNonce;

	if(pTrans)
	{
		m_trans = *pTrans;
	}

	return true;
}

template<typename tHttpTask, typename tResponceData, typename tTransaction, typename tTelemetryNonce>
void GetTokenAndRunHTTPTask<tHttpTask, tResponceData, tTransaction, tTelemetryNonce>::Start()
{
	rlTaskBase::Start();

	m_state = PENDING_TOKEN;

	m_accessToken.Init(m_localGamerIndex);
}

template<typename tHttpTask, typename tResponceData, typename tTransaction, typename tTelemetryNonce>
bool GetTokenAndRunHTTPTask<tHttpTask, tResponceData, tTransaction, tTelemetryNonce>::DoTask()
{
	tHttpTask* pHttpTask = NULL;

	rtry																				
	{
		rverify(RL_IS_VALID_LOCAL_GAMER_INDEX(m_localGamerIndex), catchall, gnetError("INVALID GAMER INDEX"));

		char dataBuffer[1024];
		RsonWriter wr(dataBuffer, RSON_FORMAT_JSON);

		wr.Begin(NULL, NULL);

#if !__FINAL
		NetworkGameTransactions::ApplyUserInfoHack(m_localGamerIndex, wr);
#endif

		rcheck(rlGetTaskManager()->CreateTask(&pHttpTask)
			,catchall
			,gnetError("NetworkGameTransactions::SessionStart: Failed to create"));

#if __BANK
		rcheck(pHttpTask->BankSerializeForceError(wr)
			,catchall
			,gnetError("Failed to serialize 'forceerror'"));
#endif // __BANK

		rcheck(m_trans.Serialize(wr)
			,catchall
			,gnetError("Failed to serialize Transaction"));

		rcheck(rlTaskBase::Configure(pHttpTask
										,m_localGamerIndex
										,0
										,wr
										,m_responseData
										,m_responseTelemetryNonce
										,&m_httpStatus), 
			catchall,
			gnetError("GetTokenAndRunHTTPTask: Failed to Configure"));

		rcheck(rlGetTaskManager()->AddParallelTask(pHttpTask), catchall,);

		return true;																	
	}
	rcatchall
	{
		if(pHttpTask)							
		{																		
			if(pHttpTask->IsPending())													
			{																		
				pHttpTask->Finish(rlTaskBase::FINISH_FAILED, RLSC_ERROR_UNEXPECTED_RESULT);						                    
			}																		
			else																	
			{																		
				m_httpStatus.ForceFailed();													
			}

			rlGetTaskManager()->DestroyTask(pHttpTask);
		}	
	}

	return false;
}

template<typename tHttpTask, typename tResponceData, typename tTransaction, typename tTelemetryNonce>
void GetTokenAndRunHTTPTask<tHttpTask, tResponceData, tTransaction, tTelemetryNonce>::Update( const unsigned timeStep )
{
	rlTaskBase::Update(timeStep);

	switch(m_state)
	{
	case PENDING_TOKEN:
		{
			m_accessToken.Update();
			if(m_accessToken.IsSuccess())
			{
				if(DoTask())
				{
					m_state = PENDING_TASK;
				}
				else
				{
					rlTaskDebug("Failed to initiate task");
					this->Finish(rlTaskBase::FINISH_FAILED);
				}
			}
			else if(m_accessToken.IsError())
			{
				rlTaskDebug("Failed - Acces Token Error");
				this->Finish(rlTaskBase::FINISH_FAILED);
			}
		}
		break;
	case PENDING_TASK:
		{
			if(!m_httpStatus.Pending())
			{
				if (m_httpStatus.Succeeded())
				{
					this->Finish(rlTaskBase::FINISH_SUCCEEDED);
				}
				else
				{
					this->Finish(rlTaskBase::FINISH_FAILED, m_httpStatus.GetResultCode());
				}
			}
		}
		break;
	case INVALID:
		{
			rlTaskDebug("GetTokenAndRunHTTPTask::Update - Invalid State, failing out");
			this->Finish(rlTaskBase::FINISH_FAILED);
		}
	}
}

template<typename tHttpTask, typename tResponceData, typename tTransaction, typename tTelemetryNonce>
void GetTokenAndRunHTTPTask<tHttpTask, tResponceData, tTransaction, tTelemetryNonce>::DoCancel()
{
	rlTaskBase::DoCancel();

	if(m_httpStatus.Pending())
	{
		rlGetTaskManager()->CancelTask(&m_httpStatus);
	}
}

#endif //__NET_SHOP_ACTIVE

#endif //GAMETRANSACTIONSESSIONMGR_H