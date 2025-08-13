#ifndef TASK_HELPERS_H
#define TASK_HELPERS_H

// Rage headers
#include "vector/matrix34.h"
#include "vectormath/vec3v.h"
#include "zlib/zlib.h"		// uses crc
#include "atl/ptr.h"

// Framework headers
#include "ai/navmesh/datatypes.h"
#include "ai/navmesh/requests.h"
#include "ai/task/taskchannel.h"
#include "ai/task/task.h"
#include "fwanimation/animdirector.h"
#include "fwanimation/animhelpers.h"
#include "fwanimation/move.h" // for Move::Handle
#include "fwMaths/random.h"
#include "fwsys/timer.h"
#include "fwutil/Flags.h"
#include "move/move_observer.h"
#include "streaming/streamingmodule.h"

// Game headers
#include "ai/AnimBlackboard.h"
#include "AI/Ambient/ConditionalAnims.h"
#include "animation/AnimDefines.h"
#include "animation/EventTags.h"
#include "animation/PMDictionaryStore.h"
#include "camera/helpers/DampedSpring.h"
#include "Debug/Debug.h"
#include "ik/IkRequest.h"
#include "Peds/PedWeapons/PedTargeting.h"
#include "Peds/pedDefines.h"
#include "pathserver/PathServer_DataTypes.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/DynamicEntity.h"
#include "streaming/PrioritizedClipSetStreamer.h"
#include "Streaming/streamingrequest.h"
#include "Task/Physics/BlendFromNmData.h"
#include "task/System/AsyncProbeHelper.h"
#include "task/system/TaskHelperFSM.h"
#include "Task/System/Tuning.h"
#include "animation/Move_config.h"
#include "VehicleAi/VehicleNodeList.h"
#include "control\WaypointRecording.h"

// rage forward declarations
namespace rage {
class crClip;
}

// Game forward declarations
struct CAmbientAudio;
struct CAmbientAudioExchange;
class CPed;
class CEntity;
class CDynamicEntity;
class CConditionalAnimsGroup;
class CCoverPoint;
class CHeli;
class CTask;
class CTaskAmbientMigrateHelper;
struct sVehicleMissionParams;

#define MAX_EXAMINABLE_CLIPS 64

////////////////////////////////////////////////////////////////////////////////

// PURPOSE: Static helper functions
class CMoveNetworkHelperFunctions
{
public:

	// PURPOSE: Sends state request or forces into the state and waits for the on enter event from the target state
	static void SetMoveNetworkState(const fwMvRequestId& requestId, const fwMvBooleanId& onEnterId, const fwMvStateId& stateId, CTask* pTask, const CPed& ped, bool ignoreCloneCheck = false);

	// PURPOSE: Checks to see if the move network is in the state the task expects it to be in
	static bool IsMoveNetworkStateValid(const CTask* pTask, const CPed& ped);

	// PURPOSE: Returns true if the anim finished event has been received
	static bool IsMoveNetworkStateFinished(const fwMvBooleanId& animFinishedEventId, const fwMvFloatId& phaseId, const CTask* pTask, const CPed& ped) ;
};

////////////////////////////////////////////////////////////////////////////////

// PURPOSE: Wrapper macro for tasks to change and query the ai / anim states and log the details to the ai_task_tty as we do it
#define DEFINE_MOVE_NETWORK_HELPER_FUNCTIONS \
void SetTaskState(s32 iState)\
{\
	AI_LOG_WITH_ARGS("[%s:%p] - %s %s : changed state from %s to %s\n", GetTaskName(), this, AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()), GetStateName(GetState()), GetStateName(iState));\
	SetState(iState);\
}\
void SetMoveNetworkState(const fwMvRequestId& requestId, const fwMvBooleanId& onEnterId, const fwMvStateId& stateId, bool ignoreCloneCheck = false)\
{\
	CMoveNetworkHelperFunctions::SetMoveNetworkState(requestId, onEnterId, stateId, this, *GetPed(), ignoreCloneCheck);\
}\
bool IsMoveNetworkStateValid() const\
{\
	return CMoveNetworkHelperFunctions::IsMoveNetworkStateValid(this, *GetPed());\
}\
bool IsMoveNetworkStateFinished(const fwMvBooleanId& animFinishedEventId, const fwMvFloatId& phaseId) const\
{\
	return CMoveNetworkHelperFunctions::IsMoveNetworkStateFinished(animFinishedEventId, phaseId, this, *GetPed());\
}\

/* end of DEFINE_MOVE_NETWORK_HELPER_FUNCTIONS */

////////////////////////////////////////////////////////////////////////////////

// CTaskSimpleTimer
// Simple timer class that can be intialised with a random min/max range.
// The owner (Task) is responsible for updating the local timer using the tick command.
class CTaskSimpleTimer
{
public:
	// Constructor with no time
	CTaskSimpleTimer() { Reset(0.f); }
	// Constructor with an exact reset time
	CTaskSimpleTimer(float fStartTime) { Reset(fStartTime); }
	// Constructor with min max times, a random time will be chosen between these
	CTaskSimpleTimer(float fStartTimeMin, float fStartTimeMax) { Reset(fStartTimeMin, fStartTimeMax); }

	// Resets the timer to an exact time
	void Reset(float fStartTime) { m_fTimer = fStartTime; }

	// Resets the timer to a random number between m_fStartTimeMin and Max
	void Reset(float fStartTimeMin, float fStartTimeMax) { m_fTimer = fwRandom::GetRandomNumberInRange(fStartTimeMin, fStartTimeMax); }

	// Tick the timer on one time step, returns true if it reaches 0
	bool Tick() { if(m_fTimer > 0.0f) m_fTimer -= fwTimer::GetTimeStep(); return IsFinished(); }
	bool Tick(float fTimeStep) { if(m_fTimer > 0.0f) m_fTimer -= fTimeStep; return IsFinished(); }

	// Returns true if the timer has reached 0
	bool IsFinished() const { return m_fTimer <= 0.0f; }

	// Make our timer finish immediately
	void ForceFinish() { m_fTimer = 0.0f; }

	// Get the time
	float GetCurrentTime() const { return m_fTimer; }

private:
	float m_fTimer;
};

// CTaskTimer
// Simple timer class that is intialised with a reset range.
// The owner (Task) is responsible for updating the local timer using the tick command.
// Reset fn resets the range to within the initial parameters
class CTaskTimer : public CTaskSimpleTimer
{
public:
	// Constructor with an exact reset time
	CTaskTimer(float fStartTime)
		: m_fStartTimeMin(fStartTime)
		, m_fStartTimeMax(fStartTime)
	{
		Reset();
	}
	// Constructor with min max times, a random time will be chosen between these at reset
	CTaskTimer(float fStartTimeMin, float fStartTimeMax)
		: m_fStartTimeMin(fStartTimeMin)
		, m_fStartTimeMax(fStartTimeMax)
	{
		Reset();
	}

	// Resets the timer to a random number between m_fStartTimeMin and Max
	void Reset() { CTaskSimpleTimer::Reset(m_fStartTimeMin, m_fStartTimeMax); }

	void SetTime(float fStartTime) { m_fStartTimeMin = fStartTime; m_fStartTimeMax = fStartTime; }
	void SetTime(float fStartTimeMin, float fStartTimeMax) { m_fStartTimeMin = fStartTimeMin; m_fStartTimeMax = fStartTimeMax; }
	float GetStartTime() const { return m_fStartTimeMin; }

private:
	float m_fStartTimeMin;
	float m_fStartTimeMax;
};

// Simple timer class that progresses in time with the main game timer (stops when the game is paused)
class CTaskGameTimer
{
public:

	CTaskGameTimer()
		: m_iStartTime(0),
		m_iDuration(0),
		m_bIsActive(false),
		m_bIsStopped(false)
	{
	}

	CTaskGameTimer(const int iStartTime, const int iDuration)
		: m_iStartTime(iStartTime),
		m_iDuration(iDuration),
		m_bIsActive(false),
		m_bIsStopped(false)
	{
	}

	void Set(const int iStartTime, const int iDuration)
	{
		m_iStartTime=iStartTime;
		m_iDuration=iDuration;
		m_bIsActive=true;
	}

	void AddTime(const int iExtraTime)
	{
		Set(fwTimer::GetTimeInMilliseconds(),iExtraTime);
		//Assert(IsSet());
		//m_iDuration+=iExtraTime;
	}

	void SubtractTime(const int iLessTime)
	{
		m_iStartTime -= iLessTime;
	}

	void Unset()
	{
		m_bIsActive=false;
	}

	bool IsSet() const
	{
		return ((bool)m_bIsActive);
	}

	bool IsOutOfTime() const
	{
		if(m_bIsActive)
		{
			if(m_bIsStopped)
			{
				m_iStartTime=fwTimer::GetTimeInMilliseconds();
				m_bIsStopped=false;
			}
			return ((m_iStartTime+m_iDuration) <=(s32) fwTimer::GetTimeInMilliseconds());
		}
		else
		{
			return false;
		}
	}

	s32 GetTimeLeft() const
	{
		if (m_bIsActive)
		{
			return (m_iStartTime+m_iDuration) - fwTimer::GetTimeInMilliseconds();
		}
		else
		{
			// should we be asking for the time left on an uninitialised timer?
			Assert(0);
			return 0;
		}
	}

	s32 GetTimeElapsed () const
	{
		if (m_bIsActive)
		{
			return fwTimer::GetTimeInMilliseconds() - m_iStartTime;
		}
		else
		{
			// should we be asking for the time elapsed on an uninitialised timer?
			Assert(0);
			return 0;
		}
	}

	s32 GetDuration() const 
	{
		return m_iDuration;
	}
	void ChangeDuration(const s32 d)
	{
		m_iDuration = d;
	}

	void Stop()
	{
		if(m_bIsActive)
		{
			m_bIsStopped=true;
			m_iDuration=m_iDuration-GetTimeElapsed();
		}
	}

private:

	mutable int m_iStartTime;
	int m_iDuration;
	bool m_bIsActive;
	mutable bool m_bIsStopped;
};

// Simple timer class that progresses in time with the systemtimer (continues when the game is paused)
class CTaskSystemTimer
{
public:

	CTaskSystemTimer()
		: m_iStartTime(0),
		m_iDuration(0),
		m_bIsActive(false),
		m_bIsStopped(false)
	{
	}

	CTaskSystemTimer(const int iStartTime, const int iDuration)
		: m_iStartTime(iStartTime),
		m_iDuration(iDuration),
		m_bIsActive(false),
		m_bIsStopped(false)
	{
	}

	void Set(const int iStartTime, const int iDuration)
	{
		m_iStartTime=iStartTime;
		m_iDuration=iDuration;
		m_bIsActive=true;
	}

	void AddTime(const int iExtraTime)
	{
		Set(fwTimer::GetSystemTimeInMilliseconds(),iExtraTime);
		//Assert(IsSet());
		//m_iDuration+=iExtraTime;
	}

	void Unset()
	{
		m_bIsActive=false;
	}

	bool IsSet() const
	{
		return ((bool)m_bIsActive);
	}

	bool IsOutOfTime() const
	{
		if(m_bIsActive)
		{
			if(m_bIsStopped)
			{
				m_iStartTime=fwTimer::GetSystemTimeInMilliseconds();
				m_bIsStopped=false;
			}
			return ((m_iStartTime+m_iDuration) <=(s32) fwTimer::GetSystemTimeInMilliseconds());
		}
		else
		{
			return false;
		}
	}

	s32 GetTimeLeft() const
	{
		if (m_bIsActive)
		{
			return (m_iStartTime+m_iDuration) - fwTimer::GetSystemTimeInMilliseconds();
		}
		else
		{
			// should we be asking for the time left on an uninitialised timer?
			Assert(0);
			return 0;
		}
	}

	s32 GetTimeElapsed () const
	{
		if (m_bIsActive)
		{
			return fwTimer::GetSystemTimeInMilliseconds() - m_iStartTime;
		}
		else
		{
			// should we be asking for the time elapsed on an uninitialised timer?
			Assert(0);
			return 0;
		}
	}

	s32 GetDuration() const 
	{
		return m_iDuration;
	}

	void Stop()
	{
		if(m_bIsActive)
		{
			m_bIsStopped=true;
			m_iDuration=m_iDuration-GetTimeElapsed();
		}
	}

private:

	mutable int m_iStartTime;
	int m_iDuration;
	bool m_bIsActive;
	mutable bool m_bIsStopped;
};

class CTickCounter
{
public:

	inline CTickCounter()
		: m_iCount(0),
		m_iPeriod(0)
	{
	}

	inline ~CTickCounter()
	{
	}

	inline void SetPeriod(const int iPeriod)
	{
		m_iPeriod=iPeriod;
	}

	inline bool Tick() 
	{
		bool bReturnValue=false;
		if(0==m_iCount)
		{
			bReturnValue=true;
		}
		m_iCount++;
		if(m_iCount>m_iPeriod)
		{
			m_iCount=0;
		}
		return bReturnValue;
		/*
		bool bReturnValue=false;
		m_iCount++;
		if(m_iCount>m_iPeriod)
		{
		m_iCount=0;
		bReturnValue=true;
		}
		return bReturnValue;
		*/
	}

	inline int GetPeriod(void) { return m_iPeriod; }
	inline void SetCount(int c) { m_iCount = c; }

private:

	int m_iCount;
	int m_iPeriod;
};

//
// helper class to be included in tasks that wish to stream their clips
// all you have to do is make the helper a member of the task, and then RequestClips by group
// and wait for that function to return true before using the clips
//
class CClipRequestHelper
{
public:
	CClipRequestHelper();
	~CClipRequestHelper();

	bool RequestClipsByIndex(s32 clipDictIndex);
	void ReleaseClips();

	bool HaveClipsBeenLoaded() 
	{
		bool bHasLoaded = m_request.HasLoaded();
		if (bHasLoaded)
		{
			m_request.ClearRequiredFlags(STRFLAG_DONTDELETE);
		}
		return bHasLoaded;
	}

	s32  GetCurrentlyRequiredDictIndex() { return m_nRequiredDictIndex; }
	s32  GetCurrentlyRequiredDictIndexConst() const { return m_nRequiredDictIndex; }

	CClipRequestHelper(const CClipRequestHelper& that)
	: m_nRequiredDictIndex((u32)-1)
	{
		RequestClipsByIndex(that.m_nRequiredDictIndex);
	}

	CClipRequestHelper& operator=(const CClipRequestHelper& that) 
	{
		RequestClipsByIndex(that.m_nRequiredDictIndex);
		return *this;
	}

private:
	strRequest  m_request;
	s32         m_nRequiredDictIndex;
};


#if USE_PM_STORE
//
// helper class to be included in tasks that wish to stream their paramatised motion templates and clips
// all you have to do is make the helper a member of the task, and then RequestPm by dictionary index and hashkey
//
class CPmRequestHelper
{
public:
	CPmRequestHelper();
	virtual ~CPmRequestHelper();

	bool RequestPm(s32 nRequiredPmDict, s32 flags = 0);
	void ReleasePm() {RequestPm(-1);}

	bool IsPmLoaded() {return m_bPmReferenced;}

private:
	s32 m_nRequiredPmDict;
	u16 m_bPmReferenced:1;
};
#endif // USE_PM_STORE

// PURPOSE: Allows tasks to request multiple clipsets
template<s32 _size>
class CMultipleClipSetRequestHelper
{
public:

	CMultipleClipSetRequestHelper()
		: m_iNumClipSetHelpers(0)
	{};
	~CMultipleClipSetRequestHelper() {};

	inline void AddClipSetRequest(const fwMvClipSetId &requestedClipSetId);
	inline bool RequestAllClipSets();
	inline void ReleaseAllClipSets();
	inline bool IsClipSetLoaded(const fwMvClipSetId &requestedClipSetId) const;
	inline bool AreAllClipSetsLoaded() const;
	inline bool HaveAddedClipSet(const fwMvClipSetId &requestedClipSetId);
	inline s32 GetNumClipSetRequests() { return m_iNumClipSetHelpers; };

private:

	fwClipSetRequestHelper m_ClipSetRequestHelpers[_size];
	s32 m_iNumClipSetHelpers;
};

template <s32 _size>
void CMultipleClipSetRequestHelper<_size>::AddClipSetRequest(const fwMvClipSetId &requestedClipSetId)
{
	if( taskVerifyf(m_iNumClipSetHelpers < _size, "Maximum number of clipsets added") )
	{
		if( taskVerifyf(!HaveAddedClipSet(requestedClipSetId), "Already added this clipset to the list of requests") )
		{
			m_ClipSetRequestHelpers[m_iNumClipSetHelpers].Request(requestedClipSetId);
			++m_iNumClipSetHelpers;
		}
	}
}

template <s32 _size>
bool CMultipleClipSetRequestHelper<_size>::RequestAllClipSets()
{
	bool bAllClipSetsLoaded = true;

	for (s32 i=0; i<m_iNumClipSetHelpers; i++)
	{
		if (!m_ClipSetRequestHelpers[i].Request())
		{
			bAllClipSetsLoaded = false;
		}
	}
	return bAllClipSetsLoaded;
}

template <s32 _size>
void CMultipleClipSetRequestHelper<_size>::ReleaseAllClipSets()
{
	for (s32 i=0; i<m_iNumClipSetHelpers; i++)
	{
		m_ClipSetRequestHelpers[i].Release();
	}
	
	m_iNumClipSetHelpers = 0;
}

template <s32 _size>
bool CMultipleClipSetRequestHelper<_size>::IsClipSetLoaded(const fwMvClipSetId &requestedClipSetId) const
{
	for (s32 i=0; i<m_iNumClipSetHelpers; i++)
	{
		if (m_ClipSetRequestHelpers[i].GetClipSetId() == requestedClipSetId)
		{
			return m_ClipSetRequestHelpers[i].IsLoaded();
		}
	}
	return false;
}

template <s32 _size>
bool CMultipleClipSetRequestHelper<_size>::AreAllClipSetsLoaded() const
{
	for (s32 i=0; i<m_iNumClipSetHelpers; i++)
	{
		if (!m_ClipSetRequestHelpers[i].IsLoaded())
		{
			return false;
		}
	}
	
	return true;
}

template <s32 _size>
bool CMultipleClipSetRequestHelper<_size>::HaveAddedClipSet(const fwMvClipSetId &requestedClipSetId)
{
	for (s32 i=0; i<m_iNumClipSetHelpers; i++)
	{
		if (m_ClipSetRequestHelpers[i].GetClipSetId() == requestedClipSetId)
		{
			return true;
		}
	}
	return false;
}

class CPrioritizedClipSetRequestHelper
{

public:

	CPrioritizedClipSetRequestHelper();
	~CPrioritizedClipSetRequestHelper();

public:

	//Note: These were added for compatibility with the old clip set request helper API.

	bool IsLoaded() const
	{
		return IsClipSetStreamedIn();
	}

	void Release()
	{
		ReleaseClipSet();
	}

	void Request()
	{

	}

	void Request(fwMvClipSetId clipSetId)
	{
		RequestClipSet(clipSetId);
	}

#if __BANK
	const CPrioritizedClipSetRequest* GetPrioritizedClipSetRequest() const { return m_pRequest; }
#endif // __BANK

public:

	bool			CanUseClipSet() const;
	fwClipSet*		GetClipSet() const;
	fwMvClipSetId	GetClipSetId() const;
	bool			IsActive() const;
	bool			IsClipSetStreamedIn() const;
	bool			IsClipSetTryingToStreamIn() const;
	bool			IsClipSetTryingToStreamOut() const;
	void			ReleaseClipSet();
	void			RequestClipSet(fwMvClipSetId clipSetId);
	void			RequestClipSetIfExists(fwMvClipSetId clipSetId);
	void			SetStreamingPriorityOverride(eStreamingPriority nStreamingPriority);
	bool			ShouldClipSetBeStreamedIn() const;
	bool			ShouldClipSetBeStreamedOut() const;
	bool			ShouldUseClipSet() const;

private:

	RegdPrioritizedClipSetRequest m_pRequest;

};

// A lightweight wrapper class that streams in the get up set and handles addrefing and removal of references
// so they don't get streamed out at the wrong time
class CGetupSetRequestHelper
{
public:
	CGetupSetRequestHelper();
	~CGetupSetRequestHelper();

	bool Request(eNmBlendOutSet requestedSetId);
	void Release();

	bool IsLoaded() const { return m_bIsReferenced; }

	eNmBlendOutSet GetRequiredGetUpSetId(void) const;

private:

	eNmBlendOutSet		m_requiredGetupSetId;
	bool					m_bIsReferenced;
};

// A different kind of cliprequesthelper that is used to request loading of a lower priority clipId set
// for ambient purposes, will only load in the clips when the ambientmanager allows it to
// and 
class CAmbientClipRequestHelper
{
public:
	// State enum
	typedef enum
	{
		AS_noGroupSelected = 0,
		AS_groupSelected,
		AS_groupStreamed
	} ClipState;

	CAmbientClipRequestHelper();
	~CAmbientClipRequestHelper();

	// Interface to select a random clipId set.
	bool ChooseRandomConditionalClipSet(CPed * pPed, CConditionalAnims::eAnimType clipType, const char * pConditionalAnimsName);
	bool ChooseRandomClipSet(const CScenarioCondition::sScenarioConditionData& conditionalData, const CConditionalAnims * pConditionalAnims, CConditionalAnims::eAnimType clipType, u32 uChosenPropHash = 0);

	void SetClipAndProp( fwMvClipSetId clipSetId, u32 propHash );
	void ResetClipSelection();

	// Gets the current state
	ClipState	GetState() const { return m_clipState; }
	void		SetState(ClipState clipState) { m_clipState = clipState; }

	// Accessors for the chosen group information
	bool RequiresProp() { return m_requiredProp.IsNotNull(); }
	s32 GetClipDictIndex() const { return m_clipDictIndex; }
	fwMvClipSetId GetClipSet() const { return m_clipSetId; }
	const strStreamingObjectName& GetRequiredProp() const { return m_requiredProp; }
	bool IsClipSetSynchronized() const { return m_bIsClipSetSynchronized; }

