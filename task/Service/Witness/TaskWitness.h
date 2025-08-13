#ifndef TASK_SERVICE_WITNESS_TASKWITNESS_H
#define TASK_SERVICE_WITNESS_TASKWITNESS_H

#include "Task/System/Task.h"
#include "Task/System/Tuning.h"

class CTaskWitness : public CTask
{
public:
	class Tunables : public CTuning
	{
	public:
		Tunables();

		u32 m_MaxTimeMoveNearCrimeMs;
		u32 m_MaxTimeMoveToLawMs;
		u32 m_MaxTimeSearchMs;
		u32 m_MaxTimeMoveToLawFailedPathfindingMs;

		PAR_PARSABLE;
	};

	enum
	{
		State_Start,
		State_MoveNearCrime,
		State_MoveToLaw,
		State_Search,
		State_Finish
	};

	explicit CTaskWitness(CPed* pCriminal, Vec3V_ConstRef crimePos);

public:

	static bool	CanGiveToWitness(const CPed& rPed);
	static bool	GiveToWitness(CPed& rPed, CPed& rCriminal, Vec3V_In vCrimePos);

public:

	// Task required implementations
	virtual aiTask*		Copy() const { return rage_new CTaskWitness(m_Criminal, m_CrimePos); }
	virtual int			GetTaskTypeInternal() const { return CTaskTypes::TASK_WITNESS; }

	// FSM required implementations
	virtual FSM_Return	UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32			GetDefaultStateAfterAbort() const { return State_Start; }

	virtual	void		CleanUp();

#if !__FINAL
	// PURPOSE: Get the name of the specified state
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

protected:
	FSM_Return Start_OnUpdate();
	FSM_Return MoveNearCrime_OnEnter();
	FSM_Return MoveNearCrime_OnUpdate();
	FSM_Return MoveToLaw_OnEnter();
	FSM_Return MoveToLaw_OnUpdate();
	FSM_Return Search_OnEnter();
	FSM_Return Search_OnUpdate();

	bool CheckSeesCriminal();
	void ReportCrimes();
	void ReportWitnessUnsuccessful();

	bool HasSpentTimeInState(u32 timeMs) const;

	RegdPed				m_Criminal;
	Vec3V				m_CrimePos;
	u32					m_TimeEnterStateMs;

	static Tunables		sm_Tunables;
};

#endif	// TASK_SERVICE_WITNESS_TASKWITNESS_H

// End of file 'task/Service/Witness/TaskWitness.h'
