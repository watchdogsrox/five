#ifndef SECURITY_RAGESECGAMEINTERFACE_H
#define SECURITY_RAGESECGAMEINTERFACE_H

#include "security/ragesecengine.h"
#include "security/ragesecpluginmanager.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Network/General/NetworkAssetVerifier.h"
#include "Network/Live/NetworkTelemetry.h"
#include "Network/Network.h"
#include "Network/NetworkInterface.h"
#if RSG_PC
#include "Network/Shop/NetworkShopping.h"
#endif

RAGE_DECLARE_CHANNEL(rageSecGame)

#define rageSecGameAssert(cond)							RAGE_ASSERT(rageSecGame,cond)
#define rageSecGameAssertf(cond,fmt,...)				RAGE_ASSERTF(rageSecGame,cond,fmt,##__VA_ARGS__)
#define rageSecGameFatalAssertf(cond,fmt,...)			RAGE_FATALASSERTF(rageSecGame,cond,fmt,##__VA_ARGS__)
#define rageSecGameVerifyf(cond,fmt,...)				RAGE_VERIFYF(rageSecGame,cond,fmt,##__VA_ARGS__)
#define rageSecGameErrorf(fmt,...)						RAGE_ERRORF(rageSecGame,fmt,##__VA_ARGS__)
#define rageSecGameWarningf(fmt,...)					RAGE_WARNINGF(rageSecGame,fmt,##__VA_ARGS__)
#define rageSecGameDisplayf(fmt,...)					RAGE_DISPLAYF(rageSecGame,fmt,##__VA_ARGS__)
#define rageSecGameDebugf1(fmt,...)						RAGE_DEBUGF1(rageSecGame,fmt,##__VA_ARGS__)
#define rageSecGameDebugf2(fmt,...)						RAGE_DEBUGF2(rageSecGame,fmt,##__VA_ARGS__)
#define rageSecGameDebugf3(fmt,...)						RAGE_DEBUGF3(rageSecGame,fmt,##__VA_ARGS__)
#define rageSecGameLogf(severity,fmt,...)				RAGE_LOGF(rageSecGame,severity,fmt,##__VA_ARGS__)


#define RAGE_SEC_GAME_PLUGIN_BONDER_MAX_TIME_LAST_PLUGIN_REFRESH 60*1000*1
#define RAGE_SEC_GAME_PLUGIN_BONDER_MAX_TIME_LAST_CHECK 60*1000*3
#define RAGE_SEC_GAME_PLUGIN_BONDER_DEBUGGING 0
#define RAGE_SEC_INLINE_POP_REACTION_QUEUE __FINAL
#define KICK_MIN_DELAY 1000*2
#define KICK_MAX_DELAY 1000*5
#if USE_RAGESEC
class rageSecGamePluginManager
{
public: 
	static void Init(u32 initializationMode);
	static void InitWidgets();
	static atQueue<RageSecPluginGameReactionObject, RAGE_SEC_MAX_NUMBER_REACTION_QUEUE_EVENTS> * GetRageSecGameReactionObjectQueue(){ return &sm_reactionQueue;};
	static atQueue<RageSecPluginGameReactionObject, RAGE_SEC_MAX_NUMBER_REACTION_QUEUE_EVENTS> * GetRageSecGameReactionPeerReportQueue(){ return &sm_queuedPeerReports;};
	static sysObfuscated<unsigned int> * GetRageSecGameReactionBailCounter() { return &sm_bailCounter;};
private:

	// This queue is responsible for holding the reactions that we get from our various plugins
	// why queue them up? Becuase we have no access to game funcitons, like any of the Network class
	// The expectation is that this will eventually be queued / depleted 
	static atQueue<RageSecPluginGameReactionObject, RAGE_SEC_MAX_NUMBER_REACTION_QUEUE_EVENTS> sm_reactionQueue;
	// Keeping a queue of the peer reports so we know when / how to broadcast them
	static atQueue<RageSecPluginGameReactionObject, RAGE_SEC_MAX_NUMBER_REACTION_QUEUE_EVENTS> sm_queuedPeerReports;
	// Keeping a static counter for when to bail out.
	static sysObfuscated<unsigned int> sm_bailCounter;
};

