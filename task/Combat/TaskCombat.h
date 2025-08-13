// FILE :    TaskCombat.h
// PURPOSE : Tasks associated with combat
// AUTHOR :  Phil H
// CREATED : 18-08-2005

// INFO :
//	Tasks:
//		TASK_COMBAT_FLANK
//		TASK_COMBAT_CHARGE
//		TASK_COMBAT_SEEK_COVER
//		TASK_COMBAT_CLOSEST_TARGET_IN_AREA
//		TASK_COMBAT_ADDITIONAL
//		TASK_SEPARATE
//		TASK_HELICOPTER_STRAFE
//		TASK_SET_AND_GUARD_AREA
//		TASK_STAND_GUARD

#ifndef INC_TASK_COMBAT_H
#define INC_TASK_COMBAT_H

// Game headers
#include "audio/conductorsutil.h"
#include "Peds/DefensiveArea.h"
#include "Peds/QueriableInterface.h"
#include "task/Combat/TacticalAnalysis.h"
#include "task/Movement/TaskMoveToTacticalPoint.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/Tuning.h"

class CEntity;
class CVehMission;
class CTaskSet;
class CWeightedTaskSet;
class CCoverPoint;

//-------------------------------------------------------------------------
// Advancing combat subtask, handles peds advancing in on an enemy
//-------------------------------------------------------------------------

class CTaskCombatFlank : public CTask
{
public:

	// Constructor
	CTaskCombatFlank(const CPed* pPrimaryTarget);
	CTaskCombatFlank();

	// Destructor
	~CTaskCombatFlank();

	// Task required functions
	virtual aiTask*				Copy() const { return rage_new CTaskCombatFlank(m_pPrimaryTarget); }
	virtual int					GetTaskTypeInternal() const { return CTaskTypes::TASK_COMBAT_FLANK; }

	// FSM required functions
	virtual FSM_Return			UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32					GetDefaultStateAfterAbort()	const { return State_Finish; }

	// FSM optional functions
	virtual FSM_Return			ProcessPreFSM();

	// Member Functions
	const float					GetAttackWindowMaxSq() const { return m_fAttackWindowMaxSq; }

	// debug:
#if !__FINAL
	friend class CTaskClassInfoManager;
	virtual void		Debug() const;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	// FSM States
	enum
	{
		State_MoveToFlankPosition,
		State_Finish
	};

	struct Tunables : public CTuning
	{
		Tunables();

		float	m_fInfluenceSphereInnerWeight;
		float	m_fInfluenceSphereOuterWeight;
		float	m_fInfluenceSphereRequestRadius;
		float	m_fInfluenceSphereCheckRouteRadius;
		float	m_fSmallInfluenceSphereRadius;
		float	m_fDistanceBetweenInfluenceSpheres;
		float	m_fAbsoluteMinDistanceToTarget;
		float	m_fCoverPointScoreMultiplier;

		PAR_PARSABLE;
	};

	static Tunables	sm_Tunables;

	static float	ScorePosition(const CTaskMoveToTacticalPoint* pTask, Vec3V_In vPosition);
	static bool		CanScoreCoverPoint(const CTaskMoveToTacticalPoint::CanScoreCoverPointInput& rInput);
	static bool		CanScoreNavMeshPoint(const CTaskMoveToTacticalPoint::CanScoreNavMeshPointInput& rInput);
	static float	ScoreCoverPoint(const CTaskMoveToTacticalPoint::ScoreCoverPointInput& rInput);
	static float	ScoreNavMeshPoint(const CTaskMoveToTacticalPoint::ScoreNavMeshPointInput& rInput);
	static bool		SpheresOfInfluenceBuilder( TInfluenceSphere* apSpheres, int &iNumSpheres, const CPed* pPed );

private:

	// State Functions
	void						MoveToFlankPosition_OnEnter();
	FSM_Return					MoveToFlankPosition_OnUpdate();

	// Member functions

	// Build our influence spheres
	bool						BuildInfluenceSpheres( TInfluenceSphere* apSpheres, int &iNumSpheres, bool bUseRouteCheckRadius ) const;

private:

	RegdConstPed	m_pPrimaryTarget;
	float			m_fAttackWindowMaxSq;
};


//-------------------------------------------------------------------------
// Seek cover subtask, the ped locates the nearest cover point and if 
// reachable, heads toward it
//-------------------------------------------------------------------------
class CTaskCombatSeekCover : public CTask
{
public:

