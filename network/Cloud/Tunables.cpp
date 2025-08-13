
#include "Network/Cloud/Tunables.h"
#include "Network/Cloud/CloudManager.h"
#include "event/EventGroup.h"
#include "event/EventNetwork.h"
#include "Network/Live/NetworkTelemetry.h"
#include "script/script.h"
//Rage includes
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "data/aes.h"
#include "data/rson.h"
#include "file/asset.h"
#include "file/device.h"
#include "file/stream.h"
#include "net/tcp.h"
#include "paging/streamer.h"
#include "security/plugins/tunablesverifier.h"
#include "system/nelem.h"
#include "system/param.h"
#include "system/timer.h"

#include "fwnet/netchannel.h"
#include "fwnet/netcloudfiledownloadhelper.h"

NETWORK_OPTIMISATIONS();

XPARAM(nonetwork);
PARAM(tunableLoadLocal, "If present, loads the tunables from the local file");
PARAM(tunableLocalFile, "If present, specifies local tunable path");
PARAM(tunableCloudFile, "If present, specifies cloud tunable path");
PARAM(tunableMemberFile, "If present, specifies cloud member tunable path");
PARAM(tunableCloudNotEncrypted, "If present, game understands that tunable file is not encrypted on cloud");

// note: this is just the default poolsize, if 'poolSize' is specified in our tunables file it is used.
// otherwise this default here is used as the poolsize.
static const u32 kDEFAULT_POOL_SIZE = 2000;
static const u32 kMAX_COMBINED_NAME = 128;
static const u32 kNAME_MAX = 128;

static const char* gs_szTunablesFile = "common:/data/tune/tunables.json";
#if __BANK
static bkGroup* ms_pBankGroup = 0;
#endif

