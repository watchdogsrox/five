//
// Task/Combat/Subtasks/TaskAimFromGround.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_TASKAIMFROMGROUND_H_
#define INC_TASKAIMFROMGROUND_H_

#include "Task/System/Task.h"
#include "Task/System/TaskTypes.h"
#include "Task/Weapons/WeaponTarget.h"

class CTaskAimGunScripted;

//////////////////////////////////////////////////////////////////////////
// PURPOSE: Allows characters to aim from the ground
//////////////////////////////////////////////////////////////////////////

class CTaskAimFromGround : public CTask
{
public:

	struct Tunables : CTuning
	{
		Tunables();
		
		float m_MaxAimFromGroundTime;

		PAR_PARSABLE;
	};

	// PURPOSE: FSM states
	enum eAimFromGroundTaskState
	{
		State_Start = 0,
		State_AimingFromGround,
		State_Finish,
	};

	// PURPOSE: Constructor
	CTaskAimFromGround(u32 scriptedGunTaskInfoHash, const fwFlags32& iFlags, const CWeaponTarget& target, bool bForceShootFromGround = false);

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_AIM_FROM_GROUND; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const { return rage_new CTaskAimFromGround(m_ScriptedGunTaskInfoHash, m_iFlags, m_target, m_bForceShootFromGround); }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		Assert(iState>=State_Start&&iState<=State_Finish);
		static const char* aStateNames[] = 
		{
			"State_Start",
			"State_AimingFromGround",
			"State_Finish"
		};

		return aStateNames[iState];
	}
#endif // !__FINAL

protected:

	////////////////////////////////////////////////////////////////////////////////

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32 GetDefaultStateAfterAbort() const { return State_Finish; }

	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	FSM_Return	Start_OnUpdate();

	void		AimingFromGround_OnEnter();
	FSM_Return	AimingFromGround_OnUpdate();
	void		AimingFromGround_OnExit();

	CTaskAimGunScripted* FindScriptedGunTask();

	////////////////////////////////////////////////////////////////////////////////

	u32 m_ScriptedGunTaskInfoHash;
	fwFlags32 m_iFlags;
	CWeaponTarget m_target;
	bool m_bForceShootFromGround;

public:

	static Tunables	sm_Tunables;

};

#endif // ! INC_TASKAIMFROMGROUND_H_