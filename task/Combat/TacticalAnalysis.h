// FILE :    TacticalAnalysis.h

#ifndef TACTICAL_ANALYSIS_H
#define TACTICAL_ANALYSIS_H

// Rage headers
#include "atl/array.h"
#include "fwtl/pool.h"
#include "fwutil/Flags.h"

// Game headers
#include "scene/RegdRefTypes.h"
#include "task/Combat/Cover/cover.h"
#include "task/System/TaskHelpers.h"
#include "task/System/Tuning.h"

// Game forward declarations
class CPhysical;
class CTacticalAnalysis;

// Typedefs
typedef	fwRegdRef<class CTacticalAnalysis> RegdTacticalAnalysis;

////////////////////////////////////////////////////////////////////////////////
// CTacticalAnalysisNavMeshPoint
////////////////////////////////////////////////////////////////////////////////

class CTacticalAnalysisNavMeshPoint
{

private:

	friend class CTacticalAnalysis;
	friend class CTacticalAnalysisNavMeshPoints;

private:

	enum Flags
	{
		F_IsValid				= BIT0,
		F_IsReservedForScript	= BIT1,
	};

	enum LineOfSightStatus
	{
		LOSS_Unchecked,
		LOSS_Clear,
		LOSS_Blocked,
	};

public:

	CTacticalAnalysisNavMeshPoint()
	: m_pNearby(NULL)
	, m_pReservedBy(NULL)
	, m_fPositionX(0.0f)
	, m_fPositionY(0.0f)
	, m_fPositionZ(0.0f)
	, m_nLineOfSightStatus(LOSS_Unchecked)
	, m_fTimeSinceLastAttemptToFindNewPosition(LARGE_FLOAT)
	, m_fTimeSinceLastLineOfSightTest(LARGE_FLOAT)
	, m_fTimeSinceLastAttemptToFindNearby(LARGE_FLOAT)
	, m_fTimeUntilRelease(0.0f)
	, m_fBadRouteValue(0.0f)
	, m_iHandle(-1)
	, m_uFlags(0)
	{}

public:

	float		GetBadRouteValue()	const { return m_fBadRouteValue; }
	int			GetHandle()			const { return m_iHandle; }
	Vec3V_Out	GetPosition()		const { return Vec3V(m_fPositionX, m_fPositionY, m_fPositionZ); }

public:

	void SetPosition(Vec3V_In vPosition)	{ m_fPositionX = vPosition.GetXf(); m_fPositionY = vPosition.GetYf(); m_fPositionZ = vPosition.GetZf(); }
	void SetTimeUntilRelease(float fTime)	{ m_fTimeUntilRelease = fTime; }

public:

	bool CanRelease(const CPed* pPed) const
	{
		if(!pPed)
		{
			return false;
		}
		else if(!IsValid())
		{
			return false;
		}
		else if(!IsReservedBy(pPed))
		{
			return false;
		}
		else
		{
			return true;
		}
	}

	bool CanReserve(const CPed* pPed) const
	{
		if(!pPed)
		{
			return false;
		}
		else if(!IsValid())
		{
			return false;
		}
		else if(IsReserved())
		{
			return false;
		}
		else
		{
			return true;
		}
	}

	void ClearTimeUntilRelease()
	{
		m_fTimeUntilRelease = 0.0f;
	}

	const CPed* GetNearby() const
	{
		return m_pNearby;
	}

	const CPed* GetReservedBy() const
	{
		return m_pReservedBy;
	}

	bool HasNearby() const
	{
		return (m_pNearby != NULL);
	}

	bool IsLineOfSightBlocked() const
	{
		return (m_nLineOfSightStatus == LOSS_Blocked);
	}

	bool IsLineOfSightClear() const
	{
		return (m_nLineOfSightStatus == LOSS_Clear);
	}

	bool IsNearby(const CPed* pPed) const
	{
		return (pPed && (pPed == m_pNearby));
	}

	bool IsReserved() const
	{
		return (m_pReservedBy != NULL);
	}

	bool IsReservedBy(const CPed* pPed) const
	{
		return (pPed && (pPed == m_pReservedBy));
	}

