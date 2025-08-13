#ifndef INC_PRIORITIZED_CLIP_SET_STREAMER
#define INC_PRIORITIZED_CLIP_SET_STREAMER

// Rage headers
#include "atl/bitset.h"
#include "bank/bank.h"
#include "data/base.h"
#include "grcore/debugdraw.h"
#include "fwanimation/anim_channel.h"
#include "fwanimation/animdefines.h"
#include "fwmaths/random.h"
#include "fwtl/regdrefs.h"
#include "fwutil/Flags.h"
#include "math/math.h"
#include "streaming/streaming_channel.h"
#include "streaming/streamingdefs.h"

// Game headers
#include "animation/debug/AnimDebug.h"
#include "task/system/tuning.h"

#define PRIORITIZED_CLIP_SET_STREAMER_WIDGETS 0

// Rage forward declarations
namespace rage
{
	class fwClipSet;
}

class CEntity;
class CPrioritizedClipSetRequestHelper;

//-----------------------------------------------------------------------------------------
RAGE_DECLARE_SUBCHANNEL(streaming, prioritized)

#define streamPrioritizedAssert(cond)					RAGE_ASSERT(streaming_prioritized,cond)
#define streamPrioritizedAssertf(cond,fmt,...)			RAGE_ASSERTF(streaming_prioritized,cond,fmt,##__VA_ARGS__)
#define streamPrioritizedFatalAssertf(cond,fmt,...)		RAGE_FATALASSERTF(streaming_prioritized,cond,fmt,##__VA_ARGS__)
#define streamPrioritizedVerify(cond)					RAGE_VERIFY(streaming_prioritized,cond)
#define streamPrioritizedVerifyf(cond,fmt,...)			RAGE_VERIFYF(streaming_prioritized,cond,fmt,##__VA_ARGS__)
#define streamPrioritizedErrorf(fmt,...)				RAGE_ERRORF(streaming_prioritized,fmt,##__VA_ARGS__)
#define streamPrioritizedWarningf(fmt,...)				RAGE_WARNINGF(streaming_prioritized,fmt,##__VA_ARGS__)
#define streamPrioritizedDisplayf(fmt,...)				RAGE_DISPLAYF(streaming_prioritized,fmt,##__VA_ARGS__)
#define streamPrioritizedDebugf1(fmt,...)				RAGE_DEBUGF1(streaming_prioritized,fmt,##__VA_ARGS__)
#define streamPrioritizedDebugf2(fmt,...)				RAGE_DEBUGF2(streaming_prioritized,fmt,##__VA_ARGS__)
#define streamPrioritizedDebugf3(fmt,...)				RAGE_DEBUGF3(streaming_prioritized,fmt,##__VA_ARGS__)
#define streamPrioritizedLogf(severity,fmt,...)			RAGE_LOGF(streaming_prioritized,severity,fmt,##__VA_ARGS__)
	//-----------------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
// CPrioritizedClipSetRequest
////////////////////////////////////////////////////////////////////////////////

typedef	fwRegdRef<class CPrioritizedClipSetRequest>			RegdPrioritizedClipSetRequest;
typedef	fwRegdRef<const class CPrioritizedClipSetRequest>	RegdConstPrioritizedClipSetRequest;

class CPrioritizedClipSetRequest : public fwRefAwareBase
{

public:

	FW_REGISTER_CLASS_POOL(CPrioritizedClipSetRequest);
	
public:

	friend class CPrioritizedClipSetBucket;
	
private:

	enum DesiredStreamingState
	{
		DSS_Unknown,
		DSS_ShouldBeStreamedIn,
		DSS_ShouldBeStreamedOut,
	};
	
	enum RunningFlags
	{
		RF_HasAddedReferenceToClipSet	= BIT0,
		RF_HasSetDontDeleteFlag			= BIT1,
	};

public:

	CPrioritizedClipSetRequest(fwMvClipSetId clipSetId);
	virtual ~CPrioritizedClipSetRequest();

public:

	fwMvClipSetId			GetClipSetId()				const { return m_ClipSetId; }
	fwClipSet*				GetClipSet()				const { return m_ClipSet; }
	DesiredStreamingState	GetDesiredStreamingState()	const { return m_nDesiredStreamingState; }
	u8						GetReferences()				const { return m_uReferences; }
	u8						GetTiebreaker()				const { return m_uTiebreaker; }

#if __BANK
	eStreamingPriority		GetStreamingPriorityOverride() const { return m_nStreamingPriorityOverride; }
#endif // __BANK

public:

