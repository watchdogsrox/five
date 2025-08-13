#ifndef __CTunables__
#define __CTunables__

// rage includes
#include "atl/map.h"
#include "atl/singleton.h"

// game includes
#include "net/task.h"
#include "network/cloud/CloudManager.h"
#include "system/memorycheck.h"
#include "system/threadpool.h"

#define TUNABLES_VERSION (1)
#define TUNABLES_REPORT_DISCREPANCIES 0
#define USE_TUNABLES_MODIFICATION_DETECTION 0
namespace rage
{
	class RsonReader;
}

class TunablesListener
{
protected:

	TunablesListener(OUTPUT_ONLY(const char* szName));
	virtual ~TunablesListener();

public:

	virtual void OnTunablesRead() = 0;

private:

#if !__NO_OUTPUT
	static const char MAX_NAME = 64; 
	char m_Name[MAX_NAME];
#endif
};

#define BASE_GLOBALS_HASH ATSTRINGHASH("BASE_GLOBALS", 0x0b5ee873)
#define MP_GLOBAL_HASH ATSTRINGHASH("MP_GLOBAL", 0x38ceb237)
#define CD_GLOBAL_HASH ATSTRINGHASH("CD_GLOBAL", 0xe3afc5bd)

// auto tunable helpers
// This is to make it easier to integrate tunables from RDR. Note that this macro just expands
// RDR's invocation to GTA V's call pattern, and is different than RDR's AutoTunable class.
// t - variable type (bool, int, float)
// d - default value
// h - tunable hash value - use ATSTRINGHASH(name, hash)
// c - callback
#define AUTO_TUNABLE_CBS(t, d, h, c)											\
	{																			\
		t tunableVar = d;														\
		if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, h, tunableVar))		\
		{																		\
			c(tunableVar);														\
		}																		\
	}

// Here, we'll run the check anywhere between 10-60 seconds
#define TUNABLES_MODIFICATION_DETECTION_TIMER_MIN 1000*10
#define TUNABLES_MODIFICATION_DETECTION_TIMER_MAX 1000*60
u32 TunableHash(unsigned nContextHash, unsigned nTunableHash);

class CTunables : datBase, public CloudListener
{
public:

	enum eUnionType 
	{ 
		kINVALID = -1,
		kINT, 
		kFLOAT, 
		kBOOL,
		kTYPE_MAX,
	};

	 CTunables();
	~CTunables();

	bool Init();
	void Shutdown();

	void AddListener(TunablesListener* pListener);
	bool RemoveListener(TunablesListener* pListener);

	void Update();

	bool  Access(const u32 nHash, eUnionType kType);
	bool  Access(const u32 nHash, bool& bValue);
	bool  Access(const u32 nHash, int& nValue);
	bool  Access(const u32 nHash, u32& nValue);
	bool  Access(const u32 nHash, float& fValue);
	bool  TryAccess(const u32 nHash, bool bValue);
	int   TryAccess(const u32 nHash, int nValue);
	u32   TryAccess(const u32 nHash, u32 nValue);
	float TryAccess(const u32 nHash, float fValue);

	bool  CheckExists(const u32 nContextHash, const u32 nTunableHash)				{ return Access(TunableHash(nContextHash, nTunableHash), kINVALID); }
	bool  Access(const u32 nContextHash, const u32 nTunableHash, bool& bValue)		{ return Access(TunableHash(nContextHash, nTunableHash), bValue); }
	bool  Access(const u32 nContextHash, const u32 nTunableHash, int& nValue)		{ return Access(TunableHash(nContextHash, nTunableHash), nValue); }
	bool  Access(const u32 nContextHash, const u32 nTunableHash, u32& nValue)		{ return Access(TunableHash(nContextHash, nTunableHash), nValue); }
	bool  Access(const u32 nContextHash, const u32 nTunableHash, float& fValue)		{ return Access(TunableHash(nContextHash, nTunableHash), fValue); }
	bool  TryAccess(const u32 nContextHash, const u32 nTunableHash, bool bValue)	{ return TryAccess(TunableHash(nContextHash, nTunableHash), bValue); }
	int   TryAccess(const u32 nContextHash, const u32 nTunableHash, int nValue)		{ return TryAccess(TunableHash(nContextHash, nTunableHash), nValue); }
	u32   TryAccess(const u32 nContextHash, const u32 nTunableHash, u32 nValue)		{ return TryAccess(TunableHash(nContextHash, nTunableHash), nValue); }
	float TryAccess(const u32 nContextHash, const u32 nTunableHash, float fValue)	{ return TryAccess(TunableHash(nContextHash, nTunableHash), fValue); }

