
#include "objects/ObjectIntelligence.h"

// Rage includes
#include "profile/page.h"
#include "profile/group.h"

// Game includes
#include "debug/DebugScene.h"
#include "objects/object.h"
#include "task/System/TaskTree.h"

// Task includes
#include "task/Animation/TaskAnims.h"

AI_OPTIMISATIONS()

PF_PAGE(GTA_ObjectAI, "GTA Object AI");
PF_GROUP(GTA_OI);
PF_LINK(GTA_ObjectAI, GTA_OI);
PF_TIMER(OI_Process, GTA_OI);
PF_TIMER(OI_Process_Tasks, GTA_OI);
PF_TIMER(OI_ProcessPhysics, GTA_OI);
PF_TIMER(OI_ProcessPostMovement, GTA_OI);

FW_INSTANTIATE_CLASS_POOL(CObjectIntelligence, CONFIGURED_FROM_FILE, atHashString("ObjectIntelligence",0xb97089de));


CObjectIntelligence::CObjectIntelligence(CObject* pObject) : m_pObject(pObject), m_taskManager(OBJECT_TASK_TREE_MAX)
{
	Init();
}

CObjectIntelligence::~CObjectIntelligence()
{
}


void CObjectIntelligence::Init()
{
	GetTaskManager()->SetTree(OBJECT_TASK_TREE_PRIMARY,   rage_new CTaskTree(m_pObject, OBJECT_TASK_PRIORITY_MAX));
	GetTaskManager()->SetTree(OBJECT_TASK_TREE_SECONDARY, rage_new CTaskTree(m_pObject, OBJECT_TASK_SECONDARY_MAX));
	m_bForcePostCameraTaskUpdate = false;
}

void CObjectIntelligence::Process()
{
	PF_FUNC(OI_Process);

	// Debug update
	DEV_ONLY(Process_Debug();)

	//Compute all events.
	//GetEventScanner()->ScanForEvents(*m_pObject);

	//Handle the events by computing event response tasks.
	//GetEventHandler()->HandleEvents();

	// Update the AI tasks
	Process_Tasks();
}

void CObjectIntelligence::Process_Tasks()
{
	// Don't process tasks for objects that are controlled by a remote machine
	if (m_pObject->IsNetworkClone())
		return;

	PF_FUNC(OI_Process_Tasks);

	// Perform the designated task.
	m_taskManager.Process(fwTimer::GetTimeStep());
}

bool CObjectIntelligence::ProcessPhysics(float fTimeStep, int nTimeSlice)
{
	// Don't process tasks for objects that are controlled by a remote machine
	if (m_pObject->IsNetworkClone())
		return false;

	PF_FUNC(OI_ProcessPhysics);

	// Perform the designated task.
	return m_taskManager.ProcessPhysics(fTimeStep, nTimeSlice);
}

void CObjectIntelligence::ProcessPostMovement()
{
	PF_FUNC(OI_ProcessPostMovement);

	m_taskManager.ProcessPostMovement(OBJECT_TASK_PRIORITY_PRIMARY);
}

void CObjectIntelligence::ProcessPostCamera()
{
	// Don't process tasks for objects that are controlled by a remote machine
	if (m_pObject->IsNetworkClone())
		return;

	if(m_bForcePostCameraTaskUpdate )
	{
		Process();
	}

	m_bForcePostCameraTaskUpdate = false;
}

void CObjectIntelligence::SetTask(aiTask* pTask, u32 const tree /*= OBJECT_TASK_TREE_PRIMARY*/, u32 const priority /* = OBJECT_TASK_PRIORITY_PRIMARY*/)
{
	m_taskManager.SetTask(tree, pTask, priority, false);
}

#if __DEV

void CObjectIntelligence::Process_Debug()
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnProcessIntelligenceOfFocusEntity(), m_pObject );
}

void CObjectIntelligence::PrintTasks()
{	
	// Print the task hierarchy for this object
	Printf("Task hierarchy\n");
	CTask* pActiveTask = GetTaskActive();
	if(pActiveTask)
	{
		CTask* pTaskToPrint = pActiveTask;
		while(pTaskToPrint)
		{
			Printf("name: %s\n", (const char*) pTaskToPrint->GetName());
			pTaskToPrint=pTaskToPrint->GetSubTask();
		}
	}
}

template<> void fwPool<CObjectIntelligence>::PoolFullCallback() 
{
	s32 size = GetSize();
	int iIndex = 0;
	while(size--)
	{
		CObjectIntelligence* pIntelligence = GetSlot(size);
		if(pIntelligence)
		{
			Displayf("%i, 0x%p, Object: 0x%p, ObjectName: %s",
				iIndex,
				pIntelligence,
				pIntelligence->GetObject(),
				pIntelligence->GetObject() ? pIntelligence->GetObject()->GetDebugName() : "unknown");
		}
		else
		{
			Displayf("%i, NULL intelligence", iIndex);
		}
		iIndex++;
	}
}
#endif // __DEV

