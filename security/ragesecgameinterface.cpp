#include "security/ragesecgameinterface.h"

#include "Network/Sessions/NetworkSession.h"
#include "security/ragesecengine.h"
#include "security/ragesecwinapi.h"
#include "security/vftassert/vftassert.h"
#include "atl/array.h"
#include "streaming/streaminginfo.h"

#if USE_RAGESEC

#if RAGE_SEC_TASK_BASIC_DEBUGGER_CHECK
#include "security/plugins/debuggercheckplugin.h"
#endif
#if RAGE_SEC_TASK_VIDEO_MODIFICATION_CHECK
#include "security/plugins/videomodificationplugin.h"
#endif

#if RAGE_SEC_TASK_RTMA
#include "security/plugins/rtmaplugin.h"
#endif

#if RAGE_SEC_TASK_NETWORK_MONITOR_CHECK
#include "security/plugins/networkmonitorplugin.h"
#endif

#if RAGE_SEC_TASK_SCRIPTHOOK
#include "security/plugins/scripthook.h"
#endif 

#if RAGE_SEC_TASK_REVOLVING_CHECKER
#include "security/plugins/revolvingcheckerplugin.h"
#endif 

#if RAGE_SEC_TASK_VEH_DEBUGGER
#include "security/plugins/vehdebuggerplugin.h"
#endif 

#if RAGE_SEC_TASK_LINK_DATA_REPORTER
#include "security/plugins/linkdatareporterplugin.h"
#endif

#if RAGE_SEC_TASK_API_CHECK
#include "security/plugins/apicheckplugin.h"
#endif

#if RAGE_SEC_TASK_TUNABLES_VERIFIER
#include "security/plugins/tunablesverifier.h"
#endif

#if RAGE_SEC_TASK_CLOCK_GUARD
#include "security/plugins/clockguard.h"
#endif

#if RAGE_SEC_TASK_EC
#include "security/plugins/ecplugin.h"
#endif

#if RAGE_SEC_TASK_CE_DETECTION
#include "security/plugins/cedetectionplugin.h"
#endif

#if RAGE_SEC_TASK_DLLNAMECHECK
#include "security/plugins/dllnamecheckplugin.h"
#endif

#if RAGE_SEC_TASK_PLAYERNAME_MONITOR_CHECK
#include "security/plugins/playernamemonitorplugin.h"
#endif

#if RAGE_SEC_TASK_SCRIPT_VARIABLE_VERIFY
#include "security/plugins/scriptvariableverifyplugin.h"
#endif

PARAM(disablebonder, "Disables the crashing that occurs when a plugin's bonder indicates its threshold has been crossed");
RAGE_DEFINE_CHANNEL(rageSecGame)
atArray<rageSecGamePluginBonder*> rageSecGamePluginBonderManager::sm_bonders;
atQueue<RageSecPluginGameReactionObject, RAGE_SEC_MAX_NUMBER_REACTION_QUEUE_EVENTS> rageSecGamePluginManager::sm_reactionQueue;
atQueue<RageSecPluginGameReactionObject, RAGE_SEC_MAX_NUMBER_REACTION_QUEUE_EVENTS> rageSecGamePluginManager::sm_queuedPeerReports;
sysObfuscated<unsigned int> rageSecGamePluginManager::sm_bailCounter;
atMap<rageSecReportCache::eReportCacheBucket,  atMap<u32, bool>*> rageSecReportCache::sm_buckets;
rageSecReportCache* rageSecReportCache::sm_Instance;

