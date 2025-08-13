// FILE :    TaskBoatChase.h
// PURPOSE : Subtask of boat combat used to chase a vehicle

#ifndef TASK_BOAT_CHASE_H
#define TASK_BOAT_CHASE_H

// Game headers
#include "ai/AITarget.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

// Forward declarations
class CBoat;
class CTaskVehiclePursue;

//=========================================================================
// CTaskBoatChase
//=========================================================================

class CTaskBoatChase : public CTask
{

public:

	enum State
	{
		State_Start = 0,
		State_Pursue,
		State_Finish,
	};

public:

	struct Tunables : public CTuning
	{
		Tunables();
		
		float m_IdealDistanceForPursue;

		PAR_PARSABLE;
	};

	CTaskBoatChase(const CAITarget& rTarget);
	virtual ~CTaskBoatChase();

public:

	void SetDesiredOffsetForPursue(float fOffset);

public:

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

private:

	void				AddToDirector();
	CBoat*				GetBoat() const;
	CVehicle*			GetTargetVehicle() const;
	CTaskVehiclePursue*	GetVehicleTaskForPursue() const;
	void				OnDesiredOffsetForPursueChanged();
	void				RemoveFromDirector();

private:

	virtual aiTask* Copy() const { return rage_new CTaskBoatChase(m_Target); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_BOAT_CHASE; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

private:

	virtual void		CleanUp();
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	void		Pursue_OnEnter();
	FSM_Return	Pursue_OnUpdate();

private:

	CAITarget	m_Target;
	float		m_fDesiredOffsetForPursue;

private:

	static Tunables	sm_Tunables;

};

#endif // TASK_BOAT_CHASE_H
