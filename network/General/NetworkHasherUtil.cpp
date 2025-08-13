//
// name:        NetworkHasherUtil.cpp
// description: Helper used to easily crc/hash common data stuff, like PAR_PARSABLE instances or whole files
//

// Standard include
#include "Network/General/NetworkHasherUtil.h"

// Rage includes
#include "file/packfile.h"
#include "profile/timebars.h"

// Framework include
#include "fwnet/neteventmgr.h"
#include "fwutil/KeyGen.h"

// Game includes
#include "Pickups/Data/PickupDataManager.h"
#include "Scene/DataFileMgr.h"
#if __BANK
#include "script\commands_network.h"
#endif
#include "Network\Cloud\Tunables.h"
#include "Network/events/NetworkEventTypes.h"
#include "Network/Live/NetworkRemoteCheaterDetector.h"
#include "script\streamedscripts.h"
#include "stats/StatsMgr.h"
#include "System/ThreadPriorities.h"
#include "Task/System/Tuning.h"
#include "Weapons/Explosion.h"
#include "Weapons/Info/WeaponInfoManager.h"

RAGE_DEFINE_CHANNEL(hasher, rage::DIAG_SEVERITY_DEBUG3, rage::DIAG_SEVERITY_WARNING, rage::DIAG_SEVERITY_ASSERT, rage::CHANNEL_POSIX_ON)

//! Buffer read size used when reading and hashing any file content
static const u32 FILE_HASHER_READ_BUFFER_SIZE = 2 * 1024;

// ================================================================================================================
// HASHER VISITOR CLASS
// ================================================================================================================

void parHasherVisitor::SimpleMember(parPtrToMember ptrToMember, parMemberSimple& metadata)
{
 	m_ComputedHash = fwKeyGen::AppendDataToKey(m_ComputedHash, (const char*)ptrToMember, (int)metadata.GetSize());
}

void parHasherVisitor::VectorMember(parPtrToMember ptrToMember,	parMemberVector& metadata)
{
	m_ComputedHash = fwKeyGen::AppendDataToKey(m_ComputedHash, (const char*)ptrToMember, (int)metadata.GetSize());
}

void parHasherVisitor::Vec3VMember(Vec3V& data, parMemberVector& UNUSED_PARAM(metadata))
{
	m_ComputedHash = fwKeyGen::AppendDataToKey(m_ComputedHash, (const char*)&data, 3 * sizeof(float));
}

void parHasherVisitor::Vector3Member(Vector3& data,	parMemberVector& UNUSED_PARAM(metadata))
{
	m_ComputedHash = fwKeyGen::AppendDataToKey(m_ComputedHash, (const char*)&data, 3 * sizeof(float));
}

void parHasherVisitor::MatrixMember(parPtrToMember ptrToMember,	parMemberMatrix& metadata)
{
	m_ComputedHash = fwKeyGen::AppendDataToKey(m_ComputedHash, (const char*)ptrToMember, (int)metadata.GetSize());
}

void parHasherVisitor::EnumMember(int& UNUSED_PARAM(data), parMemberEnum& metadata)
{
	// 'data' could be a temporary. Use GetMemberFromStruct to get the real address (c&p from rage visitor.cpp)
	m_ComputedHash = fwKeyGen::AppendDataToKey(m_ComputedHash, &metadata.GetMemberFromStruct<char>(m_ContainingStructureAddress), (int)metadata.GetSize());
}

void parHasherVisitor::BitsetMember(parPtrToMember ptrToMember, parPtrToArray UNUSED_PARAM(ptrToBits), size_t UNUSED_PARAM(numBits), parMemberBitset& metadata)
{
	m_ComputedHash = fwKeyGen::AppendDataToKey(m_ComputedHash, (const char*)ptrToMember, (int)metadata.GetSize());
}

void parHasherVisitor::StringMember(parPtrToMember UNUSED_PARAM(ptrToMember), const char* ptrToString, parMemberString& UNUSED_PARAM(metadata))
{
	if(ptrToString)
	{
		m_ComputedHash = fwKeyGen::AppendStringToKey(m_ComputedHash, ptrToString);
	}
}

void parHasherVisitor::ExternalPointerMember(void*& data, parMemberStruct& metadata)
{
	if(data)
	{
		const char *ptrToString = NULL;
		if( metadata.GetSubtype() == parMemberStructSubType::SUBTYPE_EXTERNAL_NAMED_POINTER
			|| metadata.GetSubtype() == parMemberStructSubType::SUBTYPE_EXTERNAL_NAMED_POINTER_USERNULL )
		{
			ptrToString = metadata.GetData()->m_PtrToNameCB(data);
		}

		if(ptrToString)
		{
			m_ComputedHash = fwKeyGen::AppendStringToKey(m_ComputedHash, ptrToString);
		}
	}
}

bool parHasherVisitor::BeginStructMember(parPtrToStructure UNUSED_PARAM(structAddr), parMemberStruct& UNUSED_PARAM(metadata))
{
	return true;
}

bool parHasherVisitor::BeginArrayMember(parPtrToMember UNUSED_PARAM(ptrToMember), parPtrToArray arrayContents, size_t numElements, parMemberArray& UNUSED_PARAM(metadata))
{
	return arrayContents && numElements > 0;
}

bool parHasherVisitor::BeginMapMember(parPtrToStructure UNUSED_PARAM(structAddr),	parMemberMap& UNUSED_PARAM(metadata) )
{
	return true;
}

class parHasherVisitorHT : public parHasherVisitor
{
public:
	parHasherVisitorHT(u32 initValue) : parHasherVisitor(initValue) {};

	bool BeginStructMember(parPtrToStructure UNUSED_PARAM(structAddr), parMemberStruct& UNUSED_PARAM(metadata))
	{
		return !NetworkHasherUtil::IsHashingThreadClosing();
	}