void rageSecGamePluginManager::Init(u32 initializationMode)
{
	
	//@@: location RAGESECGAMEPLUGINMANAGER_INIT_ENTRY
	if(initializationMode != INIT_CORE)
		return;

	//@@: range RAGESECGAMEPLUGINMANAGER_INIT {
	bool checkReturn = true;
#if RAGE_SEC_TASK_VIDEO_MODIFICATION_CHECK
	checkReturn &= VideoModificationPlugin_Init();
#endif

#if RAGE_SEC_TASK_BASIC_DEBUGGER_CHECK
	checkReturn &= DebuggerCheckPlugin_Init();
#endif

#if RAGE_SEC_TASK_RTMA
	checkReturn &=	RtmaPlugin_Init();
#endif
	
#if RAGE_SEC_TASK_NETWORK_MONITOR_CHECK
	checkReturn &= NetworkMonitorPlugin_Init();
#endif

#if RAGE_SEC_TASK_SCRIPTHOOK
	checkReturn &= ScriptHookPlugin_Init();
#endif

#if RAGE_SEC_TASK_REVOLVING_CHECKER
	checkReturn &= RevolvingCheckerPlugin_Init();
#endif

#if RAGE_SEC_TASK_VEH_DEBUGGER
	checkReturn &= VehDebuggerPlugin_Init();
#endif 

#if RAGE_SEC_TASK_LINK_DATA_REPORTER
	checkReturn &= LinkDataReporterPlugin_Init();
#endif

#if RAGE_SEC_TASK_API_CHECK
	checkReturn &= ApiCheckPlugin_Init();
#endif

#if RAGE_SEC_TASK_TUNABLES_VERIFIER
	checkReturn &= TunablesVerifierPlugin_Init();
#endif

#if RAGE_SEC_TASK_CLOCK_GUARD
	checkReturn &= ClockGuardPlugin_Init();
#endif

#if RAGE_SEC_TASK_EC
	checkReturn &= EcPlugin_Init();
#endif

#if RAGE_SEC_TASK_CE_DETECTION
	checkReturn &= CEDetectionPlugin_Init();
#endif

#if RAGE_SEC_TASK_DLLNAMECHECK
	checkReturn &= DllNamePlugin_Init();
#endif

#if RAGE_SEC_TASK_PLAYERNAME_MONITOR_CHECK
	checkReturn &= PlayerNameMonitorPlugin_Init();
#endif

#if RAGE_SEC_TASK_SCRIPT_VARIABLE_VERIFY
	checkReturn &= ScriptVariableVerifyPlugin_Init();
#endif

	RAGE_SEC_GAME_PLUGIN_BONDER_FORCE_REBALANCE
	//@@: } RAGESECGAMEPLUGINMANAGER_INIT

	//@@: location RAGESECGAMEPLUGINMANAGER_INIT_EXIT
	if(!checkReturn)
	{
		// Indicate that there's been a failure, and that no re-balancing is necessary.
		rageSecPluginManager::SetRebalance(false);
	}	
}

#if RSG_PC
HMODULE GetCurrentModule()
{ // NB: XP+ solution!
	HMODULE hModule = NULL;
	GetModuleHandleEx(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
		(LPCTSTR)GetCurrentModule,
		&hModule);

	return hModule;
}
#endif //RSG_PC


#if __BANK
static int sm_BankReactionType = 0;
// This 7 is derived from the number of elements inside of RageSecPluginGameReactionType
// I could have an ending element to indicate what the size is, but since this is
//bank release, and because then that assigns a constant number to an enum, I'm just
// slamming this in yolo style
static const int sm_BankReactionNumItems = 8;
static const char* sm_BankReactionTypeNames[sm_BankReactionNumItems] = {				
	"REACT_TELEMETRY",
	"REACT_P2P_REPORT",
	"REACT_DESTROY_MATCHMAKING",
	"REACT_KICK_RANDOMLY",		
	"REACT_CRASH",			
	"REACT_SET_VIDEO_MODDED_CONTENT",
	"REACT_GPU_CRASH",	
	"REACT_GAMESERVER"
};
static u32 sm_BankReactionTypeValues[sm_BankReactionNumItems] = {
	0xFC5F8D39,
	0xC34731F6,
	0xC85BE919, 
	0xDC294D77,
	0xA53765E2,
	0xCA189698,
	0xE4D062B8,
	0x4F7147A9 
};
static char sm_BankVersion[64];
static char sm_BankAddress[64];
static char sm_BankCrc[64];