	// Constructor
	CTaskCombatSeekCover( const CPed* pPrimaryTarget, const Vector3& vSearchStartPoint, const bool bConsiderBackwardsPoints, const bool bAbortIfDistanceToCoverTooFar = false );
	
	// Destructor
	~CTaskCombatSeekCover();

	// Task required functions
	virtual aiTask* Copy()		const { return rage_new CTaskCombatSeekCover( m_pPrimaryTarget, m_vCoverSearchStartPos, m_bConsiderBackwardPoints, m_bAbortIfDistanceTooFar ); }
	virtual int					GetTaskTypeInternal() const { return CTaskTypes::TASK_COMBAT_SEEK_COVER; }

	// FSM required functions
	virtual FSM_Return			UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32					GetDefaultStateAfterAbort()	const { return State_Finish; }

	// FSM optional functions
	virtual FSM_Return			ProcessPreFSM();

	// Interface functions
	void						UpdateTarget(CPed* pNewTarget) { m_pPrimaryTarget = pNewTarget; } // Changes the current target to the one passed
	const Vector3&				GetCoverSearchStartPos() const { return m_vCoverSearchStartPos; }

	// debug:
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL	

private:

	// FSM states
	enum 
	{ 
		State_Start = 0, 
		State_SeekingCover,
		State_Separating,
		State_Finish
	};

	FSM_Return					Start_OnUpdate();

	void						SeekingCover_OnEnter();
	FSM_Return					SeekingCover_OnUpdate();

	void						Separating_OnEnter();
	FSM_Return					Separating_OnUpdate();

	bool						CheckForFinish();

private:

	static bank_float ms_fDistToFireImmediately;
	static bank_s32 ms_iMaxSeparateFrequency;

	RegdConstPed	 m_pPrimaryTarget;

	bool			 m_bConsiderBackwardPoints;
	bool			 m_bAbortIfDistanceTooFar;
	bool			 m_bPlayerInitiallyClose;

	u32				 m_iNextTimeMaySeparate;

	Vector3			 m_vCoverSearchStartPos;
	Vector3			 m_vSeparationCentre;
};


//-------------------------------------------------------------------------
// Charge subtask, causes the ped to run straight towards it's target and 
// begin hand-to-hand combat
//-------------------------------------------------------------------------
class CTaskCombatChargeSubtask : public CTask
{
public:

	// Constructor
	CTaskCombatChargeSubtask(CPed* pPrimaryTarget);

	// Destructor
	~CTaskCombatChargeSubtask();
	
	// Task required functions
    virtual aiTask* Copy()		const { return rage_new CTaskCombatChargeSubtask(m_pPrimaryTarget); }
    virtual s32 GetTaskTypeInternal()	const {return CTaskTypes::TASK_COMBAT_CHARGE;}

	// FSM required functions
	virtual FSM_Return			UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32				GetDefaultStateAfterAbort()	const { return State_Finish; }

	// FSM optional functions
	virtual FSM_Return			ProcessPreFSM();

	// Interface functions
	void						UpdateTarget(CPed* pNewTarget); // Changes the current target to the one passed

	// debug:
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL	
    
private:

	// FSM states
	enum 
	{ 
		State_Start = 0, 
		State_Melee,
		State_Finish
	};

	FSM_Return					Start_OnUpdate();

	void						Melee_OnEnter();
	FSM_Return					Melee_OnUpdate();

private:

	RegdPed						m_pPrimaryTarget;
};


//-------------------------------------------------------------------------
// Default tasks for cops driving choppers
//-------------------------------------------------------------------------
class CHeli;
class CTaskVehicleGoToHelicopter;

class CTaskHelicopterStrafe : public CTask
{
	enum eState
	{
		State_Start = 0,
		State_HelicopterStrafe
	};
	
	enum RunningFlags
	{
		RF_CanSeeTarget		= BIT0,
		RF_IsBehindTarget	= BIT1,
		RF_IsColliding		= BIT2,
	};
	
public:

	struct Tunables : public CTuning
	{
		Tunables();

		int		m_FlightHeightAboveTarget;
		int		m_MinHeightAboveTerrain;
		float	m_TargetDirectionMinDot;
		float	m_TargetOffset;
		float	m_TargetMinSpeedToIgnore;
		float   m_TargetMaxSpeedToStrafe;
		float	m_TimeToAvoidTargetAfterDamaged;
		float	m_AvoidOffsetXY;
		float	m_AvoidOffsetZ;