	// Streaming interface
	bool RequestClips( bool bRequiresManagerPermissionToLoad = true );
	void ReleaseClips();

	// Choose a random clip that meets the conditions from the set
	static bool PickClipByGroup_FullConditions( s32 iClipDictIndex, u32& iClipHash );
	static bool PickClipByDict_FullConditions( s32 iClipDictIndex, u32& iClipHash );
	static bool PickClipByGroup_SimpleConditions( const CDynamicEntity* pDynamicEntity, const fwMvClipSetId &clipSetId, fwMvClipId& iClipID);
	static bool PickRandomClipInClipSet(const fwMvClipSetId &clipSetId, u32& iClipHash);
	static bool PickRandomClipInClipSetUsingBlackBoard(const CPed* pPed, const fwMvClipSetId &clipSetId, u32& iClipHash, CAnimBlackboard::TimeDelayType eTimeDelay = CAnimBlackboard::IDLE_ANIM_TIME_DELAY);

#if __BANK
	static bool PickNextClipInClipSet(const fwMvClipSetId &clipSetId, u32& iClipHash );
#endif

	CAmbientClipRequestHelper& operator=(const CAmbientClipRequestHelper& rhs);

	bool CheckClipsLoaded() { return m_clipRequestHelper.HaveClipsBeenLoaded(); }

private:
	static s32 FillClipHashArrayWithExistingClips(atFixedArray<u32, MAX_EXAMINABLE_CLIPS>& aPossibleClipHash, const fwClipSet* pClipSet);

	CClipRequestHelper		m_clipRequestHelper;
	s32						m_clipDictIndex;
	fwMvClipSetId			m_clipSetId;
	strStreamingObjectName	m_requiredProp;
	ClipState				m_clipState;
	bool					m_bHasPermissionToLoad:1;
	bool					m_bIsClipSetSynchronized:1;
};

// Manager responsible for managing the number of loaded clip sets, grants or denies loading permission
// to CAmbientClipRequestHelper which should be used for loading all ambient clipId sets.

// Struct to store clipId group requests (I.e. which clipId groups being requested accross the game)
typedef struct 
{
	s32		m_iClipDict;		//clipId dictionary requested.
	s32		m_iNumberRequests;	//number of times this clipId group has been requested.
	s32		m_iRequestPriority; //highest priority this clipId group has been requested by.
} CClipDictionaryRequest;

// Struct to store clipId group uses (I.e. which clipId groups are used and by who)
typedef struct 
{
	s32		m_iClipDict;	//clipId dictionary in use
	s32		m_iNumberUses;	//number of times this group is currently being used
	s32		m_iUsePriority; //highest priority this clipId group is being used by.
} CClipDictionaryUse;

class CAmbientClipStreamingManager
{
public:
	// This is the only class that should ever need access to the main interface functions
	// all ambient clipId requests should come through here.
	friend class CAmbientClipRequestHelper;

	CAmbientClipStreamingManager();
	~CAmbientClipStreamingManager();

	void Init();

	// Processes all the requests and grants those of highest priority
	// meaning calls to RequestClipGroup will succeed next update
	void Update();

	static CAmbientClipStreamingManager* GetInstance() {return &ms_instance;}

#if !__FINAL
	void DisplayAmbientStreamingUse();
	static bool DISPLAY_AMBIENT_STREAMING_USE;
#endif
	
private:

	// called from CAmbientClipRequestHelper to request loading of an ambient clipId dictionary
	bool RequestClipDictionary(s32 clipDictIndex, s32 iPriority);
	// called from CAmbientClipRequestHelper to register the use of an clipId dictionary (it is being requested for loading)
	void RegisterClipDictionaryUse(s32 clipDictIndex, s32 iPriority);
	// called from CAmbientClipRequestHelper to release the use of clipId dictionary (it is no longer being used)
	void ReleaseClipDictionaryUse(s32 clipDictIndex);

	// Internal helper functions
	CClipDictionaryRequest* GetHighestPriorityRequest();
	enum{ MAX_ANIMATION_DICTIONARY_REQUESTS = 32};
	enum{ MAX_ANIMATION_DICTIONARY_IN_USE = 10 };

	CClipDictionaryRequest	m_requestedAnimationDictionaries[MAX_ANIMATION_DICTIONARY_REQUESTS];
#if !__FINAL
	CClipDictionaryRequest	m_previousRequestedAnimationDictionaries[MAX_ANIMATION_DICTIONARY_REQUESTS];
#endif
	CClipDictionaryUse		m_usedAnimationDictionaries[MAX_ANIMATION_DICTIONARY_IN_USE];

	static CAmbientClipStreamingManager ms_instance;
};


//-------------------------------------------------------------------------
// A single transition clipId, with code 
//-------------------------------------------------------------------------
class CTransitionClip
{
public:
	CTransitionClip();
	enum {MAX_BONES_TO_CHECK = 5 };
	void Setup(const fwMvClipSetId &clipSetId, const fwMvClipId &clipId);

private:
	fwMvClipSetId	m_clipSetId;
	fwMvClipId		m_clipId;
	Matrix34		m_aBoneMatrices[MAX_BONES_TO_CHECK];
};


//-------------------------------------------------------------------------
// Examines the avaialble transitions from ragdoll or an unknown clipId 
// state and picks the one that is closest
//-------------------------------------------------------------------------
class CTransitionClipSelector
{
public:	
	enum { MAX_TRANSITIONAL_CLIPS = 8 };
	static void	AddPossibleClip( const fwMvClipSetId &clipSetId, const fwMvClipId &clipId );

	static bool			   ms_bInitialised;
private:
	static CTransitionClip ms_aTransitionClips[MAX_TRANSITIONAL_CLIPS];
	static s32		   ms_iNumClips;
};

//-------------------------------------------------------------------------
// Helper used to return if there's a valid navmesh route
//-------------------------------------------------------------------------
typedef enum
{
	SS_StillSearching,
	SS_SearchSuccessful,
	SS_SearchFailed
} SearchStatus;
class CNavmeshRouteSearchHelper
{
public:
	CNavmeshRouteSearchHelper();
	~CNavmeshRouteSearchHelper();

	// Starts a search betwen the points using the peds default flags, flags can be overridden using iOverridenFlags
	bool			StartSearch		( CPed* pPed, const Vector3& vStart, const Vector3& vEnd, float fCompletionRadius = 0.0f, u64 iOverridenFlags = 0 , u32 iNumInfluenceSpheres = 0, TInfluenceSphere * pInfluenceSpheres = NULL, float fReferenceDistance = 0.0f, const Vector3 * pvReferenceVector = NULL, const bool bIgnoreVehicles = false );
	// Returns the result of the search, false means search not yet complete.  pvPointsOnRoute *must* point to an array of MAX_NUM_PATH_POINTS or more vertices
	bool			GetSearchResults( SearchStatus& searchResult, float &fDistance, Vector3& vLastPoint, Vector3* pvPointsOnRoute = NULL, s32* piMaxNumPoints = NULL, TNavMeshWaypointFlag* pWaypointFlags = NULL);
	// Resets any search currently active
	void			ResetSearch();
	inline bool		IsSearchActive() { return m_bWaitingForSearchResult; }

	inline bool		GetRouteEndedInWater() { return m_bRouteEndedInWater; }
	inline bool		GetRouteEndedInInterior() { return m_bRouteEndedInInterior; }

	inline void		SetMaxNavigableAngle(const float f) { m_fMaxNavigableAngle = f; }

	inline void		SetEntityRadius(const float f) { m_fEntityRadius = f; }

	const Vector3& GetClosestPointFoundToTarget() const { return m_vClosestPointFoundToTarget; }
private:

	TPathHandle	m_hPathHandle;

	//********************************************************
	// Flags set only if the route was successfully retrieved
	Vector3 m_vClosestPointFoundToTarget;	
	u32 m_bLastRoutePointIsTarget						:1;	
	u32 m_bRouteEndedInWater							:1;
	u32 m_bRouteEndedInInterior							:1;
	
	float m_fMaxNavigableAngle;
	float m_fEntityRadius;
	bool m_bWaitingForSearchResult;
};


//-------------------------------------------------------------------------
// Helper used to return a vehicle path node search
//-------------------------------------------------------------------------
class CPathNodeRouteSearchHelper : public CTaskHelperFSM
{
public:
	FW_REGISTER_CLASS_POOL(CPathNodeRouteSearchHelper);

	enum 
	{ 
		HISTORY_TO_MAINTAIN_WHEN_EXTENDING_ROUTE			 = 3,
		HISTORY_TO_MAINTAIN_WHEN_EXTENDING_CAB_TRAILER_ROUTE = 5
	};

	enum SearchState
	{
		State_Start = 0,
		State_FindStartNodesInDirection,
		State_FindStartNodesAnyDirection,
		State_GenerateWanderRoute,
		State_Searching,
		State_ValidRoute,
		State_NoRoute
	};

	enum SearchFlags
	{
		Flag_IncludeWaterNodes			= BIT(0),
		Flag_JoinRoadInDirection		= BIT(1),
		Flag_DriveIntoOncomingTraffic	= BIT(2),
		Flag_WanderRoute				= BIT(3),
		Flag_UseShortcutLinks			= BIT(4),
		Flag_UseExistingNodes			= BIT(5),
		Flag_UseSwitchedOffNodes		= BIT(6),
		Flag_FindRouteAwayFromTarget	= BIT(7),
		Flag_AvoidTurns					= BIT(8),
		Flag_WanderOffroad				= BIT(9),
		Flag_ExtendPathWithResultsFromWander = BIT(10),
		Flag_AvoidHighways				= BIT(11),
		Flag_HighwaysOnly				= BIT(12),
		Flag_AllowWaitForNodesToLoad	= BIT(13),
		Flag_IsMissionVehicle			= BIT(14),
		Flag_StartNodesOverridden		= BIT(15),
		Flag_AvoidingAdverseConditions	= BIT(16),
		Flag_ObeyTrafficRules			= BIT(17),
		Flag_AvoidRestrictedAreas		= BIT(18),
		Flag_OnStraightLineUpdate		= BIT(19),
		Flag_CabAndTrailerExtendRoute	= BIT(20),
		Flag_FindWanderRouteTowardsTarget	= BIT(21),
	};

	CPathNodeRouteSearchHelper();
	~CPathNodeRouteSearchHelper();

	FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	void Reset();

	bool VerifyNodesAreLoadedForArea() const;
	bool StartSearch( const Vector3& vStartPosition, const Vector3& vTargetPosition, const Vector3& vStartOrientation, const Vector3& vStartVelocity, u32 iSearchFlags, const CVehicleNodeList* pPreferredTargetRoute);
	bool StartExtendCurrentRouteSearch( const Vector3& vStartPosition, const Vector3& vTargetPosition, const Vector3& vStartOrientation, const Vector3& vStartVelocity, s32 iCurrentProgress, u32 iSearchFlags, const CVehicleNodeList* pPreferredTargetRoute);
	CVehicleNodeList& GetNodeList() { return m_route; }
	bool IsDoingWanderRoute() const {return (m_iSearchFlags & Flag_WanderRoute) != 0;}
	bool ShouldAvoidTurns() const {return (m_iSearchFlags & Flag_AvoidTurns) != 0;}
	u32  GetSearchFlags() const { return m_iSearchFlags; }
	bool ShouldExtendPathWithWanderResults() const {return (m_iSearchFlags & Flag_ExtendPathWithResultsFromWander) != 0;}
	void ResetStartNodeRejectDist() {m_fStartNodeRejectDist = DEFAULT_REJECT_JOIN_TO_ROAD_DIST;}
	void IncreaseStartNodeRejectDist() {m_fStartNodeRejectDist = rage::Min(m_fStartNodeRejectDist * m_fStartNodeRejectDistMultiplier, LARGE_FLOAT);}
	void SetStartNodeRejectDistMultiplier(float f) { m_fStartNodeRejectDistMultiplier = f; }
	void GetTargetPosition(Vector3& vTarget) const {vTarget = m_vTargetPosition;}

	// If we go through a templated junction, rely on it to tell us how we're allowed to move
	//through there
	void FixUpPathForTemplatedJunction(const CVehicle* pVehicle, const u32 iForceDirectionAtJunction);

	// Update the current target node index
	void SetTargetNodeIndex(s32 iIndex) { Assert(iIndex >= 0 && iIndex < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED); m_iTargetNodeIndex = iIndex; m_route.SetTargetNodeIndex(iIndex); }

	u32 GetCurrentNumNodes() const {return m_iCurrentNumberOfNodes;}
	bool GetActuallyFoundRoute() const { return m_bActuallyFoundRoute; }

	void SetMaxSearchDistance(int maxSearchDist) { m_iMaxSearchDistance = maxSearchDist; }
	int GetMaxSearchDistance() const { return m_iMaxSearchDistance; }

	static u32 FindPathDirection(const CNodeAddress & oldNode, const CNodeAddress & currNode, const CNodeAddress & newNode, bool *pSharpTurn, float *pLeftness, const float fDotThreshold, float *pDotProduct);
	static u32 FindPathDirectionInternal(Vector2 vOldToCurr, Vector2 vCurrToNew, bool *pSharpTurn, float *pLeftness, const float fDotThreshold, float *pDotProduct);
	static u32 FindJunctionTurnDirection(const CNodeAddress& junctionPreEntranceNode, const CNodeAddress& junctionEntranceNode, const CNodeAddress & junctionNode, const CNodeAddress & junctionExitNode, bool *pSharpTurn, float *pLeftness, const float fDotThreshold, float *pDotProduct, const CPathFind & pathfind = ThePaths);
	static u32 FindUpcomingJunctionTurnDirection(const CNodeAddress& junctionNode, const CVehicleNodeList* pNodeList, bool *pSharpTurn, float *pLeftness, const float fDotThreshold, float *pDotProduct, int& iNumExitLanesOut);

	static s32 GetSlipLaneNodeLinkIndex(const CPathNode * pCurrNode, const CNodeAddress & FromNode, const CPathFind & pathfind = ThePaths);
	static s32 GetHighwayExitLinkIndex(const CPathNode * pCurrNode, const CNodeAddress & FromNode, const CPathFind & pathfind = ThePaths);

	static void GetZerothLanePositionsForNodesWithLink(const CPathNode* pNode, const CPathNode* pNextNode, const CPathNodeLink* pLink, Vector3& vCurrPosOut, Vector3& vNextPosOut);

	void QuickReplan(const s32 iLane);

	static const float sm_fDefaultDotThresholdForRoadStraightAhead;
	static const float sm_fDefaultDotThresholdForUsingJunctionNode;
private:
	// State functions
	FSM_Return Start_OnUpdate();
	FSM_Return FindStartNodesInDirection_OnUpdate();
	FSM_Return FindStartNodesAnyDirection_OnUpdate();
	FSM_Return GenerateWanderRoute_OnUpdate();
	FSM_Return Searching_OnUpdate();

	// Helper fns
	void GetNextNode(const s32 autopilotNodeIndex, s32 LanesOnCurrentLink, s16 &iLinkTemp, bool &bHappyWithNode, bool bComingOutOfOneWayStreet, const u32 iForceDirectionAtJunction, bool bLeftTurnsOnly, const bool bDisallowLeft, const bool bDisallowRight, const bool bTryToMatchDir, const Vector3& vTryToMatchDir);
	void GetNextNodeAvoidingPoint(const s32 autopilotNodeIndex, const s32 LanesOnCurrentLink, s16 &iLinkTemp, bool &bHappyWithNode, bool bComingOutOfOneWayStreet, const u32 iForceDirectionAtJunction, const bool bLeftTurnsOnly, const bool bDisallowLeft, const bool bDisallowRight, const bool bDisallowSwitchedOff);

	bool IsThisAnAppropriateNode( CNodeAddress VeryOldNode, CNodeAddress OldNode, CNodeAddress CandidateNode, bool bGoingAgainstTraffic, bool bGoingDownOneWayStreet, bool bComingOutOfOneWayStreet);
	
	float	m_fStartNodeRejectDist;
	Vector3 m_vSearchStart;
	Vector3 m_vTargetPosition;
	Vector3 m_vStartOrientation;
	Vector3 m_vStartVelocity;
	float	m_fStartNodeRejectDistMultiplier;
	u32		m_iSearchFlags;
	u32		m_iTargetNodeIndex;
	u32		m_iCurrentNumberOfNodes;

	// PURPOSE:	The max path length to explore during a search, passed in to DoPathSearch()
	//			as the MaxSearchDistance parameter.
	s32		m_iMaxSearchDistance;

	CVehicleNodeList m_PreferredTargetRoute;
	CVehicleNodeList m_route;

	// PURPOSE:	If we are in the NoRoute state, if this is set, we actually did find a path, except that it was empty
	//			because the start and destination nodes were the same.
	bool	m_bActuallyFoundRoute;
};

//-------------------------------------------------------------------------
// CVehicleFollowRoute
//		Defines a route for a vehicle to follow from a path or navmesh
//		or any other source
// CRoutePoint
//		A single point on that path
//-------------------------------------------------------------------------


#if __WIN32
#pragma warning(push)
#pragma warning(disable:4201)	// nonstandard extension used : nameless struct/union
#endif

class CRoutePoint
{
public:
	Vec4V m_vPositionAndSpeed;	// .w holds the speed value
	union
	{
		struct { 
			float	m_fCentralLaneWidth;
			float	m_fLaneWidth;
			//float	m_fSpeed;
			s8		m_iRequiredLaneIndex;	// Can be -1 if it doesn't matter
			s8		m_iCamberThisSide;		// Packed camber value
			s8		m_iCamberOtherSide;		// Calculated by finding the opposite-side link during route creation
			// Required follow information
			s8		m_iNoLanesThisSide;
			s8		m_iNoLanesOtherSide;
			u8		m_iCamberFalloffThisSide;
			u8		m_iCamberFalloffOtherSide;
			bool	m_bIsSingleTrack:1;
			bool	m_bIgnoreForNavigation:1;
			bool	m_bJunctionNode:1;		// Increases distance constraints at this point
			bool	m_bHasLeftOnlyLane:1;
			bool	m_bIsHighway:1;
			bool	m_bIsSlipLane:1;
			bool	m_bIsSlipJunction:1;
			//unless I counted wrong, we have room for 1 more bit here -JM
		};
		Vec::Vector_4V m_vParamsAsVector;
	};
	
	Vec4V m_vLaneCenterOffsetAndNodeAddrAndLane;	// .z holds node address, .w holds lane value

	void SetSpeed(float f) { m_vPositionAndSpeed.SetWf(f); }
	float GetSpeed() const { return m_vPositionAndSpeed.GetWf(); }

	Vec3V_Out GetPosition() const { return m_vPositionAndSpeed.GetXYZ(); }
	Vec3V_Out GetPositionWithRandomOffset(Vec3V_In vRandomOffset) const 
	{ 
		const Vec3V vPos = GetPosition();
		return vPos + vRandomOffset;
	}

	ScalarV_Out GetSpeedV() const { return m_vPositionAndSpeed.GetW(); }
	ScalarV_Out GetCentralLaneWidthV() const { return Vec4V(m_vParamsAsVector).GetX(); }
	ScalarV_Out GetLaneWidthV() const { return Vec4V(m_vParamsAsVector).GetY(); }

	Vec3V_Out GetLaneCenterOffset() const { return Vec3V(m_vLaneCenterOffsetAndNodeAddrAndLane.GetXY(), ScalarV(V_ZERO)); }
	Vec2V_Out GetLaneCenterOffsetXY() const { return m_vLaneCenterOffsetAndNodeAddrAndLane.GetXY(); }

	void SetLaneCenterOffsetAndNodeAddr(Vec2V_In vOffset, const CNodeAddress& addr)
	{
		CompileTimeAssert(sizeof(CNodeAddress) == 4);
		const ScalarV laneV = m_vLaneCenterOffsetAndNodeAddrAndLane.GetW();
		const ScalarV addrV = LoadScalar32IntoScalarV(*(const u32*)&addr);
		m_vLaneCenterOffsetAndNodeAddrAndLane = Vec4V(vOffset, addrV, laneV);
	}

	void SetLaneCenterOffsetAndNodeAddrAndLane(Vec2V_In vOffset, const CNodeAddress& addr, ScalarV_In laneV)
	{
		CompileTimeAssert(sizeof(CNodeAddress) == 4);
		const ScalarV addrV = LoadScalar32IntoScalarV(*(const u32*)&addr);
		m_vLaneCenterOffsetAndNodeAddrAndLane = Vec4V(vOffset, addrV, laneV);
	}

	void SetLaneCenterOffsetAndLane(Vec2V_In vOffset, ScalarV_In laneV)
	{
		const ScalarV addrV = m_vLaneCenterOffsetAndNodeAddrAndLane.GetZ();
		m_vLaneCenterOffsetAndNodeAddrAndLane = Vec4V(vOffset, addrV, laneV);
	}

	void ClearLaneCenterOffsetAndNodeAddrAndLane()
	{
		m_vLaneCenterOffsetAndNodeAddrAndLane = Vec4V(V_MASKZ);		// // (0 0 ~0 0) - zero offset, node address 0xFFFFFFFF, zero lane
		aiAssert(GetNodeAddress().IsEmpty());
	}

	const CNodeAddress& GetNodeAddress() const
	{
		return reinterpret_cast<const CNodeAddress*>(&m_vLaneCenterOffsetAndNodeAddrAndLane)[2];
	}

	void SetLane(ScalarV_In scLane) { m_vLaneCenterOffsetAndNodeAddrAndLane.SetW(scLane); }
	ScalarV_Out GetLane() const { return m_vLaneCenterOffsetAndNodeAddrAndLane.GetW(); }

	void FindRoadBoundaries(float & fLeftDistance, float & fRightDistance) const;

