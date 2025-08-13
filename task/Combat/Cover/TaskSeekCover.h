// Title	:	TaskSeekCover.h
// Author	:	Phil Hooker
// Started	:	31/05/05
// This selection of class' enables individual peds to seek and hide in cover

#ifndef TASK_SEEK_COVER_H
#define TASK_SEEK_COVER_H

#include "fwtl/pool.h"
#include "Task/System/Task.h"
#include "Task/System/TaskTypes.h"
#include "Scene/RefMacros.h"
#include "Scene/Entity.h"

// Class forward declarations
class CCoverPoint;
class CEntity;

//-------------------------------------------------------------------------
// PURPOSE:	enables individual peds to seek and hide in cover
//-------------------------------------------------------------------------

class CCoverFinderFSM : public CTaskHelperFSM
{
public:
	enum { COVER_SEARCH_ANY = 0, COVER_SEARCH_BY_POS, COVER_SEARCH_BY_SCRIPTED_ID };

	enum
	{
		SEARCH_INACTIVE = 0,
		SEARCH_ACTIVE,
		SEARCH_FAILED,
		SEARCH_SUCCEEDED
	};

	enum
	{
		FAIL_REASON_NONE = 0,
		FAIL_REASON_NAV_LINK,
		FAIL_REASON_DANGEROUS_NAV_LINK
	};

	typedef enum
	{
		State_Waiting = 0,
		State_SearchForCover,
		State_CheckCoverPointAccessibility,
		State_CheckTacticalAnalysisLos,
		State_CheckCachedCoverPointBlockedStatus,
		State_CheckCoverPointBlockedStatus,
		State_CheckLosToCoverPoint,
		State_CheckRouteToCoverPoint,
		State_CoverSearchFailed
	} SeekCoverState;

	CCoverFinderFSM();
	~CCoverFinderFSM();

	// FSM required implementations
	FSM_Return				ProcessPreFSM();
	virtual FSM_Return		UpdateFSM	(const s32 iState, const FSM_Event iEvent);

#if !__FINAL
	virtual const char*		GetName() const { return "COVER_FINDER"; }
	static const char*		GetStaticStateName( s32 );
	virtual void			Debug(s32 iIndentLevel) const;
#endif // !__FINAL

	// Function prototype of a callback fn used to check the validity of a selected cover point
	void SetCoverPointFilterConstructFn( CCover::CoverPointFilterConstructFn* pFn ) {m_pCoverPointFilterConstructFn = pFn;}

	// PURPOSE: A setup function to set the most commonly required variables in one call
	void SetupSearchParams(const CPed* pTargetPed, const Vector3& vTargetPos, s32 iFlags, float fMinDistance, float fMaxDistance, 
						  const s32 iSearchType, CCover::CoverPointFilterConstructFn* pFn);

	void SetMinMaxSearchDistances( float fMinDistance, float fMaxDistance );
	void SetCoverPointIndex(const s32 iCoverPointIndex);
	void SetFlags(s32 iFlags) { m_flags = iFlags; }
	void SetSearchType(const s32 iSearchType) { m_iCoverSearchType = iSearchType; }
	void SetSearchMode(const s32 iSearchMode) { m_iCoverSearchMode = iSearchMode; }
	void SetTargetPed(const CPed* pTargetPed) { m_pTargetPed = pTargetPed; }
	void SetTargetPosition(const Vector3& vTargetPos) { m_TargetPos = vTargetPos; }
	void SetCoverStartSearchPos(const Vector3& vCoverStartPos) { m_vCoverStartSearchPos = vCoverStartPos; }

	s32  GetCoverPointIndex() const { return m_iScriptedCoverPointID; }
	s32  GetSearchMode() const { return m_iCoverSearchMode; }
	s32	 GetSearchType() const { return m_iCoverSearchType; }
	const CPed* GetTargetPed() const { return m_pTargetPed; }
	const Vector3& GetTargetPosition() const { return m_TargetPos; }
	const Vector3& GetCoverStartSearchPos() const { return m_vCoverStartSearchPos; }

	void StartSearch(CPed* pPed); 
	bool IsActive() const { return m_bIsActive; }
	s32 GetLastSearchResult(bool bResetSearchResult);

	void ValidateCoverPoints();

protected:

	enum { COVER_POINT_HISTORY_SIZE = 8 };

	// FSM functions
	// Waiting
	FSM_Return	Waiting_OnUpdate();

	// Search for a valid cover point
	void		SearchForCover_OnEnter();
	FSM_Return	SearchForCover_OnUpdate();

	// Check to make sure the cover point is not blocked using the cover blocked status helper
	FSM_Return	CheckCachedCoverPointBlockedStatus_OnUpdate();
	void		CheckCoverPointBlockedStatus_OnEnter();
	FSM_Return	CheckCoverPointBlockedStatus_OnUpdate();

	// Check to make sure the cover point is accessible (used to be a synchronous probe)
	void		CheckCoverPointAccessibility_OnEnter();
	FSM_Return	CheckCoverPointAccessibility_OnUpdate();
	void		CheckCoverPointAccessibility_OnExit();

	// CheckLosToCoverPoint
	FSM_Return	CheckTacticalAnalysisInfo_OnUpdate(bool bIsBlockedCheck);
	void		CheckLosToCoverPoint_OnEnter();
	FSM_Return	CheckLosToCoverPoint_OnUpdate();
	void		CheckLosToCoverPoint_OnExit();

