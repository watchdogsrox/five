//
// name:        NetworkHasherUtil.h
// description: Threaded helper used to easily crc/hash common data stuff, like PAR_PARSABLE instances or whole files.
//				Only runs in network games.

#ifndef	INC_NETWORK_HASHER_UTIL_H
#define	INC_NETWORK_HASHER_UTIL_H

// Rage includes
#include "atl/array.h"
#include "atl/queue.h"
#include "atl/string.h"
#include "diag/channel.h"
#include "rline/rlgamerinfo.h"
#include "system/criticalsection.h"
#include "system/ipc.h"
#include "parser/visitor.h"

// Forward declarations
namespace rage
{
#if __BANK
	class bkBank;
	class bkButton;
	class bkCombo;
#endif
	class strStreamingInfo;
}
#if __BANK
class CNetGamePlayer;
#endif

RAGE_DECLARE_CHANNEL(hasher)

#define hasherDisplayf(fmt,...)			RAGE_DISPLAYF(hasher,fmt,##__VA_ARGS__)
#define hasherAssertf(cond,fmt,...)		RAGE_ASSERTF(hasher,cond,fmt,##__VA_ARGS__)
#define hasherVerifyf(cond,fmt,...)		RAGE_VERIFYF(hasher,cond,fmt,##__VA_ARGS__)
#define hasherDebugf(fmt, ...)			RAGE_DEBUGF1(hasher, fmt, ##__VA_ARGS__)
#define hasherDebugf1(fmt, ...)			RAGE_DEBUGF1(hasher, fmt, ##__VA_ARGS__)
#define hasherDebugf2(fmt, ...)			RAGE_DEBUGF2(hasher, fmt, ##__VA_ARGS__)
#define hasherDebugf3(fmt, ...)			RAGE_DEBUGF3(hasher, fmt, ##__VA_ARGS__)
#define hasherWarningf(fmt, ...)		RAGE_WARNINGF(hasher, fmt, ##__VA_ARGS__)
#define hasherErrorf(fmt, ...)			RAGE_ERRORF(hasher, fmt, ##__VA_ARGS__)

// ================================================================================================================
// HASHER VISITOR CLASS
// ================================================================================================================

class parHasherVisitor : public parInstanceVisitor
{
public:
	parHasherVisitor(u32 initValue) : m_ComputedHash(initValue) {};
	virtual ~parHasherVisitor() {}

	virtual void SimpleMember(parPtrToMember ptrToMember, rage::parMemberSimple& metadata);
	virtual void VectorMember(parPtrToMember ptrToMember,	rage::parMemberVector& metadata);
	virtual void Vec3VMember(Vec3V& data, parMemberVector& metadata);
	virtual void Vector3Member(Vector3& data,		parMemberVector& metadata);
	virtual void MatrixMember(parPtrToMember ptrToMember,	rage::parMemberMatrix& metadata);
	virtual void EnumMember(int& UNUSED_PARAM(data), rage::parMemberEnum& metadata);
	virtual void BitsetMember(parPtrToMember ptrToMember, parPtrToArray UNUSED_PARAM(ptrToBits), size_t UNUSED_PARAM(numBits), rage::parMemberBitset& metadata);
	virtual void StringMember(parPtrToMember UNUSED_PARAM(ptrToMember), const char* ptrToString, rage::parMemberString& UNUSED_PARAM(metadata));
	virtual void ExternalPointerMember(void*& data, rage::parMemberStruct& metadata);
	virtual bool BeginStructMember(parPtrToStructure UNUSED_PARAM(structAddr), rage::parMemberStruct& UNUSED_PARAM(metadata));
	virtual bool BeginArrayMember(parPtrToMember UNUSED_PARAM(ptrToMember), parPtrToArray arrayContents, size_t numElements, rage::parMemberArray& UNUSED_PARAM(metadata));
	virtual bool BeginMapMember(parPtrToStructure UNUSED_PARAM(structAddr),	rage::parMemberMap& UNUSED_PARAM(metadata));
	u32 GetComputedHash() const { return m_ComputedHash; }

private:
	u32 m_ComputedHash;
};

// ================================================================================================================
// HASHING HELPERS
// ================================================================================================================

template<typename _Type>
u32 GetParsableStructHash(_Type& instance, u32 value)
{
	parHasherVisitor parHaser(value);
	parHaser.Visit(instance);
	return parHaser.GetComputedHash();
}

// ================================================================================================================
// Network Hasher Util
// ================================================================================================================

class NetworkHasherUtil
{
public:
	// Keep enum in sync with commands_network.sch
	enum eHASHABLE_SYSTEM
	{
		CRC_TUNING_SYSTEM = 0,
		CRC_ITEM_INFO_DATA,
		CRC_PICKUP_INFO_DATA,
		CRC_EXPLOSION_TAG_DATA,
		CRC_DATA_FILE_CONTENTS,
		CRC_GENERIC_FILE_CONTENTS,
		CRC_SCRIPT_RESOURCE,

		INVALID_HASHABLE
	};

	// eHASHABLE_SYSTEM is packed as u8 in lots of places. Be sure it never becomes an issue
	CompileTimeAssert( INVALID_HASHABLE <= MAX_UINT8 );