	void SetStreamingPriorityOverride(eStreamingPriority nStreamingPriority) { m_nStreamingPriorityOverride = nStreamingPriority; }

private:

	void SetDesiredStreamingState(DesiredStreamingState nState) { m_nDesiredStreamingState = nState; }
	
public:

	int			GetClipDictionaryIndex() const;
	bool		IsClipSetStreamedIn() const;
	
public:

	eStreamingPriority GetStreamingPriority() const
	{
		return (m_nStreamingPriorityOverride != SP_Invalid) ? m_nStreamingPriorityOverride : m_nStreamingPriority;
	}

	bool IsClipSetTryingToStreamIn() const
	{
		return (!IsClipSetStreamedIn() && ShouldClipSetBeStreamedIn());
	}

	bool IsClipSetTryingToStreamOut() const
	{
		return (IsClipSetStreamedIn() && ShouldClipSetBeStreamedOut());
	}

	bool ShouldClipSetBeStreamedIn() const
	{
		return (m_nDesiredStreamingState == DSS_ShouldBeStreamedIn);
	}

	bool ShouldClipSetBeStreamedOut() const
	{
		return (m_nDesiredStreamingState == DSS_ShouldBeStreamedOut);
	}

private:

	void AddReferenceToClipSet(fwClipSet& rClipSet);
	void ClearDontDeleteFlag();
	void ClearDontDeleteFlag(fwClipSet& rClipSet);
	void Initialize();
	bool IsClipSetStreamedIn(const fwClipSet& rClipSet) const;
	void ReleaseClipSet();
	void ReleaseClipSet(fwClipSet& rClipSet);
	bool RequestClipSet(fwClipSet& rClipSet);
	void StreamOutClipSet(fwClipSet& rClipSet);
	void Update(fwClipSet& rClipSet);
	
private:

	void AddReference()
	{
		++m_uReferences;
		streamAssert(m_uReferences != 0);
	}

	bool HasAddedReferenceToClipSet() const
	{
		return m_uRunningFlags.IsFlagSet(RF_HasAddedReferenceToClipSet);
	}

	bool HasSetDontDeleteFlag() const
	{
		return m_uRunningFlags.IsFlagSet(RF_HasSetDontDeleteFlag);
	}

	bool IsUnreferenced() const
	{
		return (m_uReferences == 0);
	}
	
	void RandomizeTiebreaker()
	{
		//For now, tiebreakers are only used for variations.
		if(m_nStreamingPriority != SP_Variation)
		{
			return;
		}
		
		//Randomize the tiebreaker.
		m_uTiebreaker = (u8)fwRandom::GetRandomNumberInRange(0, 255);
	}
	
	void RemoveReference()
	{
		streamAssert(m_uReferences != 0);
		--m_uReferences;
	}

private:

	fwClipSet*				m_ClipSet;
	fwMvClipSetId			m_ClipSetId;
	eStreamingPriority		m_nStreamingPriority : 16;
	eStreamingPriority		m_nStreamingPriorityOverride : 16;
	DesiredStreamingState	m_nDesiredStreamingState : 8;
	fwFlags8				m_uRunningFlags;
	u8						m_uReferences;
	u8						m_uTiebreaker;

};

#if PRIORITIZED_CLIP_SET_STREAMER_WIDGETS
////////////////////////////////////////////////////////////////////////////////
// CPrioritizedClipSetBucketDebug
////////////////////////////////////////////////////////////////////////////////

struct CPrioritizedClipSetBucketDebug
{
	struct Rendering
	{
		Rendering()
		: m_bIsEnabled(true)
		{}
		
		bool m_bIsEnabled;
	};
	
	struct Budget
	{
		Budget()
		: m_bUseMultiplierOverride(false)
		, m_fMultiplierOverride(1.0f)
		{}
		
		bool	m_bUseMultiplierOverride;
		float	m_fMultiplierOverride;
	};
	
	CPrioritizedClipSetBucketDebug()
	: m_Rendering()
	, m_pGroup(NULL)
	{}
	
	Rendering	m_Rendering;
	Budget		m_Budget;
	bkGroup*	m_pGroup;
};
#endif

////////////////////////////////////////////////////////////////////////////////
// CPrioritizedClipSetBucket
////////////////////////////////////////////////////////////////////////////////