static int sm_BankEcPluginSku;
static int sm_BankEcPluginPageLow;
static int sm_BankEcPluginPageHigh;
static int sm_BankEcPluginActionFlags;
static int sm_BankEcPluginVersionNumber;
static char sm_BankEcPluginHash[1024];

static void Bank_ReactionEvent()
{
	RageSecPluginGameReactionObject obj;
	obj.type = static_cast<RageSecPluginGameReactionType>(sm_BankReactionTypeValues[sm_BankReactionType]);
	obj.version.Set(atoi(sm_BankVersion));
	obj.address.Set(atoi(sm_BankAddress));
	obj.data.Set(atoi(sm_BankCrc));
	atQueue<RageSecPluginGameReactionObject, RAGE_SEC_MAX_NUMBER_REACTION_QUEUE_EVENTS> * reactionQueue = rageSecGamePluginManager::GetRageSecGameReactionObjectQueue();
	reactionQueue->Push(obj);
}

static void Bank_StreamerCrash()
{
	pgStreamer::sm_tamperCrash.Set(1);
}

static void Bank_GPUCrash()
{
	GBuffer::IncrementAttached(sysTimer::GetSystemMsTime());
}

static void Bank_TestIterating()
{
	static atArray<sysObfuscated<u32>> sample;

	mthRandom x(sysTimer::GetSystemMsTime());
	sample.PushAndGrow(sysObfuscated<u32>(x.GetInt()));
	sample.PushAndGrow(sysObfuscated<u32>(x.GetInt()));
	sample.PushAndGrow(sysObfuscated<u32>(x.GetInt()));
	sample.PushAndGrow(sysObfuscated<u32>(x.GetInt()));
	sample.PushAndGrow(sysObfuscated<u32>(x.GetInt()));

	for(int i = 0; i < sample.GetCount(); i++)
	{
		rageSecGameDisplayf("%d", sample[i].Get());
	}
}

static void Bank_TestRageSecWinApi()
{
#if RSG_PC
	// KERNEL32 test
	if ((u64)GetProcAddress(GetModuleHandle("kernel32.dll"), "GetModuleFileNameA") !=
		(u64)Kernel32::GetModuleFileNameA)
	{
		rageSecGameDisplayf("GetModuleFileNameA() failed\n");
	}
	else
	{
		rageSecGameDisplayf("GetModuleFileNameA() success\n");
	}

	// NTDLL test
	if ((u64)GetProcAddress(GetModuleHandle("ntdll.dll"), "NtQueryVirtualMemory") !=
		(u64)NtDll::NtQueryVirtualMemory)
	{
		rageSecGameDisplayf("NtQueryVirtualMemory() failed\n");
	}
	else
	{
		rageSecGameDisplayf("NtQueryVirtualMemory() success\n");
	}

	// PSAPI test
	if ((u64)GetProcAddress(GetModuleHandle("psapi.dll"), "EnumProcessModules") !=
		(u64)Kernel32::EnumProcessModules)
	{
		rageSecGameDisplayf("EnumProcessModules() failed\n");
	}
	else
	{
		rageSecGameDisplayf("EnumProcessModules() success\n");
	}
#endif
}

static void Bank_VftAssertThread()
{
#if RSG_PC
	RAGE_SEC_ASSERT_VFT(scrThread);
	RAGE_SEC_ASSERT_VFT(GtaThread);
#endif
}

static void Bank_EcPluginAddCheck()
{
#if RSG_PC
	bool inserted = EcPlugin_InsertMemoryCheck(sm_BankEcPluginVersionNumber, (u8)sm_BankEcPluginSku, sm_BankEcPluginPageLow, sm_BankEcPluginPageHigh,
		sm_BankEcPluginActionFlags, sm_BankEcPluginHash);
	if (!inserted)
	{
		rageSecGameDisplayf("Failed to insert memory check!");
	}
#endif //RSG_PC
};