	bool Insert(const u32 nHash, bool bValue, u32 nStartPosix, u32 nEndPosix, bool bCountsForCRC, const char* szName = NULL);
	bool Insert(const u32 nHash, int nValue, u32 nStartPosix, u32 nEndPosix, bool bCountsForCRC, const char* szName = NULL);
	bool Insert(const u32 nHash, u32 nValue, u32 nStartPosix, u32 nEndPosix, bool bCountsForCRC, const char* szName = NULL);
	bool Insert(const u32 nHash, float fValue, u32 nStartPosix, u32 nEndPosix, bool bCountsForCRC, const char* szName = NULL);
	
	void ModificationDetectionUpdate();
	bool ModificationDetectionClear();
	bool ModificationDetectionRegistration(const u32 nContextHash, const u32 nTunableHash, int &param);
	bool ModificationDetectionRegistration(const u32 nContextHash, const u32 nTunableHash, float &param);

    int GetContentListID(const unsigned nContentHash);

	int GetNumMemoryChecks();
	bool GetMemoryCheck(int nIndex, MemoryCheck& memoryCheck);
	void SetMemoryCheckFlags(int nIndex, unsigned nFlags);

	int GetVersion() const { return m_iVersion; };
	int GetPoolsize() const { return m_iPoolsize; };

	u32 GetLocalCRC() const { return m_nLocalCRC; }
    u32 GetCloudCRC() const { return m_nCloudCRC; }

	bool StartCloudRequest();
	bool HasCloudTunables() const { return m_bHasCloudTunables; }
    bool IsCloudRequestPending();
	bool HasCloudRequestFinished() const { return m_bHasCompletedCloudRequest; }
	bool HaveTunablesLoaded() const { return m_tunablesRead; }

	void OnSignOut();
	void OnSignedOnline();

#if !__FINAL
	bool LoadLocalFile();
#endif

#if __BANK
	void InitWidgets();
	void Dump();
#endif

private:
	// I make these friends because I just want to access the structs and the types
	// defined - not necessarily to access the member fields themselves.
	friend class TunablesModificationTask;
	friend class TunablesWorkItem;

	union sUnionValue
	{
		sUnionValue() { m_INT = 0; }
		sUnionValue(bool bValue) { m_BOOL = bValue; }
		sUnionValue(int nValue) { m_INT = nValue; }
		sUnionValue(float fValue) { m_FLOAT = fValue; }

		bool Equals(const sUnionValue& tUnionValue) const
		{
			return m_INT == tUnionValue.m_INT;
		}

		bool    m_BOOL;
		s32     m_INT;
		float   m_FLOAT;
	};

	struct sTunableValue
	{
		sUnionValue m_Value; 
		u32 m_nStartTimePosix;
		u32 m_nEndTimePosix;
		bool m_bCountsForCRC;
	};

	struct sTunable
	{
#if !__NO_OUTPUT
		sTunable(u32 nHash, eUnionType nType) : m_nHash(nHash), m_nType(nType), m_bCountsForCRC(true), m_bValidValue(false) { }
#else
		sTunable(eUnionType nType) : m_nType(nType), m_bCountsForCRC(true), m_bValidValue(false) { }
#endif

		~sTunable() { m_TunableValues.Reset(); }

		sTunableValue* AddValue(sTunableValue& tTunableValue);
		sTunableValue* GetValue(const sTunableValue& tTunableValue);

		bool Update(u64 nNowPosix);

		atArray<sTunableValue> m_TunableValues;
		sUnionValue m_CurrentValue;
		sUnionValue m_LastValue;
		bool m_bCountsForCRC : 1;
		bool m_bValidValue : 1;
		eUnionType m_nType; 

#if !__NO_OUTPUT
		u32 m_nHash; 
#endif
	};

	bool EstablishPool();

	bool RequestCloudFile();
	void OnCloudEvent(const CloudEvent* pEvent);

#if !__FINAL
	bool LoadFile(const char* szFilePath);
#endif

	enum eFormat
	{
		FORMAT_INVALID = -1,
		FORMAT_STRING,
		FORMAT_HASH,
		FORMAT_MAX,
	};

	bool LoadFromJSON(const void* pData, unsigned nDataSize, bool bCreatePools, bool bInsertTunables, unsigned& nCRC NOTFINAL_ONLY(, bool bFromCloud));
	bool LoadParameters(rage::RsonReader* pNode);
	bool LoadTunables(rage::RsonReader* pNode, bool bInsertTunables, unsigned& nCRC, eFormat nFormat);
	bool LoadTunable(const char* szContextName, rage::RsonReader* pNode, bool bInsertTunable, unsigned& nCRC, eFormat nFormat);
	bool LoadContentLists(rage::RsonReader* pNode);
	bool LoadMemoryChecks(rage::RsonReader* pNode NOTFINAL_ONLY(, bool bFromCloud)); 
	eUnionType LoadValueFromJSON(rage::RsonReader* pNode, sUnionValue* pEntry);

#if !__NO_OUTPUT
	void WriteTunablesFile(const void* pData, unsigned nDataSize);
#endif

