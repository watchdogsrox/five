#ifndef TASK_COLLISION_RESPONSE_H
#define TASK_COLLISION_RESPONSE_H

//Game headers.
#include "animation/animdefines.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/QueriableInterface.h"
#include "Task/System/Task.h"
#include "Task/System/TaskTypes.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"
#include "Task/Movement/TaskGoTo.h"
#include "Task/Movement/TaskGoToPoint.h"

class CVehicle;
class CPed;
class CTaskSimpleLookAtEntityOrCoord;
class CClipPlayer;
class CEntity;
class CPointRoute;
class CColSphere;
class CWaveFront;

//Rage headers.
#include "vector/vector3.h"

//Std headers.
#include <string>

////////////////////
//Hit Wall
////////////////////
class CTaskHitWall : public CTaskRunClip
{
public:
	CTaskHitWall() : CTaskRunClip(CLIP_SET_STD_PED, CLIP_STD_HIT_WALL, SLOW_BLEND_IN_DELTA, SLOW_BLEND_OUT_DELTA, -1, 0) { SetInternalTaskType(CTaskTypes::TASK_HIT_WALL);}
	virtual aiTask* Copy() const {return rage_new CTaskHitWall();}
	virtual bool IsInterruptable(const CPed*) const {return false;}
	virtual int GetTaskTypeInternal() const {return (int)CTaskTypes::TASK_HIT_WALL;}
};

////////////////////
//Cower
////////////////////
class CClonedCowerInfo : public CClonedScriptClipInfo
{
	friend class CTaskCower;

public:

	CClonedCowerInfo() : m_iDuration(0) { }
	CClonedCowerInfo(s32 state, s32 iScenarioIndex, s32 iDuration) : m_iScenarioIndex(iScenarioIndex), m_iDuration(iDuration < 0 ? 0 : static_cast<u32>(iDuration))
	{
		SetStatusFromMainTaskState(state);
	}

	s32 GetDuration() { return (m_iDuration == 0) ? -1 : static_cast<s32>(m_iDuration); }
	s32 GetScenarioIndex() { return m_iScenarioIndex; }

	virtual s32 GetTaskInfoType() const { return INFO_TYPE_COWER; }
	virtual int GetScriptedTaskType() const;

	virtual CTaskFSMClone* CreateCloneFSMTask();
	
	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		bool bSyncDuration = (m_iDuration > 0);
		SERIALISE_BOOL(serialiser, bSyncDuration, "Has Duration");
		if(bSyncDuration || serialiser.GetIsMaximumSizeSerialiser())
		{
			u32 nDuration = m_iDuration;
			SERIALISE_UNSIGNED(serialiser, nDuration, SIZEOF_DURATION, "Duration");
			m_iDuration = nDuration;
		}
		else
			m_iDuration = 0;

		SERIALISE_UNSIGNED(serialiser, m_iScenarioIndex, 32, "Scenario Index");
	}

private:

	static const unsigned int SIZEOF_DURATION = 16; 
	u32 m_iDuration : SIZEOF_DURATION;
	u32 m_iScenarioIndex;

	CClonedCowerInfo(const CClonedCowerInfo &);
	CClonedCowerInfo &operator=(const CClonedCowerInfo &);
};

//-----------------------------------------------------------
// CTaskCower
// Use a cower scenario point for a specified timer interval.
//-----------------------------------------------------------

class CTaskCower : public CTaskFSMClone
{
public:
	
	//iDuration = in milliseconds
	//-1 = forever, -2=let the subtask decide.
	CTaskCower(const int iDuration = -1, const int iScenarioIndex = 0);
	virtual aiTask* Copy() const {return rage_new CTaskCower(m_iDuration);}
	virtual bool IsInterruptable(const CPed*) const {return false;}
	virtual int GetTaskTypeInternal() const {return (int)CTaskTypes::TASK_COWER;}