class rageSecGamePluginBonder
{
public:
	rageSecGamePluginBonder()
	{
#if RSG_PC
		m_lastCheckTime = 0;
#endif //RSG_PC

		m_nextCheckTime = fwRandom::GetRandomNumberInRange(0, RAGE_SEC_GAME_PLUGIN_BONDER_MAX_TIME_LAST_CHECK);
		m_bonderId = fwRandom::GetRandomNumber();
		rageSecGameDebugf3("Bonder [0x%08X] Generated", m_bonderId);

	}
	~rageSecGamePluginBonder()
	{
		rageSecGameDebugf3("Bonder [0x%08X] Deleted", m_bonderId);
	}
	bool AddReference(u32* referenceToAdd)
	{
		sysObfuscated<u32*> newReference(referenceToAdd);
		m_pluginUpdateReferences.PushAndGrow(newReference);

#if RSG_ORBIS
		rageSecGameDebugf3("Bonder [0x%08X] Added Reference [0x%" I64FMT "x] with Value [0x%08X]", m_bonderId, (unsigned long)referenceToAdd, *referenceToAdd);
#else
		rageSecGameDebugf3("Bonder [0x%08X] Added Reference [0x%" I64FMT "x] with Value [0x%08X]", m_bonderId, referenceToAdd, *referenceToAdd);
#endif
		
		return true;
	}

	bool ClearReferences()
	{
		rageSecGameDebugf3("Bonder [0x%08X] Clearing References", m_bonderId);
		m_pluginUpdateReferences.clear();
		return true;
	}

	bool UpdateBonder();


	unsigned int GetId() { return m_bonderId;}
private:
	atArray<u32*>	m_pluginUpdateReferences;
#if RSG_PC
	unsigned int	m_lastCheckTime;
#endif //RSG_PC
	unsigned int	m_nextCheckTime;
	unsigned int	m_bonderId;
};

class rageSecGamePluginBonderManager
{
public:
	static void AddBonder(rageSecGamePluginBonder *b){sm_bonders.PushAndGrow(b);}
	static void ClearBonders() {sm_bonders.Reset();}
	static bool NeedsRebalancing() {return rageSecPluginManager::NeedsRebalancing();}
	static void RebalanceBonders();

private:
	static atArray<rageSecGamePluginBonder*> sm_bonders;
};

class rageSecReportCache
{
public :
	static rageSecReportCache* GetInstance();
	rageSecReportCache();
	~rageSecReportCache();
	enum eReportCacheBucket { RCB_TELEMETRY, RCB_GAMESERVER};
	static bool HasBeenReported(eReportCacheBucket bucket, u32 val) ;
	static void AddToReported(eReportCacheBucket bucket, u32 val);
	static void Clear();
private:
	static atMap<eReportCacheBucket,  atMap<u32, bool>*> sm_buckets;
	static rageSecReportCache* sm_Instance;
};

#if RSG_ORBIS
void RageSecPopReaction();
#else
extern inline void RageSecPopReaction();
#endif

#if RSG_PC
HMODULE GetCurrentModule();
#endif

__forceinline void RageSecInduceGpuCrash()
{
	GBuffer::IncrementAttached(sysTimer::GetSystemMsTime());
}

__forceinline void RageSecInduceStreamerCrash()
{
	pgStreamer::sm_tamperCrash.Set(1);
}