	// CheckRouteToCoverPoint
	void		CheckRouteToCoverPoint_OnEnter();
	FSM_Return	CheckRouteToCoverPoint_OnUpdate();
	void		CheckRouteToCoverPoint_OnExit();
	FSM_Return	CoverSearchFailed_OnUpdate();

	// Helper functions
	void Reset(s32 iSearchResult, bool bResetState);
	CCoverPoint* SearchForCoverPoint(const Vector3& vThreatPos );
	bool EvaluateCoverPoint(Vector3 &vThreatPos );
	bool CheckForFallbackCoverPoints(s32 &iCoverPointFailReason);
	bool GetCoverPointPosition( Vector3 &vThreatPos,Vector3& vCoverCoors, Vector3* vVantagePos );
	void CoverRejected(Vector3& vCoverCoors, const char* szReason );
	bool IsDesiredCoverPointValid();
	bool ShouldSearchForCover();
	bool BuildAccessibilityCapsuleDesc(WorldProbe::CShapeTestCapsuleDesc &capsuleDesc, bool bIsSynchronous);

	// Returns the position of the threat, either the entity position or if thats null the target coords vector
	Vector3 GetThreatPosition() const;
	Vector3 GetThreatVantagePosition() const;

	Vector3			m_TargetPos;
	RegdConstPed	m_pTargetPed;

	fwFlags32		m_flags;

	// An array of previously rejected cover points, used to prevent the same point being checked twice
	CCoverPoint*					m_apFoundCoverPoints[COVER_POINT_HISTORY_SIZE];
	CCoverPoint*					m_apCoverPointsAlreadyChecked[COVER_POINT_HISTORY_SIZE];
	CCoverPoint*					m_pCoverPointBeingTested;
	CCover::CoverPointFilterConstructFn* m_pCoverPointFilterConstructFn;

	// We want to keep hold of the reason we failed for our backup cover points
	u8								m_aFailReasons[COVER_POINT_HISTORY_SIZE];

	// Route search helper
	CNavmeshRouteSearchHelper	m_RouteSearchHelper;

	// Asynchronous probe helper
	CAsyncProbeHelper			m_asyncProbeHelper;

	// A helper to find out the blocked status of a cover point
	CCoverPointStatusHelper		m_CoverStatusHelper;

	// Need the results structure for the cover object check (can't use the async probe helper)
	WorldProbe::CShapeTestSingleResult m_CoverAccessibilityResults;

	// The number of cover points we've gone through in a search
	s8							m_iNumCoverPoints;

	// The current cover point index we are testing
	s8							m_iCurrentCoverPointIndex;

	// The index used to check fall back cover points if using a defensive area and no cover points have LOS
	s8							m_iFallbackCoverPointIndex;

	// The ID of a scripted cover point
	s32							m_iScriptedCoverPointID;

	// The mode by which this task should locate cover points
	u32							m_iCoverSearchMode : 3;

	// The search type used to choose cover points
	s32							m_iCoverSearchType : 3;

	// The result of the last cover search
	s32							m_iLastSearchResult : 3;

	// Whether or not we are currently active in searching for cover
	bool						m_bIsActive : 1;

	// The position to begin searching around
	Vector3						m_vCoverStartSearchPos;

	// Distances to search for cover within
	float						m_fMinDistance;
	float						m_fMaxDistance;

	// The time we've been in the current state
	float						m_fTimeInState;

	// This is the ped that is using the cover finder
	RegdPed						m_pPed;
};

// Typedefs
typedef	fwRegdRef<class CCoverFinder> RegdCoverFinder;

class CCoverFinder : public fwRefAwareBase
{
public:
	FW_REGISTER_CLASS_POOL(CCoverFinder);

	CCoverFinder();
	~CCoverFinder() {}

	void Update();

	void SetupSearchParams(const CPed* pTargetPed, const Vector3& vTargetPos, const Vector3& vCoverSearchStartPos, s32 iFlags, float fMinDistance, 
		float fMaxDistance, const s32 iSearchType, const s32 iSearchMode, const s32 iScriptedCoverIndex, CCover::CoverPointFilterConstructFn* pFn);

	void StartSearch(CPed* pPed) { m_coverFinderFSM.StartSearch(pPed); } 
	bool IsActive() const { return m_coverFinderFSM.IsActive(); }
	s32 GetLastSearchResult(bool bResetSearchResult) { return m_coverFinderFSM.GetLastSearchResult(bResetSearchResult); }
	void ValidateCoverPoints() { m_coverFinderFSM.ValidateCoverPoints(); }

	enum
	{
		RF_ForceUpdateThisFrame = BIT0
	};

	inline void SetForceUpdateThisFrame() { m_iRunningFlags.SetFlag(RF_ForceUpdateThisFrame); }

#if !__FINAL
	virtual void	Debug(s32 iIndentLevel) const { m_coverFinderFSM.Debug(iIndentLevel); }
#endif // !__FINAL

	static void	Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

private:

	CExpensiveProcess	m_coverSearchDistributer;
	// Some helper running flags
	fwFlags8 m_iRunningFlags;
	CCoverFinderFSM		m_coverFinderFSM;
};

#endif // TASK_SEEK_COVER_H