	int NumLanesToUseForPathFollowing() const
	{
		return m_iNoLanesThisSide > 0 ? m_iNoLanesThisSide : m_iNoLanesOtherSide;
	}
};

#if __WIN32
#pragma warning(pop)
#endif

class CVehicleLaneChangeHelper;
 
class CVehicleFollowRouteHelper
{
	// PURPOSE:	Some data that can be computed per route point/segment,
	//			independently of where you are at. A possible optimization
	//			could be to compute and store this on path construction time,
	//			though it's not clear that it will be a performance win due to
	//			the increase in memory access.
	struct RoutePrecalcInfo
	{
		Vec3V	m_vCurrentTarget1;
		Vec3V	m_vCurrentTarget2;
		Vec3V	m_vDir;
		Vec3V	m_vLaneCentreCurrentTarget1;
		Vec3V	m_vLaneCentreCurrentTarget2;
		Vec3V	m_vLaneCentreDir;
		int		m_iLaneCentreIndex;
		bool	m_bQualifiesAsJunction;
		bool	m_bJunctionIsStraightAhead;
		bool	m_bSharpTurn;
		bool	m_bSingleTrack;
	};

public:
	enum { MAX_ROUTE_SIZE = 14 };
	CVehicleFollowRouteHelper();
	CVehicleFollowRouteHelper(const CVehicleFollowRouteHelper& otherFollowRoute);
	~CVehicleFollowRouteHelper();

	CVehicleFollowRouteHelper& operator=(const CVehicleFollowRouteHelper& otherFollowRoute);

	void Invalidate()
	{
		m_iNumPoints = 0;
		m_iCurrentTargetPoint = 0;
	}

	bool GetClosestPosOnPath(const Vector3 & vPos, Vector3 & vClosestPos_out, int & iPathIndexClosest_out, float & iPathSegmentPortion_out) const;
	void ConstructFromNodeList(const CVehicle* pVehicle, CVehicleNodeList& nodeList, const bool bUpdateLanesForTurns = false, CVehicleLaneChangeHelper* pLaneChangeHelper = NULL, const bool bUseStringPulling = false);
	void ConstructFromWaypointRecording(const CVehicle* pVehicle, CWaypointRecordingRoute& route, int iCurrentProgress, int iTargetProgress, bool bUseAISlowdown, bool bUseOverrideSpeed, float fOverrideSpeed);
	void ConstructFromNavmeshRoute(const CVehicle* pVehicle, Vector3* pvPointsOnRoute, const s32 iNumPoints);
	void ConstructFromRoutePointArray(const CVehicle* pVehicle, const CRoutePoint* pRoutePoints, const s32 iNumPoints, const s32 iDesiredTargetPoint=1);
	void CalculateDesiredSpeeds(const CVehicle* pVehicle, const bool bUseStringPulling);
	static void CalculateDesiredSpeeds(const CVehicle* pVehicle, const bool bUseStringPulling, CRoutePoint* pRoutePoints, int iNumPoints);

	void SetStraightLineTargetPos(Vec3V_In tgtPosV) { m_StraightLineTargetPos = tgtPosV; m_GotStraightLineTargetPos = true; }
	void ClearStraightLineTargetPos() { m_GotStraightLineTargetPos = false; }

	void SetAllowUseForPhysicalConstraint(bool b) { m_bAllowUseForPhysicalConstraint = b; }
	bool ShouldAllowUseForPhysicalConstraint() const { return m_bAllowUseForPhysicalConstraint; }

	bool ProcessSuperDummy(CVehicle * pVehicle, ScalarV_In fCruiseSpeed, s32 DrivingFlags, const float& fTimeStepIn) const;
	bool CalculatePositionAndOrientationOnVirtualRoad
		(CVehicle * pVehicle, Mat34V_In matTest, const Vec3V * pBonnetPos, bool bConsiderSettingWheelCompression, bool bUpdateDummyConstraint, 
		Vec3V_InOut vToFrontOut, Vec3V_InOut vToRightOut, Vec3V_InOut vAverageRoadToWheelOffsetOut, Vec3V * pNewPosConstrainedToRoadWidth,
		bool bPreventWheelCompression = false) const;
	void UpdateTimeslicedInterpolationTarget(CVehicle& veh, Vec3V_In newVelocityV, ScalarV_In timeStepInV) const;
	bool ComputeOnRoadPosition(const CVehicle& veh, Vec3V_Ref tgtPositionOut, bool bPreventWheelCompression = false/*, QuatV_Ref tgtOrientationOut*/) const;

	// Calculates the position the vehicle should be driving towards and the speed.
	// Returns the distance to the end of the route
	float FindTargetCoorsAndSpeed(const CVehicle* pVehicle, float fDistanceAhead, float fDistanceToProgressRoute
		, Vec3V_In vStartCoords, const bool bDrivingInReverse, Vec3V_InOut vOutTargetCoords, float& fOutSpeed, const bool bUseSpeedAtCurrentPosition=false
		, const bool bUseCachedSpeedVel=true, const bool bUseStringPulling=false, const int iOverrideEndPointIndex=-1, const bool bAllowLaneSlack=true);
	s16 GetCurrentTargetPoint() const { return m_iCurrentTargetPoint; }

	float FindDistanceToEndOfPath(const CVehicle* pVehicle) const;
	float FindDistanceToGivenNode(const CVehicle* pVehicle, const s32 iGivenNode, const bool bDriveInReverse) const;

	//static void GetPointAtDistanceAlongPath(const float fDistance, Vector3& vPointOut);
	float EstimatePathLength() const;
	void UpdateTargetPositionForStraightLineUpdate(const CVehicle* pVehicle, s32 iDrivingFlags);
	
	s16	GetNumPoints() const { return m_iNumPoints; }
	const CRoutePoint* GetRoutePoints() const { return m_RoutePoints; }

	const Vector3& ComputeCurrentSegmentTangent(Vector3& o_Tangent) const;

	// PURPOSE:	Check if any part of the path passes within the given distance of some position. The
	//			path check starts with the segment from the current vehicle position to the current
	//			target point.
	// PARAMS:	testPos			- The position to check if it's near the path.
	//			currentVehPos	- The vehicle's current position, used for the first segment to be tested.
	//			dist			- The distance threshold.
	// RETURNS:	True if the path passes near testPos.
	bool DoesPathPassNearPoint(Vec3V_In testPos, Vec3V_In currentVehPos, const float& dist) const;

	void SetRequiredLaneIndex(int pointIndex, int laneIndex)
	{
		taskAssert(laneIndex >= -0x80 && laneIndex <= 0x7f);
		m_RoutePoints[pointIndex].m_iRequiredLaneIndex = (s8)laneIndex;

		ResetPrecalcData();
	}

	int GetRequiredLaneIndex(int pointIndex)
	{
		return (s8)m_RoutePoints[pointIndex].m_iRequiredLaneIndex;
	}

	void ResetPrecalcData()
	{
		m_TargetPointForPathWaypoints = -1;
		m_JunctionStringPullDistAdd = false;
		m_GotCurrentLane = false;
	}

	bool CurrentRouteContainsLaneChange() const {return m_bCurrentRouteContainsLaneChange;}
	void SetCurrentRouteContainsLaneChange(bool b) {m_bCurrentRouteContainsLaneChange = b;}

	bool IsUpcomingLaneChangeToTheLeft() const;

#if __BANK && DEBUG_DRAW
	void DebugDrawRoute(const CVehicle & pVehicle, Vec3V_In vTestPos) const;
#endif

	int FindCurrentAtPoint(Vec3V_In vVehiclePos, int iNumNodesToSearchBeforeCurrent = 1) const;
	bool GetIsDrivingInReverse() const {return m_bDrivingInReverse;}

	static Vec3V_Out GetVehicleBonnetPosition(const CVehicle* pVehicle, const bool bDriveInReverse, const bool bGetBottomOfCar=false);
	static Vec3V_Out GetVehicleBonnetPositionForNavmeshQueries(const CVehicle* pVehicle, const bool bDriveInReverse);
	static Vec3V_Out GetVehicleDriveDir(const CVehicle* pVehicle, const bool bDriveInReverse);

	s16 FindClosestPointToPosition(Vec3V_In vPos, const int iStart=0, int iEnd=0) const;

	bool GetBoundarySegmentsBetweenPoints(const int iFromPoint, const int iToPoint, const int iNextPoint
		, bool bClampToCenterOfRoad, Vector3& vLeftEnd, Vector3& vRightEnd
		, Vector3& vLeftStartPos, Vector3& vRightStartPos, const float fVehicleHalfWidthToUseLeftSide
		, const float fVehicleHalfWidthToUseRightSide, const bool bAddExtraLaneToSlipForAvoidance=false) const;

	void GiveLaneSlack();
	
private:

	void ModifyTargetForPathTolerance(Vec3V &io_targetPos, const CVehicle& in_Vehicle, float in_Tolerance ) const;

	float FindTargetCoorsAndSpeed_Old(const CVehicle* pVehicle, float fDistanceAhead, float fDistanceToProgressRoute
		, Vec3V_In vStartCoords, Vec3V_InOut vOutTargetCoords, float& fOutSpeed, const bool bUseSpeedAtCurrentPosition
		, const bool bUseCachedSpeedVel, const bool bUseStringPulling, const int iOverrideEndPointIndex);
	float FindTargetCoorsAndSpeed_OldIsolated(float handlingMinBrakeDistancef, float handlingMaxBrakeDistancef
		, float handlingMaxSpeedAtBrakeDistancef, float driverCurveSpeedModifierf, float driverNumSecondsToChangeLanesf
		, float fDistanceAhead, float fDistanceToProgressRoute, float &fMaxSpeedInOut, Vec3V_In vStartCoords
		, Vec3V_InOut vOutTargetCoords, Vec3V_In vVehiclePos, Vec3V_In vVehicleVel
		, bool bUseSpeedAtCurrentPosition, bool bUseStringPulling, bool bRender
		, const int iOverrideEndPointIndex
		, const float fMaxDistanceAllowedFromLaneCenterInLaneSpace
		, const float fInverseRacingModifier);
	float FindTargetCoorsAndSpeed_New(const CVehicle* pVehicle, float fDistanceAhead, float fDistanceToProgressRoute
		, Vec3V_In vStartCoords, Vec3V_InOut vOutTargetCoords, float& fOutSpeed, const bool bUseSpeedAtCurrentPosition
		, const bool bUseCachedSpeedVel, const bool bUseStringPulling, const int iOverrideEndPointIndex
		, const bool bAllowLaneSlack);
	float FindTargetCoorsAndSpeed_NewIsolated(const float& handlingMinBrakeDistancef, const float& handlingMaxBrakeDistancef
		, const float& handlingMaxSpeedAtBrakeDistancef, const float& driverCurveSpeedModifierf, float driverNumSecondsToChangeLanesf
		, const float& fDistanceAhead, const float& fDistanceToProgressRoute, float &fMaxSpeedInOut, Vec3V_In vStartCoords
		, Vec3V_InOut vOutTargetCoords, Vec3V_In vVehiclePos, Vec3V_In vVehicleVel
		, bool bUseSpeedAtCurrentPosition, bool bUseStringPulling, bool bRender
		, const int iOverrideEndPointIndex
		, const float fMaxDistanceAllowedFromLaneCenterInLaneSpace
		, const float fInverseRacingModifier);
	s32 FindPreviousValidRoutePointForNavigation(const s32 iCurrent) const;
	s32 FindNextValidRoutePointForNavigation(const s32 iCurrent) const;
	void PrecalcInfoForRoutePoint(const Vec3V_In vStartCoords, const Vec3V_In vStartDir, const int i, RoutePrecalcInfo& out) const;
	void PrecalcInfoForRoutePoint(int i, RoutePrecalcInfo& out) const;
	void RecomputePathThroughLanes(bool bRecompute, float driverNumSecondsToChangeLanesf
		, Vec3V_In vStartCoords, Vec3V_In vVehicleVel
		, const float fMaxDistanceAllowedFromLaneCenter);

	//returns false and doesn't touch the vector3 ref if the target position isn't valid
	//bool GetCurrentTargetPointPosition(Vector3& vTargetPos) const;

#if __BANK
	void RenderCarAiTaskInfo(int i,
			Vec3V_In vPrevTarget1, Vec3V_In vCurrentTarget1,
			ScalarV_In maxSpeedApproachingCorner,
			ScalarV_In scDistanceSearchedAhead) const;
#endif	// __BANK

#if __DEV
	bool m_bDebugMaintainLaneForNode[MAX_ROUTE_SIZE];
	void ResetDebugMaintainLaneForNodes()
	{
		sysMemSet(&m_bDebugMaintainLaneForNode, 0, sizeof(bool) * MAX_ROUTE_SIZE);
	}
#else //__DEV
	void ResetDebugMaintainLaneForNodes() {}
#endif //__DEV

	void SetCurrentTargetPoint(s32 iCurrentTargetPoint);
	void ResetCurrentTargetPoint() {m_iCurrentTargetPoint = 0;}

	Vec3V m_vPosWhenLastConstructedNodeList;
	Vec3V m_vRandomNodeOffset;

	u32	m_nTimeLastGivenLaneSlack;
	float m_fCurrentLane;
	s16 m_iNumPoints;
	s16 m_iCurrentTargetPoint;
	u8		m_bCurrentRouteContainsLaneChange : 1;
	CRoutePoint m_RoutePoints[MAX_ROUTE_SIZE];

	Vec4V						m_vPathWaypointsAndSpeed[MAX_ROUTE_SIZE + 1];	// MAX_ROUTE_SIZE+1 to fit the last element in
	float						m_DistBetweenPathWaypoints[MAX_ROUTE_SIZE];		// Parallel array to m_vPathWaypointsAndSpeed, but with one less element.
	bool						m_bPathWaypointsIgnored[MAX_ROUTE_SIZE + 1];
	bool						m_PathWaypointDataInitialized;

	s16							m_FirstValidElementOfPathWaypointsAndSpeed;
	s16							m_TargetPointForPathWaypoints;

	float						m_MinPossibleNodeSpeedOnPath;
	float						m_PathLengthSum;
	u8							m_PathLengthSumFirstElement;

	// PURPOSE:	Set when we are meant to be going straight towards a position,
	//			which is stored in m_StraightLineTargetPos.
	bool						m_GotStraightLineTargetPos;

	// FFTODO: See if we can remove these:
	bool						m_JunctionStringPullDistAdd;
	Vec3V						m_JunctionStringPullDistAddPoint1;
	Vec3V						m_JunctionStringPullDistAddPoint2;

	// PURPOSE:	The position we are going straight towards, if m_GotStraightLineTargetPos is set.
	Vec3V						m_StraightLineTargetPos;

	// PURPOSE:	The A coefficient in a 2D line equation A*x+B*y+C, representing
	//			a line in the direction of the current lane, at the time
	//			RecomputePathThroughLanes() was last called.
	float						m_CurrentLaneA;

	// PURPOSE:	The coefficient B in the line equation.
	float						m_CurrentLaneB;

	// PURPOSE:	The coefficient C in the line equation.
	float						m_CurrentLaneC;

	// PURPOSE:	True if m_CurrentLane[A..C] have been set and represent a valid
	//			line we should test against.
	bool						m_GotCurrentLane;

	bool						m_bAllowUseForPhysicalConstraint;

	bool						m_bDrivingInReverse;
};

class CVehicleLaneChangeHelper
{
public:
	CVehicleLaneChangeHelper();
	~CVehicleLaneChangeHelper();
	void UpdatePotentialLaneChange(const CVehicle* const pVeh, CVehicleFollowRouteHelper* pFollowRoute, const CPhysical* const pObstacle, const float fDesiredSpeed, const float fDistToInitialObstruction, const int iObstructedSegment, const bool bDriveInReverse);
	bool CanSwitchLanesToAvoidObstruction (const CVehicle* const pVeh, const CVehicleFollowRouteHelper* const pFollowRoute, const CPhysical* const pAvoidEnt, const float fDesiredSpeed, const bool bDriveInReverse, u32 timeStepInMs);

	//bool WantsToChangeLanesForTraffic() const {return m_bShouldChangeLanesForTraffic;}
	//void SetShouldChangeLanesForTraffic(bool b) {m_bShouldChangeLanesForTraffic = b;}
	bool WantsToOvertake() const {return m_TimeWaitingForOvertake >= ms_TimeBeforeOvertake;}
	bool WantsToUndertake() const {return m_TimeWaitingForUndertake >= ms_TimeBeforeUndertake;}

	void ResetFrameVariables();
	void DecayLaneChangeTimers(const CVehicle* const pVeh, u32 timeStepInMs);
	bool LaneChangesAllowedForThisVehicle(const CVehicle* const pVeh, const CVehicleFollowRouteHelper* const pFollowRoute) const;
	void ClearEntityToChangeLanesFor();
	CPhysical* GetEntityToChangeLanesFor() const {return m_pEntityToChangeLanesFor;}
	bool FindEntityToChangeLanesFor(const CVehicle* const pVehicle, const CVehicleFollowRouteHelper* const pFollowRoute, const float fDesiredSpeed, const bool bDriveInReverse, CPhysical*& pObstructingEntityOut, float& fDistToObstructionOut, int& iObstructedSegmentOut);
	void SetForceUpdate(bool b) {m_bForceUpdate = b;}
	bool GetForceUpdate() const {return m_bForceUpdate;}

private:
	bool IsAreaClearToChangeLanes(const CVehicle* const pVeh, const CVehicleFollowRouteHelper* const pFollowRoute, const bool bSearchLeft, const float fDesiredSpeed, /*float& fDistToObstructionOut,*/ const bool bDriveInReverse) const;
	bool IsSegmentObstructed(const int iRouteIndex, const CVehicleFollowRouteHelper* const pFollowRoute, const CVehicle* const pVeh, const int laneToCheck, const float fDesiredSpeed, float& fDistToObstructionOut, CPhysical*& pObstructingEntityOut, float& fObstructionSpeedOut, const bool bDriveInReverse) const;
	bool ShouldChangeLanesForThisEntity	(const CVehicle* const pVeh, const CPhysical* const pAvoidEnt, const float fOriginalDesiredSpeed, const bool bDriveInReverse) const;

	RegdPhy m_pEntityToChangeLanesFor;

	//u32		m_NextTimeAllowedToChangeLanesForTraffic;

	//unlike m_NextTimeAllowedToChangeLanesForTraffic,
	//these two values govern how long our vehicle has to actually
	//be blocked by a car before deciding to change lanes
	u32		m_TimeWaitingForOvertake;
	u32		m_TimeWaitingForUndertake;
	//u8		m_bShouldChangeLanesForTraffic : 1;
	u8		m_bWaitingToOvertakeThisFrame : 1;
	u8		m_bWaitingToUndertakeThisFrame : 1;
	u8		m_bForceUpdate : 1;

	static dev_float	ms_fChangeLanesVelocityRatio;
	static u32		ms_TimeBeforeOvertake;
	static u32		ms_TimeBeforeUndertake;
	static dev_float	ms_fMinDist2ToLink;
};

class CVehicleConvertibleRoofHelper
{
public:
	CVehicleConvertibleRoofHelper() {};
	~CVehicleConvertibleRoofHelper() {};

	bool IsRoofDoingAnimatedTransition(const CVehicle* pVehicle) const;
	void PotentiallyRaiseOrLowerRoof(CVehicle* pVehicle, const bool bStoppedAtRedLight) const;
	bool IsRoofFullyLowered(CVehicle* pVehicle) const;
	bool IsRoofFullyRaised(CVehicle* pVehicle) const;
	bool ShouldRoofBeDown(CVehicle* pVehicle) const;
private:
	void LowerRoof(CVehicle* pVehicle, bool bImmediate) const;
	void RaiseRoof(CVehicle* pVehicle, bool bImmediate) const;

	static u32 ms_uLastRoofChangeTime;
};

//-------------------------------------------------------------------------
// Helper used to modify vehicles speed for shocking events
//-------------------------------------------------------------------------
class CVehicleShockingEventHelper
{
public:
	CVehicleShockingEventHelper() {};
	~CVehicleShockingEventHelper() {};

	bool ProcessShockingEvents(CVehicle *pVehicle,Vector3 &eventPosition, float &desiredSpeed);
};


//-------------------------------------------------------------------------
// Helper used to modify vehicle speed to obey junction commands
//-------------------------------------------------------------------------
class CVehicleJunctionHelper
{
public:
	CVehicleJunctionHelper();
	~CVehicleJunctionHelper();

	enum eStoppedReason
	{
		Stopped_For_Invalid = -1,
		Stopped_For_Light,
		Stopped_For_Traffic,
		Stopped_For_Giveway,
		Num_Stopped_Reasons
	};

	bool ProcessTrafficLights (CVehicle *pVehicle, float &desiredSpeed, const bool bShouldStopForLights, const bool bWasStoppedForTraffic);

	bool IsWaitingForTrafficLight() const { return m_WaitingForTrafficLight; }

	u32 GetTimeToStopForCurrentReason(const CVehicle *pVehicle) const;

	static bool ApproachingGreenOrAmberLight (const CVehicle *pVehicle);
	static bool ApproachingRedLight (const CVehicle *pVehicle);
	static bool ApproachingLight(const CVehicle *pVehicle, const bool bConsiderGreen=true, const bool bConsiderAmber=true, const bool bConsiderRed=true);

private:
	float HandleAccelerateForStopLights	(float currentDesiredSpeed, CVehicle* pVehicle, const bool bShouldObeyTrafficLights) const;
	bool AreWeTurningRightAtJunction (CVehicle* pVeh, const bool bShouldStopForLights) const;//useful for working out whether we can turn right on a red light.
	float GetMaxSpeedToApproachRedLight	(const CVehicle *pVehicle, const float fDesiredSpeed, const float fDistToLight) const;
	float GetMaxSpeedToApproachGiveWayNode	(const CVehicle *pVehicle, const float fDesiredSpeed, const float fDistToLight, const bool bHasGoSignalFromJunction) const;
private:
	eStoppedReason	m_StoppedReason;
	bool			m_WaitingForTrafficLight;
};