RAGE_DEFINE_SUBCHANNEL(net, tunable, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_tunable

using namespace rage;

static const char* gs_szTypeNames[CTunables::kTYPE_MAX] = 
{
	"kINT",		// kINT 
	"kFLOAT",	// kFLOAT 
	"kBOOL",	// kBOOL
};

static char gs_szCombinedName[kMAX_COMBINED_NAME];
#if USE_TUNABLES_MODIFICATION_DETECTION 
u32 CTunables::sm_TunableModificationDetectionTimerStart = 0;
u32 CTunables::sm_TunableModificationDetectionTimerEnd = 0;
#endif

u32 TunableHash(unsigned nContextHash, unsigned nTunableHash)
{
	// just add these - could swap to atDataHash(&nContextHash, sizeof(nContextHash), nTunableHash)
	return nContextHash + nTunableHash;
}

TunablesListener::TunablesListener(OUTPUT_ONLY(const char* szName))
{
#if !__NO_OUTPUT
	safecpy(m_Name, szName);
#endif

	if(Tunables::IsInstantiated())
	{
		gnetDebug1("TunablesListener :: Adding %s", m_Name);
		Tunables::GetInstance().AddListener(this);
	}
	else
		gnetAssertf(PARAM_nonetwork.Get(), "TunablesListener :: Cannot add %s - No Tunables Instance!", m_Name);
}

TunablesListener::~TunablesListener()
{
	if(Tunables::IsInstantiated())
	{
		gnetDebug1("TunablesListener :: Removing %s", m_Name);
		Tunables::GetInstance().RemoveListener(this);
	}
	else
		gnetAssertf(PARAM_nonetwork.Get(), "TunablesListener :: Cannot remove %s - No Tunables Instance!", m_Name);
}

CTunables::CTunables()
: m_TunableMap(atMapHashFn<u32>(), atMapEquals<u32>(), m_TunableMemoryPool.m_Functor)
, m_iVersion(0)
, m_iPoolsize(0)
, m_bHasCloudTunables(false)
, m_bHasCompletedCloudRequest(false)
, m_LastTunableCheckTime(0)
, m_nCloudRequestID(INVALID_CLOUD_REQUEST_ID)
, m_nLocalCRC(0)
, m_nCloudCRC(0)
, m_tunablesRead(false)
#if !__FINAL 
, m_nCurrentCloudFileIndex(~0u)
#endif
{

}

CTunables::~CTunables()
{
	
}

bool CTunables::Init()
{
	bool bSuccess = true; 

	// create tunables pool with default pool size
	if(m_iPoolsize == 0)
	{
		m_iPoolsize = kDEFAULT_POOL_SIZE; 
		bSuccess &= EstablishPool();
		gnetAssertf(bSuccess, "Init :: Failed to establish tunables pool. Size: %d", m_iPoolsize);
	}

	// reset variables
	m_nCloudRequestID = INVALID_CLOUD_REQUEST_ID;

	// may be cached, so make this request after loading the local copy
	StartCloudRequest();

	return bSuccess;
}

void CTunables::Shutdown()
{
	m_bHasCloudTunables = false;
	m_TunablesToUpdate.Reset();
	m_TunableMap.Reset();
}

void CTunables::Update()
{
	//@@: range CTUNABLES_UPDATE {
	const unsigned nCurrentTime = sysTimer::GetSystemMsTime() | 0x01;
	if(nCurrentTime > (m_LastTunableCheckTime + TUNABLE_CHECK_INTERVAL))
	{
		u64 nNowPosix = rlGetPosixTime();

		// update all tunables
		int nTunablesToUpdate = m_TunablesToUpdate.GetCount();
		for(int i = 0; i < nTunablesToUpdate; i++)
			m_TunablesToUpdate[i]->Update(nNowPosix);

		m_LastTunableCheckTime = nCurrentTime;
	}
#if USE_TUNABLES_MODIFICATION_DETECTION 
	ModificationDetectionUpdate();
#endif
	//@@: } CTUNABLES_UPDATE
}

#if USE_TUNABLES_MODIFICATION_DETECTION 
void CTunables::ModificationDetectionUpdate()
{
	//@@: range CTUNABLES_MODIFICATIONDETECTIONUPDATE_ENTRY {
	// Only activate or do things if we're in multiplayer
	if(NetworkInterface::IsAnySessionActive())
	{
		// Check to see if we're due for a check
#if __BANK
		if(sysTimer::HasElapsedIntervalMs(sm_TunableModificationDetectionTimerStart, 1000))
#else
		if(sysTimer::HasElapsedIntervalMs(sm_TunableModificationDetectionTimerStart, sm_TunableModificationDetectionTimerEnd))
#endif
		{
			netFireAndForgetTask<TunablesModificationTask>* task;
			if(!netTask::Create(&task)	
				|| !task->Configure(&m_TunableMap, &m_TunableModificationDetectionTable)
				|| !netTask::Run(task))	
			{
				netTask::Destroy(task);
			}

			// Reset our timer to do the next check, in another sample between 1-10 seconds
			// Get the current time
			sm_TunableModificationDetectionTimerStart = sysTimer::GetSystemMsTime();

			// Generate the next desired ending time
#if __FINAL
			sm_TunableModificationDetectionTimerEnd = fwRandom::GetRandomNumberInRange(TUNABLES_MODIFICATION_DETECTION_TIMER_MIN, TUNABLES_MODIFICATION_DETECTION_TIMER_MAX);
#endif
		}
	}
	//@@: } CTUNABLES_MODIFICATIONDETECTIONUPDATE_ENTRY
}
#endif

void CTunables::AddListener(TunablesListener* pListener)
{
	m_TunableListeners.PushAndGrow(pListener);
}

bool CTunables::RemoveListener(TunablesListener* pListener)
{
	int nListeners = m_TunableListeners.GetCount(); 
	for(int i = 0; i < nListeners; i++)
	{
		if(m_TunableListeners[i] == pListener)
		{
			m_TunableListeners.Delete(i);
			return true;
		}
	}
	return false;
}

#if !__FINAL
bool CTunables::LoadLocalFile()
{
    if(!PARAM_tunableLoadLocal.Get())
        return false; 

	const char* szTunablesFile = NULL;
	if(!PARAM_tunableLocalFile.Get(szTunablesFile))
		szTunablesFile = gs_szTunablesFile;

	// load the disk copy of the tunables
	return LoadFile(szTunablesFile);
}

bool CTunables::LoadFile(const char* szFilePath)
{
	gnetDebug1("LoadFile :: Loading Local Tunables, File: %s", szFilePath);
	
	// open the tunables file
	fiStream* pStream = ASSET.Open(szFilePath, "");
    if(!pStream)
    {
        gnetError("LoadFile :: Failed to open %s", szFilePath);
        return false;
    }

	// allocate data to read the file
	unsigned nDataSize = pStream->Size();
	char* pData = rage_new char[nDataSize+1];
	pData[nDataSize] = '\0';

	// read out the contents
	pStream->Read(pData, nDataSize);

	// load tunables
	bool bSuccess = LoadFromJSON(pData, nDataSize, true, true, m_nLocalCRC NOTFINAL_ONLY(, false));
    gnetDebug1("LoadFile :: Loaded local tunables. Generated CRC [0x%08x]", m_nLocalCRC);
	gnetAssertf(bSuccess, "LoadFile :: Errors loading tunables file %s", szFilePath);
		
	// punt data
	delete[] pData; 

	// close file
	pStream->Close();

	// 
	return bSuccess;
}
#endif

bool CTunables::LoadFromJSON(const void* pData, unsigned nDataSize, bool bCreatePools, bool bInsertTunables, unsigned& nCRC NOTFINAL_ONLY(, bool bFromCloud))
{
#if !__NO_OUTPUT
	// log out the full tunables file
	gnetDebug1("LoadFromJSON :: Full Content:");
	diagLoggedPrintLn((const char*)pData, nDataSize);
#endif

	// validate data
	//@@: location CTUNABLES_LOADFROMJSON_VALIDATE_JSON
	bool bSuccess = RsonReader::ValidateJson(static_cast<const char*>(pData), nDataSize);
	if(!gnetVerifyf(bSuccess, "LoadFromJSON :: JSON data invalid!"))
	{
		OUTPUT_ONLY(WriteTunablesFile(pData, nDataSize));
		return false;
	}

	// initialise reader
	RsonReader tReader;
	bSuccess = tReader.Init(static_cast<const char*>(pData), 0, nDataSize);
	if(!gnetVerifyf(bSuccess, "LoadFromJSON :: Cannot initialise JSON reader!"))
		return false;

	// value struct
	sUnionValue tUnionValue; 

	// grab the version
	RsonReader tVersion;
	bSuccess = tReader.GetMember("version", &tVersion);
	if(bSuccess)
	{
		eUnionType kType = LoadValueFromJSON(&tVersion, &tUnionValue);
		if(gnetVerifyf(kType == kINT, "LoadFromJSON :: Version as type %s. Should be %s", (kType != kINVALID) ? gs_szTypeNames[kType] : "INVALID", gs_szTypeNames[kINT]))
		{
			gnetDebug1("LoadFromJSON :: Version found. Version is %d", tUnionValue.m_INT);
			m_iVersion = tUnionValue.m_INT;
		}
	}
	else
	{
		gnetAssertf(0, "LoadFromJSON :: No version data!");
		OUTPUT_ONLY(WriteTunablesFile(pData, nDataSize));
	}

	// don't load incompatible versions
	if(m_iVersion != TUNABLES_VERSION)
	{
		gnetDebug1("LoadFromJSON :: Incompatible version. Code: %d, Data: %d", TUNABLES_VERSION, m_iVersion);
		OUTPUT_ONLY(WriteTunablesFile(pData, nDataSize));
		return false; 
	}

	// get tunable format - assume FORMAT_STRING
	eFormat nFormat = FORMAT_STRING;
	RsonReader tFormat;
	bool bFormatFound = tReader.GetMember("format", &tFormat);
	if(bFormatFound)
	{
		eUnionType kType = LoadValueFromJSON(&tFormat, &tUnionValue);
		if(gnetVerifyf(kType == kINT, "LoadFromJSON :: Format as type %s. Should be %s", (kType != kINVALID) ? gs_szTypeNames[kType] : "INVALID", gs_szTypeNames[kINT]))
		{
			if(tUnionValue.m_INT > FORMAT_INVALID && tUnionValue.m_INT < FORMAT_MAX)
			{
				gnetDebug1("LoadFromJSON :: Format found. Format is %d", tUnionValue.m_INT);
				nFormat = static_cast<eFormat>(tUnionValue.m_INT);
			}
			else
			{
				gnetError("LoadFromJSON :: Invalid format: %d", tUnionValue.m_INT);
				OUTPUT_ONLY(WriteTunablesFile(pData, nDataSize));
			}
		}
	}

	// if this load is allowed to specify a pool size member
	if(bCreatePools)
	{
		// grab the version
		RsonReader tPoolSize;
		bSuccess = tReader.GetMember("tunablePool", &tPoolSize);

		// it is acceptable to have no pool size entry
		if(bSuccess)
		{
			eUnionType kType = LoadValueFromJSON(&tPoolSize, &tUnionValue);
			if(gnetVerifyf(kType == kINT, "Tunables :: Pool Size as type %s. Should be %s", gs_szTypeNames[kType], gs_szTypeNames[kINT]))
			{
				gnetDebug1("LoadFromJSON :: Pool Size found. Pool Size is %d", tUnionValue.m_INT);
				m_iPoolsize = tUnionValue.m_INT;
			}
		}
		else // otherwise, just use the default
			m_iPoolsize = kDEFAULT_POOL_SIZE;

		// establish pool
		bSuccess = EstablishPool();
		gnetAssertf(bSuccess, "LoadFromJSON :: Failed to establish tunables pool. Size: %d", m_iPoolsize);
	}

	// load parameters
	//@@: range CTUNABLES_LOADFROMJSON_LOAD_STUFF {
	LoadParameters(&tReader);

	// load content lists
	LoadContentLists(&tReader);

	// load memory checks
	//@@: location CTUNABLES_LOADFROMJSON_CALL_LOAD_MEMORY_CHECKS
	LoadMemoryChecks(&tReader NOTFINAL_ONLY(, bFromCloud));

	// load tunables
	//@@: location CTUNABLES_LOADFROMJSON_CALL_LOAD_TUNABLE
	LoadTunables(&tReader, bInsertTunables, nCRC, nFormat);
	//@@: } CTUNABLES_LOADFROMJSON_LOAD_STUFF

    // let interested parties know when the local tunables are loaded
	m_tunablesRead = true;

	int nListeners = m_TunableListeners.GetCount();
	gnetDebug1("LoadFromJSON :: Triggering OnTunablesRead for %d Listeners", nListeners);

    for(int i = 0; i < nListeners; i++)
        m_TunableListeners[i]->OnTunablesRead();

	// Now initialize the timer that will do our checks.
	// In the update loop, we'll worry about if they're online or not
	//@@: location CTUNABLES_LOADFROMJSON_GETSYSTEMMSTIME
#if USE_TUNABLES_MODIFICATION_DETECTION
	sm_TunableModificationDetectionTimerStart = sysTimer::GetSystemMsTime();
	sm_TunableModificationDetectionTimerEnd = fwRandom::GetRandomNumberInRange(TUNABLES_MODIFICATION_DETECTION_TIMER_MIN, TUNABLES_MODIFICATION_DETECTION_TIMER_MAX);
#endif
	// success
	return true; 
}

bool CTunables::LoadParameters(RsonReader* 
#if !__FINAL
							   pNode
#endif
							   )
{
#if !__FINAL
	// grab parameters
	RsonReader tParameters;
	bool bSuccess = pNode->GetMember("params", &tParameters);
	
	// it is acceptable to have no parameters entries
	if(!bSuccess)
		return true;

	// value struct
	sUnionValue tUnionValue; 

	// grab parameters
	RsonReader tParameter;
	bSuccess = tParameters.GetFirstMember(&tParameter);

	// it is acceptable to have no parameter entries
	if(!bSuccess)
		return true;

	do 
	{
		char szParamName[kNAME_MAX];

		// load parameter name entry
		RsonReader tParamName;
		bSuccess = tParameter.GetMember("name", &tParamName);
		if(bSuccess)
			tParamName.AsString(szParamName, kNAME_MAX);
		else // name entry required for a parameter
		{
			gnetAssertf(0, "LoadParameters :: Parameter has no name entry!");
			continue;
		}

		// ... and the value itself
		RsonReader tParamValue;
		bSuccess = tParameter.GetMember("value", &tParamValue);

		// only bool parameters are supported
		if(gnetVerifyf(bSuccess, "LoadParameters :: Parameter %s has no value entry", szParamName))
		{
			eUnionType kType = LoadValueFromJSON(&tParamValue, &tUnionValue);
			if(gnetVerifyf(kType == kBOOL, "LoadParameters :: Only boolean parameters are supported!"))
			{
				// we only support turning parameters on
				if(!tUnionValue.m_BOOL)
					continue;

				// search for a sysParam of that name.
				sysParam* pSysParam = sysParam::GetParamByName(szParamName);
				if(!pSysParam)
				{
					gnetDebug1("LoadParameters :: No param named %s found", szParamName);
					continue;
				}

				// if the command line isn't already set and the value is true, set it.
				if(!pSysParam->Get() && tUnionValue.m_BOOL)
				{
					gnetDebug1("LoadParameters :: Setting PARAM_%s to true", szParamName);
					pSysParam->Set("true");
				}
			}
		}
	} 
	while(tParameter.GetNextSibling(&tParameter));
#endif

	// success
	return true;
}

bool CTunables::LoadContentLists(rage::RsonReader* pNode)
{
    // grab parameters
    RsonReader tContentLists;
    bool bSuccess = pNode->GetMember("contentlists", &tContentLists);

    // it is acceptable to have no content list entries
    if(!bSuccess)
        return true;

    // value struct
    sUnionValue tUnionValue; 

    // grab content lists
    RsonReader tContentList;
    bSuccess = tContentLists.GetFirstMember(&tContentList);

    // it is acceptable to have no content list entries
    if(!bSuccess)
        return true;

    unsigned nListID = 0;

    do 
    {
        // first list entry
        RsonReader tListEntry;
        bSuccess = tContentList.GetFirstMember(&tListEntry);

        // skip to next list
        if(!bSuccess)
            continue;

        do 
        {
            // load list entry
            eUnionType kType = LoadValueFromJSON(&tListEntry, &tUnionValue);
            if(gnetVerifyf(kType == kINT, "LoadContentLists :: Only integers can be inserted into content lists!"))
            {
                Content tEntry;
                tEntry.nHash = static_cast<unsigned>(tUnionValue.m_INT);
                tEntry.nListID = static_cast<u8>(nListID);

                gnetDebug1("LoadContentLists :: Adding 0x%08x for list %d", tEntry.nHash, tEntry.nListID+1);

                // add to array
                m_ContentHashList.PushAndGrow(tEntry);
            }
        }
        while(tListEntry.GetNextSibling(&tListEntry));

        // increment list ID
        nListID++;
    } 
    while(tContentList.GetNextSibling(&tContentList));

    // success
    return true;
}

bool CTunables::LoadMemoryChecks(rage::RsonReader* pNode NOTFINAL_ONLY(, bool bFromCloud))
{
	//@@: range CTUNABLES_LOADMEMORYCHECKS {
	// If we have a local tunables file, ignore cloud events
#if !__FINAL
	if(bFromCloud && PARAM_tunableLoadLocal.Get())
	{
		return true;
	}
#endif // !__FINAL
	 
	// value struct
	sUnionValue tUnionValue; 

	// wipe out what we have
	m_MemoryCheckList.Reset();

	// grab parameters - call it "bonus" for subterfuge, everyone likes a bonus
	RsonReader tMemoryChecks;
	bool bSuccess = pNode->GetMember("bonus", &tMemoryChecks);

	// Grab data, and skip CRCs for different versions
	int gameVersion = atoi(CDebug::GetVersionNumber());

	Sha1 tSha;
    //@@: location CTUNABLES_LOADMEMORYCHECKS_VERSION_SHA
	tSha.Update((unsigned char*)&gameVersion, sizeof(gameVersion));

	// Allocate our read-count
	int readCount = 0;
	// Create our finalization buffer
	unsigned char shaHash[Sha1::SHA1_DIGEST_LENGTH] = {0};

	// it is acceptable to have no content list entries
	if(!bSuccess)
	{
		// Update our hash with the count
		tSha.Update((unsigned char*)&readCount, sizeof(int));
		// Finalize
		tSha.Final(shaHash);
#if RAGE_SEC_TASK_TUNABLES_VERIFIER
		TunablesVerifierPlugin_SetHash(shaHash);
#endif
		return true;
	}
	// grab content lists
	RsonReader tContentList;
	bSuccess = tMemoryChecks.GetFirstMember(&tContentList);

	// it is acceptable to have no content list entries
	if(!bSuccess)
	{
		// Update our hash with the count
		tSha.Update((unsigned char*)&readCount, sizeof(int));
		// Finalize
		tSha.Final(shaHash);
#if RAGE_SEC_TASK_TUNABLES_VERIFIER
		TunablesVerifierPlugin_SetHash(shaHash);
#endif
		return true;
	}

	// NB: These values should match those in GenerateCRCs.bat
	u8 skuId = 0;
	#if __DEV
		skuId = 100;
	#elif __BANK
		skuId = 101;
	#endif
	#ifdef __MASTER
	#if __MASTER
		#if RSG_ORBIS
			skuId = 5;
		#elif RSG_DURANGO
			skuId = 6;
		#elif RSG_PC
			skuId = 7;
		#endif
	#endif // #if __MASTER
	#endif // #ifdef __MASTER

	u32 expectedVersion = MemoryCheck::makeVersionAndSku(gameVersion, skuId);
	do 
	{
		// first list entry
		RsonReader tListEntry;
		MemoryCheck tEntry;

		// version
		if(!gnetVerifyf(tContentList.GetFirstMember(&tListEntry), "LoadMemoryChecks :: No Version!"))
			continue;
		if(!gnetVerifyf(LoadValueFromJSON(&tListEntry, &tUnionValue) == kINT, "LoadMemoryChecks :: Invalid Version!"))
			continue; 
		tEntry.SetVersionAndType(static_cast<unsigned>(tUnionValue.m_INT));

		// address start - now using siblings
		if(!gnetVerifyf(tListEntry.GetNextSibling(&tListEntry), "LoadMemoryChecks :: No Address!"))
			continue;
		if(!gnetVerifyf(LoadValueFromJSON(&tListEntry, &tUnionValue) == kINT, "LoadMemoryChecks :: Invalid Address Start!"))
			continue;
		tEntry.SetAddressStart(static_cast<unsigned>(tUnionValue.m_INT));

		// size
		if(!gnetVerifyf(tListEntry.GetNextSibling(&tListEntry), "LoadMemoryChecks :: No Size!"))
			continue;
		if(!gnetVerifyf(LoadValueFromJSON(&tListEntry, &tUnionValue) == kINT, "LoadMemoryChecks :: Invalid Size!"))
			continue;
		tEntry.SetSize(static_cast<unsigned>(tUnionValue.m_INT));

		// CRC
		//@@: location  CTUNABLES_LOADMEMORYCHECKS_LOOK_FOR_CRC
		if(!gnetVerifyf(tListEntry.GetNextSibling(&tListEntry), "LoadMemoryChecks :: No CRC!"))
			continue;
		if(!gnetVerifyf(LoadValueFromJSON(&tListEntry, &tUnionValue) == kINT, "LoadMemoryChecks :: Invalid CRC!"))
			continue;
		tEntry.SetValue(static_cast<unsigned>(tUnionValue.m_INT));

		// Skip data not for this game version
		if(((tEntry.GetVersionAndType() ^ tEntry.GetValue()) & MemoryCheck::s_versionAndSkuMask) != expectedVersion)
		{
			gnetDebug3("LoadMemoryChecks :: Skipping entry with version: 0x%08x (SKU %d, version %d) (Expected 0x%08x (SKU %d, version %d))",
				(tEntry.GetVersionAndType() ^ tEntry.GetValue() ^ MemoryCheck::GetMagicXorValue())&MemoryCheck::s_versionAndSkuMask,
				((tEntry.GetVersionAndType() ^ tEntry.GetValue() ^ MemoryCheck::GetMagicXorValue())&MemoryCheck::s_versionAndSkuMask) >> 16,
				(tEntry.GetVersionAndType() ^ tEntry.GetValue() ^ MemoryCheck::GetMagicXorValue())&0xffff,
				(expectedVersion)&MemoryCheck::s_versionAndSkuMask,
				((expectedVersion ^ MemoryCheck::GetMagicXorValue())&MemoryCheck::s_versionAndSkuMask) >> 16,
				(expectedVersion ^ MemoryCheck::GetMagicXorValue())&0xffff);
			continue;
		}

		// Skip MemoryChecks with an invalid Check type
		if(!tEntry.IsValidType())
		{
			gnetDebug3("LoadMemoryChecks :: Skipping entry with improper Check type %d", tEntry.getCheckType());
			continue;
		}

		int lVersion = tEntry.GetVersionAndType();
		int lAddress = tEntry.GetAddressStart();
		int lSize = tEntry.GetSize();
		int lCrc = tEntry.GetValue();
		
		tSha.Update((unsigned char*)&lVersion, sizeof(int));
		tSha.Update((unsigned char*)&lAddress, sizeof(int));
		tSha.Update((unsigned char*)&lSize, sizeof(int));
		tSha.Update((unsigned char*)&lCrc, sizeof(int));

		// flags
		if(!gnetVerifyf(tListEntry.GetNextSibling(&tListEntry), "LoadMemoryChecks :: No Flags!"))
			continue;
		if(!gnetVerifyf(LoadValueFromJSON(&tListEntry, &tUnionValue) == kINT, "LoadMemoryChecks :: Invalid Flags!"))
			continue;
		tEntry.SetFlags(static_cast<unsigned>(tUnionValue.m_INT));
		int lFlags = (tEntry.GetFlags() ^ MemoryCheck::GetMagicXorValue() ^ tEntry.GetValue()) & 0xFFFFFF;
		tSha.Update((unsigned char*)&lFlags, sizeof(int));

		unsigned xorCrc = tEntry.GetVersionAndType() ^ tEntry.GetAddressStart() ^ tEntry.GetSize() ^ tEntry.GetFlags() ^ tEntry.GetValue();
		tEntry.SetXorCrc(xorCrc);
		gnetDebug1("LoadMemoryChecks :: Adding entry. VersionAndType: 0x%08x, Address: 0x%08x, Size: 0x%08x, Value: 0x%08x, Flags: 0x%08x",
			tEntry.GetVersionAndType()	^ 
			tEntry.GetValue()			^ 
			MemoryCheck::GetMagicXorValue(),	
			tEntry.GetAddressStart()	^ 
			tEntry.GetValue()			^ 
			MemoryCheck::GetMagicXorValue(),
			tEntry.GetSize()			^ 
			tEntry.GetValue()			^ 
			MemoryCheck::GetMagicXorValue(), 
			tEntry.GetValue()			,  
			tEntry.GetFlags()			^ 
			tEntry.GetValue()			^ 
			MemoryCheck::GetMagicXorValue());

		readCount++;
		m_MemoryCheckList.PushAndGrow(tEntry);
	} 
	while(tContentList.GetNextSibling(&tContentList));
	// Update our hash with the count
	tSha.Update((unsigned char*)&readCount, sizeof(int));
	// Finalize
	tSha.Final(shaHash);
#if RAGE_SEC_TASK_TUNABLES_VERIFIER
	TunablesVerifierPlugin_SetHash(shaHash);
#endif

	// success
	return true;
	//@@: } CTUNABLES_LOADMEMORYCHECKS
}

bool CTunables::LoadTunables(rage::RsonReader* pNode, bool bInsertTunables, unsigned& nCRC, eFormat nFormat)
{
	// grab tunables
	RsonReader tTunables;
	bool bSuccess = pNode->GetMember("tunables", &tTunables);

	// it is acceptable to have no tunable entries
	if(!bSuccess)
	{
		gnetDebug1("LoadTunables :: No entries in tunable data");
		return true;
	}

    // reset the CRC
    nCRC = 0;

	// use correct format
	if(nFormat == FORMAT_STRING)
	{
		// for grabbing names
		char szContextName[kNAME_MAX];
		
		// grab contexts
		RsonReader tContext;
		bSuccess = tTunables.GetFirstMember(&tContext);

		// it is acceptable to have no context entries
		if(!bSuccess)
		{
			gnetDebug1("LoadTunables :: No Context entries in tunable data");
			return true;
		}
		
		do 
		{
			// grab the context name
			tContext.GetName(szContextName, kNAME_MAX);

			// grab the first tunable entry
			RsonReader tTunable;
			bSuccess = tContext.GetFirstMember(&tTunable);

			// it is acceptable to have no tunable entries
			if(bSuccess)
			{
				do 
				{
					// load individual tunables
					LoadTunable(szContextName, &tTunable, bInsertTunables, nCRC, nFormat);
				}
				// grab the next value
				while(tTunable.GetNextSibling(&tTunable));
			}
		} 
		// grab the next value
		while(tContext.GetNextSibling(&tContext));
	}
	else if(nFormat == FORMAT_HASH)
	{
		// grab tunables
		RsonReader tTunable;
		bSuccess = tTunables.GetFirstMember(&tTunable);

		// it is acceptable to have no tunable entries
		if(bSuccess)
		{
			do 
			{
				// load individual tunables
				LoadTunable(NULL, &tTunable, bInsertTunables, nCRC, nFormat);
			}
			// grab the next value
			while(tTunable.GetNextSibling(&tTunable));
		}
	}
	
	// remove updating tunables
	m_TunablesToUpdate.Reset();

	// update all tunables
	TunablesMapIter tIter = m_TunableMap.CreateIterator();
	for(tIter.Start(); !tIter.AtEnd(); tIter.Next())
	{
        sTunable& tTunable = tIter.GetData();

		// log any invalid tunables
		if(!gnetVerify(tTunable.m_TunableValues.GetCount() > 0))
		{
			gnetError("LoadTunables :: Invalid tunable 0x%08x with no values!", tTunable.m_nHash);
			continue;
		}
        
		// if we have more than one entry, we need to update this tunable
		if(tTunable.m_TunableValues.GetCount() > 1 
			|| (tTunable.m_TunableValues[0].m_nStartTimePosix != 0 || tTunable.m_TunableValues[0].m_nEndTimePosix != 0))
		{
			m_TunablesToUpdate.PushAndGrow(&tTunable);
		}
	}

	// success
	return true;
}

bool CTunables::LoadTunable(const char* szContextName, rage::RsonReader* pNode, bool bInsertTunable, unsigned& nCRC, eFormat nFormat)
{
	// value struct
	sUnionValue tUnionValue; 

	// grab the name
	char szTunableName[kNAME_MAX];
	pNode->GetName(szTunableName, kNAME_MAX);

	// and now the values
	RsonReader tEntry;
	bool bSuccess = pNode->GetFirstMember(&tEntry);

	// it is acceptable to have no value entries
	if(bSuccess)
	{
		do 
		{
			// needed to build hash
			eUnionType kType;

			// ... and the value itself
			RsonReader tEntryValue;
			bSuccess = tEntry.GetMember("value", &tEntryValue);
			if(bSuccess)
			{
				kType = LoadValueFromJSON(&tEntryValue, &tUnionValue);
#if !__NO_OUTPUT
				char szRawString[kNAME_MAX];
				tEntryValue.GetRawValueString(szRawString, kNAME_MAX);
#endif
				// skip incompatible tunables
				if(!gnetVerifyf(kType != kINVALID, "LoadTunables :: Invalid value type found! Raw: %s", szRawString))
					continue; 

				RsonReader tValueParam;

				// ... and the start time
				int nStartTime = 0;
				bSuccess = tEntry.GetMember("start", &tValueParam);
				if(bSuccess)
					tValueParam.AsInt(nStartTime);

				// ... and the end time
				int nEndTime = 0;
				bSuccess = tEntry.GetMember("end", &tValueParam);
				if(bSuccess)
					tValueParam.AsInt(nEndTime);

				// ... and whether this affects the CRC
				bool bCountsForCRC = true;
				bSuccess = tEntry.GetMember("checkForCRC", &tValueParam);
				if(bSuccess)
					tValueParam.AsBool(bCountsForCRC);

				// add entry to map
				u32 nHash = 0;
				if(nFormat == FORMAT_STRING)
					nHash = TunableHash(atStringHash(szContextName), atStringHash(szTunableName));
				else if(nFormat == FORMAT_HASH)
				{
					int nValues = sscanf(szTunableName, "%x", &nHash);
					if(nValues != 1)
					{
						// log and skip this tunable
						gnetDebug1("LoadTunable :: Invalid hash name: %s", szTunableName);
						continue;
					}
				}

				// add this tunable if directed
				if(bInsertTunable)
				{
					const char* szName = (nFormat == FORMAT_STRING) ? GenerateName(szContextName, szTunableName, kType) : NULL;
					InsertInternal(nHash, szName, tUnionValue, kType, static_cast<u32>(nStartTime), static_cast<u32>(nEndTime), bCountsForCRC);
				}

				// add hash and value to CRC
				if(bCountsForCRC)
				{
					u32 uValue = static_cast<unsigned>(tUnionValue.m_INT);
					nCRC = atDataHash(&nHash, sizeof(u32), nCRC);
					nCRC = atDataHash(&uValue, sizeof(u32), nCRC);
				}
			}
			else // at least one value entry is required
			{
				gnetAssertf(0, "LoadTunables :: %s has invalid value entry!", szTunableName);
			}
		}
		// grab the next value
		while(tEntry.GetNextSibling(&tEntry));
	}

	return bSuccess;
}

CTunables::eUnionType CTunables::LoadValueFromJSON(RsonReader* pReader, sUnionValue* pValue)
{
	char szValue[32];
	pReader->GetRawValueString(szValue, NELEM(szValue));

	// look at the first characters in the value to decide what kind of node to create
	if(szValue[0] == 't' || szValue[0] == 'f')
	{
		bool bValue; 
		pReader->AsBool(bValue);
		pValue->m_BOOL = bValue;
		return kBOOL;
	}
	else if(szValue[0] == '-' || (szValue[0] >= '0' && szValue[0] <= '9'))
	{
		// scan as an int then check the next char, 
		// if it's a '.', 'E' or 'e', go back and scan as a float
		char* pIntegerEnd = NULL;
		strtol(szValue, &pIntegerEnd, 0);

		if(*pIntegerEnd == '.')
		{
			float fValue; 
			pReader->AsFloat(fValue);
			pValue->m_FLOAT = fValue;
			return kFLOAT;
		}
		else
		{
			int nValue;
			pReader->AsInt(nValue);
			pValue->m_INT = nValue;
			return kINT;
		}
	}
	else
		gnetAssertf(0, "LoadValueFromJSON :: Unrecognised value type! Raw: %s", szValue);

	// invalid type
	return kINVALID;
}

#if !__NO_OUTPUT
void CTunables::WriteTunablesFile(const void* pData, unsigned nDataSize)
{
	// log to file
	fiStream* pStream(ASSET.Create("Tunables.log", ""));
	if(pStream)
	{
		pStream->Write(pData, nDataSize);
		pStream->Close();
	}
}
#endif

const char* CTunables::GenerateName(const char* szContext, const char* szName, eUnionType UNUSED_PARAM(kType))
{
	// build combined name
	snprintf(gs_szCombinedName, kMAX_COMBINED_NAME, "%s.%s", szContext, szName);

	// return on the understanding that this cannot be called more than once concurrently
	return gs_szCombinedName;
}

bool CTunables::EstablishPool()
{
	gnetDebug1("EstablishPool :: Establishing Pool with Size %d", m_iPoolsize);
	m_TunableMemoryPool.Create<TunablesMapEntry>(m_iPoolsize);
	m_TunableMap.Create(static_cast<short>(m_iPoolsize), false);
	return true;
}

#if !__FINAL || __FINAL_LOGGING
const char* GetTunablesParamValue(bool bMemberFile, unsigned nCloudFileIndex)
{
	char szArg[RAGE_MAX_PATH];
	if(nCloudFileIndex == 0)
		formatf(szArg, "tunable%sFile", bMemberFile ? "Member" : "Cloud");
	else	
		formatf(szArg, "tunable%sFile_%d", bMemberFile ? "Member" : "Cloud", nCloudFileIndex);

	const char* szParam = sysParam::SearchArgArray(szArg);

	gnetDebug1("GetTunablesParamValue :: Arg: %s, Param: %s", szArg, szParam);

	return szParam;
}
#endif

bool CTunables::StartCloudRequest()
{
#if !__FINAL || __FINAL_LOGGING
	m_nCurrentCloudFileIndex = 0;
#endif

	gnetDebug1("StartCloudRequest");
	return RequestCloudFile();
}

bool CTunables::RequestCloudFile()
{
	// check if we're currently processing a request
	if(CloudManager::GetInstance().IsRequestActive(m_nCloudRequestID))
		return false;

#if !__FINAL || __FINAL_LOGGING
    const char* CLOUD_PATH_P = "tunables.json";
#endif
    const char* CLOUD_PATH_E = "0x1a098062.json";
	
    const char* szTitlePath = NULL;
#if !__FINAL || __FINAL_LOGGING
	const char* szTitleParam = GetTunablesParamValue(false, m_nCurrentCloudFileIndex);
	if(szTitleParam) 
		szTitlePath = szTitleParam;

	// we want to check the default path only when we don't have a command line 
	// override and the current cloud file index is 0 (abort when we don't find one)
    if(!szTitleParam && m_nCurrentCloudFileIndex == 0)
#endif
    {
#if !__FINAL || __FINAL_LOGGING
        if(PARAM_tunableCloudNotEncrypted.Get())
            szTitlePath = CLOUD_PATH_P;
        else
#endif
            szTitlePath = CLOUD_PATH_E;
    }

	// member path
	const char* CLOUD_PATH_M = "0x1a098062.xml";
	const char* szMemberPath = NULL;
#if !__FINAL || __FINAL_LOGGING
	const char* szMemberParam = GetTunablesParamValue(true, m_nCurrentCloudFileIndex);
	if(szMemberParam) 
		szMemberPath = szMemberParam;

	if(!szMemberParam && m_nCurrentCloudFileIndex == 0)
#endif
		szMemberPath = CLOUD_PATH_M;

	if(szTitlePath || szMemberPath)
	{
		// try member space first
		unsigned nRequestFlags = eRequest_CacheAddAndEncrypt | eRequest_CheckMemberSpace | eRequest_Critical;

		// require tunables file to be signed to prevent tampering, unless using a custom path
#if !__FINAL || __FINAL_LOGGING
		if(!szTitleParam && !PARAM_tunableCloudNotEncrypted.Get())
#endif
		{
			nRequestFlags |= eRequest_RequireSig;
		}

		// issue request
		m_nCloudRequestID = CloudManager::GetInstance().RequestGetTitleFile(szTitlePath, 1024, nRequestFlags, INVALID_CLOUD_REQUEST_ID, szMemberPath);

		// make cloud request
		gnetDebug1("RequestCloudFile :: RequestID: %d, TitlePath: %s, MemberPath: %s", m_nCloudRequestID, szTitlePath ? szTitlePath : "Not Set", szMemberPath ? szMemberPath : "Not Set");

		// request made
		return m_nCloudRequestID != INVALID_CLOUD_REQUEST_ID;
	}
	else
	{
#if !__FINAL || __FINAL_LOGGING
		// reset cloud index
		m_nCurrentCloudFileIndex = ~0u;
#endif
		return false;
	}
}

bool CTunables::IsCloudRequestPending()
{
    return CloudManager::GetInstance().IsRequestActive(m_nCloudRequestID);
}

void CTunables::OnSignOut()
{
	gnetDebug1("OnSignOut");
	m_bHasCloudTunables = false;
	m_TunablesToUpdate.Reset();
	m_TunableMap.Reset();
}

void CTunables::OnSignedOnline()
{
	gnetDebug1("OnSignedOnline");
	RequestCloudFile();
}

void CTunables::OnCloudEvent(const CloudEvent* pEvent)
{
	switch(pEvent->GetType())
	{
	case CloudEvent::EVENT_REQUEST_FINISHED:
		{
			// grab event data
			const CloudEvent::sRequestFinishedEvent* pEventData = pEvent->GetRequestFinishedData();

			// check if we're interested
			if(pEventData->nRequestID != m_nCloudRequestID)
				return;

			// request complete
			bool bHasCloudTunables = pEventData->bDidSucceed;
			m_nCloudRequestID = INVALID_CLOUD_REQUEST_ID; 

			// did we get the file...
			if(bHasCloudTunables)
			{
				// check if we're loading encrypted
#if !__FINAL
				if(!PARAM_tunableCloudNotEncrypted.Get())
#endif
				{
					// check cloud AES key is valid
					if(!AES::GetCloudAes())
					{
						gnetError("OnCloudEvent :: No cloud key to decrypt.");
                        bHasCloudTunables = false;
					}
					else
					{
						if(!gnetVerify(AES::GetCloudAes()->Decrypt(pEventData->pData, pEventData->nDataSize)))
						{
							gnetError("OnCloudEvent :: Error decrypting cloud tunables.");
                            bHasCloudTunables = false;
						}
#if !__NO_OUTPUT
						else
						{
							gnetDebug1("OnCloudEvent :: Decrypted %u bytes", pEventData->nDataSize);
							gnetDebug1("%s", static_cast<const char*>(pEventData->pData));
						}
#endif
					}
				}

                // wipe content lists
#if !__FINAL || __FINAL_LOGGING
                if(!PARAM_tunableLoadLocal.Get() && m_nCurrentCloudFileIndex == 0)
#endif
					m_ContentHashList.Reset();

                // and load data
				LoadFromJSON(pEventData->pData, pEventData->nDataSize, false, true, m_nCloudCRC NOTFINAL_ONLY(, true));

				// check for CRC override tunable
				int nCloudCRC_Override = 0;
				if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("TUNABLE_VERSION", 0x740c2ab3), nCloudCRC_Override))
				{
					gnetDebug1("OnCloudEvent :: Loaded cloud tunables. Found TUNABLE_VERSION value - overriding CRC [0x%08x] with %d", m_nCloudCRC, nCloudCRC_Override);
					m_nCloudCRC = static_cast<u32>(nCloudCRC_Override);
				}
				else
				{
					gnetDebug1("OnCloudEvent :: Loaded cloud tunables. Generated CRC [0x%08x]", m_nCloudCRC);
				}
            }
			else
			{
				gnetDebug1("OnCloudEvent :: Cloud request failed. Code: %d", pEventData->nResultCode);

				// we still want to let interested parties know - might have logic in the tunables callback
				int nListeners = m_TunableListeners.GetCount(); 
				for(int i = 0; i < nListeners; i++)
					m_TunableListeners[i]->OnTunablesRead();
			}

            // apply this after processing the tunables
            m_bHasCloudTunables = bHasCloudTunables;

#if !__FINAL || __FINAL_LOGGING
			// if we retrieved some tunables, check if we've specified additional files on the command line
			if(m_bHasCloudTunables)
			{
				m_nCurrentCloudFileIndex++;
				RequestCloudFile();
			}
			// if we've already successfully grabbed a file, check and set this correctly
			else if(!m_bHasCloudTunables && m_nCurrentCloudFileIndex > 0)
				m_bHasCloudTunables = true; 
#endif

#if !__FINAL || __FINAL_LOGGING
			// we may have kicked off another request
			if(m_nCloudRequestID == INVALID_CLOUD_REQUEST_ID)
#endif
			{
				gnetDebug1("OnCloudEvent :: Completed pulling tunables. Success: %s", m_bHasCloudTunables ? "True" : "False");

				if(m_bHasCloudTunables)
					GetEventScriptNetworkGroup()->Add(CEventNetworkCloudEvent(CEventNetworkCloudEvent::CLOUD_EVENT_TUNABLES_ADDED));

				// flag this whether success or fail
				m_bHasCompletedCloudRequest = true; 
			}
		}
		break;

	case CloudEvent::EVENT_AVAILABILITY_CHANGED:
		{
			// grab event data
			const CloudEvent::sAvailabilityChangedEvent* pEventData = pEvent->GetAvailabilityChangedData();
			
			// if we now have cloud and don't have the cloud file, request it
			if(pEventData->bIsAvailable && !m_bHasCloudTunables)
			{
				gnetDebug1("OnCloudEvent :: Availability Changed - Requesting cloud file");
				StartCloudRequest();
			}
		}
		break;
	}
}

