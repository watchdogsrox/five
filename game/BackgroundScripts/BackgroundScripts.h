#ifndef _BACKGROUND_SCRIPTS_H_
#define _BACKGROUND_SCRIPTS_H_

#include "atl/atfixedstring.h"
#include "atl/singleton.h"
#include "atl/queue.h"
#include "system/simpleallocator.h"
#include "script/thread.h"
#include "system/criticalsection.h"
#include "diag/channel.h"
#include "streaming/streamingdefs.h"

#if __BANK
#include "BackgroundScriptsDebug.h"
#endif
#include "Network/Cloud/CloudManager.h"

#define RECENT_BG_UPDATED_QUEUE_SIZE 10
#define MAX_BG_SCRIPTS 1000

RAGE_DECLARE_CHANNEL(bgScript);
  
#define bgScriptDebug1(fmt, ...)       RAGE_DEBUGF1(bgScript, fmt, ##__VA_ARGS__)
#define bgScriptDebug2(fmt, ...)       RAGE_DEBUGF2(bgScript, fmt, ##__VA_ARGS__)
#define bgScriptDebug3(fmt, ...)       RAGE_DEBUGF3(bgScript, fmt, ##__VA_ARGS__)
#define bgScriptError(fmt, ...)        RAGE_ERRORF(bgScript, fmt, ##__VA_ARGS__)
#define bgScriptWarning(fmt, ...)      RAGE_WARNINGF(bgScript, fmt, ##__VA_ARGS__)
#define bgScriptDisplayf(fmt,...)      RAGE_DISPLAYF(bgScript, fmt,##__VA_ARGS__)
#define bgScriptAssertf(cond,fmt,...)  RAGE_ASSERTF(bgScript, cond, fmt, ##__VA_ARGS__)
#define bgScriptAssert(cond)           RAGE_ASSERT(bgScript, cond)
#define bgScriptVerifyf(cond,fmt,...)  RAGE_VERIFYF(bgScript, cond, fmt, ##__VA_ARGS__)
#define bgScriptVerify(cond)           RAGE_VERIFY(bgScript,cond)

#define CONTEXT_IN_GAME "INGAME"

#if __SPU
#define BG_SCRIPTS_CRITICAL_SECTION
#define BG_SCRIPTS_RECENT_UPDATE_CRITICAL_SECTION
#else
#define BG_SCRIPTS_CRITICAL_SECTION sysCriticalSection cs(SBackgroundScripts::GetInstance().GetCriticalSectionToken())
#define BG_SCRIPTS_RECENT_UPDATE_CRITICAL_SECTION sysCriticalSection rgcs(SBackgroundScripts::GetInstance().GetRecentUpdateCriticalSectionToken())
#endif

namespace rage
{
	class parElement;
}

enum DLState
{
	DLS_NOT_STARTED ,
	DLS_STARTED,
	DLS_FAILED,
	DLS_SUCCESS,
};

// ********************************************************************************
// DON"T TOUCH - This struct needs to match BG_SCRIPT_TYPES in commands_script.sch
// ********************************************************************************
enum ScriptType
{
	SCRIPT_TYPE_UNKNOWN = -1,
	SCRIPT_TYPE_UTIL,
	NUM_SCRIPT_TYPES,
};

struct Context
{
	bool isActive;
	Context() : isActive(true) {}
};

typedef atMapMemoryPool<u32, Context> ContextMemoryPool;
typedef atMap<u32, Context, atMapHashFn<u32>, atMapEquals<u32>, ContextMemoryPool::MapFunctor > ContextMap;
typedef ContextMap::Entry ContextMapEntry;

class BGScriptInfo
{
public:
	enum StatFlags
	{
		FLAG_NONE							= 0,
		FLAG_FORCENORUN						= BIT(0),
		FLAG_ONLYRUNONCE					= BIT(1),
		FLAG_IS_COMPLETE					= BIT(2),	//< Unused

		FLAG_MODE_SP						= BIT(3),
		FLAG_MODE_MP						= BIT(4),
		
		FLAG_NEED_ALL_CONTEXTS				= BIT(5),