class CPrioritizedClipSetBucket
{

	friend class CPrioritizedClipSetStreamer;

public:

	FW_REGISTER_CLASS_POOL(CPrioritizedClipSetBucket);
	
private:

	enum RunningFlags
	{
		RF_SortRequests	= BIT0,
	};
	
private:

	struct MemoryBudget
	{
		MemoryBudget()
		: m_uInitial(0)
		, m_uCurrent(0)
		, m_iAdjustment(0)
		, m_fMultiplier(0.0f)
		{}
		
		u32		m_uInitial;
		u32		m_uCurrent;
		s32		m_iAdjustment;
		float	m_fMultiplier;
	};
	
	struct MemoryUsage
	{
		MemoryUsage()
		: m_uCurrent(0)
		{}
		
		u32 m_uCurrent;
	};

public:

	CPrioritizedClipSetBucket(atHashWithStringNotFinal hMemoryGroup);
	~CPrioritizedClipSetBucket();
	
public:

	atHashWithStringNotFinal GetMemoryGroup() const { return m_hMemoryGroup; }
	
public:

	void DecreaseMemoryUsage(u32 uAmount)
	{
		animAssert(m_MemoryUsage.m_uCurrent >= uAmount);
		m_MemoryUsage.m_uCurrent -= uAmount;
	}

	u32 GetMemoryUsage() const
	{
		return m_MemoryUsage.m_uCurrent;
	}

	bool HasRequests() const
	{
		return !m_aRequests.IsEmpty();
	}

	void IncreaseMemoryUsage(u32 uAmount)
	{
		m_MemoryUsage.m_uCurrent += uAmount;
	}

	bool IsEmpty() const
	{
		return (m_aRequests.GetCount() <= 0);
	}

	void SetMemoryBudgetAdjustment(int iAdjustment)
	{
		m_MemoryBudget.m_iAdjustment = iAdjustment;
	}
	
public:

	CPrioritizedClipSetRequest*	AddRequest(fwMvClipSetId clipSetId);
	void						Initialize();
	void						RandomizeTiebreakers();
	void						RemoveRequest(fwMvClipSetId clipSetId);
	void						Update();

public:

	static bool Compare(const CPrioritizedClipSetRequest* pRequest, const CPrioritizedClipSetRequest* pOtherRequest);
	
public:

#if PRIORITIZED_CLIP_SET_STREAMER_WIDGETS
	void						AddWidgets(bkGroup& rGroup);
	CPrioritizedClipSetRequest* GetRandomRequest() const;
	void						RenderDebug(const Vector2& vPosition, int& iLine) const;
#endif
	
private:

	CPrioritizedClipSetRequest*			CreateRequest(fwMvClipSetId clipSetId);
	void								DestroyRequest(int iIndex);
	void								DestroyRequest(CPrioritizedClipSetRequest* pRequest);
	CPrioritizedClipSetRequest*			FindRequest(fwMvClipSetId clipSetId);
	const CPrioritizedClipSetRequest*	FindRequest(fwMvClipSetId clipSetId) const;
	int									FindRequestIndex(fwMvClipSetId clipSetId) const;
	float								GenerateMultiplierForMemoryBudget() const;
	void								SortRequests();
	void								UpdateMemoryBudget();
	void								UpdateRequests();
	
private:

	static const int sMaxRequests = 128;
	
private:

	atFixedArray<CPrioritizedClipSetRequest *, sMaxRequests>	m_aRequests;
	atHashWithStringNotFinal									m_hMemoryGroup;
	MemoryBudget												m_MemoryBudget;
	MemoryUsage													m_MemoryUsage;
	fwFlags8													m_uRunningFlags;
	
private:

#if PRIORITIZED_CLIP_SET_STREAMER_WIDGETS
	CPrioritizedClipSetBucketDebug m_Debug;
#endif

};

#if PRIORITIZED_CLIP_SET_STREAMER_WIDGETS
////////////////////////////////////////////////////////////////////////////////
// CPrioritizedClipSetStreamerDebug
////////////////////////////////////////////////////////////////////////////////

struct CPrioritizedClipSetStreamerDebug
{
	struct Rendering
	{
		struct Requests
		{
			Requests()
			: m_bIsEnabled(false)
			{}

			bool m_bIsEnabled;
		};