		float	m_MinDotToBeConsideredInFront;
		float	m_BehindRotateAngleLookAhead;
		float	m_SearchRotateAngleLookAhead;
		float	m_CircleRotateAngleLookAhead;
		float	m_BehindTargetAngle;
		float	m_TargetOffsetFilter;
		
		float	m_MinTimeBetweenStrafeDirectionChanges;

		PAR_PARSABLE;
	};

	CTaskHelicopterStrafe( const CEntity* pTarget );
	~CTaskHelicopterStrafe();

	virtual aiTask*	Copy() const {return rage_new CTaskHelicopterStrafe( m_pTarget );}

	int				GetTaskTypeInternal(void) const { return CTaskTypes::TASK_HELICOPTER_STRAFE; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	s32			GetDefaultStateAfterAbort() const { return State_Start; }
	FSM_Return		ProcessPreFSM();
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);

	static Tunables GetTunables() { return sm_Tunables; }
private:
	FSM_Return		Initial_OnUpdate(CPed* pPed);

	FSM_Return		Flying_OnEnter(CPed* pPed);
	FSM_Return		Flying_OnUpdate(CPed* pPed);
	
private:

	void	UpdateTargetPosAndSpeed(CTaskVehicleGoToHelicopter* pTask, const CHeli* pHeli);
	void	UpdateOrientation(CTaskVehicleGoToHelicopter* pTask, const CHeli* pHeli);
	void	UpdateTargetCollisionProbe(const CHeli* pHeli, const Vector3& in_Position);

	bool	CanSeeTarget() const;
	float	GetCruiseSpeed() const;
	CHeli*	GetHeli();
	bool	IsBehindTarget() const;
	void	ProcessAvoidance();
	void	ProcessAvoidanceOffset();
	void	ProcessStrafeDirection();
	
private:

	WorldProbe::CShapeTestSingleResult m_targetLocationTestResults;

	CTaskTimer		m_TimerCirclingSlow;
	CTaskTimer		m_TimerCirclingFast;


	RegdConstEnt	m_pTarget;
	Vector3			m_vTargetOffset;
	Vector3			m_vTargetVehicleVelocityXY;
	Vector3			m_vAvoidOffset;
	float			m_fStrafeDirection;
	float			m_fTimeSinceLastStrafeDirectionChange;
	fwFlags8		m_uRunningFlags;
	
private:

	static Tunables	sm_Tunables;
	
};


enum eGuardMode
{
	GM_Stand,
	GM_PatrolProximity,
	GM_PatrolDefensiveArea
};

//-------------------------------------------------------------------------
// Ped defends its area
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
// Task info for setting a peds defensive area
//-------------------------------------------------------------------------
class CClonedSetAndGuardAreaInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedSetAndGuardAreaInfo(const Vector3& vDefendPosition, float fHeading, float fProximity, eGuardMode guardMode, u32 endTime, const CDefensiveArea& area );
	CClonedSetAndGuardAreaInfo();
	~CClonedSetAndGuardAreaInfo() {}

	virtual s32 GetTaskInfoType() const {return INFO_TYPE_SET_AND_GUARD_AREA;}
	virtual int GetScriptedTaskType() const { return SCRIPT_TASK_GUARD_SPHERE_DEFENSIVE_AREA; }

	virtual CTask *CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity));

	void Serialise(CSyncDataBase& serialiser)
	{
		const float MAX_AREA_RADIUS	= 100.0f;

		CSerialisedFSMTaskInfo::Serialise(serialiser);

		u32 heading		= (u32)m_heading;
		u32 proximity	= (u32)m_proximity;
		u32 guardMode	= (u32)m_guardMode;

		SERIALISE_POSITION(serialiser, m_vDefendPosition, "Defend Position");
		SERIALISE_POSITION(serialiser, m_vAreaPosition, "Area Position");
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_areaRadius, MAX_AREA_RADIUS, SIZEOF_AREA_RADIUS, "Area Radius");
		SERIALISE_UNSIGNED(serialiser, heading, SIZEOF_HEADING, "Heading");
		SERIALISE_UNSIGNED(serialiser, proximity, SIZEOF_PROXIMITY, "Proximity");
		SERIALISE_UNSIGNED(serialiser, guardMode, SIZEOF_GUARD_MODE, "Guard Mode");
		SERIALISE_UNSIGNED(serialiser, m_endTime, SIZEOF_TIMER, "End Time");

		m_heading		= (u8) heading;
		m_proximity		= (u8) proximity;
		m_guardMode		= (u8) guardMode;
	}