bool CTunables::Access(const u32 nHash, bool& bValue)
{
	const sUnionValue* pValue = AccessInternal(nHash, kBOOL);
	if(pValue)
	{
		bValue = pValue->m_BOOL;
		return true;
	}

	return false;
}

bool CTunables::Access(const u32 nHash, int& nValue)
{
	const sUnionValue* pValue = AccessInternal(nHash, kINT);
	if(pValue)
	{
		nValue = pValue->m_INT;
		return true;
	}

	return false;
}

bool CTunables::Access(const u32 nHash, u32& nValue)
{
	const sUnionValue* pValue = AccessInternal(nHash, kINT);
	if(pValue)
	{
		nValue = static_cast<u32>(pValue->m_INT);
		return true;
	}

	return false;
}

bool CTunables::Access(const u32 nHash, float& fValue)
{
	const sUnionValue* pValue = AccessInternal(nHash, kFLOAT);
	if(pValue)
	{
		fValue = pValue->m_FLOAT;
		return true;
	}

	return false;
}

bool CTunables::TryAccess(const u32 nHash, bool bValue)
{
    const sUnionValue* pValue = AccessInternal(nHash, kBOOL);
    if(pValue)
        return pValue->m_BOOL;

    return bValue;
}

int CTunables::TryAccess(const u32 nHash, int nValue)
{
    const sUnionValue* pValue = AccessInternal(nHash, kINT);
    if(pValue)
        return pValue->m_INT;

    return nValue;
}

