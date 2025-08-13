#ifndef TASK_IDLE_H
#define TASK_IDLE_H

#include "Peds/ped.h"
#include "Task/System/Task.h"
#include "Task/System/TaskMove.h"
#include "Task/System/TaskTypes.h"

// CLASS : CTaskMoveStandStill
// PURPOSE : Movement task which causes ped to stand still for specified time

class CTaskMoveStandStill : public CTaskMove
{
public:

	static const int ms_iStandStillTime;

	CTaskMoveStandStill(const int iDuration=ms_iStandStillTime, const bool bForever=false);
	virtual ~CTaskMoveStandStill();

	enum // State
	{
		Running
	};

	virtual aiTask* Copy() const { return rage_new CTaskMoveStandStill(m_iDuration, m_bForever); }

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_MOVE_STAND_STILL;}

	virtual s32 GetDefaultStateAfterAbort() const { return Running; }
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	FSM_Return StateRunnning_OnUpdate(CPed * pPed);

#if !__FINAL
	virtual void Debug() const {};
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32    ) {  return "none";  };
#endif

	virtual bool HasTarget() const { return false; }
	virtual Vector3 GetTarget(void) const { Assertf( 0, "CTaskMoveStandStill : Target position requested from stationary move task" ); return Vector3(0.0f, 0.0f, 0.0f); }
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float GetTargetRadius(void) const { Assertf( 0, "CTaskMoveStandStill : Target radius requested from stationary move task" ); return 0.0f; }

protected:
	Vector3 m_vPedPosition;
	int m_iDuration;
	bool m_bForever;
	CTaskGameTimer m_timer;
};








/////////////////
//Achieve heading
/////////////////

class CTaskMoveAchieveHeading : public CTaskMove
{
public:

    static const float ms_fHeadingChangeRateFrac;
    static const float ms_fHeadingTolerance;

	static bool IsHeadingAchieved(const CPed* pPed, float fDesiredHeading, float fHeadingTolerance=ms_fHeadingTolerance);

    CTaskMoveAchieveHeading(
		const float fDesiredHeading, 
		const float fHeadingChangeRateFrac=ms_fHeadingChangeRateFrac, 
		const float fHeadingTolerance=ms_fHeadingTolerance,
		const float fTime = 0.0f
	);
    ~CTaskMoveAchieveHeading();

	virtual aiTask* Copy() const
	{
		return rage_new CTaskMoveAchieveHeading(
			m_fDesiredHeading,
			m_fHeadingChangeRateFrac,
			m_fHeadingTolerance,
			m_fTimeToTurn
		);
	}

	enum { State_Running };
    
    virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_MOVE_ACHIEVE_HEADING;}

#if !__FINAL
	virtual void Debug() const;
#endif 

    FSM_Return StateRunning_OnUpdate(CPed* pPed);   
	virtual bool IsTaskMoving() const {return false;}

	// This task doesn't move so return the peds current position
	virtual bool HasTarget() const { return false; }
	virtual Vector3 GetTarget(void) const { Assertf( 0, "CTaskMoveAchieveHeading : Target position requested from stationary move task" ); return Vector3(0.0f, 0.0f, 0.0f); }
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float GetTargetRadius(void) const { Assertf( 0, "CTaskMoveAchieveHeading : Target radius requested from stationary move task" ); return 0.0f; }
    
	//*******************************************
	// Virtual members inherited From CTask
	//*******************************************
	virtual s32 GetDefaultStateAfterAbort() const { return State_Running; }
	virtual FSM_Return UpdateFSM(const s32, const FSM_Event);

	// Function to create cloned task information
	virtual CTaskInfo* CreateQueriableState() const;

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32    ) {  return "none";  };
#endif

    void SetHeading(const float fDesiredHeading, const float fHeadingChangeRateFrac=ms_fHeadingChangeRateFrac,  const float fHeadingTolerance=ms_fHeadingTolerance, const bool bForce=false)
    {
        if((bForce)||
           (m_fDesiredHeading!=fDesiredHeading)||
           (m_fHeadingChangeRateFrac!=fHeadingChangeRateFrac)||
           (m_fHeadingTolerance!=fHeadingTolerance))
        {
			m_fDesiredHeading=fwAngle::LimitRadianAngleSafe(fDesiredHeading);
            m_fHeadingChangeRateFrac=fHeadingChangeRateFrac;
            m_fHeadingTolerance=fHeadingTolerance;
        }
    }

