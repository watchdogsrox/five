//
// name:        NetworkAssetVerifier.h
// description: Network asset verifier: checksums important asset files to ensure that all machines involved in a session are running with the same data.
// written by:  John Gurney
//

#ifndef NETWORK_ASSET_VERIFIER_H
#define NETWORK_ASSET_VERIFIER_H

#include "data/bitbuffer.h"
#include "file/device_crc.h"
#include "math/random.h"
#include "fwmaths/random.h"
#include "security/obfuscatedtypes.h"

class CNetworkAssetVerifier
{

public:

	CNetworkAssetVerifier();

	void Init();
	void RefreshFileCRC();
	void RefreshEnvironmentCRC();
	void RefreshCRC();
	void ClobberTamperCrcAndRefresh() { SetMemoryTamperCRC(fwRandom::GetRandomNumber()); RefreshCRC();};
	unsigned GetCRC() { return m_CRC; }
	unsigned GetStaticCRC() { return m_StaticCRC; }

	bool IsDiscriminatingBySKU() const;
	bool IsJapaneseBuild() { return m_bJapaneseBuild; }

	bool Serialise(datImportBuffer& bb)
	{
		bool bSuccess = true;

		u32 l_fileChecksum = m_fileChecksum.Get();
		u32 l_numFiles = m_numFiles.Get();
#if !__FINAL
		u32 l_modelInfoCount = m_modelInfoCount.Get();
		u32 l_overrideScriptHash = m_overrideScriptHash.Get();
#endif
		u32 l_interiorProxyChecksum = m_interiorProxyChecksum.Get();
		u32 l_interiorProxyCount = m_interiorProxyCount.Get();
		u32 l_nLocalTunablesCRC = m_nLocalTunablesCRC.Get();
		u32 l_scriptSize = m_scriptSize.Get();
		u32 l_scriptStoreHash = m_scriptStoreHash.Get();
		u32 l_memoryTamperCRC = m_memoryTamperCRC.Get();
		u32 l_nBackgroundScriptCRC = m_nBackgroundScriptCRC.Get();
		u32 l_EnvironmentCRC = m_EnvironmentCRC.Get();
		u32 l_nCRC = m_CRC.Get(); 

		bSuccess &= bb.SerUns(l_numFiles, 16);
		bSuccess &= bb.SerUns(l_fileChecksum, 32);
							  
#if !__FINAL				 
		bSuccess &= bb.SerUns(l_modelInfoCount, 32);
		bSuccess &= bb.SerUns(l_overrideScriptHash, 32);
#endif						  
		bSuccess &= bb.SerUns(l_scriptSize, 32);
		bSuccess &= bb.SerUns(l_scriptStoreHash, 32);
		bSuccess &= bb.SerUns(l_interiorProxyChecksum, 32);
		bSuccess &= bb.SerUns(l_interiorProxyCount, 32);
		bSuccess &= bb.SerUns(l_nLocalTunablesCRC, 32);
		bSuccess &= bb.SerBool(m_bJapaneseBuild);
		bSuccess &= bb.SerUns(l_memoryTamperCRC, 32);
		bSuccess &= bb.SerUns(l_nBackgroundScriptCRC, 32);
		bSuccess &= bb.SerUns(l_EnvironmentCRC, 32);
		bSuccess &= bb.SerUns(l_nCRC, 32);

		m_fileChecksum.Set(l_fileChecksum);
		m_numFiles.Set(l_numFiles);
#if !__FINAL
		m_modelInfoCount.Set(l_modelInfoCount);
		m_overrideScriptHash.Set(l_overrideScriptHash);
#endif
		m_interiorProxyChecksum.Set(l_interiorProxyChecksum);
		m_interiorProxyCount.Set(l_interiorProxyCount);
		m_nLocalTunablesCRC.Set(l_nLocalTunablesCRC);
		m_scriptSize.Set(l_scriptSize);
		m_scriptStoreHash.Set(l_scriptStoreHash);
		m_memoryTamperCRC.Set(l_memoryTamperCRC);
		m_nBackgroundScriptCRC.Set(l_nBackgroundScriptCRC);
		m_EnvironmentCRC.Set(l_EnvironmentCRC);
		m_CRC.Set(l_nCRC);

		return bSuccess;
	}