	virtual s32 GetDefaultStateAfterAbort() const { return State_Finish; }
	virtual FSM_Return UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
		case State_Running : return "Running";
		case State_Finish : return "Finish";
		default: Assert(0); return NULL;
		}
	}
#endif

	// clone task implementation
	virtual CTaskInfo* CreateQueriableState() const;
	virtual void ReadQueriableState(CClonedFSMTaskInfo *pTaskInfo);
	virtual CTaskFSMClone* CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone* CreateTaskForLocalPed(CPed* pPed);
	virtual void OnCloneTaskNoLongerRunningOnOwner();

	// Helper functions.
	void	SetFlinchIgnoreEvent(const CEvent* pEvent);

	bool LeaveCowerAnimsForClone() const { return m_cloneLeaveCowerAnims; }
	bool ShouldForceStateChangeOnMigrate() const { return m_forceStateChangeOnMigrate; }
protected:

	enum
	{
		State_Running,
		State_Finish,
	};

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual bool ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	void StateRunning_OnEnter();
	FSM_Return StateRunning_OnUpdate();

private:

	
	// quit the task after duration milliseconds
	CTaskGameTimer m_Timer;

	int m_iDuration;
	s32 m_iScenarioIndex;

	bool m_cloneLeaveCowerAnims;
	bool m_forceStateChangeOnMigrate;
};

//-------------------------------------------------------------------------
// CLASS: CTaskComplexEvasiveStep
// Evasive step as result of a potential get run over
//-------------------------------------------------------------------------
class CTaskComplexEvasiveStep : public CTaskFSMClone
{
public:  

	enum ConfigFlags
	{
		CF_Dive				= BIT0,
		CF_Casual			= BIT1,
		CF_Interrupt		= BIT2,
		// Update CF_LAST_SYNCED_FLAG when needed
		CF_LAST_SYNCED_FLAG = CF_Interrupt
	};

	enum Direction
	{
		D_None,
		D_Back,
		D_Front,
		D_Left,
		D_Right,
	};

	// FSM States
	enum State
	{
		State_PlayEvasiveStepClip,
		State_GetUp,
		State_Quit
	};

public:

	struct Tunables : CTuning
	{
		Tunables();

		float m_BlendOutDelta;

		PAR_PARSABLE;
	};

public:

	static const float ms_fTurnSpeedMultiplier;
	static const float ms_fTurnTolerance;
    
	// Initialization / Destruction
    CTaskComplexEvasiveStep(CEntity* pTargetEntity, fwFlags8 uConfigFlags = 0);
	CTaskComplexEvasiveStep(const Vector3 & vDivePosition);

    ~CTaskComplexEvasiveStep();

public:

	float	GetBlendOutDelta() const;
	void	SetBlendOutDeltaOverride(float fBlendOutDelta);
	CEntity* GetTargetEntity() { return m_pTargetEntity; }

public:

	// FSM API implementation
	virtual void CleanUp();
    virtual aiTask*	Copy() const;
    
    virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_COMPLEX_EVASIVE_STEP;}

#if !__FINAL
    virtual void Debug() const{}
#endif

	virtual FSM_Return  ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32			GetDefaultStateAfterAbort() const { return State_Quit; }
    
	// Clone task implementation
	virtual bool ControlPassingAllowed(CPed*, const netPlayer&, eMigrationType)	{ return false; }
	virtual CTaskInfo* CreateQueriableState() const;
	virtual void ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual void OnCloneTaskNoLongerRunningOnOwner();
	virtual FSM_Return UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);

	// 
	bool GetIsDiving() { return m_uConfigFlags.IsFlagSet(CF_Dive); }

	static Vector3 ComputeEvasiveStepMoveDir(const CPed& ped, CVehicle *pVehicle);

	void InterruptWhenPossible() { m_uConfigFlags.SetFlag(CF_Interrupt); }

protected:

	virtual bool ShouldAbort(const AbortPriority priority, const aiEvent* pEvent);
	