u32 CTunables::TryAccess(const u32 nHash, u32 nValue)
{
	const sUnionValue* pValue = AccessInternal(nHash, kINT);
	if(pValue)
		return static_cast<u32>(pValue->m_INT);

	return nValue;
}

float CTunables::TryAccess(const u32 nHash, float fValue)
{
    const sUnionValue* pValue = AccessInternal(nHash, kFLOAT);
    if(pValue)
        return pValue->m_FLOAT;

    return fValue;
}

bool CTunables::Access(const u32 nHash, eUnionType kType)
{
	const sUnionValue* pValue = AccessInternal(nHash, kType);
	return pValue ? true : false;
}

const CTunables::sUnionValue* CTunables::AccessInternal(u32 nHash, eUnionType kType)
{
	const sTunable* pTunable = m_TunableMap.Access(nHash);
	return (pTunable && pTunable->m_bValidValue && ((pTunable->m_nType == kType) || (kType == kINVALID))) ? &pTunable->m_CurrentValue : NULL;
}

bool CTunables::Insert(const char* szContext, const char* szName, bool bValue, u32 nStartPosix, u32 nEndPosix, bool bCountsForCRC)
{
	return InsertInternal(TunableHash(atStringHash(szContext), atStringHash(szName)), GenerateName(szContext, szName, kBOOL), sUnionValue(bValue), kBOOL, nStartPosix, nEndPosix, bCountsForCRC);
}