	bool BeginMapMember(parPtrToStructure UNUSED_PARAM(structAddr),	parMemberMap& UNUSED_PARAM(metadata))
	{
		return !NetworkHasherUtil::IsHashingThreadClosing();
	}

	bool BeginArrayMember(parPtrToMember ptrToMember, parPtrToArray arrayContents, size_t numElements, parMemberArray& metadata)
	{
		return parHasherVisitor::BeginArrayMember(ptrToMember, arrayContents, numElements, metadata) && 
			!NetworkHasherUtil::IsHashingThreadClosing();
	}
};

// ================================================================================================================
// HASHING HELPERS
// ================================================================================================================

template<typename _Type>
u32 GetParsableStructHashHT(_Type& instance, u32 value)
{
	parHasherVisitorHT parHaser(value);
	parHaser.Visit(instance);
	return parHaser.GetComputedHash();
}

u32 NetworkHasherUtil::GetTuningDataHash(u32 tuningPackedIndexes, u32 value)
{
	const int uTuningGroupIdx = (tuningPackedIndexes >> 16) & MAX_UINT16;
	const int uTuningItemIdx = tuningPackedIndexes & MAX_UINT16;

	if( const CTuning* pTuning = CTuningManager::GetTuningFromIndexes(uTuningGroupIdx, uTuningItemIdx) )
	{
		value = GetParsableStructHashHT(*const_cast<CTuning*>(pTuning), value);
	}

	return value;
}

u32 NetworkHasherUtil::GetItemInfoDataHash(u32 itemInfoHash, u32 value)
{
	if( const CItemInfo* pItemInfo = CWeaponInfoManager::GetInfo(itemInfoHash) )
	{
		value = GetParsableStructHashHT(*const_cast<CItemInfo*>(pItemInfo), value);
	}

	return value;
}

u32 NetworkHasherUtil::GetPickupInfoDataHash(u32 pickupHash, u32 value)
{
	if( const CPickupData* pPickupData = CPickupDataManager::GetPickupData(pickupHash) )
	{
		value = GetParsableStructHashHT(*const_cast<CPickupData*>(pPickupData), value);
	}

	return value;
}

u32 NetworkHasherUtil::GetExplosionInfoDataHash(u32 expTag, u32 value)
{
	if(expTag < CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetSize())
	{
		CExplosionTagData& explosionData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData( static_cast<eExplosionTag>(expTag) );
		value = GetParsableStructHashHT(explosionData, value);
	}

	return value;
}

u32 NetworkHasherUtil::GetDataFileContentsHash(u32 dataFileHash, u32 value)
{
	if( const CDataFileMgr::DataFile *pDataFile = DATAFILEMGR.FindDataFile(dataFileHash) )
	{
		if( DATAFILEMGR.IsValid(pDataFile) )
		{
			return GetGenericFileHash(pDataFile->m_filename, value);
		}
	}

	return value;
}

u32 NetworkHasherUtil::GetGenericFileHash(const char* file, u32 value)
{
	const FileHandle fileHandle = CFileMgr::OpenFile(file);

	if ( hasherVerifyf(CFileMgr::IsValidFileHandle(fileHandle), "NetworkHasherUtil::GetGenericFileHash couldn't open %s", file) )
	{
		const u32 fileSize = CFileMgr::GetTotalSize(fileHandle);
		u32 filePos = 0;

		char buffer[FILE_HASHER_READ_BUFFER_SIZE];

		while (filePos < fileSize && !NetworkHasherUtil::IsHashingThreadClosing())
		{
			const u32 bufferDataSize = MIN(FILE_HASHER_READ_BUFFER_SIZE, fileSize-filePos);
			const u32 dataRead = CFileMgr::Read(fileHandle, buffer, bufferDataSize);

			hasherAssertf(bufferDataSize==dataRead, "Unexpected different data read");

			value = fwKeyGen::AppendDataToKey(value, buffer, dataRead);

			filePos += dataRead;
		}

		CFileMgr::CloseFile(fileHandle);
	}

	return value;
}

u32 NetworkHasherUtil::GetGenericResourceHash(const strStreamingInfo* pStrInfo, u32 value)
{
	hasherAssertf(pStrInfo, "Who called GetGenericResourceHash with an empty input? Game crashing in 3..2..1..");

	const strFileHandle reFileHandle = pStrInfo->GetHandle();
	if( hasherVerifyf(strIsValidHandle(reFileHandle), "GetResourcedScriptHash failed obtaining handler") )
	{
		const fiCollection *device = fiCollection::GetCollection(reFileHandle);
		const fiPackEntry &resource = device->GetEntryInfo(reFileHandle);

		if ( hasherVerifyf(device->IsPackfile() && fiIsValidHandle(static_cast<const fiPackfile*>(device)->GetPackfileHandle()), "GetGenericResourceHash got an invalid device") )
		{
			char buffer[FILE_HASHER_READ_BUFFER_SIZE];

			const u32 fileSize = resource.GetConsumedSize();
			u32 filePos = 0;

			while (filePos < fileSize && !NetworkHasherUtil::IsHashingThreadClosing())
			{
				const u32 dataRead = device->ReadBulk(static_cast<const fiPackfile*>(device)->GetPackfileHandle(), resource.GetFileOffset(), buffer, FILE_HASHER_READ_BUFFER_SIZE);

				hasherAssertf(dataRead>0, "Unexpected different data read");

				value = fwKeyGen::AppendDataToKey(value, buffer, dataRead);

				filePos += dataRead;
			}
		}
	}

	return value;
}

