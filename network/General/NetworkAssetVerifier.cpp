//
// name:        NetworkAssetVerifier.cpp
// description: Network asset verifier: checksums important asset files to ensure that all machines involved in a session are running with the same data.
// written by:  John Gurney
//


#include "network/General/NetworkAssetVerifier.h"
#include "network/Cloud/Tunables.h"

// Framework headers
#include "fwnet/netchannel.h"
#include "fwsys/gameskeleton.h"
#include "fwutil/KeyGen.h"
#include "entity/archetypemanager.h"

// Game headers
#include "game/BackgroundScripts/BackgroundScripts.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "network/Network.h"
#include "scene/ExtraContent.h"
#include "script/streamedscripts.h"
#include "system/FileMgr.h"
#include "system/system.h"
#include "System/ThreadPriorities.h"
#include "System/InfoState.h"
#include "system/appcontent.h"

#if __FINAL_LOGGING
#include "data/rson.h"
#endif

NETWORK_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, assetverifier, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_assetverifier

PARAM(networkAssetIgnoreInterior, "[assetverifier] If present, will ignore model count");
PARAM(networkAssetIgnoreModelInfoCount, "[assetverifier] If present, will ignore interior proxy differences");
PARAM(networkAssetIgnoreFiles, "[assetverifier] If present, will ignore CRC file differences");
PARAM(networkAssetIgnoreScripts, "[assetverifier] If present, will ignore overridescript differences");
PARAM(networkAssetIgnoreTunables, "[assetverifier] If present, will ignore tunable differences");
PARAM(networkAssetIgnoreMemoryTamperRestriction, "[assetverifier] If present, will ignore memory tamper restrictions");
PARAM(networkAssetIgnoreBGScriptRestriction, "[assetverifier] If present, will ignore bgscript restrictions");
PARAM(networkAssetIgnoreEnvironmentRestriction, "[assetverifier] If present, will ignore environment restrictions");
PARAM(networkAssetIgnoreAll, "[assetverifier] If present, will ignore all asset verifier differences");
NOSTRIP_FINAL_LOGGING_PARAM(networkAssetIgnoreScriptSize, "[assetverifier] If present, will ignore script.rpf size differences");

#if IS_GEN9_PLATFORM
PARAM(networkAssetEnableSKURestriction, "[assetverifier] If present, will enable SKU restrictions");
#else
PARAM(networkAssetIgnoreJapaneseRestriction, "[assetverifier] If present, will ignore Japanese restrictions");
#endif

PARAM(networkAssetSetInteriorCount, "[assetverifier] If present, overrides interior proxy count");
PARAM(networkAssetSetInteriorChecksum, "[assetverifier] If present, overrides interior proxy checksum");
PARAM(networkAssetSetModelInfoCount, "[assetverifier] If present, overrides model info count");
PARAM(networkAssetSetFileCount, "[assetverifier] If present, overrides file count");
PARAM(networkAssetSetFileChecksum, "[assetverifier] If present, overrides file checksum");
PARAM(networkAssetSetOverrideScriptHash, "[assetverifier] If present, overrides script hash");
PARAM(networkAssetSetTunablesCRC, "[assetverifier] If present, overrides tunables CRC");
PARAM(networkAssetSetJapaneseBuild, "[assetverifier] If present, overrides Japanese build setting");
PARAM(networkAssetSetEnvironmentCRC, "[assetverifier] If present, overrides environment setting");
PARAM(networkAssetSetMemoryTamperCRC, "[assetverifier] If present, overrides memory tamper CRC");
PARAM(networkAssetSetBackgroundScriptCRC, "[assetverifier] If present, overrides background script CRC");
NOSTRIP_FINAL_LOGGING_PARAM(networkAssetSetScriptSize, "[assetverifier] If present, overrides script size");
NOSTRIP_FINAL_LOGGING_PARAM(networkAssetSetScriptStoreHash, "[assetverifier] If present, overrides script store rpf hash");

CNetworkAssetVerifier::CNetworkAssetVerifier() 
	: m_numFiles(0)
	, m_fileChecksum(0)
	, m_interiorProxyChecksum(0)
	, m_interiorProxyCount(0)
	, m_bJapaneseBuild(false)
	, m_nLocalTunablesCRC(0)
#if !__FINAL
	, m_modelInfoCount(0)
	, m_overrideScriptHash(0)
#endif
	, m_scriptSize(0)
	, m_scriptStoreHash(0)
	, m_scriptRpfHash(0)
	, m_memoryTamperCRC(0)
	, m_nBackgroundScriptCRC(0)
	, m_EnvironmentCRC(0)
	, m_StaticCRC(0)
	, m_CRC(0)
{
}

