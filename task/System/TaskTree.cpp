// File header
#include "Task/System/TaskTree.h"

// Game headers
#include "Peds/Ped.h"
#include "Task/System/TaskFSMClone.h"
#include "network/general/NetworkUtil.h"
#include "streaming/streamingvisualize.h"

AI_OPTIMISATIONS()

void CTaskTree::Process(float fTimeStep)
{
	STRVIS_AUTO_CONTEXT(strStreamingVisualize::TASKTREE);

	//main tasks
	UpdateTasks(false, -1, fTimeStep);
}

bool CTaskTree::ProcessPhysics(float fTimeStep, int nTimeSlice, bool nmTasksOnly /*=false*/)
{
	// Keep track of the current priority of task being processed.
	m_iPriorityBeingProcessed = GetActiveTaskPriority();

	bool bProcessedPhysics = false;

	CTask* pTask = static_cast<CTask*>(GetActiveTask());
	while(pTask)
	{
		if(!nmTasksOnly || pTask->IsNMBehaviourTask())
		{
		    bProcessedPhysics = pTask->ProcessPhysics(fTimeStep, nTimeSlice);
		}

		pTask = pTask->GetSubTask();
	}

	m_iPriorityBeingProcessed = -1;

	return bProcessedPhysics;
}

void CTaskTree::ProcessPostMovement()
{
	// Keep track of the current priority of task being processed.
	m_iPriorityBeingProcessed = GetActiveTaskPriority();

	aiTask* pTask = GetActiveTask();
	while(pTask)
	{
		((CTask*)pTask)->ProcessPostMovement();
		pTask = pTask->GetSubTask();
	}

	m_iPriorityBeingProcessed = -1;
}

void CTaskTree::ProcessPostCamera()
{
	// Keep track of the current priority of task being processed.
	m_iPriorityBeingProcessed = GetActiveTaskPriority();

	aiTask* pTask = GetActiveTask();
	while(pTask)
	{
		((CTask*)pTask)->ProcessPostCamera();
		pTask = pTask->GetSubTask();
	}

	m_iPriorityBeingProcessed = -1;
}

void CTaskTree::ProcessPreRender2()
{
	// Keep track of the current priority of task being processed.
	m_iPriorityBeingProcessed = GetActiveTaskPriority();

	aiTask* pTask = GetActiveTask();
	while(pTask)
	{
		((CTask*)pTask)->ProcessPreRender2();
		pTask = pTask->GetSubTask();
	}

	m_iPriorityBeingProcessed = -1;
}

void CTaskTree::ProcessPostPreRender()
{
	// Keep track of the current priority of task being processed.
	m_iPriorityBeingProcessed = GetActiveTaskPriority();

	aiTask* pTask = GetActiveTask();
	while(pTask)
	{
		((CTask*)pTask)->ProcessPostPreRender();
		pTask = pTask->GetSubTask();
	}

	m_iPriorityBeingProcessed = -1;
}

void CTaskTree::ProcessPostPreRenderAfterAttachments()
{
	// Keep track of the current priority of task being processed.
	m_iPriorityBeingProcessed = GetActiveTaskPriority();

	aiTask* pTask = GetActiveTask();
	while(pTask)
	{
		((CTask*)pTask)->ProcessPostPreRenderAfterAttachments();
		pTask = pTask->GetSubTask();
	}

	m_iPriorityBeingProcessed = -1;
}

bool CTaskTree::ProcessMoveSignals()
{
	STRVIS_AUTO_CONTEXT(strStreamingVisualize::TASKTREE);

	// Keep track of the current priority of task being processed.
	m_iPriorityBeingProcessed = GetActiveTaskPriority();

	// Call ProcessMoveSignals() on all tasks and accumulate the return values
	// so that we return true if any of the calls returned true (indicating that
	// they want the updates to continue).
	bool ret = false;
	aiTask* pTask = GetActiveTask();
	while(pTask)
	{
		PrefetchDC(pTask->GetSubTask());
		ret |= ((CTask*)pTask)->ProcessMoveSignals();
		pTask = pTask->GetSubTask();
	}

	m_iPriorityBeingProcessed = -1;

	return ret;
}
