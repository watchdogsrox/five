// FILE :    TaskSubmarineChase.h
// PURPOSE : Subtask of submarine combat used to chase a vehicle

#ifndef TASK_SUBMARINE_CHASE_H
#define TASK_SUBMARINE_CHASE_H

// Game headers
#include "ai/AITarget.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

// Forward declarations
class CSubmarine;

//=========================================================================
// CTaskSubmarinChase
//=========================================================================

class CTaskSubmarineChase : public CTask
{

public:

	enum State
	{
		State_Start = 0,
		State_Pursue,
		State_Finish,
	};

	enum OffsetRelative
	{
		OffsetRelative_Local,
		OffsetRelative_World,
	};
	
public:

	struct Tunables : public CTuning
	{
		Tunables();

		float m_TargetBufferDistance;
		float m_SlowDownDistanceMax; 
		float m_SlowDownDistanceMin;

		PAR_PARSABLE;
	};
	
public:

	CTaskSubmarineChase(const CAITarget& rTarget, Vec3V_In vTargetOffset);
	virtual ~CTaskSubmarineChase();
	
public:

	Vec3V_Out				GetTargetOffset						() const { return m_vTargetOffset; }
	void					SetTargetOffset						(Vec3V_In vTargetOffset) { m_vTargetOffset = vTargetOffset; }
	void					SetTargetOffsetRelative				( OffsetRelative in_OffsetRelative) { m_OffsetRelative = in_OffsetRelative; }

public:

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char*	GetStaticStateName(s32);
#endif // !__FINAL

private:

	CVehicle*				GetTargetVehicle					() const;
	CSubmarine*				GetSub								() const;

private:

	virtual aiTask*			Copy								() const { return rage_new CTaskSubmarineChase(m_Target, m_vTargetOffset); }
	virtual s32				GetTaskTypeInternal					() const { return CTaskTypes::TASK_SUBMARINE_CHASE; }
	virtual s32				GetDefaultStateAfterAbort			() const { return State_Finish; }

	virtual FSM_Return		ProcessPreFSM						();
	virtual FSM_Return 		UpdateFSM							(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return				Start_OnUpdate						();
	void					Pursue_OnEnter						();
	FSM_Return				Pursue_OnUpdate						();
	
private:

	Vec3V_Out				CalculateTargetPosition				() const;
	void					UpdateMissionParams					();
	float					CalculateSpeed						() const;
private:

	Vec3V				m_vTargetOffset;
	Vec3V				m_vTargetVelocitySmoothed;
	OffsetRelative		m_OffsetRelative;

	CAITarget			m_Target;

private:

	static Tunables		sm_Tunables;
};

#endif // TASK_SUBMARINE_CHASE_H