		FLAG_MODE_ALL						= FLAG_MODE_SP | FLAG_MODE_MP
	};

	struct LaunchParam
	{
		enum VAR_TYPE
		{
			INT,
//			STAT,
			//SCRIPT_VAR,
// 			SETTING,
// 			EQUIPMENT,
			NUM_VAR_TYPE
		};

		enum REWARD_TYPE
		{
			CASH,
			XP,
//			UNLOCK,
			NUM_REWARD_TYPE
		};

		enum COMPARE
		{
			GREATER_THAN,
			LESS_THAN,
			EQUAL_TO,
			GREATER_THAN_EQUAL_TO,
			LESS_THAN_EQUAL_TO,
			NOT_EQUAL_TO,
			NUM_COMPARE
		};

		struct Header
		{
			u32 type:3;
			CompileTimeAssert(NUM_VAR_TYPE < BIT(3));
			u32 award_type:2;
			CompileTimeAssert(NUM_REWARD_TYPE < BIT(2));
			u32 compare:3;
			CompileTimeAssert(NUM_COMPARE < BIT(3));
			u32 isScore:1;
		};

		u32 m_nameHash;
		Header m_header;
		union
		{
			int m_iData;
		};

		union
		{
			u32 m_uAward;
			int m_iAward;
		};

		LaunchParam()
		{
			Reset();
		}

		int GetValueFromData() const;
		bool CompareValueAgainstData(int value) const;
#if __ASSERT
		void Validate(const char* pScriptName) const;
#endif

		void Reset();
	};

	enum UIInfoFlags
	{
		UIFLAG_NONE				 = 0,
		UIFLAG_					 = BIT(0),
		UIFLAG_BGIMAGE_OVERLAY	 = BIT(1),
		UIFLAG_DICT_IS_STREAMED  = BIT(2),

		UIFLAG_UNIT_SECONDS		= BIT(3),
		UIFLAG_UNIT_MINUTES		= BIT(4),		
		UIFLAG_UNIT_HOURS		= BIT(5),
		UIFLAG_UNIT_MILES		= BIT(6),
	};
	
	struct UIInfo
	{
		scrTextLabel15 m_name;
		u16 m_flags;

		UIInfo()
		{
			m_name[0] = '\0';
			m_flags = UIFLAG_NONE;
		}
		
		void SetFlag(UIInfoFlags flag) { m_flags |= (u16)flag; }
		bool GetIsFlagSet(UIInfoFlags flag) const { return (m_flags & flag) != 0; }
		bool IsDictStreamed() const { return GetIsFlagSet(UIFLAG_DICT_IS_STREAMED); }
	};
	
	// **************************************************************************
	struct LaunchData
	{
		scrValue id;
		scrValue scriptType;

		LaunchData() { id.Int = 0; scriptType.Int = SCRIPT_TYPE_UNKNOWN; }
	};
	// DON"T TOUCH - This struct needs to match launch_Data in commands_script.sch
	// **************************************************************************

	BGScriptInfo();
	virtual ~BGScriptInfo() {}

	void Shutdown();

#if __ASSERT
	virtual void Validate() const {}
#endif

	virtual bool ShouldBeRunning(const ContextMap& rContextMap, const u64& posixTime, const StatFlags gameMode) const;
	virtual bool IsRunning() const;
	void Boot();
	void Stop();

	void SetName( const char *name );
	void SetNameHash( u32 nameHash ) {m_nameHash = nameHash;}
#if !__NO_OUTPUT
	const char * GetName() const;
#endif
	u32 GetNameHash() const {return m_nameHash;}

	virtual void Init();
	virtual void SetTimes( u64 UNUSED_PARAM(start), u64 UNUSED_PARAM(end) ) {}
	virtual bool IsExpired(const u64& posixTime) const = 0;
	virtual bool HasStarted( const u64& posixTime) const = 0;

	virtual void SetProgramName( const char *programName ) = 0;
	virtual const char* GetProgramName() const = 0;