u32 NetworkHasherUtil::GetResourcedScriptHash(u32 scriptNameHash, u32 value)
{
	const strLocalIndex id = g_StreamedScripts.FindSlotFromHashKey(scriptNameHash);
	if ( hasherVerifyf(id.IsValid(), "GetResourcedScriptHash couldn't find script resource hashed 0x%X", scriptNameHash) )
	{
		const strStreamingInfo* pStrInfo = strStreamingEngine::GetInfo().GetStreamingInfo(g_StreamedScripts.GetStreamingIndex(id));
		if( hasherVerifyf(pStrInfo, "GetResourcedScriptHash failed retrieving streamingInfo for script 0x%X", scriptNameHash) )
		{
			return GetGenericResourceHash(pStrInfo, value);
		}
	}

	return value;
}

u32 NetworkHasherUtil::GetRequiredHash(eHASHABLE_SYSTEM eSystemToCheck, u32 uSpecificCheck, u32 initValue)
{
	switch(eSystemToCheck)
	{
	case CRC_TUNING_SYSTEM:
		return GetTuningDataHash(uSpecificCheck, initValue);

	case CRC_ITEM_INFO_DATA:
		return GetItemInfoDataHash(uSpecificCheck, initValue);

	case CRC_PICKUP_INFO_DATA:
		return GetPickupInfoDataHash(uSpecificCheck, initValue);

	case CRC_EXPLOSION_TAG_DATA:
		return GetExplosionInfoDataHash(uSpecificCheck, initValue);

	case CRC_DATA_FILE_CONTENTS:
		return GetDataFileContentsHash(uSpecificCheck, initValue);

	case CRC_SCRIPT_RESOURCE:
		return GetResourcedScriptHash(uSpecificCheck, initValue);

	default:
		gnetAssertf(0, "NetworkHasherUtil::GetRequiredHash: Found UNHANDLED hashable system [%d]", eSystemToCheck);
	}

	return 0;
}

u32 NetworkHasherUtil::CalculateHashInternal(const rlGamerId& requestingPlayer, const rlGamerId& requestedPlayer, eHASHABLE_SYSTEM eSystemToCheck, u32 uSpecificCheck, const char* fileNameToHash)
{
	u32 uChecksum = MAX_UINT32;

	// Start hash with who requested the CRC
	rage::u64 playerId = requestingPlayer.GetId();
	uChecksum = fwKeyGen::AppendDataToKey(uChecksum, (const char *)&playerId, sizeof(playerId));

	// Mix it with the whole asked CRC
	uChecksum = (eSystemToCheck != NetworkHasherUtil::CRC_GENERIC_FILE_CONTENTS) ? 
		GetRequiredHash(eSystemToCheck, uSpecificCheck, uChecksum) : GetGenericFileHash(fileNameToHash, uChecksum);

	// Finish it with who has been requested. In this way every crc is player-and-runtime dependent, preventing man in the middle, redirecting, or preknown table attacks
	playerId = requestedPlayer.GetId();
	uChecksum = fwKeyGen::AppendDataToKey(uChecksum, (const char *)&playerId, sizeof(playerId));

	return uChecksum;
}

// ================================================================================================================
// THREADING STATIC MEMBERS AND HELPERS
// ================================================================================================================

bool NetworkHasherUtil::sm_bCrcThreadExit = false;
u8 NetworkHasherUtil::sm_nextRequestId = 0;
int NetworkHasherUtil::s_RESULT_TIMEOUT_MS_IN_QUEUE = 60 * 1000;
int NetworkHasherUtil::s_MAX_SAME_PLAYER_CRC_REQUESTS = 3;
bool NetworkHasherUtil::s_SEND_TEL_ON_RESULT_TIMEOUT = true;
sysCriticalSectionToken NetworkHasherUtil::sm_RequestQueueCriticalSectionToken;
sysCriticalSectionToken NetworkHasherUtil::sm_ResultsQueueCriticalSectionToken;
sysIpcSema NetworkHasherUtil::sm_StartHashingSema = NULL;
sysIpcThreadId NetworkHasherUtil::sm_CrcThreadID = sysIpcThreadIdInvalid;
rage::atQueue<NetworkHasherUtil::HashRequest, NetworkHasherUtil::MAX_QUEUED_TO_HASH> NetworkHasherUtil::sm_HashingRequests;
rage::atFixedArray<NetworkHasherUtil::HashResult, NetworkHasherUtil::MAX_QUEUED_RESULTS> NetworkHasherUtil::sm_HashingResults;

#if !__FINAL
sysIpcSema s_StartHashingSemaGuard = NULL;
#endif

void NetworkHasherUtil::InitHasherThread()
{
	hasherDebugf3("Initializing network hasher thread");

	sm_bCrcThreadExit = false;

	sm_StartHashingSema = sysIpcCreateSema(0);

#if !__FINAL
	hasherDebugf3("Caching hashing semaphore value: 0x%p", sm_StartHashingSema);
	s_StartHashingSemaGuard = sm_StartHashingSema;
#endif

	gnetAssertf(sm_StartHashingSema != NULL, "NetworkHasherUtil::InitHasherThread failed creating its semaphore!");
	gnetAssertf(sm_CrcThreadID == sysIpcThreadIdInvalid, "NetworkHasherUtil::InitHasherThread leaking thread handlers!");

	const u32 HASHER_THREAD_STACK = sysIpcMinThreadStackSize;

#if !__FINAL	//< Just being extra paranoid about having no string traces about this thread in final builds
	const char* threadName = "HasherThread";
#else
	const char* threadName = "";
#endif

	enum {kHasherThreadCpu = 1};
	sm_CrcThreadID = sysIpcCreateThread(NetworkHasherUtil::UpdateHasherThread, NULL, HASHER_THREAD_STACK, PRIO_NETWORK_HASHER_UTIL_THREAD, threadName, kHasherThreadCpu);

    static const bool SEND_TEL_ON_RESULT_TIMEOUT = true;
    static const int RESULT_TIMEOUT_MS_IN_QUEUE = 60000;
    static const int MAX_SAME_PLAYER_CRC_REQUESTS = 3;

	// Cache tunables (do not keep any suspicious strings in final builds either)
	s_SEND_TEL_ON_RESULT_TIMEOUT = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SEND_TEL_ON_RESULT_TIMEOUT", 0x34e6f9e0), SEND_TEL_ON_RESULT_TIMEOUT);
	s_RESULT_TIMEOUT_MS_IN_QUEUE = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("RESULT_TIMEOUT_MS_IN_QUEUE", 0xea4f33c4), RESULT_TIMEOUT_MS_IN_QUEUE);
	s_MAX_SAME_PLAYER_CRC_REQUESTS = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MAX_SAME_PLAYER_CRC_REQUESTS", 0xd6408693), MAX_SAME_PLAYER_CRC_REQUESTS);

