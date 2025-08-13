// Title	:	QueriableInterface.h
// Author	:	Phil Hooker
// Started	:	25/07/06
//
// Implements a small queriable set of ped intelligence interface, thats compact enough to be sent between
// machines during network games.  All external ped queries (originating from another ped or an independent system)in 
// the game should be using this interface.
#include "Peds\QueriableInterface.h"

// Framework headers
#include "fwscene/search/Search.h"

//Game headers
#include "Objects/DummyObject.h"
#include "Peds\Ped.h"
#include "Peds\PedIntelligence.h"
#include "Peds\PedTaskRecord.h"

#include "scene/world/GameWorld.h"
#include "Task/Animation/TaskMoVEScripted.h"
#include "Task\Combat\Cover\TaskCover.h"
#include "Task\Combat\Cover\TaskStayInCover.h"
#include "Task\Combat\TaskCombat.h"
#include "Task\Combat\TaskNewCombat.h"
#include "Task\Combat\TaskDamageDeath.h"
#include "Task\Combat\TaskThreatResponse.h"
#include "Task/Combat/TaskSharkAttack.h"
#include "Task/Combat/Subtasks/TaskHeliChase.h"
#include "Task/Combat/Subtasks/TaskVariedAimPose.h"
#include "task\Default\Patrol\TaskPatrol.h"
#include "Task\Default\TaskAmbient.h"
#include "Task/Default/TaskArrest.h"
#include "Task/Default/TaskCuffed.h"
#include "Task/Default/TaskIncapacitated.h"
#include "Task/Default/TaskSlopeScramble.h"
#include "task/Default/TaskUnalerted.h"
#include "Task\Default\TaskWander.h"
#include "Task\General\TaskBasic.h"
#include "Task\General\TaskGeneralSweep.h"
#include "Task\General\TaskSecondary.h"
#include "Task/Movement/Jumping/TaskFallGetUp.h"
#include "Task/Movement/Jumping/TaskJump.h"
#include "Task/Movement/Climbing/TaskRappel.h"
#include "Task/Movement/Climbing/TaskVault.h"
#include "Task/Movement/Climbing/TaskDropDown.h"
#include "Task/Movement/Climbing/TaskLadderClimb.h"
#include "Task/Movement/TaskCollisionResponse.h"
#include "Task/Movement/TaskCrawl.h"
#include "Task/Movement/TaskGetUp.h"
#include "Task/Movement/TaskGoToPoint.h"
#include "Task/Movement/TaskGotoPointAiming.h"
#include "Task/Movement/TaskParachute.h"
#include "Task/Movement/TaskParachuteObject.h"
#include "Task/Movement/TaskJetpack.h"
#include "Task/Movement/TaskTakeOffPedVariation.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "Task/Movement/TaskSlideToCoord.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Physics/TaskAnimatedAttach.h"
#include "Task/Physics/TaskRageRagdoll.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskNMBalance.h"
#include "Task/Physics/TaskNMBrace.h"
#include "Task/Physics/TaskNMBuoyancy.h"
#include "Task/Physics/TaskNMElectrocute.h"
#include "Task/Physics/TaskNMExplosion.h"
#include "Task/Physics/TaskNMFallDown.h"
#include "Task/Physics/TaskNMFlinch.h"
#include "Task/Physics/TaskNMHighFall.h"
#include "Task/Physics/TaskNMJumpRollFromRoadVehicle.h"
#include "Task/Physics/TaskNMOnFire.h"
#include "Task/Physics/TaskNMPose.h"
#include "Task/Physics/TaskNMPrototype.h"
#include "Task/Physics/TaskNMRelax.h"
#include "Task/Physics/TaskNMRiverRapids.h"
#include "Task/Physics/TaskNMRollUpAndRelax.h"
#include "Task/Physics/TaskNMShot.h"
#include "Task/Physics/TaskNMSit.h"
#include "Task/Physics/TaskNMThroughWindscreen.h"
#include "Task/Physics/TaskNMInjuredOnGround.h"
#include "Task/Physics/TaskNMSimple.h"
#include "Task/Response/TaskDiveToGround.h"
#include "Task/Response/TaskReactToExplosion.h"
#include "Task/Animation/TaskAnims.h"
#include "Task/Service/Medic/TaskAmbulancePatrol.h"
#include "Task/System/TaskClassInfo.h"
#include "task/Scenario/ScenarioPoint.h"
#include "Task/Scenario/Types/TaskCowerScenario.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Task/Scenario/Types/TaskVehicleScenario.h"
#include "Task/Scenario/Types/TaskWanderingScenario.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Vehicle/TaskMountAnimal.h"
#include "Task/Vehicle/TaskMountAnimalWeapon.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "Task/Weapons/TaskProjectile.h"
#include "Task/Weapons/TaskBomb.h"
#include "Task/Weapons/Gun/TaskReloadGun.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Task/Weapons/Gun/TaskAimGunOnFoot.h"
#include "Task/Weapons/Gun/TaskAimGunScripted.h"
#include "Task/Weapons/Gun/TaskVehicleDriveby.h"
#include "Task/Weapons/TaskSwapWeapon.h"
#include "Task/Weapons/TaskWeaponBlocked.h"
#include "Task/System/TaskTree.h"
#include "Task/response/TaskReactAndFlee.h"
#include "Task/response/TaskFlee.h"
#include "Task/Service/Police/TaskPolicePatrol.h"
#include "Task/Service/Swat/TaskSwat.h"
#include "Task/Animation/TaskScriptedAnimation.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/Combat/TaskSharkAttack.h"
#include "Task/Combat/TaskReact.h"
#include "Task/Movement/TaskFall.h"
#include "Task/Combat/TaskWrithe.h"
#include "Task/Combat/Subtasks/TaskPlaneChase.h"

#include "Network/NetworkInterface.h"
#include "Network/General/NetworkUtil.h"


#define ALIGNTO(x, a)					(((x) + ((a) - 1)) & ~((a) - 1))
#define LARGEST_TASKINFO_CLASS (sizeof(CClonedControlVehicleInfo))
#define MAX_NUM_SEQUENCE_INFOS 1200

INSTANTIATE_POOL_ALLOCATOR(CTaskInfo);
FW_INSTANTIATE_BASECLASS_POOL(TaskSequenceInfo, MAX_NUM_SEQUENCE_INFOS, atHashString("TaskSequenceInfo",0x698208cf), sizeof(TaskSequenceInfo));

AI_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

//-------------------------------------------------------------------------
// CSerialisedFSMTaskInfo
//-------------------------------------------------------------------------
void CSerialisedFSMTaskInfo::Serialise(CSyncDataBase& serialiser)
{
	if(HasState())
	{
		SERIALISE_TASK_STATE(serialiser, GetTaskType(), m_iState, GetSizeOfState(), "State");
	}
}

void CSerialisedFSMTaskInfo::ReadSpecificTaskInfo(datBitBuffer &messageBuffer)
{
	CSyncDataReader serialiser(messageBuffer, 0);
	Serialise(serialiser);
}

// read/write/compare/log functions for network code
void CSerialisedFSMTaskInfo::WriteSpecificTaskInfo(datBitBuffer &messageBuffer)
{
	CSyncDataWriter serialiser(messageBuffer, 0);
	Serialise(serialiser);
}

// read/write/compare/log functions for network code
void CSerialisedFSMTaskInfo::LogSpecificTaskInfo(netLoggingInterface &log)
{
	CSyncDataLogger serialiser(&log);
	Serialise(serialiser);
}

u32 CSerialisedFSMTaskInfo::GetSpecificTaskInfoSize()
{
	CSyncDataSizeCalculator serialiser;
	Serialise(serialiser);
	return serialiser.GetSize();
}

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
TaskSequenceInfo::TaskSequenceInfo(u32 taskType) :
m_taskType          (taskType),
m_sequenceID        (0),
m_nextSequenceInfo  (0)
{
}

#if __BANK
void TaskSequenceInfo::PoolFullCallback(void* pItem)
{
	if (!pItem)
	{
		Displayf("ERROR - TaskSequenceInfo Pool FULL!\n");
	}
	else
	{
		TaskSequenceInfo* pTaskSequenceInfo = static_cast<TaskSequenceInfo*>(pItem);

		Displayf("Task type num %d, Task type name - %s, Task Sequence ID - %d, Memory Address %p", 
			pTaskSequenceInfo->m_taskType,
			static_cast<const char *>(TASKCLASSINFOMGR.GetTaskName(pTaskSequenceInfo->m_taskType)),
			pTaskSequenceInfo->m_sequenceID,
			pTaskSequenceInfo);
	}
}
#endif



//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
CTaskInfo::CTaskInfo()
{
	m_eTaskType = CTaskTypes::TASK_NONE;
	m_pNextTask = NULL;
	m_pPrevTask = NULL;
	m_iTaskPriority = 4;
    m_iTaskTreeDepth = 0;
	m_bActiveTask = TRUE;
}

//-------------------------------------------------------------------------
// Returns if the task is temporary (in a temporary task slot)
//-------------------------------------------------------------------------
bool CTaskInfo::GetIsTemporaryTask( void ) const
{
	return m_iTaskPriority < PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP;
}

//-------------------------------------------------------------------------
// Reads any task specific properties from the supplied message buffer
//-------------------------------------------------------------------------
void CTaskInfo::ReadSpecificTaskInfo(datBitBuffer &UNUSED_PARAM(messageBuffer))
{
    // no specific info for this base class
}

//-------------------------------------------------------------------------
// Writes any task specific properties to the supplied message buffer
//-------------------------------------------------------------------------
void CTaskInfo::WriteSpecificTaskInfo(datBitBuffer &UNUSED_PARAM(messageBuffer))
{
    // no specific info for this base class
}

//-------------------------------------------------------------------------
// Logs any task specific properties to the specified log file
//-------------------------------------------------------------------------
void CTaskInfo::LogSpecificTaskInfo(netLoggingInterface &UNUSED_PARAM(log))
{
    // no specific info for this base class
}

//-------------------------------------------------------------------------
// Gets the size in bits used in a message buffer of any task specific properties
//-------------------------------------------------------------------------
u32 CTaskInfo::GetSpecificTaskInfoSize()
{
    // no specific info for this base class
    return 0;
}

CTask* CTaskInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity)) 
{
	return 0; 
};

#if !__NO_OUTPUT && !__FINAL
void CTaskInfo::PoolFullCallback(void* pItem)
{
	if (!pItem)
	{
		Displayf("ERROR - CTaskInfo Pool FULL!\n");
	}
	else
	{
		CTaskInfo* pTaskInfo = static_cast<CTaskInfo*>(pItem);

		Displayf("Task Info type %d, Task Info name - %s, Task Info Active - %d, Task Info Priority - %d, Task Info Tree Depth = %d, Task Info Target = %s, Task Info Address %p", 
			pTaskInfo->GetTaskInfoType(),
			static_cast<const char *>(TASKCLASSINFOMGR.GetTaskName(pTaskInfo->GetTaskInfoType())),
			pTaskInfo->GetIsActiveTask(),
			pTaskInfo->GetTaskPriority(),
			pTaskInfo->GetTaskTreeDepth(),
			(pTaskInfo->GetTarget() ? pTaskInfo->GetTarget()->GetModelName() : "?"),
			pTaskInfo);
	}
}
#endif

//-------------------------------------------------------------------------
// Target syncing helper class (used in multiple tasks)
//-------------------------------------------------------------------------

