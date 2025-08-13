#ifndef INC_TASK_MOVE_WITHIN_DEFENSIVE_AREA_H
#define INC_TASK_MOVE_WITHIN_DEFENSIVE_AREA_H

// FILE :    TaskMoveWithinDefensiveArea.h
// PURPOSE : Controls a peds movement within their defensive area
// CREATED : 02-04-2012

//Game headers
#include "parser/manager.h"
#include "Peds/CombatBehaviour.h"
#include "Peds/QueriableInterface.h"
#include "Task/System/TaskFSMClone.h"

////////////////////////////////////////
//   TASK_MOVE_WITHIN_DEFENSIVE_AREA   //
////////////////////////////////////////

class CTaskMoveWithinDefensiveArea : public CTask
{
public:

	// FSM states
	enum
	{
		State_Start = 0,
		State_SearchForCover,
		State_GoToDefensiveArea,
		State_Finish
	};

	enum ConfigFlags
	{
		CF_DontSearchForCover	= BIT0,
	};

	CTaskMoveWithinDefensiveArea(const CPed* pPrimaryTarget);
	virtual ~CTaskMoveWithinDefensiveArea();

	// Task required implementations
	virtual aiTask*				Copy() const						{ return rage_new CTaskMoveWithinDefensiveArea(m_pPrimaryTarget); }
	virtual int					GetTaskTypeInternal() const					{ return CTaskTypes::TASK_MOVE_WITHIN_DEFENSIVE_AREA; }
	virtual s32					GetDefaultStateAfterAbort() const	{ return State_Start; }

	fwFlags8&					GetConfigFlags()					{ return m_uConfigFlags; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

private:

	// State Machine Update Functions
	virtual FSM_Return			ProcessPreFSM();
	virtual FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// Local state function calls
	FSM_Return					Start_OnUpdate				();
	void						SearchForCover_OnEnter		();
	FSM_Return					SearchForCover_OnUpdate		();
	void						GoToDefensiveArea_OnEnter	();
	FSM_Return					GoToDefensiveArea_OnUpdate	();

	bool						UpdateGoToLocation			();

	// Destination in the defensive area
	Vector3 m_vDefensiveAreaPoint;

	// Current target
	RegdConstPed m_pPrimaryTarget;

	// Have we already searched for cover? (used primarily for going to cover search area before doing the search)
	bool m_bHasSearchedForCover;

	// Config flags that hold some info on how this task should behave
	fwFlags8 m_uConfigFlags;
};

#endif // INC_TASK_MOVE_WITHIN_DEFENSIVE_AREA_H