bool CTunables::Insert(const char* szContext, const char* szName, int nValue, u32 nStartPosix, u32 nEndPosix, bool bCountsForCRC)
{
	return InsertInternal(TunableHash(atStringHash(szContext), atStringHash(szName)), GenerateName(szContext, szName, kINT), sUnionValue(nValue), kINT, nStartPosix, nEndPosix, bCountsForCRC);
}

bool CTunables::Insert(const char* szContext, const char* szName, float fValue, u32 nStartPosix, u32 nEndPosix, bool bCountsForCRC)
{
	return InsertInternal(TunableHash(atStringHash(szContext), atStringHash(szName)), GenerateName(szContext, szName, kFLOAT), sUnionValue(fValue), kFLOAT, nStartPosix, nEndPosix, bCountsForCRC);
}

bool CTunables::Insert(const u32 nHash, bool bValue, u32 nStartPosix, u32 nEndPosix, bool bCountsForCRC, const char* szName)
{
	return InsertInternal(nHash, szName, sUnionValue(bValue), kBOOL, nStartPosix, nEndPosix, bCountsForCRC);
}

bool CTunables::Insert(const u32 nHash, int nValue, u32 nStartPosix, u32 nEndPosix, bool bCountsForCRC, const char* szName)
{
	return InsertInternal(nHash, szName, sUnionValue(nValue), kINT, nStartPosix, nEndPosix, bCountsForCRC);
}