static void Bank_EcPluginProcessAllChecks()
{
#if RSG_PC
	bool result = EcPlugin_Work();
	if (result)
	{
		rageSecGameDisplayf("The executable checker plugin made a detection!");
	}
#endif //RSG_PC
}

static void Bank_DbgPluginTestAllChecks()
{
#if RAGE_SEC_TASK_BASIC_DEBUGGER_CHECK
	DebuggerCheckPlugin_Test();
#endif //RAGE_SEC_TASK_BASIC_DEBUGGER_CHECK
}

#endif //__BANK

void rageSecGamePluginManager::InitWidgets()
{
#if __BANK
	bkBank	&bank = BANKMGR.CreateBank("RageSec");
	
	bkGroup *rxnGrp = bank.AddGroup("Reaction Group", true, "Testing out the individual reactions.");
	rxnGrp->AddCombo("Reaction Type", &sm_BankReactionType, sm_BankReactionNumItems, sm_BankReactionTypeNames);
	rxnGrp->AddText("Version", &sm_BankVersion[0], sizeof(sm_BankVersion));
	rxnGrp->AddText("Address", &sm_BankAddress[0], sizeof(sm_BankAddress));
	rxnGrp->AddText("CRC    ", &sm_BankCrc[0], sizeof(sm_BankCrc));
	rxnGrp->AddButton("Test Array Creation", Bank_TestIterating);
	rxnGrp->AddButton("Fire Reaction", Bank_ReactionEvent);
	rxnGrp->AddButton("GPU Crash", Bank_GPUCrash);
	rxnGrp->AddButton("Streamer Crash", Bank_StreamerCrash);
	
	bkGroup *papiGrp = bank.AddGroup("PAPI Group", true, "Various tests for PAPI integration.");
	papiGrp->AddButton("Test RageSec WinAPI", Bank_TestRageSecWinApi);

	bkGroup *vftGrp = bank.AddGroup("VFT Assert Group", true, "Tests VFT Assert on various classes.");
	vftGrp->AddTitle("Note: This will fail if signature functions are not post-patched.");
	vftGrp->AddButton("scrThread/GtaThread", Bank_VftAssertThread);

	// RageSec Plugin Widgets
	bkGroup* pluginsGrp = bank.AddGroup("Security Plugins");	

	// Executable Checker Widget
	bkGroup* ecPluginGrp = pluginsGrp->AddGroup("EC Plugin", false);
	ecPluginGrp->AddText("Byte String", &sm_BankEcPluginHash[0], sizeof(sm_BankEcPluginHash));
	ecPluginGrp->AddText("SKU", &sm_BankEcPluginSku);
	ecPluginGrp->AddText("Page Low", &sm_BankEcPluginPageLow);
	ecPluginGrp->AddText("Page High", &sm_BankEcPluginPageHigh);
	ecPluginGrp->AddText("Action Flags", &sm_BankEcPluginActionFlags);
	ecPluginGrp->AddText("Version Number", &sm_BankEcPluginVersionNumber);
	ecPluginGrp->AddButton("Add Check", Bank_EcPluginAddCheck);
	ecPluginGrp->AddButton("Process All Checks", Bank_EcPluginProcessAllChecks);

	// Anti Debugging Testing Widget
	bkGroup* dbgPluginGrp = pluginsGrp->AddGroup("Debugger Checks", false);
	dbgPluginGrp->AddButton("Perform all checks", Bank_DbgPluginTestAllChecks);

	// Add more as follows

#endif
}