class CVehicleWaypointRecordingHelper
{
public:
	CVehicleWaypointRecordingHelper();
	~CVehicleWaypointRecordingHelper();

	int GetProgress() const { return m_iProgress; }
	void SetProgress(int progress) {m_iProgress = progress;}
	int GetTargetProgress() const {return m_iTargetProgress;}
	u32	GetFlags() const {return m_iFlags;	}
	void SetLoopPlayback(const bool b) { m_bLoopPlayback = b; }
	bool GetIsLoopPlayback() const {return m_bLoopPlayback;}
	bool WantsToPause() const {return m_bWantsToPause;}
	bool WantsToResumePlayback() const {return m_bWantsToResumePlayback;}
	void SetPause();
	void SetResume();
	void SetOverrideSpeed(const float fMBR);
	void UseDefaultSpeed();
	bool IsSpeedOverridden() const {return m_bOverrideSpeed;}
	bool IsSpeedJustOverridden() const {return m_bJustOverriddenSpeed;}
	float GetOverrideSpeed() const {return m_fOverrideSpeed;}
	void ResetJustOverriddenSpeed() {m_bJustOverriddenSpeed = false;}
	CWaypointRecordingRoute* GetRoute() const {return m_pRoute;}
	void ClearRoute() { m_pRoute = NULL; }
	inline const int GetRouteSize() const {Assert(m_pRoute); return m_pRoute->GetSize();}

	bool Init(CWaypointRecordingRoute * pRoute, const int iInitialProgress=0, const u32 iFlags=0, const int iTargetProgress=-1);

private:
	void DeInit();

	::CWaypointRecordingRoute * m_pRoute;
	int m_iProgress;
	int m_iTargetProgress;
	u32 m_iFlags;
	float m_fOverrideSpeed;
	bool m_bLoopPlayback;
	bool m_bWantsToPause;
	bool m_bWantsToResumePlayback;
	bool m_bOverrideSpeed;
	bool m_bJustOverriddenSpeed;
};

//-------------------------------------------------------------------------
// Helper used to control vehicle dummies
//-------------------------------------------------------------------------
class CVehicleDummyHelper
{

public:

	enum Flags
	{
		ForceReal					= BIT0,
		ForceRealUnlessNoCollision	= BIT1,
	};
	
public:

	CVehicleDummyHelper();
	~CVehicleDummyHelper();
	
public:

	static void Process(CVehicle& rVehicle, fwFlags8 uFlags);

};

//-------------------------------------------------------------------------
// Helper used to test if there is a navmesh line-of-sight
//-------------------------------------------------------------------------
class CNavmeshLineOfSightHelper
{
public:
	CNavmeshLineOfSightHelper();
	~CNavmeshLineOfSightHelper();

	bool			StartLosTest(const Vector3 & vStart, const Vector3 & vEnd, const float fRadius=0.0f, const bool bTestDynamicObjects=false, bool bNoLosAcrossWaterBoundary=false, bool bStartsInWater=false, const s32 iNumExcludeEntities=0, CEntity ** ppExcludeEntities=NULL, const float fMaxSlopeAngle=0.0f);
	bool			StartLosTest(Vector3 * pVerts, const int iNumVerts, const float fRadius=0.0f, const bool bTestDynamicObjects=false, const bool bQuitAtFirstLOSFail=true, bool bNoLosAcrossWaterBoundary=false, bool bStartsInWater=false, const s32 iNumExcludeEntities=0, CEntity ** ppExcludeEntities=NULL, const float fMaxSlopeAngle=0.0f);


	// Returns the result of the LOS, false means search not yet complete.
	// bLosIsClear returns the overall status of the line-test
	// ppLosResults if non-NULL is filled with the status of each individual LOS test of a multi-LOS
	bool			GetTestResults( SearchStatus & searchResult, bool & bLosIsClear, bool& bNoSurfaceAtCoords, const int iNumLosResults, bool ** ppLosResults );

	// Resets any search currently active
	void			ResetTest();
	inline bool		IsTestActive() { return m_bWaitingForLosResult; }

private:

	bool m_bWaitingForLosResult;
	TPathHandle	m_hPathHandle;
};

//------------------------------------------------------------------------------------------------
// Helper used to search for clear locations in the navmesh:
//
// vSearchOrigin = center of cylinder volume
// fSearchRadiusXY = radius of cylinder
// fSearchDistZ = half height of cylinder (measured from vSearchOrigin.z)
// fClearAreaRadius = size of desired clear area to be found
// iFlags = optional flags
// fMinimumRadius = optional minimum XY distance from cylinder origin to center of clear area
//------------------------------------------------------------------------------------------------
class CNavmeshClearAreaHelper
{
public:
	CNavmeshClearAreaHelper();
	~CNavmeshClearAreaHelper();

	enum
	{
		Flag_AllowInteriors		= BIT0,
		Flag_AllowExterior		= BIT1,
		Flag_AllowWater			= BIT2,
		Flag_AllowSheltered		= BIT3,
		Flag_TestForObjects		= BIT4
	};

	bool			StartClearAreaSearch( const Vector3 & vSearchOrigin, const float fSearchRadiusXY, const float fSearchDistZ, const float fClearAreaRadius, const u32 iFlags, const float fMinimumRadius=0.0f );
	bool			GetSearchResults( SearchStatus & searchResult, Vector3 & resultPosition );

	void			ResetSearch();
	inline bool		IsSearchActive() { return m_bSearchActive; }

private:

	bool m_bSearchActive;
	TPathHandle m_hSearchHandle;
};


//--------------------------------------------------------------------------------------------------------------------
// Helper used to search for positions on the navmesh, close to an input position:
//
// vSearchOrigin = center of spherical volume
// fSearchRadius = radius of search sphere
// iFlags = optional flags
// fZWeighting = multiplier applied to prefer points closer to vSearchOrigin.z
// fMinimumSpacing = optional required minimum spacing between results (helps to avoid bunched up points)
// iNumAvoidSpheres = number of optional spheres used to block positions
// pAvoidSpheres = array of spheres (or NULL if iNumAvoidSpheres is zero)
// iMaxNumResults = the helper will return multiple close positions (up to CClosestPositionRequest::MAX_NUM_RESULTS)
//--------------------------------------------------------------------------------------------------------------------
class CNavmeshClosestPositionHelper
{
public:
	CNavmeshClosestPositionHelper();
	~CNavmeshClosestPositionHelper();

	enum
	{
		Flag_ConsiderDynamicObjects		= 0x01,
		Flag_ConsiderInterior			= 0x02,
		Flag_ConsiderExterior			= 0x04,
		Flag_ConsiderOnlyLand			= 0x08,
		Flag_ConsiderOnlyPavement		= 0x10,
		Flag_ConsiderOnlyNonIsolated	= 0x20,
		Flag_ConsiderOnlySheltered		= 0x40,
		Flag_ConsiderOnlyNetworkSpawn	= 0x80,
		Flag_ConsiderOnlyWater			= 0x100
	};

	bool			StartClosestPositionSearch( const Vector3 & vSearchOrigin, const float fSearchRadius, const u32 iFlags, const float fZWeightingAbove=1.0f, const float fZWeightingAtOrBelow=1.0f, const float fMinimumSpacing=0.0f, const s32 iNumAvoidSpheres=0, const spdSphere * pAvoidSpheres=NULL, const s32 iMaxNumResults=CClosestPositionRequest::MAX_NUM_RESULTS );
	bool			GetSearchResults( SearchStatus & searchResult, s32 & iOut_NumPositions, Vector3 * pOut_Positions, const s32 iMaxNumPositions );

	void			ResetSearch();
	inline bool		IsSearchActive() const { return m_bSearchActive; }

private:

	bool m_bSearchActive;
	TPathHandle m_hSearchHandle;
};

class CGetupProbeHelper : public WorldProbe::CShapeTestCapsuleDesc
{
public:
	CGetupProbeHelper(float fCustomZProbeEndOffset = FLT_MAX);
	virtual ~CGetupProbeHelper();

	bool StartClosestPositionSearch(const float fSearchRadius, const u32 iFlags, const float fZWeightingAbove = 1.0f, const float fZWeightingAtOrBelow = 1.0f, const float fMinimumSpacing = 0.0f);
	void SubmitProbes();

	bool GetClosestPosition(Vector3& vClosestPosition, bool bForceLOSCheck = false, bool bDoDoorProbe = false) const;

	const WorldProbe::CShapeTestHitPoint& GetResult(int iResult) const;
	const Vector3& GetEndPosition(int iResult) const;
	Vector3 GetProbeEndPosition(int iResult) const;
	inline int GetNumResult() const { return m_iNumProbe; }
	inline u8 GetStateIncludeFlags() const { return m_nStateIncludeFlags; }

	float GetCustomZProbeEndOffset() { return m_fCustomZProbeEndOffset; }
	void SetCustomZProbeEndOffset(float fCustomZProbeEndOffset) { m_fCustomZProbeEndOffset = fCustomZProbeEndOffset; }

	bool GetAllResultsReady();

	void ResetSearch();

private:

	bool CheckDoorProbe(s32 i) const;
	bool CheckGroundNormal(s32 i) const;
	WorldProbe::CShapeTestSingleResult m_CapsuleResults[CClosestPositionRequest::MAX_NUM_RESULTS];
	Vector3 m_vEndPosition[CClosestPositionRequest::MAX_NUM_RESULTS];
	CNavmeshClosestPositionHelper m_ClosestPositionHelper;

	int m_iNumProbe;
	float m_fCustomZProbeEndOffset;
};

/////
//
// helper to avoid shorelines
//
/////
class CBoatAvoidanceHelper
{
public:

	static float ms_DefaultMaxAvoidanceOrientation;

	enum AvoidanceFlag
	{
		AF_BlockedCutToTarget = BIT0,
	};

	CBoatAvoidanceHelper(CVehicle& in_Vehicle);
	~CBoatAvoidanceHelper();

	float GetAvoidanceOrientation() const { return m_fBoatAvoidanceOrientation; }

	void SetAvoidanceOrientation(float in_AvoidanceOrientation);
	void SetDesiredOrientation(float in_DesiredOrientation);
	void SetMaxAvoidanceOrientationLeft(float in_MaxOrientationAngle);
	void SetMaxAvoidanceOrientationRight(float in_MaxOrientationAngle);
	void SetMaxAvoidanceOrientationLeftThisFrame(float in_MaxOrientationAngle);
	void SetMaxAvoidanceOrientationRightThisFrame(float in_MaxOrientationAngle);

	void SetDefaultMaxAvoidanceOrientation(float in_MaxOrientationAngle);

	void SetAvoidanceFlag(AvoidanceFlag in_Flag);
	bool IsAvoidanceFlagSet(AvoidanceFlag in_Flag) const;

	void SetEnabled(bool in_Value) { m_bEnabled = in_Value; }
	void SetSteeringEnabled(bool in_Value) { m_bEnableSteering = in_Value; }
	void SetSlowDownEnabled(bool in_Value) { m_bEnableSlowingDown = in_Value; }
	void SetAvoidanceFeelerScale(float in_Value) { m_AvoidanceFeelerScale = in_Value; }
	void SetAvoidanceFeelerMaxLength( float in_Value ) { m_AvoidanceFeelerMaxLength = in_Value; }

	float ComputeAvoidanceOrientation(float in_Orientation) const;
	float ComputeAvoidanceSpeed(float in_Speed, float in_SlowDownScale) const;
	float ComputeSliceAngle(const CVehicle& in_Vehicle) const;

	bool IsBlocked() const;
	bool IsBeached() const;
	bool IsAvoiding() const;
	void SetReverse(bool in_Value);
	void Reset();
	
	void Process(CVehicle& in_Vehicle, float in_fElapsedTime);
	void ProcessBeached(CVehicle& in_Vehicle, float in_fElapsedTime);

#if !__FINAL
	void Debug(const CVehicle& in_Vehicle) const;
#endif //!__FINAL

private:

	atFixedArray<CNavmeshLineOfSightHelper, 5> m_NavmeshLOSHelpers;
	atFixedArray<bool, 5> m_NavmeshLOSClear;

	float m_fBoatAvoidanceOrientation;
	float m_fBoatPreviousAvoidanceDelta;
	float m_fBoatDesiredOrientation;
	float m_fBoatMaxAvoidanceOrientationLeft;
	float m_fBoatMaxAvoidanceOrientationRight;

	float m_fBoatMaxAvoidanceOrientationLeftThisFrame;
	float m_fBoatMaxAvoidanceOrientationRightThisFrame;

	float m_fTimeOutOfWater;
	float m_AvoidanceFeelerScale;
	float m_AvoidanceFeelerMaxLength;
	int m_ProbeQueryIndex;

	fwFlags8 m_AvoidanceFlags;

	bool m_bEnabled;
	bool m_bEnableSteering;
	bool m_bEnableSlowingDown;
	bool m_bReverse;
};


//
// follow route helper
//
class CBoatFollowRouteHelper
{
public:

	class CBoatRoutePoint
	{
	public:

		CBoatRoutePoint();
		CBoatRoutePoint(const CBoatRoutePoint& in_rhs);
		CBoatRoutePoint(Vec2V_In in_Position, ScalarV_In in_Width);
		CBoatRoutePoint& operator=(const CBoatRoutePoint& in_rhs);

		Vec2V m_Position;
		ScalarV m_Width;
	};


	CBoatFollowRouteHelper();
	~CBoatFollowRouteHelper();

	void OverrideTarget(const Vector3& in_Target);
	void OverrideDestination(const Vector3& in_Target);
	const Vector3& ComputeTarget(sVehicleMissionParams& params, const CVehicle& in_Vehicle) const;
	const Vector3& ComputeTarget(Vector3& o_Vector, const CVehicle& in_Vehicle) const;
	const Vector3& ComputeSegmentTarget(Vector3& o_Vector, const CVehicle& in_Vehicle) const;
	Vec2V_Out ComputeSegmentDirection(Vec2V_Ref o_Direction) const;
	const float ComputeSegmentWidth() const;

	void ConstructFromStartEnd(CVehicle& in_Vehicle, const Vector3& in_Target);
	void ConstructFromNodeList(CVehicle& in_Vehicle, const CVehicleNodeList& in_NodeList, const Vector3& in_Target);
	bool IsLastSegment() const;	
	bool IsFinished() const;	
	float GetSegmentTValue() const { return m_SegmentTValue.Getf(); }

	bool Progress(CVehicle& in_Vehicle, float in_fDistance);


#if !__FINAL
	void Debug(const CVehicle& DEBUG_DRAW_ONLY(in_Vehicle)) const;
#endif //!__FINAL

private:

	atArray<CBoatRoutePoint> m_RoutePoints;
	ScalarV m_SegmentTValue;

};


class CClipPlayer;
class CTask;
class CDynamicEntity;

class CClipNetwork;

typedef	taskRegdRef<CTask>									RegdTask;
typedef	taskRegdRef<const CTask>							RegdConstTask;

// A helper class to simplify the task interface with the move network.
// Should Encapsulates all message passing functionality as well as retrieve any return messages.
class CMoveNetworkHelper
{
public:
	enum MoveFlags
	{
		Flag_IsActive = BIT0,			// Is the network active (currently unused)
		Flag_IsInsertedAsChild = BIT1,	// Has this networkhelper been inserted into another network helper as a child.
		Flag_ForceStateReady = BIT2		// Used to force the event found flag on the network player when forcing state.
	};
	CMoveNetworkHelper();
	~CMoveNetworkHelper();

#if __BANK
	//dynamically load a network def from the file system (used for MoVE previewing)
	mvNetworkDef* LoadNetworkDefFromDisk(const char * filename);
#endif //__BANK

	// PURPOSE: Requests the specified network
	bool RequestNetworkDef(const fwMvNetworkDefId &networkDefId);

	// PURPOSE: Queries the streaming system to determine if a particular network definition is streamed in and ready to use
	bool IsNetworkDefStreamedIn(const fwMvNetworkDefId &networkDefId);

	// PURPOSE: Instructs the streaming system to load the network def immediately, and blocks until it is available to use
	void ForceLoadNetworkDef(const fwMvNetworkDefId &networkDefId);

	// PURPOSE: Loads the specified network def and initializes the helpers' network player with the definition.
	//			The network player can then be added to the ped's main network (via CMove), or to another network player.
	void CreateNetworkPlayer(const CDynamicEntity* pEntity, const fwMvNetworkDefId &networkDefId);

#if __BANK
	// PURPOSE: Initializes the helpers' network player with the definition.
	//			The network player can then be added to the ped's main network (via fwMove), or to another network player.
	void CreateNetworkPlayer(const CDynamicEntity* pEntity, const mvNetworkDef* networkDef);
#endif // __BANK

	// PURPOSE: Use this to release the current network player when the task is done with it. If no other tasks /
	//			helpers are referencing the player, it will be destroyed.
	void ReleaseNetworkPlayer();

	// PURPOSE: Return a pointer to the attached network player to be inserted into fwMove, or another network
	inline fwMoveNetworkPlayer* GetNetworkPlayer()
	{
		return m_pPlayer;
	}

	inline const fwMoveNetworkPlayer* GetNetworkPlayer() const
	{
		return m_pPlayer;
	}

	// PURPOSE: Returns the move network helper flags
	const fwFlags16& GetMoveFlags() const { return m_flags; }

	// Requests/Signals/Clips
	void SendRequest( const fwMvRequestId &requestId);

	void SetFloat( const fwMvFloatId &floatId, const float fValue);
	// Get the value of a real signal parameter
	float GetFloat( const fwMvFloatId &floatId) const;
	// Get the value of a float signal. Returns true if the signal was found, false otherwise.
	bool GetFloat( const fwMvFloatId &floatId, float& val) const;

	void SetBoolean( const fwMvBooleanId &booleanId, const bool bValue);
	// Get the value of a boolean control parameter
	bool GetBoolean( const fwMvBooleanId &booleanId) const;
	// Get the value of a bool signal. Returns true if the signal was found, false otherwise.
	bool GetBoolean( const fwMvBooleanId &booleanId, bool& val) const;

	// Sends an clip clip for the network to use
	void SetClip( const crClip* pAnimation, const fwMvClipId &clipId);

	//gets the value of a clip type signal
	const crClip* GetClip( const fwMvClipId &clipId) const;

	// Sends an animation to the network
	void SetAnimation( const crAnimation* pAnimation, const fwMvAnimId &animId);
	// PURPOSE: Return a pointer to the animation currently assigned to the given parameter id
	const crAnimation* GetAnimation( const fwMvAnimId &animId) const;

	// PURPOSE:
	void SetParameterizedMotion( const crpmParameterizedMotion* pParam, const fwMvParameterizedMotionId &pmId);
	// PURPOSE: Return a pointer to the PM currently assigned to the given parameter id
	const crpmParameterizedMotion* GetParameterizedMotion( const fwMvParameterizedMotionId &pmId) const;

	// PURPOSE:
	void SetExpressions( crExpressions* pExpression, const fwMvExpressionsId &exprId);
	// PURPOSE: Return a pointer to the Expressions currently assigned to the given parameter id
	const crExpressions* GetExpressions( const fwMvExpressionsId &exprId) const;

	// PURPOSE:
	void SetFilter( crFrameFilter* pFilter, const fwMvFilterId &filterId);
	// PURPOSE: Return a pointer to the filter currently assigned to the given parameter id
	const crFrameFilter* GetFilter( const fwMvFilterId &filterId) const;

	// PURPOSE:
	void SetFrame( crFrame* pFrame, const fwMvFrameId &frameId);
	const crFrame* GetFrame( const fwMvFrameId &frameId) const;

	// PURPOSE: Event tag retrieval methods
	// Will search for a crProperty with the correct name in the parameter buffer
	// and return a readable event type if it exists.
	template<class T>
	const T* GetEvent(CClipEventTags::Key& hash) const
	{
		fwMvPropertyId propertyId;
		propertyId.SetHash(hash.GetHash());
		return static_cast<const T*>(GetProperty(propertyId));
	}

	// PURPOSE: returns the value of an output param of type property with the given name
	const crProperty* GetProperty( const fwMvPropertyId &propId) const;

	// PURPOSE: Sets the value of a flag on the network. Flags are similar to switches (i.e. boolean control parameters), 
	// except that they have persistant values. Set a flags value one frame and it will remain the same the next frame.
	void SetFlag( bool bflagValue, const fwMvFlagId &signalId);

	// PURPOSE: Applies an clipId set to the MoVE network.
	//			Note: This will not change any clips already playing!
	//			Only newly created clip nodes (for example created during transitions
	//			to new states) will use the new set.
	void SetClipSet( const fwMvClipSetId &clipSetId, const fwMvClipSetVarId &clipSetVarId = CLIP_SET_VAR_ID_DEFAULT);
	const fwClipSet* GetClipSet(const fwMvClipSetVarId &clipSetVarId = CLIP_SET_VAR_ID_DEFAULT) const;
	const fwMvClipSetId GetClipSetId(const fwMvClipSetVarId &clipSetVarId = CLIP_SET_VAR_ID_DEFAULT) const;

	// PURPOSE:	Returns true if the network player has been created - NOTE: this does not necessarily mean the 
	//			network player is attached to the motion tree - see IsNetworkAttached below
	inline bool IsNetworkActive() const { return m_pPlayer != NULL; }

	// PURPOSE:	Returns true if the network was attached to the motion tree at the end of the last clipId update
	inline bool IsNetworkAttached() const { 
		if (m_pPlayer)
		{
			return m_pPlayer->IsInserted();
		}
		return false;
	}

	// PURPOSE:	Returns true if the network was attached to the motion tree at the end of the last clipId update
	inline bool IsNetworkBlendingOut() const { 
		if (m_pPlayer)
		{
			return m_pPlayer->IsBlendingOut();
		}
		return false;
	}

