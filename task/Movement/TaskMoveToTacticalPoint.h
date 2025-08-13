// FILE :    TaskMoveToTacticalPoint.h

#ifndef TASK_MOVE_TO_TACTICAL_POINT_H
#define TASK_MOVE_TO_TACTICAL_POINT_H

// Rage headers
#include "atl/array.h"
#include "fwutil/Flags.h"

// Game headers
#include "ai/AITarget.h"
#include "scene/RegdRefTypes.h"
#include "task/Movement/TaskNavBase.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

// Game forward declarations
class CCoverPoint;
class CEvent;
class CTacticalAnalysis;
class CTacticalAnalysisCoverPoint;
class CTacticalAnalysisNavMeshPoint;

//=========================================================================
// CTaskMoveToTacticalPoint
//=========================================================================

class CTaskMoveToTacticalPoint : public CTask
{

public:

	enum State
	{
		State_Start = 0,
		State_Resume,
		State_FindPointToMoveTo,
		State_UseCover,
		State_MoveToPosition,
		State_WaitAtPosition,
		State_Finish,
	};

	enum ConfigFlags
	{
		CF_DisableCoverPoints				= BIT0,
		CF_DisableNavMeshPoints				= BIT1,
		CF_DontQuitWhenPointIsReached		= BIT2,
		CF_CanMoveThroughSpheresOfInfluence	= BIT3,
		CF_DontCheckAttackWindow			= BIT4,
		CF_DontCheckDefensiveArea			= BIT5,
		CF_OnlyCheckInfluenceSpheresOnce	= BIT6,
		CF_RejectNegativeScores				= BIT7,
		CF_DisablePointsWithoutClearLos		= BIT8,
	};

private:

	enum RouteFailedReason
	{
		RFR_None,
		RFR_UnableToFind,
		RFR_TooCloseToTarget,
	};

	enum RunningFlags
	{
		RF_DisableCoverPoints				= BIT0,
		RF_HasSetTimeUntilReleaseForPoint	= BIT1,
		RF_HasCheckedInfluenceSpheres		= BIT2
	};

public:

	struct CanScoreCoverPointInput
	{
		CanScoreCoverPointInput()
		: m_pTask(NULL)
		, m_pPoint(NULL)
		{}

		const CTaskMoveToTacticalPoint*		m_pTask;
		const CTacticalAnalysisCoverPoint*	m_pPoint;
	};

	struct CanScoreNavMeshPointInput
	{
		CanScoreNavMeshPointInput()
		: m_pTask(NULL)
		, m_pPoint(NULL)
		{}

		const CTaskMoveToTacticalPoint*			m_pTask;
		const CTacticalAnalysisNavMeshPoint*	m_pPoint;
	};

	struct IsCoverPointValidToMoveToInput
	{
		IsCoverPointValidToMoveToInput()
		: m_pTask(NULL)
		, m_pPoint(NULL)
		{}

		const CTaskMoveToTacticalPoint*		m_pTask;
		const CTacticalAnalysisCoverPoint*	m_pPoint;
	};

	struct IsNavMeshPointValidToMoveToInput
	{
		IsNavMeshPointValidToMoveToInput()
		: m_pTask(NULL)
		, m_pPoint(NULL)
		{}

		const CTaskMoveToTacticalPoint*			m_pTask;
		const CTacticalAnalysisNavMeshPoint*	m_pPoint;
	};

	struct Point
	{
		Point()
		: m_pCoverPoint(NULL)
		, m_iHandleForNavMeshPoint(-1)
		{}

		CCoverPoint*	m_pCoverPoint;
		int				m_iHandleForNavMeshPoint;
	};

	struct ScoreCoverPointInput
	{
		ScoreCoverPointInput()
		: m_pTask(NULL)
		, m_pPoint(NULL)
		{}

		const CTaskMoveToTacticalPoint*		m_pTask;
		const CTacticalAnalysisCoverPoint*	m_pPoint;
	};

	struct ScoreNavMeshPointInput
	{
		ScoreNavMeshPointInput()
		: m_pTask(NULL)
		, m_pPoint(NULL)
		{}

		const CTaskMoveToTacticalPoint*			m_pTask;
		const CTacticalAnalysisNavMeshPoint*	m_pPoint;
	};

	struct ScorePositionInput
	{
		ScorePositionInput()
		: m_pTask(NULL)
		, m_vPosition(V_ZERO)
		{}

		const	CTaskMoveToTacticalPoint*	m_pTask;
				Vec3V						m_vPosition;
	};

	struct Tunables : CTuning
	{
		struct Scoring
		{
			struct CoverPoint
			{
				struct Bonus
				{
					Bonus()
					{}