private:

	// State Machine
	FSM_Return StatePlayEvasiveStepClip_OnEnter(CPed* pPed);
	FSM_Return StatePlayEvasiveStepClip_OnUpdate(CPed* pPed);
	void	   StatePlayEvasiveStepClip_OnExit(CPed* pPed);

	FSM_Return StateGetUp_OnEnter(CPed* pPed);
	FSM_Return StateGetUp_OnUpdate(CPed* pPed);

	// Subtask creation
	aiTask* CreatePlayEvasiveStepSubtask(CPed* pPed);
	aiTask* CreateGetUpSubTask(CPed* pPed);

	//
	bool CanAbortForDamageAnim(const aiEvent* pEvent) const;
	bool CanInterrupt() const;
	bool CanSwitchToNm() const;
	bool GetClipForReaction(Direction nOrientation, Direction nDirection, bool bDive, bool bCasual, fwMvClipSetId& clipSetId, fwMvClipId& clipId) const;
	void OnBlendOutDeltaOverrideChanged();
	bool ShouldInterrupt() const;
	void UpdateHeading();

private:

	BANK_ONLY(Vector3 m_vMoveDir;);
	Vector3 m_vDivePosition;
    RegdEnt m_pTargetEntity;
	float m_fBlendOutDeltaOverride;
	fwFlags8 m_uConfigFlags;
	bool m_bUseDivePosition;

private:

	static Tunables sm_Tunables;

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif
};

//-------------------------------------------------------------------------
// CLASS: CClonedComplexEvasiveStepInfo
// TaskInfo class for CTaskComplexEvasiveStep
//-------------------------------------------------------------------------
class CClonedComplexEvasiveStepInfo : public CSerialisedFSMTaskInfo
{

public:

	// Initialization / Destruction
	CClonedComplexEvasiveStepInfo( 
		s32 const iState, 
		CEntity* pTarget,
		u8 const uConfigFlags,
		bool const bUseDivePosition,
		const Vector3& vDivePosition
	);

	CClonedComplexEvasiveStepInfo();
	~CClonedComplexEvasiveStepInfo();

	// Getters
	const CEntity*						GetTarget()	const			{ return m_pTarget.GetEntity();	}
	CEntity*							GetTarget()					{ return m_pTarget.GetEntity();	}

	u8									GetConfigFlags() const		{ return m_uConfigFlags; }
	bool								GetUseDivePosition() const  { return m_bUseDivePosition ; }
	const Vector3&						GetDivePosition	() const	{ return m_vDivePosition; }

	virtual s32							GetTaskInfoType() const		{ return INFO_TYPE_EVASIVE_STEP;}

	// CSerializedFSMTaskInfo overrides
	virtual CTask*						CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity));
	virtual CTaskFSMClone*				CreateCloneFSMTask();

	virtual bool						HasState() const			{ return true; }
	virtual u32							GetSizeOfState() const		{ return CTaskComplexEvasiveStep::State_Quit; }
	virtual const char*					GetStatusName(u32) const	{ return "Status";}

	virtual void						Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		ObjectId targetID = m_pTarget.GetEntityID();
		serialiser.SerialiseObjectID(targetID, "Target Entity");
		m_pTarget.SetEntityID(targetID);

		SERIALISE_UNSIGNED(serialiser, m_uConfigFlags, SIZEOF_CONFIG_FLAGS, "Config Flags");
		SERIALISE_BOOL(serialiser, m_bUseDivePosition);
		SERIALISE_POSITION_SIZE(serialiser, m_vDivePosition, "Dive Position", SIZEOF_DIVE_POSITION);
	}

private:
	
	// Serialization sizes
	static const unsigned SIZEOF_DIVE_POSITION = 24;
	static const unsigned SIZEOF_CONFIG_FLAGS = datBitsNeeded<CTaskComplexEvasiveStep::CF_LAST_SYNCED_FLAG>::COUNT;
	
	// Copy forbidden
	CClonedComplexEvasiveStepInfo(const CClonedComplexEvasiveStepInfo &);
	CClonedComplexEvasiveStepInfo &operator=(const CClonedComplexEvasiveStepInfo &);

	// State
	CSyncedEntity m_pTarget;
	u8			  m_uConfigFlags;
	bool		  m_bUseDivePosition;
	Vector3		  m_vDivePosition;
};