bool rageSecGamePluginBonder::UpdateBonder()
{
#if RSG_PC
	unsigned int currTimer = sysTimer::GetSystemMsTime();
#if RAGE_SEC_GAME_PLUGIN_BONDER_DEBUGGING
	unsigned int refreshDeltaTime = m_nextCheckTime;
#else
	unsigned int refreshDeltaTime = currTimer - m_lastCheckTime;
#endif
	if( refreshDeltaTime >= m_nextCheckTime)
	{
		//@@: location RAGESECGAMEPLUGINBONDER_UPDATEBONDER_SET_NEXT_CHECK_TIME
		rageSecGameDebugf3("Bonder [0x%08X] Performing Update Check", m_bonderId);
		m_nextCheckTime = currTimer + fwRandom::GetRandomNumberInRange(0, RAGE_SEC_GAME_PLUGIN_BONDER_MAX_TIME_LAST_CHECK);
		//@@: range RAGESECGAMEPLUGINBONDER_UPDATEBONDER {
		if(m_pluginUpdateReferences.GetCount() == 0)
		{
			rageSecGameFatalAssertf(true, "No ragesec plugin update references found; this is unexpected.");
			if(!PARAM_disablebonder.Get())
				pgStreamer::sm_tamperCrash.Set(1);
		}

		for(int i = 0; i < m_pluginUpdateReferences.GetCount(); i++)
		{

			u32 *updateTime = m_pluginUpdateReferences[i];
#if RSG_ORBIS
			rageSecGameDebugf3("Bonder [0x%08X] Checking Reference [0x%" I64FMT "x] with Value [0x%08X]", m_bonderId, (unsigned long)updateTime, *updateTime);
#else
			rageSecGameDebugf3("Bonder [0x%08X] Checking Reference [0x%" I64FMT "x] with Value [0x%08X]", m_bonderId, updateTime, *updateTime);
#endif
			unsigned int deltaTime =  currTimer - *updateTime;
			if(deltaTime > RAGE_SEC_GAME_PLUGIN_BONDER_MAX_TIME_LAST_PLUGIN_REFRESH)
			{
				rageSecGameFatalAssertf(true, "A RageSec Plugin has failed to meet it's timing standards.");
				if(!PARAM_disablebonder.Get())
					pgStreamer::sm_tamperCrash.Set(1);
				i = m_pluginUpdateReferences.GetCount();
			}
		}
		//@@: } RAGESECGAMEPLUGINBONDER_UPDATEBONDER

		//@@: location RAGESECGAMEPLUGINBONDER_UPDATEBONDER_SET_LAST_CHECK_TIME
		m_lastCheckTime = currTimer;
		
		rageSecGameDebugf3("Bonder [0x%08X] Next Check in [%d] ms",m_bonderId, m_nextCheckTime);
		

	}
#endif
	return true;
}

void rageSecGamePluginBonderManager::RebalanceBonders()
{
	rageSecGameDebugf3("Rebalancing bonders - Entry");
	//@@: location RAGESECGAMEPLGUINBONDERMANAGER_REBALANCEBONDERS_ENTRY_GET_COUNT
	int bonderCount = sm_bonders.GetCount();

	// Clear the bonder references
	for(int i = 0; i < bonderCount; i++)
		sm_bonders[i]->ClearReferences();

	// Assign the references
	atArray<u32*> pluginUpdateReferences;
	//@@: location RAGESECGAMEPLGUINBONDERMANAGER_REBALANCEBONDERS_ENTRY
	rageSecPluginManager::GetPluginUpdateReferences(pluginUpdateReferences);
	size_t pluginUpdateReferencesCount = pluginUpdateReferences.GetCount();

	// If we have no plugins, we're not really 'balanced' yet, so
	// just mosey on as if we still need to
	//@@: range RAGESECGAMEPLGUINBONDERMANAGER_REBALANCEBONDERS_INNER {
	if(pluginUpdateReferencesCount == 0)
	{
		rageSecGameDebugf3("No references to rebalance. Returning");
		return;
	}
	// Now assign a bonder to each of our references
	rageSecGameDebugf3("Rebalancing bonders - Assignment");

	for(int i =0; i < pluginUpdateReferencesCount; i++)
	{
		unsigned int selectedBonder = fwRandom::GetRandomNumberInRange(0,bonderCount);
		u32 *ptr = pluginUpdateReferences[i];
#if RSG_ORBIS
		rageSecGameDebugf3("Assigning [0x%" I64FMT "x] with value [0x%08X] to Bonder ID: [0x%08X]", (unsigned long)ptr, *ptr, sm_bonders[selectedBonder]->GetId());
#else
		rageSecGameDebugf3("Assigning [0x%" I64FMT "x] with value [0x%08X] to Bonder ID: [0x%08X]", ptr, *ptr, sm_bonders[selectedBonder]->GetId());
#endif
		sm_bonders[selectedBonder]->AddReference(ptr);
	}
	//@@: } RAGESECGAMEPLGUINBONDERMANAGER_REBALANCEBONDERS_INNER


	//@@: location RAGESECGAMEPLGUINBONDERMANAGER_REBALANCEBONDERS_EXIT
	rageSecPluginManager::SetRebalance(false);
}