	// PURPOSE:	Returns true the network player was attached to the motion tree, but has since been removed
	inline bool HasNetworkExpired() const { 
		if (m_pPlayer)
		{
			return m_pPlayer->HasExpired();
		}
		return false;
	}

	// PURPOSE:	Returns true if the network was not yet inserted into the motion tree at the end of the last clipId update
	inline bool IsNetworkReadyForInsert() const { 
		if (m_pPlayer)
		{
			return m_pPlayer->IsReadyForInsert();
		}
		return false;
	}

	static const fwMvStateId ms_DefaultStateMachineId;

	void ForceStateChange( const fwMvStateId &targetStateId, const fwMvStateId &stateMachineId = ms_DefaultStateMachineId);

	// PURPOSE: Passes a network player into this network as a parameter. The slotName should be the name of the network node
	//			in this move network that will hold the subnetwork.
	void SetSubNetwork(fwMoveNetworkPlayer* player, const fwMvNetworkId &networkId);

	// PURPOSE: Use to set up a deferred insert into a parent network helper. Use IsParentReadyForInsert to poll whether
	//			the parent network is in the correct state or not.
	void SetupDeferredInsert(CMoveNetworkHelper* pParent, const fwMvBooleanId &parentReadyEventId, const fwMvNetworkId &insertNetworkId, float blendDuration = 0.0f);
	 
	// PURPOSE: Performs a deferred insert into the parent network.
	// RETURNS: True if the insert was successful, false if the parent was not ready.
	void DeferredInsert();

	bool HasDeferredInsertParent() const { return m_deferredInsertParent!=NULL; }

	CMoveNetworkHelper* GetDeferredInsertParent() { return m_deferredInsertParent; }

	bool IsParentReadyForInsert() { animAssert(m_deferredInsertParent!=NULL); return m_deferredInsertParent->IsInTargetState(); }

	// PURPOSE: Sets the cross blend duration to use when inserting a new subnetwork this frame
	void SetSubNetworkCrossBlend( /*fwMove::Id id,*/ float duration);

	// PURPOSE: Helper methods to test whether a required target state has been achieved by the move network
	//			To use, call this once (at the point of triggering your transition), providing the id of the event
	//			that will signal entry of the target state. Then poll using IsInTargetState to
	//			see if the state has been achieved in Move.
	inline void WaitForTargetState(const fwMvBooleanId &enterStateEventId) { 
		fwMoveNetworkPlayer* player = GetNetworkPlayer();
		if (player)
		{
			player->WatchForEvent(enterStateEventId);
		}
	}

	// PURPOSE: Checks to see if the target state entered signal has been fired by the network.
	inline bool IsWaitingForTargetState() {
		fwMoveNetworkPlayer* player = GetNetworkPlayer();
		if (player && player->IsWatchingForEvent())
		{
			return true;
		}
		return false;
	}

	// PURPOSE: Checks to see if the target state entered signal has been fired by the network.
	inline bool IsInTargetState() const {
		const fwMoveNetworkPlayer* player = GetNetworkPlayer();
		if (player)
		{
			if (player->EventFound())
			{
				return true;
			}
			else if (!player->IsWatchingForEvent())
			{
				return true;
			}
		}
		return false;
	}

	// PURPOSE: Call this to force the next state ready chek to pass instantly. Can be used to avoid
	//			The signal round trip required to get the on enter event when starting a network
	//			in a known state.
	inline void ForceNextStateReady() { m_flags.SetFlag(Flag_ForceStateReady); }


	// PURPOSE: Takes ownership of another move network helpers network player.
	//			Can be used to transfer clip state seamlessly between tasks.
	//			Useful in copy methods, etc.
	inline void TransferNetworkPlayer(CMoveNetworkHelper otherHelper)
	{
		if (otherHelper.GetNetworkPlayer())
		{
			//release our network player if it exists
			ReleaseNetworkPlayer();

			m_pPlayer = otherHelper.GetNetworkPlayer();
			m_pPlayer->AddRef();
			otherHelper.ReleaseNetworkPlayer();
		}
	}

	// PURPOSE: Check for footstep property tag
	inline bool GetFootstep(const fwMvBooleanId &booleanId) const
	{
#if NEW_MOVE_TAGS
		u32 hashId = booleanId.GetHash();
		const crProperty* prop = GetProperty(fwMvPropertyId(hashId));
		if(prop)
		{
			crPropertyAttributeBoolAccessor accessor(prop->GetAttribute(crProperty::CalcKey("Right", 0xB8CCC339)));
			bool right;
			if(accessor.Get(right))
			{
				static const u32 footHeelLeftId = ATSTRINGHASH("AEF_FOOT_HEEL_L", 0x1C2B5199);
				static const u32 footHeelRightId = ATSTRINGHASH("AEF_FOOT_HEEL_R", 0xD97C4C50);

				return right?(hashId==footHeelRightId):(hashId==footHeelLeftId);
			}
		}

		return false;
#else // NEW_MOVE_TAGS
		return GetBoolean(booleanId);
#endif // NEW_MOVE_TAGS
	}

protected:
	// Helper
	static u32 GenerateMoveId(const char* str) {return crc32(0, (Bytef*)str, (int)strlen(str));}

	CMoveNetworkHelper*	m_deferredInsertParent;		// The parent network that this subnetwork will be inserted into when deferring inserts
	fwMvNetworkId	m_deferredInsertNetworkId;	// The signal that will be used to insert this network into the parent network
	float	m_deferredInsertDuration;	// The duration specified by the parent that can 

	//The move network player for this task helper
	fwMoveNetworkPlayer* m_pPlayer;

	fwFlags16		m_flags;

#if __DEV
	u16	m_numNetworkPlayersCreatedThisFrame;
	u32 m_lastNetworkPlayerCreatedFrame;
#endif // __DEV
};

//////////////////////////////////////////////////////////////////////////

class CClipHelper;

// Move network helper granting the ability to cross blend clips on a number of priorities
// CClipHelpers can be inserted into this.


class CSimpleBlendHelper : public CMoveNetworkHelper
{
public:

	typedef  Functor3<CSimpleBlendHelper&, float, u32> BlendEventCB;

	enum ePriority
	{
		Task_Low = 0,
		Task_Medium,
		Task_High,
		Task_Num
	};

	enum eHelperStatus
	{
		Status_HelperChanged = 0,	// A new  has been added, but it has not yet been sent to the move network.
		Status_HelperSent,			// The helpers clip and blend request has been sent to the network, but the output parameters are still old.
		Status_HelperSignalsValid,	// The output parameters in the buffer match the inserted clip helper.
		Status_HelperDeferredBlendOut // The helper is due to be blended out next frame
	};

	enum eBlendHelperFlags
	{
		Flag_TagSyncWithMotionTask = BIT0
	};


	CSimpleBlendHelper();
	~CSimpleBlendHelper();

	// simple interface to start off a clip
	bool	BlendInClip( CDynamicEntity* pEntity, CClipHelper* pHelper, eAnimPriority animPriority, float fBlendDelta, u8 terminationType );
	void	BlendOutClip( CClipHelper* pHelper, float fBlendDelta = NORMAL_BLEND_OUT_DELTA, u32 uFlags = 0 );
	void	BlendOutClip( eAnimPriority priority, float fBlendDelta = NORMAL_BLEND_OUT_DELTA, u32 uFlags = 0 );

	// Sets the filter on this clip helper directly
	inline void SetFilter( CClipHelper* pHelper, crFrameFilter* pFilter ) {
		ePriority priority = FindPriority(pHelper);
		if (priority<Task_Num)
		{
			CMoveNetworkHelper::SetFilter( pFilter, GetFilterId(priority) );
		}
	}

	// Tells the network whether or not to loop the value
	inline void SetLooped( CClipHelper* pHelper, bool loop) { 
		ePriority priority = FindPriority(pHelper);
		if (priority<Task_Num)
		{
			SetBoolean( GetLoopedId(priority), loop );  
		}
	}

	inline bool GetLooped (const CClipHelper* pHelper ) 
	{ 
		ePriority priority = FindPriority(pHelper);
		if ( IsNetworkActive() && priority<Task_Num && m_helperStatus[priority]==Status_HelperSignalsValid )
			return GetBoolean(GetLoopedOutId(priority));
		else
			return false;
	}

	inline void SetPhase( CClipHelper* pHelper, float phase) { 	
		ePriority priority = FindPriority(pHelper);
		if (priority<Task_Num)
		{
			SetFloat( GetPhaseId(priority), phase); 
		}
	}

	inline float GetPhase (const CClipHelper* pHelper ) 
	{ 
		ePriority priority = FindPriority(pHelper);
		if (IsNetworkActive() && priority<Task_Num && m_helperStatus[priority]==Status_HelperSignalsValid)
		{
			float phase = GetFloat(GetPhaseOutId(priority));
			return Max(phase, 0.0f);
		}
		else
		{
			return 0.0f;
		}
	}

	inline float GetLastPhase (const CClipHelper* pHelper ) 
	{ 
		ePriority priority = FindPriority(pHelper);
		return priority<Task_Num ? m_lastPhase[priority] : 0.f;
	}

	inline void SetRate( CClipHelper* pHelper, float rate) { 
		ePriority priority = FindPriority(pHelper);
		if (priority<Task_Num)
		{
			m_rates[priority] = rate;
			SetFloat( GetRateId(priority), rate);
		}
	}

	inline float GetRate (const CClipHelper* pHelper ) 
	{ 
		ePriority priority = FindPriority(pHelper);
		if (IsNetworkActive() && priority<Task_Num && m_helperStatus[priority]==Status_HelperSignalsValid)
		{
			float rate = GetFloat(GetRateOutId(priority));
			return rate >= 0.0f ? rate : m_rates[priority];
		}
		else
		{
			return 1.0f;
		}
	}

	// PURPOSE: Get the total weight of the clips currently blended in on the helper
	float GetTotalWeight();

	// PURPOSE: Find out if a particular clipId flag is set on one of the clips currently being played back
	bool IsFlagSet(u32 iFlag) const;

	// PURPOSE: Converts the legacy clipId priority flag to a task priority
	static CSimpleBlendHelper::ePriority GetTaskPriority(eAnimPriority priority);

	// PURPOSE: Do some per update processing on the currently running clip helpers
	//			This takes care of backward compatibility with events and clipId player flags.
	//			This functionality should be removed once the old style events are replaced,
	//			and the task a.p.i is changed to remove clipId player flags.
	void Update( float deltaTime );

	// PURPOSE: Do some per update processing on the currently running clip helpers
	//			Currently used to update the status flags that well us whether the 
	//			output parameters match the inserted network helper.
	void UpdatePostMotionTree();

	// PURPOSE: An update that will be called immediately before the MoVE output parameter buffer is flipped
	void UpdatePreOutputBufferFlip();

	// PURPOSE: Sets the callback function used to insert the blend simple helper's move network into
	//			the motion tree.
	inline void SetInsertCallback(fwMove::InsertNetworkCB callback)
	{
		m_insertCB = callback;
	}

	// PURPOSE: Sets a callback that will be called when an clip helper blends in
	//			on the move network helper
	inline void SetClipBlendInCallback(BlendEventCB callback)
	{
		m_blendInCB = callback;
	}

	// PURPOSE: Sets a callback that will be called when an clip helper blends out
	//			on the move network helper
	inline void SetClipBlendOutCallback(BlendEventCB callback)
	{
		m_blendOutCB = callback;
	}

	u32 GetNumActiveClips();

	CClipHelper* FindClip(const char * pClipDictName, const char * pClipHash);
	CClipHelper* FindClipByIndexAndHash(s32 iClipDictIndex, u32 iClipHash);
	CClipHelper* FindClipBySetAndClip(const fwMvClipSetId &clipSetId, fwMvClipId clipId);
	CClipHelper* FindClipByClipId(fwMvClipId clipId);
	CClipHelper* FindClipBySetId(const fwMvClipSetId &clipSetId);
	CClipHelper* FindClipByPriority(eAnimPriority priority);
	CClipHelper* FindClipByIndex(s32 iDictionaryIndex);

	// PURPOSE: TEMPORARTO SUPPORT SPINE FIXUP
	//	will search the currently playing clip helpers and return the one with the highest blend and the upper body flag enabled
	const CClipHelper* GetHighestUpperBodyHelper();

private:

	// PURPOSE: Clears all slot data and signals any attached helpers to terminate immediately
	void ResetSlots();
	void ResetSlot(int i);

	inline fwMvClipId GetClipId( ePriority priority ) { static const fwMvClipId ms_Id[Task_Num] = { fwMvClipId("clip_low",0xDF552448), fwMvClipId("clip_medium",0x6A27382A), fwMvClipId("clip_high",0x6AAEFB78) }; return ms_Id[priority]; }
	inline fwMvFloatId GetPhaseId( ePriority priority ) { static const fwMvFloatId ms_Id[Task_Num] = { fwMvFloatId("phase_low",0xCB932131), fwMvFloatId("phase_medium",0x8848CE0A), fwMvFloatId("phase_high",0x672A1A52) }; return ms_Id[priority]; }
	inline fwMvFloatId GetRateId( ePriority priority ) { static const fwMvFloatId ms_Id[Task_Num] = { fwMvFloatId("rate_low",0x8FA91E), fwMvFloatId("rate_medium",0x6C8B94D3), fwMvFloatId("rate_high",0xBB106978) }; return ms_Id[priority]; }
	inline fwMvId GetDeltaId( ePriority priority ) { static const fwMvId ms_Id[Task_Num] = { fwMvId("delta_low",0x6B50D4EE), fwMvId("delta_medium",0x88E2B298), fwMvId("delta_high",0xDF6A2CD1) }; return ms_Id[priority]; }
	inline fwMvBooleanId GetLoopedId( ePriority priority ) { static const fwMvBooleanId ms_Id[Task_Num] = { fwMvBooleanId("looped_low",0xD9B7FBEF), fwMvBooleanId("looped_medium",0x7E7BAED6), fwMvBooleanId("looped_high",0x8F987B5D) }; return ms_Id[priority]; }
	inline fwMvFilterId GetFilterId( ePriority priority ) { static const fwMvFilterId ms_Id[Task_Num] = { fwMvFilterId("filter_low",0x93E670C1), fwMvFilterId("filter_medium",0x2E13D6B7), fwMvFilterId("filter_high",0x1FD31DC1) }; return ms_Id[priority]; }
	inline fwMvRequestId GetTransitionId( ePriority priority ) { static const fwMvRequestId ms_Id[Task_Num] = { fwMvRequestId("transition_low",0xA3E41F99), fwMvRequestId("transition_medium",0x9079C645), fwMvRequestId("transition_high",0xC1762DEC) }; return ms_Id[priority]; }
	inline fwMvFloatId GetDurationId( ePriority priority ) { static const fwMvFloatId ms_Id[Task_Num] = { fwMvFloatId("duration_low",0xB552F6F0), fwMvFloatId("duration_medium",0x809CCEDB), fwMvFloatId("duration_high",0x7F070FD0) }; return ms_Id[priority]; }
	inline fwMvFlagId GetUpperBodyIkId( ePriority priority ) { static const fwMvFlagId ms_Id[Task_Num] = { fwMvFlagId("enable_upper_ik_low",0x7374B984), fwMvFlagId("enable_upper_ik_medium",0x736ECE57), fwMvFlagId("enable_upper_ik_high",0x1E57D263) }; return ms_Id[priority]; }
	inline fwMvFloatId GetWeightId( ePriority priority ) { static const fwMvFloatId ms_Id[Task_Num] = { fwMvFloatId("weight_low",0x6599BB2D), fwMvFloatId("weight_medium",0xD46EAC44), fwMvFloatId("weight_high",0x2D3AF5FF) }; return ms_Id[priority]; }
	inline fwMvFloatId GetPhaseOutId( ePriority priority ) { static const fwMvFloatId ms_Id[Task_Num] = { fwMvFloatId("phase_out_low",0x900AD3B2), fwMvFloatId("phase_out_medium",0x418EF067), fwMvFloatId("phase_out_high",0xF5B12AFD) }; return ms_Id[priority]; }
	inline fwMvFloatId GetRateOutId( ePriority priority ) { static const fwMvFloatId ms_Id[Task_Num] = { fwMvFloatId("rate_out_low",0xC9DCEB75), fwMvFloatId("rate_out_medium",0x6CA9B3DC), fwMvFloatId("rate_out_high",0xBBE3B6CC) }; return ms_Id[priority]; }
	inline fwMvBooleanId GetLoopedOutId( ePriority priority ) { static const fwMvBooleanId ms_Id[Task_Num] = { fwMvBooleanId("looped_out_low",0x1FDB9734), fwMvBooleanId("looped_out_medium",0x22777431), fwMvBooleanId("looped_out_high",0xBD9EFCB3) }; return ms_Id[priority]; }

	inline ePriority FindPriority(const CClipHelper* pHelper) const
	{ 
		for (u32 i=Task_Low; i<Task_Num; i++)
		{
			if (m_slots[i]==pHelper)
				return (ePriority)i;
		}
		return Task_Num;
	}

	// PURPOSE: Creates the network player and attaches it to the ped at the requested priority.
	void StartNetworkPlayer(CDynamicEntity* pEntity, float transitionDuration, u32 flags = 0);

	// PURPOSE: Sends the signals to blend out the clipId on the move network.
	//			May be delayed for a frame if requested by the helper.
	void SendBlendOutSignals(ePriority priority, float blendDuration, u32 uFlags);

	// PURPOSE: Stores the supporting data for the clip slots
	CClipHelper*	m_slots[Task_Num];
	float			m_lastPhase[Task_Num];
	u8				m_terminationTypes[Task_Num];

	float			m_weights[Task_Num];
	float			m_weightDeltas[Task_Num];

	float			m_rates[Task_Num];	//stores the currently specified rate values for the currently inserted helper
	u8				m_helperStatus[Task_Num]; //stores the clip helper status
	float			m_blendOutDeltaNextFrame[Task_Num]; // used to defer blending out the clipId on this slot for a frame
														// should be initialised to 1.0f. if the value is <= 0.0f, the 
														// blend out signals will be sent next frame
	u32				m_events;	// stores the combined events for all the clips currently playing in the blend helper
	
	// PURPOSE: the callback to insert the network player into the parent move network
	fwMove::InsertNetworkCB m_insertCB;
	BlendEventCB	m_blendInCB;	// A callback that will be called when an clipId blend in on the helper
	BlendEventCB	m_blendOutCB;	// A callback that will be called when an clipId blends out on the helper
};

//////////////////////////////////////////////////////////////////////////