void CNetworkAssetVerifier::Init()
{
#if !__FINAL
	// set debug parameters
	if(PARAM_networkAssetIgnoreAll.Get())
	{
		// not setting timeout here - we should explicitly toggle that on if required
		// not setting environment here - we should explicitly toggle that on if required

		PARAM_networkAssetIgnoreModelInfoCount.Set("1");
		PARAM_networkAssetIgnoreFiles.Set("1");
		PARAM_networkAssetIgnoreInterior.Set("1");
		PARAM_networkAssetIgnoreScripts.Set("1");
		PARAM_networkAssetIgnoreScriptSize.Set("1");
		PARAM_networkAssetIgnoreTunables.Set("1");
		PARAM_networkAssetIgnoreBGScriptRestriction.Set("1");
	}
#endif

	//@@: range CNETWORKASSETVERIFIER_INIT {
	GenerateFileCRC();
	GenerateInteriorProxyData();
	GetScriptSize();
#if !__FINAL
	CountModelInfos();
	CacheOverridenScriptHash();
#endif
	GenerateLocalTunablesCRC();
	GenerateJapaneseBuild();
	GenerateBackgroundScriptCRC();
	GenerateEnvironmentCRC();

	// build matchmaking CRC
	GenerateStaticCRC();
	GenerateCRC(); 

	// log sync data
	OUTPUT_ONLY(DumpSyncData());
	//@@: } CNETWORKASSETVERIFIER_INIT
}

bool CNetworkAssetVerifier::IsDiscriminatingBySKU() const
{
#if IS_GEN9_PLATFORM
#if !__FINAL
	// default to restricted, so the parameter is used to disable / ignore
	int enableRestriction = 0;
	if(PARAM_networkAssetEnableSKURestriction.Get(enableRestriction))
		return enableRestriction != 0;
#endif

	static const bool s_DefaultSetting = true; 

	// use the default value if the tunables aren't instantiated yet
	if(!Tunables::IsInstantiated())
		return s_DefaultSetting;
	
	// provide a tunable override to toggle at run-time
	return Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_ASSET_ENABLE_SKU_RESTRICTIONS", 0xd7fa76b7), s_DefaultSetting);
#else
	// default to restricted, so the parameter is used to disable / ignore
	return !PARAM_networkAssetIgnoreJapaneseRestriction.Get();
#endif
}

void CNetworkAssetVerifier::RefreshFileCRC()
{
	if(!PARAM_networkAssetIgnoreFiles.Get())
	{
		GenerateFileCRC();
		GenerateCRC();
	}
}

void CNetworkAssetVerifier::RefreshEnvironmentCRC()
{
	if(!PARAM_networkAssetIgnoreEnvironmentRestriction.Get())
	{
		GenerateEnvironmentCRC();
		GenerateCRC();
		GenerateStaticCRC();
	}
}

void CNetworkAssetVerifier::RefreshCRC()
{
	if(!PARAM_networkAssetIgnoreFiles.Get())
	{
		GenerateCRC();
		GenerateStaticCRC();
	}
}