CClonedTaskInfoWeaponTargetHelper::CClonedTaskInfoWeaponTargetHelper()
:
	m_bHasTargetEntity(false),
	m_bHasTargetPosition(false),
	m_pTargetEntity(NULL),
	m_vTargetPosition(VEC3_ZERO)		
{}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------

CClonedTaskInfoWeaponTargetHelper::CClonedTaskInfoWeaponTargetHelper(CAITarget const* target)
{
	Clear();

	if(target && target->GetIsValid())
	{
		s32 nTargetMode = target->GetMode();

		if(NetworkUtils::GetNetworkObjectFromEntity(target->GetEntity()))
		{
			if( ( nTargetMode == CAITarget::Mode_Entity ) || ( nTargetMode == CAITarget::Mode_EntityAndOffset ) )
			{
				m_bHasTargetEntity = true; 
				m_pTargetEntity.SetEntity((CEntity*)target->GetEntity());
			}

			if( ( nTargetMode == CAITarget::Mode_WorldSpacePosition ) || ( nTargetMode == CAITarget::Mode_EntityAndOffset ) )
			{
				m_bHasTargetPosition = true; 
				target->GetPosition(m_vTargetPosition);
			}
		}
		else
		{			
			if( target->GetEntity() && ( nTargetMode == CAITarget::Mode_Entity || nTargetMode == CAITarget::Mode_EntityAndOffset ) )
			{
				// we're aiming at an entity but we don't have a networked target so we change the targeting to aiming at a world space position....
				m_bHasTargetPosition = true; 
				m_vTargetPosition = VEC3V_TO_VECTOR3( target->GetEntity()->GetTransform().GetPosition() );
			}
			else if( nTargetMode == CAITarget::Mode_WorldSpacePosition ) 
			{
				m_bHasTargetPosition = true; 
				target->GetPosition(m_vTargetPosition);
			}
		}
	}
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------

void CClonedTaskInfoWeaponTargetHelper::ReadQueriableState(CAITarget& target)
{
	// Set the target up...
	UpdateTarget(target);

	// if the cloned info says we've got a target...
	if(HasTargetEntity())
	{
		// but we don't
		if(!target.GetEntity())
		{
			// then make sure we've received an ID and the target is just out of scope on the clone...
			Assert(NETWORK_INVALID_OBJECT_ID != GetTargetEntityId());
		}
	}		
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------

void CClonedTaskInfoWeaponTargetHelper::UpdateTarget(CAITarget& target)
{
	// the target the owner is aiming at may not exist on this machine....
	if(HasTargetEntity())
	{
		if(!m_pTargetEntity.GetEntity())
		{
			// Check we've at least got an ID sent over so it's just the target out of scope on the clone...
			Assert(NETWORK_INVALID_OBJECT_ID != GetTargetEntityId());
		}
	}

	if(m_bHasTargetEntity && m_bHasTargetPosition && m_pTargetEntity.GetEntity())
		target.SetEntityAndOffset(m_pTargetEntity.GetEntity(), m_vTargetPosition);
	else if(m_bHasTargetEntity && m_pTargetEntity.GetEntity())
		target.SetEntity(m_pTargetEntity.GetEntity());
	else if(m_bHasTargetPosition)
		target.SetPosition(m_vTargetPosition);		
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------

void CClonedTaskInfoWeaponTargetHelper::Clear()
{
	m_bHasTargetEntity = false;
	m_bHasTargetPosition = false;
	m_pTargetEntity = NULL;
	m_vTargetPosition = VEC3_ZERO;
}

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
CQueriableInterface::CQueriableInterface()
:m_pTaskInfoFirst(NULL),
m_pTaskInfoLast(NULL),
m_pFirstTaskSequenceInfo(NULL),
m_iScriptCommand(SCRIPT_TASK_INVALID),
m_iScriptTaskStage(CPedScriptedTaskRecordData::VACANT_STAGE)
{
#if __ASSERT
	m_CachedInfoMayBeOutOfDate = false;
#endif
}

//-------------------------------------------------------------------------
// Destructor
//-------------------------------------------------------------------------
CQueriableInterface::~CQueriableInterface()
{
	ResetTaskInfo();
    ResetTaskSequenceInfo();
}

//-------------------------------------------------------------------------
// Resets all currently stored data
//-------------------------------------------------------------------------
void CQueriableInterface::Reset( void )
{
	ResetTaskInfo();

	m_CachedHostileTarget = NULL;
#if __ASSERT
	m_CachedInfoMayBeOutOfDate = false;	// Shouldn't be out of date since we manually cleared it above.
#endif
}



//-------------------------------------------------------------------------
// Wrapper function to estimate the target for an attacking character,
// returns NULL if not considered to be attacking
//-------------------------------------------------------------------------
CEntity* CQueriableInterface::FindHostileTarget()
{
	// TODO make this work with the new combat task
 	if( IsTaskCurrentlyRunning( CTaskTypes::TASK_COMBAT ) )
 	{
 		return GetTargetForTaskType( CTaskTypes::TASK_COMBAT );
 	}
	return NULL;
}

//-------------------------------------------------------------------------
// Updates any data we have cached for performance reasons
//-------------------------------------------------------------------------
void CQueriableInterface::UpdateCachedInfo()
{
	m_CachedHostileTarget = FindHostileTarget();

#if __ASSERT
	m_CachedInfoMayBeOutOfDate = false;
#endif
}


//-------------------------------------------------------------------------
// Resets all currently stored data
//-------------------------------------------------------------------------
void CQueriableInterface::ResetTaskInfo( void )
{
	while( m_pTaskInfoFirst )
	{
		CTaskInfo * pInfoToDelete = m_pTaskInfoFirst;
		m_pTaskInfoFirst = pInfoToDelete->GetNextTaskInfo();
		delete pInfoToDelete;
		if( m_pTaskInfoFirst )
		{
			m_pTaskInfoFirst->SetPrevTaskInfo(NULL);
		}
	}
	m_pTaskInfoFirst = NULL;
	m_pTaskInfoLast = NULL;

#if __ASSERT
	m_CachedInfoMayBeOutOfDate = true;
#endif
}

//-------------------------------------------------------------------------
// Resets all currently stored data
//-------------------------------------------------------------------------
void CQueriableInterface::ResetScriptTaskInfo( void )
{
	m_iScriptCommand   = SCRIPT_TASK_INVALID;
	m_iScriptTaskStage = CPedScriptedTaskRecordData::VACANT_STAGE;
}

//-------------------------------------------------------------------------
// Adds information about a particular task
//-------------------------------------------------------------------------
void CQueriableInterface::AddTaskInformation( CTask* pTask, const bool bActive, const s32 iTaskPriority, fwEntity* pEntity )
{
	CTaskInfo* pNewTaskInfo = GenerateTaskSpecificInfo( pTask, pEntity );

	if (pNewTaskInfo)
	{
		// Set any general status flags
		pNewTaskInfo->SetIsActiveTask( bActive );
		pNewTaskInfo->SetTaskPriority( iTaskPriority );

        // Set the network sequence for the new task
        if(pNewTaskInfo->HasNetSequences() && NetworkInterface::IsGameInProgress())
        {
            if(pTask->IsClonedFSMTask())
	        {
                CTaskFSMClone *pTaskFSMClone = SafeCast(CTaskFSMClone, pTask);

                if(pTaskFSMClone->GetNetSequenceID() == INVALID_TASK_SEQUENCE_ID)
                {
                    TaskSequenceInfo *taskSequenceInfo = GetTaskSequenceInfo(pTask->GetTaskType());

                    u32 netSequenceID = 0;

                    if(taskSequenceInfo == 0)
                    {
                        taskSequenceInfo = AddTaskSequenceInfo(pTask->GetTaskType());
                        Assert(taskSequenceInfo);
                    }
                    else
                    {
						// handle wrapping of the sequence id
						if (taskSequenceInfo->m_sequenceID == MAX_TASK_SEQUENCE_ID)
						{
							taskSequenceInfo->m_sequenceID = 0;
						}
						else
						{
							taskSequenceInfo->m_sequenceID++;
						}

                        netSequenceID = taskSequenceInfo->m_sequenceID;
                    }

                    pTaskFSMClone->SetNetSequenceID(netSequenceID);
                }

                pNewTaskInfo->SetNetSequenceID(pTaskFSMClone->GetNetSequenceID());
	        }
        }

		// Add the task info to the linked list
		AddTaskInfo(pNewTaskInfo);
	}
}

//-------------------------------------------------------------------------
// Returns the task info if the task is present in the ped at teh priority
//-------------------------------------------------------------------------
bool CQueriableInterface::IsTaskPresentAtPriority( s32 iTaskType, u32 iPriority ) const
{
	CTaskInfo* pNextTaskInfo = m_pTaskInfoFirst;
	while( pNextTaskInfo )
	{
		if( pNextTaskInfo->GetTaskType() == iTaskType &&
			pNextTaskInfo->GetTaskPriority() == iPriority )
		{
			return true;
		}
		pNextTaskInfo = pNextTaskInfo->GetNextTaskInfo();
	}
	return false;
}

//-------------------------------------------------------------------------
// Returns the task info if the task is currently running on the ped
//-------------------------------------------------------------------------
CTaskInfo* CQueriableInterface::GetTaskInfoForTaskType( s32 iTaskType, u32 iPriority, bool bAssertIfNotFound ) const
{
	CTaskInfo* pNextTaskInfo = m_pTaskInfoFirst;
	bool bFound = false;

	while( pNextTaskInfo )
	{
		if( pNextTaskInfo->GetTaskType() == iTaskType)
		{
			bFound = true;

			if (iPriority == PED_TASK_PRIORITY_MAX || iPriority == pNextTaskInfo->GetTaskPriority())
			{
				return pNextTaskInfo;
			}
		}

		pNextTaskInfo = pNextTaskInfo->GetNextTaskInfo();
	}

	if (bAssertIfNotFound)
	{
		if (bFound)
		{
			Assertf(0, "Could not find task info for task type %d with priority %d", iTaskType, iPriority);
		}
		else
		{
			Assertf(0, "Could not find task info for task type %d", iTaskType);
		}
	}

	return NULL;
}


//-------------------------------------------------------------------------
// Returns the task info if the task is currently running on the ped
//-------------------------------------------------------------------------
const CEntity* CQueriableInterface::GetTargetForTaskType( s32 iTaskType, u32 iPriority ) const
{
	CTaskInfo* pTaskInfo = GetTaskInfoForTaskType( iTaskType, iPriority );

	if (AssertVerify(pTaskInfo))
	{
		return pTaskInfo->GetTarget();
	}

	return NULL;
}

CEntity* CQueriableInterface::GetTargetForTaskType( s32 iTaskType, u32 iPriority )
{
	CTaskInfo* pTaskInfo = GetTaskInfoForTaskType( iTaskType, iPriority );

	if (AssertVerify(pTaskInfo))
	{
		return pTaskInfo->GetTarget();
	}

	return NULL;
}

//-------------------------------------------------------------------------
// Returns the status stored with this task
//-------------------------------------------------------------------------
s32 CQueriableInterface::GetStateForTaskType( s32 iTaskType , u32 iPriority) const
{
	CTaskInfo* pTaskInfo = GetTaskInfoForTaskType( iTaskType, iPriority );
	
	if (AssertVerify(pTaskInfo))
	{
		return pTaskInfo->GetState();
	}

	return -1;
}


//-------------------------------------------------------------------------
// Returns the scenario type stored with this task
//-------------------------------------------------------------------------
s32	CQueriableInterface::GetScenarioTypeFromTaskInfo(CTaskInfo& taskInfo)
{
	Assertf((taskInfo.GetTaskInfoType() == CTaskInfo::INFO_TYPE_USE_SCENARIO) ||
			(taskInfo.GetTaskInfoType() >= CTaskInfo::INFO_TYPE_SCENARIO && taskInfo.GetTaskInfoType() < CTaskInfo::INFO_TYPE_SCENARIO_END),
			"Task Info Type %d, Task Type %d", taskInfo.GetTaskInfoType(), taskInfo.GetTaskType());

	CClonedScenarioInfo* scenarioInfo1 = static_cast<CClonedScenarioInfo*>(&taskInfo);
	CClonedScenarioFSMInfoBase* scenarioInfo = static_cast<CClonedScenarioFSMInfoBase*>(scenarioInfo1);
	Assert(scenarioInfo == dynamic_cast<CClonedScenarioFSMInfoBase*>(&taskInfo));
	return scenarioInfo->GetScenarioType();
}


//-------------------------------------------------------------------------
// Returns the scenario point stored with this task
//-------------------------------------------------------------------------
const CScenarioPoint* CQueriableInterface::GetScenarioPointFromTaskInfo(CTaskInfo& taskInfo)
{
	Assertf((taskInfo.GetTaskInfoType() == CTaskInfo::INFO_TYPE_USE_SCENARIO) ||
			(taskInfo.GetTaskInfoType() >= CTaskInfo::INFO_TYPE_SCENARIO && taskInfo.GetTaskInfoType() < CTaskInfo::INFO_TYPE_SCENARIO_END),
			"Task Info Type %d, Task Type %d", taskInfo.GetTaskInfoType(), taskInfo.GetTaskType());

	CClonedScenarioInfo* scenarioInfo1 = static_cast<CClonedScenarioInfo*>(&taskInfo);
	CClonedScenarioFSMInfoBase* scenarioInfo = static_cast<CClonedScenarioFSMInfoBase*>(scenarioInfo1);
	Assert(scenarioInfo == dynamic_cast<CClonedScenarioFSMInfoBase*>(&taskInfo));
	return scenarioInfo->GetScenarioPoint();
}

//-------------------------------------------------------------------------
// Wrapper function to get any running scenario type
//-------------------------------------------------------------------------
s32  CQueriableInterface::GetRunningScenarioType(const bool bMustBeRunning)
{
	// These scenario tasks currently don't seem to be in use and may end up getting removed:
	Assert(!FindTaskInfoIfCurrentlyRunning( CTaskTypes::TASK_MOVE_BETWEEN_POINTS_SCENARIO, bMustBeRunning ));
	Assert(!FindTaskInfoIfCurrentlyRunning( CTaskTypes::TASK_WANDERING_IN_RADIUS_SCENARIO, bMustBeRunning ));
	Assert(!FindTaskInfoIfCurrentlyRunning( CTaskTypes::TASK_PARKED_VEHICLE_SCENARIO, bMustBeRunning ));

	CTaskInfo* pInfo;

	pInfo = FindTaskInfoIfCurrentlyRunning(CTaskTypes::TASK_WANDERING_SCENARIO, bMustBeRunning);
	if(pInfo)
	{
		return GetScenarioTypeFromTaskInfo(*pInfo);
	}
	pInfo = FindTaskInfoIfCurrentlyRunning(CTaskTypes::TASK_CHAT_SCENARIO, bMustBeRunning);
	if(pInfo)
	{
		return GetScenarioTypeFromTaskInfo(*pInfo);
	}
	pInfo = FindTaskInfoIfCurrentlyRunning(CTaskTypes::TASK_USE_SCENARIO, bMustBeRunning);
	if(pInfo)
	{
		return GetScenarioTypeFromTaskInfo(*pInfo);
	}
	pInfo = FindTaskInfoIfCurrentlyRunning(CTaskTypes::TASK_USE_VEHICLE_SCENARIO, bMustBeRunning);
	if(pInfo)
	{
		return GetScenarioTypeFromTaskInfo(*pInfo);
	}

	//pInfo = FindTaskInfoIfCurrentlyRunning(CTaskTypes::TASK_MOVE_BETWEEN_POINTS_SCENARIO, bMustBeRunning);
	//if(pInfo)
	//{
	//	return GetScenarioTypeFromTaskInfo(*pInfo);
	//}
	//pInfo = FindTaskInfoIfCurrentlyRunning(CTaskTypes::TASK_WANDERING_IN_RADIUS_SCENARIO, bMustBeRunning);
	//if(pInfo)
	//{
	//	return GetScenarioTypeFromTaskInfo(*pInfo);
	//}
	//pInfo = FindTaskInfoIfCurrentlyRunning(CTaskTypes::TASK_COMPLEX_DRIVING_SCENARIO, bMustBeRunning);
	//if(pInfo)
	//{
	//	return GetScenarioTypeFromTaskInfo(*pInfo);
	//}
	//pInfo = FindTaskInfoIfCurrentlyRunning(CTaskTypes::TASK_CONTROL_SCENARIO, bMustBeRunning);
	//if(pInfo)
	//{
	//	return GetScenarioTypeFromTaskInfo(*pInfo);
	//}
	//pInfo = FindTaskInfoIfCurrentlyRunning(CTaskTypes::TASK_PARKED_VEHICLE_SCENARIO, bMustBeRunning);
	//if(pInfo)
	//{
	//	return GetScenarioTypeFromTaskInfo(*pInfo);
	//}

	return Scenario_Invalid;
}

//-------------------------------------------------------------------------
// Wrapper function to get any running scenario point
//-------------------------------------------------------------------------
CScenarioPoint const* CQueriableInterface::GetRunningScenarioPoint(const bool bMustBeRunning)
{
	// These scenario tasks currently don't seem to be in use and may end up getting removed:
	Assert(!FindTaskInfoIfCurrentlyRunning( CTaskTypes::TASK_MOVE_BETWEEN_POINTS_SCENARIO, bMustBeRunning ));
	Assert(!FindTaskInfoIfCurrentlyRunning( CTaskTypes::TASK_WANDERING_IN_RADIUS_SCENARIO, bMustBeRunning ));
	Assert(!FindTaskInfoIfCurrentlyRunning( CTaskTypes::TASK_PARKED_VEHICLE_SCENARIO, bMustBeRunning ));

	CTaskInfo* pInfo;

	pInfo = FindTaskInfoIfCurrentlyRunning(CTaskTypes::TASK_WANDERING_SCENARIO, bMustBeRunning);
	if(pInfo)
	{
		return GetScenarioPointFromTaskInfo(*pInfo);
	}
	pInfo = FindTaskInfoIfCurrentlyRunning(CTaskTypes::TASK_CHAT_SCENARIO, bMustBeRunning);
	if(pInfo)
	{
		return GetScenarioPointFromTaskInfo(*pInfo);
	}
	pInfo = FindTaskInfoIfCurrentlyRunning(CTaskTypes::TASK_USE_SCENARIO, bMustBeRunning);
	if(pInfo)
	{
		return GetScenarioPointFromTaskInfo(*pInfo);
	}
	pInfo = FindTaskInfoIfCurrentlyRunning(CTaskTypes::TASK_USE_VEHICLE_SCENARIO, bMustBeRunning);
	if(pInfo)
	{
		return GetScenarioPointFromTaskInfo(*pInfo);
	}

	//pInfo = FindTaskInfoIfCurrentlyRunning(CTaskTypes::TASK_MOVE_BETWEEN_POINTS_SCENARIO, bMustBeRunning);
	//if(pInfo)
	//{
	//	return GetScenarioPointFromTaskInfo(*pInfo);
	//}
	//pInfo = FindTaskInfoIfCurrentlyRunning(CTaskTypes::TASK_WANDERING_IN_RADIUS_SCENARIO, bMustBeRunning);
	//if(pInfo)
	//{
	//	return GetScenarioPointFromTaskInfo(*pInfo);
	//}
	//pInfo = FindTaskInfoIfCurrentlyRunning(CTaskTypes::TASK_COMPLEX_DRIVING_SCENARIO, bMustBeRunning);
	//if(pInfo)
	//{
	//	return GetScenarioPointFromTaskInfo(*pInfo);
	//}
	//pInfo = FindTaskInfoIfCurrentlyRunning(CTaskTypes::TASK_CONTROL_SCENARIO, bMustBeRunning);
	//if(pInfo)
	//{
	//	return GetScenarioPointFromTaskInfo(*pInfo);
	//}
	//pInfo = FindTaskInfoIfCurrentlyRunning(CTaskTypes::TASK_PARKED_VEHICLE_SCENARIO, bMustBeRunning);
	//if(pInfo)
	//{
	//	return GetScenarioPointFromTaskInfo(*pInfo);
	//}

	return NULL;
}

//-------------------------------------------------------------------------
// Returns the network synchronised scene ID
//-------------------------------------------------------------------------
int CQueriableInterface::GetNetworkSynchronisedSceneID() const
{
    int networkSceneID = -1;

    CTaskInfo *pTaskInfo = GetTaskInfoForTaskType(CTaskTypes::TASK_SYNCHRONIZED_SCENE, PED_TASK_PRIORITY_MAX);

    if(pTaskInfo)
    {
        networkSceneID = static_cast<CClonedSynchronisedSceneInfo *>(pTaskInfo)->GetNetworkSceneID();
    }

    return networkSceneID;
}

//-------------------------------------------------------------------------
// Returns the target entry point stored with this task
//-------------------------------------------------------------------------
s32 CQueriableInterface::GetTargetEntryPointForTaskType( s32 iTaskType , u32 iPriority )
{
	s32 iEntryPoint = -1;

	CTaskInfo* pTaskInfo = GetTaskInfoForTaskType(iTaskType, iPriority);
	if(AssertVerify(pTaskInfo))
	{
		iEntryPoint = pTaskInfo->GetTargetEntryExitPoint(); // virtual
	}

	return iEntryPoint;
}

//-------------------------------------------------------------------------
// Internal function for adding a task info to the linked list
//-------------------------------------------------------------------------
void CQueriableInterface::AddTaskInfo(CTaskInfo *taskInfo)
{
    if(AssertVerify(taskInfo))
    {
        if( m_pTaskInfoLast != NULL )
	    {
		    m_pTaskInfoLast->SetNextTaskInfo(taskInfo);
		    taskInfo->SetPrevTaskInfo(m_pTaskInfoLast);
		    m_pTaskInfoLast = taskInfo;
	    }
	    else
	    {
		    Assert( m_pTaskInfoFirst == NULL );
		    m_pTaskInfoFirst = taskInfo;
		    m_pTaskInfoLast = taskInfo;
	    }
    }

#if __ASSERT
	m_CachedInfoMayBeOutOfDate = true;
#endif
}

//-------------------------------------------------------------------------
// Returns the progress of the sequence task
//-------------------------------------------------------------------------
s8	CQueriableInterface::GetSequenceProgressForTaskType( int iInRequestedProgress, s32 iTaskType, u32 iPriority )
{
	CTaskInfo* pTaskInfo = GetTaskInfoForTaskType( iTaskType, iPriority );

	Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_SEQUENCE ); 

	CClonedTaskSequenceInfo* pSeqTaskInfo = static_cast<CClonedTaskSequenceInfo*>(pTaskInfo);
	if( iInRequestedProgress == 0 )
	{
		return pSeqTaskInfo->GetPrimarySequenceProgress();
	}
	else if( iInRequestedProgress == 1 )
	{
		return pSeqTaskInfo->GetSubtaskSequenceProgress();
	}
	else
	{
		Assert(0);
		return -1;
	}
}


//-------------------------------------------------------------------------
// Internal function which sets any task specific data in the task information union
//-------------------------------------------------------------------------
CTaskInfo* CQueriableInterface::GenerateTaskSpecificInfo( CTask* pTaskIn, fwEntity* UNUSED_PARAM(pEntity) )
{
	CTaskInfo* pTaskInfo = pTaskIn->CreateQueriableState();

#if __ASSERT
	s32 taskType = pTaskIn->GetTaskType();
    // ensure all task types with associated clone tasks are synchronised over the network
    if(pTaskInfo && pTaskInfo->HasNetSequences())
    {
        // tasks can be sent over the network when synced as clone tasks or when given to remotely owned peds
        bool bIsSyncedTaskType = IsTaskSentOverNetwork(taskType) || CanTaskBeGivenToNonLocalPeds(taskType);

        Assertf(bIsSyncedTaskType || !NetworkInterface::IsGameInProgress(), "%s (%d) has net sequences but is not marked to be sent over the network!", static_cast<const char *>(TASKCLASSINFOMGR.GetTaskName(taskType)), taskType);
    }
#endif

    if (Verifyf( pTaskInfo, "Failed to create task info for task type: %s",  static_cast<const char *>(TASKCLASSINFOMGR.GetTaskName(pTaskIn->GetTaskType()))))
	{
		pTaskInfo->SetType(pTaskIn->GetTaskType());
	}

	return pTaskInfo;
}


//-------------------------------------------------------------------------
// Internal function which creates an empty task info structure for network code
//-------------------------------------------------------------------------
CTaskInfo *CQueriableInterface::CreateEmptyTaskInfo(u32 taskType, bool ASSERT_ONLY(bAssert))
{
    CTaskInfo* pTaskInfo = 0;

    switch(taskType)
    {
	case CTaskTypes::TASK_COVER:
		pTaskInfo = rage_new CClonedCoverInfo();
		break;
	case CTaskTypes::TASK_IN_COVER:
		pTaskInfo = rage_new CClonedUseCoverInfo();
		break;
	case CTaskTypes::TASK_ENTER_COVER:
		pTaskInfo = rage_new CClonedCoverIntroInfo();
		break;
	case CTaskTypes::TASK_MOTION_IN_COVER:
		pTaskInfo = rage_new CClonedMotionInCoverInfo();
		break;
    case CTaskTypes::TASK_ENTER_VEHICLE:
        pTaskInfo = rage_new CClonedEnterVehicleInfo();
        break;
    case CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE:
        pTaskInfo = rage_new CClonedEnterVehicleOpenDoorInfo();
        break;
    case CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE:
        pTaskInfo = rage_new CClonedSeatShuffleInfo();
        break;
    case CTaskTypes::TASK_EXIT_VEHICLE:
        pTaskInfo = rage_new CClonedExitVehicleInfo();
        break;
#if ENABLE_HORSE
	case CTaskTypes::TASK_MOUNT_ANIMAL:
		pTaskInfo = rage_new CClonedMountAnimalInfo();
		break;
	case CTaskTypes::TASK_DISMOUNT_ANIMAL:
		pTaskInfo = rage_new CClonedDismountAnimalInfo();
		break;
#endif
    case CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE:
        pTaskInfo = rage_new CClonedAimAndThrowProjectileInfo();
        break;
    case CTaskTypes::TASK_THROW_PROJECTILE:
        pTaskInfo = rage_new CClonedThrowProjectileInfo();
        break;
	case CTaskTypes::TASK_MOUNT_THROW_PROJECTILE:
        pTaskInfo = rage_new CClonedMountThrowProjectileInfo();
        break;
    case CTaskTypes::TASK_GUN:
        pTaskInfo = rage_new CClonedGunInfo();
        break;
	case CTaskTypes::TASK_AIM_GUN_BLIND_FIRE:
		pTaskInfo = rage_new CClonedAimGunBlindFireInfo();
		break;
	case CTaskTypes::TASK_AIM_GUN_ON_FOOT:
		pTaskInfo = rage_new CClonedAimGunOnFootInfo();
		break;
	case CTaskTypes::TASK_DYING_DEAD:
		pTaskInfo = rage_new CClonedDyingDeadInfo();
		break;
    case CTaskTypes::TASK_DAMAGE_ELECTRIC:
        pTaskInfo = rage_new CClonedDamageElectricInfo();
        break;
    case CTaskTypes::TASK_VEHICLE_GUN:
        pTaskInfo = rage_new CClonedVehicleGunInfo();
        break;
    case CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY:
        pTaskInfo = rage_new CClonedAimGunVehicleDriveByInfo();
        break;
	case CTaskTypes::TASK_FALL_OVER:
		pTaskInfo = rage_new CClonedFallOverInfo();
		break;
	case CTaskTypes::TASK_GET_UP:
		pTaskInfo = rage_new CClonedGetUpInfo();
		break;
	case CTaskTypes::TASK_FALL_AND_GET_UP:
		pTaskInfo = rage_new CClonedFallAndGetUpInfo();
		break;
	case CTaskTypes::TASK_NM_RELAX:			
		pTaskInfo = rage_new CClonedNMRelaxInfo();
		break;
	case CTaskTypes::TASK_NM_POSE:	
		pTaskInfo = rage_new CClonedNMPoseInfo();
		break;
	case CTaskTypes::TASK_NM_BRACE:	
		pTaskInfo = rage_new CClonedNMBraceInfo();
		break;
	case CTaskTypes::TASK_NM_BUOYANCY:	
		pTaskInfo = rage_new CClonedNMBuoyancyInfo();
		break;
	case CTaskTypes::TASK_NM_SHOT:	
		pTaskInfo = rage_new CClonedNMShotInfo();
		break;
	case CTaskTypes::TASK_NM_HIGH_FALL:	
		pTaskInfo = rage_new CClonedNMHighFallInfo();
		break;
	case CTaskTypes::TASK_NM_BALANCE:	
		pTaskInfo = rage_new CClonedNMBalanceInfo();
		break;
	case CTaskTypes::TASK_NM_ELECTROCUTE:
		pTaskInfo = rage_new CClonedNMElectrocuteInfo();
		break;
	case CTaskTypes::TASK_NM_EXPLOSION:	
		pTaskInfo = rage_new CClonedNMExposionInfo();
		break;
	case CTaskTypes::TASK_NM_ONFIRE:	
		pTaskInfo = rage_new CClonedNMOnFireInfo();
		break;
	case CTaskTypes::TASK_NM_JUMP_ROLL_FROM_ROAD_VEHICLE:	
		pTaskInfo = rage_new CClonedNMJumpRollInfo();
		break;
	case CTaskTypes::TASK_NM_FLINCH:	
		pTaskInfo = rage_new CClonedNMFlinchInfo();
		break;
	case CTaskTypes::TASK_NM_SIT:	
		pTaskInfo = rage_new CClonedNMSitInfo();
		break;
	case CTaskTypes::TASK_NM_FALL_DOWN:	
		pTaskInfo = rage_new CClonedNMFallDownInfo();
		break;
	case CTaskTypes::TASK_NM_INJURED_ON_GROUND:	
		pTaskInfo = rage_new CClonedNMInjuredOnGroundInfo();
		break;
	case CTaskTypes::TASK_NM_PROTOTYPE:	
		pTaskInfo = rage_new CClonedNMPrototypeInfo();
		break;
	case CTaskTypes::TASK_NM_CONTROL:
		pTaskInfo = rage_new CClonedNMControlInfo();
		break;
	case CTaskTypes::TASK_NM_THROUGH_WINDSCREEN:	
		pTaskInfo = rage_new CClonedNMThroughWindscreenInfo();
		break;
	case CTaskTypes::TASK_NM_RIVER_RAPIDS:	
		pTaskInfo = rage_new CClonedNMRiverRapidsInfo();
		break;
	case CTaskTypes::TASK_NM_SIMPLE:	
		pTaskInfo = rage_new CClonedNMSimpleInfo();
		break;
	case CTaskTypes::TASK_PAUSE:	
		pTaskInfo = rage_new CClonedPauseInfo();
		break;
	case CTaskTypes::TASK_PARACHUTE:	
		pTaskInfo = rage_new CClonedParachuteInfo();
		break;
	case CTaskTypes::TASK_RAGE_RAGDOLL:	
		pTaskInfo = rage_new CClonedRageRagdollInfo();
		break;
	case CTaskTypes::TASK_BOMB:
		pTaskInfo = rage_new CClonedBombInfo();
		break;
	case CTaskTypes::TASK_SWAP_WEAPON:
		pTaskInfo = rage_new CClonedSwapWeaponInfo();
		break;
    case CTaskTypes::TASK_RUN_NAMED_CLIP:
        pTaskInfo = rage_new CClonedScriptClipInfo();
        break;
	case CTaskTypes::TASK_COMBAT:
		pTaskInfo = rage_new CClonedCombatTaskInfo();
		break;
    case CTaskTypes::TASK_COMBAT_CLOSEST_TARGET_IN_AREA:
        pTaskInfo = rage_new CClonedCombatClosestTargetInAreaInfo();
        break;
	case CTaskTypes::TASK_USE_SEQUENCE:
		pTaskInfo = rage_new CClonedTaskSequenceInfo();
		break;
	case CTaskTypes::TASK_CONTROL_VEHICLE:
		pTaskInfo = rage_new CClonedControlVehicleInfo();
		break;
	case CTaskTypes::TASK_SMART_FLEE:
		pTaskInfo = rage_new CClonedControlTaskSmartFleeInfo();
		break;
	case CTaskTypes::TASK_RELOAD_GUN:
		pTaskInfo = rage_new CClonedTaskReloadGunInfo();
		break;
	case CTaskTypes::TASK_ESCAPE_BLAST:
		pTaskInfo = rage_new CClonedControlTaskEscapeBlastInfo();
		break;
	case CTaskTypes::TASK_FLY_AWAY:
		pTaskInfo = rage_new CClonedControlTaskFlyAwayInfo();
		break;
	case CTaskTypes::TASK_SCENARIO_FLEE:
		pTaskInfo = rage_new CClonedControlTaskScenarioFleeInfo();
		break;
	case CTaskTypes::TASK_SHARK_ATTACK:
		pTaskInfo = rage_new CClonedTaskSharkAttackInfo();
		break;
	case CTaskTypes::TASK_REVIVE:
		pTaskInfo = rage_new CClonedReviveInfo();
		break;
	case CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON:
		pTaskInfo = rage_new CClonedControlTaskVehicleMountedWeaponInfo();
		break;
    case CTaskTypes::TASK_HELI_PASSENGER_RAPPEL:
        pTaskInfo = rage_new CClonedHeliPassengerRappelInfo();
		break;
	case CTaskTypes::TASK_SCRIPTED_ANIMATION:
		pTaskInfo = rage_new CClonedScriptedAnimationInfo();
		break;
	case CTaskTypes::TASK_VAULT:
		pTaskInfo = rage_new CClonedTaskVaultInfo();
		break;
	case CTaskTypes::TASK_DROP_DOWN:
		pTaskInfo = rage_new CClonedTaskDropDownInfo();
		break;
	case CTaskTypes::TASK_CLIMB_LADDER:
		pTaskInfo = rage_new CClonedClimbLadderInfo();
		break;
    case CTaskTypes::TASK_LEAVE_ANY_CAR:
        pTaskInfo = rage_new CClonedLeaveAnyCarInfo();
		break;
    case CTaskTypes::TASK_CAR_DRIVE_WANDER:
        pTaskInfo = rage_new CClonedDriveCarWanderInfo();
		break;
    case CTaskTypes::TASK_DO_NOTHING:
        pTaskInfo = rage_new CClonedDoNothingInfo();
        break;
    case CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH:
        pTaskInfo = rage_new CClonedMoveFollowNavMeshInfo();
        break;
    case CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL:
        pTaskInfo = rage_new CClonedGoToPointAndStandStillTimedInfo();
        break;
	case CTaskTypes::TASK_RAPPEL:
		pTaskInfo = rage_new CClonedRappelDownWallInfo();
		break;
	case CTaskTypes::TASK_AMBIENT_CLIPS:
		pTaskInfo = rage_new CClonedTaskAmbientClipsInfo();
		break;
	case CTaskTypes::TASK_USE_SCENARIO:
		pTaskInfo = rage_new CClonedTaskUseScenarioInfo();
		break;
	case CTaskTypes::TASK_USE_VEHICLE_SCENARIO:
		pTaskInfo = rage_new CClonedTaskUseVehicleScenarioInfo();
		break;
	case CTaskTypes::TASK_MOVE_BETWEEN_POINTS_SCENARIO:
	case CTaskTypes::TASK_WANDERING_IN_RADIUS_SCENARIO:
		pTaskInfo = rage_new CClonedScenarioInfo();
		break;
	case CTaskTypes::TASK_WANDERING_SCENARIO:
		pTaskInfo = rage_new CClonedTaskWanderingScenarioInfo();
		break;
	case CTaskTypes::TASK_COWER:
		pTaskInfo = rage_new CClonedCowerInfo();
		break;
	case CTaskTypes::TASK_COWER_SCENARIO:
		pTaskInfo = rage_new CClonedCowerScenarioInfo();
		break;
	case CTaskTypes::TASK_CRAWL:
		pTaskInfo = rage_new CClonedCrawlInfo();
		break;
	case CTaskTypes::TASK_CROUCH:
		pTaskInfo = rage_new CClonedCrouchInfo();
		break;
	case CTaskTypes::TASK_JUMP:
		pTaskInfo = rage_new CClonedTaskJumpInfo();
		break;
	case CTaskTypes::TASK_FALL:
		pTaskInfo = rage_new CClonedFallInfo();
		break;
	case CTaskTypes::TASK_GENERAL_SWEEP:
		pTaskInfo = rage_new CClonedGeneralSweepInfo();
		break;
	case CTaskTypes::TASK_ARREST_PED2:
		pTaskInfo = rage_new CClonedTaskArrestPed2Info();
		break;
#if ENABLE_TASKS_ARREST_CUFFED
	case CTaskTypes::TASK_ARREST:
		pTaskInfo = rage_new CClonedTaskArrestInfo();
		break;
	case CTaskTypes::TASK_CUFFED:
		pTaskInfo = rage_new CClonedTaskCuffedInfo();
		break;
	case CTaskTypes::TASK_IN_CUSTODY:
		pTaskInfo = rage_new CClonedTaskInCustodyInfo();
		break;
#endif // ENABLE_TASKS_ARREST_CUFFED
	case CTaskTypes::TASK_ON_FOOT_SLOPE_SCRAMBLE:
		pTaskInfo = rage_new CClonedSlopeScrambleInfo();
		break;
	case CTaskTypes::TASK_AIM_GUN_SCRIPTED:
		pTaskInfo = rage_new CClonedAimGunScriptedInfo();
		break;
	case CTaskTypes::TASK_MELEE:
		pTaskInfo = rage_new CClonedTaskMeleeInfo();
		break;
	case CTaskTypes::TASK_MELEE_ACTION_RESULT:
		pTaskInfo = rage_new CClonedTaskMeleeResultInfo();
		break;
	case CTaskTypes::TASK_MOVE_SCRIPTED:
		pTaskInfo = rage_new CClonedTaskMoVEScriptedInfo();
		break;
	case CTaskTypes::TASK_REACT_AND_FLEE:
		pTaskInfo = rage_new CClonedReactAndFleeInfo();
		break;
	case CTaskTypes::TASK_GO_TO_POINT_AIMING:
		pTaskInfo = rage_new CClonedGoToPointAimingInfo();
		break;
	case CTaskTypes::TASK_WEAPON_BLOCKED:
		pTaskInfo = rage_new CClonedWeaponBlockedInfo();
		break;
	case CTaskTypes::TASK_MOVE_ACHIEVE_HEADING:
		pTaskInfo = rage_new CClonedAchieveHeadingInfo();
		break;
	case CTaskTypes::TASK_COMPLEX_TURN_TO_FACE_ENTITY:
		pTaskInfo = rage_new CClonedTurnToFaceEntityOrCoordInfo();
		break;
	case CTaskTypes::TASK_SEEK_ENTITY_AIMING:
		pTaskInfo = rage_new CClonedSeekEntityAimingInfo();
		break;
	case CTaskTypes::TASK_MOVE_SEEK_ENTITY_STANDARD:
		pTaskInfo = rage_new CClonedSeekEntityStandardInfo();
		break;
	case CTaskTypes::TASK_MOVE_SEEK_ENTITY_LAST_NAV_MESH_INTERSECTION:
		pTaskInfo = rage_new CClonedSeekEntityLastNavMeshIntersectionInfo();
		break;
	case CTaskTypes::TASK_MOVE_SEEK_ENTITY_OFFSET_ROTATE:
		pTaskInfo = rage_new CClonedSeekEntityOffsetRotateInfo();
		break;
	case CTaskTypes::TASK_MOVE_SEEK_ENTITY_OFFSET_FIXED:
		pTaskInfo = rage_new CClonedSeekEntityOffsetFixedInfo();
		break;
	case CTaskTypes::TASK_MOVE_SEEK_ENTITY_RADIUS_ANGLE:
		pTaskInfo = rage_new CClonedSeekEntityRadiusAngleInfo();
		break;
	case CTaskTypes::TASK_TRIGGER_LOOK_AT:
		pTaskInfo = rage_new CClonedLookAtInfo();
		break;
	case CTaskTypes::TASK_SLIDE_TO_COORD:
		pTaskInfo = rage_new CClonedSlideToCoordInfo();
		break;
    case CTaskTypes::TASK_SET_PED_DEFENSIVE_AREA:
        pTaskInfo = rage_new CClonedSetPedDefensiveAreaInfo();
		break;
	case CTaskTypes::TASK_SET_AND_GUARD_AREA:
		pTaskInfo = rage_new CClonedSetAndGuardAreaInfo();
		break;
	case CTaskTypes::TASK_STAND_GUARD:
		pTaskInfo = rage_new CClonedStandGuardInfo();
		break;
	case CTaskTypes::TASK_STAY_IN_COVER:
		pTaskInfo = rage_new CClonedStayInCoverInfo();
		break;
	case CTaskTypes::TASK_WRITHE:
		pTaskInfo = rage_new CClonedWritheInfo();
		break;
	case CTaskTypes::TASK_TAKE_OFF_PED_VARIATION:
		pTaskInfo = rage_new ClonedTakeOffPedVariationInfo();
		break;
	case CTaskTypes::TASK_THREAT_RESPONSE:
		pTaskInfo = rage_new CClonedThreatResponseInfo();
		break;
	case CTaskTypes::TASK_ANIMATED_ATTACH:
		pTaskInfo = rage_new CClonedAnimatedAttachInfo();
		break;
	case CTaskTypes::TASK_SET_BLOCKING_OF_NON_TEMPORARY_EVENTS:
		pTaskInfo = rage_new CClonedSetBlockingOfNonTemporaryEventsInfo();
		break;
	case CTaskTypes::TASK_FORCE_MOTION_STATE:
		pTaskInfo = rage_new CClonedForceMotionStateInfo();
		break;
	case CTaskTypes::TASK_HANDS_UP:
		pTaskInfo = rage_new CClonedHandsUpInfo();
		break;
    case CTaskTypes::TASK_SYNCHRONIZED_SCENE:
        pTaskInfo = rage_new CClonedSynchronisedSceneInfo();
		break;
	case CTaskTypes::TASK_REACT_AIM_WEAPON:
		pTaskInfo = rage_new CClonedReactAimWeaponInfo();
		break;
	case CTaskTypes::TASK_GO_TO_POINT_ANY_MEANS:
		pTaskInfo = rage_new CClonedGoToPointAnyMeansInfo();
		break;
	case CTaskTypes::TASK_UNALERTED:
		pTaskInfo = rage_new CClonedTaskUnalertedInfo();
		break;
	case CTaskTypes::TASK_PARACHUTE_OBJECT:
		pTaskInfo = rage_new CClonedParachuteObjectInfo();
		break;
	case CTaskTypes::TASK_CLEAR_LOOK_AT:
		pTaskInfo = rage_new CClonedClearLookAtInfo();
		break;
	case CTaskTypes::TASK_VEHICLE_PROJECTILE:
		pTaskInfo = rage_new CClonedVehicleProjectileInfo();
		break;
	case CTaskTypes::TASK_WANDER:
		pTaskInfo = rage_new CClonedWanderInfo();
		break;
	case CTaskTypes::TASK_PLANE_CHASE:
		pTaskInfo = rage_new CClonedPlaneChaseInfo();
		break;
	case CTaskTypes::TASK_AMBULANCE_PATROL:
		pTaskInfo = rage_new CClonedAmbulancePatrolInfo();
		break;
	case CTaskTypes::TASK_REACT_TO_EXPLOSION:
		pTaskInfo = rage_new CClonedReactToExplosionInfo();
		break;
	case CTaskTypes::TASK_DIVE_TO_GROUND:
		pTaskInfo = rage_new CClonedDiveToGroundInfo();
		break;
	case CTaskTypes::TASK_COMPLEX_EVASIVE_STEP:
		pTaskInfo = rage_new CClonedComplexEvasiveStepInfo();
		break;
	case CTaskTypes::TASK_VARIED_AIM_POSE:
		pTaskInfo = rage_new CClonedVariedAimPoseInfo();
		break;
	case CTaskTypes::TASK_HELI_CHASE:
		pTaskInfo = rage_new CClonedHeliChaseInfo();
		break;
#if CNC_MODE_ENABLED
	case CTaskTypes::TASK_INCAPACITATED:
		pTaskInfo = rage_new CClonedTaskIncapacitatedInfo();
		break;
#endif
	case CTaskTypes::TASK_PATROL:
		pTaskInfo = rage_new CClonesTaskPatrolInfo();
		break;

#if ENABLE_JETPACK
	case CTaskTypes::TASK_JETPACK:	
		pTaskInfo = rage_new CClonedJetpackInfo();
		break;
#endif
	default:
        break;
    }

    if(pTaskInfo == 0)
    {
		aiAssertf(!bAssert, "CQueriableInterface::CreateEmptyTaskInfo - %s is not handled", TASKCLASSINFOMGR.GetTaskName(taskType));
        pTaskInfo = rage_new CTaskInfo();
    }

    pTaskInfo->SetType(taskType);

	return pTaskInfo;
}

//-------------------------------------------------------------------------
// Finds the first non-temporary task (the first task that doesn't exist in
// a temporary slot
//-------------------------------------------------------------------------
CTaskInfo* CQueriableInterface::GetFirstNonTemporaryTask() const
{
	CTaskInfo* pNextTaskInfo = m_pTaskInfoFirst;
	while( pNextTaskInfo )
	{
		if( !pNextTaskInfo->GetIsTemporaryTask() )
		{
			break;
		}
		pNextTaskInfo = pNextTaskInfo->GetNextTaskInfo();
	}

    return pNextTaskInfo;
}


//-------------------------------------------------------------------------
// Finds the first task of the given priority
//-------------------------------------------------------------------------
CTaskInfo* CQueriableInterface::GetFirstTaskOfPriority( u32 iPriority ) const
{
	CTaskInfo* pNextTaskInfo = m_pTaskInfoFirst;
	while( pNextTaskInfo )
	{
		if( pNextTaskInfo->GetTaskPriority() == iPriority )
		{
			break;
		}
		pNextTaskInfo = pNextTaskInfo->GetNextTaskInfo();
	}

	return pNextTaskInfo;
}

//-------------------------------------------------------------------------
// Returns true if the given task type is a task which runs a simple anim
//-------------------------------------------------------------------------
bool CQueriableInterface::IsAnimTask(u32 UNUSED_PARAM(taskType))
{
	// disabled for now
	return false;
}

// name:        AddTaskSequenceInfo
// description: adds a task sequence info 
void CQueriableInterface::AddTaskSequenceInfo(TaskSequenceInfo* taskSequenceInfo)
{
	if (AssertVerify(taskSequenceInfo))
	{
		Assert(!GetTaskSequenceInfo(taskSequenceInfo->m_taskType));

		taskSequenceInfo->m_nextSequenceInfo = m_pFirstTaskSequenceInfo;
		m_pFirstTaskSequenceInfo = taskSequenceInfo;
	}
}

// name:        AddTaskSequenceInfo
// description: adds a task sequence info for the specified task type
TaskSequenceInfo *CQueriableInterface::AddTaskSequenceInfo(u32 taskType)
{
    Assert(!GetTaskSequenceInfo(taskType));

#if __BANK
	if(TaskSequenceInfo::GetPool()->GetNoOfFreeSpaces() == 0)
	{
		TaskSequenceInfo::GetPool()->PoolFullCallback();
		Assertf(0, "TaskSequenceInfo pool ran out, size %d, can't add task type %d, task name %s. See previous TTY for pool dump list details", TaskSequenceInfo::GetPool()->GetSize(),taskType, static_cast<const char *>(TASKCLASSINFOMGR.GetTaskName(taskType)));
	}
#endif

    TaskSequenceInfo *sequenceInfo = rage_new TaskSequenceInfo(taskType);

    AddTaskSequenceInfo(sequenceInfo);

    return sequenceInfo;
}

// name:        GetTaskSequenceInfo
// description: returns the task sequence info corresponding to the specified task type
void CQueriableInterface::RemoveTaskSequenceInfo(TaskSequenceInfo *taskSequenceInfo)
{
    Assert(taskSequenceInfo);

    if(taskSequenceInfo)
    {
        bool removed = false;

        if(taskSequenceInfo == m_pFirstTaskSequenceInfo)
        {
            m_pFirstTaskSequenceInfo = taskSequenceInfo->m_nextSequenceInfo;
            removed                  = true;
        }
        else if(m_pFirstTaskSequenceInfo)
        {
            TaskSequenceInfo *prevSequenceInfo = m_pFirstTaskSequenceInfo;
            TaskSequenceInfo *currSequenceInfo = m_pFirstTaskSequenceInfo->m_nextSequenceInfo;

            while(currSequenceInfo && !removed)
            {
                if(taskSequenceInfo == currSequenceInfo)
                {
                    prevSequenceInfo->m_nextSequenceInfo = taskSequenceInfo->m_nextSequenceInfo;
                    removed = true;
                }

                prevSequenceInfo = currSequenceInfo;
                currSequenceInfo = currSequenceInfo->m_nextSequenceInfo;
            }
        }

        if(Verifyf(removed, "Trying to remove a task sequence info not in the list!"))
        {
            delete taskSequenceInfo;
        }
    }
}

// name:        GetTaskSequenceInfo
// description: returns the task sequence info corresponding to the specified task type
TaskSequenceInfo *CQueriableInterface::GetTaskSequenceInfo(u32 taskType)
{
    TaskSequenceInfo *sequenceInfo = m_pFirstTaskSequenceInfo;

    while(sequenceInfo)
    {
        if(sequenceInfo->m_taskType == taskType)
        {
            return sequenceInfo;
        }

        sequenceInfo = sequenceInfo->m_nextSequenceInfo;
    }

    return 0;
}


//-------------------------------------------------------------------------
// Resets all the task sequence info used by this queriable interface
//-------------------------------------------------------------------------
void CQueriableInterface::ResetTaskSequenceInfo()
{
    TaskSequenceInfo *sequenceInfo = m_pFirstTaskSequenceInfo;
	TaskSequenceInfo *nextSequenceInfo = NULL;

	while(sequenceInfo)
	{
		nextSequenceInfo = sequenceInfo->m_nextSequenceInfo;

		delete sequenceInfo;

		sequenceInfo = nextSequenceInfo;
	}

	m_pFirstTaskSequenceInfo = NULL;
}

//-------------------------------------------------------------------------
// Creates a normal task from the current queriable state
//-------------------------------------------------------------------------
CTask* CQueriableInterface::CreateNewTaskFromQueriableState( fwEntity* pEntity )
{
	for(int iTaskPriority=0; iTaskPriority<PED_TASK_PRIORITY_MAX; iTaskPriority++ )
	{
		CTaskInfo *pTaskInfo = GetFirstTaskOfPriority( iTaskPriority );

		while(pTaskInfo)
		{
			CTask *newTask = pTaskInfo->CreateLocalTask(pEntity);

			if(newTask)
			{
				return newTask;
			}

			pTaskInfo = pTaskInfo->GetNextTaskInfo();
		}
	}

	return 0;
}

//-------------------------------------------------------------------------
// Parses and logs the contents of a task data buffer for a specific task type
//-------------------------------------------------------------------------
void CQueriableInterface::LogTaskInfo(netLoggingInterface *log, u32 taskType, const u8 *dataBlock, const int numBits)
{
    if(log)
    {
        CTaskInfo *taskInfo = CreateEmptyTaskInfo(taskType);

        if(taskInfo)
        {
			datBitBuffer messageBuffer;
            messageBuffer.SetReadOnlyBits(dataBlock, numBits, 0);
            taskInfo->ReadSpecificTaskInfo(messageBuffer);
            taskInfo->LogSpecificTaskInfo(*log);

            delete taskInfo;
        }
    }
}

//-------------------------------------------------------------------------
// Adds information about a particular task from the network
//-------------------------------------------------------------------------
CTaskInfo *CQueriableInterface::AddTaskInformationFromNetwork(u32 taskType, const bool bActive, const s32 iTaskPriority, const u32 iTaskSequenceId)
{
    CTaskInfo *taskInfo = CreateTaskInformationForNetwork(taskType, bActive, iTaskPriority, iTaskSequenceId);

    if(AssertVerify(taskInfo))
    {
        // Add the task info to the linked list
	    AddTaskInfo(taskInfo);
    }

    return taskInfo;
}

//-------------------------------------------------------------------------
// Adds information about a particular task from the network
//-------------------------------------------------------------------------
CTaskInfo *CQueriableInterface::CreateTaskInformationForNetwork(u32 taskType, const bool bActive, const s32 iTaskPriority, const u32 iTaskSequenceId )
{
    CTaskInfo *taskInfo = CreateEmptyTaskInfo(taskType, true);

    if(AssertVerify(taskInfo))
    {
        taskInfo->SetIsActiveTask(bActive);
        taskInfo->SetTaskPriority(iTaskPriority);
		taskInfo->SetNetSequenceID(iTaskSequenceId);
     }

    return taskInfo;
}

//-------------------------------------------------------------------------
// Adds a task info structure directly from the network (usually clones of prior existing task info structures)
//-------------------------------------------------------------------------
void CQueriableInterface::AddTaskInfoFromNetwork(CTaskInfo *taskInfo)
{
    AddTaskInfo(taskInfo);
}

//-------------------------------------------------------------------------
// Unlinks the specified task info from the queriable state without deleting it
//-------------------------------------------------------------------------
void CQueriableInterface::UnlinkTaskInfoFromNetwork(CTaskInfo *taskInfoToUnlink)
{
    if(taskInfoToUnlink)
    {
        // update the last task info pointer if necessary
        if(m_pTaskInfoLast == taskInfoToUnlink)
        {
            m_pTaskInfoLast = taskInfoToUnlink->GetPrevTaskInfo();
        }

        // update the task info's previous task
        CTaskInfo *prevTaskInfo = taskInfoToUnlink->GetPrevTaskInfo();

        if(prevTaskInfo)
        {
            prevTaskInfo->SetNextTaskInfo(taskInfoToUnlink->GetNextTaskInfo());
        }
        else
        {
            Assert(m_pTaskInfoFirst == taskInfoToUnlink);
            m_pTaskInfoFirst = taskInfoToUnlink->GetNextTaskInfo();
        }

        // update the task infos next task
        if(taskInfoToUnlink->GetNextTaskInfo())
        {
            taskInfoToUnlink->GetNextTaskInfo()->SetPrevTaskInfo(prevTaskInfo);
        }

        taskInfoToUnlink->SetPrevTaskInfo(0);
        taskInfoToUnlink->SetNextTaskInfo(0);

#if __ASSERT
		m_CachedInfoMayBeOutOfDate = true;
#endif
    }
}

void CQueriableInterface::UnlinkTaskSequenceInfoFromNetwork(TaskSequenceInfo *taskSequenceInfoToUnlink)
{
	if (taskSequenceInfoToUnlink)
	{
		TaskSequenceInfo *currInfo = m_pFirstTaskSequenceInfo;
		TaskSequenceInfo *lastInfo = 0;

		while (currInfo)
		{
			TaskSequenceInfo *nextInfo = currInfo->m_nextSequenceInfo;

			if (currInfo == taskSequenceInfoToUnlink)
			{
				if (lastInfo)
				{
					lastInfo->m_nextSequenceInfo = nextInfo;
				}
				else
				{
					Assert(m_pFirstTaskSequenceInfo == taskSequenceInfoToUnlink);
					m_pFirstTaskSequenceInfo = nextInfo;
				}

				taskSequenceInfoToUnlink->m_nextSequenceInfo = 0;

				return;
			}

			lastInfo = currInfo;
			currInfo = nextInfo;
		}
#if __ASSERT
		m_CachedInfoMayBeOutOfDate = true;
#endif
	}

	Assertf(0, "CQueriableInterface::UnlinkTaskSequenceInfoFromNetwork - failed to find sequence info");
}

//-------------------------------------------------------------------------
// Get task specific size of packed data for sending over network
//-------------------------------------------------------------------------
u32 CQueriableInterface::GetTaskSpecificNetworkPackedSize(u32 taskType)
{
    u32 size = 0;

    CTaskInfo *taskInfo = CreateEmptyTaskInfo(taskType);

    if(AssertVerify(taskInfo))
    {
        size = taskInfo->GetSpecificTaskInfoSize();

        delete taskInfo;
    }

    Assert(size <= CTaskInfo::MAX_SPECIFIC_TASK_INFO_SIZE);

    return size;
}

//-------------------------------------------------------------------------
// Get the latest task sequence for the specified task type
//-------------------------------------------------------------------------
u32 CQueriableInterface::GetTaskNetSequenceForType(s32 taskType)
{
    u32     netSequence  = 0;
    CTaskInfo *currTaskInfo = m_pTaskInfoFirst;

    while(currTaskInfo)
    {
        if(currTaskInfo->GetTaskType() == taskType)
        {
            if(currTaskInfo->HasNetSequences())
            {
                netSequence = currTaskInfo->GetNetSequenceID();
            }
        }

        currTaskInfo = currTaskInfo->GetNextTaskInfo();
    }

    return netSequence;
}

//-------------------------------------------------------------------------
// Informs the queriable interface that a new task has been generated to replace an existing clone task
//-------------------------------------------------------------------------
void CQueriableInterface::HandleCloneToLocalTaskSwitch(CTaskFSMClone& newTask)
{
	TaskSequenceInfo *taskSequenceInfo = GetTaskSequenceInfo(newTask.GetTaskType());

	if (!taskSequenceInfo)
	{
		taskSequenceInfo = AddTaskSequenceInfo(newTask.GetTaskType());
	}

	taskSequenceInfo->m_sequenceID = newTask.GetNetSequenceID();
}


#if !__FINAL
//-------------------------------------------------------------------------
// debug function for printing out task names
//-------------------------------------------------------------------------
const CTaskInfo* CQueriableInterface::GetDebugTask( s32 i ) const
{
	s32 iCount = 0;
	CTaskInfo* pNextTaskInfo = m_pTaskInfoFirst;
	while( pNextTaskInfo )
	{
		if(iCount == i)
		{
			return pNextTaskInfo;
		}
		pNextTaskInfo = pNextTaskInfo->GetNextTaskInfo();
		++iCount;
	}
	return NULL;
}

//-------------------------------------------------------------------------
// debug function for printing out task names
//-------------------------------------------------------------------------
s32 CQueriableInterface::GetDebugTaskType( s32 i ) const
{
	const CTaskInfo* pTaskInfo = GetDebugTask(i);
	if( pTaskInfo )
	{
		return pTaskInfo->GetTaskType();
	}
	return -1;
}

//-------------------------------------------------------------------------
// debug function for printing out task names
//-------------------------------------------------------------------------
s32 CQueriableInterface::GetDebugTaskInfoType( s32 i ) const
{
	const CTaskInfo* pTaskInfo = GetDebugTask(i);
	if( pTaskInfo )
	{
		return pTaskInfo->GetTaskInfoType();
	}
	return -1;
}

//-------------------------------------------------------------------------
// debug function for printing out task names
//-------------------------------------------------------------------------
u32 CQueriableInterface::GetDebugTaskPriority( s32 i ) const
{
	const CTaskInfo* pTaskInfo = GetDebugTask(i);
	if( pTaskInfo )
	{
		return pTaskInfo->GetTaskPriority();
	}
	return false;
}
#endif

//-------------------------------------------------------------------------
// Indicates whether the task of the specified type is known about by the network code
//-------------------------------------------------------------------------
bool CQueriableInterface::IsTaskHandledByNetwork(s32 taskType, bool *createsCloneTask, bool UNUSED_PARAM(bAssertIfNotAndInNetworkGame))
{
    bool sentOverNetwork  = IsTaskSentOverNetwork(taskType, createsCloneTask);
    bool ignoredByNetwork = IsTaskIgnoredByNetwork(taskType);

    if((sentOverNetwork || ignoredByNetwork))
	{
		return true;
	}

    // This assert will fire all the time during the game development process so I'm disabling it until the debugging phase
//#if __ASSERT
//	if(bAssertIfNotAndInNetworkGame&&NetworkInterface::IsGameInProgress())
//	{
//		Assertf(0, "%s is not in the network task lists but is being queried by the AI!", static_cast<const char *>(TASKCLASSINFOMGR.GetTaskName(taskType)));
//	}
//#endif
	return false;
}

//-------------------------------------------------------------------------
// Indicates whether the task of the specified type is included in the
// subset of the queriable state sent over the network
//-------------------------------------------------------------------------
bool CQueriableInterface::IsTaskSentOverNetwork(s32 taskType, bool *isImportantTask)
{
    if(isImportantTask)
    {
        *isImportantTask = false;
    }

	switch(taskType)
    {
		// tasks that generate clone tasks or reconstruct tasks on remote clones during the ownership change process
	case CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE:
	case CTaskTypes::TASK_AIM_GUN_BLIND_FIRE:
    case CTaskTypes::TASK_AIM_GUN_CLIMBING:
    case CTaskTypes::TASK_AIM_GUN_ON_FOOT:
    case CTaskTypes::TASK_AIM_GUN_RUN_AND_GUN:
	case CTaskTypes::TASK_AIM_GUN_SCRIPTED:
    case CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY:
    case CTaskTypes::TASK_AMBIENT_CLIPS:
	case CTaskTypes::TASK_ANIMATED_ATTACH:
	case CTaskTypes::TASK_ARREST:
	case CTaskTypes::TASK_ARREST_PED2:
    case CTaskTypes::TASK_BOMB:
    case CTaskTypes::TASK_CHAT_SCENARIO:
	case CTaskTypes::TASK_CLIMB_LADDER:
	case CTaskTypes::TASK_COMBAT:
    case CTaskTypes::TASK_COMBAT_CLOSEST_TARGET_IN_AREA:
	case CTaskTypes::TASK_COMBAT_FLANK:
    case CTaskTypes::TASK_COMPLEX_ON_FIRE:
    case CTaskTypes::TASK_COMPLEX_DRIVING_SCENARIO:
	case CTaskTypes::TASK_COMPLEX_TURN_TO_FACE_ENTITY:
	case CTaskTypes::TASK_COMPLEX_EVASIVE_STEP:
	case CTaskTypes::TASK_CONTROL_VEHICLE:
	case CTaskTypes::TASK_COVER:
	case CTaskTypes::TASK_COWER:
	case CTaskTypes::TASK_COWER_SCENARIO:
	case CTaskTypes::TASK_CRAWL:
	case CTaskTypes::TASK_CUFFED:
	case CTaskTypes::TASK_DAMAGE_ELECTRIC:
	case CTaskTypes::TASK_DEAD:
	case CTaskTypes::TASK_DISMOUNT_ANIMAL:
	case CTaskTypes::TASK_DIVE_TO_GROUND:
	case CTaskTypes::TASK_DROP_DOWN:
	case CTaskTypes::TASK_DYING_DEAD:
	case CTaskTypes::TASK_ENTER_COVER:
    case CTaskTypes::TASK_ENTER_VEHICLE:
	case CTaskTypes::TASK_ESCAPE_BLAST:
	case CTaskTypes::TASK_EXIT_VEHICLE:
	case CTaskTypes::TASK_FALL:
    case CTaskTypes::TASK_FALL_AND_GET_UP:
    case CTaskTypes::TASK_FALL_OVER:
	case CTaskTypes::TASK_FLY_AWAY:
	case CTaskTypes::TASK_GENERAL_SWEEP:
	case CTaskTypes::TASK_GET_UP:
    case CTaskTypes::TASK_GO_TO_AND_CLIMB_LADDER:
	case CTaskTypes::TASK_GO_TO_POINT_AIMING:
	case CTaskTypes::TASK_GO_TO_POINT_ANY_MEANS:
	case CTaskTypes::TASK_GUN:
	case CTaskTypes::TASK_HANDS_UP:
	case CTaskTypes::TASK_HELI_CHASE:
	case CTaskTypes::TASK_HELI_PASSENGER_RAPPEL:
    case CTaskTypes::TASK_IN_COVER:
	case CTaskTypes::TASK_IN_CUSTODY:
    case CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE:
	case CTaskTypes::TASK_INCAPACITATED:
	case CTaskTypes::TASK_JUMP:
    case CTaskTypes::TASK_MELEE:
	case CTaskTypes::TASK_MELEE_ACTION_RESULT:
	case CTaskTypes::TASK_MOTION_IN_COVER:
	case CTaskTypes::TASK_MOUNT_ANIMAL:
	case CTaskTypes::TASK_MOUNT_THROW_PROJECTILE:	
	case CTaskTypes::TASK_MOVE_ACHIEVE_HEADING:
	case CTaskTypes::TASK_MOVE_BETWEEN_POINTS_SCENARIO:
	case CTaskTypes::TASK_MOVE_SCRIPTED:
    case CTaskTypes::TASK_NM_CONTROL:
    case CTaskTypes::TASK_NM_BALANCE:
	case CTaskTypes::TASK_NM_BRACE:
	case CTaskTypes::TASK_NM_BUOYANCY:
	case CTaskTypes::TASK_NM_ELECTROCUTE:
	case CTaskTypes::TASK_NM_EXPLOSION:
    case CTaskTypes::TASK_NM_FALL_DOWN:
    case CTaskTypes::TASK_NM_FLINCH:
    case CTaskTypes::TASK_NM_HIGH_FALL:
    case CTaskTypes::TASK_NM_INJURED_ON_GROUND:
    case CTaskTypes::TASK_NM_JUMP_ROLL_FROM_ROAD_VEHICLE:
    case CTaskTypes::TASK_NM_ONFIRE:
    case CTaskTypes::TASK_NM_PROTOTYPE:
    case CTaskTypes::TASK_NM_POSE:
    case CTaskTypes::TASK_NM_RELAX:
	case CTaskTypes::TASK_NM_SHOT:
	case CTaskTypes::TASK_NM_SIMPLE:
	case CTaskTypes::TASK_NM_SIT:
    case CTaskTypes::TASK_NM_THROUGH_WINDSCREEN:
	case CTaskTypes::TASK_NM_RIVER_RAPIDS:
    case CTaskTypes::TASK_ON_FOOT_SLOPE_SCRAMBLE:
    case CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE:
	case CTaskTypes::TASK_PAUSE:
	case CTaskTypes::TASK_PARACHUTE:
	case CTaskTypes::TASK_PARACHUTE_OBJECT:
	case CTaskTypes::TASK_PATROL:
	case CTaskTypes::TASK_PLANE_CHASE:
    case CTaskTypes::TASK_RAPPEL:
	case CTaskTypes::TASK_REACT_AND_FLEE:
	case CTaskTypes::TASK_REACT_AIM_WEAPON:
	case CTaskTypes::TASK_REACT_TO_EXPLOSION:
	case CTaskTypes::TASK_RELOAD_GUN:
	case CTaskTypes::TASK_REVIVE:
	case CTaskTypes::TASK_RUN_NAMED_CLIP:
	case CTaskTypes::TASK_SCENARIO_FLEE:
    case CTaskTypes::TASK_SCRIPTED_ANIMATION:
	case CTaskTypes::TASK_SET_AND_GUARD_AREA:
	case CTaskTypes::TASK_SET_BLOCKING_OF_NON_TEMPORARY_EVENTS:
    case CTaskTypes::TASK_SET_PED_DEFENSIVE_AREA:
	case CTaskTypes::TASK_SHARK_ATTACK:
    case CTaskTypes::TASK_SIT_DOWN:
    case CTaskTypes::TASK_SIT_IDLE:
	case CTaskTypes::TASK_SMART_FLEE:
	case CTaskTypes::TASK_STAND_GUARD:
	case CTaskTypes::TASK_STAY_IN_COVER:
    case CTaskTypes::TASK_SWAP_WEAPON:
    case CTaskTypes::TASK_SYNCHRONIZED_SCENE:
	case CTaskTypes::TASK_TAKE_OFF_PED_VARIATION:
    case CTaskTypes::TASK_THROW_PROJECTILE:
	case CTaskTypes::TASK_UNALERTED:
    case CTaskTypes::TASK_USE_SCENARIO:
    case CTaskTypes::TASK_USE_SEQUENCE:
	case CTaskTypes::TASK_USE_VEHICLE_SCENARIO:
	case CTaskTypes::TASK_VARIED_AIM_POSE:
    case CTaskTypes::TASK_VAULT:
    case CTaskTypes::TASK_VEHICLE_GUN:
	case CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON:
	case CTaskTypes::TASK_VEHICLE_PROJECTILE:
    case CTaskTypes::TASK_VEHICLE_SHUNT:
    case CTaskTypes::TASK_WANDER:
    case CTaskTypes::TASK_WANDERING_SCENARIO:
    case CTaskTypes::TASK_WANDERING_IN_RADIUS_SCENARIO:
	case CTaskTypes::TASK_WEAPON_BLOCKED:
	case CTaskTypes::TASK_WRITHE:
	case CTaskTypes::TASK_JETPACK:
	
    // *** PLEASE TRY AND KEEP THESE IN ALPHABETICAL ORDER - IT MAKES IT EASIER CHECK IF A TASK TYPE IS ALREADY HANDLED ***
        if(isImportantTask)
        {
            *isImportantTask = true;
        }
        // Intentional fall through


    // tasks that are used for querying by the AI systems
	case CTaskTypes::TASK_AMBULANCE_PATROL:
	case CTaskTypes::TASK_BRING_VEHICLE_TO_HALT:
    case CTaskTypes::TASK_COMPLEX_LEAVE_CAR_AND_FLEE:
    case CTaskTypes::TASK_COMPLEX_USE_MOBILE_PHONE:
    case CTaskTypes::TASK_COMPLEX_WAIT_FOR_SEAT_TO_BE_FREE:
    case CTaskTypes::TASK_PLAYER_IDLES:
    case CTaskTypes::TASK_PLAYER_WEAPON:
	case CTaskTypes::TASK_RAGE_RAGDOLL:
    case CTaskTypes::TASK_REACT_TO_GUN_AIMED_AT:
	// *** PLEASE TRY AND KEEP THESE IN ALPHABETICAL ORDER - IT MAKES IT EASIER CHECK IF A TASK TYPE IS ALREADY HANDLED ***
		return true;
    default:
		if (IsAnimTask(taskType))
			return true;

        return false;
    }

}

//-------------------------------------------------------------------------
// Indicates whether the task is queried by the AI but unimportant for network games
//-------------------------------------------------------------------------
bool CQueriableInterface::IsTaskIgnoredByNetwork(s32 taskType)
{
    switch(taskType)
    {
    case CTaskTypes::TASK_ADDITIONAL_COMBAT_TASK:
	case CTaskTypes::TASK_AFFECT_SECONDARY_BEHAVIOUR:
	case CTaskTypes::TASK_AGGRESSIVE_RUBBERNECK:
	case CTaskTypes::TASK_AGITATED_ACTION:
    case CTaskTypes::TASK_BLEND_FROM_NM:
    case CTaskTypes::TASK_CAR_DRIVE:
    case CTaskTypes::TASK_CAR_DRIVE_POINT_ROUTE:
    case CTaskTypes::TASK_CAR_DRIVE_WANDER:
    case CTaskTypes::TASK_CAR_REACT_TO_VEHICLE_COLLISION:
	case CTaskTypes::TASK_CAR_SET_TEMP_ACTION:
    case CTaskTypes::TASK_COMBAT_FLANK:
    case CTaskTypes::TASK_COMPLEX_CAR_DRIVE_MISSION_KILL_PED:
    case CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT:
    case CTaskTypes::TASK_COMPLEX_EVASIVE_STEP:
    case CTaskTypes::TASK_COMPLEX_SEARCH_WANDER_AFTER_TIME:
 	case CTaskTypes::TASK_COVER:
	case CTaskTypes::TASK_ENTER_COVER:
	case CTaskTypes::TASK_CROUCH:
    case CTaskTypes::TASK_DO_NOTHING:
	case CTaskTypes::TASK_EXHAUSTED_FLEE:
	case CTaskTypes::TASK_FOLLOW_LEADER_IN_FORMATION:
	case CTaskTypes::TASK_GROWL_AND_FLEE:
	case CTaskTypes::TASK_IN_VEHICLE_BASIC:
    case CTaskTypes::TASK_JUMPVAULT:
    case CTaskTypes::TASK_LEAVE_ANY_CAR:
    case CTaskTypes::TASK_MOBILE_PHONE_CHAT_SCENARIO:
	case CTaskTypes::TASK_NM_GENERIC_ATTACH:
    case CTaskTypes::TASK_PUT_ON_HELMET:
    case CTaskTypes::TASK_RUN_CLIP:
	case CTaskTypes::TASK_SEEK_ENTITY_AIMING:
	case CTaskTypes::TASK_SET_PED_IN_VEHICLE:
	case CTaskTypes::TASK_SHARK_ATTACK:
	case CTaskTypes::TASK_SHOCKING_EVENT_BACK_AWAY:
    case CTaskTypes::TASK_SHOCKING_EVENT_HURRYAWAY:
    case CTaskTypes::TASK_SHOCKING_EVENT_GOTO:
	case CTaskTypes::TASK_SHOCKING_EVENT_REACT:
	case CTaskTypes::TASK_SHOCKING_EVENT_REACT_TO_AIRCRAFT:
	case CTaskTypes::TASK_SHOCKING_EVENT_STOP_AND_STARE:
    case CTaskTypes::TASK_SHOCKING_EVENT_WATCH:
	case CTaskTypes::TASK_SHOCKING_POLICE_INVESTIGATE:
	case CTaskTypes::TASK_THREAT_RESPONSE:
	case CTaskTypes::TASK_WALK_AWAY:
   // *** PLEASE TRY AND KEEP THESE IN ALPHABETICAL ORDER - IT MAKES IT EASIER CHECK IF A TASK TYPE IS ALREADY HANDLED ***

	// temp until implemented:
#if __DEV
	case CTaskTypes::TASK_NM_BIND_POSE:
#endif	//	__DEV
    case CTaskTypes::TASK_NM_DANGLE:
    case CTaskTypes::TASK_NM_SCRIPT_CONTROL:
       	return true;
    default:
        return false;
    }
}

bool CQueriableInterface::CanTaskBeGivenToNonLocalPeds(s32 taskType)
{
	// *** ALL TASKS ADDED HERE MUST HAVE AN INFO WITH CreateLocalTask() DEFINED ***

    switch(taskType)
    {
	case CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE:
	case CTaskTypes::TASK_AFFECT_SECONDARY_BEHAVIOUR:
	case CTaskTypes::TASK_CAR_DRIVE_WANDER:
	case CTaskTypes::TASK_CLEAR_LOOK_AT:
    case CTaskTypes::TASK_COMBAT_CLOSEST_TARGET_IN_AREA:
    case CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT:
	case CTaskTypes::TASK_COMPLEX_TURN_TO_FACE_ENTITY:
    case CTaskTypes::TASK_CONTROL_VEHICLE:
	case CTaskTypes::TASK_COWER:
	case CTaskTypes::TASK_COVER:
	case CTaskTypes::TASK_CROUCH:
	case CTaskTypes::TASK_DO_NOTHING:
    case CTaskTypes::TASK_ENTER_VEHICLE:
	case CTaskTypes::TASK_FORCE_MOTION_STATE:
	case CTaskTypes::TASK_GENERAL_SWEEP:
	case CTaskTypes::TASK_GO_TO_POINT_AIMING:
	case CTaskTypes::TASK_GO_TO_POINT_ANY_MEANS:
	case CTaskTypes::TASK_GUN:
	case CTaskTypes::TASK_HANDS_UP:
	case CTaskTypes::TASK_LEAVE_ANY_CAR:
	case CTaskTypes::TASK_MOVE_ACHIEVE_HEADING:
	case CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH:
	case CTaskTypes::TASK_MOVE_SEEK_ENTITY_STANDARD:
	case CTaskTypes::TASK_MOVE_SEEK_ENTITY_LAST_NAV_MESH_INTERSECTION:
	case CTaskTypes::TASK_MOVE_SEEK_ENTITY_OFFSET_ROTATE:
	case CTaskTypes::TASK_MOVE_SEEK_ENTITY_OFFSET_FIXED:
	case CTaskTypes::TASK_MOVE_SEEK_ENTITY_RADIUS_ANGLE:
    case CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL:
	case CTaskTypes::TASK_PARACHUTE:
	case CTaskTypes::TASK_RAPPEL:
	case CTaskTypes::TASK_SCRIPTED_ANIMATION:
	case CTaskTypes::TASK_SEEK_ENTITY_AIMING:
	case CTaskTypes::TASK_SET_AND_GUARD_AREA:
	case CTaskTypes::TASK_SET_BLOCKING_OF_NON_TEMPORARY_EVENTS:
	case CTaskTypes::TASK_SET_PED_DEFENSIVE_AREA:
	case CTaskTypes::TASK_SLIDE_TO_COORD:
	case CTaskTypes::TASK_SMART_FLEE:
	case CTaskTypes::TASK_STAND_GUARD:
	case CTaskTypes::TASK_STAY_IN_COVER:
	case CTaskTypes::TASK_SWAP_WEAPON:
	case CTaskTypes::TASK_THREAT_RESPONSE:
	case CTaskTypes::TASK_TRIGGER_LOOK_AT:
	case CTaskTypes::TASK_USE_SCENARIO:
	case CTaskTypes::TASK_USE_SEQUENCE:
	case CTaskTypes::TASK_WANDER:
		// *** PLEASE TRY AND KEEP THESE IN ALPHABETICAL ORDER - IT MAKES IT EASIER CHECK IF A TASK TYPE IS ALREADY HANDLED ***
        return true;
    default:
        return false;
    }
}

#if __ASSERT

void CQueriableInterface::ValidateTaskInfos()
{
    for(unsigned taskType = static_cast<unsigned>(CTaskTypes::TASK_HANDS_UP); taskType < static_cast<unsigned>(CTaskTypes::MAX_NUM_TASK_TYPES); taskType++)
    {
        CTaskInfo *pTaskInfo = CreateEmptyTaskInfo(taskType);
        if(pTaskInfo)
        {
            if(pTaskInfo->HasNetSequences())
            {
                // tasks can be sent over the network when synced as clone tasks or when given to remotely owned peds
                bool bIsSyncedTaskType = IsTaskSentOverNetwork(taskType) || CanTaskBeGivenToNonLocalPeds(taskType);

                Assertf(bIsSyncedTaskType, "%s has net sequences but is not marked to be sent over the network!", static_cast<const char *>(TASKCLASSINFOMGR.GetTaskName(taskType)));
            }

            delete pTaskInfo;
        }
    }
}

#endif // __ASSERT