private:

	CClonedSetAndGuardAreaInfo(const CClonedSetAndGuardAreaInfo &);
	CClonedSetAndGuardAreaInfo &operator=(const CClonedSetAndGuardAreaInfo &);

	static const unsigned int SIZEOF_AREA_RADIUS	= 10;
	static const unsigned int SIZEOF_HEADING		= 8;
	static const unsigned int SIZEOF_PROXIMITY		= 8;
	static const unsigned int SIZEOF_GUARD_MODE		= 2;
	static const unsigned int SIZEOF_TIMER			= 32;

	Vector3					m_vDefendPosition;
	Vector3					m_vAreaPosition;
	float					m_areaRadius;
	u32						m_endTime;
	u8						m_heading;
	u8						m_proximity;
	u8						m_guardMode;
};

class CTaskSetAndGuardArea : public CTask
{
public:

	// Constructor
	CTaskSetAndGuardArea( const Vector3& vDefendPosition, float fHeading, float fProximity, eGuardMode guardMode, float fTimer, const CDefensiveArea& area, bool bTakePositionFromPed = false, bool bSetDefensiveArea = true );
	
	// Destructor
	~CTaskSetAndGuardArea();

	// Task required functions
	virtual aiTask* Copy() const;
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_SET_AND_GUARD_AREA; } 

	// FSM required functions
	virtual FSM_Return			UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32				GetDefaultStateAfterAbort()	const { return State_Finish; }

	// debug:
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL	

	virtual CTaskInfo*	CreateQueriableState() const;

private:

	enum 
	{
		State_Start = 0,
		State_GuardingArea,
		State_Finish
	};

	FSM_Return		Start_OnUpdate();

	void			GuardingArea_OnEnter();
	FSM_Return		GuardingArea_OnUpdate();

	Vector3			m_vDefendPosition;
	CDefensiveArea	m_defensiveArea;
	float			m_fHeading;
	float			m_fProximity;
	eGuardMode		m_eGuardMode;
	float			m_fTimer;
	u32				m_nEndTime;
	bool			m_bTakePosFromPed;
	bool			m_bSetDefensiveArea;
};

//-------------------------------------------------------------------------
// Ped defends a position
//-------------------------------------------------------------------------
class CClonedStandGuardInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedStandGuardInfo(const Vector3& position, float heading, float proximity, eGuardMode guardMode, u32 endTime);
	CClonedStandGuardInfo();
	~CClonedStandGuardInfo() {}

	virtual s32 GetTaskInfoType() const {return INFO_TYPE_STAND_GUARD;}
	virtual int GetScriptedTaskType() const { return SCRIPT_TASK_STAND_GUARD; }

	virtual CTask *CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity));

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		u32 heading		= (u32)m_heading;
		u32 proximity	= (u32)m_proximity;
		u32 guardMode	= (u32)m_guardMode;

		SERIALISE_POSITION(serialiser, m_position, "Defend Position");
		SERIALISE_UNSIGNED(serialiser, heading, SIZEOF_HEADING, "Heading");
		SERIALISE_UNSIGNED(serialiser, proximity, SIZEOF_PROXIMITY, "Proximity");
		SERIALISE_UNSIGNED(serialiser, guardMode, SIZEOF_GUARD_MODE, "Guard Mode");
		SERIALISE_UNSIGNED(serialiser, m_endTime, SIZEOF_TIMER, "End Time");

		m_heading		= (u8) heading;
		m_proximity		= (u8) proximity;
		m_guardMode		= (u8) guardMode;
	}

private:

	CClonedStandGuardInfo(const CClonedStandGuardInfo &);
	CClonedStandGuardInfo &operator=(const CClonedStandGuardInfo &);

	static const unsigned int SIZEOF_HEADING		= 8;
	static const unsigned int SIZEOF_PROXIMITY		= 8;
	static const unsigned int SIZEOF_GUARD_MODE		= 2;
	static const unsigned int SIZEOF_TIMER			= 32;

	Vector3 m_position;
	u32		m_endTime;
	u8		m_heading;
	u8		m_proximity;
	u8		m_guardMode;
};