					float	m_Current;

					PAR_SIMPLE_PARSABLE;
				};

				struct Penalty
				{
					struct BadRoute
					{
						BadRoute()
						{}

						float m_ValueForMin;
						float m_ValueForMax;
						float m_Min;
						float m_Max;

						PAR_SIMPLE_PARSABLE;
					};

					Penalty()
					{}

					BadRoute m_BadRoute;

					float	m_Arc;
					float	m_LineOfSight;
					float	m_Nearby;

					PAR_SIMPLE_PARSABLE;
				};

				CoverPoint()
				{}

				float	m_Base;
				Bonus	m_Bonus;
				Penalty	m_Penalty;

				PAR_SIMPLE_PARSABLE;
			};

			struct NavMeshPoint
			{
				struct Bonus
				{
					Bonus()
					{}

					float	m_Current;

					PAR_SIMPLE_PARSABLE;
				};

				struct Penalty
				{
					struct BadRoute
					{
						BadRoute()
						{}

						float m_ValueForMin;
						float m_ValueForMax;
						float m_Min;
						float m_Max;

						PAR_SIMPLE_PARSABLE;
					};

					Penalty()
					{}

					BadRoute m_BadRoute;

					float	m_LineOfSight;
					float	m_Nearby;

					PAR_SIMPLE_PARSABLE;
				};

				NavMeshPoint()
				{}

				float	m_Base;
				Bonus	m_Bonus;
				Penalty	m_Penalty;

				PAR_SIMPLE_PARSABLE;
			};

			struct Position
			{
				Position()
				{}

				float m_MaxDistanceFromPed;
				float m_ValueForMinDistanceFromPed;
				float m_ValueForMaxDistanceFromPed;
				float m_MaxDistanceFromOptimal;
				float m_ValueForMinDistanceFromOptimal;
				float m_ValueForMaxDistanceFromOptimal;

				PAR_SIMPLE_PARSABLE;
			};

			Scoring()
			{}

			CoverPoint		m_CoverPoint;
			NavMeshPoint	m_NavMeshPoint;
			Position		m_Position;

			PAR_SIMPLE_PARSABLE;
		};

		Tunables();

		Scoring	m_Scoring;
		float	m_TargetRadiusForMoveToPosition;
		float	m_TimeUntilRelease;
		float	m_MaxDistanceToConsiderCloseToPositionToMoveTo;
		float	m_TimeBetweenInfluenceSphereChecks;

		PAR_PARSABLE;
	};

public:

	typedef bool(*CanScoreCoverPointCallback)(const CanScoreCoverPointInput& rInput);
	typedef bool(*CanScoreNavMeshPointCallback)(const CanScoreNavMeshPointInput& rInput);
	typedef bool(*IsCoverPointValidToMoveToCallback)(const IsCoverPointValidToMoveToInput& rInput);
	typedef bool(*IsNavMeshPointValidToMoveToCallback)(const IsNavMeshPointValidToMoveToInput& rInput);
	typedef float(*ScoreCoverPointCallback)(const ScoreCoverPointInput& rInput);
	typedef float(*ScoreNavMeshPointCallback)(const ScoreNavMeshPointInput& rInput);

public:

	CTaskMoveToTacticalPoint(const CAITarget& rTarget, fwFlags16 uConfigFlags = 0);
	virtual ~CTaskMoveToTacticalPoint();

public:

			fwFlags16&	GetConfigFlags()		{ return m_uConfigFlags; }
	const	fwFlags16	GetConfigFlags() const	{ return m_uConfigFlags; }

public:

	void SetCanScoreCoverPointCallback(CanScoreCoverPointCallback pCallback)					{ m_pCanScoreCoverPointCallback = pCallback; }
	void SetCanScoreNavMeshPointCallback(CanScoreNavMeshPointCallback pCallback)				{ m_pCanScoreNavMeshPointCallback = pCallback; }
	void SetIsCoverPointValidToMoveToCallback(IsCoverPointValidToMoveToCallback pCallback)		{ m_pIsCoverPointValidToMoveToCallback = pCallback; }
	void SetIsNavMeshPointValidToMoveToCallback(IsNavMeshPointValidToMoveToCallback pCallback)	{ m_pIsNavMeshPointValidToMoveToCallback = pCallback; }
	void SetMaxPositionsToExclude(int iCount)													{ m_iMaxPositionsToExclude = iCount; }
	void SetScoreCoverPointCallback(ScoreCoverPointCallback pCallback)							{ m_pScoreCoverPointCallback = pCallback; }
	void SetScoreNavMeshPointCallback(ScoreNavMeshPointCallback pCallback)						{ m_pScoreNavMeshPointCallback = pCallback; }
	void SetSpheresOfInfluenceBuilder(CTaskNavBase::SpheresOfInfluenceBuilder* pBuilder)
	{
		m_pSpheresOfInfluenceBuilder = pBuilder;
		m_uRunningFlags.ClearFlag(RF_HasCheckedInfluenceSpheres); 
	}
	void SetTimeToWaitAtPosition(float fTime)													{ m_fTimeToWaitAtPosition = fTime; }
	void SetTimeBetweenPointUpdates(float fTime)												{ m_fTimeBetweenPointUpdates = fTime; }

public:

	void ClearSubTaskToUseDuringMovement();
	bool DoesPointWeAreMovingToHaveClearLineOfSight() const;
	bool IsAtPoint() const;
	bool IsMovingToPoint() const;
	void SetSubTaskToUseDuringMovement(CTask* pTask);
	const CPed* GetTargetPed() const;

public:

	static bool		DefaultCanScoreCoverPoint(const CanScoreCoverPointInput& rInput);
	static bool		DefaultCanScoreNavMeshPoint(const CanScoreNavMeshPointInput& rInput);
	static bool		DefaultIsCoverPointValidToMoveTo(const IsCoverPointValidToMoveToInput& rInput);
	static bool		DefaultIsNavMeshPointValidToMoveTo(const IsNavMeshPointValidToMoveToInput& rInput);
	static float	DefaultScoreCoverPoint(const ScoreCoverPointInput& rInput);
	static float	DefaultScoreNavMeshPoint(const ScoreNavMeshPointInput& rInput);
	static float	DefaultScorePosition(const ScorePositionInput& rInput);
	static bool		DefaultSpheresOfInfluenceBuilder(TInfluenceSphere* apSpheres, int &iNumSpheres, const CPed* pPed);

public:

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

private:

			bool				AbortNavMeshTask();
			void				ActivateTimeslicing();
			bool				CanNeverScoreCoverPoint(const CTacticalAnalysisCoverPoint& rPoint) const;
			bool				CanNeverScoreNavMeshPoint(const CTacticalAnalysisNavMeshPoint& rPoint) const;
			bool				CanScoreCoverPoint(const CTacticalAnalysisCoverPoint& rPoint) const;
			bool				CanScoreNavMeshPoint(const CTacticalAnalysisNavMeshPoint& rPoint) const;
			bool				CanUseCover() const;
			int					ChooseStateToMoveToPoint() const;
			int					ChooseStateToResumeTo(bool& bKeepSubTask) const;
			void				ClearTimeUntilReleaseForCoverPoint();
			void				ClearTimeUntilReleaseForNavMeshPoint();
			void				ClearTimeUntilReleaseForPoint();
			CTask*				CreateSubTaskToUseDuringMovement() const;
			bool				DisablePointsWithoutClearLos() const;
			void				ExcludePointToMoveTo();
			bool				FindPointToMoveTo(Point& rPositionToMoveTo) const;
			bool				GetPositionToMoveTo(Vec3V_InOut vPositionToMoveTo) const;
			bool				GetPositionToMoveToFromCoverPoint(Vec3V_InOut vPositionToMoveTo) const;
			bool				GetPositionToMoveToFromNavMeshPoint(Vec3V_InOut vPositionToMoveTo) const;
			CTacticalAnalysis*	GetTacticalAnalysis() const;
			bool				HasExcludedTooManyPositions() const;
			RouteFailedReason	HasRouteFailed();
			bool				IsCloseToPositionToMoveTo() const;
			bool				IsCoverPointNeverValidToMoveTo(const CTacticalAnalysisCoverPoint& rPoint) const;
			bool				IsCoverPointValidToMoveTo(const CTacticalAnalysisCoverPoint& rPoint) const;
			bool				IsFollowingNavMeshRoute() const;
			bool				IsMovingToCoverPoint() const;
			bool				IsMovingToCoverPoint(CCoverPoint* pCoverPoint) const;
			bool				IsMovingToInvalidCoverPoint() const;
			bool				IsMovingToInvalidNavMeshPoint() const;
			bool				IsMovingToInvalidPosition() const;
			bool				IsMovingToNavMeshPoint() const;
			bool				IsMovingToNavMeshPoint(int iHandle) const;
			bool				IsMovingToPoint(const Point& rPoint) const;
			bool				IsNavMeshPointNeverValidToMoveTo(const CTacticalAnalysisNavMeshPoint& rPoint) const;
			bool				IsNavMeshPointValidToMoveTo(const CTacticalAnalysisNavMeshPoint& rPoint) const;
			bool				IsPositionExcluded(Vec3V_In vPosition) const;
			void				KeepCoverPoint();
			void				OnRouteFailed(RouteFailedReason nReason);
			void				OnRouteTooCloseToTarget();
			void				OnUnableToFindRoute();
			void				ProcessCoverPoint();
			void				ReleaseCoverPoint();
			void				ReleaseNavMeshPoint();
			void				ReleasePoint();
			bool				ReserveCoverPoint(CCoverPoint* pCoverPoint, bool bForce = false);
			bool				ReserveNavMeshPoint(int iHandle);
			bool				ReservePoint(Point& rPoint);
			float				ScoreCoverPoint(const CTacticalAnalysisCoverPoint& rPoint) const;
			float				ScoreNavMeshPoint(const CTacticalAnalysisNavMeshPoint& rPoint) const;
			void				SetStateForFailedToMoveToPoint();
			void				SetStateToMoveToPoint();
			void				SetTimeUntilReleaseForCoverPoint();
			void				SetTimeUntilReleaseForNavMeshPoint();
			void				SetTimeUntilReleaseForPoint();
			bool				ShouldKeepCoverPoint() const;
			bool				ShouldNotUseLaddersOrClimbs() const;
			bool				ShouldQuitWhenPointIsReached() const;
			bool				ShouldReleasePointDueToCleanUp() const;
			bool				ShouldScoreCoverPoints() const;
			bool				ShouldScoreNavMeshPoints() const;
			bool				ShouldSetTimeUntilReleaseDueToAbortFromEvent(const CEvent& rEvent) const;
			bool				UpdatePointToMoveTo();

private:

			CTacticalAnalysisCoverPoint*	GetCoverPointToMoveTo();
	const	CTacticalAnalysisCoverPoint*	GetCoverPointToMoveTo() const;
			CTacticalAnalysisCoverPoint*	GetPointForCoverPoint(CCoverPoint* pCoverPoint);
	const	CTacticalAnalysisCoverPoint*	GetPointForCoverPoint(CCoverPoint* pCoverPoint) const;
			CTacticalAnalysisNavMeshPoint*	GetPointForNavMeshPoint(int iHandle);
	const	CTacticalAnalysisNavMeshPoint*	GetPointForNavMeshPoint(int iHandle) const;
			CTacticalAnalysisNavMeshPoint*	GetNavMeshPointToMoveTo();
	const	CTacticalAnalysisNavMeshPoint*	GetNavMeshPointToMoveTo() const;

private:

	virtual aiTask* Copy() const;
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_MOVE_TO_TACTICAL_POINT; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Resume; }

	virtual void		CleanUp();
	virtual void		DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	FSM_Return	Resume_OnUpdate();
	void		FindPointToMoveTo_OnEnter();
	FSM_Return	FindPointToMoveTo_OnUpdate();
	void		FindPointToMoveTo_OnExit();
	void		UseCover_OnEnter();
	FSM_Return	UseCover_OnUpdate();
	void		MoveToPosition_OnEnter();
	FSM_Return	MoveToPosition_OnUpdate();
	void		MoveToPosition_OnExit();
	void		WaitAtPosition_OnEnter();
	FSM_Return	WaitAtPosition_OnUpdate();

private:

	static const int sMaxPositionsToExclude = 12;
	
private:

	atFixedArray<Vec3V, sMaxPositionsToExclude>	m_aExcludedPositions;
	CAITarget									m_Target;
	Point										m_PointToMoveTo;
	RegdTask									m_pSubTaskToUseDuringMovement;
	CanScoreCoverPointCallback					m_pCanScoreCoverPointCallback;
	CanScoreNavMeshPointCallback				m_pCanScoreNavMeshPointCallback;
	ScoreCoverPointCallback						m_pScoreCoverPointCallback;
	ScoreNavMeshPointCallback					m_pScoreNavMeshPointCallback;
	IsCoverPointValidToMoveToCallback			m_pIsCoverPointValidToMoveToCallback;
	IsNavMeshPointValidToMoveToCallback			m_pIsNavMeshPointValidToMoveToCallback;
	CTaskNavBase::SpheresOfInfluenceBuilder*	m_pSpheresOfInfluenceBuilder;
	float										m_fTimeSinceLastPointUpdate;
	float										m_fTimeToWaitAtPosition;
	float										m_fTimeBetweenPointUpdates;
	float										m_fTimeToWaitBeforeFindingPointToMoveTo;
	float										m_fInfluenceSphereCheckTimer;
	int											m_iMaxPositionsToExclude;
	fwFlags16									m_uConfigFlags;
	fwFlags8									m_uRunningFlags;

private:

	static Tunables sm_Tunables;

};

#endif // TASK_MOVE_TO_TACTICAL_POINT_H