///////////////////////////
//Walk round fire
///////////////////////////

class CTaskWalkRoundFire : public CTask
{
public:

	enum
	{
		State_Initial,
		State_Moving
	};

	static const float ms_fFireRadius;
    
    CTaskWalkRoundFire(const float fMoveBlendRatio, const Vector3& vFirePos, const float fFireRadius, const Vector3& vTarget);
    ~CTaskWalkRoundFire();
   
    virtual aiTask* Copy() const
	{
    	return rage_new CTaskWalkRoundFire(m_fMoveBlendRatio,m_vFirePos,m_fFireRadius,m_vTarget);
	}   
    virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_WALK_ROUND_FIRE;}
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

#if !__FINAL
    virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case State_Initial : return "Initial";
			case State_Moving : return "Moving";
			default: Assert(0); return NULL;
		}
	}
#endif

	FSM_Return StateInitial_OnEnter(CPed * pPed);
	FSM_Return StateInitial_OnUpdate(CPed * pPed);
	FSM_Return StateMoving_OnEnter(CPed * pPed);
	FSM_Return StateMoving_OnUpdate(CPed * pPed);

private:
      
	float m_fMoveBlendRatio;
	// [SPHERE-OPTIMISE] m_vFirePos, m_fFireRadius should be spdSphere
    Vector3 m_vFirePos;
    float m_fFireRadius;
    Vector3 m_vTarget;
    Vector3 m_vStartPos;
    
    void ComputeDetourTarget(const CPed& ped, Vector3& vDetourTarget);
};

// NAME : CTaskMoveWalkRoundVehicle
// PURPOSE : A task tailored specifically to calculating a point-route around a vehicle
// This differs from CTaskWalkRoundEntity in that it must be aware of doors and other features.
class CTaskMoveWalkRoundVehicle : public CTaskMove
{
public:
	CTaskMoveWalkRoundVehicle(const Vector3 & vTarget, CVehicle * pVehicle, const float fMoveBlendRatio, const u32 iFlags);
	virtual ~CTaskMoveWalkRoundVehicle();

	enum
	{
		State_Initial,
		State_FollowingPointRoute
	};

	enum
	{
		Flag_TestHasClearedEntity	=	0x1
	};

	static const int m_iTestClearedEntityFrequency;

#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case State_Initial: return "Initial";
			case State_FollowingPointRoute: return "FollowingPointRoute";
			default : return NULL;
		}
	}
#endif 

	inline virtual CTask * Copy() const
	{
		return rage_new CTaskMoveWalkRoundVehicle(m_vTarget, m_pVehicle, m_fMoveBlendRatio, m_iFlags);
	}
	inline virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOVE_WALK_ROUND_VEHICLE; }
	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

	inline CVehicle * GetVehicle() { return m_pVehicle; }

	FSM_Return StateInitial_OnEnter(CPed* pPed);
	FSM_Return StateInitial_OnUpdate(CPed* pPed);
	FSM_Return StateFollowingPointRoute_OnEnter(CPed* pPed);
	FSM_Return StateFollowingPointRoute_OnUpdate(CPed* pPed);

	// Implement CTaskMoveInterface
	virtual bool IsTaskMoving() const { return true; }
	virtual bool HasTarget() const;
	virtual Vector3 GetTarget() const;
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float GetTargetRadius(void) const;
	virtual void SetTarget( const CPed* pPed, Vec3V_In vTarget, const bool bForce=false);
	virtual void SetTargetRadius(const float UNUSED_PARAM(fTargetRadius)) { }
	virtual void SetSlowDownDistance(const float UNUSED_PARAM(fSlowDownDistance)) { }