#if __BANK
	InitBank();
#endif
}

void NetworkHasherUtil::ShutdownHasherThread()
{
	hasherDebugf3("Shutting down network hasher thread");

#if __BANK
	ShutdownBank();
#endif

	//Signal the thread to stop
	sm_bCrcThreadExit = true;
	if(sm_CrcThreadID != sysIpcThreadIdInvalid)
	{
		//Unblock the thread
		sysIpcSignalSema(sm_StartHashingSema);

		//Wait for it to stop
		sysIpcWaitThreadExit(sm_CrcThreadID);
		sm_CrcThreadID = sysIpcThreadIdInvalid;
	}
	sm_bCrcThreadExit = false;

	// Clean semaphores
	if(sm_StartHashingSema)
	{
		sysIpcDeleteSema(sm_StartHashingSema);
		sm_StartHashingSema = NULL;
	}

#if !__FINAL
	s_StartHashingSemaGuard = NULL;
#endif

	// Clean data arrays
	sm_HashingRequests.Reset();
	sm_HashingResults.Reset();
}

void NetworkHasherUtil::UpdateResultsQueue(bool fromLoadingCode)
{
	if (!fromLoadingCode)
	{
		PF_PUSH_TIMEBAR("UpdateResultsQueue");
	}

	if( AssertVerify(NetworkInterface::IsNetworkOpen() && NetworkInterface::GetLocalPlayer()) )
	{
#if __BANK
		UpdateBankCombos();
#endif

		const rlGamerId& LocalPlayerId = NetworkInterface::GetLocalPlayer()->GetRlGamerId();

		// If we can't get the lock right now, don't worry about it, just retry flush in next frame.
		if( sm_ResultsQueueCriticalSectionToken.TryLock() )
		{
			// Loop through all available hashing results, checking which ones need to trigger a reply, which ones need to match with a reply, and which ones have timed out
			for(int i=0; i < sm_HashingResults.GetCount(); ++i)
			{
				const HashResult& iResult = sm_HashingResults[i];

				// CASE 1: CRC check result belongs to a CNetworkCrcHashRequestEvent (another player asked us to calculate this hash).
				// Send him back reply event and remove from the list
				if(iResult.m_RequestingPlayer != LocalPlayerId)
				{
					if(NetworkInterface::GetEventManager().CheckForSpaceInPool())
					{
						hasherDebugf3("Replying hash request: id[%d] players[0x%lX-0x%lX] asked[%d %d] checksum[0x%X]", 
							iResult.m_id, iResult.m_RequestingPlayer.GetId(), iResult.m_RequestedPlayer.GetId(), iResult.m_eSystemToCheck, iResult.m_uSpecificCheck, iResult.m_uChecksum);

						// send reply crc event if player is still there
						if( const CNetGamePlayer* pRequester = NetworkInterface::GetPlayerFromGamerId(iResult.m_RequestingPlayer) )
						{
							CNetworkCrcHashCheckEvent::TriggerReply(iResult.m_id, pRequester->GetPhysicalPlayerIndex(), (u8)iResult.m_eSystemToCheck, iResult.m_uSpecificCheck, iResult.m_uChecksum);
						}

						// delete event and continue
						sm_HashingResults.DeleteFast(i);
						--i;
					}
				}

				// CASE 2: CRC was requested by us and hasn't expired. This means it's either our own calculation, or either another player's CNetworkCrcHashRequestEvent reply.
				// Loop through all results trying to find its matching pair. If found, process checksum accordingly, and remove pair from the list.
				else if(iResult.m_uValidityTimer >= fwTimer::GetTimeInMilliseconds())
				{
#if __BANK
					// If single local check: this was triggered as a debug hash via RAG. Output and delete result
					if( iResult.m_RequestedPlayer == LocalPlayerId)
					{
						hasherDebugf3("Found local test hash request: id[%d] asked[%d %d] checksum[0x%X]", iResult.m_id, iResult.m_eSystemToCheck, iResult.m_uSpecificCheck, iResult.m_uChecksum);
						sm_HashingResults.DeleteFast(i);
						--i;
						continue;
					}
#endif
					for(int j=sm_HashingResults.GetCount()-1; j > i; --j)
					{
						const HashResult& jResult = sm_HashingResults[j];

						// If same req-reply pair
						if( iResult.m_id == jResult.m_id 
							&& iResult.m_eSystemToCheck == jResult.m_eSystemToCheck
							&& iResult.m_RequestedPlayer == jResult.m_RequestedPlayer 
							&& iResult.m_RequestingPlayer == jResult.m_RequestingPlayer )
						{
							if(Unlikely( iResult.m_uChecksum != jResult.m_uChecksum ))
							{
								NetworkRemoteCheaterDetector::NotifyCrcMismatch(
									NetworkInterface::GetPlayerFromGamerId(iResult.m_RequestedPlayer)
									, (u32)iResult.m_eSystemToCheck
									, iResult.m_uSpecificCheck
									, iResult.m_uChecksum);
							}

							// This runs in the main update thread, so is fine to call stuff from the outer world
							CStatsMgr::UpdateCRCStats(iResult.m_uChecksum != jResult.m_uChecksum);

							hasherDebugf3("Found hash request-reply pair: id[%d] players[0x%lX-0x%lX] asked[%d %d] checksum1[0x%X] checksum2[0x%X]  MATCH? %d", 
								iResult.m_id, iResult.m_RequestingPlayer.GetId(), iResult.m_RequestedPlayer.GetId(), iResult.m_eSystemToCheck, iResult.m_uSpecificCheck, 
								iResult.m_uChecksum, jResult.m_uChecksum, iResult.m_uChecksum == jResult.m_uChecksum);

							sm_HashingResults.DeleteFast(j);
							sm_HashingResults.DeleteFast(i);
							--i;
							break;
						}
					}
				}

				// CASE 3: This is a CRC requested from us which got expired (starved). Or either remote player calculation didn't reply, or either our own one failed.
				// Send CRC_NOT_REPLIED telemetry event to R* and remove from list
				else 
				{
					hasherDebugf3("Hash request timed out: id[%d] players[0x%lX-0x%lX] asked[%d %d] checksum[0x%X]", 
						iResult.m_id, iResult.m_RequestingPlayer.GetId(), iResult.m_RequestedPlayer.GetId(), iResult.m_eSystemToCheck, iResult.m_uSpecificCheck, iResult.m_uChecksum);

					if(s_SEND_TEL_ON_RESULT_TIMEOUT)
					{
						NetworkRemoteCheaterDetector::NotifyMissingCrcReply(NetworkInterface::GetPlayerFromGamerId(iResult.m_RequestedPlayer), (u32)iResult.m_eSystemToCheck, iResult.m_uSpecificCheck);
					}
					sm_HashingResults.DeleteFast(i);
					--i;
				}
			}

			sm_ResultsQueueCriticalSectionToken.Unlock();
		}
	}

	if (!fromLoadingCode)
	{
		PF_POP_TIMEBAR();
	}
}