class CTaskStandGuard : public CTask
{
public:

	struct Tunables : public CTuning
	{
		Tunables();

		// How long to wait at a patrol point before moving on.
		s32			m_MinStandWaitTimeMS;
		s32			m_MaxStandWaitTimeMS;

		// How long to wait at the base guard point before going on patrol.
		s32			m_MinDefendPointWaitTimeMS;
		s32			m_MaxDefendPointWaitTimeMS;

		// When cover can't be found, this is how far away the ped should look from their initial position to find a navmesh point.
		float		m_MinNavmeshPatrolRadiusFactor;
		float		m_MaxNavmeshPatrolRadiusFactor;

		// Factor for checking the route length of a patrol path compared to the proximity.
		float		m_RouteRadiusFactor;

		PAR_PARSABLE;
	};

	// Constructor
	CTaskStandGuard( const Vector3& v1, float fHeading, float fProximity, eGuardMode guardMode, float fTimer );

	// Destructor
	~CTaskStandGuard();

	// Task required functions
	virtual aiTask*				Copy() const;
	virtual int					GetTaskTypeInternal() const { return CTaskTypes::TASK_STAND_GUARD; } 

	// FSM required functions
	virtual FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32					GetDefaultStateAfterAbort()	const { return State_Finish; }

	// FSM optional functions
	virtual FSM_Return			ProcessPreFSM();
	virtual void				CleanUp();

	// debug:
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL	

	virtual CTaskInfo*	CreateQueriableState() const;

private:

	enum 
	{
		State_Start = 0,
		State_GoToDefendPosition,
		State_GoToStandPosition,
		State_MoveAchieveHeading,
		State_MoveStandStill,
		State_Finish
	};

	FSM_Return		Start_OnUpdate();

	void			GoToDefendPosition_OnEnter();
	FSM_Return		GoToDefendPosition_OnUpdate();

	void			GoToStandPosition_OnEnter();
	FSM_Return		GoToStandPosition_OnUpdate();

	void			MoveAchieveHeading_OnEnter();
	FSM_Return		MoveAchieveHeading_OnUpdate();

	void			MoveStandStill_OnEnter();
	FSM_Return		MoveStandStill_OnUpdate();

	enum eCoverSearchType
	{
		CS_ANY = 0,
		CS_PREFER_PROVIDE_COVER,
		CS_MUST_PROVIDE_COVER
	};

	bool	IsAtPosition(Vector3 &vPos, float fProximity, CPed *pPed);
	aiTask*	CreateSubTask(const int iSubTaskType, CPed *pPed);
	bool	SetupNewStandingPoint(CPed *pPed);
	bool	SetupReturnToDefendPoint(CPed *pPed);

	Vector3 m_vDefendPosition;
	Vector3 m_vThreatDir;
	Vector3 m_vStandPoint;
	Vector3 m_vLastStandPoint;
	
	float	m_fHeading;
	float	m_fStandHeading;
	float	m_fProximity;
	float	m_fMinProximity;
	float	m_fReturnToDefendPointChance;
	float	m_fTaskTimer;
	float	m_fMoveBlendRatio;

	u32	m_nEndTime;
	s32	m_nStandingPointTime;
	s32	m_nCoverSearchMode:16;
	s32	m_nMoveAttempts:16;

	bool		m_bIsCrouching:1;
	eGuardMode	m_eGuardMode:3;
	bool		m_bIsHeadingToDefendPoint:1;

private:

	// Tuanbles.
	static Tunables	sm_Tunables;
};


//-------------------------------------------------------------------------
// Task to try and separate this ped from others while in combat
// o Works out the central bunch position
// o Requests a 3d path away from that position
// o Steps along the path at set intervals trying to find a position that has LOS with the target.
// o If it finds a position moves the ped to that pos
//-------------------------------------------------------------------------
class CTaskSeparate : public CTask
{
public:
	friend class CTaskCounter;

	static float MIN_SEPARATION_DISTANCE; // Distance if a single ped is within to try and separate
	static float MAX_SEPARATION_DISTANCE; // Distance to consider peds to find the avg center of concentration
	static float LOS_CHECK_INTERVAL_DISTANCE; // Interval between each LOS check on the returned route
	static bool ms_bPreserveSlopeInfoInPath; // Whether to add waypoints into path whenever slope changes significantly