protected:

	// Determine whether or not the task should abort
	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);

	bool CalculateRouteRoundVehicle(CPed * pPed);

	bool TestHasClearedEntity(CPed * pPed);

	bool HasVehicleMoved() const;

	// If the initial target is inside the vehicle, then we may put it out to the other side
	Vector3 m_vTarget;
	// Vectors used to determine when the vehicle has moved or reorientated
	Vector3 m_vInitialCarPos;
	Vector3 m_vInitialCarRightAxis;
	Vector3 m_vInitialCarFwdAxis;

	RegdVeh m_pVehicle;
	CPointRoute * m_pPointRoute;
	CTaskGameTimer m_TestClearedEntityTimer;
	u32 m_iLastTaskUpdateTimeMs;
	u32 m_iFlags;
	bool m_bCanUpdateTaskThisFrame;
};

class CTaskWalkRoundVehicleDoor : public CTaskMove
{
public:
	CTaskWalkRoundVehicleDoor(const Vector3 & vTarget, CVehicle * pParentVehicle, const s32 iDoorIndex, const float fMoveBlendRatio, const u32 iFlags=0);
	virtual ~CTaskWalkRoundVehicleDoor();

	enum
	{
		State_Moving = 0
	};
	enum
	{
		Flag_TestHasClearedDoor	=	0x1
	};

#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case State_Moving: return "State_Moving";
			default : return NULL;
		}
	}
#endif 

	inline virtual CTask * Copy(void) const
	{
		return rage_new CTaskWalkRoundVehicleDoor(m_vTarget, m_pParentVehicle, m_iDoorIndex, m_fMoveBlendRatio, m_iFlags);
	}
	inline virtual int GetTaskTypeInternal(void) const { return CTaskTypes::TASK_MOVE_WALK_ROUND_VEHICLE_DOOR; }
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Moving; }

	FSM_Return StateMoving_OnEnter(CPed* pPed);
	FSM_Return StateMoving_OnUpdate(CPed* pPed);

	// Implement CTaskMoveInterface
	virtual bool IsTaskMoving() const { return true; }
	virtual bool HasTarget() const;
	virtual Vector3 GetTarget() const;
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float GetTargetRadius(void) const;
	virtual void SetTarget( const CPed* pPed, Vec3V_In vTarget, const bool bForce=false);
	virtual void SetTargetRadius(const float UNUSED_PARAM(fTargetRadius)) { }
	virtual void SetSlowDownDistance(const float UNUSED_PARAM(fSlowDownDistance)) { }

	inline CVehicle * GetVehicle() { return m_pParentVehicle; }

protected:

	// Determine whether or not the task should abort
	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);

	void CreateDoorBoundAI(CPed * pPed, const Matrix34 & matVehicle, CEntityBoundAI & outBoundAi, Matrix34 & outMatDoor);
	s32 CalculateRoute(CPed * pPed, Vector3 * pOutRoutePoints);

	Vector3 m_vTarget;
	RegdVeh m_pParentVehicle;
	s32 m_iDoorIndex;
	u32 m_iFlags;
};

class CTaskWalkRoundEntity : public CTaskMove
{
public:

	enum
	{
		State_Initial,
		State_WalkingRoundEntity
	};

	// How often ped will recalculate their route round the entity
	static const int ms_iRecalcRouteFrequency;
	static const int m_iTestClearedEntityFrequency;

	// For walking round other peds, this defines the maximum distance ped will travel.
	// This will help to avoid deadlocks when two peds try to strafe around each other in same direction.
	// Each ped will have a slightly randomised version of this value.
	static const float ms_fDefaultMaxDistWillTravelToWalkRndPed;

	static const float ms_fDefaultWaypointRadius;

	CTaskWalkRoundEntity(const float fMoveBlendRatio, const Vector3 & vTargetPos, CEntity * pEntity, bool bStrafeAroundEntity=false);
	virtual ~CTaskWalkRoundEntity();

	void Init();

#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
		case State_Initial: return "Initial";
		case State_WalkingRoundEntity: return "WalkingRoundEntity";
		default : return NULL;
		}
	}