		struct ClipDictionaries
		{
			ClipDictionaries()
			: m_bCountedAgainstCost(false)
			, m_bNotCountedAgainstCost(false)
			{
				m_aMemoryGroupNameFilter[0] = '\0';
			}

			bool m_bCountedAgainstCost;
			bool m_bNotCountedAgainstCost;
			char m_aMemoryGroupNameFilter[256];
		};

		Rendering()
		: m_Requests()
		, m_ClipDictionaries()
		, m_vScroll(0.0f, 0.0f)
		{}
		
		Requests			m_Requests;
		ClipDictionaries	m_ClipDictionaries;
		Vector2				m_vScroll;
	};

	struct Metadata : public datBase
	{
		Metadata()
		{}

		void DumpMissingToOutput();
	};

	struct RequestController
	{
		struct Individual : public datBase
		{
			Individual()
			: m_ClipSelector(false, true, false)
			, m_iPriorityIndex(0)
			{}
			
			void AddRequest();
			void RemoveRequest();
			
			CDebugClipSelector	m_ClipSelector;
			int					m_iPriorityIndex;
		};

		struct Bulk : public datBase
		{
			Bulk()
			: m_iAmount(10)
			{}
			
			void AddRandomRequests();
			void RemoveRandomRequests();
			
			int m_iAmount;
		};
		
		RequestController()
		: m_Individual()
		, m_Bulk()
		{}
		
		Individual	m_Individual;
		Bulk		m_Bulk;
	};

	CPrioritizedClipSetStreamerDebug()
	: m_Rendering()
	, m_Metadata()
	, m_RequestController()
	, m_pGroup(NULL)
	{}
	
	Rendering			m_Rendering;
	Metadata			m_Metadata;
	RequestController	m_RequestController;
	bkGroup*			m_pGroup;
};
#endif

////////////////////////////////////////////////////////////////////////////////
// CPrioritizedClipSetStreamer
////////////////////////////////////////////////////////////////////////////////

class CPrioritizedClipSetStreamer
{

public:

	static CPrioritizedClipSetStreamer& GetInstance() { FastAssert(sm_Instance); return *sm_Instance; }

private:

	CPrioritizedClipSetStreamer();
	~CPrioritizedClipSetStreamer();
	
public:

	CPrioritizedClipSetRequest*			AddRequest(fwMvClipSetId clipSetId);
	u32									CalculateMemoryCostForClipDictionary(strLocalIndex iIndex) const;
	u32									CalculateMemoryCostForClipSetToStreamIn(const fwClipSet& rClipSet, atHashWithStringNotFinal hMemoryGroup) const;
	const CPrioritizedClipSetRequest*	FindRequest(fwMvClipSetId clipSetId) const;
	atHashWithStringNotFinal			GetMemoryGroupForClipDictionary(strLocalIndex iIndex) const;
	void								Initialize();
	void								OnClipDictionaryAllRefsReset(strLocalIndex iIndex);
	void								OnClipDictionaryLoaded(strLocalIndex iIndex);
	void								OnClipDictionaryRemoved(strLocalIndex iIndex);
	void								OnClipDictionaryRefAdded(strLocalIndex iIndex);
	void								OnClipDictionaryRefRemoved(strLocalIndex iIndex);
	void								OnClipDictionaryRefRemovedWithoutDelete(strLocalIndex iIndex);
	void								RemoveRequest(fwMvClipSetId clipSetId);
	bool								ShouldClipDictionaryCountAgainstMemoryCost(strLocalIndex iIndex) const;
	void								Update(float fTimeStep);

public:

	bool IsClipDictionaryCountedAgainstMemoryCost(int iIndex) const
	{
		return m_IsClipDictionaryCountedAgainstMemoryCost.IsSet(iIndex);
	}
	
public:

	static void	Init(unsigned initMode);
	static void	Shutdown(unsigned shutdownMode);
	static void Update();
	
public:

#if PRIORITIZED_CLIP_SET_STREAMER_WIDGETS
	void						AddWidgets(bkBank& rBank);
	CPrioritizedClipSetBucket*	GetRandomBucket() const;
	void						RemoveWidgets();
	void						RenderDebug() const;
#endif
	
private:

	void								ApplyMemoryBudgetAdjustments();
	atHashWithStringNotFinal			CalculateSituation() const;
	bool								CanUpdateSituation() const;
	CPrioritizedClipSetBucket*			CreateBucket(atHashWithStringNotFinal hMemoryGroup);
	CPrioritizedClipSetBucket*			FindBucket(fwMvClipSetId clipSetId);
	const CPrioritizedClipSetBucket*	FindBucket(fwMvClipSetId clipSetId) const;
	CPrioritizedClipSetBucket*			FindBucket(atHashWithStringNotFinal hMemoryGroup);
	const CPrioritizedClipSetBucket*	FindBucket(atHashWithStringNotFinal hMemoryGroup) const;
	int									FindBucketIndex(fwMvClipSetId clipSetId) const;
	int									FindBucketIndex(atHashWithStringNotFinal hMemoryGroup) const;
	atHashString						GetMemoryGroup(fwMvClipSetId clipSetId) const;
	void								InitializeBuckets();
	void								InitializeMemoryCosts();
	void								OnSituationChanged();
	void								RandomizeTiebreakers();
	bool								ShouldRandomizeTiebreakers() const;
	void								UpdateBuckets();
	void								UpdateMemoryCostForClipDictionary(strLocalIndex iIndex);
	void								UpdateMemoryCosts();
	void								UpdateSituation();
	void								UpdateTimers(float fTimeStep);
	
private:

	static const int sMaxBuckets = 32;
	
private:

	atBitSet												m_IsClipDictionaryCountedAgainstMemoryCost;
	atFixedArray<CPrioritizedClipSetBucket *, sMaxBuckets>	m_aBuckets;
	float													m_fTimeSinceLastTiebreakersRandomization;
	int														m_iClipDictionaryIndexToCheck;
	atHashWithStringNotFinal								m_hSituation;
	
private:

	static CPrioritizedClipSetStreamer* sm_Instance;
	
private:

#if PRIORITIZED_CLIP_SET_STREAMER_WIDGETS
	CPrioritizedClipSetStreamerDebug m_Debug;
#endif
	
};

////////////////////////////////////////////////////////////////////////////////

#define DEFINE_ENUM_FROM_LIST(S) S
#define DEFINE_STRINGS_FROM_LIST(S) #S

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// CPrioritizedClipSetRequestManager
//
// PURPOSE: Manager to handle the prioritized streaming in of clipsets for gameplay systems
//	- consolidates all requests for a given frame into a single prioritized clipset request per clipset
//	- scores and alters the priorities of the requests for optimal performance and maxium variety
////////////////////////////////////////////////////////////////////////////////

class CPrioritizedClipSetRequestManager
{
public:

	static bool ms_SortRequests;
#if __DEV
	static u32 ms_TotalRequestAllocationSize;
	static u32 ms_TotalHelperAllocationSize;
#endif // __DEV

#if __BANK
	static bool ms_bSpewDetailedRequests;
	static bool ms_bSpewDetailedRequests2;
#endif // __BANK

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		bool	m_RenderDebugDraw;
		Vector2	m_vScroll;
		float	m_fIndent;
		s32		m_MaxNumRequestsPerContext;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

	CPrioritizedClipSetRequestManager();
	~CPrioritizedClipSetRequestManager();

	static CPrioritizedClipSetRequestManager& GetInstance() { FastAssert(ms_Instance); return *ms_Instance; }

	typedef float (ClipSetScoringFn) ( void* pData );

	// PURPOSE: Macro to allow us to define the enum and string lists once
	#define REQUEST_CONTEXTS(X)	X(RC_Default),			\
								X(RC_Vehicle),			\
								X(RC_Cover),			\
								X(RC_GestureVehicle),	\
								X(RC_GestureScenario),	\
								X(RC_Max),				\

	enum eRequestContext
	{
		REQUEST_CONTEXTS(DEFINE_ENUM_FROM_LIST)
	};

#if DEBUG_DRAW
	static const char* szContextNames[];
#endif // DEBUG_DRAW

	struct sScoringParams
	{
		CEntity* entity;
	};

	struct sRequest
	{
#if DEBUG_DRAW
		void RenderDebug(const Vector2& vPosition, int& iLine, bool bClipSetLoaded) const;
#endif // DEBUG_DRAW

		sRequest(fwMvClipSetId clipSetId, eRequestContext context = RC_Default, eStreamingPriority desiredPriority = SP_Invalid, float fLifeTime = 1.0f)
			: clipsetId(clipSetId)
			, context(context)
			, priority(desiredPriority)
			, pScoringFn(NULL)
			, pScoringData(NULL)
			, fRequestLifeTime(fLifeTime)
			, iNumRequests(0)
		{

		}