#if !RAGE_SEC_INLINE_POP_REACTION_QUEUE
/*
 *
 * PLEASE BE AWARE THAT ANY CHANGES IN THIS FUNCTION NEED TO BE REFLECTED TO THE *.H FILE
 * THIS IS BECAUSE THE __FORCEINLINE IS JUST A SUGGESTION TO THE COMPILER, AND THE ONLY WAY TO 
 * FORCE THAT BEHAVIOR IS THROUGH MURKY AND ANNOYING #DEFINE'S
 *
 *
 */
void RageSecPopReaction()
{
	atQueue<RageSecPluginGameReactionObject, RAGE_SEC_MAX_NUMBER_REACTION_QUEUE_EVENTS> * reactionQueue =
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue();																	
	sysObfuscated<unsigned int> * bailCounter = rageSecGamePluginManager::GetRageSecGameReactionBailCounter();										
	if(!reactionQueue->IsEmpty())																									
	{																																
		rageSecGameDebugf3("Found reaction object to process");																		
		RageSecPluginGameReactionObject obj = reactionQueue->Pop();																	
		switch(obj.type)																											
		{																															

		case REACT_TELEMETRY:																										
			{																														
				rageSecGameDebugf3("TELEM: 0x%08X, 0x%08X, 0x%08X, 0x%08X",															
					obj.id, obj.version.Get(),obj.address.Get(),obj.data.Get());													
				CNetworkTelemetry::AppendInfoMetric(																				
					obj.version.Get(),obj.address.Get(),obj.data.Get());															
				break;																												
			}																														
		case REACT_P2P_REPORT:																										
			{																														
				if(netInterface::GetNumPhysicalPlayers() <= 1)																		
				{																													
					rageSecGameDebugf3("P2P QUEUED - Not enough players found to report: 0x%08X, 0x%08X, 0x%08X, 0x%08X",			
						obj.id, obj.version.Get(),obj.address.Get(),obj.data.Get());												
					rageSecGamePluginManager::GetRageSecGameReactionPeerReportQueue()->Push(obj);									
				}																													
				else																												
				{																													
					rageSecGameDebugf3("P2P: 0x%08X, 0x%08X, 0x%08X, 0x%08X",														
						obj.id, obj.version.Get(),obj.address.Get(),obj.data.Get());												
					CNetworkCodeCRCFailedEvent::Trigger(obj.version.Get(),obj.address.Get(),obj.data.Get());						
				}																													
				break;																												
			}																														
		case REACT_DESTROY_MATCHMAKING:																								
			{																														
				rageSecGameDebugf3("MATCHMAKEBUST: 0x%08X", obj.id);																
				CNetwork::GetAssetVerifier().ClobberTamperCrcAndRefresh();															
				break;																												
			}																																																												
		case REACT_SET_VIDEO_MODDED_CONTENT:																						
			{																														
				rageSecGameDebugf3("VIDEO: 0x%08X", obj.id);																		
				CReplayMgr::SetWasModifiedContent();																				
				break;																												
			}																														
		case REACT_KICK_RANDOMLY:																									
			{																														
				rageSecGameDebugf1("PRNG-KICK: 0x%08X", obj.id);																	
				if(bailCounter->Get() == 0)																								
					bailCounter->Set(fwRandom::GetRandomNumberInRange(KICK_MIN_DELAY,KICK_MAX_DELAY));
				break;																												
			}	
		case REACT_GAMESERVER:
			{
#if RSG_PC
				rageSecGameDebugf3("GSERVER: 0x%08X, 0x%08X", obj.id, obj.data.Get());
				// Create a new transaction...
				NetShopTransactionId transactionId = NET_SHOP_INVALID_TRANS_ID;

				// Send as a GameServer service. Earns do this so anyone sniffing may be thrown off.
				// SERVICE_BONUS == Detect

				NETWORK_SHOPPING_MGR.BeginService(transactionId, 
					0xbc537e0d, // NET_SHOP_TTYPE_SERVICE
					0xAE04310C, // SERVICE_BONUS
					obj.data.Get(), 
					0xFCFA31AD, // NET_SHOP_ACTION_BONUS
					0, CATALOG_ITEM_FLAG_BANK_ONLY);

				NETWORK_SHOPPING_MGR.StartCheckout(transactionId);
#endif
				break;
			}
		case REACT_INVALID:																											
		default:																													
			{																														
				rageSecGameDebugf3("INVALID: 0x%08X", obj.id);																		
				break;																												
			}																														
		}																															
	}																																

	if(netInterface::GetNumPhysicalPlayers() > 1 && !rageSecGamePluginManager::GetRageSecGameReactionPeerReportQueue()->IsEmpty())	
	{																																
		RageSecPluginGameReactionObject obj = rageSecGamePluginManager::GetRageSecGameReactionPeerReportQueue()->Pop();				
		rageSecGameDebugf3("P2P SENT: 0x%08X, 0x%08X, 0x%08X, 0x%08X",																
			obj.id, obj.version.Get(),obj.address.Get(),obj.data.Get());															
		CNetworkCodeCRCFailedEvent::Trigger(obj.version.Get(),obj.address.Get(),obj.data.Get());									
	}																																

	if(bailCounter->Get() > 0)
	{																																
		bailCounter->Set(bailCounter->Get()-1);
		if(bailCounter->Get() == 0)																										
		{																															
			CNetwork::GetNetworkSession().SetSessionStateToBail(BAIL_NETWORK_ERROR);												
		}																															
	}																																

	if(!rageSecEngine::GetRageLandReactions()->IsEmpty())																			
	{																																
		RageSecPluginGameReactionObject obj = rageSecEngine::GetRageLandReactions()->Pop();											
		reactionQueue->Push(obj);																									
	}            
}
#endif