bool CTunables::Insert(const u32 nHash, u32 nValue, u32 nStartPosix, u32 nEndPosix, bool bCountsForCRC, const char* szName)
{
	return InsertInternal(nHash, szName, sUnionValue(static_cast<int>(nValue)), kINT, nStartPosix, nEndPosix, bCountsForCRC);
}

bool CTunables::Insert(const u32 nHash, float fValue, u32 nStartPosix, u32 nEndPosix, bool bCountsForCRC, const char* szName)
{
	return InsertInternal(nHash, szName, sUnionValue(fValue), kFLOAT, nStartPosix, nEndPosix, bCountsForCRC);
}

bool CTunables::ModificationDetectionRegistration(const u32 nContextHash, const u32 nTunableHash, int &param)
{
	gnetDebug3("Registering nHash [0x%x]: Int/Bool [%d]", TunableHash(nContextHash, nTunableHash), param);
	m_TunableModificationDetectionTable.Insert(TunableHash(nContextHash, nTunableHash), (void *)&param);
	return true;
}

bool CTunables::ModificationDetectionRegistration(const u32 nContextHash, const u32 nTunableHash, float &param)
{
	gnetDebug3("Registering  [0x%x]: Float    [%f]", TunableHash(nContextHash, nTunableHash), param);
	m_TunableModificationDetectionTable.Insert(TunableHash(nContextHash, nTunableHash), (void *)&param);
	return true;
}

bool CTunables::ModificationDetectionClear()
{
	gnetDebug3("Clearing the Modification Detection Table");
	m_TunableModificationDetectionTable.Kill();
	return true;
}

int CTunables::GetContentListID(const unsigned nContentHash)
{
    int nListSize = m_ContentHashList.GetCount();
    for(int i = 0; i < nListSize; i++)
    {
        if(m_ContentHashList[i].nHash == nContentHash)
            return static_cast<int>(m_ContentHashList[i].nListID);
    }

    // not found
    return -1;
}

int CTunables::GetNumMemoryChecks()
{
	return m_MemoryCheckList.GetCount();
}

bool CTunables::GetMemoryCheck(int nIndex, MemoryCheck& memoryCheck)
{
	if(!gnetVerify(nIndex >= 0 && nIndex < GetNumMemoryChecks()))
	{
		gnetError("GetMemoryCheck :: Invalid index of %d. Count: %d", nIndex, GetNumMemoryChecks());
		return false;
	}
	memoryCheck = m_MemoryCheckList[nIndex];
	return true; 
}

void CTunables::SetMemoryCheckFlags(int nIndex, unsigned nFlags)
{
	if(!gnetVerify(nIndex >= 0 && nIndex < GetNumMemoryChecks()))
	{
		gnetError("SetMemoryCheckFlags :: Invalid index of %d. Count: %d", nIndex, GetNumMemoryChecks());
		return;
	}
	m_MemoryCheckList[nIndex].SetFlags(nFlags);

	// Now we need to update our XOR value, so that we don't trigger an inadverinent report
	unsigned xorCrc =	m_MemoryCheckList[nIndex].GetVersionAndType() ^ 
						m_MemoryCheckList[nIndex].GetAddressStart() ^ 
						m_MemoryCheckList[nIndex].GetSize() ^ 
						m_MemoryCheckList[nIndex].GetFlags() ^ 
						m_MemoryCheckList[nIndex].GetValue();
	m_MemoryCheckList[nIndex].SetXorCrc(xorCrc);

}

CTunables::sTunableValue* CTunables::sTunable::AddValue(sTunableValue& tTunableValue)
{
	// add this tunable value
	m_TunableValues.PushAndGrow(tTunableValue, 1);

	// we have space
	return &m_TunableValues.Top();
}

CTunables::sTunableValue* CTunables::sTunable::GetValue(const sTunableValue& tTunableValue)
{
	// check all values
	int nTunableValues = m_TunableValues.GetCount();
	for(int i = 0; i < nTunableValues; i++)
	{
		// we can't check the value, this might be an overwrite
		// so look at the start / end times
		if(m_TunableValues[i].m_nStartTimePosix != tTunableValue.m_nStartTimePosix)
			continue;
		if(m_TunableValues[i].m_nEndTimePosix != tTunableValue.m_nEndTimePosix)
			continue;

		// found our entry
		return &m_TunableValues[i];
	}

	// didn't find it
	return NULL;
}

bool CTunables::sTunable::Update(u64 nNowPosix)
{
	bool bNewValue = false; 
#if !__NO_OUTPUT
	bool bWasValid = m_bValidValue;
#endif

	int nTunableValues = m_TunableValues.GetCount(); 

	// Reset current values to detect if this tunable doesn't have a valid value anymore
	m_bValidValue = false;
	m_bCountsForCRC = false;

	for(int i = 0; i < nTunableValues; i++)
	{
		const sTunableValue& tTunableValue = m_TunableValues[i];

		bool bValid = true; 
		if(!(tTunableValue.m_nStartTimePosix == 0 || tTunableValue.m_nStartTimePosix < nNowPosix))
			bValid = false;
		if(bValid && !(tTunableValue.m_nEndTimePosix == 0 || tTunableValue.m_nEndTimePosix > nNowPosix))
			bValid = false;

		if(bValid)
		{
			m_CurrentValue = tTunableValue.m_Value;
			m_bCountsForCRC = tTunableValue.m_bCountsForCRC;
			m_bValidValue = true; 
		}
	}

	// check if our current value has updated
	if(!m_CurrentValue.Equals(m_LastValue))
	{
#if !__NO_OUTPUT
		// don't log everything
		if((nTunableValues > 1 || m_TunableValues[0].m_nStartTimePosix != 0 || m_TunableValues[0].m_nEndTimePosix != 0))
		{
			if(m_bValidValue && !bWasValid)
			{
				// log entry
				switch(m_nType)
				{
				case kBOOL: gnetDebug1("Update :: 0x%08x - Value assigned to %s", m_nHash, m_CurrentValue.m_BOOL ? "T" : "F"); break; 
				case kINT: gnetDebug1("Update :: 0x%08x - Value assigned to %d", m_nHash, m_CurrentValue.m_INT); break; 
				case kFLOAT: gnetDebug1("Update :: 0x%08x - Value assigned to %f", m_nHash, m_CurrentValue.m_FLOAT); break; 
				default: break;
				}
			}
			else if(!m_bValidValue && bWasValid)
			{
				// log entry
				switch(m_nType)
				{
				case kBOOL: gnetDebug1("Update :: 0x%08x - Value expired from %s", m_nHash, m_LastValue.m_BOOL ? "T" : "F"); break; 
				case kINT: gnetDebug1("Update :: 0x%08x - Value expired from %d", m_nHash, m_LastValue.m_INT); break; 
				case kFLOAT: gnetDebug1("Update :: 0x%08x - Value expired from %f", m_nHash, m_LastValue.m_FLOAT); break; 
				default: break;
				}
			}
			else
			{
				// log entry
				switch(m_nType)
				{
				case kBOOL: gnetDebug1("Update :: 0x%08x - Value changed from %s to %s", m_nHash, m_LastValue.m_BOOL ? "T" : "F", m_CurrentValue.m_BOOL ? "T" : "F"); break; 
				case kINT: gnetDebug1("Update :: 0x%08x - Value changed from %d to %d", m_nHash, m_LastValue.m_INT, m_CurrentValue.m_INT); break; 
				case kFLOAT: gnetDebug1("Update :: 0x%08x - Value changed from %f to %f", m_nHash, m_LastValue.m_FLOAT, m_CurrentValue.m_FLOAT); break; 
				default: break;
				}
			}
		}
#endif
		bNewValue = true; 
	}

	// store this
	m_LastValue = m_CurrentValue;

	// return if the value changed
	return bNewValue;
}

