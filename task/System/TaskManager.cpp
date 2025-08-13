// File header
#include "Task/System/TaskManager.h"

// Game headers
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedTaskRecord.h"
#include "Task/Default/TaskWander.h"
#include "Task/System/TaskFSMClone.h"
#include "Vehicles/Vehicle.h"

AI_OPTIMISATIONS()

CTaskManager::CTaskManager(int iNumberOfTaskTreesRequired, bool treesAreExternallyOwned)
: aiTaskManager(iNumberOfTaskTreesRequired, treesAreExternallyOwned)
{
}

CTaskManager::~CTaskManager()
{
}

bool CTaskManager::ProcessPhysics(float fTimeStep, int nTimeSlice, bool nmTasksOnly /*=false*/)
{
	bool retResult = false;
	// Loop through task trees until one has processed physics, or we've loop through them all.
	for(int i = 0; i < GetTreeCount(); i++)
	{
		if(static_cast<CTaskTree*>(GetTree(i))->ProcessPhysics(fTimeStep, nTimeSlice, nmTasksOnly))
		{
			retResult = true;
		}
	}

	return retResult;
}

void CTaskManager::ProcessPostMovement(int iTaskTree)
{
	return static_cast<CTaskTree*>(GetTree(iTaskTree))->ProcessPostMovement();
}

void CTaskManager::ProcessPostCamera(int iTaskTree)
{
	return static_cast<CTaskTree*>(GetTree(iTaskTree))->ProcessPostCamera();
}

void CTaskManager::ProcessPreRender2(int iTaskTree)
{
	return static_cast<CTaskTree*>(GetTree(iTaskTree))->ProcessPreRender2();
}

void CTaskManager::ProcessPostPreRender(int iTaskTree)
{
	return static_cast<CTaskTree*>(GetTree(iTaskTree))->ProcessPostPreRender();
}

void CTaskManager::ProcessPostPreRenderAfterAttachments(int iTaskTree)
{
	return static_cast<CTaskTree*>(GetTree(iTaskTree))->ProcessPostPreRenderAfterAttachments();
}

bool CTaskManager::ProcessMoveSignals(int iTaskTree)
{
	return static_cast<CTaskTree*>(GetTree(iTaskTree))->ProcessMoveSignals();
}

#if __ASSERT
bool CTaskManager::HandlesRagdoll(int iTaskTree, CPed* pPed)
{
	CTaskTree* pTree = static_cast<CTaskTree*>(GetTree(iTaskTree));

	for (s32 i=0; i<pTree->GetPriorityCount(); i++)
	{
		CTask* pTask = static_cast<CTask*>(pTree->GetTask(i));
		if (pTask && pTask->HandlesRagdoll(pPed))
			return true;
	}
	return false;
}
#endif //__ASSERT

/////////////////////////////////////////////////

CPedTaskManager::CPedTaskManager(CPed* pPed, int iNumberOfTaskTreesRequired)
: CTaskManager(iNumberOfTaskTreesRequired)
, m_pPed(pPed)
{

}

CPedTaskManager::~CPedTaskManager()
{

}

void CPedTaskManager::OnTaskChanged()
{
	if(m_pPed)
	{
		// force intelligence update next frame if outside of the ped's update
		m_pPed->GetPedAiLod().SetTaskSetNeedIntelligenceUpdate(true);
	}
}

/////////////////////////////////////////////////

CVehicleTaskManager::CVehicleTaskManager(CVehicle* pVehicle)
: CTaskManager(VEHICLE_TASK_TREE_MAX, true)
, m_PrimaryTaskTree(pVehicle, VEHICLE_TASK_PRIORITY_MAX)
, m_SecondaryTaskTree(pVehicle, VEHICLE_TASK_SECONDARY_MAX)
, m_pVehicle(pVehicle)
{
	SetExternallyOwnedTree(VEHICLE_TASK_TREE_PRIMARY, &m_PrimaryTaskTree);
	SetExternallyOwnedTree(VEHICLE_TASK_TREE_SECONDARY, &m_SecondaryTaskTree);
}

CVehicleTaskManager::~CVehicleTaskManager()
{

}

void CVehicleTaskManager::OnTaskChanged()
{
	if(m_pVehicle)
	{
		// force intelligence update next frame
		m_pVehicle->m_nVehicleFlags.bLodForceUpdateThisTimeslice = true;
	}
}

/////////////////////////////////////////////////