		sRequest()
			: clipsetId(CLIP_SET_ID_INVALID)
			, context(RC_Default)
			, priority(SP_Invalid)
			, pScoringFn(NULL)
			, pScoringData(NULL)
			, fRequestLifeTime(1.0f)
			, iNumRequests(0)
		{

		}

		sRequest(const sRequest& other)
			: clipsetId(other.clipsetId)
			, context(other.context)
			, priority(other.priority)
			, pScoringFn(other.pScoringFn)
			, pScoringData(other.pScoringData)
			, fRequestLifeTime(other.fRequestLifeTime)
			, iNumRequests(other.iNumRequests)
		{

		}

		fwMvClipSetId		clipsetId;
		eRequestContext		context;
		eStreamingPriority	priority;
		ClipSetScoringFn*	pScoringFn;
		void*				pScoringData;
		float				fRequestLifeTime;
		s32					iNumRequests;
	};

	static void	Init(unsigned initMode);
	static void	Shutdown(unsigned shutdownMode);

	static bool Compare(const sRequest* pRequest, const sRequest* pOtherRequest);

	void Init();
	void Shutdown();
	bool ShouldClipSetBeStreamedOut(eRequestContext context, fwMvClipSetId clipSetId);

	bool Request(eRequestContext context, fwMvClipSetId clipSetId, eStreamingPriority priority, float fLifeTime, bool bCheckLoaded = true);
	bool IsLoaded(fwMvClipSetId clipSetId);
	void Update(float fTimeStep);

	bool HasClipSetBeenRequested(eRequestContext context, fwMvClipSetId clipSetId);

#if __BANK
	const CPrioritizedClipSetRequestHelper* GetPrioritizedClipSetRequestHelperForClipSet(eRequestContext context, fwMvClipSetId clipSetId);
#endif // __BANK

#if DEBUG_DRAW
	void RenderDebug();
	void RenderHelperDebug(const Vector2& vPosition, int& iLine, const CPrioritizedClipSetRequestHelper& helper) const;
#endif // DEBUG_DRAW

private:

	fwClipSet* GetClipSet(fwMvClipSetId clipSetId) const;
	s32	 GetRequestHelperIndexIfExists(eRequestContext context, fwMvClipSetId clipSetId);
	s32	 GetRequestIndexIfExists(eRequestContext context, fwMvClipSetId clipSetId);
	void UpdateRequest(sRequest& rOldRequest, const sRequest& rNewRequest);
	void UpdateRequest(eRequestContext context, s32 iOldRequestIndex, const sRequest& rNewRequest) { UpdateRequest(*m_aClipSetRequests[context].aRequests[iOldRequestIndex], rNewRequest); }
	bool Request(sRequest& request, bool bCheckLoaded = true);
	//void ScoreAndSortRequests(eRequestContext context);
	//void RemoveDuplicatesFromSortedArray(eRequestContext context);
#if __DEV
	void CheckForDuplicates(eRequestContext context) const;
#endif // __DEV
	bool IsOtherRequestADupe(eRequestContext context, u32 uCurrentUniqueElementIndex, s32 iOtherRequestIndex) const;
	void CopyDetailsToUniqueElement(eRequestContext context, u32 uCurrentUniqueElementIndex, s32 iUniqueRequestIndex, s32 iNumDuplicatesFound);
	void ProcessRequests(eRequestContext context, float fTimeStep);
	void AddRequestHelperIfNoneExists(eRequestContext context);
	s32	 FindRequestWithClipSetId(eRequestContext context, fwMvClipSetId clipSetId);
	void SortRequests(eRequestContext context);

	struct sClipSetHelpers
	{
		atArray<CPrioritizedClipSetRequestHelper*> aPrioritisedClipSetRequestHelpers;
	};

	struct sClipSetRequests
	{
		atArray<sRequest*> aRequests;
	};

	atFixedArray<sClipSetRequests, RC_Max>		m_aClipSetRequests;
	atFixedArray<sClipSetHelpers, RC_Max>		m_aClipSetHelpers;

	// PURPOSE: Static instance of the clipset request manager
	static CPrioritizedClipSetRequestManager* ms_Instance;
};

////////////////////////////////////////////////////////////////////////////////

#endif // INC_PRIORITIZED_CLIP_SET_STREAMER