#if RAGE_SEC_ENABLE_REBALANCING
#define RAGE_SEC_GAME_PLUGIN_BONDER_CHECK_REBALANCE if(rageSecGamePluginBonderManager::NeedsRebalancing()) { rageSecGamePluginBonderManager::RebalanceBonders();};
#define RAGE_SEC_GAME_PLUGIN_BONDER_FORCE_REBALANCE rageSecGamePluginBonderManager::RebalanceBonders();
#else
#define RAGE_SEC_GAME_PLUGIN_BONDER_CHECK_REBALANCE 
#define RAGE_SEC_GAME_PLUGIN_BONDER_FORCE_REBALANCE 
#endif

#define RAGE_SEC_GAME_PLUGIN_BONDER_ADD rageSecGamePluginBonderManager::AddBonder(this);
#define RAGE_SEC_GAME_PLUGIN_BONDER_UPDATE rageSecGamePluginBonder::UpdateBonder();
#if RAGE_SEC_INLINE_POP_REACTION_QUEUE
#if RSG_PC																															
#define RAGE_SEC_POP_REACTION  atQueue<RageSecPluginGameReactionObject, RAGE_SEC_MAX_NUMBER_REACTION_QUEUE_EVENTS> * reactionQueue =\
    rageSecGamePluginManager::GetRageSecGameReactionObjectQueue();																	\
	sysObfuscated<unsigned int> * bailCounter = rageSecGamePluginManager::GetRageSecGameReactionBailCounter();						\
	if(!reactionQueue->IsEmpty())																									\
	{																																\
		rageSecGameDebugf3("Found reaction object to process");																		\
		RageSecPluginGameReactionObject obj = reactionQueue->Pop();																	\
		switch(obj.type)																											\
		{																															\
																																	\
		case REACT_TELEMETRY:																										\
			{																														\
				rageSecGameDebugf3("TELEM: 0x%08X, 0x%08X, 0x%08X, 0x%08X",															\
                    obj.id, obj.version.Get(),obj.address.Get(),obj.data.Get());													\
				CNetworkTelemetry::AppendInfoMetric(																				\
                    obj.version.Get(),obj.address.Get(),obj.data.Get());															\
				break;																												\
			}																														\
		case REACT_P2P_REPORT:																										\
			{																														\
				if(netInterface::GetNumPhysicalPlayers() <= 1)																		\
				{																													\
					rageSecGameDebugf3("P2P QUEUED - Not enough players found to report: 0x%08X, 0x%08X, 0x%08X, 0x%08X",			\
                        obj.id, obj.version.Get(),obj.address.Get(),obj.data.Get());												\
					rageSecGamePluginManager::GetRageSecGameReactionPeerReportQueue()->Push(obj);									\
				}																													\
				else																												\
				{																													\
					rageSecGameDebugf3("P2P: 0x%08X, 0x%08X, 0x%08X, 0x%08X",														\
                        obj.id, obj.version.Get(),obj.address.Get(),obj.data.Get());												\
					CNetworkCodeCRCFailedEvent::Trigger(obj.version.Get(),obj.address.Get(),obj.data.Get());						\
				}																													\
				break;																												\
			}																														\
		case REACT_DESTROY_MATCHMAKING:																								\
			{																														\
				rageSecGameDebugf3("MATCHMAKEBUST: 0x%08X", obj.id);																\
				CNetwork::GetAssetVerifier().ClobberTamperCrcAndRefresh();															\
				break;																												\
			}																														\
		case REACT_SET_VIDEO_MODDED_CONTENT:																						\
			{																														\
				rageSecGameDebugf3("VIDEO: 0x%08X", obj.id);																		\
				CReplayMgr::SetWasModifiedContent();																				\
				break;																												\
			}																														\
		case REACT_KICK_RANDOMLY:																									\
			{																														\
				rageSecGameDebugf1("PRNG-KICK: 0x%08X", obj.id);																	\
				if(bailCounter->Get() == 0)																							\
					bailCounter->Set(fwRandom::GetRandomNumberInRange(KICK_MIN_DELAY,KICK_MAX_DELAY));								\
				break;																												\
			}																														\
		case REACT_GAMESERVER:																										\
			{                                                                                                                       \
				rageSecGameDebugf3("GSERVER: 0x%08X, 0x%08X", obj.id, obj.data.Get());												\
				NetShopTransactionId transactionId = NET_SHOP_INVALID_TRANS_ID;														\
				NETWORK_SHOPPING_MGR.BeginService(transactionId,																	\
					0xbc537e0d, 0xAE04310C,	obj.data.Get(),	0xFCFA31AD, 0, CATALOG_ITEM_FLAG_BANK_ONLY);							\
				NETWORK_SHOPPING_MGR.StartCheckout(transactionId);																	\
					break;																											\
			}																														\
		case REACT_INVALID:																											\
		default:																													\
			{																														\
				rageSecGameDebugf3("INVALID: 0x%08X", obj.id);																		\
				break;																												\
			}																														\
		}																															\
	}																																\
																																	\
	if(netInterface::GetNumPhysicalPlayers() > 1 && !rageSecGamePluginManager::GetRageSecGameReactionPeerReportQueue()->IsEmpty())	\
	{																																\
		RageSecPluginGameReactionObject obj = rageSecGamePluginManager::GetRageSecGameReactionPeerReportQueue()->Pop();				\
		rageSecGameDebugf3("P2P SENT: 0x%08X, 0x%08X, 0x%08X, 0x%08X",																\
            obj.id, obj.version.Get(),obj.address.Get(),obj.data.Get());															\
		CNetworkCodeCRCFailedEvent::Trigger(obj.version.Get(),obj.address.Get(),obj.data.Get());									\
	}																																\
																																	\
	if(bailCounter->Get() > 0)																										\
	{																																\
		bailCounter->Set(bailCounter->Get()-1);																						\
		if(bailCounter->Get() == 0)																									\
		{																															\
			CNetwork::GetNetworkSession().SetSessionStateToBail(BAIL_NETWORK_ERROR);												\
		}																															\
	}																																\
																																	\
	if(!rageSecEngine::GetRageLandReactions()->IsEmpty())																			\
	{																																\
		RageSecPluginGameReactionObject obj = rageSecEngine::GetRageLandReactions()->Pop();											\
		reactionQueue->Push(obj);																									\
	}                                                                                                                             