	void SetFlag(StatFlags flag) { m_flags |= flag; }
	void ClearFlag(StatFlags flag) { m_flags &= ~flag; }
	bool GetAreAllFlagsSet(StatFlags flag) const { return (m_flags & flag) == flag; }
	bool GetIsAnyFlagSet(StatFlags flag) const { return (m_flags & flag) !=0; }
	
	strIndex& GetImageIndex() { return m_imageIndex; }
		
	u32 GetCategory() const {return m_category;}
	void SetCategory(u32 category) {m_category = category;}

	virtual void AddContext( u32 contextHash ) = 0;

	virtual void UpdateStatus(const ContextMap& contextMap, const u64& posixTime, const StatFlags gameMode);

	bool RequestedEnd();

	virtual void AddLaunchParam(LaunchParam& param) = 0;
	virtual const LaunchParam* GetLaunchParam(u32 hash) const = 0;

#if __BANK
	virtual int DebugGetContextCount() const = 0;
	virtual int DebugGetLaunchParamCount() const = 0;
#endif

	void SetScriptId(int id) {m_launchData.id.Int = id;}
	ScriptType GetScriptType() const {return (ScriptType) m_launchData.scriptType.Int;}
	
	bool IsTypeUtility() const { return GetScriptType() == SCRIPT_TYPE_UTIL; }

	virtual void Fixup() {};

	void RequestStreamedScript(const char* pMountPath, bool bBlockingLoad);

protected:
	virtual void OnFailedToLoad() {}

	LaunchData m_launchData;
	
private:
	scrThreadId m_scriptThdId;
	scrProgramId m_iPTProgramId;
	strIndex m_imageIndex;
	u32 m_msExitTime;
	int m_flags;
	u32 m_nameHash;
	u32 m_category;
};

class CommonScriptInfo: public BGScriptInfo
{
public:
	CommonScriptInfo();
	void Init();

	bool ShouldBeRunning(const ContextMap& rContextMap, const u64& posixTime, const StatFlags gameMode) const;

	void SetProgramName( const char *programName );
	const char* GetProgramName() const {return m_szPTProgramName;}

	void AddLaunchParam(LaunchParam& param);
	const LaunchParam* GetLaunchParam(u32 hash) const;

	void AddContext( u32 contextHash );

	bool HasActiveContext(const ContextMap& rContextMap) const;

#if __BANK
	virtual int DebugGetContextCount() const {return m_aValidContexts.size();}
	virtual int DebugGetLaunchParamCount() const {return m_aLaunchParams.size();}
#endif

	enum { PROGRAM_NAME_SIZE = 32 };

protected:
	atFixedArray<LaunchParam, 12> m_aLaunchParams;

private:
	static const int MAX_CONTEXTS = 8;
	atFixedArray<u32, MAX_CONTEXTS> m_aValidContexts;

	char m_szPTProgramName[PROGRAM_NAME_SIZE];
};

class UtilityScriptInfo: public CommonScriptInfo
{
public:
	UtilityScriptInfo();

	void Init();
	void SetTimes( u64 start, u64 end );
	bool IsExpired(const u64& posixTime) const { return m_iEndTime < posixTime; };
	bool HasStarted( const u64& posixTime ) const { return m_iStartTime < posixTime; }

	bool ShouldBeRunning(const ContextMap& rContextMap, const u64& posixTime, const StatFlags gameMode) const;

private:
	u64 m_iStartTime;
	u64 m_iEndTime;
};

class BackgroundScripts : CloudListener
{
public:
	typedef atFixedString<64> CategoryString;
	typedef atMap<u32, CategoryString > CategoryMap;

	BackgroundScripts();
	~BackgroundScripts ();

	static void InitClass(unsigned initMode);
	static void UpdateClass();
	static void ShutdownClass(unsigned shutdownMode);

	static u32 GetBGScriptCloudHash();

	void Init();
	void Update();
	void Shutdown();

    bool RequestCloudFile();
    bool HasCloudScripts();
	bool IsCloudRequestPending();
    u64 GetCloudModifiedTime() { return m_nModifiedTime; }

	sysMemAllocator* GetScriptInfoAllocator() const { return m_pScriptInfoAllocator; }

#if __ASSERT
	void Validate() const;
#endif

#if !__NO_OUTPUT
	bool StartContext( const char* pContextName );
	bool EndContext( const char* pContextName );
#endif
	
