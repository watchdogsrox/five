// FILE :    TaskAdvance.h

#ifndef TASK_ADVANCE_H
#define TASK_ADVANCE_H

// Rage headers
#include "atl/array.h"
#include "fwutil/Flags.h"

// Game headers
#include "ai/AITarget.h"
#include "scene/RegdRefTypes.h"
#include "task/Combat/TacticalAnalysis.h"
#include "task/Movement/TaskMoveToTacticalPoint.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

// Game forward declarations
class CCoverPoint;
class CEvent;

//=========================================================================
// CTaskAdvance
//=========================================================================

class CTaskAdvance : public CTask
{

public:

	enum ConfigFlags
	{
		CF_IsFrustrated = BIT0,
	};

	enum State
	{
		State_Start = 0,
		State_Resume,
		State_MoveToTacticalPoint,
		State_Seek,
		State_Finish,
	};

public:

	struct Tunables : CTuning
	{
		Tunables();

		float	m_TimeToWaitAtPosition;
		float	m_TimeBetweenPointUpdates;
		float	m_TimeBetweenSeekChecksAtTacticalPoint;

		PAR_PARSABLE;
	};

public:

	CTaskAdvance(const CAITarget& rTarget, fwFlags8 uConfigFlags = 0);
	virtual ~CTaskAdvance();

public:

	bool IsSeeking() const
	{
		return (GetState() == State_Seek);
	}

public:

			void	ClearSubTaskToUseDuringMovement();
	const	CPed*	GetTargetPed() const;
			bool	IsMovingToPoint() const;
			void	SetSubTaskToUseDuringMovement(CTask* pTask);

public:

	static float ScoreCoverPoint(const CTaskMoveToTacticalPoint::ScoreCoverPointInput& rInput);
	static float ScoreNavMeshPoint(const CTaskMoveToTacticalPoint::ScoreNavMeshPointInput& rInput);
	static float ScorePosition(const CTaskMoveToTacticalPoint* pTask, Vec3V_In vPosition);

public:

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

private:

	bool	CanSeek() const;
	int		ChooseStateToResumeTo(bool& bKeepSubTask) const;
	CTask*	CreateSubTaskToUseDuringMovement() const;

private:

	virtual aiTask* Copy() const;
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_ADVANCE; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Resume; }

	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	FSM_Return	Resume_OnUpdate();
	void		MoveToTacticalPoint_OnEnter();
	FSM_Return	MoveToTacticalPoint_OnUpdate();
	void		Seek_OnEnter();
	FSM_Return	Seek_OnUpdate();
	
private:

	CAITarget	m_Target;
	RegdTask	m_pSubTaskToUseDuringMovement;
	float		m_fTimeSinceLastSeekCheck;
	fwFlags8	m_uConfigFlags;

private:

	static Tunables sm_Tunables;

};

#endif // TASK_ADVANCE_H