// A helper class to simplify the task interface with the clip systems
// Represents a single clip that can be played on a simple blend clip helper
class CClipHelper
  {
public:

	friend class CSimpleBlendHelper;
		
	enum eStatus
	{
		Status_Unassigned = 0,
		Status_BlendingIn,
		Status_FullyBlended,
		Status_BlendingOut,
		Status_Terminated,
	};


	enum eTerminationType
	{
		TerminationType_OnDelete = 0,
		TerminationType_OnFinish,
	};

	// PURPOSE: Computes the heading delta when approaching from the left/right
	// PARAMS: fFrom, fTo, angles in radians between -PI and PI
	// RETURNS: A non positive/negative heading delta
	static float ComputeHeadingDeltaFromLeft(float fFrom, float fTo);
	static float ComputeHeadingDeltaFromRight(float fFrom, float fTo);

	// PURPOSE: Approach an angle between -PI and PI from a particular direction
	static bool ApproachFromRight(float& fCurrentVal, float fDesiredVal, float fRate, float fTimeStep);
	static bool ApproachFromLeft(float& fCurrentVal, float fDesiredVal, float fRate, float fTimeStep);

	CClipHelper(CTask* pTask);
	CClipHelper();
	~CClipHelper();

	// Clip interface
	// Start clip
	bool				StartClipBySetAndClip(CDynamicEntity* pEntity, const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, float fBlendDelta, eTerminationType terminationType, u32 appendFlags = 0);
	bool				StartClipBySetAndClip(CDynamicEntity* pEntity, const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, int flags, atHashString filterHash, int priority, float fBlendDelta, eTerminationType terminationType);
	bool				StartClipByClip(CDynamicEntity* pEntity, const crClip* pClip, int flags, atHashString filterHash = BONEMASK_ALL, int priority = AP_MEDIUM, float fBlendDelta = NORMAL_BLEND_IN_DELTA, eTerminationType terminationType = TerminationType_OnFinish);
	bool				StartClipByDictAndHash(CDynamicEntity* pEntity, s32 iDictionaryIndex, u32 clipHash, int flags, atHashString filterHash, int priority, float fBlendDelta, eTerminationType terminationType);
	void				StopClip( float fBlendDelta = NORMAL_BLEND_OUT_DELTA, u32 uFlags = 0);

	// Get clip
	CClipHelper*		GetClipHelper() { return (m_iStatus==Status_BlendingIn || m_iStatus==Status_FullyBlended || m_iStatus==Status_BlendingOut) && m_pBlendHelper && (m_pBlendHelper->IsNetworkActive() && !m_pBlendHelper->HasNetworkExpired()) ? this : NULL; }
	const CClipHelper*	GetClipHelper() const { return (m_iStatus==Status_BlendingIn || m_iStatus==Status_FullyBlended || m_iStatus==Status_BlendingOut) && m_pBlendHelper && (m_pBlendHelper->IsNetworkActive() && !m_pBlendHelper->HasNetworkExpired()) ? this : NULL; }

	// Set clip from one explicitly running
	//bool				SetClip(CClipPlayer* pClipPlayer, eTerminationType terminationType);
	// PURPOSE: Get the pointer to the associated task to use for debugging
	CTask*		GetAssociatedTask()		{return m_pAssociatedTask;}
	void		SetAssociatedTask(CTask* pTask)		{ m_pAssociatedTask = pTask; }

	// allows us to associate the clip helper directly with an entity (safety check, and useful for debugging)
	void		SetAssociatedEntity(CDynamicEntity* pEntity)		{ m_pAssociatedEntity = pEntity; }
	CDynamicEntity*	GetAssociatedEntity() const;

	// Get the status of the clip helper
	// All access to the status must be through here
	s32 GetStatus() const
	{ 
		switch(m_iStatus)
		{
		case Status_BlendingIn:
			if (fwTimer::GetTimeInMilliseconds()>m_transitionEnd)
			{
				m_iStatus = Status_FullyBlended;
			}
			break;
		case Status_BlendingOut:
			if (fwTimer::GetTimeInMilliseconds()>m_transitionEnd)
			{
				m_iStatus = Status_Terminated;
			}
			break;
		default:
			break;
		}
		return m_iStatus;
	}

	// Reset the status to unassigned, its only valid to call this from an cliphelper that has previously terminated
	void ResetStatus() { Assert(m_iStatus == Status_Terminated); m_iStatus = Status_Unassigned; }

	// Call this function every frame with a *ped-relative* target heading to scale the extracted angular velocity.
	// If the clipId turns the wrong way wrt the shortest arc to the target heading, then clipId's angular velocity will be applied unmodified.
	void ProcessAchieveTargetHeading(CPed * pPed, const float fTargetHeading);
	// Phase
	inline float GetPhase() const { 
		return GetBlendHelper().GetPhase(this); 
	}

	// Phase
	inline float GetLastPhase() const { 
		return GetBlendHelper().GetLastPhase(this); 
	}

	inline void SetPhase(float phase) { 
		GetBlendHelper().SetPhase(this, phase); 
	}

	// Time

	inline float GetTime() const { 
		if (m_pClip)
		{
			return m_pClip->ConvertPhaseToTime(GetPhase());
		}
		else
		{
			return 0.0f;
		}
	}

	inline void SetTime(float time) { 
		if (m_pClip)
		{
			SetPhase(m_pClip->ConvertTimeToPhase(time));
		}
	}

	//////////////////////////////////////////////////////////////////////////

	inline float GetDuration() const { 
		if (m_pClip)
		{
			return m_pClip->GetDuration();
		}
		else
		{
			return 0.0f;
		}
	}

	// Rate
	inline float GetRate() const { 
		return GetBlendHelper().GetRate(this); 
	}

	inline void SetRate(float rate) { 
		GetBlendHelper().SetRate(this, rate); 
	}

	//// Get the blend amount
	float GetBlend() const;

	// PURPOSE: Restarts the clip helper if it is blending out or terminated
	void Restart(float fBlendDelta);

	// Get the amount of phase used to increment the clipId.
	float GetPhaseUpdateAmount() const;

	// Get the amount of time used to increment the clipId
	// calculated from its phase update amount and duration.
	float GetTimeUpdateAmount() const;

	// Sets the filter from a bone mask
	void SetFilter(atHashString filterHash);

	inline atHashString GetFilterHash() const { return m_filterHash; }

	// PURPOSE: Returns the clipId priority of the clipId (should match clipId group metadata)
	s32 GetPriority() { return m_iPriority; }

	// PURPOSE: Accessor method for the clip currently being played back
	const crClip* GetClip() const { return m_pClip; }

	bool IsFullyBlended() const { return GetStatus()==Status_FullyBlended; }
	bool IsBlendingIn() const { return GetStatus()==Status_BlendingIn; }
	bool IsBlendingOut() const { return GetStatus()==Status_BlendingOut; }

	// PURPOSE:	Check if a clip is currently being played, and hasn't been stopped or ended.
	// NOTES:	This was added because calls to GetClip() weren't reliable enough, we would still
	//			have a clip pointer even after StopClip() was called.
	bool IsPlayingClip() const
	{
		return (GetClip() != NULL)
				&& !IsBlendingOut()
				&& (GetStatus() != CClipHelper::Status_Terminated)
				&& (GetStatus() != CClipHelper::Status_Unassigned);
	}

	// PURPOSE: 
	inline bool IsHeldAtEnd() const { return GetPhase()==1.0f && IsFullyBlended(); }

	// Does the position of the mover at the end of the clipId differ from the beginning.
	bool ContributesToLinearMovement() const { return GetClip() ? fwAnimHelpers::ContributesToLinearMovement(*GetClip()) : false; }

	// Does the rotation of the mover at the end of the clipId differ from the beginning.
	bool ContributesToAngularMovement() const { return GetClip() ? fwAnimHelpers::ContributesToAngularMovement(*GetClip()) : false; }

	CSimpleBlendHelper& GetBlendHelper() const;
protected:

	// PURPOSE: Used by the simple blend helper to signal to the helper whether it is still being used
	inline void SetStatus(eStatus status)
	{
		taskAssert(static_cast<s32>(status) <= 0xFFFF);
		m_iStatus = status;
	}

	// PURPOSE: The clipId priority (loaded from metadata or specified in the tag )
	inline void SetPriority(s32 priority)
	{
		taskAssert(priority <= 0xFFFF);
		m_iPriority = (eAnimPriority)priority; 
	}

private:

	// PURPOSE: Sets the clip to playback with this clip helper
	void		SetPlaybackClip(const crClip* pClip);

	crpmParameterizedMotion* GetParameterizedMotion(s32 UNUSED_PARAM(dictId), u32 UNUSED_PARAM(nameHash)) { animAssertf(0, "Implement PM asset loading for MoVE"); return NULL; }
	crpmParameterizedMotion* GetParameterizedMotion(strStreamingObjectName UNUSED_PARAM(dictName), const char * UNUSED_PARAM(pmName)) { animAssertf(0, "Implement PM asset loading for MoVE"); return NULL; }

	pgRef<const crClip>	m_pClip;		// The clip to play back. This is refed by the clip helper.
	atHashString		m_filterHash;		// The bone mask this clipId is being played with

	CTask * 				m_pAssociatedTask;		// allows us to associate with a task
	pgRef<CDynamicEntity>	m_pAssociatedEntity;	// allows us to associate directly with an entity. TODO - Remove this when all objects have a task tree.

	mutable eStatus	m_iStatus : 16;		// The current playback status of this clip helper. Tells us if it is plugged into the simple blender MoVE network.
	eAnimPriority	m_iPriority : 16;								// The clipId priority (from metadata or the calling task) for the clipId to be played at

	u32		m_transitionEnd;	// used to work out if the clipId is blending in, blending out or fully blended
	u32		m_transitionStart;	// as above

	CSimpleBlendHelper* m_pBlendHelper; // The simple blend helper the clip helper is attached to

	//////////////////////////////////////////////////////////////////////////
	// **** Brought these over from CClipPlayer for backward compatibility****
	// **** Should probably get rid of them! ****
	//////////////////////////////////////////////////////////////////////////
public:

	//
	// Event management
	//

	// Query the events from the last update for a specific event
	bool IsEvent(const CClipEventTags::Key propertyKey) const;
	template < class Attr_T, class Value_T>
	bool IsEvent(const CClipEventTags::Key propertyKey, const CClipEventTags::Key attributeKey, Value_T value) const
	{
		// Is the specified event in the last update phase
		// TODO - make this search the parameter buffer once we have support for exposing tags through MoVE
		return CClipEventTags::FindFirstEventTag<crTag, Attr_T>(GetClip(), propertyKey, attributeKey, value, GetLastPhase(), GetPhase())!=NULL;
	}
	template < class Attr_T, class Value_T>
	bool ContainsEvent(const CClipEventTags::Key propertyKey, const CClipEventTags::Key attributeKey, Value_T value) const
	{
		return CClipEventTags::FindFirstEventTag<crTag, Attr_T>(GetClip(), propertyKey, attributeKey, value, 0.f, 1.f)!=NULL;
	}

	//
	// Flag setting (replace these with specific calls to the helper)
	//

	// Get/Set all control flags (See eClipPlayerFlags in ClipDefines.h)
	int GetAllFlags() const {return m_flags;}
	void SetAllFlags(const int iFlags) {m_flags = iFlags;}

	// Get/Set/Query a specific flag (See eClipPlayerFlags in ClipDefines.h)
	void SetFlag(const int iFlag) {m_flags |= iFlag;}
	void UnsetFlag(const int iFlag) {if(IsFlagSet(iFlag)) m_flags &= ~iFlag;}
	bool IsFlagSet(const int iFlag) const {return ((m_flags & iFlag) ? true : false);}

	//
	// Clip identifier info
	//

	// Return the slot index
	u32 GetClipDictIndex() const { return m_clipDictIndex.Get(); }

	// Return the clipId hash key
	u32 GetClipHashKey() const { return m_clipHashKey; }

	const char * GetClipName() const 
	{
		const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(m_clipDictIndex.Get(), m_clipHashKey);

		if (pClip && pClip->GetName())
		{
			return pClip->GetName();
		}
		return "";
	}
	
	strStreamingObjectName GetClipDictionaryMetadata() const
	{
		return fwAnimManager::GetName(m_clipDictIndex);
	}

	fwMvClipId GetClipId() const {return m_clipId;}
	fwMvClipSetId GetClipSet() const {return m_clipSetId;}

private:

	// TEMP - clipId player flags!
	s32 m_flags;		//

	// fwMvClipId and GroupId (can this become dev only?)
	fwMvClipId m_clipId;
	fwMvClipSetId m_clipSetId;

	// Clip slot index and hashkey 
	// (identifies the clipId data in the clipId manager)
	strLocalIndex m_clipDictIndex;
	u32 m_clipHashKey;
};

//////////////////////////////////////////////////////////////////////////

class CGenericClipHelper : public CMoveNetworkHelper
{
public:

	CGenericClipHelper();
	~CGenericClipHelper();

	// simple interface to start off a clip. looped can be specified explicitly. To specify hold at end, pass an autoblendoutdelta of 0.0f
	bool	BlendInClipBySetAndClip( CDynamicEntity* pEntity, const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, float fBlendInDelta = NORMAL_BLEND_IN_DELTA, float fAutoBlendOutDelta = NORMAL_BLEND_OUT_DELTA, bool looped = false, bool holdAtEnd = false, bool autoInsert = true );
	bool	BlendInClipByDictAndHash( CDynamicEntity* pEntity, s32 iDictionaryIndex, u32 uClipHash, float fBlendInDelta = NORMAL_BLEND_IN_DELTA, float fAutoBlendOutDelta = NORMAL_BLEND_OUT_DELTA, bool looped = false, bool holdAtEnd = false, bool autoInsert = true );
	bool	BlendInClipByClip( CDynamicEntity* pEntity, const crClip* pClip, float fBlendInDelta = NORMAL_BLEND_IN_DELTA, float fAutoBlendOutDelta = NORMAL_BLEND_OUT_DELTA, bool looped = false, bool holdAtEnd = false, bool autoInsert = true );
	void	BlendOutClip( float fBlendDelta = NORMAL_BLEND_OUT_DELTA );

	// Sets the filter on this clip helper directly
	void SetFilter( atHashString filter );

	inline bool IsAnimating()
	{
		return IsNetworkActive() && !HasNetworkExpired() && !IsNetworkBlendingOut() && GetClip()!=NULL;
	}

	// Tells the network whether or not to loop the value
	inline void SetLooped( bool loop ) { 
		SetBoolean( GetLoopedId(), loop );  
	}

	inline bool GetLooped ()  const
	{ 
		return GetBoolean(GetLoopedOutId());
	}

	inline void SetPhase( float phase ) { 	
		SetFloat( GetPhaseId(), phase); 
	}

	inline float GetPhase () const
	{ 
		float phase = GetFloat(GetPhaseOutId());
		return Max(phase, 0.0f);
	}

	inline float GetLastPhase () const
	{ 
		return m_lastPhase;
	}

	inline void SetLastPhase (float lastPhase)
	{ 
		m_lastPhase = lastPhase;
	}


	inline void SetRate( float rate ) { 
		SetFloat( GetRateId(), rate);
	}

	inline float GetRate () const
	{ 
		float rate = GetFloat(GetRateOutId());
		return rate >= 0.0f ? rate : 1.0f;
	}

	inline void SetAutoBlendOutDelta(float delta)
	{
		m_AutoBlendOutDelta = delta;
	}

	// Query the events from the last update for a specific event
	bool IsEvent(const CClipEventTags::Key propertyKey) const;
	template < class Attr_T, class Value_T>
	bool IsEvent(const CClipEventTags::Key propertyKey, const CClipEventTags::Key attributeKey, Value_T value) const
	{
		// Is the specified event in the last update phase
		// TODO - make this search the parameter buffer once we have support for exposing tags through MoVE
		return CClipEventTags::FindFirstEventTag<crTag, Attr_T>(GetClip(), propertyKey, attributeKey, value, GetLastPhase(), GetPhase())!=NULL;
	}

	// PURPOSE: Do some per update processing. Used to trigger the auto blend out on the clip
	//			if applicable.
	void Update();

	inline const crClip* GetClip() const {return m_pClip; }

	inline bool IsClipPlaying( const fwMvClipSetId &clipSetId, const fwMvClipId &clipId) const
	{
		const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(clipSetId, clipId);

		if (pClip && m_pClip == pClip)
		{
			return true;
		}
		return false;
	}

	inline bool IsClipPlaying( strStreamingObjectName pClipDictName, const char * pClipHash) const
	{
		s32 dictIndex = fwAnimManager::FindSlot(pClipDictName).Get();
		u32 clipHash = atStringHash(pClipHash);

		return IsClipPlaying(dictIndex, clipHash);
	}

	inline bool IsClipPlaying( s32 dictIndex, u32 clipHash) const
	{
		const crClip* desiredClip = fwAnimManager::GetClipIfExistsByDictIndex(dictIndex, clipHash);

		if (desiredClip && desiredClip==m_pClip)
		{
			return true;
		}

		return false;
	}

	inline bool IsClipPlaying( const crClip* pClip) const
	{
		if (pClip && pClip==m_pClip)
		{
			return true;
		}

		return false;
	}


	inline bool IsHeldAtEnd() const 
	{
		return (GetPhase()>=1.0f) && (m_bHoldAtEnd==1);
	}

	inline float GetDuration() const
	{
		return m_pClip ? m_pClip->GetDuration() : 0.0f;
	}

	inline void SetInsertCallback(fwMove::InsertNetworkCB callback)
	{
		m_insertCB = callback;
	}

private:

	inline void SetClip(const crClip* pClip)
	{
		if (m_pClip)
		{
			m_pClip->Release();
		}

		m_pClip = pClip;

		if (m_pClip)
		{
			m_pClip->AddRef();
		}
	}

	inline fwMvClipId GetClipId() const { static const fwMvClipId ms_Id("clip",0xE71BD32A); return ms_Id; }
	inline fwMvFloatId GetPhaseId() const { static const fwMvFloatId ms_Id("phase",0xA27F482B); return ms_Id; }
	inline fwMvFloatId GetRateId() const { static const fwMvFloatId ms_Id("rate",0x7E68C088); return ms_Id; }
	inline fwMvBooleanId GetLoopedId() const { static const fwMvBooleanId ms_Id("looped",0x204169A7); return ms_Id; }
	inline fwMvFilterId GetFilterId() const { static const fwMvFilterId ms_Id("filter",0x49DC73B3); return ms_Id; }
	inline fwMvRequestId GetTransitionId() const { static const fwMvRequestId ms_Id("transition",0xB9933991); return ms_Id; }
	inline fwMvFloatId GetDurationId() const { static const fwMvFloatId ms_Id("duration",0xDB0B92D5); return ms_Id; }

	inline fwMvFloatId GetPhaseOutId() const { static const fwMvFloatId ms_Id("phase_out",0xC152E4FC); return ms_Id; }
	inline fwMvFloatId GetRateOutId() const { static const fwMvFloatId ms_Id("rate_out",0x9305A09E); return ms_Id; }
	inline fwMvBooleanId GetLoopedOutId()const { static const fwMvBooleanId ms_Id("looped_out",0xD0457806); return ms_Id; }

	// PURPOSE: Creates the network player and attaches it to the ped at the requested priority.
	void StartNetworkPlayer(CDynamicEntity* pEntity, float transitionDuration, bool doInsert = true);

	// Keep a reference to the clip being played back - so we can ref it.
	pgRef<const crClip> m_pClip;

	// The blend out rate to use when automatically blending the clipId out at the end.
	// if this is zero the clipId will not automatically blend out at the end
	float m_AutoBlendOutDelta;
	bool m_bHoldAtEnd : 1;
	float m_lastPhase;

	// PURPOSE: callback functor used to insert the network into its parent
	fwMove::InsertNetworkCB m_insertCB;
};

//////////////////////////////////////////////////////////////////////////

class CChatHelper
{
public:

	static bank_u32 ms_iDefaultMinTimeBeforeChatReset;
	static bank_u32 ms_iDefaultMaxTimeBeforeChatReset;
	static bank_u32 ms_iDefaultValidityTime; 

	static CChatHelper* CreateChatHelper();
	~CChatHelper();

		// Declare a pool from which to allocate memory for chat helpers
	FW_REGISTER_CLASS_POOL(CChatHelper);

	static CTask*		GetPedsTask(CPed* pPed);

	void				ProcessChat(CPed* pPed);	
	bool				ShouldStartTaskAsInitiator();
	bool				ScanForInComingChat();
	void				FinishChat();
	bool				ShouldGoToChatState();
	CPed*				GetInComingChatPed() const		{ return m_pInComingChatPed.Get(); }		// Gets any incoming chat request ped or null if none exists
	void				SetInComingChatPed(CPed* pPed)	{ m_pInComingChatPed = pPed; }				// Sets the incoming chat request ped of this ped as pPed
	CPed*				GetOutGoingChatPed() const		{ return m_pOutGoingChatPed.Get(); }		// Gets any incoming chat request ped or null if none exists
	bool				GetIsChatting() const			{ return m_bIsChatting; }
	void				SetIsChatting(const bool bIsChatting) { m_bIsChatting = bIsChatting; }
	void				SetBestChatPed(CPed* pPed)			{ m_pBestChatTarget = pPed; m_iTimeBestPedWasValid = fwTimer::GetTimeInMilliseconds(); }
	void				UpdateChat();			// Scans periodically for good chat peds
	CPed*				GetBestChatPed() const; // Gets the best chat ped (valid from upto a second ago)	
	void				SetTimeBeforeChatTimerReset(u32 iResetTime) { m_iTimeDelayBeforeChatTimerReset = iResetTime; }

	u32				GetValidityTime() const { return m_iValidityTime; }
	void				SetValidityTime(u32 iValidityTime) { m_iValidityTime = iValidityTime; } 

	u32				GetTimeDelayBeforeChatTimerReset() const { return m_iTimeDelayBeforeChatTimerReset; }
	void				SetTimeDelayBeforeChatTimerReset(u32 iTimeDelayBeforeChatTimerReset) { m_iTimeDelayBeforeChatTimerReset = iTimeDelayBeforeChatTimerReset; } 

#if !__FINAL
	void				Debug() const;
#endif // !__FINAL

private:
	CChatHelper(); // Use create chathelper function to construct (prevents crash on ps3)

private:
	RegdPed				m_pInComingChatPed;					// Used to handle incoming chat requests
	RegdPed				m_pOutGoingChatPed;
	RegdPed				m_pThisPed;
	RegdPed				m_pBestChatTarget;

	u32				m_iTimeBestPedWasValid;	
	bool				m_bIsSet;
	bool				m_bShouldChat;						// Should we chat or not (depends on if we have a valid chat ped and we've not attempted to chat recently)	
	CTaskGameTimer		m_chatTimer;						// Timer used to decide whether to attempt to chat

	// Variables setable/getable by script
	u32				m_iTimeDelayBeforeChatTimerReset;	// The time before we attempt to chat again
	u32				m_iValidityTime;					// The time a ped is considered valid
	bool				m_bIsChatting;
};

class CClearanceSearchHelper
{
public:
	CClearanceSearchHelper(float fDistanceToTest = 25.0f); // Default to testing with a length of 25 meters

	~CClearanceSearchHelper();
																	
	// Helper to perform an 8 directional clearance search			
	// to determine the largest clear path from a point.			
	// Uses async probes.											
	// GetBestDirection will return either the first vector direction
	// that doesn't intersect anything, or the largest clear vector from
	// the search point

	typedef enum 
	{
		North = 0,
		NorthEast,
		East,
		SouthEast,
		South,
		SouthWest,
		West,
		NorthWest,
		MaxNumDirections
	} ProbeDirection;

	void			Start(const Vector3 &vPositionToSearchFrom);
	void			Update();
	bool			IsFinished() const { return m_bFinished; }
	void			GetShortestDirection(Vector3& vShortestDirectionOut);
	void			GetLongestDirection(Vector3& vLongestDirectionOut);
	void			GetLongestDirectionAway(Vector3& vLongestDirectionOut, Vector3& vAvoidDirection);
	void			GetDirection(int i, Vector3& vDirectionOut);
	float			GetDistance(int i) { return m_fDirectionVectorDistances[i]; }
	bool			IsActive() const		{ return m_bIsActive; }
	void			GetDirectionVector(ProbeDirection probeDirection, Vector3& vDirectionOut) const;
#if !__FINAL
	void			Debug() const;
#endif // !__FINAL
private:
	void			Reset();
	void			IssueWorldProbe(const Vector3& vPositionToSearchFrom, const Vector3& vPositionToEndSearch);
	
	CAsyncProbeHelper	m_probeTestHelper;			// Used to do the line tests
	Vector3				m_vSearchPosition;
	// No need to store the actual vectors - this just bloats the size of the helper
	// We can calculate the exact vectors from the distances below
	float				m_fDirectionVectorDistances[MaxNumDirections];
	ProbeDirection		m_eCurrentProbeDirection;
	bool				m_bIsActive;
	bool				m_bFinished;
	float				m_fDistanceToTest;			// How far we should do the line tests

};