	bool IsReservedForScript() const
	{
		return (m_uFlags.IsFlagSet(F_IsReservedForScript));
	}

	bool IsValid() const
	{
		return (m_uFlags.IsFlagSet(F_IsValid));
	}

	void Release(const CPed* ASSERT_ONLY(pPed))
	{
		aiAssert(CanRelease(pPed));
		m_pReservedBy = NULL;
	}

	void Reserve(const CPed* pPed)
	{
		aiAssert(CanReserve(pPed));
		m_pReservedBy = pPed;
	}

	void ReserveForScript()
	{
		m_uFlags.SetFlag(F_IsReservedForScript);
	}

private:

	RegdConstPed		m_pNearby;
	RegdConstPed		m_pReservedBy;
	float				m_fPositionX;
	float				m_fPositionY;
	float				m_fPositionZ;
	LineOfSightStatus	m_nLineOfSightStatus;
	float				m_fTimeSinceLastAttemptToFindNewPosition;
	float				m_fTimeSinceLastLineOfSightTest;
	float				m_fTimeSinceLastAttemptToFindNearby;
	float				m_fTimeUntilRelease;
	float				m_fBadRouteValue;
	int					m_iHandle;
	fwFlags8			m_uFlags;

};

////////////////////////////////////////////////////////////////////////////////
// CTacticalAnalysisNavMeshPoints
////////////////////////////////////////////////////////////////////////////////

class CTacticalAnalysisNavMeshPoints
{

private:

	friend class CTacticalAnalysis;

public:

	struct ClosestPosition
	{
		ClosestPosition()
		: m_vPosition(V_ZERO)
		, m_iHandle(-1)
		{}

		CNavmeshClosestPositionHelper	m_Helper;
		Vec3V							m_vPosition;
		int								m_iHandle;
	};

	struct LineOfSightTest
	{
		LineOfSightTest()
		: m_AsyncProbeHelper()
		, m_iHandle(-1)
		{}

		CAsyncProbeHelper	m_AsyncProbeHelper;
		int					m_iHandle;
	};

	struct Tunables : CTuning
	{
		Tunables();

		float	m_MinDistance;
		float	m_MaxDistance;
		float	m_BufferDistance;
		float	m_MinTimeBetweenAttemptsToFindNewPosition;
		float	m_MinTimeBetweenLineOfSightTests;
		float	m_MinTimeBetweenAttemptsToFindNearby;
		float	m_MaxSearchRadiusForNavMesh;
		float	m_RadiusForFindNearby;
		float	m_MinDistanceBetweenPositionsWithClearLineOfSight;
		float	m_MinDistanceBetweenPositionsWithoutClearLineOfSightInExteriors;
		float	m_MinDistanceBetweenPositionsWithoutClearLineOfSightInInteriors;
		float	m_MaxXYDistanceForNewPosition;
		int		m_MaxNearbyToFindPerFrame;

		PAR_PARSABLE;
	};

public:

	CTacticalAnalysisNavMeshPoints();
	~CTacticalAnalysisNavMeshPoints();

public:

	const CTacticalAnalysisNavMeshPoint* GetPointForHandle(int iHandle) const
	{
		int iIndex = FindPointForHandle(iHandle);
		return (iIndex >= 0) ? &m_aPoints[iIndex] : NULL;
	}

	CTacticalAnalysisNavMeshPoint& GetPointForIndex(int iIndex)
	{
		return m_aPoints[iIndex];
	}

	const CTacticalAnalysisNavMeshPoint& GetPointForIndex(int iIndex) const
	{
		return m_aPoints[iIndex];
	}

	int GetNumPoints() const
	{
		return m_aPoints.GetMaxCount();
	}

public:

	bool ReleaseHandle(const CPed* pPed, int iHandle);
	bool ReleasePoint(const CPed* pPed, CTacticalAnalysisNavMeshPoint& rPoint);
	bool ReserveHandle(const CPed* pPed, int iHandle);
	bool ReservePoint(const CPed* pPed, CTacticalAnalysisNavMeshPoint& rPoint);

public:

	static const Tunables& GetTunables() { return sm_Tunables; }

public:

	static bool AddPointFromScript(Vec3V_In vPosition);
	static void ClearPointsFromScript();

private:

	void SetTacticalAnalysis(const CTacticalAnalysis* pTacticalAnalysis) { m_pTacticalAnalysis = pTacticalAnalysis; }

private:

	void		Activate();
	float		CalculateDistanceSqFromPed(Vec3V_In vPosition) const;
	float		CalculateOffsetForLineOfSightTests() const;
	void		Deactivate();
	void		DecayBadRouteValues(float fTimeStep);
	void		FindNearby();
	void		FindNearby(CTacticalAnalysisNavMeshPoint& rPoint);
	int			FindNextPointForFindNearby() const;
	int			FindNextPointForFindNewPosition() const;
	int			FindNextPointForLineOfSightTest() const;
	int			FindPointForHandle(int iHandle) const;
	Vec3V_Out	GenerateNewPosition() const;
	void		InitializeOffsetForLineOfSightTests();
	void		InitializePoints();
	bool		IsLineOfSightTestRunningForHandle(int iHandle) const;
	bool		IsNewPositionValid(Vec3V_In vPosition) const;
	bool		IsPositionValid(Vec3V_In vPosition) const;
	bool		IsXYDistanceGreaterThan(Vec3V_In vPositionA, Vec3V_In vPositionB, float fMinDistance) const;
	void		OnBadRoute(Vec3V_In vPosition, float fValue);
	void		ProcessResultsForLineOfSightTests();
	void		ResetInvalidPoints();
	void		ResetLineOfSightTests();
	void		ResetPoint(CTacticalAnalysisNavMeshPoint& rPoint);
	void		ResetPoints();
	void		SetPoint(CTacticalAnalysisNavMeshPoint& rPoint, Vec3V_In vPosition);
	void		StartNewLineOfSightTests();
	void		SetPointsFromScript();
	void		Update(float fTimeStep);
	void		UpdateInvalidPoints(float fTimeStep);
	void		UpdateLineOfSightTests();
	void		UpdatePoints(float fTimeStep);
	void		UpdatePositions();
	void		UpdateTimersForInvalidPoints(float fTimeStep);
	void		UpdateTimersForValidPoints(float fTimeStep);
	void		UpdateValidPoints(float fTimeStep);

private:

	static bool IsPointBetterForFindNearby(const CTacticalAnalysisNavMeshPoint& rPoint, const CTacticalAnalysisNavMeshPoint& rOtherPoint)
	{
		float fTime = rPoint.m_fTimeSinceLastAttemptToFindNearby;
		float fOtherTime = rOtherPoint.m_fTimeSinceLastAttemptToFindNearby;
		if(fTime != fOtherTime)
		{
			return (fTime > fOtherTime);
		}

		return (&rPoint < &rOtherPoint);
	}

	static bool IsPointBetterForFindNewPosition(const CTacticalAnalysisNavMeshPoint& rPoint, const CTacticalAnalysisNavMeshPoint& rOtherPoint)
	{
		float fTime = rPoint.m_fTimeSinceLastAttemptToFindNewPosition;
		float fOtherTime = rOtherPoint.m_fTimeSinceLastAttemptToFindNewPosition;
		if(fTime != fOtherTime)
		{
			return (fTime > fOtherTime);
		}

		return (&rPoint < &rOtherPoint);
	}

	static bool IsPointBetterForLineOfSightTest(const CTacticalAnalysisNavMeshPoint& rPoint, const CTacticalAnalysisNavMeshPoint& rOtherPoint)
	{
		float fTime = rPoint.m_fTimeSinceLastLineOfSightTest;
		float fOtherTime = rOtherPoint.m_fTimeSinceLastLineOfSightTest;
		if(fTime != fOtherTime)
		{
			return (fTime > fOtherTime);
		}

		return (&rPoint < &rOtherPoint);
	}

private:

	static void SyncPointsFromScript();

private:

	static const int sMaxPoints = 64;
	static const int sMaxPointsFromScript = 4;
	static const int sMaxLineOfSightTests = 3;

private:

	atRangeArray<CTacticalAnalysisNavMeshPoint, sMaxPoints>	m_aPoints;
	atRangeArray<LineOfSightTest, sMaxLineOfSightTests>		m_aLineOfSightTests;
	ClosestPosition											m_ClosestPosition;
	const CTacticalAnalysis*								m_pTacticalAnalysis;
	float													m_fOffsetForLineOfSightTests;

private:

	static atFixedArray<Vec3V, sMaxPointsFromScript> sm_aPointsFromScript;

private:

	static Tunables sm_Tunables;

};

////////////////////////////////////////////////////////////////////////////////
// CTacticalAnalysisCoverPointSearch
////////////////////////////////////////////////////////////////////////////////

class CTacticalAnalysisCoverPointSearch
{

private:

	friend class CTacticalAnalysis;

public:

	enum State
	{
		Idle,
		Find,
		CullInvalid,
		Score,
		CullExcessive,
		Finished,
	};

public:

	struct CoverPointWithScore
	{
		CoverPointWithScore()
		: m_pCoverPoint(NULL)
		, m_fScore(0.0f)
		{}

		CoverPointWithScore(const CoverPointWithScore& rOther)
		: m_pCoverPoint(rOther.m_pCoverPoint)
		, m_fScore(rOther.m_fScore)
		{}

		CCoverPoint*	m_pCoverPoint;
		float			m_fScore;
	};

	struct Options
	{
		Options()
		: m_vPosition(V_ZERO)
		, m_iMaxResults(0)
		, m_fMinDistance(0.0f)
		, m_fMaxDistance(0.0f)
		{}

		Options(Vec3V_In vPosition, int uMaxResults, float fMinDistance, float fMaxDistance)
		: m_vPosition(vPosition)
		, m_iMaxResults(uMaxResults)
		, m_fMinDistance(fMinDistance)
		, m_fMaxDistance(fMaxDistance)
		{}

		Vec3V	m_vPosition;
		int		m_iMaxResults;
		float	m_fMinDistance;
		float	m_fMaxDistance;
	};

	struct Tunables : CTuning
	{
		struct Scoring
		{
			Scoring()
			{}

			float	m_Occupied;
			float	m_Scripted;
			float	m_PointOnMap;
			float	m_MinDistanceToBeConsideredOptimal;
			float	m_MaxDistanceToBeConsideredOptimal;
			float	m_Optimal;

			PAR_SIMPLE_PARSABLE;
		};

		Tunables();

		Scoring	m_Scoring;
		int		m_ScoreCalculationsPerFrame;

		PAR_PARSABLE;
	};

public:

	CTacticalAnalysisCoverPointSearch();
	~CTacticalAnalysisCoverPointSearch();

public:

	bool CanStart() const
	{
		return (!IsActive());
	}

	Vec3V_Out GetPosition() const
	{
		return m_Options.m_vPosition;
	}

	bool IsActive() const
	{
		return (m_nState != Idle);
	}

	bool IsFinished() const
	{
		return (m_nState == Finished);
	}

public:

	float	CalculateScore(const CCoverPoint& rCoverPoint) const;
	void	DoCullExcessive();
	void	DoCullInvalid();
	void	DoFind();
	void	DoScore();
	bool	IsValid(CCoverPoint& rCoverPoint) const;
	void	Reset();
	void	Start(const Options& rOptions);
	void	Update(float fTimeStep);

private:

	void SetTacticalAnalysis(const CTacticalAnalysis* pTacticalAnalysis) { m_pTacticalAnalysis = pTacticalAnalysis; }

private:

	static int SortByScore(const void* pA, const void* pB)
	{
		const CoverPointWithScore* pCoverPointWithScoreA = (const CoverPointWithScore *)pA;
		const CoverPointWithScore* pCoverPointWithScoreB = (const CoverPointWithScore *)pB;

		float fScoreA = pCoverPointWithScoreA->m_fScore;
		float fScoreB = pCoverPointWithScoreB->m_fScore;
		if(fScoreA != fScoreB)
		{
			return (fScoreA > fScoreB) ? -1 : 1;
		}

		CCoverPoint* pCoverPointA = pCoverPointWithScoreA->m_pCoverPoint;
		CCoverPoint* pCoverPointB = pCoverPointWithScoreB->m_pCoverPoint;
		if(pCoverPointA != pCoverPointB)
		{
			return (pCoverPointA < pCoverPointB) ? -1 : 1;
		}

		return (pCoverPointWithScoreA < pCoverPointWithScoreB) ? -1 : 1;
	}

private:

	static const int sMaxCoverPoints = 768;

public:

	atFixedArray<CoverPointWithScore, sMaxCoverPoints>	m_aCoverPoints;
	Options												m_Options;
	const CTacticalAnalysis*							m_pTacticalAnalysis;
	State												m_nState;
	int													m_iIndexToScore;

private:

	static Tunables sm_Tunables;

};

////////////////////////////////////////////////////////////////////////////////
// CTacticalAnalysisCoverPoint
////////////////////////////////////////////////////////////////////////////////

class CTacticalAnalysisCoverPoint
{

private:

	friend class CTacticalAnalysis;
	friend class CTacticalAnalysisCoverPoints;

private:

	enum ArcStatus
	{
		AS_Unknown,
		AS_ProvidesCover,
		AS_DoesNotProvideCover,
	};

	enum LineOfSightStatus
	{
		LOSS_Unchecked,
		LOSS_Clear,
		LOSS_Blocked,
	};

public:

	CTacticalAnalysisCoverPoint()
	: m_pCoverPoint(NULL)
	, m_pNearby(NULL)
	, m_nLineOfSightStatus(LOSS_Unchecked)
	, m_nArcStatus(AS_ProvidesCover)
	, m_fTimeSinceLastLineOfSightTest(LARGE_FLOAT)
	, m_fTimeSinceLastAttemptToFindNearby(LARGE_FLOAT)
	, m_fTimeSinceLastStatusUpdate(LARGE_FLOAT)
	, m_fTimeUntilRelease(0.0f)
	, m_fBadRouteValue(0.0f)
	, m_uStatus(0)
	{}

public:

	float			GetBadRouteValue()	const { return m_fBadRouteValue; }
	CCoverPoint*	GetCoverPoint()		const { return m_pCoverPoint; }
	u8				GetStatus()			const { return m_uStatus; }

public:

	void SetStatus(u8 uStatus)				{ m_uStatus = uStatus; }
	void SetTimeUntilRelease(float fTime)	{ m_fTimeUntilRelease = fTime; }

public:

	bool CanRelease(const CPed* pPed) const
	{
		if(!pPed)
		{
			return false;
		}
		else if(!m_pCoverPoint)
		{
			return false;
		}
		else if(!IsReservedBy(pPed))
		{
			return false;
		}
		else
		{
			return true;
		}
	}

	bool CanReserve(const CPed* pPed) const
	{
		if(!pPed)
		{
			return false;
		}
		else if(!m_pCoverPoint)
		{
			return false;
		}
		else if(IsReserved())
		{
			return false;
		}
		else
		{
			return true;
		}
	}

	void ClearTimeUntilRelease()
	{
		m_fTimeUntilRelease = 0.0f;
	}

	bool DoesArcNotProvideCover() const
	{
		return (m_nArcStatus == AS_DoesNotProvideCover);
	}

	bool DoesArcProvideCover() const
	{
		return (m_nArcStatus == AS_ProvidesCover);
	}

	const CPed* GetNearby() const
	{
		return m_pNearby;
	}

	CPed* GetReservedBy() const
	{
		return m_pCoverPoint->GetOccupiedBy();
	}

	bool HasNearby() const
	{
		return (m_pNearby != NULL);
	}

	bool IsLineOfSightBlocked() const
	{
		return (m_nLineOfSightStatus == LOSS_Blocked);
	}

	bool IsLineOfSightClear() const
	{
		return (m_nLineOfSightStatus == LOSS_Clear);
	}

	bool IsNearby(const CPed* pPed) const
	{
		return (pPed && (pPed == m_pNearby));
	}

	bool IsReserved() const
	{
		if(!m_pCoverPoint->IsOccupied())
		{
			return false;
		}

		return true;
	}

	bool IsReservedBy(const CPed* pPed) const
	{
		if(!m_pCoverPoint->IsOccupiedBy(pPed))
		{
			return false;
		}

		return true;
	}

public:

	void Release(CPed* pPed);
	void Reserve(CPed* pPed);

private:

	CCoverPoint*		m_pCoverPoint;
	RegdConstPed		m_pNearby;
	LineOfSightStatus	m_nLineOfSightStatus;
	float				m_fTimeSinceLastLineOfSightTest;
	float				m_fTimeSinceLastAttemptToFindNearby;
	float				m_fTimeSinceLastStatusUpdate;
	float				m_fTimeUntilRelease;
	float				m_fBadRouteValue;
	ArcStatus			m_nArcStatus;
	u8					m_uStatus;

};

////////////////////////////////////////////////////////////////////////////////
// CTacticalAnalysisCoverPoints
////////////////////////////////////////////////////////////////////////////////

class CTacticalAnalysisCoverPoints
{

private:

	friend class CTacticalAnalysis;

public:

	struct LineOfSightTest
	{
		LineOfSightTest()
		: m_AsyncProbeHelper()
		, m_pCoverPoint(NULL)
		{}

		CAsyncProbeHelper	m_AsyncProbeHelper;
		CCoverPoint*		m_pCoverPoint;
	};

	struct Tunables : CTuning
	{
		Tunables();

		float	m_MinDistanceMovedToStartSearch;
		float	m_MaxTimeBetweenSearches;
		float	m_MinDistanceForSearch;
		float	m_MaxDistanceForSearch;
		float	m_MinTimeBetweenLineOfSightTests;
		float	m_MinTimeBetweenAttemptsToFindNearby;
		float	m_MinTimeBetweenStatusUpdates;
		float	m_RadiusForFindNearby;
		int		m_MaxNearbyToFindPerFrame;

		PAR_PARSABLE;
	};

public:

	CTacticalAnalysisCoverPoints();
	~CTacticalAnalysisCoverPoints();

public:

	CTacticalAnalysisCoverPoint* GetPointForCoverPoint(CCoverPoint* pCoverPoint)
	{
		int iIndex = FindPointForCoverPoint(pCoverPoint);
		return (iIndex >= 0) ? &m_aPoints[iIndex] : NULL;
	}

	const CTacticalAnalysisCoverPoint* GetPointForCoverPoint(CCoverPoint* pCoverPoint) const
	{
		int iIndex = FindPointForCoverPoint(pCoverPoint);
		return (iIndex >= 0) ? &m_aPoints[iIndex] : NULL;
	}

	CTacticalAnalysisCoverPoint& GetPointForIndex(int iIndex)
	{
		return m_aPoints[iIndex];
	}

	const CTacticalAnalysisCoverPoint& GetPointForIndex(int iIndex) const
	{
		return m_aPoints[iIndex];
	}

	int GetNumPoints() const
	{
		return m_aPoints.GetCount();
	}

public:

	bool ReleaseCoverPoint(CPed* pPed, CCoverPoint* pCoverPoint);
	bool ReleasePoint(CPed* pPed, CTacticalAnalysisCoverPoint& rPoint);
	bool ReserveCoverPoint(CPed* pPed, CCoverPoint* pCoverPoint);
	bool ReservePoint(CPed* pPed, CTacticalAnalysisCoverPoint& rPoint);

private:

	CTacticalAnalysisCoverPointSearch& GetSearch() { return m_Search; }

private:

	void SetTacticalAnalysis(const CTacticalAnalysis* pTacticalAnalysis) { m_pTacticalAnalysis = pTacticalAnalysis; }

private:

	void		Activate();
	bool		CanStartSearch() const;
	void		Deactivate();
	void		DecayBadRouteValues(float fTimeStep);
	void		FindNearby();
	void		FindNearby(CTacticalAnalysisCoverPoint& rPoint);
	int			FindNextPointForFindNearby() const;
	int			FindNextPointForLineOfSightTest() const;
	int			FindNextPointForStatusUpdate() const;
	int			FindPointForCoverPoint(CCoverPoint* pCoverPoint) const;
	void		InitializePoint(CCoverPoint* pCoverPoint, CTacticalAnalysisCoverPoint& rPoint);
	Vec3V_Out	GetSearchPosition() const;
	bool		IsLineOfSightTestRunningForCoverPoint(CCoverPoint* pCoverPoint) const;
	bool		IsPointValid(const CTacticalAnalysisCoverPoint& rPoint) const;
	bool		LoadPointForCoverPoint(CCoverPoint* pCoverPoint, CTacticalAnalysisCoverPoint& rPoint) const;
	void		MergeSearchResults();
	void		OnBadRoute(Vec3V_In vPosition, float fValue);
	void		OnSearchFinished();
	void		OnSearchStarted();
	void		ProcessResultsForLineOfSightTests();
	void		ReleasePoint(CTacticalAnalysisCoverPoint& rPoint);
	void		ReleasePoints();
	void		RemoveInvalidPoints();
	void		ResetLineOfSightTests();
	void		ResetPoints();
	bool		ShouldStartSearch() const;
	bool		ShouldStartSearchDueToDistance() const;
	bool		ShouldStartSearchDueToTime() const;
	void		StartNewLineOfSightTests();
	void		StartSearch();
	void		Update(float fTimeStep);
	void		UpdateArcs();
	void		UpdateLineOfSightTests();
	void		UpdatePoints(float fTimeStep);
	void		UpdateSearch(float fTimeStep);
	void		UpdateStatuses();
	void		UpdateTimers(float fTimeStep);
	void		UpdateTimersForPoints(float fTimeStep);

public:

	static const Tunables& GetTunables() { return sm_Tunables; }

private:

	static bool IsPointBetterForFindNearby(const CTacticalAnalysisCoverPoint& rPoint, const CTacticalAnalysisCoverPoint& rOtherPoint)
	{
		float fTime = rPoint.m_fTimeSinceLastAttemptToFindNearby;
		float fOtherTime = rOtherPoint.m_fTimeSinceLastAttemptToFindNearby;
		if(fTime != fOtherTime)
		{
			return (fTime > fOtherTime);
		}

		return (&rPoint < &rOtherPoint);
	}

	static bool IsPointBetterForLineOfSightTest(const CTacticalAnalysisCoverPoint& rPoint, const CTacticalAnalysisCoverPoint& rOtherPoint)
	{
		float fTime = rPoint.m_fTimeSinceLastLineOfSightTest;
		float fOtherTime = rOtherPoint.m_fTimeSinceLastLineOfSightTest;
		if(fTime != fOtherTime)
		{
			return (fTime > fOtherTime);
		}

		return (&rPoint < &rOtherPoint);
	}

	static bool IsPointBetterForStatusUpdate(const CTacticalAnalysisCoverPoint& rPoint, const CTacticalAnalysisCoverPoint& rOtherPoint)
	{
		float fTime = rPoint.m_fTimeSinceLastStatusUpdate;
		float fOtherTime = rOtherPoint.m_fTimeSinceLastStatusUpdate;
		if(fTime != fOtherTime)
		{
			return (fTime > fOtherTime);
		}

		return (&rPoint < &rOtherPoint);
	}

private:

	static const int sMaxPoints = 64;
	static const int sMaxLineOfSightTests = 3;

private:

	atFixedArray<CTacticalAnalysisCoverPoint, sMaxPoints>	m_aPoints;
	atRangeArray<LineOfSightTest, sMaxLineOfSightTests>		m_aLineOfSightTests;
	CCoverPointStatusHelper									m_StatusHelper;
	CTacticalAnalysisCoverPointSearch						m_Search;
	Vec3V													m_vLastPositionForSearch;
	const CTacticalAnalysis*								m_pTacticalAnalysis;
	float													m_fTimeSinceLastSearch;

private:

	static Tunables sm_Tunables;

};

////////////////////////////////////////////////////////////////////////////////
// CTacticalAnalysis
////////////////////////////////////////////////////////////////////////////////

class CTacticalAnalysis : public fwRefAwareBase
{

private:

	FW_REGISTER_CLASS_POOL(CTacticalAnalysis);

private:

	friend class CTacticalAnalysisNavMeshPoints;
	friend class CTacticalAnalysisCoverPointSearch;
	friend class CTacticalAnalysisCoverPoints;

private:

	enum RunningFlags
	{
		RF_IsActive	= BIT0,
	};

public:

	struct Tunables : CTuning
	{
		struct BadRoute
		{
			BadRoute()
			{}

			float m_ValueForUnableToFind;
			float m_ValueForTooCloseToTarget;
			float m_MaxDistanceForTaint;
			float m_DecayRate;

			PAR_SIMPLE_PARSABLE;
		};

		struct Rendering
		{
			Rendering()
			{}

			bool	m_Enabled;
			bool	m_CoverPoints;
			bool	m_NavMeshPoints;
			bool	m_Position;
			bool	m_LineOfSightStatus;
			bool	m_ArcStatus;
			bool	m_Reserved;
			bool	m_Nearby;
			bool	m_BadRouteValue;
			bool	m_Reservations;
			bool	m_LineOfSightTests;

			PAR_SIMPLE_PARSABLE;
		};

		Tunables();

		BadRoute	m_BadRoute;
		Rendering	m_Rendering;

		float		m_MaxSpeedToActivate;
		float		m_MinSpeedToDeactivate;
		float		m_MaxTimeWithNoReferences;
		bool		m_Enabled;

		PAR_PARSABLE;
	};

public:

	explicit CTacticalAnalysis(const CPed* pPed);
	~CTacticalAnalysis();

public:

			CTacticalAnalysisCoverPoints&	GetCoverPoints()			{ return m_CoverPoints; }
	const	CTacticalAnalysisCoverPoints&	GetCoverPoints()	const	{ return m_CoverPoints; }
			CTacticalAnalysisNavMeshPoints&	GetNavMeshPoints()			{ return m_NavMeshPoints; }
	const	CTacticalAnalysisNavMeshPoints&	GetNavMeshPoints()	const	{ return m_NavMeshPoints; }

public:

	void OnRouteTooCloseTooTarget(Vec3V_In vPosition);
	void OnUnableToFindRoute(Vec3V_In vPosition);
	void Release();

public:

	static const Tunables& GetTunables() { return sm_Tunables; }

public:

	static CTacticalAnalysis*	Find(const CPed& rPed);
	static void					Init(unsigned initMode);
	static bool					IsActive(const CPed& rPed);
	static CTacticalAnalysis*	Request(const CPed& rPed);
	static void					Shutdown(unsigned shutdownMode);
	static void					Update();

public:

#if __BANK
	static void Render();
#endif

private:

	const	CPed*	GetPed()					const { return m_pPed; }
			float	GetTimeSinceHadReferences()	const { return m_fTimeSinceHadReferences; }

private:

	bool HasReferences() const
	{
		return (m_uReferences > 0);
	}

	bool IsActive() const
	{
		return (m_uRunningFlags.IsFlagSet(RF_IsActive));
	}

private:

	void				Activate();
	float				CalculateSpeedForPed() const;
	bool				CalculateVantagePosition(CCoverPoint* pCoverPoint, Vec3V_InOut vVantagePosition) const;
	bool				CalculateVantagePositionForPed(Vec3V_InOut vVantagePosition) const;
	void				Deactivate();
	const CPed*			FindNearby(Vec3V_In vPosition, float fRadius) const;
	const CPhysical*	GetDominantEntity() const;
	Vec3V_Out			GetPedPosition() const;
	void				OnBadRoute(Vec3V_In vPosition, float fValue);
	void				Request();
	bool				ShouldActivate() const;
	bool				ShouldDeactivate() const;
	bool				StartLineOfSightTest(CAsyncProbeHelper& rHelper, Vec3V_In vPosition) const;
	void				Update(float fTimeStep);
	void				UpdateTimers(float fTimeStep);

private:

#if __BANK
	void RenderDebug();
#endif

private:

	static bool CanRequest(const CPed& rPed);

private:

	CTacticalAnalysisNavMeshPoints	m_NavMeshPoints;
	CTacticalAnalysisCoverPoints	m_CoverPoints;
	RegdConstPed					m_pPed;
	float							m_fTimeSinceHadReferences;
	u8								m_uReferences;
	fwFlags8						m_uRunningFlags;

private:

	static Tunables sm_Tunables;

};

#endif //TACTICAL_ANALYSIS_H