void CNetworkAssetVerifier::GenerateCRC()
{
	//@@: range CNETWORKASSETVERIFIER_GENERATECRC {
	m_CRC = 0;
#if !__FINAL
	if(!PARAM_networkAssetIgnoreModelInfoCount.Get())
	{
		u32 l_modelInfoCount = m_modelInfoCount;
		m_CRC = atDataHash(&l_modelInfoCount, sizeof(l_modelInfoCount), m_CRC);
		gnetDebug1("GenerateCRC :: ModelInfoCount: %u, Rolling: 0x%08x", m_modelInfoCount.Get(), m_CRC.Get());
	}
    else
    {
        gnetDebug1("GenerateCRC :: *Ignoring* ModelInfoCount: %u", m_modelInfoCount.Get());
    }

	if(!PARAM_networkAssetIgnoreScripts.Get())
	{	
		u32 l_overrideScriptHash = m_overrideScriptHash;
		m_CRC = atDataHash(&l_overrideScriptHash, sizeof(l_overrideScriptHash), m_CRC);
		gnetDebug1("GenerateCRC :: Script Hash Override: 0x%08x, Rolling: 0x%08x", m_overrideScriptHash.Get(), m_CRC.Get()); 
	}
    else
    {
        gnetDebug1("GenerateCRC :: *Ignoring* Script Hash Override: 0x%08x", m_overrideScriptHash.Get());
    }
#endif

	if(!PARAM_networkAssetIgnoreInterior.Get())
	{
		u32 l_interiorProxyCount = m_interiorProxyCount;
		u32 l_interiorProxyChecksum = m_interiorProxyChecksum;
		m_CRC = atDataHash(&l_interiorProxyCount, sizeof(l_interiorProxyCount), m_CRC);
		gnetDebug1("GenerateCRC :: Interior Proxy Count: %u, Rolling: 0x%08x", m_interiorProxyCount.Get(), m_CRC.Get()); 
		m_CRC = atDataHash(&l_interiorProxyChecksum, sizeof(l_interiorProxyChecksum), m_CRC);
		gnetDebug1("GenerateCRC :: Interior Proxy Checksum: 0x%08x, Rolling: 0x%08x", m_interiorProxyChecksum.Get(), m_CRC.Get()); 
	}
    else
    {
        gnetDebug1("GenerateCRC :: *Ignoring* Interior Proxy Count: %u,", m_interiorProxyCount.Get());
        gnetDebug1("GenerateCRC :: *Ignoring* Interior Proxy Checksum: 0x%08x", m_interiorProxyChecksum.Get());
    }

	if(!PARAM_networkAssetIgnoreScriptSize.Get())
	{
		u32 l_scriptSize = m_scriptSize;
		u32 l_scriptStoreHash = m_scriptStoreHash;
		m_CRC = atDataHash(&l_scriptSize, sizeof(l_scriptSize), m_CRC);
		gnetDebug1("GenerateCRC :: Script Size: %u, Rolling: 0x%08x", m_scriptSize.Get(), m_CRC.Get()); 
		m_CRC = atDataHash(&l_scriptStoreHash, sizeof(l_scriptStoreHash), m_CRC);
		gnetDebug1("GenerateCRC :: Script Store Hash: 0x%08x, Rolling: 0x%08x", m_scriptStoreHash.Get(), m_CRC.Get()); 
	}
    else
    {
        gnetDebug1("GenerateCRC :: *Ignoring* Script Size: %u", m_scriptSize.Get());
        gnetDebug1("GenerateCRC :: *Ignoring* Script Store Hash: 0x%08x", m_scriptStoreHash.Get());
    }

	if(!PARAM_networkAssetIgnoreFiles.Get())
	{

		u32 l_numFiles = m_numFiles;
		u32 l_fileChecksum = m_fileChecksum;
		m_CRC = atDataHash(&l_numFiles, sizeof(l_numFiles), m_CRC);
		gnetDebug1("GenerateCRC :: Num Files: %u, Rolling: 0x%08x", m_numFiles.Get(), m_CRC.Get()); 
		m_CRC = atDataHash(&l_fileChecksum, sizeof(l_fileChecksum), m_CRC);
		gnetDebug1("GenerateCRC :: File Checksum: 0x%08x, Rolling: 0x%08x", m_fileChecksum.Get(), m_CRC.Get()); 
	}
    else
    {
        gnetDebug1("GenerateCRC :: *Ignoring* Num Files: %u", m_numFiles.Get());
        gnetDebug1("GenerateCRC :: *Ignoring* File Checksum: 0x%08x", m_fileChecksum.Get());
    }

	if(!PARAM_networkAssetIgnoreTunables.Get())
	{
		u32 l_nLocalTunesablesCrc = m_nLocalTunablesCRC;
		m_CRC = atDataHash(&l_nLocalTunesablesCrc , sizeof(l_nLocalTunesablesCrc ), m_CRC);
		gnetDebug1("GenerateCRC :: Tunables CRC: 0x%08x, Rolling: 0x%08x", m_nLocalTunablesCRC.Get(), m_CRC.Get()); 
	}
    else
    {
        gnetDebug1("GenerateCRC :: *Ignoring* Tunables CRC: 0x%08x", m_nLocalTunablesCRC.Get());
    }

	if(IsDiscriminatingBySKU())
	{
		unsigned nJapaneseHash = m_bJapaneseBuild ? 0x7A9A435E : 0;
		m_CRC = atDataHash(&nJapaneseHash, sizeof(nJapaneseHash), m_CRC);
		gnetDebug1("GenerateCRC :: Japanese Build: %s, Rolling: 0x%08x", m_bJapaneseBuild ? "True" : "False", m_CRC.Get()); 
	}
    else
    {
        gnetDebug1("GenerateCRC :: *Ignoring* Japanese Build: %s", m_bJapaneseBuild ? "True" : "False");
    }

	if(!PARAM_networkAssetIgnoreMemoryTamperRestriction.Get())
	{
		u32 l_memoryTamperCRC = m_memoryTamperCRC;
		m_CRC = atDataHash(&l_memoryTamperCRC, sizeof(l_memoryTamperCRC), m_CRC);
		gnetDebug1("GenerateCRC :: Memory Tamper CRC: 0x%08x, Rolling: 0x%08x", m_memoryTamperCRC.Get(), m_CRC.Get());
	}
    else
    {
        gnetDebug1("GenerateCRC :: *Ignoring* Memory Tamper CRC: 0x%08x", m_memoryTamperCRC.Get());
    }

	if(!PARAM_networkAssetIgnoreBGScriptRestriction.Get())
	{
		u32 l_nBackgroundScriptCRC = m_nBackgroundScriptCRC;
		m_CRC = atDataHash(&l_nBackgroundScriptCRC, sizeof(l_nBackgroundScriptCRC), m_CRC);
		gnetDebug1("GenerateCRC :: Background Script CRC: 0x%08x, Rolling: 0x%08x", m_nBackgroundScriptCRC.Get(), m_CRC.Get());
	}
    else
    {
        gnetDebug1("GenerateCRC :: *Ignoring* Background Script CRC: 0x%08x,", m_nBackgroundScriptCRC.Get());
    }

	if(!PARAM_networkAssetIgnoreEnvironmentRestriction.Get())
	{
		u32 l_EnvironmentCRC = m_EnvironmentCRC;
		m_CRC = atDataHash(&l_EnvironmentCRC, sizeof(l_EnvironmentCRC), m_CRC);
		gnetDebug1("GenerateCRC :: Environment CRC: 0x%08x, Rolling: 0x%08x", m_EnvironmentCRC.Get(), m_CRC.Get());
	}
	else
	{
		gnetDebug1("GenerateCRC :: *Ignoring* Environment CRC: 0x%08x,", m_EnvironmentCRC.Get());
	}

#if __FINAL_LOGGING && RSG_SCE
	
	// FUTURE TODO: File paths for PC or Xbox One
	const int MAX_DIRS = 9;
	const char* dirs[MAX_DIRS] = { "/app0/", "/usb0/", "/usb1/", "/usb2/", "/usb3/", "/usb4/", "/usb5/", "/usb6/", "/usb7/" };

	for (int i = 0; i < MAX_DIRS; i++)
	{
		char path[RAGE_MAX_PATH] = {0};
		formatf(path, "%sscripthash.bin", dirs[i]);
		
		const fiDevice* device = fiDevice::GetDevice(path);
		if (device)
		{
			fiHandle h = device->Open(path, true);
			if (h != fiHandleInvalid)
			{
				gnetDebug1("%s was found.", path);

				char bigBuffer[4086] = {0};
				int bytesRead = device->Read(h, bigBuffer, sizeof(bigBuffer));
				if (!bytesRead)
					continue;

				gnetDebug1("Read script hash file: \n%s", bigBuffer);

// 				{
// 					"m_interiorProxyChecksum" : 1,
// 					"m_interiorProxyCount" : 2,
// 					"m_scriptSize" : 3,
// 					"m_scriptRpfHash" : 4,
// 					"m_scriptStoreHash" : 5,
// 					"m_memoryTamperCRC" : 6,
// 					"m_nBackgroundScriptCRC" : 7,
// 					"m_nCRC" : 8
// 				}

#define READ_CRC_FROM_JSON(x)							\
	if (rr.HasMember(#x))								\
	{													\
		RsonReader rVal;								\
		rr.GetMember(#x, &rVal);						\
		unsigned int temp = 0;							\
		rVal.AsUns(temp);								\
		gnetDebug1("Overriding %s: 0x%08x", #x, temp);	\
		x.Set(temp);									\
	}

#define READ_INT_CRC_FROM_JSON(x)						\
	if (rr.HasMember(#x))								\
	{													\
		RsonReader rVal;								\
		rr.GetMember(#x, &rVal);						\
		int temp = 0;									\
		rVal.AsInt(temp);								\
		gnetDebug1("Overriding %s: 0x%08x", #x, temp);	\
		x.Set(temp);									\
	}
				
				RsonReader rr(bigBuffer);
				READ_CRC_FROM_JSON(m_interiorProxyChecksum);
				READ_CRC_FROM_JSON(m_interiorProxyCount);
				READ_CRC_FROM_JSON(m_scriptSize);
				READ_CRC_FROM_JSON(m_scriptRpfHash);
				READ_CRC_FROM_JSON(m_scriptStoreHash);
				READ_CRC_FROM_JSON(m_memoryTamperCRC);
				READ_CRC_FROM_JSON(m_nBackgroundScriptCRC);
				READ_INT_CRC_FROM_JSON(m_CRC);

				break;
			}
			else
			{
				gnetDebug1("%s was not found.", path);
			}
		}
	}
#endif

	gnetDebug1("GenerateCRC :: Result: 0x%08x", m_CRC.Get()); 
	//@@: } CNETWORKASSETVERIFIER_GENERATECRC
}

void CNetworkAssetVerifier::GenerateStaticCRC()
{
	// nothing to do here
	m_StaticCRC = 0;

	// include the environment in the static / early discriminator to prevent any cross talk via invites
	if(!PARAM_networkAssetIgnoreEnvironmentRestriction.Get())
	{
		u32 l_EnvironmentCRC = m_EnvironmentCRC;
		m_StaticCRC = atDataHash(&l_EnvironmentCRC, sizeof(l_EnvironmentCRC), m_StaticCRC);
		gnetDebug1("GenerateStaticCRC :: Environment CRC: 0x%08x, Rolling: 0x%08x", m_EnvironmentCRC.Get(), m_StaticCRC.Get());
	}
	else
	{
		gnetDebug1("GenerateStaticCRC :: *Ignoring* Environment CRC: 0x%08x,", m_EnvironmentCRC.Get());
	}

	gnetDebug1("GenerateStaticCRC :: Result: 0x%08x", m_StaticCRC.Get());
}

void CNetworkAssetVerifier::GenerateFileCRC()
{
	//@@: range CNETWORKASSETVERIFIER_GENERATEFILECRC {
	// reset checksum
	m_fileChecksum = 0;
	m_numFiles = 0;
	atBinaryMap<int, atFinalHashString>& rMap = fiDeviceCrc::GetFilenameCrcMap();
	rMap.FinishInsertion();

	if (!fiDeviceCrc::IsCRCTableConsistent())
	{
		gnetAssertf(false, "CRC table is not consistent! Tamper response code will activate shortly and crash the game!");
		gnetError("CRC table is not consistent! Tamper response code will activate shortly.");

		// boom!
		fwArchetypeManager::ClearArchetypePtrStore();
	}

	for(atBinaryMap<int, atFinalHashString>::Iterator mapIt = rMap.Begin();mapIt!= rMap.End(); ++mapIt)
	{
#if __ASSERT
		const char* fileName = mapIt.GetKey().TryGetCStr();
		Displayf("CRC'D FILES: %s", fileName);
		if(!strncmp(fileName,"dlc",3))
		{
			if(!EXTRACONTENT.GetPartOfCompatPack(fileName)&&strstr(fileName,"content.xml")==NULL)
			{
				gnetAssertf(0,"A CRC is being generated for a DLC file that isn't in a compatibility pack. Make sure your content.xml is set-up correctly, File: %s",fileName);
			}
		}
#endif // __ASSERT
		unsigned nCheckSum = (*mapIt);
		m_fileChecksum = atDataHash(&nCheckSum, sizeof(u32), m_fileChecksum.Get());
		m_numFiles = m_numFiles +1;

		// track each file
		gnetDebug2("GenerateFileCRC :: Adding file %d with checksum 0x%08x (rolling: 0x%08x) - Name: %s", m_numFiles.Get(), nCheckSum, m_fileChecksum.Get(), mapIt.GetKey().TryGetCStr());
	}

	gnetDebug1("GenerateFileCRC :: Generated checksum 0x%08x for %d files", m_fileChecksum.Get(), m_numFiles.Get()); 

	u32 l_numFiles = 0;
	if(PARAM_networkAssetSetFileCount.Get(l_numFiles))
	{
		m_numFiles = l_numFiles;
		gnetDebug1("GenerateFileCRC :: Number of Files Overridden: %u", m_numFiles.Get() ); 
	}
	u32 l_fileCheckSum = 0;
	if(PARAM_networkAssetSetFileChecksum.Get(l_fileCheckSum))
	{
		m_fileChecksum = l_fileCheckSum;
		gnetDebug1("GenerateFileCRC :: File Checksum Overridden: 0x%08x", m_fileChecksum.Get()); 
	}
	//@@: } CNETWORKASSETVERIFIER_GENERATEFILECRC
}

#if !__NO_OUTPUT
void CNetworkAssetVerifier::DumpSyncData()
{
    gnetDebug1("DumpSyncData :: Num Files: %u (Ignore: %s)", m_numFiles.Get(), PARAM_networkAssetIgnoreFiles.Get() ? "True" : "False");
	gnetDebug1("DumpSyncData :: File Checksum: 0x%08x (Ignore: %s)", m_fileChecksum.Get(), PARAM_networkAssetIgnoreFiles.Get() ? "True" : "False");

#if !__FINAL
	gnetDebug1("DumpSyncData :: Num Model Info: %u (Ignore: %s)", m_modelInfoCount.Get(), PARAM_networkAssetIgnoreModelInfoCount.Get() ? "True" : "False");
	gnetDebug1("DumpSyncData :: Script Hash Override: 0x%08x (Ignore: %s)", m_overrideScriptHash.Get(), PARAM_networkAssetIgnoreScripts.Get() ? "True" : "False");
#endif

	gnetDebug1("DumpSyncData :: Script Size: %u (Ignore: %s)", m_scriptSize.Get(), PARAM_networkAssetIgnoreScriptSize.Get() ? "True" : "False");
	gnetDebug1("DumpSyncData :: Script Store Hash: 0x%08x (Ignore: %s)", m_scriptStoreHash.Get(), PARAM_networkAssetIgnoreScriptSize.Get() ? "True" : "False");
	gnetDebug1("DumpSyncData :: Interior Proxy Checksum: 0x%08x (Ignore: %s)", m_interiorProxyChecksum.Get(), PARAM_networkAssetIgnoreInterior.Get() ? "True" : "False");
	gnetDebug1("DumpSyncData :: Interior Proxy Count: %u (Ignore: %s)", m_interiorProxyCount.Get(), PARAM_networkAssetIgnoreInterior.Get() ? "True" : "False");
	gnetDebug1("DumpSyncData :: Tunables CRC: 0x%08x (Ignore: %s)", m_nLocalTunablesCRC.Get(), PARAM_networkAssetIgnoreTunables.Get() ? "True" : "False");
	gnetDebug1("DumpSyncData :: Japanese Build: %s (Ignore: %s)", m_bJapaneseBuild ? "True" : "False", IsDiscriminatingBySKU() ? "False" : "True");
	gnetDebug1("DumpSyncData :: Memory Tamper CRC: 0x%08x (Ignore: %s)", m_memoryTamperCRC.Get(), PARAM_networkAssetIgnoreMemoryTamperRestriction.Get() ? "True" : "False");
	gnetDebug1("DumpSyncData :: Background Script CRC = 0x%08x (Ignore: %s)", m_nBackgroundScriptCRC.Get(), PARAM_networkAssetIgnoreBGScriptRestriction.Get() ? "True" : "False");
	gnetDebug1("DumpSyncData :: Environment CRC = 0x%08x (Ignore: %s)", m_EnvironmentCRC.Get(), PARAM_networkAssetIgnoreEnvironmentRestriction.Get() ? "True" : "False");
	gnetDebug1("DumpSyncData :: CRC: 0x%08x", m_CRC.Get());
}
#endif

bool CNetworkAssetVerifier::Equals(const CNetworkAssetVerifier& other OUTPUT_ONLY(, const char* otherName)) const
{
#if !__FINAL
	if(!PARAM_networkAssetIgnoreModelInfoCount.Get())
	{
		if(other.m_modelInfoCount != m_modelInfoCount)
		{
			gnetError("Different Model Info Count for %s. This: %d, That: %d.", otherName, m_modelInfoCount.Get(), other.m_modelInfoCount.Get());
			return false;
		}
	}

	if(!PARAM_networkAssetIgnoreScripts.Get())
	{
		if(other.m_overrideScriptHash != m_overrideScriptHash)
		{
			gnetError("Different Overridden Script for %s. This: 0x%08x, That: 0x%08x.", otherName, m_overrideScriptHash.Get(), other.m_overrideScriptHash.Get());
			return false;
		}
	}
#endif

	if(!PARAM_networkAssetIgnoreScriptSize.Get())
	{
		if(other.m_scriptSize != m_scriptSize)
		{
			gnetError("Different script.rpf sizes for %s. This: 0x%08x, That: 0x%08x.", otherName, m_scriptSize.Get(), other.m_scriptSize.Get());
			return false;
		}

		if(other.m_scriptStoreHash != m_scriptStoreHash)
		{
			gnetError("Different script.rpf name hashes for %s. This: 0x%08x, That: 0x%08x.", otherName, m_scriptStoreHash.Get(), other.m_scriptStoreHash.Get());
			gnetDebug1("Dumping script store hash generation due to script fingerprint mismatch:");
#if !__NO_OUTPUT
			g_StreamedScripts.CalculateScriptStoreFingerprint(m_scriptRpfHash, true);
#endif
			return false;
		}
	}

	if (!PARAM_networkAssetIgnoreInterior.Get())
	{
		if(other.m_interiorProxyCount != m_interiorProxyCount)
		{
			gnetError("Different Interior Proxy Count for %s. This: %u, That: %u.", otherName, m_interiorProxyCount.Get(), other.m_interiorProxyCount.Get());
			return false;
		}

		if(other.m_interiorProxyChecksum != m_interiorProxyChecksum)
		{
			gnetError("Different Interior Proxy Checksum for %s. This: 0x%08x, That: 0x%08x.", otherName, m_interiorProxyChecksum.Get(), other.m_interiorProxyChecksum.Get());
#if !__FINAL
			gnetDebug1("Dumping interior proxy infos due to interior proxy checksum mismatch");
			CInteriorProxy::DumpInteriorProxyInfo();
#endif
			gnetAssertf(false, "Network interior sync issues likely. Proxy checksum fail for %s. This: 0x%08x, That: 0x%08x.", otherName, m_interiorProxyChecksum.Get(), other.m_interiorProxyChecksum.Get());
			return false;
		}
	}
	//@@: range CNETWORKASSETVERIFIER_EQUALS_FILE_CRC_CHECK {

	if (!PARAM_networkAssetIgnoreFiles.Get())
	{
		if(other.m_numFiles == m_numFiles)
		{
			if(other.m_fileChecksum != m_fileChecksum)
			{
				gnetError("File CRC differs for %s!. This: 0x%08x, That: 0x%08x.", otherName, m_fileChecksum.Get(), other.m_fileChecksum.Get());
				return false;
			}
		}
		else
		{
			gnetError("Num files differ for %s. This: %d, That: %d.", otherName, m_numFiles.Get(), other.m_numFiles.Get());
			return false;
		}
	}

	//@@: } CNETWORKASSETVERIFIER_EQUALS_FILE_CRC_CHECK

	//@@: range CNETWORKASSETVERIFIER_EQUALS_TUNABLES_CRC_CHECK {
	if(!PARAM_networkAssetIgnoreTunables.Get())
	{
		if(other.m_nLocalTunablesCRC != m_nLocalTunablesCRC)
		{
			gnetError("Different tunables for %s. This: 0x%08x, That: 0x%08x.", otherName, m_nLocalTunablesCRC.Get(), other.m_nLocalTunablesCRC.Get());
			return false;
		}
	} 
	//@@: } CNETWORKASSETVERIFIER_EQUALS_TUNABLES_CRC_CHECK

	if(IsDiscriminatingBySKU())
	{
		if(other.m_bJapaneseBuild != m_bJapaneseBuild)
		{
			gnetError("Different SKU for %s. This: %s, That: %s.", otherName, m_bJapaneseBuild ? "Japanese" : "Rest of World", other.m_bJapaneseBuild ? "Japanese" : "Rest of World");
			return false;
		}
	}

	if(!PARAM_networkAssetIgnoreMemoryTamperRestriction.Get())
	{
		if(other.m_memoryTamperCRC != m_memoryTamperCRC)
		{
			gnetError("Different memory tamper values for %s. This: 0x%08x, That: 0x%08x.", otherName, m_memoryTamperCRC.Get(), other.m_memoryTamperCRC.Get());
			return false;
		}
	}

	if(!PARAM_networkAssetIgnoreBGScriptRestriction.Get())
	{
		if(other.m_nBackgroundScriptCRC.Get() != m_nBackgroundScriptCRC.Get())
		{
			gnetError("Different background script CRC for %s. This: 0x%08x, That: 0x%08x.", otherName, m_nBackgroundScriptCRC.Get(), other.m_nBackgroundScriptCRC.Get());
			return false;
		}
	}

	if(!PARAM_networkAssetIgnoreEnvironmentRestriction.Get())
	{
		if(other.m_EnvironmentCRC.Get() != m_EnvironmentCRC.Get())
		{
			gnetError("Different environment CRC for %s. This: 0x%08x, That: 0x%08x.", otherName, m_EnvironmentCRC.Get(), other.m_EnvironmentCRC.Get());
			return false;
		}
	}

	// if we get this far, someone has added something to the CRC but not synced it - assert to alert this
	if(!gnetVerifyf(other.m_CRC.Get() == m_CRC.Get(), "%s joined with a different CRC, check that command lines starting -netAsset* match!", otherName))
	{
		gnetError("Different CRC for %s. This: 0x%08x, That: 0x%08x.", otherName, m_CRC.Get(), other.m_CRC.Get());
		return false;
	}

	return true;
}

#if !__FINAL
void CNetworkAssetVerifier::CountModelInfos()
{
	m_modelInfoCount = 0;

	const u32 maxModelInfos = CModelInfo::GetMaxModelInfos();
	for (u32 i=fwArchetypeManager::GetStartArchetypeIndex(); i<maxModelInfos; i++)
	{
		const CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(i)));

		// Only take into account permanent archetypes
		if (pModelInfo && !pModelInfo->IsStreamedArchetype())
		{
			fwModelId id = fwArchetypeManager::LookupModelId(pModelInfo);

			// Skip DLC model infos
			if(id.GetMapTypeDefIndex() == fwFactory::EXTRA)
				continue;			

			m_modelInfoCount = m_modelInfoCount +1;
		}
	}

	gnetDebug1("CountModelInfos :: Model Info count: %u", m_modelInfoCount.Get());
	u32 l_modelInfoCount = 0;
	if(PARAM_networkAssetSetModelInfoCount.Get(l_modelInfoCount))
	{
		m_modelInfoCount = l_modelInfoCount;
		gnetDebug1("CountModelInfos :: Model Info Count Overridden: %u", m_modelInfoCount.Get()); 
	}
}

void CNetworkAssetVerifier::CacheOverridenScriptHash()
{
	m_overrideScriptHash = 0;

	XPARAM(override_script);
	const char *overrideScriptRpfName = NULL;
	if( PARAM_override_script.Get(overrideScriptRpfName) )
	{
		m_overrideScriptHash = atStringHash(overrideScriptRpfName);
	}
	gnetDebug1("CacheOverridenScriptHash :: Overridden scripts hash = 0x%08x", m_overrideScriptHash.Get());
	u32 l_overrideScriptHash = 0;
	if(PARAM_networkAssetSetOverrideScriptHash.Get(l_overrideScriptHash))
	{
		m_overrideScriptHash = l_overrideScriptHash;
		gnetDebug1("CacheOverridenScriptHash :: Overrridden Script Hash Overridden: 0x%08x", m_overrideScriptHash.Get()); 
	}
}
#endif

void CNetworkAssetVerifier::GenerateInteriorProxyData()
{
	m_interiorProxyChecksum = CInteriorProxy::GetChecksum();
	m_interiorProxyCount = CInteriorProxy::GetCheckSumCount();

	u32 l_interiorProxyChecksum = 0;
	u32 l_interiorProxyCount = 0;
	gnetDebug1("GenerateInteriorProxyData :: Interior Proxy Count: %u", m_interiorProxyCount.Get());
	gnetDebug1("GenerateInteriorProxyData :: Interior Proxy Checksum: 0x%08x", m_interiorProxyChecksum.Get());
	
	// overrides
	if(PARAM_networkAssetSetInteriorCount.Get(l_interiorProxyCount))
	{
		m_interiorProxyCount = l_interiorProxyCount;
		gnetDebug1("GenerateInteriorProxyData :: Interior Proxy Count Overridden: %u", m_interiorProxyCount.Get()); 

	}
	if(PARAM_networkAssetSetInteriorChecksum.Get(l_interiorProxyChecksum))
	{
		m_interiorProxyChecksum = l_interiorProxyChecksum;
		gnetDebug1("GenerateInteriorProxyData :: Interior Proxy Checksum Overridden: 0x%08x", m_interiorProxyChecksum.Get()); 
	}
}

void CNetworkAssetVerifier::GetScriptSize()
{
	gnetAssertf(!CFileMgr::IsValidFileHandle(CFileMgr::OpenFile("platform:/patch/levels/gta5/script/script.rpf")),"There is a script.rpf in the next-gen patch directory now, update path in NetworkAssetVerifier.cpp");
	char scriptRpfName[256] = "platform:/levels/gta5/script/script.rpf";

	atHashString scriptRpfHash = ATSTRINGHASH("platform:/levels/gta5/script/script.rpf",0x51de1836);

	// Handle overridden script
	XPARAM(override_script);
	const char *overrideScriptRpfName = NULL;
	if( PARAM_override_script.Get(overrideScriptRpfName) )
	{
		formatf(scriptRpfName, "platform:/levels/gta5/script/%s.rpf", overrideScriptRpfName);
		scriptRpfHash = atStringHash(scriptRpfName);
	}

	FileHandle fileHandle = CFileMgr::OpenFile(scriptRpfName);

	u32 fileSize = 0;

	if (!CFileMgr::IsValidFileHandle(fileHandle))
	{
		gnetAssertf(0, "GetScriptSize :: Problem opening file: %s", scriptRpfName);
	}
	else
	{
		fileSize = CFileMgr::GetTotalSize(fileHandle);

		// close the file
		CFileMgr::CloseFile(fileHandle);
	}

	m_scriptSize = fileSize;
	m_scriptStoreHash = g_StreamedScripts.CalculateScriptStoreFingerprint(m_scriptRpfHash, true);

 	gnetDebug1("GetScriptSize :: Script Size: %u", m_scriptSize.Get()); 
 	gnetDebug1("GetScriptSize :: Script RPF Hash: 0x%08x", m_scriptRpfHash.Get());
 	gnetDebug1("GetScriptSize :: Script Store Hash: 0x%08x", m_scriptStoreHash.Get());  

	// overrides
	u32 l_scriptSize = 0;
	u32 l_scriptStoreHash = 0;
	if(PARAM_networkAssetSetScriptSize.Get(l_scriptSize))
	{
		gnetDebug1("GetScriptSize :: Script Size Overridden: %u", m_scriptSize.Get()); 
		m_scriptSize = l_scriptSize;
	}
	if(PARAM_networkAssetSetScriptStoreHash.Get(l_scriptStoreHash))
	{
		m_scriptStoreHash = l_scriptStoreHash;
		gnetDebug1("GetScriptSize :: Script Store Hash Overridden: 0x%08x", m_scriptStoreHash.Get()); 
	}
}

void CNetworkAssetVerifier::GenerateLocalTunablesCRC()
{
	// get the local tunables CRC
	m_nLocalTunablesCRC = Tunables::GetInstance().GetLocalCRC();
	gnetDebug1("GenerateLocalTunablesCRC :: Tunables CRC: 0x%08x", m_nLocalTunablesCRC.Get()); 
	u32 l_nLocalTunablesCRC = 0;
	if(PARAM_networkAssetSetTunablesCRC.Get(l_nLocalTunablesCRC))
	{
		m_nLocalTunablesCRC = l_nLocalTunablesCRC;
		gnetDebug1("GenerateLocalTunablesCRC :: Tunables CRC Overridden: 0x%08x", m_nLocalTunablesCRC.Get()); 
	}
}

void CNetworkAssetVerifier::SetMemoryTamperCRC(u32 input)
{
	m_memoryTamperCRC = input;
	gnetDebug1("GenerateMemoryTamperCRC :: Memory tamper CRC: 0x%08x", m_memoryTamperCRC.Get());
	u32 l_memoryTamperCRC = 0;
	if(PARAM_networkAssetSetMemoryTamperCRC.Get(l_memoryTamperCRC))
	{
		m_memoryTamperCRC = l_memoryTamperCRC; 
		gnetDebug1("GenerateMemoryTamperCRC :: Memory tamper CRC Overridden: 0x%08x", m_memoryTamperCRC.Get()); 
	}
}

void CNetworkAssetVerifier::GenerateBackgroundScriptCRC()
{
	m_nBackgroundScriptCRC = BackgroundScripts::GetBGScriptCloudHash();
	gnetDebug1("GenerateBackgroundScriptCRC :: Background Script CRC: 0x%08x", m_nBackgroundScriptCRC.Get());
	u32 l_nBackgroundScriptCRC = 0;
	if(PARAM_networkAssetSetBackgroundScriptCRC.Get(l_nBackgroundScriptCRC))
	{
		m_nBackgroundScriptCRC = l_nBackgroundScriptCRC;
		gnetDebug1("GenerateBackgroundScriptCRC :: Background Script CRC Overridden: 0x%08x", m_nBackgroundScriptCRC.Get()); 
	}
}

void CNetworkAssetVerifier::GenerateEnvironmentCRC()
{
	m_EnvironmentCRC = 0;

	// use both the ROS and Native environments
	const u32 rosEnvironment = static_cast<u32>(g_rlTitleId ? g_rlTitleId->m_RosTitleId.GetEnvironment() : RLROS_ENV_UNKNOWN);
	const u32 nativeEnvironment = static_cast<u32>(rlGetNativeEnvironment());
	const u32 isStagingEnv = rlIsUsingStagingEnvironment() ? 1 : 0;
	
	m_EnvironmentCRC = atDataHash(&rosEnvironment, sizeof(rosEnvironment), m_EnvironmentCRC.Get());
	m_EnvironmentCRC = atDataHash(&nativeEnvironment, sizeof(nativeEnvironment), m_EnvironmentCRC.Get());
	m_EnvironmentCRC = atDataHash(&isStagingEnv, sizeof(isStagingEnv), m_EnvironmentCRC.Get());

	gnetDebug1("GenerateEnvironmentCRC :: Environment CRC: 0x%08x (ROS: %s, Native: %s, Staging: %s)", 
		m_EnvironmentCRC.Get(), 
		rlRosGetEnvironmentAsString(static_cast<rlRosEnvironment>(rosEnvironment)), 
		rlGetEnvironmentString(static_cast<rlEnvironment>(nativeEnvironment)),
		isStagingEnv == 1 ? "True" : "False");

	u32 l_EnvironmentCRC = 0;
	if(PARAM_networkAssetSetEnvironmentCRC.Get(l_EnvironmentCRC))
	{
		m_EnvironmentCRC = l_EnvironmentCRC;
		gnetDebug1("GenerateEnvironmentCRC :: Environment CRC Overridden: 0x%08x", m_EnvironmentCRC.Get());
	}
}

void CNetworkAssetVerifier::GenerateJapaneseBuild()
{
	if(sysAppContent::IsJapaneseBuild())
		m_bJapaneseBuild = true;

	gnetDebug1("GenerateJapaneseBuild :: Japanese Build: %s", m_bJapaneseBuild ? "True" : "False"); 
	
#if !__FINAL
	unsigned nJapaneseBuild = 0;
	if(PARAM_networkAssetSetJapaneseBuild.Get(nJapaneseBuild))
	{
		m_bJapaneseBuild = (nJapaneseBuild != 0);
		gnetDebug1("GenerateJapaneseBuild :: Japanese Build Overridden: %s", m_bJapaneseBuild ? "True" : "False"); 
	}
#endif
}
