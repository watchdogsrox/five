//
// task/Combat/TaskCombatMounted.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef TASK_COMBAT_TASK_COMBAT_MOUNTED_H
#define TASK_COMBAT_TASK_COMBAT_MOUNTED_H

#include "ai/task/taskstatetransition.h"	// aiTaskStateTransitionTable
#include "task/System/TaskMove.h"			// CTaskMove
#include "task/System/Tuning.h"				// CTuning
#include "task/Combat/CombatManager.h"

class CTaskVehicleGun;

//-----------------------------------------------------------------------------

class CTaskCombatMountedTransitions : public aiTaskStateTransitionTable
{
public:
	virtual void Init();
};

//-----------------------------------------------------------------------------

class CTaskCombatMounted : public CTask
{
public:
	enum State
	{
		State_Invalid = -1,
		State_Active,

		kNumStates
	};

	explicit CTaskCombatMounted(const CEntity* pPrimaryTarget);

	virtual aiTask* Copy() const;
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_COMBAT_MOUNTED; }

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Active; }

	void SetTarget(const CEntity* pPrimaryTarget)
	{
		m_PrimaryTarget = pPrimaryTarget;
	}

#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

protected:
	void Active_OnEnter();
	FSM_Return Active_OnUpdate();

	CTaskVehicleGun* CreateGunTask();

	RegdConstEnt		m_PrimaryTarget;
};

//-----------------------------------------------------------------------------

class CTaskMoveCombatMounted : public CTaskMove
{
public:
	struct Tunables : public CTuning
	{
		Tunables();

		atArray<float>	m_CircleTestRadii;
		float			m_CircleTestsMoveDistToTestNewPos;	// = 4.0f
		float			m_MinTimeSinceAnyCircleJoined;		// = 0.2f
		float			m_MinTimeSinceSameCircleJoined;		// = 0.4f
		float			m_TransitionReactionTime;			// = 1.0f
		float			m_VelStartCircling;					// = 3.0f
		float			m_VelStopCircling;					// = 3.5f
		u32				m_MaxTimeWaitingForCircleMs;		// = 3000

		PAR_PARSABLE;
	};

	static void InitTransitionTables();

	typedef enum
	{
		CF_StateEnded			= BIT0,
		CF_TargetMoving			= BIT1,
		CF_TargetStationary		= BIT2,
		CF_TargetNotInCircle	= BIT3
	} ConditionFlags;

	enum State
	{
		State_Invalid = -1,
		State_Start,
		State_Chasing,
		State_Circling,

		kNumStates
	};

	explicit CTaskMoveCombatMounted(const CEntity* pPrimaryTarget);

	virtual aiTask* Copy() const;
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_MOVE_COMBAT_MOUNTED; }

	virtual void CleanUp();
	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Start; }

	// CTaskMove functions
	virtual bool HasTarget() const { return false; }
	virtual Vector3 GetTarget(void) const {Assertf(0, "CTaskMoveCombatMounted : GetTarget() incorrectly called" ); return Vector3(0.0f, 0.0f, 0.0f);}
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float GetTargetRadius(void) const {Assertf(0, "CTaskMoveCombatMounted : GetTargetRadius() incorrectly called" ); return 0.0f;}

	void SetPrimaryTarget(const CEntity* pPrimaryTarget)
	{
		m_PrimaryTarget = pPrimaryTarget;
	}

#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	static Tunables		sm_Tunables;

protected:
	// Returns a pointer to the transition table data, overriden by tasks that use transitional sets
	virtual aiTaskTransitionTableData* GetTransitionTableData() { return &m_TransitionTableData; }

	// Transition table implementations
	// Return the valid transitional set
	virtual aiTaskStateTransitionTable*	GetTransitionSet() { return &sm_Transitions; }

	// Generate the conditional flags to choose a transition
	virtual s64 GenerateTransitionConditionFlags(bool bStateEnded);

	Vec3V_Out GetTargetVelocityV() const;

	FSM_Return Start_OnUpdate();

	void Chasing_OnEnter();
	FSM_Return Chasing_OnUpdate();
	void Chasing_OnExit();

	void Circling_OnEnter();
	FSM_Return Circling_OnUpdate();
	void Circling_OnExit();

	void RegisterAttacker(const CEntity* pNewTgt);
	void UpdateRootTarget();

	RegdConstEnt							m_PrimaryTarget;
	RegdConstEnt							m_RootTarget;

	// Transition table data - stores variables used by the base transition table implementation
	aiTaskTransitionTableData				m_TransitionTableData;

	CCombatMountedAttacker					m_Attacker;

	u32										m_TimeWaitingForCircleMs;

	bool									m_RunningCirclingTask;
	bool									m_TargetNotInCircle;

	static CTaskCombatMountedTransitions	sm_Transitions;
};

//-----------------------------------------------------------------------------

class CTaskMoveCircle : public CTaskMove
{
public:
	enum State
	{
		State_Invalid = -1,
		State_Start,
		State_ApproachingCircle,
		State_Circling,

		kNumStates
	};

	explicit CTaskMoveCircle(Vec3V_In circleCenterV, float radius, bool dirClockwise, float moveBlendRatio);

	virtual aiTask* Copy() const;
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_MOVE_CIRCLE; }

	virtual void CleanUp();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Start; }

	// CTaskMove functions - not sure if we need a "target" here in this interface.
	virtual bool HasTarget() const { return false; }
	virtual Vector3 GetTarget(void) const {Assertf(0, "CTaskMoveCircle : GetTarget() incorrectly called" ); return Vector3(0.0f, 0.0f, 0.0f);}
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float GetTargetRadius(void) const {Assertf(0, "CTaskMoveCircle : GetTargetRadius() incorrectly called" ); return 0.0f;}

#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

protected:
	FSM_Return Start_OnUpdate();

	void ApproachingCircle_OnEnter();
	FSM_Return ApproachingCircle_OnUpdate();

	void Circling_OnEnter();
	FSM_Return Circling_OnUpdate();

	void ComputePointOnCircle(Vec3V_Ref ptOut, bool useIsectIfInside);
	bool TestPosAndDirOnCircle(bool alreadyThere) const;

	Vec4V				m_CircleCenterAndRadius;
	bool				m_DirClockwise;
};

//-----------------------------------------------------------------------------

#endif	// TASK_COMBAT_TASK_COMBAT_MOUNTED_H

// End of file 'task/Combat/TaskCombatMounted.h'
