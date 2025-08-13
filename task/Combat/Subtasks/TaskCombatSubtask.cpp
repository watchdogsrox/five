// FILE :    TaskCombatSubtask.cpp
// PURPOSE : Combat subtask abstract class, each subtask inherits from this to keep
//			 the interface standard
// AUTHOR :  Phil H
// CREATED : 25-06-2008

// C++ headers
#include <limits>

// Framework headers
//#include "fwmaths/Maths.h"

// Game headers
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Task\Combat\Subtasks\TaskCombatSubTask.h"


//=========================================================================
// ABSTRACT SUBTASK 
//=========================================================================

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
CTaskComplexCombatSubtask::CTaskComplexCombatSubtask( CPed* pPrimaryTarget )//, CVehicle* pVehicleLastSeenIn )
: m_pPrimaryTarget(pPrimaryTarget),
m_bMadeAbortable(false),
m_bTaskFailed(false),
m_bHasVector(false),
m_vNextTaskRelatedVector(0.0f, 0.0f, 0.0f)
{
	m_iSpecificTask = CTaskTypes::TASK_NONE;
}


//-------------------------------------------------------------------------
// Destructor
//-------------------------------------------------------------------------
CTaskComplexCombatSubtask::~CTaskComplexCombatSubtask()
{
}

//-------------------------------------------------------------------------
// Creates the first subtask
//-------------------------------------------------------------------------
aiTask* CTaskComplexCombatSubtask::CreateFirstSubTask(CPed* pPed)
{
	aiTask* pFirstSubTask = NULL;
	//	if( m_pPrimaryTarget )
	{
		// First work out the first task as decided by the decision maker
		s32 iTaskType = GetTaskFromDecisionMaker( pPed );

		// Alter this task depending on the current situation
		iTaskType = OverrideTask( pPed, iTaskType );

		// Create an instance of the task
		pFirstSubTask = CreateSubTask( iTaskType, pPed );
	}
	return pFirstSubTask;
}