rageSecReportCache* rageSecReportCache::GetInstance()
{
	if(!sm_Instance)
		sm_Instance = rage_new rageSecReportCache();
	return sm_Instance;
}

rageSecReportCache::rageSecReportCache()
{
	sm_buckets.Insert(RCB_TELEMETRY, rage_new atMap<u32, bool>());
	sm_buckets.Insert(RCB_GAMESERVER, rage_new atMap<u32, bool>());
}

rageSecReportCache::~rageSecReportCache()
{
	// Kill all the entries
	atMap<eReportCacheBucket, atMap<u32, bool>*>::Iterator entry = sm_buckets.CreateIterator();
	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		(*entry)->Kill();
	}

	// Kill the object itself
	sm_buckets.Kill();
}
bool rageSecReportCache::HasBeenReported(eReportCacheBucket bucket, u32 val) 
{ 
	return sm_buckets[bucket]->Access(val) == NULL ? false : true;
}
void rageSecReportCache::AddToReported(eReportCacheBucket bucket, u32 val) 
{
	sm_buckets[bucket]->Insert(val, true);
}
void rageSecReportCache::Clear() 
{ 
	atMap<eReportCacheBucket, atMap<u32, bool>*>::Iterator entry = sm_buckets.CreateIterator();
	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		(*entry)->Reset();
	}
}
#endif