void NetworkHasherUtil::PushNewHashRequest(u8 reqId, const rlGamerId& requestingPlayer, const rlGamerId& requestedPlayer, eHASHABLE_SYSTEM eSystemToCheck, u32 uSpecificCheck, const char* fileNameToHash, bool bIsARemoteRequest)
{
	gnetAssertf(sm_CrcThreadID != sysIpcThreadIdInvalid, "NetworkHasherUtil::PushNewHashRequest called when thread is not initialised");
	gnetAssertf(NetworkInterface::IsGameInProgress(), "NetworkHasherUtil::PushNewHashRequest called on SP game. Crash in 3..2..1..");
	gnetAssertf(requestingPlayer.IsValid() && requestedPlayer.IsValid(), "NetworkHasherUtil::PushNewHashRequest called with invalid players");

	bool bWorkerThreadNeedsToWakeUp = false;

	// Each remote player can ask for us only a maximum number of requests. This protects against flood attacks
	u32 numPendingRequestsOfThisPlayer = 0;

	// CRITICAL SECTION: Append new hashing request
	sm_RequestQueueCriticalSectionToken.Lock();
	{
		if(AssertVerify(!sm_HashingRequests.IsFull()))
		{
			if(bIsARemoteRequest)
			{
				for(int i=0; i < sm_HashingRequests.GetCount(); ++i )
				{
					if(sm_HashingRequests[i].m_RequestingPlayer == requestingPlayer)
					{
						++numPendingRequestsOfThisPlayer;
					}
				}
			}

			if(numPendingRequestsOfThisPlayer < s_MAX_SAME_PLAYER_CRC_REQUESTS)
			{
				bWorkerThreadNeedsToWakeUp = sm_HashingRequests.IsEmpty();

				HashRequest& newRequest = sm_HashingRequests.Append();
				newRequest.m_id = reqId;
				newRequest.m_RequestingPlayer = requestingPlayer;
				newRequest.m_RequestedPlayer = requestedPlayer;
				newRequest.m_eSystemToCheck = eSystemToCheck;
				newRequest.m_uSpecificCheck = uSpecificCheck;
				newRequest.m_FileName.Set(fileNameToHash, fileNameToHash ? static_cast<int>(strlen(fileNameToHash)) : 0, 0);
			}
			else
			{
				hasherDebugf3("FLOOD! Too many hash requests from player: id[%d] players[0x%lX-0x%lX] asked[%d %d]", 
					reqId, requestingPlayer.GetId(), requestedPlayer.GetId(), eSystemToCheck, uSpecificCheck);

				NetworkRemoteCheaterDetector::NotifyCrcRequestFlood(NetworkInterface::GetPlayerFromGamerId(requestingPlayer), (u32)eSystemToCheck, uSpecificCheck);
			}
		}
	}
	sm_RequestQueueCriticalSectionToken.Unlock();

	if(bWorkerThreadNeedsToWakeUp)
	{
#if !__FINAL
		if(s_StartHashingSemaGuard != sm_StartHashingSema)
		{
			hasherErrorf("Something has trashed the semaphore value!! HashingSema[0x%p] != HashingSemaGuard[0x%p]", sm_StartHashingSema, s_StartHashingSemaGuard);
		}
#endif
		sysIpcSignalSema(sm_StartHashingSema);
	}

	CStatsMgr::IncrementCRCNumChecks(!bIsARemoteRequest);
}