	bool Serialise(datExportBuffer& bb) const
	{
		bool bSuccess = true;

		u32 l_fileChecksum = m_fileChecksum.Get();
		u32 l_numFiles = m_numFiles.Get();
#if !__FINAL
		u32 l_modelInfoCount = m_modelInfoCount.Get();
		u32 l_overrideScriptHash = m_overrideScriptHash.Get();
#endif
		u32 l_interiorProxyChecksum = m_interiorProxyChecksum.Get();
		u32 l_interiorProxyCount = m_interiorProxyCount.Get();
		u32 l_nLocalTunablesCRC = m_nLocalTunablesCRC.Get();
		u32 l_scriptSize = m_scriptSize.Get();
		u32 l_scriptStoreHash = m_scriptStoreHash.Get();
		u32 l_memoryTamperCRC = m_memoryTamperCRC.Get();
		u32 l_nBackgroundScriptCRC = m_nBackgroundScriptCRC.Get();
		u32 l_EnvironmentCRC = m_EnvironmentCRC.Get();
		u32 l_nCRC = m_CRC.Get(); 

		bSuccess &= bb.SerUns(l_numFiles, 16);
		bSuccess &= bb.SerUns(l_fileChecksum, 32);

#if !__FINAL				 
		bSuccess &= bb.SerUns(l_modelInfoCount, 32);
		bSuccess &= bb.SerUns(l_overrideScriptHash, 32);
#endif						  
		bSuccess &= bb.SerUns(l_scriptSize, 32);
		bSuccess &= bb.SerUns(l_scriptStoreHash, 32);
		bSuccess &= bb.SerUns(l_interiorProxyChecksum, 32);
		bSuccess &= bb.SerUns(l_interiorProxyCount, 32);
		bSuccess &= bb.SerUns(l_nLocalTunablesCRC, 32);
		bSuccess &= bb.SerBool(m_bJapaneseBuild);
		bSuccess &= bb.SerUns(l_memoryTamperCRC, 32);
		bSuccess &= bb.SerUns(l_nBackgroundScriptCRC, 32);
		bSuccess &= bb.SerUns(l_EnvironmentCRC, 32);
		bSuccess &= bb.SerUns(l_nCRC, 32);

		return bSuccess;
	}

	bool Equals(const CNetworkAssetVerifier& other OUTPUT_ONLY(, const char* otherName)) const;

#if !__NO_OUTPUT
	void DumpSyncData();
#endif

private:

#if !__FINAL
	void CountModelInfos();
	void CacheOverridenScriptHash();
#endif

	void GenerateFileCRC();
	void GenerateInteriorProxyData();
	void GetScriptSize();
	void GenerateLocalTunablesCRC();
	void SetMemoryTamperCRC(u32);
	void GenerateBackgroundScriptCRC();
	void GenerateJapaneseBuild();
	void GenerateEnvironmentCRC();

	void GenerateStaticCRC();
	void GenerateCRC();

	bool m_bJapaneseBuild;
	sysObfuscated<u32> m_fileChecksum;
	sysObfuscated<u32> m_numFiles;
#if !__FINAL
	sysObfuscated<u32> m_modelInfoCount;
	sysObfuscated<u32> m_overrideScriptHash;
#endif
	sysObfuscated<u32> m_interiorProxyChecksum;
	sysObfuscated<u32> m_interiorProxyCount;
	sysObfuscated<u32> m_nLocalTunablesCRC;
	sysObfuscated<u32> m_scriptSize;
	sysObfuscated<u32> m_scriptStoreHash;
	sysObfuscated<u32> m_scriptRpfHash;
	sysObfuscated<u32> m_memoryTamperCRC;
	sysObfuscated<u32> m_nBackgroundScriptCRC;
	sysObfuscated<u32> m_EnvironmentCRC;

	sysObfuscated<int> m_CRC;
	sysObfuscated<int> m_StaticCRC;
 };


#endif  // NETWORK_ASSET_VERIFIER_H