#endif 

	inline virtual CTask * Copy(void) const
	{
		CTaskWalkRoundEntity * pTask = rage_new CTaskWalkRoundEntity(m_fMoveBlendRatio, m_vTargetPos, m_pEntity, m_bStrafeAroundEntity);
		pTask->SetCheckForCarDoors( GetCheckForCarDoors() );
		pTask->SetWaypointRadius( GetWaypointRadius() );
		pTask->SetHitDoorId( GetHitDoorId() );
		pTask->SetLineTestAgainstBuildings( m_bLineTestAgainstBuildings );
		pTask->SetLineTestAgainstVehicles( m_bLineTestAgainstVehicles );
		pTask->SetLineTestAgainstObjects( m_bLineTestAgainstObjects );
		pTask->SetTestWhetherPedHasClearedEntity( GetTestWhetherPedHasClearedEntity() );
		return pTask;
	}

	inline virtual int GetTaskTypeInternal(void) const { return CTaskTypes::TASK_WALK_ROUND_ENTITY; }

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

	FSM_Return Initial_OnEnter(CPed* pPed);
	FSM_Return Initial_OnUpdate(CPed* pPed);
	FSM_Return WalkingRoundEntity_OnUpdate(CPed* pPed);

	inline void SetQuitNextFrame() { m_bQuitNextFrame = true; }

	inline CEntity * GetEntity(void) const { return m_pEntity; }
	virtual bool IsTaskMoving() const { return true; }

	//Implement this function for sub-classes to return  an appropriate target point.
	virtual bool HasTarget() const { return true; }
	inline virtual Vector3 GetTarget(void) const { return m_vTargetPos; }
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	inline virtual float GetTargetRadius(void) const { return CTaskMoveGoToPoint::ms_fTargetRadius; }
	virtual void SetTarget( const CPed* pPed, Vec3V_In vTarget, const bool bForce=false);
	inline virtual void SetTargetRadius	( const float UNUSED_PARAM(fTargetRadius)) {};
	inline virtual void SetSlowDownDistance( const float UNUSED_PARAM(fSlowDownDistance)) {};

	inline void SetCheckForCarDoors(const bool b) { m_bCheckForCarDoors = b; }
	inline bool GetCheckForCarDoors() const { return m_bCheckForCarDoors; }

	inline void SetTestWhetherPedHasClearedEntity(const bool b) { m_bTestHasClearedEntity = b; }
	inline bool GetTestWhetherPedHasClearedEntity() const { return m_bTestHasClearedEntity; }

	inline void SetHitDoorId(const eHierarchyId iDoorId) { m_iHitDoorId = iDoorId; }
	inline eHierarchyId GetHitDoorId() const { return m_iHitDoorId; }

	inline void SetWaypointRadius(const float r) { m_fWaypointRadius = r; }
	inline float GetWaypointRadius() const { return m_fWaypointRadius; }

	// Take care when setting these, as it could make this task *very* expensive.
	// Especially if the m_fRecalcRouteFrequency is low.
	inline void SetLineTestAgainstBuildings(const bool b) { m_bLineTestAgainstBuildings = b; }
	inline bool GetLineTestAgainstBuildings() const { return m_bLineTestAgainstBuildings; }
	inline void SetLineTestAgainstVehicles(const bool b) { m_bLineTestAgainstVehicles = b; }
	inline bool GetLineTestAgainstVehicles() const { return m_bLineTestAgainstVehicles; }
	inline void SetLineTestAgainstObjects(const bool b) { m_bLineTestAgainstObjects = b; }
	inline bool GetLineTestAgainstObjects() const { return m_bLineTestAgainstObjects; }

	bool HasEntityMovedOrReorientated();