	bool StartContext( u32 contextHash );
	bool EndContext( u32 contextHash );

	void EndAllContexts();

	BANK_ONLY(int GetScriptInfoCount() const {return m_aScriptInfos.size();})
	BGScriptInfo* GetScriptInfo(int scriptId);
	BGScriptInfo* GetScriptInfo(const char* scriptInfoName);
	BGScriptInfo* GetScriptInfoFromHash(u32 scriptNameHash);
	int GetScriptIDFromHash(u32 scriptNameHash);

	const CategoryMap& GetCategoryMap() const {return m_categories;}
	void SetGameMode(BGScriptInfo::StatFlags gameMode) {m_currentGameMode = gameMode;}

	sysCriticalSectionToken& GetCriticalSectionToken() {return m_token;}
	sysCriticalSectionToken& GetRecentUpdateCriticalSectionToken() {return m_recentUpdateToken;}

	BANK_ONLY(BackgroundScriptsDebug& GetDebug() {return m_debug;})

	BGScriptInfo* AddBGScriptInfo(ScriptType scriptType, u32 nameHash);

private: // data store

	void AquireMemoryPools();
	void ReleaseMemoryPools();

public:
	void FixupScriptInfos();

private: // XML parser state // bottle this up and keep for only during parsing
	template <size_t _Size>
	bool GetStringAttrib( char (&outString)[_Size], parElement& elt, const char *reference );
	bool GetIntAttrib( int &outInt, parElement& elt, const char *reference );
	bool GetTimeDateAttrib( tm *outTM, class parElement& elt, const char *reference );
	bool GetBoolAttrib(parElement& elt, const char *reference );

	enum ePTXMLStates
	{
		kState_Root,
		kState_Scripts,
		kState_Contexts,
		kState_Context,
		kState_ScriptInfo,
		kState_AssetInfo,
		kState_LaunchParams,
		kState_LaunchParam,
		kState_UtilityScripts,
		kState_Utility,

		kNumXmlStates
	};

	void cbEnterElement(parElement& elt, bool isLeaf );
	void cbLeaveElement(bool isLeaf);
	void cbHandleData( char* data, int UNUSED_PARAM(size), bool UNUSED_PARAM(dataIncomplete) );

	void LoadDataFromPackFile(const char* pMountPath);
	BANK_ONLY(void DumpXMLError(parElement& elt);)
	void ProcessScriptInfo(parElement& elt, ScriptType scriptType);
	bool ProcessScriptInfoData(parElement& elt);
	bool ProcessScriptLaunchParam(parElement& elt);
	void LoadResourcedSCFile(BGScriptInfo* pInfo);
	void LoadData( const char* filename, const char* mountPath );

	void OnCloudEvent(const CloudEvent* pEvent);

private:
	static u32 m_uCloudHash;

	bool m_bIsInitialised; 

	atFixedArray<BGScriptInfo*, MAX_BG_SCRIPTS> m_aScriptInfos;

	ContextMemoryPool m_contextMemoryPool;
	ContextMap m_contextMap;

	void* m_pScriptInfoHeapBuffer;
	sysMemSimpleAllocator* m_pScriptInfoAllocator;

	CategoryMap m_categories;

	atFixedArray<ePTXMLStates, 10> m_XmlStateStack;
	BGScriptInfo* m_pCurrentParsingScript;
	const char* m_pCurrentMountPath;
	bool m_bParsingCloud;

	bool m_bHasCloudScripts;
    bool m_bHasRequestedCloudScripts;
	CloudRequestID m_nCloudRequestID;
    u64 m_nModifiedTime;

	BGScriptInfo::StatFlags m_currentGameMode;

	sysCriticalSectionToken m_token;
	sysCriticalSectionToken m_recentUpdateToken;

#if __BANK
	friend class BackgroundScriptsDebug;
	BackgroundScriptsDebug m_debug;
#endif
};

typedef atSingleton<BackgroundScripts> SBackgroundScripts;

#endif	// _BACKGROUND_SCRIPTS_H_
