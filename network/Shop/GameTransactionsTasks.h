
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
#ifndef GAMETRANSACTIONSTASK_H
#define GAMETRANSACTIONSTASK_H

#include "network/NetworkTypes.h"

#if __NET_SHOP_ACTIVE

#include "diag/channel.h"
#include "rline/rlhttptask.h"
#include "bank/bank.h"
#include "atl/bitset.h"

#define __NET_GAMETRANS_FAKE_LATENCY __BANK

RAGE_DECLARE_SUBCHANNEL(net, shop_gameTransactions);

namespace rage 
{ 
	class RsonWriter;
	class RsonReader;
}

class DeleteSlotResponseData;

namespace NetworkGameTransactions
{
	class PlayerBalance;
	class InventoryItemSet;
	class TelemetryNonce;
}


#if __BANK

//PURPOSE: Deal with forcing game server errors and getting them triggered at the server side.
enum eGameServerErrorCodes
{
	 GS_ERROR_NOT_IMPLEMENTED
	,GS_ERROR_SYSTEM_ERROR
	,GS_ERROR_INVALID_DATA
	,GS_ERROR_INVALID_PRICE
	,GS_ERROR_INCORRECT_FUNDS
	,GS_ERROR_INSUFFICIENT_INVENTORY
	,GS_ERROR_INVALID_ITEM
	,GS_ERROR_INVALID_PERMISSIONS
	,GS_ERROR_INVALID_CATALOG_VERSION
	,GS_ERROR_INVALID_TOKEN
	,GS_ERROR_INVALID_REQUEST
	,GS_ERROR_INVALID_ACCOUNT
	,GS_ERROR_INVALID_NONCE
	,GS_ERROR_EXPIRED_TOKEN
	,GS_ERROR_CACHE_OPERATION
	,GS_ERROR_INVALID_LIMITER_DEFINITION
	,GS_ERROR_GAME_TRANSACTION_EXCEPTION
	,GS_ERROR_EXCEED_LIMIT
	,GS_ERROR_EXCEED_LIMIT_MESSAGE
	,GS_ERROR_AUTH_FAILED
	,GS_ERROR_STALE_DATA
	,GS_ERROR_BAD_SIGNATURE
	,GS_ERROR_NET_MAINTENANCE
	,GS_ERROR_USER_FORCE_BAIL
	,GS_ERROR_CLIENT_FORCE_FAILED
	,GS_ERROR_INVALID_GAME_VERSION
    ,GS_ERROR_INVALID_ENTITLEMENT

	,GS_ERROR_MAX
};
const char* GetGameServerErrorCodeName(const int value);

//PURPOSE: Helper class to deal with serializing different game server error codes.
class ForceErrorHelper
{
public:
	ForceErrorHelper() : m_bits(GS_ERROR_MAX) {;}

	bool  Serialize(RsonWriter& wr) const;

public:
	atBitSet m_bits;
};