	// Constructor
	CTaskSeparate( const Vector3* pvSeparationCentre, const Vector3& vTargetPos, const CPed* pTargetPed, aiTask* pAdditionalSubTask = NULL, bool bStrafing = false );
	
	// Destructor
	~CTaskSeparate();

	// Task required functions
	virtual aiTask* Copy() const;
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_SEPARATE; } 

	// FSM required functions
	virtual FSM_Return			UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32				GetDefaultStateAfterAbort()	const { return State_Finish; }

	// FSM optional functions
	virtual FSM_Return			ProcessPreFSM();
	virtual void				CleanUp();

	// Interface functions
	
	// Returns true if the ped needs to separate away from nearby peds, pvCenter is the centre of the concentration of peds
	static bool NeedsToSeparate( CPed* pPed, const CPed* pTargetPed, const Vector3& vPedPosition, float fSeparationDist, Vector3* pvCentre, s32* piNumberOffendingPeds = NULL, bool bIgnoreTarget = false);

	// debug:
#if !__FINAL
	virtual void				Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL	

private:

	enum 
	{
		State_Start = 0,
		State_Separating,
		State_Finish
	};

	FSM_Return		Start_OnUpdate();
	FSM_Return		Separating_OnUpdate();

private:

	// Returns the interval along the stored route
	bool GetIntervalAlongRoute( int iInterval, CPed* pPed, Vector3& vOutInteval );
	// Checks the next interval along the route to see if its valid
	bool CheckNextIntevalPosition( Vector3& vOutInteval, CPed* pPed );
	// Creates a direction to flee in and make a path request
	void CreateFleePathRequest( CPed* pPed, bool bRandom );

	enum {MAX_POINTS_TO_STORE_ALONG_PATH =5};
	enum {NUM_LOS_CHECKS=8};
	Vector3						m_vSeparationCentre;
	Vector3						m_avPathPoints[MAX_POINTS_TO_STORE_ALONG_PATH];
	Vector3						m_vTargetPos;
#if __DEBUG
	Vector3						m_vInitialCentre;
	Vector3						m_vRecalculatedCentre;
#endif
	s32						m_iPathPoints;
	s32						m_iLastCheckedInterval;
	// Route search helper
	CNavmeshRouteSearchHelper	m_RouteSearchHelper;
	CAsyncProbeHelper			m_AsyncProbeHelper;

	RegdConstPed				m_pTargetPed;
	RegdaiTask m_pAdditionalSubTask;
	s32						m_iNumberOffendingPeds;
	bool						m_bHasSeparationCentre:1;
	bool						m_bStrafe:1;
	bool						m_bTriedRandomSeparationDirection:1;
	bool						m_bSeparateBlind:1;


};

//-------------------------------------------------------------------------
// Ped fights anyone it hates nearby
//-------------------------------------------------------------------------
class CClonedCombatClosestTargetInAreaInfo;

class CTaskCombatClosestTargetInArea : public CTask
{
public:

	static const float INVALID_OVERALL_TIME;

public:

    friend class CClonedCombatClosestTargetInAreaInfo;

	// Constructor
	CTaskCombatClosestTargetInArea( const Vector3& vPos, const float fArea, const bool bGetPosFromPed, float fOverallTime = INVALID_OVERALL_TIME, fwFlags32 uCombatConfigFlags = 0 );

	// Destructor
	~CTaskCombatClosestTargetInArea();

	// Task required functions
	virtual aiTask* Copy() const { return rage_new CTaskCombatClosestTargetInArea(m_vPos, m_fArea, m_bGetPosFromPed, m_fCombatTimer, m_uCombatConfigFlags); }
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_COMBAT_CLOSEST_TARGET_IN_AREA;}

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// FSM required functions
	virtual FSM_Return			UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32				GetDefaultStateAfterAbort()	const { return GetState(); }
	virtual CTaskInfo*          CreateQueriableState() const;

	void SetInitialTarget(CPed* pTarget) { m_pInitialTarget = pTarget; }

private:

	enum 
	{
		State_Start = 0,
		State_WaitForTarget,
		State_Combat,
		State_Finish
	};

	FSM_Return		Start_OnUpdate();

	void			WaitForTarget_OnEnter();
	FSM_Return		WaitForTarget_OnUpdate();

	void			Combat_OnEnter();
	FSM_Return		Combat_OnUpdate();

	const CPed*		FindClosestTarget(CPed* pPed);
	const CPed*		SearchForNewTargets(CPed* pPed);
	bool			CanAttackPed(const CPed* pPed, const CPed* pTargetPed);