	static void InitHasherThread();
	static void ShutdownHasherThread();
	static void UpdateResultsQueue(bool fromLoadingCode);

	static void PushNewHashRequest(u8 reqId, const rlGamerId& requestingPlayer, const rlGamerId& requestedPlayer, eHASHABLE_SYSTEM eSystemToCheck, u32 uSpecificCheck, const char* fileNameToHash, bool bIsARemoteRequest);
	static void PushNewHashResult(u8 reqId, const rlGamerId& requestingPlayer, const rlGamerId& requestedPlayer, eHASHABLE_SYSTEM eSystemToCheck, u32 uSpecificCheck, u32 uResultChecksum);

	static inline u8 GetNextRequestId() { return sm_nextRequestId++; }
	static inline bool IsHashingThreadClosing() { return sm_bCrcThreadExit; }

private:

	enum { MAX_QUEUED_TO_HASH = 40 };
	enum { MAX_QUEUED_RESULTS = 40 };

	struct HashEntryBase
	{
		// CRC unique identifier (used to match player requests and replies)
		rlGamerId m_RequestingPlayer;
		rlGamerId m_RequestedPlayer;
		u8 m_id;

		// Hash calc members needed for both calc and result telemetry event
		eHASHABLE_SYSTEM m_eSystemToCheck : 8;
		u32 m_uSpecificCheck;
	};

	struct HashRequest : HashEntryBase
	{
		atString m_FileName;	//< Filename strings are necessary only in some requests. They are stored in m_uSpecificCheck as a hash in their HashResult
	};

	struct HashResult : HashEntryBase
	{
		u32 m_uChecksum;		//< Result of the whole hashing process
		u32 m_uValidityTimer;	//< Results can time out after RESULT_TIMEOUT_MS_IN_QUEUE milliseconds.
	};

	static void UpdateHasherThread(void *ptr);

	// Function called by the hash worker. Returns num of remaining requests so system can keep looping while > 0
	static u32 CalculateNextHash();

	// Hashing functions
	static u32 CalculateHashInternal(const rlGamerId& requestingPlayer, const rlGamerId& requestedPlayer, eHASHABLE_SYSTEM eSystemToCheck, u32 uSpecificCheck, const char* fileNameToHash);
	static u32 GetRequiredHash(eHASHABLE_SYSTEM eSystemToCheck, u32 uSpecificCheck, u32 initValue);
	static u32 GetTuningDataHash(u32 tuningPackedIndexes, u32 value);
	static u32 GetItemInfoDataHash(u32 itemInfoHash, u32 value);
	static u32 GetPickupInfoDataHash(u32 pickupHash, u32 value);
	static u32 GetExplosionInfoDataHash(u32 expTag, u32 value);
	static u32 GetDataFileContentsHash(u32 dataFileHash, u32 value);
	static u32 GetGenericFileHash(const char* file, u32 initValue);
	static u32 GetGenericResourceHash(const rage::strStreamingInfo* pResourceStrInfo, u32 value);
	static u32 GetResourcedScriptHash(u32 scriptNameHash, u32 value);

	// static members (threading and tunables)
	static bool sm_bCrcThreadExit;
	static bool s_SEND_TEL_ON_RESULT_TIMEOUT;
	static u8 sm_nextRequestId;
	static int s_RESULT_TIMEOUT_MS_IN_QUEUE;
	static int s_MAX_SAME_PLAYER_CRC_REQUESTS;
	static sysIpcSema sm_StartHashingSema;
	static sysCriticalSectionToken sm_RequestQueueCriticalSectionToken;
	static sysCriticalSectionToken sm_ResultsQueueCriticalSectionToken;
	static sysIpcThreadId sm_CrcThreadID;
	static rage::atQueue<HashRequest, MAX_QUEUED_TO_HASH> sm_HashingRequests;
	static rage::atFixedArray<HashResult, MAX_QUEUED_RESULTS> sm_HashingResults;

	// RAG debugging stuff
#if __BANK
	static void InitBank();
	static void UpdateBankCombos();
	static void ShutdownBank();
	static void CreateBankWidgetsOnDemand();
	static void Bank_TriggerFileCrcHackerCheck();
	static void Bank_TriggerPlayerCrcHackerCheck();
	static void Bank_TriggerTuningCrcHackerCheck();
	static void Bank_TriggeProcessDIGeventBreakout();
	static const CNetGamePlayer* NetworkBank_GetChosenPlayer();

	static bkBank* ms_pBank;
	static bkButton* ms_pCreateBankButton;
	static char ms_fileNameToHash[];
	static char ms_subsystemToHash[];
	static char ms_tunableGroupToHash[];
	static char ms_tunableInstanceToHash[];
	static const char* s_Players[];
	static unsigned int s_NumPlayers;
	static bkCombo* s_PlayerCombo;
	static int s_PlayerComboIndex;
	static int s_HashableSystemComboIndex;
	static int s_NumTimesToTriggerCmd;
	static char ms_telemetryEntryToProcess[];
	static char ms_strSender[];
	static char ms_strReceiver[];
	static char ms_strSystem[];
	static char ms_strResult[];
	static int ms_specifCheck;
	static int ms_crcFromDig;
	static int ms_crcCalculated;
#endif
};

#endif	// INC_NETWORK_HASHER_UTIL_H