#else
#define RAGE_SEC_POP_REACTION  atQueue<RageSecPluginGameReactionObject, RAGE_SEC_MAX_NUMBER_REACTION_QUEUE_EVENTS> * reactionQueue =\
    rageSecGamePluginManager::GetRageSecGameReactionObjectQueue();																	\
	sysObfuscated<unsigned int> * bailCounter = rageSecGamePluginManager::GetRageSecGameReactionBailCounter();						\
	if(!reactionQueue->IsEmpty())																									\
	{																																\
		rageSecGameDebugf3("Found reaction object to process");																		\
		RageSecPluginGameReactionObject obj = reactionQueue->Pop();																	\
		switch(obj.type)																											\
		{																															\
																																	\
		case REACT_TELEMETRY:																										\
			{																														\
				rageSecGameDebugf3("TELEM: 0x%08X, 0x%08X, 0x%08X, 0x%08X",															\
                    obj.id, obj.version.Get(),obj.address.Get(),obj.data.Get());													\
				CNetworkTelemetry::AppendInfoMetric(																				\
                    obj.version.Get(),obj.address.Get(),obj.data.Get());															\
				break;																												\
			}																														\
		case REACT_P2P_REPORT:																										\
			{																														\
				if(netInterface::GetNumPhysicalPlayers() <= 1)																		\
				{																													\
					rageSecGameDebugf3("P2P QUEUED - Not enough players found to report: 0x%08X, 0x%08X, 0x%08X, 0x%08X",			\
                        obj.id, obj.version.Get(),obj.address.Get(),obj.data.Get());												\
					rageSecGamePluginManager::GetRageSecGameReactionPeerReportQueue()->Push(obj);									\
				}																													\
				else																												\
				{																													\
					rageSecGameDebugf3("P2P: 0x%08X, 0x%08X, 0x%08X, 0x%08X",														\
                        obj.id, obj.version.Get(),obj.address.Get(),obj.data.Get());												\
					CNetworkCodeCRCFailedEvent::Trigger(obj.version.Get(),obj.address.Get(),obj.data.Get());						\
				}																													\
				break;																												\
			}																														\
		case REACT_DESTROY_MATCHMAKING:																								\
			{																														\
				rageSecGameDebugf3("MATCHMAKEBUST: 0x%08X", obj.id);																\
				CNetwork::GetAssetVerifier().ClobberTamperCrcAndRefresh();															\
				break;																												\
			}																														\
		case REACT_SET_VIDEO_MODDED_CONTENT:																						\
			{																														\
				rageSecGameDebugf3("VIDEO: 0x%08X", obj.id);																		\
				CReplayMgr::SetWasModifiedContent();																				\
				break;																												\
			}																														\
		case REACT_KICK_RANDOMLY:																									\
			{																														\
				rageSecGameDebugf1("PRNG-KICK: 0x%08X", obj.id);																	\
				if(bailCounter->Get() == 0)																							\
					bailCounter->Set(fwRandom::GetRandomNumberInRange(KICK_MIN_DELAY,KICK_MAX_DELAY));								\
				break;																												\
			}																														\
		case REACT_GAMESERVER:																										\
			{                                                                                                                       \
					break;																											\
			}																														\
		case REACT_INVALID:																											\
		default:																													\
			{																														\
				rageSecGameDebugf3("INVALID: 0x%08X", obj.id);																		\
				break;																												\
			}																														\
		}																															\
	}																																\
																																	\
	if(netInterface::GetNumPhysicalPlayers() > 1 && !rageSecGamePluginManager::GetRageSecGameReactionPeerReportQueue()->IsEmpty())	\
	{																																\
		RageSecPluginGameReactionObject obj = rageSecGamePluginManager::GetRageSecGameReactionPeerReportQueue()->Pop();				\
		rageSecGameDebugf3("P2P SENT: 0x%08X, 0x%08X, 0x%08X, 0x%08X",																\
            obj.id, obj.version.Get(),obj.address.Get(),obj.data.Get());															\
		CNetworkCodeCRCFailedEvent::Trigger(obj.version.Get(),obj.address.Get(),obj.data.Get());									\
	}																																\
																																	\
	if(bailCounter->Get() > 0)																										\
	{																																\
		bailCounter->Set(bailCounter->Get()-1);																						\
		if(bailCounter->Get() == 0)																									\
		{																															\
			CNetwork::GetNetworkSession().SetSessionStateToBail(BAIL_NETWORK_ERROR);												\
		}																															\
	}																																\
																																	\
	if(!rageSecEngine::GetRageLandReactions()->IsEmpty())																			\
	{																																\
		RageSecPluginGameReactionObject obj = rageSecEngine::GetRageLandReactions()->Pop();											\
		reactionQueue->Push(obj);																									\
	}   
#endif // RSG_PC
#else
#define RAGE_SEC_POP_REACTION  RageSecPopReaction();
#endif // RAGE_SEC_INLINE_POP_REACTION_QUEUE
#else
#define RAGE_SEC_GAME_PLUGIN_BONDER_CHECK_REBALANCE 
#define RAGE_SEC_GAME_PLUGIN_BONDER_FORCE_REBALANCE
#define RAGE_SEC_GAME_PLUGIN_BONDER_ADD 
#define RAGE_SEC_GAME_PLUGIN_BONDER_UPDATE 
#define RAGE_SEC_POP_REACTION 

#endif // USE_RAGE_SEC
#endif	// SECURITY_RAGESECGAMEINTERFACE_H