private:
	Vector3			 m_vPos;
	float			 m_fCombatTimer;
	float			 m_fArea;
	fwFlags32		 m_uCombatConfigFlags;
	bool			 m_bGetPosFromPed;
	RegdConstPed	 m_pInitialTarget;
	RegdConstPed	 m_pPreviousTarget;
	CTaskSimpleTimer m_WaitForTargetTimer;
#if !__FINAL
public:

	// PURPOSE: Display debug information specific to this task
	void Debug() const;

	// PURPOSE: Get the name of the specified state
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL
};

//////////////////////////////////////////////////////////////////////////
// CClonedCombatClosestTargetInAreaInfo
//////////////////////////////////////////////////////////////////////////
class CClonedCombatClosestTargetInAreaInfo : public CSerialisedFSMTaskInfo
{
public:

	static const float    MAX_AREA;

public:
	CClonedCombatClosestTargetInAreaInfo();
	CClonedCombatClosestTargetInAreaInfo(s32 state, const Vector3 &pos, float area, bool getPosFromPed, float combatTimer);
	~CClonedCombatClosestTargetInAreaInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_COMBAT_CLOSEST_TARGET_IN_AREA;}
    virtual int GetScriptedTaskType() const { return m_GetPosFromPed ? SCRIPT_TASK_COMBAT_HATED_TARGETS_AROUND_PED:SCRIPT_TASK_COMBAT_HATED_TARGETS_IN_AREA;  }

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

        SERIALISE_BOOL(serialiser, m_GetPosFromPed, "GetsPosFromPed");

        if(!m_GetPosFromPed)
        {
            SERIALISE_POSITION(serialiser, m_Pos, "Position");
        }

        SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_Area, MAX_AREA, SIZEOF_AREA,  "Area");

		Assertf(m_CombatTimer == CTaskCombatClosestTargetInArea::INVALID_OVERALL_TIME, "Combat timer is not -1.0, this is not currently supported!");
	}

    virtual CTask *CreateLocalTask(fwEntity *pEntity);

private:

    static const unsigned SIZEOF_AREA  = 8;

	CClonedCombatClosestTargetInAreaInfo(const CClonedCombatClosestTargetInAreaInfo &);
	CClonedCombatClosestTargetInAreaInfo &operator=(const CClonedCombatClosestTargetInAreaInfo &);

    Vector3		m_Pos;
	u32			m_endTime;
	float		m_Area;
	bool		m_GetPosFromPed;
    float		m_CombatTimer;
};

//-------------------------------------------------------------------------
// An additional task started with movement or whatever
// Will aim/fire at target if there is a LOS
// or stream and play upper body clips for armed peds running around
//-------------------------------------------------------------------------
class CTaskCombatAdditionalTask : public CTask
{
public:

	struct Tunables : public CTuning
	{
		Tunables();

		int		m_iBulletEventResponseLengthMs;
		float	m_fChanceOfDynamicRun;
		float	m_fMaxDynamicStrafeDistance;
		float	m_fMinTimeInState;
		float	m_fMoveBlendRatioLerpTime;
		float	m_fMinDistanceToClearCorner;
		float	m_fMaxDistanceFromCorner;
		float	m_fMaxLeavingCornerDistance;
		float	m_fBlockedLosAimTime;
		float	m_fStartAimingDistance;
		float	m_fStopAimingDistance;
		float	m_fMinOtherPedDistanceDiff;
		float	m_fMinTimeBetweenRunDirectlyChecks;
		float	m_fMaxTimeStrafing;
		float	m_fMinTimeRunning;
		float	m_fForceStrafeDistance;
		float	m_fMaxLosBlockedTimeToForceClearLos;
		PAR_PARSABLE;
	};


	static Tunables	ms_Tunables;
	static s32	  ms_iTimeOfLastOrderClip;
	static bank_u32 ms_iTimeBetweenOrderClips;
	static bank_float  ms_fDistanceToGiveOrders;
	static bank_float  ms_fTimeBetweenCommunicationClips;
	static bank_float  ms_fTimeToDoMoveCommunicationClips;

	// State
	enum
	{
		State_Start = 0,
		State_SwapWeapon,
		State_AimingOrFiring,
		State_PlayingClips,
		State_Finish
	};