	const sUnionValue* AccessInternal(const u32 nHash, eUnionType kType);

	bool Insert(const char* szContext, const char* szName, bool bValue, u32 nStartPosix, u32 nEndPosix, bool bCountsForCRC);
	bool Insert(const char* szContext, const char* szName, int nValue, u32 nStartPosix, u32 nEndPosix, bool bCountsForCRC);
	bool Insert(const char* szContext, const char* szName, float fValue, u32 nStartPosix, u32 nEndPosix, bool bCountsForCRC);

	bool InsertInternal(const u32 nHash, const char* szName, sUnionValue tValue, eUnionType kType, u32 nStartPosix, u32 nEndPosix, bool bCountsForCRC);
	bool InsertInternal(const u32 nHash, const char* szName, sTunableValue& tTunableValue, eUnionType kType);

	const char* GenerateName(const char* szContext, const char* szName, eUnionType kType);

#if __BANK
    static const int kMAX_TEXT = 64;
    char m_szBank_Context[kMAX_TEXT];
    char m_szBank_Tunable[kMAX_TEXT];
    char m_szBank_Value[kMAX_TEXT];
    char m_szBank_StartTime[kMAX_TEXT];
    char m_szBank_EndTime[kMAX_TEXT];
    bool m_bBank_Toggle;
    void Bank_InsertBool();
    void Bank_InsertInt();
    void Bank_InsertFloat();
	void Bank_Dump();
	void Bank_LoadLocalFile();
	void Bank_RequestServerFile();
#endif

	typedef atMapMemoryPool<u32, sTunable> TunablesMemoryPool;
	typedef atMap<u32, sTunable, atMapHashFn<u32>, atMapEquals<u32>, TunablesMemoryPool::MapFunctor> TunablesMap;
	typedef TunablesMap::Iterator TunablesMapIter;
	typedef TunablesMap::Entry TunablesMapEntry;

	TunablesMemoryPool m_TunableMemoryPool;
	TunablesMap m_TunableMap;

	atArray<sTunable*> m_TunablesToUpdate; 

    struct Content
    {
        unsigned nHash;
        u8 nListID;
    };
    atArray<Content> m_ContentHashList;

	atArray<MemoryCheck> m_MemoryCheckList;

	int m_iVersion;
	int m_iPoolsize;

	bool m_bHasCompletedCloudRequest;
	bool m_bHasCloudTunables;

	CloudRequestID m_nCloudRequestID;

#if !__FINAL || __FINAL_LOGGING
	unsigned m_nCurrentCloudFileIndex;
#endif

	static const u32 TUNABLE_CHECK_INTERVAL = 1000;
	u32 m_LastTunableCheckTime;

	u32 m_nLocalCRC; 
    u32 m_nCloudCRC;

	atArray<TunablesListener*> m_TunableListeners;
	bool m_tunablesRead;

	atMap<u32, void *> m_TunableModificationDetectionTable;
	static u32 sm_TunableModificationDetectionTimerStart;
	static u32 sm_TunableModificationDetectionTimerEnd;
};

//////////////////////////////////////////////////////
// TunablesWorkItem
//////////////////////////////////////////////////////
class TunablesWorkItem : public sysThreadPool::WorkItem
{
public:
	bool Configure(CTunables::TunablesMap *map,atMap<u32, void *> *modTable);
	virtual void DoWork();

private:
	CTunables::TunablesMap	*m_tunablesMap;
	atMap<u32, void *>		*m_tunablesModificationTable;
};

//////////////////////////////////////////////////////
// TunablesModificationTask
//////////////////////////////////////////////////////
class TunablesModificationTask : public netTask
{
public:
	NET_TASK_DECL(TunablesModificationTask)
	TunablesModificationTask();
	virtual bool Configure(CTunables::TunablesMap *map,atMap<u32, void *> *modTable);
	virtual netTaskStatus OnUpdate(int* resultCode);
	virtual void OnCancel();
private:
	enum State
	{
		STATE_DETECTION_BEGIN,
		STATE_DETECTION_WAIT,
		STATE_DETECTION_COMPLETE
	};
	TunablesWorkItem m_workItem;
	State m_State;
	CTunables::TunablesMap	*m_tunablesMap;
	atMap<u32, void *>		*m_tunablesModificationTable;
};

typedef atSingleton<CTunables> Tunables;
#endif