protected:

	// Handle aborting the task - this will be called in addition to CleanUp when we are aborting
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);
	
	float m_fTimeToTurn;
private:

	// Used when responding to get target
	Vector3 m_vPedsPosition;

    float m_fDesiredHeading;
    float m_fHeadingChangeRateFrac;
    float m_fHeadingTolerance;
    
    u32 m_bDoingIK	:1;
    
    void SetUpIK(CPed * pPed);
    void QuitIK(CPed * pPed);

	static const atHashWithStringNotFinal ms_HeadIkHashKey;
};

class CClonedAchieveHeadingInfo : public CSerialisedFSMTaskInfo
{
public:

	CClonedAchieveHeadingInfo(const float fDesiredHeading, const float fTime) : m_fDesiredHeading(fDesiredHeading), m_fTime(fTime) {}
	CClonedAchieveHeadingInfo() : m_fDesiredHeading(0.0f), m_fTime(0.0f) {}

	virtual s32		GetTaskInfoType() const { return CTaskInfo::INFO_TYPE_ACHIEVE_HEADING; }
	virtual int GetScriptedTaskType() const { return SCRIPT_TASK_ACHIEVE_HEADING; }

	virtual CTask  *CreateLocalTask(fwEntity *UNUSED_PARAM(pEntity));

	void Serialise(CSyncDataBase& serialiser)
	{
		const unsigned SIZEOF_HEADING = 8;
		const unsigned SIZEOF_TIME = 12;

		SERIALISE_PACKED_FLOAT(serialiser, m_fDesiredHeading, PI, SIZEOF_HEADING, "Desired Heading");
		SERIALISE_PACKED_FLOAT(serialiser, m_fTime, 1000.0f, SIZEOF_TIME, "Time");
	}

private:

	float   m_fDesiredHeading; 
	float	m_fTime;
};

class CTaskMoveFaceTarget : public CTaskMove
{
public:

    CTaskMoveFaceTarget(
		const CAITarget& target
		)
		: CTaskMove(MOVEBLENDRATIO_STILL)
		, m_Target(target)
		, m_bLimitUpdateBasedOnDistance(false)
	{
		SetInternalTaskType(CTaskTypes::TASK_MOVE_FACE_TARGET);
		// nothing to see here
	}

    ~CTaskMoveFaceTarget()
	{
		//nothing to see here
	}

	void SetLimitUpdateBasedOnDistance(bool bValue) { m_bLimitUpdateBasedOnDistance = bValue; }

	virtual aiTask* Copy() const
	{
		CTaskMoveFaceTarget* pTask = rage_new CTaskMoveFaceTarget(
			m_Target
		);
		pTask->SetLimitUpdateBasedOnDistance(m_bLimitUpdateBasedOnDistance);
		return pTask;
	}

    virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_MOVE_FACE_TARGET;}

	virtual bool IsTaskMoving() const {return false;}

	// This task doesn't move so return the peds current position
	virtual bool HasTarget() const { return false; }
	virtual Vector3 GetTarget(void) const { Assertf( 0, "CTaskMoveFaceEntity : Target position requested from stationary move task" ); return Vector3(0.0f, 0.0f, 0.0f); }
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float GetTargetRadius(void) const { Assertf( 0, "CTaskMoveFaceEntity : Target radius requested from stationary move task" ); return 0.0f; }
    
	//*******************************************
	// Virtual members inherited From CTask
	//*******************************************
	virtual s32 GetDefaultStateAfterAbort() const { return 0; }

	virtual CTask::FSM_Return ProcessPreFSM();

	virtual FSM_Return UpdateFSM(const s32, const FSM_Event)
	{
		return FSM_Continue;
	}

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32    ) {  return "none";  };
#endif