void NetworkHasherUtil::PushNewHashResult(u8 reqId, const rlGamerId& requestingPlayer, const rlGamerId& requestedPlayer, eHASHABLE_SYSTEM eSystemToCheck, u32 uSpecificCheck, u32 uResultChecksum)
{
	gnetAssertf(requestingPlayer.IsValid() && requestedPlayer.IsValid(), "NetworkHasherUtil::PushNewHashResult called with invalid players");

	// CRITICAL SECTION: Append new results
	sm_ResultsQueueCriticalSectionToken.Lock();
	{
		if(AssertVerify(!sm_HashingResults.IsFull()))
		{
			HashResult& newResult = sm_HashingResults.Append();
			newResult.m_id = reqId;
			newResult.m_RequestingPlayer = requestingPlayer;
			newResult.m_RequestedPlayer = requestedPlayer;
			newResult.m_eSystemToCheck = eSystemToCheck;
			newResult.m_uSpecificCheck = uSpecificCheck;
			newResult.m_uChecksum = uResultChecksum;
			newResult.m_uValidityTimer = fwTimer::GetTimeInMilliseconds() + static_cast<u32>(s_RESULT_TIMEOUT_MS_IN_QUEUE);
		}
	}
	sm_ResultsQueueCriticalSectionToken.Unlock();
}

void NetworkHasherUtil::UpdateHasherThread(void *UNUSED_PARAM(ptr))
{
	while( !IsHashingThreadClosing() ) 
	{
		// wait until we get a signal to start the update
		sysIpcWaitSema(sm_StartHashingSema);

		// Process all queued hashes if we woke up due to a new hash request
		do {} while ( !IsHashingThreadClosing() && CalculateNextHash() > 0 );
	}
}

u32 NetworkHasherUtil::CalculateNextHash()
{
	PF_PUSH_TIMEBAR("CalculateNextHash");

	u8 reqId = 0;
	rlGamerId requestingPlayer;
	rlGamerId requestedPlayer;
	eHASHABLE_SYSTEM eSystemToCheck = INVALID_HASHABLE;
	u32 uSpecificCheck = 0; 
	atString fileNameToHash;
	u32 uNumRemaningRequests = 0;

	// CRITICAL SECTION: Remove oldest hashing request
	sm_RequestQueueCriticalSectionToken.Lock();
	if( !sm_HashingRequests.IsEmpty() )
	{
		const HashRequest &request = sm_HashingRequests.Pop();
		reqId = request.m_id;
		requestingPlayer = request.m_RequestingPlayer;
		requestedPlayer = request.m_RequestedPlayer; 
		eSystemToCheck = request.m_eSystemToCheck;
		uSpecificCheck = request.m_uSpecificCheck;
		fileNameToHash = request.m_FileName;

		// Cache num remaining requests so we can keep looping on UpdateHasherThread without entering another critical section
		uNumRemaningRequests = sm_HashingRequests.GetCount();
	}
	sm_RequestQueueCriticalSectionToken.Unlock();

	if(eSystemToCheck != INVALID_HASHABLE)
	{
#if !__NO_OUTPUT
		rage::sysTimer hashingTime;

		hasherDebugf3("Start hash calculation: id[%d] players[0x%lX-0x%lX] asked[%d %d]", 
			reqId, requestingPlayer.GetId(), requestedPlayer.GetId(), eSystemToCheck, uSpecificCheck);
#endif

		const u32 resultCrc = CalculateHashInternal(requestingPlayer, requestedPlayer, eSystemToCheck, uSpecificCheck, fileNameToHash.c_str());

		// Bother to push result only if we managed to finish whole hash correctly. When thread is shutting down all hashing loops end, so resultCrc might be wrong
		if( !IsHashingThreadClosing() )
		{
#if !__NO_OUTPUT
			hasherDebugf3("Finished hash calculation: id[%d]  checksum[0x%X]   (took %.2f ms)", 
				reqId, resultCrc, hashingTime.GetMsTime());
#endif

			PushNewHashResult(reqId, requestingPlayer, requestedPlayer, eSystemToCheck, uSpecificCheck, resultCrc);
		}
	}

	PF_POP_TIMEBAR();

	return uNumRemaningRequests;
}

// ================================================================================================================
// RAG DEBUG STUFF
// ================================================================================================================

#if __BANK
bkBank* NetworkHasherUtil::ms_pBank = NULL;
bkButton* NetworkHasherUtil::ms_pCreateBankButton = NULL;
static const int FILE_TO_HASH_STRING_SIZE = 128;
char NetworkHasherUtil::ms_fileNameToHash[FILE_TO_HASH_STRING_SIZE];
static const int FILE_SUBSYSTEM_TO_HASH_STRING_SIZE = 64;
char NetworkHasherUtil::ms_subsystemToHash[FILE_SUBSYSTEM_TO_HASH_STRING_SIZE];
static const int TUNABLE_TO_HASH_STRING_SIZE = 64;
char NetworkHasherUtil::ms_tunableGroupToHash[TUNABLE_TO_HASH_STRING_SIZE];
char NetworkHasherUtil::ms_tunableInstanceToHash[TUNABLE_TO_HASH_STRING_SIZE];
const char* NetworkHasherUtil::s_Players[MAX_NUM_ACTIVE_PLAYERS];
unsigned int NetworkHasherUtil::s_NumPlayers = 0;
bkCombo* NetworkHasherUtil::s_PlayerCombo = NULL;
int NetworkHasherUtil::s_PlayerComboIndex = 0;
int NetworkHasherUtil::s_HashableSystemComboIndex = 0;
int NetworkHasherUtil::s_NumTimesToTriggerCmd = 1;
static const int TELEMETRY_ENTRY_SIZE = 128;
char NetworkHasherUtil::ms_telemetryEntryToProcess[TELEMETRY_ENTRY_SIZE];
char NetworkHasherUtil::ms_strSender[65];
char NetworkHasherUtil::ms_strReceiver[65];
char NetworkHasherUtil::ms_strSystem[33];
char NetworkHasherUtil::ms_strResult[70];
int NetworkHasherUtil::ms_specifCheck = 0;
int NetworkHasherUtil::ms_crcFromDig = 0;
int NetworkHasherUtil::ms_crcCalculated = 0;