protected:

	// Determine whether or not the task should abort
	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);

	bool TestHasClearedEntity( CPed * pPed );

	aiTask * CreateSubTask(const int iSubTaskType, CPed * ped) const;

	Vector3 m_vTargetPos;
	Vector3 m_vPedsStartingPos;			// Used to determine how for ped has moved from their starting position

	Vector3 m_vEntityInitialPos;		// Used to see if entity has moved
	Vector3 m_vEntityInitialRightVec;	// Used to see if entity has reorientated
	Vector3 m_vEntityInitialUpVec;		// Used to see if entity has reorientated

	RegdEnt m_pEntity;
	CPointRoute * m_pPointRoute;
	CTaskGameTimer m_RecalcRouteTimer;
	CTaskGameTimer m_TestClearedEntityTimer;
	float m_fMaxDistWillTravelToWalkRndPedSqr;
	u32 m_iRecalcRouteFrequency;
	eHierarchyId m_iHitDoorId;
	float m_fWaypointRadius;

	// Which side of the entity the ped&target are on, as calc'd with the CEntityBoundAI
	s32 m_iLastPedSide;
	s32 m_iLastTargetSide;

	u32 m_bNewTarget					:1;
	u32 m_bStrafeAroundEntity			:1;
	u32 m_bFirstTimeRun					:1;
	u32 m_bCheckForCarDoors				:1;
	u32 m_bLineTestAgainstBuildings		:1;
	u32 m_bLineTestAgainstVehicles		:1;
	u32 m_bLineTestAgainstObjects		:1;
	u32 m_bQuitNextFrame				:1;
	u32 m_bTestHasClearedEntity			:1;
};


class CTaskWalkRoundCarWhileWandering : public CTaskMove
{
public:

	enum
	{
		State_Initial,
		State_FollowingNavMesh
	};

	CTaskWalkRoundCarWhileWandering(const float fMoveBlendRatio, const Vector3 & vTargetPos, CVehicle * pVehicle);
	virtual ~CTaskWalkRoundCarWhileWandering();

	void Init();

#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case State_Initial: return "Initial";
			case State_FollowingNavMesh: return "FollowingNavMesh";
			default : return NULL;
		}
	}
#endif 

	inline virtual CTask * Copy() const {
		CTaskWalkRoundCarWhileWandering * pClone = rage_new CTaskWalkRoundCarWhileWandering(m_fMoveBlendRatio, m_vTargetPos, m_pVehicle);
		return pClone;
	}

	inline virtual int GetTaskTypeInternal(void) const { return CTaskTypes::TASK_MOVE_WANDER_AROUND_VEHICLE; }
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

	FSM_Return Initial_OnEnter(CPed* pPed);
	FSM_Return Initial_OnUpdate(CPed* pPed);
	FSM_Return FollowingNavMesh_OnEnter(CPed* pPed);
	FSM_Return FollowingNavMesh_OnUpdate(CPed* pPed);

	inline CVehicle * GetVehicle() const { return m_pVehicle; }
	virtual bool IsTaskMoving() const { return true; }

	//Implement this function for sub-classes to return  an appropriate target point.
	inline virtual Vector3 GetTarget(void) const { return m_vTargetPos; }
	inline virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	inline virtual float GetTargetRadius(void) const { return CTaskMoveGoToPoint::ms_fTargetRadius; }
	virtual void SetTarget( const CPed* UNUSED_PARAM(pPed), Vec3V_In UNUSED_PARAM(vTarget), const bool UNUSED_PARAM(bForce)=false) { }
	inline virtual void SetTargetRadius	( const float UNUSED_PARAM(fTargetRadius)) {};
	inline virtual void SetSlowDownDistance( const float UNUSED_PARAM(fSlowDownDistance)) {};
	inline virtual bool HasTarget() const { return true; }

protected:

	void CalculateTargetPos(CPed * pPed);
	bool IsPedPastCar(CPed * pPed);

	Vector3 m_vInitialWanderTarget;
	Vector3 m_vTargetPos;
	Vector3 m_vInitialToVehicle;
	RegdVeh m_pVehicle;
	bool m_bStartedOnPavement;
};


#endif