	enum {
		RT_Aim = 1<<0,
		RT_Fire = 1<<1,
		RT_Clips = 1<<2,
		RT_Communicate = 1<<3,
		RT_AimAtSpecificPos = 1<<4,
		RT_AllowAimFireToggle = 1<<5,
		RT_MakeDynamicAimFireDecisions = 1<<6,
		RT_AllowAimFixupIfLosBlocked = 1<<7,
		RT_AllowAimingOrFiringIfLosBlocked = 1<<8,
		RT_Default = RT_Aim|RT_Fire|RT_Clips|RT_Communicate,
		RT_DefaultNoFiring = RT_Aim|RT_Clips|RT_Communicate,
		RT_DefaultJustClips = RT_Clips|RT_Communicate,
	};

	// Constructor/Destructor
	CTaskCombatAdditionalTask( s32 eRunningFlags, const CPed* pTarget, const Vector3& vTarget, float fTimer = 0.0f );
	~CTaskCombatAdditionalTask();

	// Task required functions
	virtual aiTask* Copy() const { return rage_new CTaskCombatAdditionalTask(m_iRunningFlags, m_pTargetPed, m_vTarget, m_fTimer ); }
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_ADDITIONAL_COMBAT_TASK;} 

	// FSM required functions
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual FSM_Return	ProcessPreFSM();
	virtual s32		GetDefaultStateAfterAbort()	const { return GetState(); }
	virtual bool	MayDeleteOnAbort() const { return true; }	// This task doesn't resume, OK to delete if aborted.

	// Debug
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif 

	void SetTargetPed(const CPed* pNewTarget);
	const CPed* GetTarget() { return m_pTargetPed; }
	static fwMvClipId GetOrderClip( CPed* pPed );
	void SetAimAtSpecificPosition(Vector3& vAimAtPos) { m_vSpecificAimAtPosition = vAimAtPos; }
	void SetFlags( s32 iFlags ) { m_iRunningFlags = iFlags; }

	// Called from the whizzed by event to let the task know there was one
	void OnGunShotWhizzedBy(const CEventGunShotWhizzedBy& rEvent);

private:
	s32 GetDesiredTaskState( CPed* pPed, bool bTaskFinished );
	void GetDesiredTarget(CAITarget& target);

	bool ShouldPedStopAndWaitForGroupMembers( CPed* pPed );

	fwMvClipId GetSignalDirectionFromMovementTask( CPed* pPed );
	aiTask* CreateTaskFromState( CPed* pPed, s32 state );

protected:
	// State Functions
	FSM_Return					Start_OnUpdate						(CPed* pPed);

	void						SwapWeapon_OnEnter					();
	FSM_Return					SwapWeapon_OnUpdate					();

	void						AimingOrFiring_OnEnter				(CPed* pPed);
	FSM_Return					AimingOrFiring_OnUpdate				(CPed* pPed);
	void						AimingOrFiring_OnExit				();

	void						PlayingClips_OnEnter				(CPed* pPed);
	FSM_Return					PlayingClips_OnUpdate				(CPed* pPed);

	void						Finish_OnEnter						(CPed* pPed);
	FSM_Return					Finish_OnUpdate						(CPed* pPed);

	// Common function
	void						ProcessState(CPed* pPed);

	// Member function
	bool						IsStill() const;
	bool						ShouldClearCorner(bool bWasClearingCorner);
	bool						ShouldAimOrFireDynamically();
	bool						ShouldAimOrFire();
	bool						ShouldAimOrFireAtTarget(bool bWasClearingCorner);
	bool						CanSwitchStates();
	bool						HasClearLos();
	void						UpdateBlockedLosTime();

private:

	Vector3			m_vLastCornerCleared;
	Vector3			m_vSpecificAimAtPosition;
	Vector3			m_vTarget;

	float			m_fTimer;
	float			m_fOriginalMoveBlendRatio;
	float			m_fMoveBlendRatioLerp;
	float			m_fBlockedLosTime;
	float			m_fCheckForRunningDirectlyTimer;

	u32				m_uLastGunShotWhizzedByTime;
	s32				m_iRunningFlags;

	bool			m_bAimTargetChanged;
	bool			m_bClearingCorner;
	bool			m_bLeavingCorner;

	RegdConstPed	m_pTargetPed;
};


#endif // INC_TASK_COMBAT_H