// A class to handle the streaming of reaction clips into the patrol, stand guard and investigation tasks
class CReactionClipVariationHelper
{
public:
	CReactionClipVariationHelper(); 

	~CReactionClipVariationHelper();

	void		Update(CPed* pPed);
	s32			GetSelectedDictionaryIndex() const;
	void		CleanUp();

private:
	CTaskTimer					m_reactionVariationRequestTimer;
	CAmbientClipRequestHelper	m_reactionVariationRequestHelper;
};

// This class allows playback of clips whilst attached to an entity or a world location
// the functions compute a new position/orientation (situation) directly from the clipId each frame with added functionality
// to fix up the result depending on the target situation the ped should end up in
// The initial situation is stored as an offset to the target situation in case the target is moving (e.g. a vehicle)
class CPlayAttachedClipHelper
{
public:

	CPlayAttachedClipHelper();
	~CPlayAttachedClipHelper();

	void		GetTarget(Vector3& vTarget, Quaternion& qTarget) const { vTarget = m_vTargetPosition; qTarget = m_qTargetRotation; }
	void		GetTargetPosition(Vector3& vTarget) const { vTarget = m_vTargetPosition; }
	float		GetTargetPositionZ() const { return m_vTargetPosition.z; }

	// Should be called initially to set the desired target situation, and continuously updated if the target changes
	void		SetTarget(const Vector3& vTargetPosition, const Quaternion& qTargetRotation, bool bIgnoreTranslation = false) { if (!bIgnoreTranslation) { m_vTargetPosition = vTargetPosition; } m_qTargetRotation = qTargetRotation; DEV_ONLY(m_Flags.SetFlag(ACH_TargetSet)); }
	// Should be called before starting the clipId but after the above function to store the ped's initial situation as offsets to the target
	void		SetInitialOffsets(const CPed& ped, bool bIgnoreTranslation = false);

	void		SetInitialTranslationOffset(const Vector3& vTargetPosition) { m_vInitialOffset = vTargetPosition; }
	void		SetInitialRotationOffset(const Quaternion& qTargetRotation) { m_qInitialRotationalOffset = qTargetRotation; }
	Vector3		GetInitialTranslationOffset() const { return m_vInitialOffset; }
	Quaternion	GetInitialRotationOffset() const { return m_qInitialRotationalOffset; }

	// This function calculates the ideal situation to begin playing the clipId from (useful for telling a ped to go somewhere to start the clipId)
	void		ComputeIdealStartSituation(const crClip* pClip, Vector3& vIdealStartPosition, Quaternion& qIdealStartOrientation);
	// This function calculates the ideal situation to end the clipId from an initial position (with option of performing a ground position check)
	// fDistanceShiftedInZ is how much the ideal end position's z component was shifted by probe for the ground.
	// Returns true if the ground position check succeeded (if one was requested).
	bool		ComputeIdealEndSituation(CPed* pPed, const Vector3& vInitialPosition, const Quaternion& qInitialOrientation, const crClip* pClip, Vector3& vIdealEndPosition, Quaternion& qIdealEndOrientation, float& fDistanceShiftedInZ, bool bFindGroundPos = true, bool bCheckVehicles = true);
	// Computes the initial situation from the initial offsets (respects attachments as long as set target is called before this)
	void		ComputeInitialSituation(Vector3& vInitialPosition, Quaternion& qInitialRotation, bool bIgnoreTranslation = false);
	// This should be called after compute initial position and orientation to 'update' the clipId absolutely
	void		ComputeSituationFromClip(const crClip* pClip, float fStartPhase, float fClipPhase, Vector3& vNewPosition, Quaternion& qNewOrientation, float fTimeStep = 0.0f, CPed* pPed = NULL, Quaternion* qOrignalOrientation = NULL);
	// This function computes the difference between the remaining movement in the clipId and the remaining movement to the target situation
	void		ComputeOffsets(const crClip* pClip, float fClipPhase, float fEndPhase, const Vector3& vNewPosition, const Quaternion& qNewOrientation, Vector3& vOffset, Quaternion& qOffset, Quaternion* qOptionalSeatOrientation = NULL, const Vector3* vScale = NULL, bool bClipHasRotation = false);
	// This function adds the difference calculated above to enable clips to nail the target situation
	void		ApplyOffsets(Vector3& vCurrentPos, Quaternion& qCurrentOrientation, const crClip* pClip, const Vector3& vOffset, const Quaternion& qOffset, float fClipPhase);

	//toggle the enabling of translational offset for clips
	void		SetTranslationalOffsetSetEnabled(bool enable) { m_Flags.ChangeFlag(ACH_EnableTOffset, enable); }
	//toggle the enabling of rotational offset for clips
	void		SetRotationalOffsetSetEnabled(bool enable) { m_Flags.ChangeFlag(ACH_EnableROffset, enable); }

	//accessor for whether translational offset is enabled
	const bool GetTranslationalOffsetEnabled() { return m_Flags.IsFlagSet(ACH_EnableTOffset); }
	//accessor for whether translational offset is enabled
	const bool GetRotationalOffsetEnabled() { return m_Flags.IsFlagSet(ACH_EnableROffset); }

	// Temp until all clips have been moved over to using fixup events
	void		ApplyMountOffsets(Vector3& vCurrentPos, Quaternion& qCurrentOrientation, const crClip* pClip, const Vector3& vOffset, const Quaternion& qOffset, float fClipPhase);
	void		ApplyBikePickPullEntryOffsets(Vector3& vCurrentPos, Quaternion& qCurrentOrientation, const crClip* pClip, const Vector3& vOffset, const Quaternion& qOffset, float fClipPhase);
	void		ApplyVehicleEntryOffsets(Vector3& vCurrentPos, Quaternion& qCurrentOrientation, const crClip* pClip, const Vector3& vOffset, const Quaternion& qOffset, float fClipPhase, bool bApplyIfTagsNotFound = true, bool bGettingOnDirectWhenOnSide = false, const CVehicle* pVehicle = NULL);
	void		ApplyVehicleShuffleOffsets(Vector3& vCurrentPos, Quaternion& qCurrentOrientation, const crClip* pClip, const Vector3& vOffset, const Quaternion& qOffset, float fClipPhase);
	void		ApplyVehicleExitOffsets(Vector3& vCurrentPos, Quaternion& qCurrentOrientation, const crClip* pClip, const Vector3& vOffset, const Quaternion& qOffset, float fClipPhase);

	float		ComputeFixUpThisFrame(float fTotalFixUp, float fClipPhase, float fStartPhase, float fEndPhase);
	bool		FindGroundPos(CPed* pPed, const Vector3& vTestPosition, float fTestDepth, Vector3& vGroundPos, bool bCheckVehicles=true);

private:

	// Internal helper functions
	Quaternion	ComputeRotationalFixUpThisFrame(const Quaternion& qTotalRotationalFixUp, float fClipPhase, float fStartPhase, float fEndPhase);

	// Gets the phase for the specified fixup event
	bool		FindFixUpEventPhase(int iEventToFind, float& fEventPhase, const crClip* pClip, const float fStartPhase = 0.0f, const float fStopPhase = 1.0f) const;

protected:
	enum
	{
									//translational and rotational offsets are defaulted to enabled
		ACH_EnableTOffset	= BIT0,	//whether translational offset is enabled
		ACH_EnableROffset	= BIT1,	//whether rotational offset is enabled
#if __DEV
		ACH_TargetSet		= BIT2
#endif
	};


	Quaternion	m_qInitialRotationalOffset;
	Quaternion	m_qTargetRotation;

	Vector3		m_vInitialOffset;
	Vector3		m_vTargetPosition;

	fwFlags8 m_Flags;
};

class CVehicleClipRequestHelper
{
public:
	FW_REGISTER_CLASS_POOL(CVehicleClipRequestHelper);

	// Statics
	static const s32 MAX_NUM_CLIPSET_REQUESTS = 10;
	static const s32 MAX_NUM_VEHICLES_TO_CONSIDER = 5;

#if __BANK
	const CVehicle* m_apVehiclesConsidered[MAX_NUM_VEHICLES_TO_CONSIDER];

	void			Debug() const;
#endif

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		float m_MinDistanceToScanForNearbyVehicle;
		float m_MaxDistanceToScanForNearbyVehicle;
		float m_MinDistUpdateFrequency;
		float m_MaxDistUpdateFrequency;
		float m_MinDistPercentageToScaleScanArc;
		float m_MinDistScanArc;
		float m_MaxDistScanArc;

		bool m_DisableVehicleDependencies;
		bool m_DisableStreamedVehicleAnimRequestHelper;
		bool m_EnableStreamedEntryAnims;
		bool m_EnableStreamedInVehicleAnims;
		bool m_EnableStreamedEntryVariationAnims;
		bool m_StreamConnectedSeatAnims;
		bool m_StreamInVehicleAndEntryAnimsTogether;
		bool m_StreamEntryAndInVehicleAnimsTogether;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	


	enum VehicleClipFlags
	{
		AF_StreamSeatClips = BIT0,
		AF_StreamEntryClips = BIT1,
		AF_StreamExitClips = BIT2,
		AF_StreamEntryVariations = BIT3,
		AF_StreamConnectedSeatAnims = BIT4
	};

	CVehicleClipRequestHelper();
	~CVehicleClipRequestHelper();

	void SetPed(CPed* pPed) { m_pPed = pPed; }
	void Update(float fTimeStep);

	const CVehicle* StreamAnimsForNearbyVehiclesAndFindTargetVehicle();
	s32 FindTargetEntryPointIndex() const;
	void UpdateStreamedVariations(const CVehicle* pTargetVehicle, s32 iTargetEntryPointIndex);

	bool RequestClipsFromVehicle(const CVehicle* pVehicle, s32 iFlags = AF_StreamEntryClips|AF_StreamSeatClips, s32 iTargetSeat = 0);
	bool RequestCommonClipsFromEntryPoint(const CVehicle* pVehicle, s32 iEntryPointIndex);
	bool RequestClipsFromEntryPoint(const CVehicle* pVehicle, s32 iEntryPointIndex);
	bool RequestClipsFromExitPoint(const CVehicle* pVehicle, s32 iEntryPointIndex);
	const CConditionalClipSet* ChooseVariationClipSetFromEntryIndex(const CVehicle* pVehicle, s32 iEntryIndex);
	bool RequestClipsFromSeat(const CVehicle* pVehicle, s32 iSeatIndex);
	bool RequestClipSet(fwMvClipSetId clipSetId);
	const CVehicle* GetTargetVehicle() const { return m_pTargetVehicle.Get(); }
	s32	GetTargetEntryPointIndex() const { return m_iTargetEntryPointIndex; }
	const CConditionalClipSet* GetConditionalClipSet() const { return m_pConditionalClipSet; }
    s32 GetNumClipSetsRequested() const { return m_iNumClipSetsRequested; }
	void ClearAllRequests();

private:
	bool RequestStunnedClipsFromSeat(const CVehicle* pVehicle, s32 iSeatIndex);
	bool HasClipSetBeenRequested(fwMvClipSetId clipSetId) const;
	bool RequestDirectAccessEntryClipsForSeat(const CVehicle* pVehicle, s32 iSeat);
	bool RequestDirectAccessExitClipsForSeat(const CVehicle* pVehicle, s32 iSeat);
	void RequestEntryVariationClipSet(const CConditionalClipSet* pConditionalClipSet);
	s32	 GetClosestTargetEntryPointIndexForPed(const CVehicle* pVehicle, CPed* pPed, s32 iSeat, bool bConsiderNonDriverAccess = true) const;

private:

	RegdPed									m_pPed;
	fwMvClipSetId							m_aClipSetIdsRequested[MAX_NUM_CLIPSET_REQUESTS];	// Used to track which clipsets we've requested
	s32										m_iNumClipSetsRequested;
	s32										m_iNumVehiclesConsidered;
	float									m_fTimeSinceLastUpdate;
	float									m_fScanDistance;
	float									m_fUpdateFrequency;
	float									m_fScanArc;

	RegdConstVeh							m_pTargetVehicle;
	s32										m_iTargetEntryPointIndex;
	const CConditionalClipSet*				m_pConditionalClipSet;
};

////////////////////////////////////////////////////////////////////////////////

class CDirectionHelper
{
public:

	enum Direction
	{
		D_Invalid = -1,
		D_FrontLeft,
		D_FrontRight,
		D_BackLeft,
		D_BackRight,
		D_Left,
		D_Right,
		D_Max,
	};

	CDirectionHelper();
	~CDirectionHelper();

	static Direction	CalculateClosestDirection(Vec3V_In vFromPosition, float fFromHeading, Vec3V_In vToPosition);
	static Direction	CalculateClosestDirectionFromValue(float fValue);
	static float		CalculateDirectionValue(Vec3V_In vFromPosition, float fFromHeading, Vec3V_In vToPosition);
	static float		CalculateDirectionValue(float fFromHeading, float fToHeading);
	static Direction	MoveDirectionToLeft(Direction nDirection);
	static Direction	MoveDirectionToRight(Direction nDirection);
};

////////////////////////////////////////////////////////////////////////////////

class CVehicleDriftHelper
{

public:

	CVehicleDriftHelper();
	~CVehicleDriftHelper();
	
public:

	float GetValue() const { return m_fValue; }
	
public:

	void Update(float fMinValueForCorrection, float fMaxValueForCorrection, float fMinRate, float fMaxRate);
	
private:

	float m_fValue;
	float m_fRate;
	
};

////////////////////////////////////////////////////////////////////////////////

class CTalkHelper
{

public:

	struct Options
	{
		enum Flags
		{
			CanBeFriendly	= BIT0,
			CanBeUnfriendly	= BIT1,
		};
		
		Options(fwFlags8 uFlags, float fMaxDistance, float fMinDot)
		: m_fMaxDistance(fMaxDistance)
		, m_fMinDot(fMinDot)
		, m_uFlags(uFlags)
		{}

		float		m_fMaxDistance;
		float		m_fMinDot;
		fwFlags8	m_uFlags;
	};

public:

	CTalkHelper();
	~CTalkHelper();
	
public:

	static bool Talk(CPed& rTalker, const Options& rOptions);
	static bool Talk(CPed& rTalker, const CAmbientAudio& rAudio);
	static bool Talk(CPed& rTalker, CPed& rPedToTalkTo, const CAmbientAudio& rAudio);
	static bool Talk(CPed& rTalker, CPed& rPedToTalkTo, const CAmbientAudioExchange& rExchange);
	
private:

	static Vec3V_Out	CalculateDirectionForPedToTalk(const CPed& rPed);
	static bool			CanPedTalk(const CPed& rPed);
	static bool			CanScorePedToTalkTo(const CPed& rTalker, const CPed& rPedToTalkTo, const Options& rOptions, CAmbientAudioExchange& rExchange);
	static bool			CanTalkToPed(const CPed& rPedToTalkTo);
	static CPed*		FindPedToTalkTo(const CPed& rTalker, const Options& rOptions, CAmbientAudioExchange& rExchange);
	static void			GetExchangeForPed(const CPed& rTalker, const CPed& rPedToTalkTo, const Options& rOptions, CAmbientAudioExchange& rExchange);
	static void			GetResponseForPed(const CPed& rTalker, const CPed& rPedToTalkTo, CAmbientAudio& rAudio);
	static bool			IsPedFlippingSomeoneOff(const CPed& rPed, Vec3V_InOut vDirection);
	static void			OnPedFailedToTalk(const CPed& rTalker, const CAmbientAudio& rAudio);
	static void			OnPedTalkedToPed(CPed& rTalker, CPed& rPedToTalkTo, const CAmbientAudio& rAudio);
	static float		ScorePedToTalkTo(const CPed& rTalker, const CPed& rPedToTalkTo, const Options& rOptions);
	
};

////////////////////////////////////////////////////////////////////////////////

class CUpperBodyAimPitchHelper
{
public:

	static fwMvNetworkId	ms_UpperBodyAimNetworkId;
	static fwMvRequestId	ms_BlendInUpperBodyAimRequestId;
	static fwMvFloatId		ms_UpperBodyBlendInDurationId;
	static fwMvFloatId		ms_PitchId;
	static fwMvFloatId		ms_AimBlendInDurationId;
	static fwMvFloatId		ms_GripBlendInDurationId;

	static void		CalculateAimVector(const CPed* pPed, Vector3& vStart, Vector3& vEnd, Vector3& vTargetPos, const bool bForceUseCameraIfPlayer = false);
	static float	ComputeDesiredPitchSignal(const CPed& ped, float* pfOverrideDesiredPitch = NULL, const bool bForceUseCameraIfPlayer = false, const bool bUntranformRelativeToPed = false);

	CUpperBodyAimPitchHelper();
	~CUpperBodyAimPitchHelper();

	bool IsBlendedIn() const { return m_moveNetworkHelper.IsNetworkActive() ? m_moveNetworkHelper.IsNetworkAttached() : false; }
	bool BlendInUpperBodyAim(CMoveNetworkHelper& parentMoveNetworkHelper, const CPed& ped, float fBlendInDuration, const bool bForceUseCameraIfPlayer = false, float fAimTransitionBlend = INSTANT_BLEND_DURATION, float fGripTransitionBlend = INSTANT_BLEND_DURATION);
	void Update(const CPed& ped, const bool bForceUseCameraIfPlayer = false);
	void SetClipSet(fwMvClipSetId clipSetId) { m_ClipSetId = clipSetId; }
	float GetCurrentPitch() { return m_fCurrentPitchSignal; }
	fwMvClipSetId GetClipSet() { return m_moveNetworkHelper.IsNetworkActive() ? m_moveNetworkHelper.GetClipSetId() : CLIP_SET_ID_INVALID; }
	fwMvClipSetId GetDefaultClipSet(const CPed& ped) const;

private:

	void UpdatePitchParameter(const CPed& ped, const bool bForceUseCameraIfPlayer);

	CMoveNetworkHelper	m_moveNetworkHelper;
	float				m_fCurrentPitchSignal;
	fwMvClipSetId		m_ClipSetId;
};

////////////////////////////////////////////////////////////////////////////////

class CEquipWeaponHelper
{

public:

	enum Response
	{
		R_None,
		R_Keep,
		R_MatchTarget,
		R_EquipBest,
		R_Max,
	};

public:

	struct EquipBestWeaponInput
	{
		EquipBestWeaponInput()
		: m_bForceIntoHand(false)
		, m_bProcessWeaponInstructions(false)
		{}

		bool m_bForceIntoHand;
		bool m_bProcessWeaponInstructions;
	};

	struct EquipBestMeleeWeaponInput
	{
		EquipBestMeleeWeaponInput()
		: m_bForceIntoHand(false)
		, m_bProcessWeaponInstructions(false)
		{}

		bool m_bForceIntoHand;
		bool m_bProcessWeaponInstructions;
	};

	struct EquipWeaponForTargetInput
	{
		EquipWeaponForTargetInput()
		: m_EquipBestWeaponInput()
		, m_EquipBestMeleeWeaponInput()
		{}

		EquipBestWeaponInput		m_EquipBestWeaponInput;
		EquipBestMeleeWeaponInput	m_EquipBestMeleeWeaponInput;
	};

public:

	CEquipWeaponHelper();
	~CEquipWeaponHelper();

public:

	static void EquipBestMeleeWeapon(CPed& rPed, const EquipBestMeleeWeaponInput& rInput);
	static void EquipBestWeapon(CPed& rPed, const EquipBestWeaponInput& rInput);
	static void EquipWeaponForTarget(CPed& rPed, const CPed& rTarget, const EquipWeaponForTargetInput& rInput);
	static void MatchTargetWeapon(CPed& rPed, const CPed& rTarget, const EquipBestWeaponInput& rBestWeaponInput, const EquipBestMeleeWeaponInput& rBestMeleeWeaponInput);
	static void RequestEquippedWeaponAnims(CPed& rPed);
	static void ReleaseEquippedWeaponAnims(CPed& rPed);
	static bool AreEquippedWeaponAnimsStreamedIn(CPed& rPed);
	static bool ShouldPedSwapWeapon(const CPed& rPed);

private:

	static Response ChooseResponseFromAttributes(const CPed& rPed, const CPed& rTarget);
	static Response ChooseResponseFromFriends(const CPed& rPed);
	static Response ChooseResponseFromPersonality(const CPed& rPed, const CPed& rTarget);
	static bool		IsArmed(const CPed& rPed);
	static bool		IsArmedWithGun(const CPed& rPed);
	static bool		IsHelpingWithFistFight(const CPed& rPed, const CPed& rTarget);
	static bool		ShouldAlwaysEquipBestWeapon(const CPed& rPed, const CPed& rTarget);
	static bool		ShouldAlwaysKeepWeapon(const CPed& rPed, const CPed& rTarget);
	static bool		ShouldAlwaysMatchTargetWeapon(const CPed& rPed, const CPed& rTarget);
	static bool		ShouldNeverEquipBestWeapon(const CPed& rPed, const CPed& rTarget);
	static bool		ShouldNeverKeepWeapon(const CPed& rPed, const CPed& rTarget);
	static bool		ShouldNeverMatchTargetWeapon(const CPed& rPed, const CPed& rTarget);

};

////////////////////////////////////////////////////////////////////////////////

class CWeaponInfoHelper
{

public:

	enum Armament
	{
		A_None,
		A_Equipped,
		A_Best,
	};

	enum State
	{
		S_None,
		S_NonViolent,
		S_Unarmed,
		S_Melee,
		S_Armed,
	};

public:

	CWeaponInfoHelper();
	~CWeaponInfoHelper();

public:

	static State CalculateState(const CPed& rPed, Armament nArmament);

};

////////////////////////////////////////////////////////////////////////////////

class CFindNearestCarNodeHelper : public CTaskHelperFSM
{

public:

	enum State
	{
		State_SearchNotStarted = 0,
		State_StartingSearch , 
		State_FloodFillSearchInProgress, 
		State_StartingLargerSearch , 
		State_LargerFloodFillSearchInProgress, 
		State_Failed, 
		State_Success,
		State_Finished,
	};

public:

	struct GenerateRandomPositionNearRoadNodeInput
	{
		GenerateRandomPositionNearRoadNodeInput(const CNodeAddress& rNode)
		: m_Node(rNode)
		, m_fMaxDistance(20.0f)
		, m_fMaxAngle(PI)
		{}

		CNodeAddress	m_Node;
		float			m_fMaxDistance;
		float			m_fMaxAngle;
	};

public:

	CFindNearestCarNodeHelper();
	~CFindNearestCarNodeHelper();

public:

	bool GetNearestCarNode(CNodeAddress& rNode) const;
	bool GetNearestCarNodePosition(Vector3& vNodePosition) const;
	void SetSearchPosition(const Vector3& vPosition);
	void Reset();

public:

	static bool GenerateRandomPositionNearRoadNode(const GenerateRandomPositionNearRoadNodeInput& rInput, Vec3V_InOut vPosition);

private:

	// State Machine Update Functions
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);

	FSM_Return		SearchNotStarted_OnUpdate();
	FSM_Return		StartingSearch_OnUpdate();
	FSM_Return		FloodFillSearchInProgress_OnUpdate(); 
	FSM_Return		StartingLargerSearch_OnUpdate();
	FSM_Return		LargerFloodFillSearchInProgress_OnUpdate(); 
	FSM_Return		Failed_OnUpdate();
	FSM_Return		Success_OnUpdate();

private:

	bool ShouldStartNewSearch() const;

private:

	TPathHandle		m_pathHandle;
	Vector3			m_vNearestCarNodePosition;
	Vector3			m_vLastSearchPosition;
	Vector3			m_vSearchPosition;
	CNodeAddress	m_NearestCarNode;
	float			m_fTimeSinceLastSearch;
	bool			m_bStartNewSearch;
	bool			m_bActive;

public:

#if !__FINAL
	virtual const char* GetName() const { return "FIND_NEAREST_CAR_NODE_HELPER"; }
	virtual const char* GetStateName() const;
	virtual void	Debug(s32 iIndentLevel) const;
#endif // !__FINAL

};

////////////////////////////////////////////////////////////////////////////////

class CReactToBeingJackedHelper
{

public:

	static void CheckForAndMakeOccupantsReactToBeingJacked(const CPed& rJacker, CEntity& rEntityWithOccupants, int iEntryPoint, bool bIgnoreIfPedInSeatIsLaw);

};

////////////////////////////////////////////////////////////////////////////////

class CCoverPointStatusHelper : public CTaskHelperFSM
{

public:

	enum State
	{
		State_Waiting = 0,
		State_CheckProbe,
		State_CheckCover,
		State_CheckVantage,
		State_Finished,
	};

public:

	CCoverPointStatusHelper();
	~CCoverPointStatusHelper();

public:

	CCoverPoint*	GetCoverPoint() const { return m_pCoverPoint; }
	bool			GetFailed()		const { return m_bFailed; }
	u8				GetStatus()		const { return m_uStatus; }

public:

	bool IsActive() const
	{
		return (GetState() != State_Waiting);
	}

	bool IsFinished() const
	{
		return (GetState() == State_Finished);
	}

public:
 
	void Reset();
	void Start(CCoverPoint* pCoverPoint);

public:

#if !__FINAL
	virtual const char* GetName() const { return "CCoverPointStatusHelper"; }
	virtual const char* GetStateName() const;
#endif // !__FINAL

private:

	bool IsCoverPointValid() const;

private:

	FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Waiting_OnUpdate();
	void		CheckProbe_OnEnter();
	FSM_Return	CheckProbe_OnUpdate();
	void		CheckProbe_OnExit();
	void		CheckCover_OnEnter();
	FSM_Return	CheckCover_OnUpdate();
	void		CheckCover_OnExit();
	void		CheckVantage_OnEnter();
	FSM_Return	CheckVantage_OnUpdate();
	void		CheckVantage_OnExit();
	void		Finished_OnEnter();
	FSM_Return	Finished_OnUpdate();

private:

	WorldProbe::CShapeTestResults	m_ProbeResults;
	WorldProbe::CShapeTestResults	m_CoverResults;
	WorldProbe::CShapeTestResults	m_VantageResults;
	Vector3							m_vCoverCapsuleStart;
	Vector3							m_vCoverCapsuleEnd;
	Vector3							m_vVantageCapsuleStart;
	Vector3							m_vVantageCapsuleEnd;
	CCoverPoint*					m_pCoverPoint;
	float							m_fProbedGroundHeight;
	u8								m_uStatus;
	bool							m_bFailed;
	bool							m_bReset;
	bool							m_bHasProbedGroundHeight;

};

////////////////////////////////////////////////////////////////////////////////

struct CNearestPedInCombatHelper
{
	enum OptionFlags
	{
		OF_MustBeLawEnforcement		= BIT0,
		OF_MustBeOnFoot				= BIT1,
		OF_MustBeOnScreen			= BIT2,
	};

	static CPed* Find(const CPed& rTarget, fwFlags8 uOptionFlags = 0);
};

////////////////////////////////////////////////////////////////////////////////

struct CPositionFixupHelper
{
	enum Flags
	{
		F_IgnoreZ = BIT0,
	};

	static void Apply(CPed& rPed, Vec3V_In vTarget, const crClip& rClip, float fPhase, float fMaxPositionChangePerSecond, float fTimeStep, fwFlags8 uFlags = 0);
};

////////////////////////////////////////////////////////////////////////////////

struct CHeadingFixupHelper
{
	static void Apply(CPed& rPed, Vec3V_In vTarget, const crClip& rClip, float fPhase, float fMaxHeadingChangePerSecond, float fTimeStep);
	static void Apply(CPed& rPed, Vec3V_In vTarget, const crClip& rClip, float fPhase, float fMaxHeadingChange);
	static void Apply(CPed& rPed, float fTargetHeading, const crClip& rClip, float fPhase, float fMaxHeadingChangePerSecond, float fTimeStep);
	static void Apply(CPed& rPed, float fTargetHeading, const crClip& rClip, float fPhase, float fMaxHeadingChange);
};

////////////////////////////////////////////////////////////////////////////////

struct CExplosionHelper
{
	static bool CalculateTimeUntilExplosion(const CEntity& rEntity, float& fTime);
};

////////////////////////////////////////////////////////////////////////////////




class CScenarioClipHelper : public CExpensiveProcess
{
public:

	static const s32 sm_WinnerArraySize = 3;
	static const s32 sm_IndexTranslationArraySize = 8;
	static const u32 sm_NumberOfResults = 8;

	typedef enum
	{
		ClipCheck_Failed,
		ClipCheck_Pending,
		ClipCheck_Success,
	} ScenarioClipCheckType;

	struct sClipSortStruct
	{
		float fClipDelta;
		s8 iClipIndex;
	};

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		// Nothing here anymore, but I don't think we should delete the tunable struct just yet as there will probably
		// be a need for it in the future.

		PAR_PARSABLE;
	};

public:
	
	CScenarioClipHelper();

	virtual ~CScenarioClipHelper();

	ScenarioClipCheckType PickClipFromSet(const CPed* pPed, const CEntity* pScenarioEntity, const CAITarget& rTarget, fwMvClipSetId clipSetId, fwMvClipId &chosenClipId, 
		bool bRotateToTarget, bool bDoProbeChecks, bool bForceOneFrameCheck, bool bCheckFullLengthOfAnimation=false, bool bIgnoreWorldGeometryInCheck=false,
		float fFinalZOverride=0.0f, float fCapsuleRadiusOverride=0.0f);

	void PickFrontFacingClip(fwMvClipSetId, fwMvClipId &chosenClipId);

	// Reset the list of previously clear clips.
	void ResetPreviousWinners();
	bool CanStoreAnyMoreWinningIndices() const;

	// Forget about any in-progress shapetests and restore memory.
	void Reset();

private:

	CScenarioClipHelper(const CScenarioClipHelper& helper);
	const CScenarioClipHelper& operator=(const CScenarioClipHelper& helper);

	void FireCapsuleTest(const CPed* pPed, const CEntity* pScenarioEntity, const Vector3& vStart, const Vector3& vEnd, float fRadius, bool bAsync, bool bIgnoreWorldGeometryDuringExitProbeCheck);

	ScenarioClipCheckType CheckCapsuleTest();

	void StorePreviousWinner(s8 iIndex);
	bool IsIndexInPreviousWinners(s8 iIndex) const;

	void BuildTranslationArray(const fwClipSet& rClipSet, float fHeading, bool bRotateToTarget);
	bool HasBuiltTranslationArray() const;
	void ResetTranslationArray();

private:

	u8 m_iCurrentExitClipIndex;

	// Collection of previous "good clips" that we discovered using this helper.
	s8 m_aPreviousWinners[sm_WinnerArraySize];

	// Array of clip indices ordered by closest rotation to target first.
	s8 m_aTranslationArray[sm_IndexTranslationArraySize];

	WorldProbe::CShapeTestResults* m_pCapsuleResults;

	static Tunables sm_Tunables;

#if __BANK
	static bool sm_bRenderProbeChecks;
#endif
};

///////////////////////////////////////////////////////////////////////////////////////////
// CScenarioSpeedHelper
//   Helper logic for getting a ped on a scenario chain to match speed with another ped.
///////////////////////////////////////////////////////////////////////////////////////////

class CScenarioSpeedHelper
{
public:
	static bool ShouldUseSpeedMatching(const CPed& ped);
	static float GetMBRToMatchSpeed(const CPed& ped, const CPed& target, Vec3V_In vDestinationScenarioPoint);

private:
	static const float ms_fInnerCircle;
	static const float ms_fOuterCircle;
	static const float ms_fSpeedChangeBig;
	static const float ms_fSpeedChangeSmall;
	static const float ms_fSmallSpeedDiffThreshold;
};

////////////////////////////////////////////////////////////////////////////////

struct CFleeHelpers
{
	static void InitializeSecondaryTarget(const CAITarget& rTarget, CAITarget& rSecondaryTarget);
	static bool IsPositionOfTargetValid(const CAITarget& rTarget);
	static bool IsPositionValid(Vec3V_In vPosition);
	static bool ProcessTargets(CAITarget& rTarget, CAITarget& rSecondaryTarget);
	static bool ProcessTargets(CAITarget& rTarget, CAITarget& rSecondaryTarget, Vec3V_InOut vLastTargetPosition);
};

////////////////////////////////////////////////////////////////////////////////

struct CTimeHelpers
{
	static u32 GetTimeSince(u32 uTime);
};

////////////////////////////////////////////////////////////////////////////////

struct CLastNavMeshIntersectionHelper
{
	static bool Get(const CAITarget& rTarget, Vec3V_InOut vPosition);
	static bool Get(const CEntity& rEntity, Vec3V_InOut vPosition);
	static bool Get(const CPed& rPed, Vec3V_InOut vPosition);
};

////////////////////////////////////////////////////////////////////////////////

struct CHeliRefuelHelper
{
	static bool Process(const CPed& rPed, const CPed& rTarget);
};

////////////////////////////////////////////////////////////////////////////////

struct CHeliFleeHelper
{
	static CTask* CreateTaskForHeli(const CHeli& rHeli, const CAITarget& rTarget);
	static CTask* CreateTaskForPed(const CPed& rPed, const CAITarget& rTarget);
};

////////////////////////////////////////////////////////////////////////////////

struct CVehicleUndriveableHelper
{
	static bool IsUndriveable(const CVehicle& rVehicle);
};

////////////////////////////////////////////////////////////////////////////////

struct CVehicleStuckDuringCombatHelper
{
	static bool IsStuck(const CVehicle& rVehicle);
};

////////////////////////////////////////////////////////////////////////////////

struct CFindClosestFriendlyPedHelper
{
	static CPed* Find(const CPed& rPed, float fMaxDistance);
};

////////////////////////////////////////////////////////////////////////////////

#if FPS_MODE_SUPPORTED
class CFirstPersonIkHelper
{
public:
	enum eFirstPersonType
	{
		FP_OnFoot,
		FP_OnFootDriveBy,
		FP_DriveBy,
		FP_MobilePhone,
	};

	enum eArm
	{
		FP_ArmLeft = 0,
		FP_ArmRight
	};

	CFirstPersonIkHelper(eFirstPersonType = FP_OnFoot);
	~CFirstPersonIkHelper();

	void Process(CPed* pPed);
	void Reset(CPed* pPed = NULL, bool bResetSolver = false);
	void OnFireEvent(const CPed* pPed, const CWeaponInfo* pWeaponInfo);

	void SetArm(eArm arm) { m_uArm = (u8)arm; }

	void SetExtraFlags(u32 flags) { m_ExtraFlags |= flags; }
	void ClearExtraFlags(u32 flags) { m_ExtraFlags &= ~flags; }
	void ResetExtraFlags() { m_ExtraFlags = 0; }

	void SetOffset(Vec3V_In vOffset) { m_vOffset = vOffset; }
	Vec3V_Out GetOffset() const { return m_vOffset; }
	void SetUseRecoil(bool bUseRecoil) { m_bUseRecoil = bUseRecoil; }
	void SetUseAnimBlockingTags(bool bUseTags) { m_bUseAnimBlockingTags = bUseTags; }
	void SetUseSecondaryMotion(bool bUseSecondaryMotion) { m_bUseSecondaryMotion = bUseSecondaryMotion; }
	void SetUseLook(bool bUseLook) { m_bUseLook = bUseLook; }
	void SetBlendInRate(ArmIkBlendRate eRate) { m_eBlendInRate = eRate; }
	void SetBlendOutRate(ArmIkBlendRate eRate) { m_eBlendOutRate = eRate; }
	ArmIkBlendRate GetBlendInRate() const { return m_eBlendInRate; }
	ArmIkBlendRate GetBlendOutRate() const { return m_eBlendOutRate; }

	void SetMinMaxDriveByYaw(float fMin, float fMax) { m_fMinDrivebyYaw = fMin; m_fMaxDrivebyYaw=fMax; }

	eFirstPersonType GetType() const { return m_eFirstPersonType; }

	static bool ProcessLook(CPed* pPed, Mat34V& mtxEntity);

private:
	struct RecoilData
	{
		float m_fDisplacement;
		float m_fDuration;

		float m_fMaxDisplacement;
		float m_fMaxDuration;

		float m_fScaleBackward;
		float m_fScaleVertical;

		bool m_bActive;
	};

	struct SecondaryMotionData
	{
		camDampedSpring m_Movement[2];
		camDampedSpring m_Aiming[2];
		camDampedSpring m_Bounce[3];
		camDampedSpring m_BounceRotation[3];
		float m_fCurrentMbrY;
		float m_fCurrentTranslationBlend;
		float m_fCurrentRollScale;
		float m_fCurrentPitchScale;
		float m_fCurrentBounceScale;
		s32 m_LeftArmRollBoneIdx;
		s32 m_LeftForeArmRollBoneIdx;
	};

private:
	float SampleResponseCurve(float fInterval);
	bool ProcessRecoil(Mat34V& mtxSource);
	bool ProcessSecondaryMotion(CPed* pPed, Mat34V& mtxSource, QuatV& qAdditive);
	bool AcceptAnyCHWeightForCover(const CPed& rPed);

	Vec3V m_vOffset;
	RegdConstWeaponInfo m_pWeaponInfo;
	SecondaryMotionData m_SecondaryMotionData;
	RecoilData m_RecoilData;
	eFirstPersonType m_eFirstPersonType;

	float m_fMinDrivebyYaw;
	float m_fMaxDrivebyYaw;

	ArmIkBlendRate m_eBlendInRate;
	ArmIkBlendRate m_eBlendOutRate;

	u32 m_ExtraFlags;

	u8 m_bActive				: 1;
	u8 m_uArm					: 1;
	u8 m_bUseRecoil				: 1;
	u8 m_bUseAnimBlockingTags	: 1;
	u8 m_bUseSecondaryMotion	: 1;
	u8 m_bUseLook				: 1;
};
#endif // FPS_MODE_SUPPORTED

////////////////////////////////////////////////////////////////////////////////

struct CTargetInWaterHelper
{
	static bool IsInWater(const CEntity& rEntity);
};

////////////////////////////////////////////////////////////////////////////////

struct CCreekHackHelperGTAV
{
	static bool IsAffected(const CVehicle& rVehicle);
};

////////////////////////////////////////////////////////////////////////////////

//////////////////////////////
// CLASS: CPedsWaitingToBeOutOfSightManager
// PURPOSE: Manage peds waiting for being out of players sight to execute some action (Eg: repath if they are currently fleeing out of pavement).
//////////////////////////////
class CPedsWaitingToBeOutOfSightManager
{
public:

	//---------------------------------------------------
	// Auxiliary Types
	//---------------------------------------------------

	// PURPOSE: Returns if the ped should be removed from the manager
	typedef bool (*FnIsValidCB)(const CPed& rPed);

	// PURPOSE: Action to do when the ped is out of sight
	typedef void (*FnOnOutOfSightCB)(CPed& rPed);

	//
	// CLASS: CPedInfo
	// PURPOSE: Manage a single ped waiting to be out of sight
	//
	class CPedInfo
	{
	public:

		// PURPOSE: Ped state
		struct SState
		{
			enum Enum
			{
				UNDEFINED,
				QUEUED,
				IN_PROCESS,
				//...
				COUNT
			};

			static const char* GetName(Enum eState);
		};

		// PURPOSE: Check results
		struct STestResult
		{
			enum Enum
			{
				UNDEFINED,
				SUCCESS,
				FAILURE,
				DELAYED,
				WAITING_RESULT,
				TEST_ERROR,
				//...
				COUNT
			};

			static const char* GetName(Enum eState);
		};



		// Initialization / Destruction
		CPedInfo();

		void Init(CPed& rPed);
		void Done();

		STestResult::Enum UpdateCurrentTest(CPedsWaitingToBeOutOfSightManager& rManager);

		// Manager notifications
		void OnTestFailed() { m_uNumConsecutiveChecksPassed = 0; }
		void OnQueued() { m_hCurrentShapeTestHandle.Reset(); m_iCurrentShapeTestPlayerIdx = 0; m_uStartProcessedTS = 0; m_eState = SState::QUEUED; }
		void OnStartProcessing() { m_uStartProcessedTS = fwTimer::GetTimeInMilliseconds(); }

		// Getters
		bool IsValid();

		const WorldProbe::CShapeTestSingleResult& GetCurrentShapeTestHandle() const { return m_hCurrentShapeTestHandle; }
		s32 GetCurrentShapeTestPlayerIdx() const { return m_iCurrentShapeTestPlayerIdx; }
		void IncCurrentShapeTestPlayerIdx() { ++m_iCurrentShapeTestPlayerIdx; m_hCurrentShapeTestHandle.Reset(); }

		u32 GetNumConsecutiveChecksPassed() const { return m_uNumConsecutiveChecksPassed; }

		CPed* GetPed() { return m_pPed; }
		const CPed* GetPed() const { return m_pPed; }

		float GetInManagerTime() const { return m_uAddedToManagerTS > 0u ? (fwTimer::GetTimeInMilliseconds()  - m_uAddedToManagerTS) / 1000.0f : 0.0f; }
		float GetInProcessTime() const { return m_uStartProcessedTS > 0u ? (fwTimer::GetTimeInMilliseconds()  - m_uStartProcessedTS) / 1000.0f : 0.0f; }

		SState::Enum GetState() const { return m_eState; }
		STestResult::Enum GetLastTestResult() const { return m_eLastTestResult; }

		// State
		void SetState(const SState::Enum eState) { m_eState = eState; }

		// Shape Checks
		void IncNumConsecutiveChecksPassed() { ++m_uNumConsecutiveChecksPassed; }

	private:

		static STestResult::Enum ProcessShapeTestResult(WorldProbe::CShapeTestSingleResult& rResults);

		RegdPed m_pPed;
		u32 m_uAddedToManagerTS;
		u32 m_uStartProcessedTS;

		s32 m_iCurrentShapeTestPlayerIdx;
		WorldProbe::CShapeTestSingleResult m_hCurrentShapeTestHandle;

		u32 m_uNumConsecutiveChecksPassed;

		SState::Enum m_eState;
		STestResult::Enum m_eLastTestResult;
	};

public:

	// Initialization / destruction
	void Init(const u32 uQueueSize, u32 uMinMillisecondsBetweenShapeTests, u32 uNumConsecutiveChecksToSucceed, FnIsValidCB fnIsValid, FnOnOutOfSightCB fnOnOutOfSight);
	void Done();

	// Update
	void Process();

	// Peds management
	bool Add(CPed& rPed);
	void Remove(CPed& rPed);
	void Remove(atSNode<CPedInfo>& rNode);
	void ReQueue(atSNode<CPedInfo>& rNode);

	atSNode<CPedInfo>* Find(const CPed& rPed);

	// Debug
#if __BANK
	void DEBUG_RenderList();
#endif // __BANK

private:

	// Peds Processing
	void RemoveInvalidNodes();
	void ProcessNode(atSNode<CPedInfo>* pNode);
	void UpdateNodeState(atSNode<CPedInfo>* pNode);

	bool CanSubmitShapeTest() const { return fwTimer::GetTimeInMilliseconds() - m_uLastLOSCheckTS > m_uMinMillisecondsBetweenShapeTests; }
	void SubmitShapeTest(const Vector3& vStart, const Vector3& vEnd, WorldProbe::CShapeTestSingleResult* phCurrentShapeTestHandle);

	// Internal types
	typedef atSListPreAlloc<CPedInfo> TQueue;

	// Instance variables
	TQueue m_aPeds;
	FnIsValidCB m_fnIsValid;
	FnOnOutOfSightCB n_fnOnOutOfSight;
	u32 m_uLastLOSCheckTS;
	u32 m_uMinMillisecondsBetweenShapeTests;
	u32 m_uNumConsecutiveChecksToSucceed;
};

#endif // TASK_HELPERS_H
