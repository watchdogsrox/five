//Game headers.
#include "Task\System\TaskTreeMotion.h"

#include "Peds\Ped.h"

AI_OPTIMISATIONS()

CTaskTreeMotion::CTaskTreeMotion(fwEntity* pEntity, const s32 iNumberOfPriorities) :
CTaskTree(pEntity, iNumberOfPriorities)
{
}

void CTaskTreeMotion::SetTask(aiTask* pTask, const s32 iPriority, const bool bForceNewTask)
{
	CTaskTree::SetTask(pTask, iPriority, bForceNewTask);
	
	if(iPriority == PED_TASK_MOTION_DEFAULT)
	{
		CPed *pPed = SafeCast(CPed, m_pEntity);
		if(pPed)
		{
			pPed->SetCurrentMotionTaskDirty();
		}
	}
}

void CTaskTreeMotion::OnNewSubTaskCreated(aiTask* UNUSED_PARAM(pTask), aiTask* UNUSED_PARAM(pSubTask))
{
    CPed *pPed = SafeCast(CPed, m_pEntity);
	if(pPed)
	{
		pPed->SetCurrentMotionTaskDirty();
	}
}

void CTaskTreeMotion::OnTaskDeleted(aiTask* UNUSED_PARAM(pTask))
{
	CPed *pPed = SafeCast(CPed, m_pEntity);
	if(pPed)
	{
		pPed->SetCurrentMotionTaskDirty();
	}
}