static const char* eHASHABLE_SYSTEM_Strings[NetworkHasherUtil::INVALID_HASHABLE] = {
	"CRC_TUNING_SYSTEM",
	"CRC_ITEM_INFO_DATA",
	"CRC_PICKUP_INFO_DATA",
	"CRC_EXPLOSION_TAG_DATA",
	"CRC_DATA_FILE_CONTENTS",
	"CRC_GENERIC_FILE_CONTENTS",
	"CRC_SCRIPT_RESOURCE"
};

void NetworkHasherUtil::InitBank()
{
	if( !ms_pBank )
	{
		ms_pBank = &BANKMGR.CreateBank("NetworkHasherUtil", 0, 0, false);

		if(gnetVerifyf(ms_pBank, "Failed to create NetworkHasherUtil bank"))
		{
			ms_pCreateBankButton = ms_pBank->AddButton("Create Network Hasher Util widgets", &NetworkHasherUtil::CreateBankWidgetsOnDemand);
		}
	}
}

void NetworkHasherUtil::ShutdownBank()
{
	// Create the weapons bank
	if( ms_pBank )
	{
		BANKMGR.DestroyBank(*ms_pBank);
		ms_pBank = NULL;
	}
}

void NetworkHasherUtil::CreateBankWidgetsOnDemand()
{
	gnetAssertf(ms_pBank, "NetworkHasherUtil bank needs to be created first");

	if(ms_pCreateBankButton) //delete the create bank button
	{
		ms_pCreateBankButton->Destroy();
		ms_pCreateBankButton = NULL;

		s_PlayerComboIndex = 0;
		s_NumPlayers = 0;
		s_PlayerCombo = ms_pBank->AddCombo("Player", &s_PlayerComboIndex, s_NumPlayers, (const char **)s_Players);
		ms_pBank->AddSlider("Num times to trigger cmd", &s_NumTimesToTriggerCmd, 1, 10, 1);

		ms_pBank->PushGroup("TRIGGER_FILE_CRC_HACKER_CHECK");
		sprintf(ms_fileNameToHash, "%s", "");
		ms_pBank->AddText("File to hash", ms_fileNameToHash, FILE_TO_HASH_STRING_SIZE);
		ms_pBank->AddButton("Trigger command", NetworkHasherUtil::Bank_TriggerFileCrcHackerCheck);
		ms_pBank->PopGroup();

		ms_pBank->PushGroup("TRIGGER_PLAYER_CRC_HACKER_CHECK");
		s_HashableSystemComboIndex = 0;
		ms_pBank->AddCombo("System", &s_HashableSystemComboIndex, (int)INVALID_HASHABLE, eHASHABLE_SYSTEM_Strings);
		sprintf(ms_subsystemToHash, "%s", "");
		ms_pBank->AddText("Subsystem to hash", ms_subsystemToHash, FILE_SUBSYSTEM_TO_HASH_STRING_SIZE);
		ms_pBank->AddButton("Trigger command", NetworkHasherUtil::Bank_TriggerPlayerCrcHackerCheck);
		ms_pBank->PopGroup();

		ms_pBank->PushGroup("TRIGGER_TUNING_CRC_HACKER_CHECK");
		sprintf(ms_tunableGroupToHash, "%s", "");
		ms_pBank->AddText("Tunable group", ms_tunableGroupToHash, TUNABLE_TO_HASH_STRING_SIZE);
		sprintf(ms_tunableInstanceToHash, "%s", "");
		ms_pBank->AddText("Tunable instance", ms_tunableInstanceToHash, TUNABLE_TO_HASH_STRING_SIZE);
		ms_pBank->AddButton("Trigger command", NetworkHasherUtil::Bank_TriggerTuningCrcHackerCheck);
		ms_pBank->PopGroup();

		ms_pBank->AddSeparator();
		ms_pBank->AddSeparator();
		ms_pBank->AddSeparator();

		ms_pBank->PushGroup("## VERIFY HACKER FROM DIG EVENT DATA ##");
		sprintf(ms_telemetryEntryToProcess, "%s", "");
		ms_pBank->AddText("DIG telemetry data", ms_telemetryEntryToProcess, TELEMETRY_ENTRY_SIZE);
		ms_pBank->AddTitle("senderPlayer | receiverPlayer | extra1 | extra2 | extra3");
		ms_pBank->AddTitle("ie: \"XBL 1A...43|XBL 27...D1|6|1459623836|1048287998\"");
		ms_pBank->AddButton("PROCESS DATA", NetworkHasherUtil::Bank_TriggeProcessDIGeventBreakout);
		ms_pBank->AddSeparator();
		ms_pBank->AddTitle(":: RESULTS ::");
		sprintf(ms_strSender, "%s", "");
		ms_pBank->AddText("sender ID", ms_strSender, sizeof(ms_strSender), true);
		sprintf(ms_strReceiver, "%s", "");
		ms_pBank->AddText("receiver ID", ms_strReceiver, sizeof(ms_strReceiver), true);
		sprintf(ms_strSystem, "%s", "");
		ms_pBank->AddText("system", ms_strSystem, sizeof(ms_strSystem), true);
		ms_pBank->AddText("check", &ms_specifCheck, true);
		ms_pBank->AddText("crc from DIG", &ms_crcFromDig, true);
		ms_pBank->AddText("crc calculated (%d)", &ms_crcCalculated, true);
		sprintf(ms_strResult, "%s", "");
		ms_pBank->AddText("RESULT", ms_strResult, sizeof(ms_strResult), true);
		ms_pBank->PopGroup();
	}
}

