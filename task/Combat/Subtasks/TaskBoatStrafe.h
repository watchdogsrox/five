// FILE :    TaskBoatStrafe.h
// PURPOSE : Subtask of boat combat used to strafe a target

#ifndef TASK_BOAT_STRAFE_H
#define TASK_BOAT_STRAFE_H

// Game headers
#include "ai/AITarget.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

// Forward declarations
class CBoat;

//=========================================================================
// CTaskBoatStrafe
//=========================================================================

class CTaskBoatStrafe : public CTask
{

public:

	enum State
	{
		State_Start = 0,
		State_Approach,
		State_Strafe,
		State_Finish,
	};

public:

	struct Tunables : public CTuning
	{
		Tunables();
		
		float m_AdditionalDistanceForApproach;
		float m_AdditionalDistanceForStrafe;
		float m_CruiseSpeedForStrafe;
		float m_RotationLookAhead;
		float m_MaxAdjustmentLookAhead;

		PAR_PARSABLE;
	};

	CTaskBoatStrafe(const CAITarget& rTarget, float fRadius);
	virtual ~CTaskBoatStrafe();

public:

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

private:

	CBoat*	GetBoat() const;
	bool	ShouldApproach() const;
	bool	ShouldStrafe() const;

private:

	virtual aiTask* Copy() const { return rage_new CTaskBoatStrafe(m_Target, m_fRadius); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_BOAT_STRAFE; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	void		Approach_OnEnter();
	FSM_Return	Approach_OnUpdate();
	void		Strafe_OnEnter();
	FSM_Return	Strafe_OnUpdate();
	
private:

	Vec3V_Out	CalculateTargetPositionForStrafe() const;
	void		UpdateTargetPositionForStrafe();

private:

	CAITarget	m_Target;
	float		m_fRadius;

private:

	static Tunables	sm_Tunables;

};

#endif // TASK_BOAT_STRAFE_H