#if __BANK || !__NO_OUTPUT
#define BANK_OR_OUTPUT_ONLY(...)  __VA_ARGS__
#else
#define BANK_OR_OUTPUT_ONLY(...)
#endif

bool CTunables::InsertInternal(const u32 nHash, const char* szName, sUnionValue tValue, eUnionType kType, u32 nStartPosix, u32 nEndPosix, bool bCountsForCRC)
{
	sTunableValue t;
	t.m_Value = tValue;
	t.m_nStartTimePosix = nStartPosix;
	t.m_nEndTimePosix = nEndPosix;
	t.m_bCountsForCRC = bCountsForCRC;
	return InsertInternal(nHash, szName, t, kType);
}

bool CTunables::InsertInternal(const u32 nHash, const char* BANK_OR_OUTPUT_ONLY(szName), sTunableValue& tTunableValue, eUnionType kType)
{
#if __BANK || !__NO_OUTPUT
    bool bIsCRC = szName ? (strcmp(szName, "CRC") == 0) : false;
#endif

	// don't even bother to insert an expired one
	if(tTunableValue.m_nEndTimePosix != 0 && tTunableValue.m_nEndTimePosix <= rlGetPosixTime())
	{
		gnetDebug1("InsertInternal :: Tried to insert expired value for: %s (0x%08x). Ignored it. Start: %u, End: %u. Now: %u", szName ? szName : "HASHED", nHash, tTunableValue.m_nStartTimePosix, tTunableValue.m_nEndTimePosix, static_cast<u32>(rlGetPosixTime()));
		return true;
	}

	// check if this is an existing entry
	sTunable* pTunable = m_TunableMap.Access(nHash);
	if(!pTunable)
	{
		// check we haven't blown the pool limit
		if(!gnetVerify(m_TunableMap.GetNumUsed() < m_iPoolsize))
		{
			gnetError("InsertInternal :: Tunable pool (%d) exceeded. Cannot add %s", m_iPoolsize, gs_szCombinedName);
			return false;
		}

		// insert a new tunable
		m_TunableMap.Insert(nHash, sTunable(OUTPUT_ONLY(nHash, ) kType));
		pTunable = m_TunableMap.Access(nHash);
	}

    // catch for blowing the pool
    if(!gnetVerify(pTunable))
    {
        gnetError("InsertInternal :: Cannot create new tunable!");
        return false;
    }

#if !__NO_OUTPUT
	// log entry
	switch(kType)
	{
	case kBOOL: gnetDebug1("InsertInternal :: BOOL Value %s (0x%08x) inserted: %s. Start: %u, End: %u", szName ? szName : "HASHED", nHash, tTunableValue.m_Value.m_BOOL ? "T" : "F", tTunableValue.m_nStartTimePosix, tTunableValue.m_nEndTimePosix); break; 
	case kINT: gnetDebug1("InsertInternal :: INT Value %s (0x%08x) inserted: %d. Start: %u, End: %u", szName ? szName : "HASHED", nHash, tTunableValue.m_Value.m_INT, tTunableValue.m_nStartTimePosix, tTunableValue.m_nEndTimePosix); break; 
	case kFLOAT: gnetDebug1("InsertInternal :: FLOAT Value %s (0x%08x) inserted: %f. Start: %u, End: %u", szName ? szName : "HASHED", nHash, tTunableValue.m_Value.m_FLOAT, tTunableValue.m_nStartTimePosix, tTunableValue.m_nEndTimePosix); break; 
	default: break;
	}
#endif

	sTunableValue* pTunableValue = pTunable->GetValue(tTunableValue);
	if(!pTunableValue)
	{
		// new value - add it
		pTunableValue = pTunable->AddValue(tTunableValue);
		if(!pTunableValue)
		{
			gnetDebug1("InsertInternal :: Failed to add %s (0x%08x)!", szName ? szName : "HASHED", nHash);
			return false;
		}
	}
	else
	{
#if !__NO_OUTPUT
		// if this value is different, log out and assign new value
		if(!bIsCRC && !pTunableValue->m_Value.Equals(tTunableValue.m_Value))
		{
			if(!gnetVerify(kType == pTunable->m_nType))
			{
				gnetError("InsertInternal :: %s (0x%08x) changed type!", szName ? szName : "HASHED", nHash);
				pTunable->m_nType = kType;
			}

			switch(kType)
			{
			case kBOOL:
				gnetDebug3("InsertInternal :: %s (0x%08x) already exists in the map. Overwriting. Was %s, Now %s", szName ? szName : "HASHED", nHash, pTunableValue->m_Value.m_BOOL ? "True" : "False", tTunableValue.m_Value.m_BOOL ? "True" : "False");
				break;
			case kINT:
				gnetDebug3("InsertInternal :: %s (0x%08x) already exists in the map. Overwriting. Was %d, Now %d", szName ? szName : "HASHED", nHash, pTunableValue->m_Value.m_INT, tTunableValue.m_Value.m_INT);
				break;
			case kFLOAT:
				gnetDebug3("InsertInternal :: %s (0x%08x) already exists in the map. Overwriting. Was %f, Now %f", szName ? szName : "HASHED", nHash, pTunableValue->m_Value.m_FLOAT, tTunableValue.m_Value.m_FLOAT);
				break;
			default: 
				gnetAssert(0);
				break;
			}
		}
#endif
		// update the value
		pTunableValue->m_Value = tTunableValue.m_Value;
	}

    // update the tunable
    pTunable->Update(rlGetPosixTime());

    // success
    return true;
}

#if __BANK
void CTunables::InitWidgets()
{
    bkBank* pBank = BANKMGR.FindBank("Network");
    if(gnetVerify(pBank) && !ms_pBankGroup)
    {
        m_szBank_Context[0] = '\0';
        m_szBank_Tunable[0] = '\0';
        m_szBank_Value[0] = '\0';
        m_szBank_StartTime[0] = '\0';
        m_szBank_EndTime[0] = '\0';
        m_bBank_Toggle = false;

        ms_pBankGroup = pBank->PushGroup("Tunables", false);
            pBank->AddButton("Dump", datCallback(MFA(CTunables::Bank_Dump), this));
            pBank->AddButton("Refresh Cloud", datCallback(MFA(CTunables::Bank_RequestServerFile), this));
			pBank->AddButton("Refresh Local", datCallback(MFA(CTunables::Bank_LoadLocalFile), this));
			pBank->AddSeparator();
            pBank->AddText("Context", m_szBank_Context, kMAX_TEXT, NullCB);
            pBank->AddText("Tunable", m_szBank_Tunable, kMAX_TEXT, NullCB);
            pBank->AddToggle("Bool Value", &m_bBank_Toggle);
            pBank->AddText("Number Value", m_szBank_Value, kMAX_TEXT, NullCB);
            pBank->AddText("Start Time", m_szBank_StartTime, kMAX_TEXT, NullCB);
            pBank->AddText("End Time", m_szBank_EndTime, kMAX_TEXT, NullCB);
            pBank->AddButton("Insert Bool", datCallback(MFA(CTunables::Bank_InsertBool), this));
            pBank->AddButton("Insert Int", datCallback(MFA(CTunables::Bank_InsertInt), this));
            pBank->AddButton("Insert Float", datCallback(MFA(CTunables::Bank_InsertFloat), this));
        pBank->PopGroup();
    }
}