private:

	CAITarget	m_Target;
	bool		m_bLimitUpdateBasedOnDistance;
};



// CLASS: CTaskTurnToFaceEntityOrCoord
// PURPOSE : Turns ped to face given entity or coordinate

class CTaskTurnToFaceEntityOrCoord : public CTask
{
public:

	enum
	{
		State_AchievingHeading,
	};

	CTaskTurnToFaceEntityOrCoord(
		const CEntity* pEntity, 
		const float fHeadingChangeRateFrac = CTaskMoveAchieveHeading::ms_fHeadingChangeRateFrac, 
		const float fHeadingTolerance = CTaskMoveAchieveHeading::ms_fHeadingTolerance,
		const float fTime = 0.0f
		);
	CTaskTurnToFaceEntityOrCoord(
		const Vector3& vTarget,
		const float fHeadingChangeRateFrac = CTaskMoveAchieveHeading::ms_fHeadingChangeRateFrac,
		const float fHeadingTolerance = CTaskMoveAchieveHeading::ms_fHeadingTolerance,
		const float fTime = 0.0f
		);
	~CTaskTurnToFaceEntityOrCoord();

	virtual aiTask* Copy() const
	{
		if(m_bFacingEntity)
		{
			return rage_new CTaskTurnToFaceEntityOrCoord(m_pEntity, m_fHeadingChangeRateFrac, m_fHeadingTolerance, m_fTimeToTurn);
		}
		else
		{
			return rage_new CTaskTurnToFaceEntityOrCoord(m_vTarget, m_fHeadingChangeRateFrac, m_fHeadingTolerance, m_fTimeToTurn);
		}
	}			

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_COMPLEX_TURN_TO_FACE_ENTITY;}
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_AchievingHeading; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
		case State_AchievingHeading : return "AchievingHeading";
		default: Assert(0); return NULL;
		}
	}
#endif 

	FSM_Return StateAchievingHeading_OnEnter(CPed * pPed);
	FSM_Return StateAchievingHeading_OnUpdate(CPed * pPed); 

	// Function to create cloned task information
	virtual CTaskInfo* CreateQueriableState() const;

private:

	RegdConstEnt m_pEntity;
	bool m_bFacingEntity;
	Vector3 m_vTarget;

	float m_fHeadingChangeRateFrac;
	float m_fHeadingTolerance;
	float m_fTimeToTurn;

	float ComputeTargetHeading(CPed* pPed) const;
};

class CClonedTurnToFaceEntityOrCoordInfo : public CSerialisedFSMTaskInfo
{
public:

	CClonedTurnToFaceEntityOrCoordInfo(CEntity* pEntity)
	: m_hasTargetEntity(true)
	, m_vTargetCoord(VEC3_ZERO)
	{
		if (pEntity)
		{
			m_targetEntity.SetEntity(pEntity);
		}
	}

	CClonedTurnToFaceEntityOrCoordInfo(const Vector3& vTarget)
		: m_hasTargetEntity(false)
		, m_vTargetCoord(vTarget)
	{
	}

	CClonedTurnToFaceEntityOrCoordInfo()
		: m_hasTargetEntity(false)
		, m_vTargetCoord(VEC3_ZERO)
	{
	}	

	virtual s32	GetTaskInfoType() const { return CTaskInfo::INFO_TYPE_TURN_TO_FACE_ENTITY; }
	virtual int GetScriptedTaskType() const { return SCRIPT_TASK_TURN_PED_TO_FACE_ENTITY; }

	virtual CTask  *CreateLocalTask(fwEntity *pEntity);

	void Serialise(CSyncDataBase& serialiser)
	{
		SERIALISE_BOOL(serialiser, m_hasTargetEntity);

		if (m_hasTargetEntity && !serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_SYNCEDENTITY(m_targetEntity, serialiser, "Target Entity");
		}
		else
		{
			SERIALISE_POSITION(serialiser, m_vTargetCoord, "Target coord");
		}
	}

private:

	bool			m_hasTargetEntity;
	CSyncedEntity	m_targetEntity;
	Vector3			m_vTargetCoord;
};

#endif