void NetworkHasherUtil::UpdateBankCombos()
{
	// don't update the players combo if the network bank is closed
	if( Unlikely(ms_pBank==NULL || ms_pCreateBankButton || (ms_pBank && !ms_pBank->AreAnyWidgetsShown())) )
		return;

	// check whether it is time to refresh all of the combo boxes
	// refresh the list every second
	static unsigned lastTime = 0;
	const unsigned thisTime = fwTimer::GetSystemTimeInMilliseconds();
	if((thisTime <= lastTime) || ((thisTime - lastTime) <= 1000))
		return;

	lastTime = thisTime;

	// update the player list
	s_NumPlayers = 0;
	const unsigned numActivePlayers = netInterface::GetNumActivePlayers();
	const netPlayer* const* activePlayers = netInterface::GetAllActivePlayers();

	for(unsigned index = 0; index < numActivePlayers; index++)
	{
		const netPlayer *pPlayer = activePlayers[index];
		s_Players[s_NumPlayers++] = pPlayer->GetGamerInfo().GetName();
	}

	// account for selected player having left
	s_PlayerComboIndex = Min(s_PlayerComboIndex, static_cast<int>(s_NumPlayers) - 1);
	s_PlayerComboIndex = Max(s_PlayerComboIndex, 0);

	// update player combo
	if(s_PlayerCombo)
	{
		s_PlayerCombo->UpdateCombo("Player", &s_PlayerComboIndex, s_NumPlayers, (const char **)s_Players);
	}
}

const CNetGamePlayer* NetworkHasherUtil::NetworkBank_GetChosenPlayer()
{
	if(unsigned(s_PlayerComboIndex) < s_NumPlayers)
	{
		const char* targetName = s_Players[s_PlayerComboIndex];
		gnetAssert(targetName);

		const unsigned numActivePlayers = netInterface::GetNumActivePlayers();
		const netPlayer* const* activePlayers    = netInterface::GetAllActivePlayers();

		for(unsigned index = 0; index < numActivePlayers; index++)
		{
			const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, activePlayers[index]);

			if (!strcmp(pPlayer->GetGamerInfo().GetName(), targetName))
			{
				return pPlayer;
			}
		}
	}

	return NULL;
}

void NetworkHasherUtil::Bank_TriggerFileCrcHackerCheck()
{
	if( const CNetGamePlayer *pPlayer = NetworkBank_GetChosenPlayer() )
	{
		int desiredRepetitions = s_NumTimesToTriggerCmd;
		do {
			network_commands::CommandTriggerFileCrcHackerCheck(pPlayer->GetPhysicalPlayerIndex(), ms_fileNameToHash);
		} while (--desiredRepetitions > 0);
	}
}

void NetworkHasherUtil::Bank_TriggerPlayerCrcHackerCheck()
{
	if( const CNetGamePlayer *pPlayer = NetworkBank_GetChosenPlayer() )
	{
		const u32 specificCheck = ( s_HashableSystemComboIndex != CRC_EXPLOSION_TAG_DATA ) ? atStringHash(ms_subsystemToHash) : atoi(ms_subsystemToHash);
		int desiredRepetitions = s_NumTimesToTriggerCmd;
		do{
			network_commands::CommandTriggerPlayerCrcHackerCheck(pPlayer->GetPhysicalPlayerIndex(), s_HashableSystemComboIndex, specificCheck);
		} while (--desiredRepetitions > 0);
	}
}

void NetworkHasherUtil::Bank_TriggerTuningCrcHackerCheck()
{
	if( const CNetGamePlayer *pPlayer = NetworkBank_GetChosenPlayer() )
	{
		int desiredRepetitions = s_NumTimesToTriggerCmd;
		do{
			network_commands::CommandTriggerTuningCrcHackerCheck(pPlayer->GetPhysicalPlayerIndex(), ms_tunableGroupToHash, ms_tunableInstanceToHash);
		} while (--desiredRepetitions > 0);
	}
}

void NetworkHasherUtil::Bank_TriggeProcessDIGeventBreakout()
{
	u64 senderGamerId, receiverGamerId = 0;
	rlGamerId senderGamer, receiverGamer;
	u32 extra1 = INVALID_HASHABLE;
	ms_specifCheck = 0;
	ms_crcFromDig = 0;
	char userIDprefix[8];
	
	// Parse input
	sscanf(ms_telemetryEntryToProcess, "%s 0x%lX|%*s 0x%lX|%u|%u|%u", userIDprefix, &senderGamerId, &receiverGamerId, &extra1, &ms_specifCheck, &ms_crcFromDig);

	senderGamer.SetId(senderGamerId);
	receiverGamer.SetId(receiverGamerId);

	// Process input in the main game thread. Is alright, this is dev stuff
	ms_crcCalculated = CalculateHashInternal(senderGamer, receiverGamer, (eHASHABLE_SYSTEM)extra1, ms_specifCheck, "");
	const int invalidCRChash = CalculateHashInternal(senderGamer, receiverGamer, CRC_ITEM_INFO_DATA, 0, "");	//< ask crc of item with hash 0. This will return the invalid crc for this case (even invalid hash is user dependent)

	hasherDebugf3("Dev hash calculation: players[0x%lX-0x%lX] asked[%d %d] result[0x%X]", 
		senderGamer.GetId(), receiverGamer.GetId(), extra1, ms_specifCheck, ms_crcCalculated);

	const bool bValidResult = extra1 < INVALID_HASHABLE && ms_crcCalculated != invalidCRChash;
	const bool bSenderWasHacker = ms_crcFromDig != ms_crcCalculated;

	// Display results
	formatf(ms_strSender, "%s 0x%llX", userIDprefix, senderGamerId);
	formatf(ms_strReceiver, "%s 0x%llX", userIDprefix, receiverGamerId);
	formatf(ms_strSystem, "%s", extra1 < INVALID_HASHABLE? eHASHABLE_SYSTEM_Strings[extra1] : "INVALID !!");

	if(bValidResult)
	{
		formatf(ms_strResult, "%s (0x%llX) IS THE CHEATER", bSenderWasHacker? "sender" : "receiver", bSenderWasHacker? senderGamerId : receiverGamerId);
	}
	else
	{
		formatf(ms_strResult, "INVALID INPUT !!!!");
	}
}
#endif // __BANK