#define GAME_SERVER_ERROR_BANK(name) \
	public: \
		static ForceErrorHelper sm_errors; \
		virtual bool BankSerializeForceError(RsonWriter& wr) const {return sm_errors.Serialize(wr);} \
		static void  Bank_InitWidgets(bkBank& bank) \
		{ \
			bank.PushGroup(#name); \
			{ \
				for(int i=0; i<GS_ERROR_MAX; i++) \
					bank.AddToggle(GetGameServerErrorCodeName(i), &sm_errors.m_bits, (u8)i, NullCB); \
			} \
			bank.PopGroup(); \
		}; \

#else
#define GAME_SERVER_ERROR_BANK(name)
#endif // __BANK

class GameTransactionBaseHttpTask : public rlHttpTask
{
	typedef rlHttpTask Base;

public:
	RL_TASK_DECL(GameTransactionBaseHttpTask);
	RL_TASK_USE_CHANNEL(net_shop_gameTransactions);

	GameTransactionBaseHttpTask(sysMemAllocator* allocator = NULL, u8* bounceBuffer = NULL, const unsigned sizeofBounceBuffer = 0) 
		: Base(allocator, bounceBuffer, sizeofBounceBuffer)
		, m_localGamerIndex(-1)
		, m_HasNonce(false)
	{
#if __NET_GAMETRANS_FAKE_LATENCY
		m_FakeLatency = 0;
		m_bFinishedRequested = false;
#endif // __NET_GAMETRANS_FAKE_LATENCY
	}

	//PURPOSE
	//  Base configuration call.
	//PARAMS
	//  transactionData - json string with the data associated with this endpoint.
	//  dataLen         - length of the string.
	bool Configure(const int localGamerIndex, const u32 challengeResponseInfo, RsonWriter& wr);

	virtual const char* GetServiceEndpoint() const = 0;

	virtual const char* GetServerAppName() const;

	virtual const char* ContentType() const; 

	virtual bool UseHttps() const { return true; }

	virtual SSL_CTX* GetSslCtx() const { return rlRos::GetRosSslContext(); }

	virtual const char* GetServiceName() const;
	virtual const char* GetUrlHostName(char* hostnameBuf, const unsigned sizeofBuf) const;
	virtual bool GetServicePath(char* svcPath, const unsigned maxLen) const;
	virtual netHttpVerb GetHttpVerb() const { return NET_HTTP_VERB_POST; }

#if __NET_GAMETRANS_FAKE_LATENCY
	virtual void Update(const unsigned timeStep);
	virtual void Finish(const FinishType finishType, const int resultCode = 0);
#else
	virtual void Finish(const FinishType finishType, const int resultCode = 0);
#endif // __NET_GAMETRANS_FAKE_LATENCY

	virtual bool ProcessResponse(const char* response, int& resultCode);
	virtual bool ProcessSuccess(const RsonReader &UNUSED_PARAM(rr)) {return true;}
	virtual bool ProcessError(const RsonReader &rr);

	static int GetTransactionEventErrorCode(const atHashString& codestringhash);

	//PURPOSE
	//  Called when the base class finishes this finish can be delayed due to Latency testing.
	virtual void OnFinish(const int resultCode);

#if __BANK
public:
	virtual bool  BankSerializeForceError(RsonWriter& /*wr*/) const {return true;}
#endif // __BANK


protected:

	//PURPOSE
	//  Called to Serialize extra information.
	virtual bool  Serialize(RsonWriter& wr) const;
	
	//Virtual override to specify when the transaction should try to authenticate the server
	//using challenge-response authentication.
	virtual bool HasTransactionData() const { return true; }

	virtual bool CanFlagForSessionRestart() const {return true;}
	virtual bool CanTriggerCatalogRefresh() const {return true;}

	virtual bool ApplyServiceTokenHeader(const int localGamerIndex);
	
	virtual bool ProcessResponseCatalogInfo(const RsonReader& rr);
	virtual bool ProcessChallengeResponseInfo(const RsonReader& rr);
	virtual bool ProcessGameVersionInfo(const RsonReader& rr);

	//Virtual override to specify when the transaction should try to authenticate the server
	//using challenge-response authentication.
	virtual bool UseChallengeResponseInfo() const { return true; }

	//Virtual override to specify when the transaction should use the GStoken when available 
	//or the RSAccessToken
	virtual bool UsesGSToken() const { return true; }

	atString GetGSToken();
	atString GetRSAccessToken(const int localGamerIndex);

public:

	//Returns TRUE if the transaction set the nonce and has to be rolled-back
	virtual bool  HasNonce() const {return m_HasNonce;}
	void SetHasNonce() {m_HasNonce=true;}

protected:
	int         m_localGamerIndex;
	int         m_sessionCri;
	bool        m_HasNonce;

#if __NET_GAMETRANS_FAKE_LATENCY
	int         m_FakeLatency;
	bool        m_bFinishedRequested;
	FinishType  m_FinishTypeRequested;
	int         m_FinishResultCode;
#endif // __NET_GAMETRANS_FAKE_LATENCY
};

class GameTransactionHttpTask : public GameTransactionBaseHttpTask
{
	typedef GameTransactionBaseHttpTask Base;

public:
	RL_TASK_DECL(GameTransactionHttpTask);
	RL_TASK_USE_CHANNEL(net_shop_gameTransactions);

	GameTransactionHttpTask(sysMemAllocator* allocator = NULL, u8* bounceBuffer = NULL, const unsigned sizeofBounceBuffer = 0) 
		: Base(allocator, bounceBuffer, sizeofBounceBuffer)
		, m_LocalGamerIndex(-1)
		, m_pPlayerBalanceResponse(NULL)
		, m_pItemSet(NULL)
	{
	}

	//PURPOSE
	//  Specialized configuration for using the return types.
	//PARAMS
	//  transactionData - json string with the data associated with this endpoint
	//  dataLen         - length of the string
	//  outPB           - structure to output the player balance in the response.  Can be NULL.
	//  outItemSet      - structure to output the item set in the response.  Can be NULL.
	bool Configure(const int localGamerIndex
					,const u32 challengeResponseInfo
					,RsonWriter& wr
					,NetworkGameTransactions::PlayerBalance* outPB
					,NetworkGameTransactions::InventoryItemSet* outItemSet
					,NetworkGameTransactions::TelemetryNonce* outTelemetryNonce);

	//PURPOSE
	//  Base configuration call.
	bool Configure(const int localGamerIndex
		,const u32 challengeResponseInfo
		,RsonWriter& wr
					,NetworkGameTransactions::TelemetryNonce* outTelemetryNonce);

	//PURPOSE
	//  Called when the base class finishes this finish can be delayed due to Latency testing.
	virtual void OnFinish(const int resultCode);

	//PURPOSE
	//  Apply server Data.
	virtual bool ApplyServerData() const {return false;}

protected:
	virtual bool ProcessSuccess(const RsonReader &rr); 

protected:
	int                                        m_LocalGamerIndex;
	NetworkGameTransactions::PlayerBalance*    m_pPlayerBalanceResponse;
	NetworkGameTransactions::InventoryItemSet* m_pItemSet;
	NetworkGameTransactions::TelemetryNonce*   m_pTelemetryNonce;
};

class NULLGameTransactionHttpTask : public rlHttpTask
{
public:

	bool Configure( int fakeLatency );	

	virtual void Start();

	virtual void Update(const unsigned timeStep);

protected:
	//Return true to use HTTPS, false to use HTTP
	virtual bool UseHttps() const {return true;}

	virtual SSL_CTX* GetSslCtx() const { return rlRos::GetRosSslContext(); }

	//Returns the host name part of the URL to be used.
	//E.g. http://{hostname}/path/to/resource?arg1=0&arg2=1
	virtual const char* GetUrlHostName(char* hostnameBuf, const unsigned sizeofBuf) const
	{
		return safecpy(hostnameBuf, "localhost", sizeofBuf);
	}

	//Returns the path part of the URL to be used.
	//E.g. http://example.com/{path to resource}?arg1=0&arg2=1
	virtual bool GetServicePath(char* svcPath, const unsigned maxLen) const
	{
		safecpy(svcPath, "gametransactions/null", maxLen);
		return true;
	}

	virtual bool ProcessResponse(const char* , int& )
	{
		return true;
	}

	int m_FakeLatency; 
};

class DeleteSlotTransactionHttpTask: public GameTransactionHttpTask
{
public:
	GAME_SERVER_ERROR_BANK("DeleteSlot");
	RL_TASK_DECL(DeleteSlotTransactionHttpTask);
	RL_TASK_USE_CHANNEL(net_shop_gameTransactions);

	virtual const char* GetServiceEndpoint() const { return "DeleteSlot"; }

	bool Configure(const int localGamerIndex
					,const u32 challengeResponseInfo
					,RsonWriter& wr
					,DeleteSlotResponseData* outResponseData
					,NetworkGameTransactions::TelemetryNonce* outTelemetryNonce);

	virtual bool UsesGSToken() const { return false; } //The session Transactions don't use the GSToken.

	virtual bool UseChallengeResponseInfo() const { return false; }
};

#endif //__NET_SHOP_ACTIVE

#endif //GAMETRANSACTIONSTASK_H

//eof