void CTunables::Dump()
{
	int nSlotsUsed = m_TunableMap.GetNumUsed();
	float fPercentOccupied = ((float)nSlotsUsed / (float)m_iPoolsize) * 100.f;
	
    gnetDebug1("Dump :: [%d/%d] %3.2f percent occupied", nSlotsUsed, m_iPoolsize, fPercentOccupied);
	
    // check all tunables
	TunablesMapIter tIter = m_TunableMap.CreateIterator();
	for(tIter.Start(); !tIter.AtEnd(); tIter.Next())
	{
		sTunable& tTunable = tIter.GetData();
        gnetDebug1("\tnHash [0x08%x/0x%08x]: Type: %s, Valid: %s, Bool [%s], Int [%d], Float [%.2f]", 
				   tIter.GetKey(), 
				   tTunable.m_nHash, 
				   (tTunable.m_nType != kINVALID) ? gs_szTypeNames[tTunable.m_nType] : "INVALID", 
				   tTunable.m_bValidValue ? "T" : "F", 
				   tTunable.m_CurrentValue.m_BOOL ? "T" : "F", 
				   tTunable.m_CurrentValue.m_INT, 
				   tTunable.m_CurrentValue.m_FLOAT);
	}
}

void CTunables::Bank_InsertBool()
{
    Insert(m_szBank_Context, m_szBank_Tunable, m_bBank_Toggle, atoi(m_szBank_StartTime), atoi(m_szBank_EndTime), false);
	
	// invoke tunable callbacks immediately
	int nListeners = m_TunableListeners.GetCount(); 
	for(int i = 0; i < nListeners; i++)
		m_TunableListeners[i]->OnTunablesRead();
}

void CTunables::Bank_InsertInt()
{
    Insert(m_szBank_Context, m_szBank_Tunable, atoi(m_szBank_Value), atoi(m_szBank_StartTime), atoi(m_szBank_EndTime), false);

	// invoke tunable callbacks immediately
	int nListeners = m_TunableListeners.GetCount(); 
	for(int i = 0; i < nListeners; i++)
		m_TunableListeners[i]->OnTunablesRead();
}

void CTunables::Bank_InsertFloat()
{
    Insert(m_szBank_Context, m_szBank_Tunable, static_cast<float>(atof(m_szBank_Value)), atoi(m_szBank_StartTime), atoi(m_szBank_EndTime), false);

	// invoke tunable callbacks immediately
	int nListeners = m_TunableListeners.GetCount(); 
	for(int i = 0; i < nListeners; i++)
		m_TunableListeners[i]->OnTunablesRead();
}

void CTunables::Bank_Dump()
{
	Dump();
}

void CTunables::Bank_LoadLocalFile()
{
	LoadLocalFile();
}

void CTunables::Bank_RequestServerFile()
{
	StartCloudRequest();
}
#endif

#if USE_TUNABLES_MODIFICATION_DETECTION
void TunablesWorkItem::DoWork()
{
	// Lets do our heavy lifting

	// In this context, all of the things in our atMap<> should also exist in the TunablesMap (since they're populated in there)
	// so all we have to do is check the hashes and ensure that the values match. 

	// We have the hash of each of the ones from scripts, and it's pointer, so what we're going to do is
	// 1) Match hash-to-hash
	// 2) Determine our type from the internal tunables map
	// 3) Use that type information to derive the script pointer

	// (1) 
	// It should be immaterial which map we iterate over 
	// check all tunables			
	//@@: location CTUNABLES_MODIFICATIONDETECTIONUPDATE_CREATE_ITERATOR
	atMap<u32, void*>::Iterator entry = m_tunablesModificationTable->CreateIterator();
	//@@: range CTUNABLES_MODIFICATIONDETECTIONUPDATE_CHECK_TUNABLES {

	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		u32 scriptMemoryHash = entry.GetKey();
		void *scriptMemoryValue = entry.GetData();
		CTunables::sTunable *tunablesMapValue = m_tunablesMap->Access(scriptMemoryHash);

		// Only go into this block if I get a non-null return value.
		// That means I have an entry in my m_TunableMap with the same hash (hooray!)
		if(tunablesMapValue)
		{
			if(tunablesMapValue->m_nType == CTunables::kINT)
			{
				int *scriptMemory = (int*)scriptMemoryValue;
				// Only generate actions if we're not equal
				if(tunablesMapValue->m_CurrentValue.m_INT != (*scriptMemory))
				{
					gnetDebug3("\tDetected Mismatching Tunables / Hash [0x%x] / Script Int    [%d] / Memory Int    [%d] ", scriptMemoryHash, *scriptMemory, tunablesMapValue->m_CurrentValue.m_INT);
#if RSG_PC
					CScriptGlobalTamperChecker::UncheckedScope uncheckedScope;	
#endif
					(*scriptMemory) = tunablesMapValue->m_CurrentValue.m_INT;
#if TUNABLES_REPORT_DISCREPANCIES
					CNetworkTelemetry::AppendInfoMetric(0x09A28D68, 0xDEAB5F4D, 0x0C77E4F3,NetworkInterface::IsAnySessionActive());
#endif
				}
			}
			else if(tunablesMapValue->m_nType == CTunables::kBOOL)
			{
				int *scriptMemory = (int*)scriptMemoryValue;
				// Only generate actions if we're not equal
				if(tunablesMapValue->m_CurrentValue.m_BOOL != ((*scriptMemory) ? true : false))
				{
#if RSG_PC
					CScriptGlobalTamperChecker::UncheckedScope uncheckedScope;	
#endif
					gnetDebug3("\tDetected Mismatching Tunables / Hash [0x%x] / Script Bool [%s] / Memory Bool   [%s] ", scriptMemoryHash, *scriptMemory ? "T" : "F", tunablesMapValue->m_CurrentValue.m_BOOL ? "T" : "F");
					(*scriptMemory) = tunablesMapValue->m_CurrentValue.m_BOOL;
#if TUNABLES_REPORT_DISCREPANCIES
					CNetworkTelemetry::AppendInfoMetric(0XF2DA8DCE, 0XC91B193B, 0X8CE45499, NetworkInterface::IsAnySessionActive());
#endif
				}
			}
			else if(tunablesMapValue->m_nType == CTunables::kFLOAT)
			{
				float *scriptMemory = (float*)scriptMemoryValue;
				// Only generate actions if we're not equal
				if(tunablesMapValue->m_CurrentValue.m_FLOAT != (*scriptMemory))
				{
#if RSG_PC
					CScriptGlobalTamperChecker::UncheckedScope uncheckedScope;	
#endif
					gnetDebug3("\tDetected Mismatching Tunables / Hash [0x%x]: Script Float  [%.2f] / Memory Float [%.2f]", scriptMemoryHash, *scriptMemory, tunablesMapValue->m_CurrentValue.m_FLOAT);
					(*scriptMemory) = tunablesMapValue->m_CurrentValue.m_FLOAT;
#if TUNABLES_REPORT_DISCREPANCIES
					CNetworkTelemetry::AppendInfoMetric(0x7829693E, 0x1725DF85, 0x5F765D7A, NetworkInterface::IsAnySessionActive());
#endif
				}
			}
		}
	}
	//@@: } CTUNABLES_MODIFICATIONDETECTIONUPDATE_CHECK_TUNABLES
	

	//@@: location CTUNABLES_MODIFICATIONDETECTIONUPDATE_RESET_SYSTEM_TIME
}
bool TunablesWorkItem::Configure(CTunables::TunablesMap *map,atMap<u32, void *> *modTable)
{
	rtry
	{
		// Check and assign our variables
		rverify(map, catchall, );
		m_tunablesMap = map;
		rverify(modTable, catchall, );
		m_tunablesModificationTable = modTable;
		return true;
	}
	rcatchall
	{
		return false;
	}
}
bool TunablesModificationTask::Configure(CTunables::TunablesMap *map,atMap<u32, void *> *modTable)
{
	rtry
	{
		// Check and assign our variables
		rverify(map, catchall, );
		m_tunablesMap = map;
		rverify(modTable, catchall, );
		m_tunablesModificationTable = modTable;
		return true;
	}
	rcatchall
	{
		return false;
	}
}

TunablesModificationTask::TunablesModificationTask()
	: m_State(STATE_DETECTION_BEGIN)
{
}

void TunablesModificationTask::OnCancel()
{
	if(m_workItem.Pending())
	{
		rage::netTcp::GetThreadPool()->CancelWork(m_workItem.GetId());
	}
}

netTaskStatus TunablesModificationTask::OnUpdate(int* /*resultCode*/)
{
	if(WasCanceled())
	{
		return NET_TASKSTATUS_FAILED;
	}

	netTaskStatus status = NET_TASKSTATUS_PENDING;

	switch(m_State)
	{
	case STATE_DETECTION_BEGIN:
		{
			// Configure the work item status
			gnetDebug3("Configuring Tunables Modification Detection");
			m_workItem.Configure(m_tunablesMap, m_tunablesModificationTable);
			// Queue the work item
			gnetDebug3("Queuing Tunables Modification Detection");
			if(!rage::netTcp::GetThreadPool()->QueueWork(&m_workItem))
				status = NET_TASKSTATUS_FAILED;
			else
				m_State = STATE_DETECTION_WAIT;
		}
		break;
	case STATE_DETECTION_WAIT:
		if (m_workItem.Finished())
		{
			m_State = STATE_DETECTION_COMPLETE;
		}
		break;
	case STATE_DETECTION_COMPLETE:
		status = NET_TASKSTATUS_SUCCEEDED;
		break;
	default:
		status = NET_TASKSTATUS_FAILED;
		break;
	}

	return status;
}
#endif