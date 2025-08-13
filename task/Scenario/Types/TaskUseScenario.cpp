#include "TaskUseScenario.h"

#include "physics/physics.h"

#include "animation/MovePed.h"

#include "camera/gameplay/GameplayDirector.h"

// Rage headers 
#include "fwscene/stores/mapdatastore.h"
#include "phBound/boundcomposite.h"
#include "math/angmath.h"

// Game headers

#include "Animation/GestureManager.h"

#include "Event/Event.h"
#include "Event/EventShocking.h"
#include "Event/System/EventData.h"
#include "Event/EventWeapon.h"

#include "fwanimation/directorcomponentsyncedscene.h"

#include "modelinfo/PedModelInfo.h"

#include "ai/ambient/AmbientModelSetManager.h"
#include "ai/BlockingBounds.h"

#include "game/Clock.h"

#include "Network/Objects/Entities/NetObjPlayer.h"

#include "Objects/DummyObject.h"

#include "Peds/CombatBehaviour.h"
#include "Peds/FleeBehaviour.h"
#include "Peds/ped.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedIntelligence.h"
#include "peds/pedplacement.h"
#include "Peds/pedpopulation.h"			// For CPedPopulation.
#include "Peds/PopZones.h"


#include "physics/physics.h"

#include "scene/world/GameWorld.h"

#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Default/AmbientAnimationManager.h"
#include "Task/Default/TaskAmbient.h"
#include "Task/Default/TaskUnalerted.h"
#include "Task/Motion/TaskMotionBase.h"
#include "task/Motion/Locomotion/TaskHumanLocomotion.h"
#include "Task/Movement/TaskCollisionResponse.h"
#include "Task/Movement/TaskGoTo.h"
#include "Task/Movement/TaskGetUp.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskNMFlinch.h"
#include "Task/Physics/TaskNMRelax.h"
#include "Task/Response/TaskFlee.h"
#include "Task/Response/TaskReactAndFlee.h"
#include "Task/Scenario/ScenarioDebug.h"
#include "Task/Scenario/ScenarioPointManager.h"
#include "Task/Scenario/Info/ScenarioActionManager.h"
#include "Task/Scenario/info/ScenarioInfo.h"
#include "Task/Scenario/info/ScenarioInfoConditions.h"
#include "Task/Scenario/info/ScenarioInfoManager.h"
#include "Task/Scenario/Types/TaskCowerScenario.h"
 
AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CTaskUseScenarioEntityExtension
//////////////////////////////////////////////////////////////////////////

FW_INSTANTIATE_CLASS_POOL(CTaskUseScenarioEntityExtension, CONFIGURED_FROM_FILE, atHashString("CTaskUseScenarioEntityExtension",0x6eb2989));		
AUTOID_IMPL(CTaskUseScenarioEntityExtension);

void CTaskUseScenarioEntityExtension::AddTask(CTaskUseScenario& task)
{
	taskAssertf(FindIndexOfTaskInArray(&task) < 0, "CTaskUseScenario already in CTaskUseScenarioEntityExtension's list.");

	if(!taskVerifyf(!m_Tasks.IsFull(), "Too many scenario users of a single object, increase CTaskUseScenarioEntityExtension::kMaxScenarioTasksPerObject (%d)", kMaxScenarioTasksPerObject))
	{
		return;
	}

	m_Tasks.Append() = &task;
}


void CTaskUseScenarioEntityExtension::RemoveTask(CTaskUseScenario& task)
{
	int index = FindIndexOfTaskInArray(&task);

	// Normally, we expect to find the task in the array, but there are some cases where we clear
	// the references to tasks as they get deleted (for MT reasons). In that case, there should still
	// be an empty slot, so we can search for that, and remove it (nothing else cleans them up).
	if(index < 0)
	{
		index = FindIndexOfTaskInArray(NULL);
	}

	if(!taskVerifyf(index >= 0, "Tried to remove a CTaskUseScenario from CTaskUseScenarioEntityExtension that wasn't in the list."))
	{
		return;
	}

	// This is essentially a DeleteFast(), except clearing the reference at the end of the array properly.
	const int lastIndex = m_Tasks.GetCount() - 1;
	if(index != lastIndex)
	{
		m_Tasks[index] = m_Tasks[lastIndex];
	}
	m_Tasks[lastIndex] = NULL;
	m_Tasks.SetCount(lastIndex);
}


void CTaskUseScenarioEntityExtension::NotifyTasksOfNewObject(CObject& obj)
{
	const int cnt = m_Tasks.GetCount();
	for(int i = 0; i < cnt; i++)
	{
		CTask* pTask = m_Tasks[i];
		if(taskVerifyf(pTask, "Expected non-NULL task in CTaskUseScenarioEntityExtension."))
		{
			taskAssert(pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO);
			static_cast<CTaskUseScenario*>(pTask)->NotifyNewObjectFromDummyObject(obj);
		}
	}
}


int CTaskUseScenarioEntityExtension::FindIndexOfTaskInArray(CTaskUseScenario* pTask)
{
	const int cnt = m_Tasks.GetCount();
	for(int i = 0; i < cnt; i++)
	{
		if(m_Tasks[i] == pTask)
		{
			return i;
		}
	}
	return -1;
}

//////////////////////////////////////////////////////////////////////////
// CTaskUseScenario
//////////////////////////////////////////////////////////////////////////
const float CTaskUseScenario::fMAX_IDLE_TIME = 100000.0f;

CTaskUseScenario::Tunables CTaskUseScenario::sm_Tunables;
u32 CTaskUseScenario::sm_NextTimeAnybodyCanGetUpMS = 0;
float CTaskUseScenario::sm_fLastCowerPanicRate = 0.0f;
float CTaskUseScenario::sm_fLastRate = 0.0f;
float CTaskUseScenario::sm_fUninitializedIdleTime = -2.0f; // Should be < -1, which means "infinite idle time."
bool CTaskUseScenario::sm_bDetachScenarioPeds = true;
bool CTaskUseScenario::sm_bAggressiveDetachScenarioPeds = false;

IMPLEMENT_SCENARIO_TASK_TUNABLES(CTaskUseScenario, 0x27b0e9ae);

dev_float sfAmbientEventIgnoreTime = 20.0f;
dev_float sfAmbientEventResetTime = 35.0f;

SCENARIO_DEBUG_ONLY(bool CTaskUseScenario::sm_bDoPavementCheck = true;)
bool CTaskUseScenario::sm_bIgnoreBlackboardForScenarioReactions = false;

#if !__64BIT
CompileTimeAssert(sizeof(CTaskUseScenario) <= 464);
#endif

CTaskUseScenario::CTaskUseScenario(s32 iScenarioIndex, s32 iFlags)
: CTaskScenario(iScenarioIndex, NULL)
, m_Target()
, m_vPosition(Vector3::ZeroType)
, m_pConditionalAnimsGroup(NULL)
, m_eScenarioMode(SM_WorldLocation)
, m_fIdleTime(sm_fUninitializedIdleTime)
, m_fHeading(0.0f)
, m_fBaseHeadingOffset(0.0f)
, m_fBlockingAreaCheck(0.0f)
, m_panicBaseClipSetId(CLIP_SET_ID_INVALID)
, m_SyncedSceneBlendOutPhase(1.0f)
, m_OverriddenMoveBlendRatio(-1.0f)
, m_fLastEventTimer(0.0f)
, m_fLastPavementCheckTimer(0.0f)
, m_TDynamicObjectIndex(DYNAMIC_OBJECT_INDEX_NONE)
, m_PavementHelper()
, m_iCachedThreatResponse(CTaskThreatResponse::TR_None)
, m_iFlags(iFlags)
, m_ConditionalAnimsChosen(-1)
, m_uCachedBoundsFlags(0)
, m_SyncedSceneId(INVALID_SYNCED_SCENE_ID)
, m_iEventTriggeringPanic(-1)
, m_iPriorityOfEventTriggeringPanic(-1)
, m_iLastEventType(-1)
, m_iScenarioReactionFlags()
, m_iExitClipsFlags()
, m_iShiftedBoundsCountdown(0)
, m_bIgnoreLowPriShockingEvents(false)
, m_bNeedsCleanup(false)
, m_bSlaveReadyForSyncedExit(false)
, m_bConsiderOrientation(false)
, m_bFinalApproachSet(false)
, m_bExitToLowFall(false)
, m_bExitToHighFall(false)
, m_bHasExitStartPos(false)
, m_bDefaultTransitionRequested(false)
, m_bCheckedScenarioPointForObstructions(false)
, mbNewTaskMigratedWhileCowering(false)
, mbOldTaskMigratedWhileCowering(false)
, m_bMedicTypeScenario(false)
, m_bAllowDestructionEvenInMPForMissingEntity(false)
, m_pClonesParamedicScenarioPoint(NULL)
, m_bHasTaskAmbientMigrateHelper(false)
, m_WasInScope(false)
#if SCENARIO_DEBUG
, m_bIgnorePavementCheckWhenLeavingScenario(false)
, m_bCheckedEnterAnim(false)
, m_pDebugInfo(NULL)
#endif // SCENARIO_DEBUG
{
	SetChosenClipId(CLIP_ID_INVALID, NULL, this);
	m_iFlags.SetFlag(SF_StartInCurrentPosition);
	SetInternalTaskType(CTaskTypes::TASK_USE_SCENARIO);
}


CTaskUseScenario::CTaskUseScenario(s32 iScenarioIndex, const Vector3& vPosition, float fHeading, s32 iFlags)
: CTaskScenario(iScenarioIndex, NULL)
, m_Target()
, m_vPosition(vPosition)
, m_pConditionalAnimsGroup(NULL)
, m_eScenarioMode(SM_WorldLocation)
, m_fIdleTime(sm_fUninitializedIdleTime)
, m_fHeading(fHeading)
, m_fBaseHeadingOffset(0.0f)
, m_fBlockingAreaCheck(0.0f)
, m_panicBaseClipSetId(CLIP_SET_ID_INVALID)
, m_SyncedSceneBlendOutPhase(1.0f)
, m_OverriddenMoveBlendRatio(-1.0f)
, m_fLastEventTimer(0.0f)
, m_fLastPavementCheckTimer(0.0f)
, m_TDynamicObjectIndex(DYNAMIC_OBJECT_INDEX_NONE)
, m_PavementHelper()
, m_iCachedThreatResponse(CTaskThreatResponse::TR_None)
, m_iFlags(iFlags)
, m_ConditionalAnimsChosen(-1)
, m_uCachedBoundsFlags(0)
, m_SyncedSceneId(INVALID_SYNCED_SCENE_ID)
, m_iEventTriggeringPanic(-1)
, m_iPriorityOfEventTriggeringPanic(-1)
, m_iLastEventType(-1)
, m_iScenarioReactionFlags()
, m_iExitClipsFlags()
, m_iShiftedBoundsCountdown(0)
, m_bIgnoreLowPriShockingEvents(false)
, m_bNeedsCleanup(false)
, m_bSlaveReadyForSyncedExit(false)
, m_bConsiderOrientation(false)
, m_bFinalApproachSet(false)
, m_bExitToLowFall(false)
, m_bExitToHighFall(false)
, m_bHasExitStartPos(false)
, m_bDefaultTransitionRequested(false)
, m_bCheckedScenarioPointForObstructions(false)
, mbNewTaskMigratedWhileCowering(false)
, mbOldTaskMigratedWhileCowering(false)
, m_bMedicTypeScenario(false)
, m_bAllowDestructionEvenInMPForMissingEntity(false)
, m_pClonesParamedicScenarioPoint(NULL)
, m_bHasTaskAmbientMigrateHelper(false)
, m_WasInScope(false)
#if SCENARIO_DEBUG
, m_bIgnorePavementCheckWhenLeavingScenario(false)
, m_bCheckedEnterAnim(false)
, m_pDebugInfo(NULL)
#endif // SCENARIO_DEBUG
{
	SetChosenClipId(CLIP_ID_INVALID, NULL, this);
	SetInternalTaskType(CTaskTypes::TASK_USE_SCENARIO);
}

CTaskUseScenario::CTaskUseScenario(s32 iScenarioIndex, CScenarioPoint* pScenarioPoint, s32 iFlags)
: CTaskScenario(iScenarioIndex, pScenarioPoint)
, m_Target()
, m_vPosition(Vector3::ZeroType)
, m_pConditionalAnimsGroup(NULL)
, m_eScenarioMode(SM_EntityEffect)
, m_fIdleTime(sm_fUninitializedIdleTime)
, m_fHeading(0.0f)
, m_fBaseHeadingOffset(0.0f)
, m_fBlockingAreaCheck(0.0f)
, m_panicBaseClipSetId(CLIP_SET_ID_INVALID)
, m_SyncedSceneBlendOutPhase(1.0f)
, m_OverriddenMoveBlendRatio(-1.0f)
, m_fLastEventTimer(0.0f)
, m_fLastPavementCheckTimer(0.0f)
, m_TDynamicObjectIndex(DYNAMIC_OBJECT_INDEX_NONE)
, m_PavementHelper()
, m_iCachedThreatResponse(CTaskThreatResponse::TR_None)
, m_iFlags(iFlags)
, m_ConditionalAnimsChosen(-1)
, m_uCachedBoundsFlags(0)
, m_SyncedSceneId(INVALID_SYNCED_SCENE_ID)
, m_iEventTriggeringPanic(-1)
, m_iPriorityOfEventTriggeringPanic(-1)
, m_iLastEventType(-1)
, m_iScenarioReactionFlags()
, m_iExitClipsFlags()
, m_iShiftedBoundsCountdown(0)
, m_bIgnoreLowPriShockingEvents(false)
, m_bNeedsCleanup(false)
, m_bSlaveReadyForSyncedExit(false)
, m_bConsiderOrientation(false)
, m_bFinalApproachSet(false)
, m_bExitToLowFall(false)
, m_bExitToHighFall(false)
, m_bHasExitStartPos(false)
, m_bDefaultTransitionRequested(false)
, m_bCheckedScenarioPointForObstructions(false)
, mbNewTaskMigratedWhileCowering(false)
, mbOldTaskMigratedWhileCowering(false)
, m_bMedicTypeScenario(false)
, m_bAllowDestructionEvenInMPForMissingEntity(false)
, m_bHasTaskAmbientMigrateHelper(false)
, m_pClonesParamedicScenarioPoint(NULL)
, m_WasInScope(false)
#if SCENARIO_DEBUG
, m_bIgnorePavementCheckWhenLeavingScenario(false)
, m_bCheckedEnterAnim(false)
, m_pDebugInfo(NULL)
#endif // SCENARIO_DEBUG
{
	SetChosenClipId(CLIP_ID_INVALID, NULL, this);

	if(!GetScenarioPoint())
	{
		m_eScenarioMode = SM_WorldLocation;
	}
	else
	{
		CEntity* pScenarioEntity = GetScenarioEntity();
		if(!pScenarioEntity)
		{
			m_eScenarioMode = SM_WorldEffect;
		}
		else if(pScenarioEntity->GetIsTypeObject())
		{
			// If there is a CDummyObject associated with this CObject, store a pointer to it, register us
			// so we get notified if it turns into a CObject again later, and remember which scenario point
			// we were using, in case there is more than one.
			CDummyObject* pDummyObject = static_cast<CObject*>(pScenarioEntity)->GetRelatedDummy();
			SetDummyObjectForAttachedEntity(pDummyObject);
			if(pDummyObject)
			{
				const CScenarioPoint* pt = GetScenarioPoint();
				m_DummyObjectScenarioPtOffsetAndType.SetXYZ(pt->GetLocalPosition());
				m_DummyObjectScenarioPtOffsetAndType.SetWi(pt->GetScenarioTypeVirtualOrReal());
			}
		}
	}
	SetInternalTaskType(CTaskTypes::TASK_USE_SCENARIO);
}

CTaskUseScenario::CTaskUseScenario(const CTaskUseScenario& other)
: CTaskScenario(other)
, m_PlayAttachedClipHelper(other.m_PlayAttachedClipHelper)
, m_Target()
, m_vPosition(other.m_vPosition)
, m_pConditionalAnimsGroup(other.m_pConditionalAnimsGroup)
, m_SynchronizedPartner(other.m_SynchronizedPartner)
, m_eScenarioMode(other.m_eScenarioMode)
, m_fIdleTime(other.m_fIdleTime)
, m_fHeading(other.m_fHeading)
, m_fBaseHeadingOffset(other.m_fBaseHeadingOffset)
, m_fBlockingAreaCheck(0.0f)
, m_pPropHelper(other.m_pPropHelper)
, m_panicBaseClipSetId(CLIP_SET_ID_INVALID)
, m_SyncedSceneBlendOutPhase(1.0f)
, m_OverriddenMoveBlendRatio(other.m_OverriddenMoveBlendRatio)
, m_fLastEventTimer(0.0f)
, m_fLastPavementCheckTimer(0.0f)
, m_TDynamicObjectIndex(DYNAMIC_OBJECT_INDEX_NONE)
, m_PavementHelper() // Intentionally not copied.
, m_iCachedThreatResponse(CTaskThreatResponse::TR_None)
, m_iFlags(other.m_iFlags)
, m_ConditionalAnimsChosen(other.m_ConditionalAnimsChosen)
, m_uCachedBoundsFlags(0)
, m_SyncedSceneId(INVALID_SYNCED_SCENE_ID)	// Probably wouldn't do any good to copy this.
, m_iEventTriggeringPanic(-1)
, m_iPriorityOfEventTriggeringPanic(-1)
, m_iLastEventType(-1)
, m_iScenarioReactionFlags()
, m_iExitClipsFlags()
, m_iShiftedBoundsCountdown(0)
, m_bIgnoreLowPriShockingEvents(false)	// Intentionally false, probably shouldn't copy this (or it must at least be done through SetIgnoreLowPriShockingEvents()).
, m_bNeedsCleanup(false)				// Intentionally not copied.
, m_bSlaveReadyForSyncedExit(false)		// Intentionally not copied.
, m_bConsiderOrientation(false)
, m_bFinalApproachSet(false)
, m_bExitToLowFall(false)
, m_bExitToHighFall(false)
, m_bHasExitStartPos(other.m_bHasExitStartPos)
, m_bDefaultTransitionRequested(false)
, m_bCheckedScenarioPointForObstructions(false)
, mbNewTaskMigratedWhileCowering(false)
, mbOldTaskMigratedWhileCowering(false)
, m_bMedicTypeScenario(false)
, m_bAllowDestructionEvenInMPForMissingEntity(false)
, m_bHasTaskAmbientMigrateHelper(false)
, m_pClonesParamedicScenarioPoint(NULL)
, m_WasInScope(false)
#if SCENARIO_DEBUG
, m_bIgnorePavementCheckWhenLeavingScenario(false)
, m_bCheckedEnterAnim(false)
, m_pDebugInfo(NULL)
#endif // SCENARIO_DEBUG
{
	SetChosenClipId(other.GetChosenClipId(NULL, this), NULL, this);

	SetDummyObjectForAttachedEntity(other.m_DummyObjectForAttachedEntity);
	m_DummyObjectScenarioPtOffsetAndType = other.m_DummyObjectScenarioPtOffsetAndType;

	// Copy this property in case it was changed by the user from what
	// CTaskScenario's constructor sets it to.

	if(other.m_pAdditionalTaskDuringApproach)
	{
		aiTask* pCopiedAdditionalTask = other.m_pAdditionalTaskDuringApproach->Copy();
		taskAssert(dynamic_cast<CTask*>(pCopiedAdditionalTask));
		SetAdditionalTaskDuringApproach(static_cast<CTask*>(pCopiedAdditionalTask));
	}

	// Clear the bound capsule restore flag on the copy.  The original task will be force aborted as soon as this function returns (at least when used
	// as a part of CNetObjPed::CreateCloneTasksFromLocalTasks), which will reset the bounds offset on the ped.  However, 
	// the newly copied task needs this copied flag to be cleared so that it can properly offset the bounds when it starts to run.
	m_iFlags.ClearFlag(SF_CapsuleNeedsToBeRestored);

	SetInternalTaskType(CTaskTypes::TASK_USE_SCENARIO);
}

CTaskUseScenario::~CTaskUseScenario()
{
	taskAssert(!m_bIgnoreLowPriShockingEvents);		// CleanUp() should prevent this too.

	taskAssert(m_SyncedSceneId == INVALID_SYNCED_SCENE_ID);

	m_PavementHelper.CleanupPavementFloodFillRequest();

	if(m_pAdditionalTaskDuringApproach)
	{
		delete m_pAdditionalTaskDuringApproach;
		m_pAdditionalTaskDuringApproach = NULL;
	}

	// CleanUp() should prevent this assert.
	taskAssertf(!m_pPropHelper || !m_pPropHelper->IsPropLoaded() || m_pPropHelper->IsPropSharedAmongTasks(),
		"%s Prop name hash [0x%x], IsPropLoading %s IsPropNotRequired %s",
		(GetEntity() && GetPed())?GetPed()->GetDebugName():"Null ped",
		m_pPropHelper->GetPropNameHash().GetHash(),
		m_pPropHelper->IsPropLoading()?"TRUE":"FALSE",
		m_pPropHelper->IsPropNotRequired()?"TRUE":"FALSE");

#if SCENARIO_DEBUG
	if (m_pDebugInfo)
	{
		delete m_pDebugInfo;
	}
#endif // SCENARIO_DEBUG

	if(m_TDynamicObjectIndex!=DYNAMIC_OBJECT_INDEX_NONE)
	{
		CPathServerGta::RemoveBlockingObject(m_TDynamicObjectIndex);
		m_TDynamicObjectIndex=DYNAMIC_OBJECT_INDEX_NONE;
	}

	// Make sure the bound include flags were properly restored.
	taskAssert(m_uCachedBoundsFlags == 0);

	if(m_pClonesParamedicScenarioPoint)
	{
		RemoveScenarioPoint();
		delete m_pClonesParamedicScenarioPoint;
	}

	// Make sure to unregister the CTaskUseScenarioEntityExtension from the CDummyObject. We do use
	// registered references for the tasks so it wouldn't necessarily be a huge disaster if we didn't, but
	// it's better to not be sloppy about it.
	SetDummyObjectForAttachedEntity(NULL);
}

fwMvClipId const& CTaskUseScenario::GetChosenClipId(CPed const* UNUSED_PARAM(pPed), CTaskUseScenario const* UNUSED_PARAM(pTask)) const
{
#if 0/*__BANK*/

	if(NetworkInterface::IsGameInProgress() && pPed && pPed->GetNetworkObject())
	{
		NetworkLogUtils::WritePlayerText(netInterface::GetObjectManager().GetLog(), pPed->GetNetworkObject()->GetPhysicalPlayerIndex(), "CTASKUSESCENARIO::GETCHOSENCLIPID",
		" : ped %p %s %s : task %p %s : %s calling CTaskUseScenario::GetChosenClipId : returning %d %s",
		pPed,
		pPed ? pPed->GetDebugName() : "NULL",
		pPed && pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "NULL NET OBJ",
		pTask,
		pTask ? pTask->GetStaticStateName(pTask->GetState()) : "NULL TASK",
		callerFunc,
		m_ChosenClipId.GetHash(),
		m_ChosenClipId.GetCStr()
		);
	}

#endif

	return m_ChosenClipId; 
}

void CTaskUseScenario::SetChosenClipId(fwMvClipId const& clipId, CPed const* UNUSED_PARAM(pPed), CTaskUseScenario const* UNUSED_PARAM(pTask))
{
#if 0/*__BANK*/

	if(NetworkInterface::IsGameInProgress() && pPed && pPed->GetNetworkObject())
	{
		NetworkLogUtils::WritePlayerText(netInterface::GetObjectManager().GetLog(), pPed->GetNetworkObject()->GetPhysicalPlayerIndex(), "CTASKUSESCENARIO::SETCHOSENCLIPID",
		" : ped %p %s %s : task %p %s : %s calling CTaskUseScenario::SetChosenClipId : From %d %s: To %d %s",
		pPed,
		pPed ? pPed->GetDebugName() : "NULL",
		pPed && pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "NULL NET OBJ",
		pTask,
		pTask ? pTask->GetStaticStateName(pTask->GetState()) : "NULL TASK",
		callerFunc,
		m_ChosenClipId.GetHash(),
		m_ChosenClipId.GetCStr(),
		clipId.GetHash(),
		clipId.GetCStr()
		);
	}

#endif

	m_ChosenClipId = clipId; 
}

fwMvClipSetId const& CTaskUseScenario::GetEnterClipSetId(CPed const* UNUSED_PARAM(pPed), CTaskUseScenario const* UNUSED_PARAM(pTask)) const
{
#if 0/*__BANK*/

	if(NetworkInterface::IsGameInProgress() && pPed && pPed->GetNetworkObject())
	{
		NetworkLogUtils::WritePlayerText(netInterface::GetObjectManager().GetLog(), pPed->GetNetworkObject()->GetPhysicalPlayerIndex(), "CTASKUSESCENARIO::GETENTERCLIPSETID",
		" : ped %p %s %s : task %p %s : %s calling CTaskUseScenario::GetEnterClipSetId : returning %d %s",
		pPed,
		pPed ? pPed->GetDebugName() : "NULL",
		pPed && pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "NULL NET OBJ",
		pTask,
		pTask ? pTask->GetStaticStateName(pTask->GetState()) : "NULL TASK",
		callerFunc,
		m_enterClipSetId.GetHash(),
		m_enterClipSetId.GetCStr()
		);
	}

#endif

	return m_enterClipSetId; 
}

void CTaskUseScenario::SetEnterClipSetId(fwMvClipSetId const& clipSetId, CPed const* UNUSED_PARAM(pPed), CTaskUseScenario const* UNUSED_PARAM(pTask))
{
#if 0/*__BANK*/

	if(NetworkInterface::IsGameInProgress() && pPed && pPed->GetNetworkObject())
	{
		NetworkLogUtils::WritePlayerText(netInterface::GetObjectManager().GetLog(), pPed->GetNetworkObject()->GetPhysicalPlayerIndex(), "CTASKUSESCENARIO::SETENTERCLIPSETID",
		" : ped %p %s %s : task %p %s : %s calling CTaskUseScenario::SetEnterClipSetId : From %d %s: To %d %s",
		pPed,
		pPed ? pPed->GetDebugName() : "NULL",
		pPed && pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "NULL NET OBJ",
		pTask,
		pTask ? pTask->GetStaticStateName(pTask->GetState()) : "NULL TASK",
		callerFunc,
		m_enterClipSetId.GetHash(),
		m_enterClipSetId.GetCStr(),
		clipSetId.GetHash(),
		clipSetId.GetCStr()
		);
	}
#endif

	m_enterClipSetId = clipSetId; 
}

void CTaskUseScenario::CleanUp()
{
	taskAssert(m_SyncedSceneId == INVALID_SYNCED_SCENE_ID);

	m_PavementHelper.CleanupPavementFloodFillRequest();

	CPed* pPed = GetPed();

	CNetObjPed*	netObjPed	= static_cast<CNetObjPed *>(pPed->GetNetworkObject());

	if(netObjPed && !GetIsFlagSet(aiTaskFlags::HandleCloneSwapToSameTaskType))
	{
		netObjPed->SetOverrideScopeBiggerWorldGridSquare(false);
	}


	if(GetClipHelper())
	{
		float fBlendOutDelta = SLOW_BLEND_OUT_DELTA;

		if ( (m_iScenarioIndex >= 0) && SCENARIOINFOMGR.GetScenarioInfoByIndex(GetScenarioType()))
		{
			fBlendOutDelta = fabs(GetScenarioInfo().GetOutroBlendOutDuration());
			// Metadata stores it in seconds, StopClip() expects things to be 1 / seconds.
			fBlendOutDelta = fBlendOutDelta == 0.0f ? INSTANT_BLEND_OUT_DELTA : (-1.0f / fBlendOutDelta);
		}
		
		bool bCloneSkipReactInReactAndFlee = false;

		if(pPed->IsNetworkClone() && pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_REACT_AND_FLEE ) )
		{
			CTaskInfo* pTaskInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_REACT_AND_FLEE, PED_TASK_PRIORITY_MAX, false);

			if(taskVerifyf(pTaskInfo, "%s Expect task info",pPed->GetDebugName()))
			{
				CClonedReactAndFleeInfo* pTaskReactAndFleeInfo = static_cast<CClonedReactAndFleeInfo*>(pTaskInfo);
				
				u8 uConfigFlags = pTaskReactAndFleeInfo->GetControlFlags();

				if(uConfigFlags & CTaskReactAndFlee::CF_DisableReact)
				{
					bCloneSkipReactInReactAndFlee = true;
				}
			}
		}

		// If we've decided to not play react and flee animations, make sure we don't instantly blend out,
		// we need the animation to keep playing while we blend in the react-and-flee move network.
		if(bCloneSkipReactInReactAndFlee || pPed->GetPedResetFlag(CPED_RESET_FLAG_SkipReactInReactAndFlee))
		{
			fBlendOutDelta = REALLY_SLOW_BLEND_OUT_DELTA;
		}

		// Blend slowly if exiting to a fall.
		if (m_bExitToHighFall)
		{
			fBlendOutDelta = REALLY_SLOW_BLEND_OUT_DELTA;
		}

		//Calculate the flags.
		u32 uFlags = !m_iExitClipsFlags.IsFlagSet(eExitClipsEndsInWalk) ? 0 : CMovePed::Task_TagSyncTransition;
		
		//Stop the clip.
		StopClip(fBlendOutDelta, uFlags);
	}

	// Reset the event that triggered the panic, so if we resume this task we can react again, B*1633011
	if (NetworkInterface::IsGameInProgress() && pPed && pPed->PopTypeIsMission())
	{
		m_iEventTriggeringPanic = -1;
	}

	if (m_bHasTaskAmbientMigrateHelper && pPed->GetIsAttached() && sm_bAggressiveDetachScenarioPeds)
	{
		pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
	}

	// Detach and re-enable collisions, but only if we haven't already done so.
	if (pPed && m_bNeedsCleanup)
	{
		pPed->SetNoCollision(NULL, 0);

		const CScenarioPoint* pPoint = GetScenarioPoint();
		if(pPoint && pPoint->IsFlagSet(CScenarioPointFlags::ResetNoCollisionOnCleanUp))
		{
			pPed->ResetNoCollision();
		}

		if (pPed->GetIsAttached())
		{
			bool bDetach = !(GetTaskAmbientMigrateHelper() && !HasTriggeredExit() && !IsInProcessPostMovementAttachState()) && !mbOldTaskMigratedWhileCowering;

			if (m_bHasTaskAmbientMigrateHelper && sm_bDetachScenarioPeds)
			{
				bDetach = true;
			}

			if (bDetach)
			{
				pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
			}
		}

		// Now that the ped is done with the scenario, toggle off fixed physics so the scenario entity (if it exists) can be pushed around.
		ToggleFixedPhysicsForScenarioEntity(false);
		m_bNeedsCleanup = false;

		SetUsingScenario(false);

		// Nothing but this task currently sets this flag to true, and if it's still true now when the
		// task is ending, we should probably clear it so it doesn't have any effect beyond the
		// lifetime of the task.
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ActivateOnSwitchFromLowPhysicsLod, false);

		// Reset the collision state.
		TogglePedCapsuleCollidesWithInactives(false);

		// Just in case the ped left PlayAmbients without OnExit begin called.
		if (m_iFlags.IsFlagSet(SF_CapsuleNeedsToBeRestored))
		{
			ManagePedBoundForScenario(false);
		}

		// Restore the ragdoll activation properties of the ped.
		if (SCENARIOINFOMGR.GetScenarioInfoByIndex(GetScenarioType()) && GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::RagdollEasilyFromPlayerContact))
		{
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ActivateRagdollFromMinorPlayerContact, false);
		}

		// Clear the stored clip information in the exit clip helper.
		m_ExitHelper.Reset();
		m_ExitHelper.ResetPreviousWinners();
	}

	// fix to prevent peds getting left attached after migration 
	if (pPed->GetIsAttached() && m_bHasTaskAmbientMigrateHelper)
	{
		pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
	}

	// If you've allocated a prop helper, we need to clean it up
	if (m_pPropHelper)
	{
		if (pPed)
		{
			// If the UsePropFromEnvironment flag is set, make sure we don't leave the scenario with the prop still attached.
			if(SCENARIOINFOMGR.GetScenarioInfoByIndex(GetScenarioType()) && GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::UsePropFromEnvironment))
			{
				if(!m_pPropHelper->IsHoldingProp())
				{
					m_pPropHelper->UpdateExistingOrMissingProp(*pPed);
				} 
				else 
				{
					// Unlike when dropping the prop due to a tag being found in an animation, here we probably
					// need to activate physics, since we don't know if the prop is in a good position.
					const bool activatePhys = true;
					m_pPropHelper->DropProp(*pPed, activatePhys);
				}
			}
			// If you weren't UsePropFromEnvironment but we have a prop, we still need to do cleanup for it. [11/15/2012 mdawe]
			else
			{
				m_pPropHelper->ReleasePropRefIfUnshared(pPed);
			}
		}
		m_pPropHelper.free();
	}

	RemoveScenarioPoint();

	// Make sure we don't leave a count on CPedIntelligence::m_iIgnoreLowPriShockingEventsCount.
	SetIgnoreLowPriShockingEvents(false);

	// Stop ignoring the walked into event.
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DisablePotentialToBeWalkedIntoResponse, false);

	// Can't be sitting down when you leave a scenario.
	pPed->SetIsSitting(false);

	// Note: we intentionally don't call SetStationaryReactionsEnabledForPed(..., false) here,
	// because the whole point of that is leave it on for the next task if we got aborted
	// (and if we timed out, it would be cleared elsewhere).

	// In the event that the scenario action manager determined what kind of threat response decision was performed,
	// we need to force the new task to use that value again here so the reaction is consistent.
	// The response task is created freshly every frame and so it has to be done right before the task exits.
	aiTask* pEventResponseTask = pPed->GetPedIntelligence()->GetEventResponseTask();
	if (pEventResponseTask && pEventResponseTask->GetTaskType() == CTaskTypes::TASK_THREAT_RESPONSE)
	{
		if (m_iCachedThreatResponse != CTaskThreatResponse::TR_None)
		{
			CTaskThreatResponse* pTaskThreatResponse = static_cast<CTaskThreatResponse*>(pEventResponseTask);
			pTaskThreatResponse->SetThreatResponseOverride(m_iCachedThreatResponse);
		}
	}

	if(m_TDynamicObjectIndex!=DYNAMIC_OBJECT_INDEX_NONE)
	{
		CPathServerGta::RemoveBlockingObject(m_TDynamicObjectIndex);
		m_TDynamicObjectIndex=DYNAMIC_OBJECT_INDEX_NONE;
	}

	if (m_iScenarioReactionFlags.IsFlagSet(SREF_AggroOrCowardReaction))
	{
		CTaskUnalerted* pTaskUnalerted = static_cast<CTaskUnalerted*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_UNALERTED));
		if (pTaskUnalerted)
		{
			pTaskUnalerted->SetWanderPathFrameDelay(15);	// This is to prevent two paths being created and screwing up the walkstart/runstart
		}
	}
}

void CTaskUseScenario::DoAbort(const AbortPriority priority, const aiEvent* pEvent)
{
	CTaskScenario::DoAbort(priority, pEvent);

	// We will resume from the Start state, which expects that m_ConditionalAnimsChosen to be -1.
	m_ConditionalAnimsChosen = -1;

	// Reset chosen clip.
	SetChosenClipId(CLIP_ID_INVALID, GetPed(), this);

	// Reset chosen clipsets.
	m_baseClipSetId = CLIP_SET_ID_INVALID;
	SetEnterClipSetId(CLIP_SET_ID_INVALID, GetPed(), this);
	m_exitClipSetId = CLIP_SET_ID_INVALID;
	m_panicClipSetId = CLIP_SET_ID_INVALID;
	m_panicBaseClipSetId = CLIP_SET_ID_INVALID;

	// Reset internal reaction state flags - with the exception of SREF_AggroOrCowardReaction
	u16 TempFlags = (m_iScenarioReactionFlags.GetAllFlags() & (SREF_AggroOrCowardReaction));
	m_iScenarioReactionFlags.ClearAllFlags();
	m_iScenarioReactionFlags.SetFlag(TempFlags);

	// Reset certain scenario flags.
	m_iFlags.ClearFlag(SF_ShouldAbort | SF_ForceHighPriorityStreamingForNormalExit | SF_ScenarioResume);

	// Should probably reset this too, since we're starting over.
	ResetExitAnimations();

	m_PavementHelper.CleanupPavementFloodFillRequest();
		

	// Note: we may need to do more things here for this to work properly.
	// For example, even if we warped to the scenario in the first place, we probably
	// wouldn't want to warp back if we got knocked off.
}

// ShouldAbort for TaskUseScenario typically holds off on responding to an event until a suitable exit animation can be played.
// The metadata system for determining these exit animations can be found in scenariotriggers.meta.
bool CTaskUseScenario::ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	s32 iState = GetState();

	// Any immediate request should be allowed to abort unless our subtask cant be aborted
	if (CTask::ABORT_PRIORITY_IMMEDIATE==iPriority)
	{
		if (GetSubTask()) 
		{
			if (GetSubTask()->MakeAbortable(iPriority,pEvent))
			{
				return true;
			}
			return false;
		}
		return true;
	}

	// If the ped has not gotten into the scenario yet, then just quit.
	if (iState == State_Start || iState == State_GoToStartPosition)
	{
		// if we are not a scenario that ignores obstructions and we have reached our static event count maximum
		if (!GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::IgnoreObstructions)
			&& pEvent && pEvent->GetEventType() == EVENT_STATIC_COUNT_REACHED_MAX)
		{
			CScenarioPoint* pPoint = GetScenarioPoint();
			if (pPoint)
			{
				// if we are not obstructed, mark the point as obstructed UNLESS this is a player ped.
				if (!pPoint->IsObstructedAndReset() && !pPed->IsAPlayerPed()) 
				{
					pPoint->SetObstructed();
				}
			}
		}
		return true;
	}

	// Formerly the ped would only respond to events if it was in State_PlayAmbients or PanicExit
	// Changed to allow all states to be handled by events.  However, the ideal way to handle this would be to
	// tag all enter animations with a phase and only handle the event if that critical point is reached.
	// Otherwise blend out immediately to handle the event response.
	// Only proceed if the event is valid.
	if (pEvent)
	{
		//Special case for ragdoll events:
		// Have a look at the event response task, if its a reaction that uses the ragdoll, then allow the task to abort
		aiTask* pEventResponseTask = pPed->GetPedIntelligence()->GetPhysicalEventResponseTask();

		if (!pEventResponseTask)
		{
			pEventResponseTask = pPed->GetPedIntelligence()->GetEventResponseTask();
		}

		const bool bNmRagdollTask = pEventResponseTask && CTaskNMBehaviour::DoesResponseHandleRagdoll(pPed, pEventResponseTask);

		// check if a ragdoll subtask would allow us to abort
		if (bNmRagdollTask)
		{
			if (GetSubTask()) 
			{
				if (GetSubTask()->MakeAbortable(CTask::ABORT_PRIORITY_URGENT,pEvent))
				{
					return true;
				}
				nmEntityDebugf(GetPed(), "CTaskUseScenario, Not aborting for ragdoll event %s, as the sub task didn't abort", pEvent ? pEvent->GetName() : "none");
				return false;
			}
			return true;
		}

		const CEvent* pEEvent = (const CEvent*)pEvent;

		if( pEEvent->RequiresAbortForMelee(pPed) )
			return true;

		if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COWER_SCENARIO)
		{
			// Cowering shouldn't flinch because of the event that triggered cowering to begin with, otherwise the ped would continually flinch as
			// that event persists during cowering.
			CTaskCowerScenario* pCowerTask = static_cast<CTaskCowerScenario*>(GetSubTask());
			pCowerTask->SetFlinchIgnoreEvent(pEEvent);
		}
		else if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COWER)
		{
			CTaskCower* pCowerTask = static_cast<CTaskCower*>(GetSubTask());
			pCowerTask->SetFlinchIgnoreEvent(pEEvent);
		}
		
		const CScenarioPoint* pScenarioPoint = GetScenarioPoint();
		if(pScenarioPoint)
		{
			//Check if we should ignore threats if our los is blocked.
			if (pScenarioPoint->IsFlagSet(CScenarioPointFlags::IgnoreThreatsIfLosNotClear))
			{
				//Check if this event is considered a threat, and we don't have a los.
				if(IsEventConsideredAThreat(*pEEvent) && !IsLosClearToThreatEvent(*pEEvent))
				{
					nmEntityDebugf(GetPed(), "CTaskUseScenario, Not aborting for event %s, as its not in line of sight", pEvent ? pEvent->GetName() : "none");
					return false;
				}
			}

			//Check to see if we're trying to start an investigation task
			if (pEvent->GetEventType() == EVENT_SUSPICIOUS_ACTIVITY && pScenarioPoint->DoesAllowInvestigation())
			{
				// Allow this scenario to exit.
				TriggerImmediateExitOnEvent();
			}

			if((pEvent->GetEventType() == EVENT_AGITATED) || (pEvent->GetEventType() == EVENT_AGITATED_ACTION))
			{
				OnLeavingScenarioDueToAgitated();
			}
		}

		// Look up the appropriate response to the event in metadata.
		if (m_iEventTriggeringPanic != pEEvent->GetEventType())
		{
			// Let the CTaskCowerScenario subtask deal with the event while cowering.
			// The exception to this are damage events which should be dealt with at this level as they are the only thing that can
			// kick a ped out of cower.
			if (GetState() == State_WaitingOnExit && pEEvent->GetEventType() != EVENT_DAMAGE)
			{
				if (pEEvent->GetEventType() == EVENT_SCRIPT_COMMAND || pEEvent->GetEventType() == EVENT_GIVE_PED_TASK)
				{
					// The ped was cowering, but now script has given them something else to do.
					// Skip the MakeAbortable() call below on the subtask and let the event response code take over to 
					// get the ped out of this task.
				}
				else
				{
					// Otherwise, send the event on to the cowering subtask so the ped can flinch/change stance as appropriate.
					CTask* pSubTask = GetSubTask();
					if (pSubTask && pSubTask->GetTaskType() != CTaskTypes::TASK_AMBIENT_CLIPS)
					{
						return pSubTask->MakeAbortable(iPriority, pEvent);
					}
				}
			}

			// When responding to an event, the conditional animations have to be chosen if they aren't already set.
			if (m_ConditionalAnimsChosen == -1)
			{
				DetermineConditionalAnims();
			}
			ScenarioActionContext sContext(this, *pEEvent, m_iEventTriggeringPanic, pEEvent->GetResponseTaskType());
			if (CScenarioActionManager::GetInstance().ExecuteTrigger(sContext))
			{
				StartEventReact(const_cast<CEvent&>(*pEEvent));
			}
		}
	}
	else
	{
		//Check if no event was specified.
		//Presumably we have been requested to abort through the MakeAbortable interface.
		//Perform a regular scenario exit.
		m_iFlags.SetFlag(SF_ShouldAbort);
	}

	// Only abort once the animations are done.
	if (m_iScenarioReactionFlags.IsFlagSet(SREF_AllowEventsToKillThisTask))
	{
		// Process the flee animations (if one was started) and keep the flee navmesh search alive on the ped.
		KeepNavMeshTaskAlive();
		// Force the motion state to be idle so that the runstarts are given a chance to work.
		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle);

		return true;
	}

	nmEntityDebugf(GetPed(), "CTaskUseScenario, Not aborting for event %s, default behaviour", pEvent ? pEvent->GetName() : "none");
	return false;
}

// Tell the ped to do an aggro reaction and then exit the task.
// If the ped was already doing an aggro reaction, then just exit anyway.
// If the ped was in the Goto state, then exit immediately.
void CTaskUseScenario::TriggerAggroThenExit(CPed* pPedAgitator)
{
	CPed* pPed = GetPed();
	if(pPed->IsNetworkClone())
	{
		return;
	}

	// If in a stationary point, ignore this trigger.  It could cause the ped to leave the point.
	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee))
	{
		ForgetAboutAgitation();
		return;
	}

	bool bAllowEventsToKillThisTask = false;
	if(GetState() == State_GoToStartPosition)
	{
		bAllowEventsToKillThisTask = true;
	}
	else if(GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::DoSimpleBlendoutForEvents))
	{
		if(GetState() == State_Enter)
		{
			bAllowEventsToKillThisTask = true;
		}
	}
	// Respect special flag that lets all dispute exits blend out immediately.
	if (GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::QuickExitsFromHassling))
	{
		CTaskAmbientClips* pTask = static_cast<CTaskAmbientClips *>(FindSubTaskOfType(CTaskTypes::TASK_AMBIENT_CLIPS));
		if (pTask)
		{
			pTask->SetCleanupBlendOutDelta(REALLY_SLOW_BLEND_OUT_DELTA);
		}

		bAllowEventsToKillThisTask = true;
	}

	if (bAllowEventsToKillThisTask)
	{
		// Even though there is no event, this flag will kill the task in ProcessPreFSM() of the next frame.
		// Better to do this now than wait for the ped to get all the way to the scenario point and finish playing their intro to get the event.
		m_iScenarioReactionFlags.SetFlag(SREF_AllowEventsToKillThisTask);
	}
	else if (!IsRespondingToEvent())
	{
		SetAmbientPosition(pPedAgitator);
		m_iScenarioReactionFlags.SetFlag(SREF_AggroAgitation);
		m_iScenarioReactionFlags.SetFlag(SREF_RespondingToEvent);
	}
	m_iScenarioReactionFlags.SetFlag(SREF_ExitScenario);

	OnLeavingScenarioDueToAgitated();
}

// Return true if the ambient position has yet to be set by script or code.
bool CTaskUseScenario::IsAmbientPositionInvalid() const
{
	return !m_Target.GetIsValid();
}

// Store off the position of where the event occured so the ped knows which direction to react towards.
void CTaskUseScenario::SetAmbientPosition(const aiEvent* pEvent)
{
	if(GetPed() && GetPed()->IsNetworkClone())
	{
		return;
	}

	if (pEvent)
	{
		CEvent* pEEvent = (CEvent*)pEvent;
		CEntity* pEntity = ShouldRespondToSourceEntity(pEEvent->GetEventType()) ? pEEvent->GetSourceEntity() : pEEvent->GetTargetEntity();
		if (pEntity)
		{
			m_Target.SetEntity(pEntity);
		}
		else
		{
			Assertf(!pEEvent->GetSourcePos().IsClose(VEC3_ZERO, SMALL_FLOAT), 
				"Event type %d had a position close to the origin!", pEEvent->GetEventType());
			m_Target.SetPosition(pEEvent->GetSourcePos());
		}
	}
}

// Return true if the ambient position for the reaction should be towards the source entity.
// Return false if the ambient position for the reaction should be towards the target entity.
bool CTaskUseScenario::ShouldRespondToSourceEntity(int iEventType) const
{
	switch(iEventType)
	{
		case EVENT_RESPONDED_TO_THREAT:
		case EVENT_CRIME_CRY_FOR_HELP:
			return false;
		default:
			return true;
	}
}

// Set the stored position to be the ped.
void CTaskUseScenario::SetAmbientPosition(CPed* pPed)
{
	if(GetPed()->IsNetworkClone())
	{
		return;
	}

	m_Target.SetEntity(pPed);
}

// Set the stored position to be the passed in position.
void CTaskUseScenario::SetAmbientPosition(Vec3V_In vPos)
{
	if(GetPed()->IsNetworkClone())
	{
		return;
	}

	m_Target.SetPosition(VEC3V_TO_VECTOR3(vPos));
}

// Check if the conditional animations are already chosen
// If they are not, then set the appropriate panic flags
void CTaskUseScenario::TriggerExitToFlee(const CEvent& rEvent)
{
	if(GetPed()->IsNetworkClone())
	{
		return;
	}

	SetPanicFleeFlags();
	SetAmbientPosition(&rEvent);
	CacheOffThreatResponseDecision(CTaskThreatResponse::TR_Flee);
}

void CTaskUseScenario::SetPanicFleeFlags()
{
	// Make sure m_ConditionalAnimsChosen is set, if it wasn't already.
	DetermineConditionalAnims();
	m_iFlags.SetFlag(SF_ShouldAbort);
	m_iScenarioReactionFlags.SetFlag(SREF_ExitingToFlee);
	m_iScenarioReactionFlags.SetFlag(SREF_ExitScenario);
}

void CTaskUseScenario::SetCombatExitFlags()
{
	m_iFlags.SetFlag(SF_ShouldAbort);
	m_iScenarioReactionFlags.SetFlag(SREF_ExitingToCombat);
	m_iScenarioReactionFlags.SetFlag(SREF_ExitScenario);
}

//  PURPOSE:  Set the flags to exit for combat.
void CTaskUseScenario::TriggerExitToCombat(const CEvent& rEvent)
{
	if(GetPed()->IsNetworkClone())
	{
		return;
	}

	SetCombatExitFlags();
	SetAmbientPosition(&rEvent);
	CacheOffThreatResponseDecision(CTaskThreatResponse::TR_Fight);
}

// PURPOSE: Called by script to force a ped to panic flee a scenario.
//			vAmbientPosition - which direction to react towards
void CTaskUseScenario::ScriptForcePanicExit(Vec3V_In vAmbientPosition, bool skipOutroIfNoPanic)
{
	CPed* pPed = GetPed();
	if (pPed->IsNetworkClone())
	{
		return;
	}

	// This is an unusual call because ordinarily an event sets the ambient position.
	// However, here there is no event because script is forcing the ped to react in some direction.
	SetPanicFleeFlags();
	m_iScenarioReactionFlags.SetFlag(SREF_RespondingToEvent);

	if (skipOutroIfNoPanic)
	{
		m_iFlags.SetFlag(SF_NeverUseRegularExit);
	}

	//Set the ambient position.
	SetAmbientPosition(vAmbientPosition);
}

// PURPOSE: Removes all the set ped config flags that direct how peds exit scenarios. 
void CTaskUseScenario::ClearScriptedTriggerFlags(CPed& ped)
{
	ped.SetPedConfigFlag(CPED_CONFIG_FLAG_ForcePlayFleeScenarioExitOnNextScriptCommand, false);
	ped.SetPedConfigFlag(CPED_CONFIG_FLAG_ForcePlayImmediateScenarioExitOnNextScriptCommand, false);
	ped.SetPedConfigFlag(CPED_CONFIG_FLAG_ForcePlayNormalScenarioExitOnNextScriptCommand, false);
	ped.SetPedConfigFlag(CPED_CONFIG_FLAG_ForcePlayDirectedNormalScenarioExitOnNextScriptCommand, false);
}

// Start a coward reaction and then exit the scenario, afterwards starting CTaskHurryAway.
void CTaskUseScenario::TriggerCowardThenExitToEvent(const CEvent& rEvent)
{
	SetAmbientPosition(&rEvent);
	m_iScenarioReactionFlags.SetFlag(SREF_CowardReaction);
	m_iScenarioReactionFlags.SetFlag(SREF_ExitScenario);
	SetDoCowardEnter();
	GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_DontWatchFirstOnNextHurryAway, true);
}

// PURPOSE:  Play the regular exit and then respond to the event like a normal ped.
void CTaskUseScenario::TriggerNormalExitToEvent(const CEvent& rEvent)
{
	SetAmbientPosition(&rEvent);
	m_iFlags.SetFlag(SF_ShouldAbort);
	m_iScenarioReactionFlags.SetFlag(SREF_ExitScenario);
	GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_DontWatchFirstOnNextHurryAway, true);
}

// Tell the ped to play a small shock animation inside of CTaskAmbientClips.
void CTaskUseScenario::TriggerShockAnimation(const CEvent& rEvent)
{
	CTaskAmbientClips* pTask = static_cast<CTaskAmbientClips *>(FindSubTaskOfType(CTaskTypes::TASK_AMBIENT_CLIPS));
	if (!m_iScenarioReactionFlags.IsFlagSet(SREF_RespondingToEvent) && pTask && rEvent.IsShockingEvent())
	{
		const CEventShocking& pShockingEvent = static_cast<const CEventShocking&>(rEvent);
		const CEventShocking::Tunables& rTunables = pShockingEvent.GetTunables();
		//Create an ambient event.
		CAmbientEvent ambientEvent(rTunables.m_AmbientEventType, rEvent.GetEntityOrSourcePosition(), rTunables.m_AmbientEventLifetime, fwRandom::GetRandomNumberInRange(rTunables.m_MinTimeForAmbientReaction, rTunables.m_MaxTimeForAmbientReaction));

		//Add the ambient event.
		pTask->AddEvent(ambientEvent);
	}
	m_iScenarioReactionFlags.SetFlag(SREF_TempInPlaceReaction);
}

void CTaskUseScenario::TriggerHeadLook(const CEvent& rEvent)
{
	CPed* pPed = GetPed();
	
	aiTask* pTaskActive = pPed->GetPedIntelligence()->GetTaskSecondaryActive();
	if(!pTaskActive || pTaskActive->MakeAbortable(CTask::ABORT_PRIORITY_URGENT, &rEvent))
	{
		CTaskAmbientLookAtEvent* pTask = rage_new CTaskAmbientLookAtEvent(&rEvent);
		pPed->GetPedIntelligence()->AddTaskSecondary(pTask, PED_TASK_SECONDARY_PARTIAL_ANIM);
	}
	m_iScenarioReactionFlags.SetFlag(SREF_TempInPlaceReaction);
}

// PURPOSE:  Have the ped cower in place until the next script command is given to them.
void CTaskUseScenario::TriggerScriptedCowerInPlace()
{
	m_iScenarioReactionFlags.SetFlag(SREF_ScriptedCowerInPlace);
	GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee, true);
}

// PURPOSE:  Have the ped stop cowering in place to respond to some new stimuli.
void CTaskUseScenario::TriggerStopScriptedCowerInPlace()
{
	if (m_iScenarioReactionFlags.IsFlagSet(SREF_ScriptedCowerInPlace))
	{
		m_iScenarioReactionFlags.ClearFlag(SREF_ScriptedCowerInPlace);
		GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee, false);

		// Reset the panic clipset id as it was chosen when the ped started to cower.
		m_panicClipSetId = CLIP_SET_ID_INVALID;
	}
}

// PURPOSE:  Set should abort to exit normally.
void CTaskUseScenario::TriggerNormalExit()
{
	m_iFlags.SetFlag(SF_ShouldAbort);
	m_iScenarioReactionFlags.SetFlag(SREF_ExitScenario);
}

// PURPOSE:  Set should abort to exit normally, but allow for a chosen direction to play the animations towards.
void CTaskUseScenario::TriggerDirectedExitFromScript()
{
	TriggerNormalScriptExitCommon();

	m_iScenarioReactionFlags.SetFlag(SREF_DirectedScriptExit);

	GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_ForcePlayDirectedNormalScenarioExitOnNextScriptCommand, false);
}

// PURPOSE:  Set should abort to exit normally, but ignore wander pavement checks.
void CTaskUseScenario::TriggerNormalExitFromScript()
{
	TriggerNormalScriptExitCommon();

	// Clear the ped config flag.
	GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_ForcePlayNormalScenarioExitOnNextScriptCommand, false);
}

// Common code utilized by both normal and directed script exits.
void CTaskUseScenario::TriggerNormalScriptExitCommon()
{
	// Cancel the pavement search if one is in progress.
	m_PavementHelper.CleanupPavementFloodFillRequest();

	// Pretend that we made a flood fill check and it succeeded.
	m_iFlags.SetFlag(SF_CanExitForNearbyPavement);
	m_iFlags.SetFlag(SF_PerformedPavementFloodFill);

	// Set should abort so the ped will leave the scenario point using the standard exit animations.
	m_iFlags.SetFlag(SF_ShouldAbort);

	// Force high priority streaming of the exit clip.
	m_iFlags.SetFlag(SF_ForceHighPriorityStreamingForNormalExit);

	m_iFlags.SetFlag(SF_ExitToScriptCommand);
}

void CTaskUseScenario::TriggerFleeExitFromScript()
{
	CPed* pPed = GetPed();
	
	if (pPed->IsNetworkClone())
	{
		return;
	}

	SetPanicFleeFlags();
	
	// Don't set the ambient position like is normally done during flee exit triggering.
	// Instead it is set manually by script in CommandSetPedShouldPlayFleeScenarioExitOnNextCommand.

	// Clear the ped config flag.
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ForcePlayFleeScenarioExitOnNextScriptCommand, false);
}

// PURPOSE:  Set should abort to exit normally, but do so as flagged for disputes.
void CTaskUseScenario::TriggerExitForCowardDisputes(bool bFast, CPed* pPedAgitator)
{
	CPed* pPed = GetPed();

	if (pPed->IsNetworkClone())
	{
		return;
	}

	// If in a stationary point, ignore this trigger.  It could cause the ped to leave the point.
	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee))
	{
		ForgetAboutAgitation();
		return;
	}

	// Wander away from the player when this task exits.
	CTaskUnalerted* pTaskUnalerted = static_cast<CTaskUnalerted*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_UNALERTED));
	if (pTaskUnalerted)
	{
		pTaskUnalerted->SetWanderPathFrameDelay(15);	// This is to prevent two paths being created and screwing up the walkstart/runstart
		pTaskUnalerted->SetShouldWanderAwayFromPlayer(true);
	}

	// Respect special flag that lets all dispute exits blend out immediately.
	if (GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::QuickExitsFromHassling))
	{
		CTaskAmbientClips* pTask = static_cast<CTaskAmbientClips *>(FindSubTaskOfType(CTaskTypes::TASK_AMBIENT_CLIPS));
		if (pTask)
		{
			pTask->SetCleanupBlendOutDelta(REALLY_SLOW_BLEND_OUT_DELTA);
		}

		bFast = true;
	}

	bool bAllowEventsToKillThisTask = false;
	if(GetState() == State_GoToStartPosition)
	{
		bAllowEventsToKillThisTask = true;
	}
	else if(GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::DoSimpleBlendoutForEvents))
	{
		if(GetState() == State_Enter)
		{
			bAllowEventsToKillThisTask = true;
		}
		else if(bFast)
		{
			bAllowEventsToKillThisTask = true;
		}
	}
	if (bAllowEventsToKillThisTask)
	{
		// Even though there is no event, this flag will kill the task in ProcessPreFSM() of the next frame.
		// Better to do this now than wait for the ped to get all the way to the scenario point and finish playing their intro to get the event.
		m_iScenarioReactionFlags.SetFlag(SREF_AllowEventsToKillThisTask);
	}
	else
	{
		m_iFlags.SetFlag(SF_ShouldAbort);
		m_iScenarioReactionFlags.SetFlag(SREF_CowardAgitation);
		m_iScenarioReactionFlags.SetFlag(SREF_ExitScenario);
	}

	if (!IsRespondingToEvent())
	{
		SetAmbientPosition(pPedAgitator);
	}

	OnLeavingScenarioDueToAgitated();
}

void CTaskUseScenario::TriggerFleeExitForCowardDisputes(Vec3V_In vPos)
{
	CPed* pPed = GetPed();

	if (pPed->IsNetworkClone())
	{
		return;
	}

	// If in a stationary point, ignore this trigger.  It could cause the ped to leave the point.
	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee))
	{
		ForgetAboutAgitation();
		return;
	}

	SetPanicFleeFlags();
	SetAmbientPosition(vPos);
	CacheOffThreatResponseDecision(CTaskThreatResponse::TR_Flee);

	bool bAllowEventsToKillThisTask = false;
	if(GetState() == State_GoToStartPosition)
	{
		bAllowEventsToKillThisTask = true;
	}
	else if(GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::DoSimpleBlendoutForEvents))
	{
		bAllowEventsToKillThisTask = true;
	}
	else if (GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::QuickExitsFromHassling))
	{
		CTaskAmbientClips* pTask = static_cast<CTaskAmbientClips *>(FindSubTaskOfType(CTaskTypes::TASK_AMBIENT_CLIPS));
		if (pTask)
		{
			pTask->SetCleanupBlendOutDelta(REALLY_SLOW_BLEND_OUT_DELTA);
		}

		bAllowEventsToKillThisTask = true;
	}

	if (bAllowEventsToKillThisTask)
	{
		// Even though there is no event, this flag will kill the task in ProcessPreFSM() of the next frame.
		// Better to do this now than wait for the ped to get all the way to the scenario point and finish playing their intro to get the event.
		m_iScenarioReactionFlags.SetFlag(SREF_AllowEventsToKillThisTask);
	}

	OnLeavingScenarioDueToAgitated();
}


// PURPOSE:  Set the flag to allow the ped to allow any event to cause the scenario ped to immediately quit.
// The ped will only exit if they are currently responding to some event.
void CTaskUseScenario::TriggerImmediateExitOnEvent()
{
	CPed* pPed = GetPed();
#if !HACK_RDR3
	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee) && !StationaryReactionsHack())
	{
#else
	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee))
	{
#endif
		// Do nothing - the ped will call HandlePanicSetUp() in PlayAmbients.
		// This will cause the ped to start cowering in place.
	}
	else
	{
		m_iScenarioReactionFlags.SetFlag(SREF_AllowEventsToKillThisTask);
		m_iScenarioReactionFlags.SetFlag(SREF_ExitScenario);

		CTask* pSubtask = GetSubTask();
		if (pSubtask)
		{
			if (pSubtask->GetTaskType() == CTaskTypes::TASK_AMBIENT_CLIPS)
			{
				float fDelta = GetScenarioInfo().GetImmediateExitBlendOutDuration();

				// Metadata stores it in seconds, StopClip() expects things to be -1 / seconds.
				fDelta = fDelta == 0.0f ? INSTANT_BLEND_OUT_DELTA : (-1.0f / fDelta);

				CTaskAmbientClips* pTaskAmbientClips = static_cast<CTaskAmbientClips*>(pSubtask);
				pTaskAmbientClips->SetCleanupBlendOutDelta(fDelta);

				// Clear the ped config flag.
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ForcePlayImmediateScenarioExitOnNextScriptCommand, false);
			}
		}
	}
}

// Return true if the ped has recently seen this kind of event.
bool CTaskUseScenario::HasRespondedToRecently(const CEvent& rEvent)
{
	const static float s_fRecentEventThresholdInSeconds = 10.0f;

	// Currently responding to the same event type, return true.
	if (m_iEventTriggeringPanic == rEvent.GetEventType()) 
	{
		return true; 
	}
	// Check the last event and see if it matches the type.
	else if (m_iLastEventType == rEvent.GetEventType())
	{
		// Check if the ped responded to this event recently.
		if (m_fLastEventTimer < s_fRecentEventThresholdInSeconds)
		{
			return true;
		}
	}
	return false;
}

// Returns true if this scenario point has a valid shocking reaction animation.
bool CTaskUseScenario::HasAValidShockingReactionAnimation(const CEvent& rEvent) const
{
	if (!rEvent.IsShockingEvent())
	{
		// Only shocking events have the tunables necessary for this kind of reaction.
		return false;
	}

	const CEventShocking& rShockingEvent = static_cast<const CEventShocking&>(rEvent);
	CScenarioCondition::sScenarioConditionData conditionData;
	conditionData.eAmbientEventType = rShockingEvent.GetTunables().m_AmbientEventType;
	conditionData.pPed = GetPed();

	fwMvClipSetId reactClipSetId = ChooseConditionalClipSet(CConditionalAnims::AT_REACTION, conditionData);

	if (reactClipSetId == CLIP_SET_ID_INVALID)
	{
		return false;
	}
	else
	{
		return true;
	}
}

bool CTaskUseScenario::HasAValidCowardReactionAnimation(const CEvent& UNUSED_PARAM(rEvent)) const
{
	CScenarioCondition::sScenarioConditionData conditionData;
	conditionData.eAmbientEventType = AET_Threatened;
	conditionData.pPed = GetPed();

	fwMvClipSetId reactClipSetId = ChooseConditionalClipSet(CConditionalAnims::AT_PANIC_INTRO, conditionData);

	if (reactClipSetId == CLIP_SET_ID_INVALID)
	{
		return false;
	}
	else
	{
		return true;
	}
}

bool CTaskUseScenario::HasAValidNormalExitAnimation() const
{
	CScenarioCondition::sScenarioConditionData conditionData;
	conditionData.pPed = GetPed();

	fwMvClipSetId reactClipSetId = ChooseConditionalClipSet(CConditionalAnims::AT_EXIT, conditionData);

	if (reactClipSetId == CLIP_SET_ID_INVALID)
	{
		return false;
	}
	else
	{
		return true;
	}
}

bool CTaskUseScenario::HasTriggeredExit() const
{
	//Ensure the flag is set.
	if(!m_iScenarioReactionFlags.IsFlagSet(SREF_ExitScenario))
	{
		return false;
	}

	return true;
}

// Sets flags noting that the ped is doing a response to an event.
void CTaskUseScenario::StartEventReact(CEvent& rEvent)
{
	m_iScenarioReactionFlags.SetFlag(SREF_RespondingToEvent);
	m_iEventTriggeringPanic = (short)rEvent.GetEventType();
	m_iPriorityOfEventTriggeringPanic = (short)rEvent.GetEventPriority();

	// Consume the event if this is a one time event response (head tracking or in-place anims).
	if (m_iScenarioReactionFlags.IsFlagSet(SREF_TempInPlaceReaction))
	{
		rEvent.Tick();
	}
}

// Clear flags associated with some previous event response.
void CTaskUseScenario::EndEventReact()
{
	// Assert that we really were reacting to some event.
	Assert(m_iScenarioReactionFlags.IsFlagSet(SREF_RespondingToEvent));

	//Reset the scenario reaction flags.
	m_iScenarioReactionFlags.ClearAllFlags();

	// Reset the clipsets so they get chosen properly the next time around.
	m_panicClipSetId = CLIP_SET_ID_INVALID;
	m_panicBaseClipSetId = CLIP_SET_ID_INVALID;

	if (m_iEventTriggeringPanic != -1)
	{
		//Need to clear all events at this point so the ped doesn't keep responding to the same event.
		GetPed()->GetPedIntelligence()->RemoveEventsOfType(m_iEventTriggeringPanic);
		
		//Store off this event type as the last event responded to.
		m_iLastEventType = m_iEventTriggeringPanic;
		m_fLastEventTimer = 0.0f;

		//Reset the current event being responded to as -1.
		m_iEventTriggeringPanic = -1;
		m_iPriorityOfEventTriggeringPanic = -1;

		//Clear flags relating to driving a scenario exit now that the event has ended.
		m_iFlags.ClearFlag(SF_ShouldAbort);
	}
}

// Helper function to see if an event requires a valid target.
bool CTaskUseScenario::EventRequiresValidTarget(s32 iEventType)
{
	switch(iEventType)
	{
		case EVENT_SCRIPT_COMMAND:
		case EVENT_GIVE_PED_TASK:
			return false;
		default:
			return true;
	}
}

void CTaskUseScenario::OnLeavingScenarioDueToAgitated()
{
	GetPed()->GetPedIntelligence()->GetLastUsedScenarioFlags().SetFlag(CPedIntelligence::LUSF_ExitedDueToAgitated);
}

void CTaskUseScenario::SetLastUsedScenarioPoint()
{
	GetPed()->GetPedIntelligence()->SetLastUsedScenarioPoint(GetScenarioPoint());
	GetPed()->GetPedIntelligence()->SetLastUsedScenarioPointType(GetScenarioPointType());
	GetPed()->GetPedIntelligence()->GetLastUsedScenarioFlags().ClearAllFlags();
}

void CTaskUseScenario::TogglePedCapsuleCollidesWithInactives(bool bCollides)
{
	CPed* pPed = GetPed();
	phInst* pInst = pPed->GetAnimatedInst();

	// Check to make sure we are in the level.
	if(pInst && pInst->IsInLevel())
	{
		// Grab the level index.
		u16 uLevelIndex = pInst->GetLevelIndex();
	
		// Set the collision flags.
		PHLEVEL->SetInactiveCollidesAgainstInactive(uLevelIndex, bCollides);

		if (bCollides)
		{
			PHLEVEL->FindAndAddOverlappingPairs(uLevelIndex);
		}

		// Cause any peds this ped collides with to become activated so they slide out of the way.
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_AlwaysWakeUpPhysicsOfIntersectedPeds, bCollides);
	}
}

#if SCENARIO_DEBUG
void CTaskUseScenario::SetOverrideAnimData(const ScenarioAnimDebugInfo& info)
{
	if (!m_pDebugInfo)
	{
		m_pDebugInfo = rage_new ScenarioAnimDebugInfo(info);
	}
	else
	{
		*m_pDebugInfo = info;
	}
	m_iDebugFlags.SetFlag(SFD_UseDebugAnimData);
	m_iDebugFlags.ClearFlag(SFD_WantNextDebugAnimData);
}
#endif // SCENARIO_DEBUG

void CTaskUseScenario::InitConditions(CScenarioCondition::sScenarioConditionData& conditionDataOut) const
{
	u32 iFlags = 0;
	const CPed* pPed = GetPed();
	
	if(m_iFlags.IsFlagSet(SF_IgnoreTimeCondition))
	{
		iFlags |= SF_IgnoreTime;
	}

	conditionDataOut.pPed = pPed;
	conditionDataOut.iScenarioFlags = iFlags;

	if(m_iFlags.IsFlagSet(SF_SynchronizedSlave))
	{
		conditionDataOut.m_RoleInSyncedScene = RISS_Slave;
	}
	else if(m_iFlags.IsFlagSet(SF_SynchronizedMaster))
	{
		conditionDataOut.m_RoleInSyncedScene = RISS_Master;
	}

	if (m_Target.GetIsValid())
	{
		Vector3 vPos;
		if (m_Target.GetPosition(vPos))
		{
			conditionDataOut.vAmbientEventPosition = VECTOR3_TO_VEC3V(vPos);
		}
	}

	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee))
	{
		if (CTaskCowerScenario::ShouldPlayDirectedCowerAnimation(m_iEventTriggeringPanic))
		{
			conditionDataOut.eAmbientEventType = AET_Directed_In_Place;
		}
		else
		{
			conditionDataOut.eAmbientEventType = AET_In_Place;
		}

	}
	else if (m_iScenarioReactionFlags.IsFlagSet(SREF_AggroAgitation))
	{
		conditionDataOut.eAmbientEventType = AET_Threatening;
	}
	else if (m_iScenarioReactionFlags.IsFlagSet(SREF_CowardReaction))
	{
		conditionDataOut.eAmbientEventType = AET_Threatened;
	}
}


bool CTaskUseScenario::CreateSynchronizedSceneForScenario()
{
	taskAssert(m_SyncedSceneId == INVALID_SYNCED_SCENE_ID);

	CScenarioPoint* pt = GetScenarioPoint();
	if(m_iFlags.IsFlagSet(CTaskUseScenario::SF_SynchronizedMaster)
			&& taskVerifyf(pt, "Scenario point needed for synchronized scenario animations."))
	{
		fwSyncedSceneId syncedScene = fwAnimDirectorComponentSyncedScene::StartSynchronizedScene();
		if(fwAnimDirectorComponentSyncedScene::IsValidSceneId(syncedScene))
		{
			Vec3V posV = pt->GetPosition();
			QuatV orientation;
			pt->GetDirection(orientation);
			fwAnimDirectorComponentSyncedScene::SetSyncedSceneOrigin(syncedScene, RC_VECTOR3(posV), RC_QUATERNION(orientation));

			fwAnimDirectorComponentSyncedScene::AddSyncedSceneRef(syncedScene);
			m_SyncedSceneId = syncedScene;

			return true;
		}
	}

	return false;
}


void CTaskUseScenario::ReleaseSynchronizedScene()
{
	if(m_SyncedSceneId != INVALID_SYNCED_SCENE_ID)
	{
		if(taskVerifyf(fwAnimDirectorComponentSyncedScene::IsValidSceneId(m_SyncedSceneId),
				"Expected the synced scene to be valid since we have a ref-count on it, but something must have destroyed it."))
		{
			fwAnimDirectorComponentSyncedScene::DecSyncedSceneRef(m_SyncedSceneId);
		}
		m_SyncedSceneId = INVALID_SYNCED_SCENE_ID;
	}
}


bool CTaskUseScenario::StartPlayingSynchronized(const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, float blendInDelta, bool tagSyncOut)
{
	// Make sure we're not already playing a synchronized animation.
	if(!taskVerifyf(m_SyncedSceneId == INVALID_SYNCED_SCENE_ID, "Synchronized scene is already running, unexpectedly."))
	{
		return false;
	}

	// Look up the clip set, clip, and dictionary.
	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
	if(!pClipSet)
	{
		return false;
	}
	const crClip* pClip = pClipSet->GetClip(clipId);
	if(!pClip)
	{
		return false;
	}
	const crClipDictionary* pClipDict = pClip->GetDictionary();
	if(!pClipDict)
	{
		return false;
	}

	if(m_iFlags.IsFlagSet(SF_SynchronizedMaster))
	{
		// The master is responsible for creating the synchronized scene.

		if(!CreateSynchronizedSceneForScenario())
		{
			return false;
		}

		// Set it up to play once and then end.
		fwAnimDirectorComponentSyncedScene::SetSyncedSceneLooped(m_SyncedSceneId, false);
		fwAnimDirectorComponentSyncedScene::SetSyncedSceneHoldLastFrame(m_SyncedSceneId, false);
	}
	else if(taskVerifyf(m_iFlags.IsFlagSet(SF_SynchronizedSlave), "Ped is neither master nor slave, can't play synchronized animations."))
	{
		// For the slave, we look up the master's synchronized scene.

		CTaskUseScenario* pMasterTask = FindSynchronizedMasterTask();
		if(pMasterTask)
		{
			m_SyncedSceneId = pMasterTask->m_SyncedSceneId;
		}
		taskAssertf(m_SyncedSceneId != INVALID_SYNCED_SCENE_ID, "Failed to get synchronized scene ID from master.");
	}

	if(!fwAnimDirectorComponentSyncedScene::IsValidSceneId(m_SyncedSceneId))
	{
		return false;
	}

	// Start the task.
	const atHashString clipDictName = pClipSet->GetClipDictionaryName();
	float blendOutDelta = NORMAL_BLEND_OUT_DELTA;

	// Look for tags in the clip to see when we can blend it out.
	const float blendOutPhase = FindSynchronizedSceneBlendOutPhase(*pClip);
	m_SyncedSceneBlendOutPhase = blendOutPhase;

	// Adjust the blend-out rate if we start blending out significantly before the end of the animation.
	if(blendOutPhase < 0.99f)
	{
		// Compute the duration of the animation after the blend-out phase.
		const float remainingPhaseAfterStartBlend = 1.0f - blendOutPhase;
		const float remainingTimeAfterStartBlend = Max(remainingPhaseAfterStartBlend*pClip->GetDuration(), SMALL_FLOAT);

		// Compute the blend-out rate so we blend out during the remainder of the animation,
		// but it may be best to not let it go slower than REALLY_SLOW_BLEND_OUT_DELTA.
		blendOutDelta = (-1.0f)/remainingTimeAfterStartBlend;
		blendOutDelta = Min(blendOutDelta, REALLY_SLOW_BLEND_OUT_DELTA);
	}

	eSyncedSceneFlagsBitSet flags;
	flags.BitSet().Set(SYNCED_SCENE_USE_KINEMATIC_PHYSICS);
	if (tagSyncOut)
		flags.BitSet().Set(SYNCED_SCENE_TAG_SYNC_OUT);

	CTaskSynchronizedScene* pSyncAnimTask = rage_new CTaskSynchronizedScene(m_SyncedSceneId, clipId, clipDictName, blendInDelta, blendOutDelta, flags);
	SetNewTask(pSyncAnimTask);

	return true;
}


CTaskUseScenario* CTaskUseScenario::FindSynchronizedMasterTask() const
{
	const CPed* pMaster = m_SynchronizedPartner;
	if(pMaster)
	{
		CTaskUseScenario* pMasterTask = static_cast<CTaskUseScenario*>(pMaster->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
		return pMasterTask;
	}
	return NULL;
}


float CTaskUseScenario::FindSynchronizedSceneBlendOutPhase(const crClip& clip) const
{
	// Note: it seems a bit inconsistent now, in how we have TagSyncBlendOut,
	// MOVE_ENDS_IN_WALK, and now BLEND_OUT, which all seem similar.

	// Also note that BLEND_OUT is a slightly different type of tag, in that it's a
	// property attribute of a MoveEvent tag, or something like that.

	static const fwMvBooleanId s_BlendOutTag("BLEND_OUT",0xAB120D43);

	const crTags* pTags = clip.GetTags();

	if(pTags)
	{
		crTagIterator moveIt(*pTags, 0.0f, 1.0f, CClipEventTags::MoveEvent);
		while(*moveIt)
		{
			const crTag* pTag = *moveIt;
			if(pTag)
			{
				const crPropertyAttribute* attrib = pTag->GetProperty().GetAttribute(CClipEventTags::MoveEvent);
				if(attrib && attrib->GetType() == crPropertyAttribute::kTypeHashString)
				{
					if(s_BlendOutTag.GetHash() == static_cast<const crPropertyAttributeHashString*>(attrib)->GetHashString().GetHash())
					{
						return pTag->GetStart();
					}	
				}
			}
			++moveIt;
		}
	}

	// Potentially this code could replace what's above, but I had problems to get it working right,
	// with the template parameters and optional parameters.
	//	const crTag* pTag = CClipEventTags::FindFirstEventTag<crProperty, crPropertyAttributeBool>(&clip, CClipEventTags::MoveEvent, s_BlendOutTag);
	//	if(pTag)
	//	{
	//		return pTag->GetStart();
	//	}

	return 1.0f;
}

fwMvClipSetId CTaskUseScenario::ChooseConditionalClipSet(CConditionalAnims::eAnimType animType, CScenarioCondition::sScenarioConditionData& conditionData, const CConditionalClipSet** ppOutConClipSet /*= NULL*/, u32* const pOutPropSetHash /*= NULL*/) const
{
	fwMvClipSetId retval = CLIP_SET_ID_INVALID;

#if SCENARIO_DEBUG
	if (m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData) && m_pDebugInfo->m_AnimType == animType)
	{
		retval = m_pDebugInfo->m_ClipSetId;
		if (ppOutConClipSet)
		{
			*ppOutConClipSet = NULL;
		}
	}
	else
#endif // SCENARIO_DEBUG
	{
		if (m_pConditionalAnimsGroup && m_ConditionalAnimsChosen != -1 && m_ConditionalAnimsChosen < m_pConditionalAnimsGroup->GetNumAnims())
		{
			const CConditionalAnims * pAnims = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimsChosen);
			if ( pAnims )
			{
				const CConditionalClipSet* pChoosen = CConditionalAnims::ChooseClipSet(*pAnims->GetClipSetArray(animType),conditionData);
				if ( pChoosen )
				{
					retval = fwMvClipSetId(pChoosen->GetClipSetHash());
				}

				if (ppOutConClipSet)
				{
					*ppOutConClipSet = pChoosen;
				}

				if (pOutPropSetHash)
				{
					if (pAnims->GetPropSetHash() != 0)
					{
						*pOutPropSetHash = pAnims->GetPropSetHash();
					}
				}
			}
		}
	}

	return retval;
}

CScenarioClipHelper::ScenarioClipCheckType CTaskUseScenario::ChooseClipInClipSet(CConditionalAnims::eAnimType animType, const fwMvClipSetId& clipSet, fwMvClipId& outClipId)
{
	if (clipSet == CLIP_SET_ID_INVALID)
	{
		outClipId = CLIP_ID_INVALID;
		return CScenarioClipHelper::ClipCheck_Failed;
	}

	CScenarioClipHelper::ScenarioClipCheckType retval = CScenarioClipHelper::ClipCheck_Failed;
#if SCENARIO_DEBUG
	if (m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData) && m_pDebugInfo->m_AnimType == animType)
	{
		outClipId = m_pDebugInfo->m_ClipId;
		retval = CScenarioClipHelper::ClipCheck_Success;
	}
	else
#endif // SCENARIO_DEBUG
	{
		CPed* pPed = GetPed();
		bool bForceOneFrameExitCheck = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_NeverDoScenarioExitProbeChecks) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceSynchronousScenarioExitChecking) SCENARIO_DEBUG_ONLY(|| m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData));
		bool bInPlaceReaction = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee) && animType == CConditionalAnims::AT_PANIC_INTRO;
		bool bDoExitProbeChecks = !pPed->IsAPlayerPed() && GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::ValidateExitsWithProbeChecks) && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_NeverDoScenarioExitProbeChecks);
		if (bInPlaceReaction)
		{
			// In place reactions should always just pick by simple conditions.
			bool bSuccess = CAmbientClipRequestHelper::PickClipByGroup_SimpleConditions( pPed, clipSet, outClipId );

			if (bSuccess)
			{
				retval = CScenarioClipHelper::ClipCheck_Success;
			}
			else
			{
				retval = CScenarioClipHelper::ClipCheck_Failed;
			}
		}
		else if (animType == CConditionalAnims::AT_PANIC_EXIT || animType == CConditionalAnims::AT_PANIC_INTRO ||
			(animType == CConditionalAnims::AT_EXIT && bDoExitProbeChecks))
		{
			// When exiting to combat, need to rotate to target and do probe checks to make sure the anim can actually be played.
			retval = PickClipFromSet(pPed, clipSet, outClipId, m_Target.GetIsValid(), bDoExitProbeChecks, bForceOneFrameExitCheck);
		}
		else if (animType == CConditionalAnims::AT_ENTER && IsAScenarioResume())
		{
			// Probe the re-entry clips.
			retval = PickReEnterClipFromSet(pPed, clipSet, outClipId);
		}
		else
		{
			// Default to just picking by simple conditions.
			bool bSuccess = CAmbientClipRequestHelper::PickClipByGroup_SimpleConditions( pPed, clipSet, outClipId );

			if (bSuccess)
			{
				retval = CScenarioClipHelper::ClipCheck_Success;
			}
			else
			{
				retval = CScenarioClipHelper::ClipCheck_Failed;
			}
		}
	}

	return retval;
}

bool CTaskUseScenario::AreBaseClipsStreamed()
{
	if ( m_baseClipSetId != CLIP_SET_ID_INVALID )
	{
		m_ClipSetRequestHelper.RequestClipSetIfExists(m_baseClipSetId);
		return m_ClipSetRequestHelper.IsClipSetStreamedIn();
	}
	return false;
}

bool CTaskUseScenario::AreEnterClipsStreamed()
{
	const fwMvClipSetId enterClipSetId  = GetEnterClipSetId(GetPed(), this);

	bool hasEnterClip = ( enterClipSetId != CLIP_SET_ID_INVALID );
	if (hasEnterClip)
	{
		const fwMvClipId chosenClipId = GetChosenClipId(GetPed(), this);
		bool hasChosenClip = ( chosenClipId!= CLIP_ID_INVALID );
		m_EnterClipSetRequestHelper.Request(enterClipSetId);
		bool enterClipStreamed	= m_EnterClipSetRequestHelper.IsLoaded();

		bool propReady			= m_pPropHelper ? m_pPropHelper->IsPropReady() : true;
		return hasChosenClip && enterClipStreamed && propReady;
	}
	return false;
}

CTask::FSM_Return CTaskUseScenario::ProcessPreFSM()
{
	CPed* pPed = GetPed();
	if (m_iScenarioReactionFlags.IsFlagSet(SREF_AllowEventsToKillThisTask))
	{
		// ShouldAbort() should have killed this task, but the event expired.  In this case, just quit.
		// Force the motion state to be idle so that the runstarts are given a chance to work.

		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle);
		return FSM_Quit;
	}
	if (m_iLastEventType != -1)
	{
		m_fLastEventTimer += GetTimeStep();
	}
	m_fLastPavementCheckTimer += GetTimeStep();

	// Reset this, will be set later in the frame if we are indeed ready to leave on the master's command.
	m_bSlaveReadyForSyncedExit = false;

	m_iFlags.ClearFlag(SF_WouldHaveLeftLastFrame);

	CScenarioPoint* pScenarioPoint = GetScenarioPoint();

	if (pPed && pPed->GetVelocity().Mag2() < 0.1f * 0.1f)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_IsInStationaryScenario, true);
	}

	CNetObjPed*	netObjPed	= static_cast<CNetObjPed *>(pPed->GetNetworkObject());

	if(netObjPed)
	{
		netObjPed->SetOverrideScopeBiggerWorldGridSquare(true);
	}

	bool bIsNetworkGame = NetworkInterface::IsGameInProgress();

	switch (m_eScenarioMode)
	{
	case SM_EntityEffect:
		{
			if(!pScenarioPoint)
			{
				if(bIsNetworkGame)
				{
					if(m_bAllowDestructionEvenInMPForMissingEntity)
					{
						FlagToDestroyPedIfApplicable();
						return FSM_Quit;
					}
					else if(pPed->IsNetworkClone())
					{	
						// If the task is aborted on the clone then quit
						if(GetIsFlagSet(aiTaskFlags::IsAborted))
						{
							return FSM_Quit;
						}

						//wait for streamed in scenario effect
						CTaskScenario::CloneFindScenarioPoint();

						if (!GetScenarioPoint() && !GetIsWaitingForEntity())
						{
							// At this point, I think we should either be waiting for an entity,
							// or have a valid scenario point. If not, clones may get to update
							// the state machine despite there not being a scenario point, which
							// may not be stable.
#if __ASSERT
							CNetObjPed*					netObjPed					= static_cast<CNetObjPed *>(pPed->GetNetworkObject());
							const CScenarioInfo*		pTaskScenarioInfo			= SCENARIOINFOMGR.GetScenarioInfoByIndex(GetScenarioType());
							const CScenarioInfo*		pTaskInfoScenarioInfo		= NULL;
							CTaskInfo*					pTaskInfo					= NULL;
							CClonedScenarioFSMInfoBase* pScenarioInfo				= NULL;

							if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(GetTaskType()))
							{
								pTaskInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(GetTaskType(), PED_TASK_PRIORITY_MAX);
								pScenarioInfo = dynamic_cast<CClonedScenarioFSMInfoBase*>(pTaskInfo);
								if (pScenarioInfo)
								{
									pTaskInfoScenarioInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(pScenarioInfo->GetScenarioType());
								}
							}

							taskAssertf(	GetScenarioPoint() || GetIsWaitingForEntity(),
								"B* 452942: %s: scenario type index %d task scenario type name %s, task info scenario type %s",
								netObjPed ? netObjPed->GetLogName():"NULL CNetObjPed",
								GetScenarioType(),
								pTaskScenarioInfo ? pTaskScenarioInfo->GetName(): "Unknown task scenario",
								pTaskInfoScenarioInfo ? pTaskInfoScenarioInfo->GetName(): "Unknown task info scenario"
								);
#endif
							return FSM_Quit;
						}
					}
					else
					{	
						CScenarioPoint* pScenarioPoint = NULL;

						CloneCheckScenarioPointValidAndGetPointer(&pScenarioPoint);

						if(pScenarioPoint!=NULL)
						{
							SetScenarioPoint(pScenarioPoint);

							//Handle re-attaching if needed if ped has been left
							//unattached when last task quit, or when warping to clone and creating anew
							NetworkSetUpPedPositionAndAttachments();
						}
					}				
				}
				else 
				{
					// Keeping the behavior the same for SP, but we can't do this in a MP game because of issues like B*1627729.
					FlagToDestroyPedIfApplicable();
					return FSM_Quit;
				}
			}
			else
			{
				if(bIsNetworkGame)
				{
					if( IsDynamic() && !pPed->IsNetworkClone() && pPed->m_nFlags.bInMloRoom)
					{
						//If local interior ped should be attach to dynamic entity but is attached to world instead 
						fwAttachmentEntityExtension *pedAttachExt = pPed->GetAttachmentExtension();
						if( pedAttachExt && pedAttachExt->GetIsAttached() && (pedAttachExt->GetAttachFlags() & ATTACH_FLAG_ATTACHED_TO_WORLD_BASIC) )
						{
							pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
							NetworkSetUpPedPositionAndAttachments();
						}
					}
				}
			}
		}
		break;
	case SM_WorldEffect:
		{
			if (!pScenarioPoint)
			{
				if (bIsNetworkGame)
				{
					if (pPed->IsNetworkClone())
					{
						if(GetIsFlagSet(aiTaskFlags::IsAborted))
						{
							return FSM_Quit;
						}

						//wait for streamed in scenario effect
						CTaskScenario::CloneFindScenarioPoint();

						if (!GetScenarioPoint() && !GetIsWaitingForEntity())
						{
							return FSM_Quit;
						}
					}
					else
					{	//B* 1755689 keep checking for local SM_WorldEffect SP to come in range and load it in
						//and set ped position up
						CScenarioPoint* pScenarioPoint = NULL;

						CloneCheckScenarioPointValidAndGetPointer(&pScenarioPoint);

						if(pScenarioPoint!=NULL)
						{
							SetScenarioPoint(pScenarioPoint);

							GetWorldScenarioPointIndex(); //this will set/refresh the cached index for the newly loaded scenario point

							aiDebugf2("Local Ped %s Loaded scenario point type %d, world index %d, in CTaskUseScenario::ProcessPreFSM", 
								pPed->GetDebugName(), 
								pScenarioPoint->GetScenarioTypeVirtualOrReal(), 
								GetCachedWorldScenarioPointIndex());

							//Handle re-attaching if needed if ped has been left
							//unattached when last task quit, or when warping to clone and creating anew
							NetworkSetUpPedPositionAndAttachments();
						}
					}				
				} 
				else 
				{
					FlagToDestroyPedIfApplicable();
					return FSM_Quit;
				}
			}
		}
		break;
	case SM_WorldLocation:
	default:
		break;
	}

	if(bIsNetworkGame && pScenarioPoint && m_eScenarioMode!=SM_WorldLocation )
	{	//We have a scenario point and in a net game and we are not a SM_WorldLocation so use m_vPosition to keep a cache of position 
		m_vPosition = VEC3V_TO_VECTOR3(pScenarioPoint->GetPosition());  

		taskAssertf(!m_vPosition.IsClose(VEC3_ZERO,0.1f),"m_vPosition cache is set close to origin. %s,  SP flags 0x%x, SP name %s, Entity %s, m_eScenarioMode %d,  m_iScenarioIndex %d",
			pPed?pPed->GetDebugName():"Null ped",
			pScenarioPoint->GetAllFlags(),
			pScenarioPoint->GetScenarioName(),
			pScenarioPoint->GetEntity()?pScenarioPoint->GetEntity()->GetDebugName():"Null entity",
			m_eScenarioMode,
			m_iScenarioIndex);
	}

	if (m_iScenarioIndex < 0 || (bIsNetworkGame && SCENARIOINFOMGR.GetScenarioInfoByIndex(m_iScenarioIndex)==NULL) )
	{
#if __ASSERT
		if(bIsNetworkGame && m_iScenarioIndex>=0)
		{
			taskAssertf(0,"%s NULL scenario info m_iScenarioIndex %d, Num scenario types %d, Num all scenarios %d",
				pPed?pPed->GetDebugName():"NUll ped",
				m_iScenarioIndex,
				SCENARIOINFOMGR.GetScenarioCount(false),
				SCENARIOINFOMGR.GetScenarioCount(true));
		}
#endif
		return FSM_Quit;
	}
	// Past this point we are expecting a valid scenario info.
	const CScenarioInfo& rInfo = GetScenarioInfo();

	if (pScenarioPoint && !CScenarioManager::CheckScenarioPointEnabledByGroupAndAvailability(*pScenarioPoint))
	{
		TriggerNormalExit();
	}

	if (m_iFlags.IsFlagSet(SF_CheckBlockingAreas))
	{
		//If the point we are using is now in a blocking area then we need to exit
		static float TIME_TO_CHECK_BLOCKING = 1.0f; //float seconds... 
		m_fBlockingAreaCheck += GetTimeStep();
		// static bool check = false;
		if (m_fBlockingAreaCheck >= TIME_TO_CHECK_BLOCKING)
		{
			m_fBlockingAreaCheck = 0.0f;
			bool posvalid = false;
			Vector3 position = m_vPosition;
			switch (m_eScenarioMode)
			{
			case SM_EntityEffect:
			case SM_WorldEffect:
				if (pScenarioPoint)
				{
					position = VEC3V_TO_VECTOR3(pScenarioPoint->GetPosition());
					posvalid = true;
				}
				break;
			case SM_WorldLocation:
				position = m_vPosition;
				posvalid = true;
				break;
			default:
				break;
			}

			if(posvalid && (CScenarioBlockingAreas::ShouldScenarioQuitFromBlockingAreas(position, true, false)))
			{
				m_iFlags.SetFlag(SF_ShouldAbort);
			}
		}
	}

	// If we've yet to choose an clip index from our entry set then we should attempt to do so
	PickEntryClipIfNeeded();

	// Old scenario tasks like CTaskSeatedScenario used to allow checking for
	// shocking events. Now, we do that if SF_CheckShockingEvents is set.
	// Note: if needed, we could perhaps add a parameter to the scenario data,
	// in case we never want it with certain types of scenarios.
	if(m_iFlags & SF_CheckShockingEvents)
	{
		pPed->GetPedIntelligence()->SetCheckShockFlag(true);	
	}

	// If SF_AddToHistory is set, but SF_WasAddedToHistory is not, we should try to add ourselves
	// to the scenario spawn history.
	if((m_iFlags.GetAllFlags() & (SF_AddToHistory | SF_WasAddedToHistory)) == SF_AddToHistory)
	{
		if(pScenarioPoint)
		{
			// If the point doesn't already have history, add it and set SF_WasAddedToHistory.
			if(!pScenarioPoint->HasHistory())
			{
				CScenarioPointChainUseInfo* chainUserInfo = NULL;

				// For adding to the history, try to grab a CScenarioPointChainUseInfo object from the parent task.
				const CTask* pParentTask = GetParent();
				if(pParentTask && pParentTask->GetTaskType() == CTaskTypes::TASK_UNALERTED)
				{
					chainUserInfo = static_cast<const CTaskUnalerted*>(pParentTask)->GetScenarioChainUserData();

#if (!__NO_OUTPUT) && (__ASSERT || __BANK)
					if(chainUserInfo)
					{
						aiDebugf2("Ped %s (%p) found chaining user info %p from parent task.", pPed->GetDebugName(), (void*)pPed, chainUserInfo);
						aiDebugf2("- ped %p, vehicle %p, hist %d", (void*)chainUserInfo->GetPed(), (void*)chainUserInfo->GetVehicle(), chainUserInfo->GetNumRefsInSpawnHistory());
					}
					else
					{
						aiDebugf2("Ped %s (%p) found no chaining user info in parent task.", pPed->GetDebugName(), (void*)pPed);
					}
#endif	// (!__NO_OUTPUT) && (__ASSERT || __BANK)
				}

				// If the point is chained, it's probably not a good idea to add to the history without a chaining user.
				// In MP games, the parent task might need to do some stuff first to get the chaining user set up.
				// Proceed only if we have a user entry, or if the point isn't chained.
				if(chainUserInfo || !pScenarioPoint->IsChained())
				{
					// Now, we probably don't really want to be in the history multiple times for the same chain.
					// We only actually add to the history if we don't already have history references on the
					// chaining user, but we will set 
					if(!chainUserInfo || !chainUserInfo->GetNumRefsInSpawnHistory())
					{
						CScenarioManager::GetSpawnHistory().Add(*pPed, *pScenarioPoint, GetScenarioType(), chainUserInfo);
						if(pScenarioPoint->HasHistory())	// Only set the flag if we were successful.
						{
							m_iFlags.SetFlag(SF_WasAddedToHistory);
						}
					}
					else
					{
						m_iFlags.SetFlag(SF_WasAddedToHistory);
					}
				}
			}
			// else, in the case of the point already having a history, we may
			// want to keep WasAddedToHistory false so we keep trying to add ourselves
			// to the history, in case there was some other ped that used to use
			// the point and registered with the history.
		}
		else if(!GetIsWaitingForEntity())
		{
			// We have no point and we're not waiting for a prop to materialize,
			// so we can probably set the flag to not have to worry about it on the next update.
			m_iFlags.SetFlag(SF_WasAddedToHistory);
		}
	}

	// Handle ped reset flags.
	if (GetState() == State_Exit || GetState() == State_StreamWaitClip || GetState() == State_WaitForProbes 
		|| GetState() == State_WaitForPath || GetState() == State_PanicExit || GetState() == State_NMExit || GetState() == State_PanicNMExit)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_PermitEventDuringScenarioExit, true);
	}

	if (rInfo.GetIsFlagSet(CScenarioInfoFlags::BlockNonTempEvents))
	{
		pPed->SetBlockingOfNonTemporaryEvents(true);
	}

	if (rInfo.GetIsFlagSet(CScenarioInfoFlags::DisableAgitation))
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CanBeAgitated,false);
	}

	if (GetState() > State_GoToStartPosition)
	{
		if (rInfo.GetIsFlagSet(CScenarioInfoFlags::ForceWantedLevelWhenKilled))
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceWantedLevelWhenKilled, true);
		}

		if (rInfo.GetIsFlagSet(CScenarioInfoFlags::EnableKinematicPhysicsForPed))
		{
			// Reset flag - use kinematic physics so the ped does not get pushed around too easily.
			pPed->SetPedResetFlag(CPED_RESET_FLAG_TaskUseKinematicPhysics, true);
		}

		if (rInfo.GetIsFlagSet(CScenarioInfoFlags::DontActivateRagdollForCollisionWithAnyPedImpact))
		{
			// Reset flag - turn off reaction to impact with any ped.
			pPed->SetPedResetFlag(CPED_RESET_FLAG_DontActivateRagdollFromAnyPedImpactReset, true);
		}

		if (rInfo.GetIsFlagSet(CScenarioInfoFlags::DontActivateRagdollForCollisionWithPlayerBumpImpact))
		{
			// Reset flag - turn off reaction to impact with the player when doing an animated bump.
			pPed->SetPedResetFlag(CPED_RESET_FLAG_DontActivateRagdollFromPlayerPedImpactReset, true);
		}

		if (rInfo.GetIsFlagSet(CScenarioInfoFlags::DontActivateRagdollForCollisionWithPlayerRagdollImpact))
		{
			// Reset flag - turn off reaction to impact with the player when ragdolling.
			pPed->SetPedResetFlag(CPED_RESET_FLAG_DontActivateRagdollFromPlayerRagdollImpactReset, true);
		}

		// Set the ped capsule radius override.
		float fRadius = rInfo.GetCapsuleRadiusOverride();
		if (fRadius > 0.0f)
		{
			pPed->SetDesiredMainMoverCapsuleRadius(fRadius, true);
		}
	}

	// It is possible that the target becomes invalid during the course of the event response.  In this case, stop doing a reaction.
	if (IsRespondingToEvent() && !m_Target.GetIsValid() && EventRequiresValidTarget(m_iEventTriggeringPanic))
	{
		if (!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee))
		{
			EndEventReact();
		}
	}

	// Handle motion states while playing certain animations.
	HandleAnimatedTurns();
	
	// The block leg ik is a reset flag, so we need to continuously set it if needed
	// For peds sitting on sloped ground this can prevent them from sinking, popping back up and sinking again
	if (rInfo.GetIsFlagSet(CScenarioInfoFlags::BlockLegIk))
	{
		pPed->SetDisableLegSolver(true);
	}
	return FSM_Continue;
}


bool CTaskUseScenario::ProcessMoveSignals()
{
	// Return false if nothing is happening.
	bool bReturnValue = false;

	// Cache ped handle
	CPed* pPed = GetPed();

	// Check the state
	switch(GetState())
	{
		case State_Start:
		case State_GoToStartPosition:
		case State_Enter:
		case State_ScenarioTransition:
		{
			// If we have a prop management helper
			if (m_pPropHelper)
			{
				// Process Move events/loading/tags for props on the idle clip
				CPropManagementHelper::Context context(*pPed);
				context.m_ClipHelper	= GetClipHelper();
				context.m_PropClipId	= GetChosenClipId(pPed, this);
				context.m_PropClipSetId	= GetEnterClipSetId(pPed, this);
				m_pPropHelper->ProcessMoveSignals(context);

				// Return true since we want to always check for prop signals
				bReturnValue = true;
			}
			break;
		}
		case State_PanicExit:
		{
			// if we have a prop management helper
			if (m_pPropHelper)
			{
				// Process Move events/loading/tags for props on the panic clip
				CPropManagementHelper::Context context(*pPed);
				context.m_ClipHelper	= GetClipHelper();
				context.m_PropClipId	= GetChosenClipId(pPed, this);
				context.m_PropClipSetId = m_panicClipSetId;
				m_pPropHelper->ProcessMoveSignals(context);

				// Return true since we want to always check for prop signals
				bReturnValue = true;
			}
			break;
		}
		case State_Exit:
		{
			// if we have a prop management helper
			if (m_pPropHelper)
			{
				// Process Move events/loading/tags for props on the exit clip
				CPropManagementHelper::Context context(*pPed);
				context.m_ClipHelper	= GetClipHelper();
				context.m_PropClipId	= GetChosenClipId(pPed, this);
				context.m_PropClipSetId = m_exitClipSetId;
				m_pPropHelper->ProcessMoveSignals(context);

				// Return true since we want to always check for prop signals
				bReturnValue = true;
			}
			break;
		}
	}
	
	// Return false if nothing is going on, possibly stopping further calls to ProcessMoveSignals().
	return bReturnValue;
}


CTask::FSM_Return CTaskUseScenario::ProcessPostFSM()
{
	m_iFlags.ClearFlag(SF_DontLeaveThisFrame);

	if(m_iFlags.IsFlagSet(SF_SynchronizedMaster))
	{
		// Clear this flag, keeping track of if any slaves think they are not ready.
		// If they are not, they should tell us again before our next update.
		m_iFlags.ClearFlag(SF_SlaveNotReadyForSync);
	}
	else if(m_iFlags.IsFlagSet(SF_SynchronizedSlave))
	{
		// If m_bSlaveReadyForSyncedExit didn't get set this update, tell the
		// master that we don't seem to be ready.
		if(!m_bSlaveReadyForSyncedExit)
		{
			CTaskUseScenario* pMasterTask = FindSynchronizedMasterTask();
			if(pMasterTask)
			{
				pMasterTask->NotifySlaveNotReadyForSyncedAnim();
			}
		}
	}

	// Request any chosen gesture clip set
	const CConditionalAnimsGroup *pConditionalAnimsGroup = GetConditionalAnimsGroup();
	if(pConditionalAnimsGroup)
	{
		s32 conditionalAnimIndex = GetConditionalAnimIndex();
		if(conditionalAnimIndex >= 0 && conditionalAnimIndex < pConditionalAnimsGroup->GetNumAnims())
		{
			const CConditionalAnims *pConditionalAnims = pConditionalAnimsGroup->GetAnims(conditionalAnimIndex);
			if(pConditionalAnims)
			{
				fwMvClipSetId clipSetId = pConditionalAnims->GetGestureClipSetId();
				if(clipSetId != CLIP_SET_ID_INVALID)
				{
					g_pGestureManager->RequestScenarioGestureClipSet(clipSetId);
				}
			}
		}
	}

	return FSM_Continue;
}


CTask::FSM_Return CTaskUseScenario::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin

	FSM_State(State_Start)
		FSM_OnEnter
			return Start_OnEnter();
		FSM_OnUpdate
			return Start_OnUpdate();
		FSM_OnExit
			Start_OnExit();

	FSM_State(State_GoToStartPosition)
		FSM_OnEnter
			GoToStartPosition_OnEnter();
		FSM_OnUpdate
			return GoToStartPosition_OnUpdate();
		FSM_OnExit
			GoToStartPosition_OnExit();

	FSM_State(State_Enter)
		FSM_OnEnter
			Enter_OnEnter();
		FSM_OnUpdate
			return Enter_OnUpdate();

	FSM_State(State_PlayAmbients)
		FSM_OnEnter
			PlayAmbients_OnEnter();
		FSM_OnUpdate
			return PlayAmbients_OnUpdate();
		FSM_OnExit
			PlayAmbients_OnExit();

	FSM_State(State_Exit)
		FSM_OnEnter
			Exit_OnEnter();
		FSM_OnUpdate
			return Exit_OnUpdate();
		FSM_OnExit
			Exit_OnExit();

	FSM_State(State_AggroOrCowardIntro)
		FSM_OnEnter
			AggroOrCowardIntro_OnEnter();
		FSM_OnUpdate
			return AggroOrCowardIntro_OnUpdate();
		FSM_OnExit
			AggroOrCowardIntro_OnExit();

	FSM_State(State_AggroOrCowardOutro)
		FSM_OnEnter
			AggroOrCowardOutro_OnEnter();
		FSM_OnUpdate
			return AggroOrCowardOutro_OnUpdate();
		FSM_OnExit
			AggroOrCowardOutro_OnExit();

	FSM_State(State_WaitingOnExit)
		FSM_OnEnter
			WaitingOnExit_OnEnter();
		FSM_OnUpdate
			return WaitingOnExit_OnUpdate();
		FSM_OnExit
			WaitingOnExit_OnExit();

	FSM_State(State_StreamWaitClip)
		FSM_OnEnter
			StreamWaitClip_OnEnter();
		FSM_OnUpdate
			return StreamWaitClip_OnUpdate();
		FSM_OnExit
			StreamWaitClip_OnExit();

	FSM_State(State_WaitForProbes)
		FSM_OnEnter
			WaitForProbes_OnEnter();
		FSM_OnUpdate
			return WaitForProbes_OnUpdate();

	FSM_State(State_WaitForPath)
		FSM_OnEnter
			WaitForPath_OnEnter();
		FSM_OnUpdate
			return WaitForPath_OnUpdate();

	FSM_State(State_PanicExit)
		FSM_OnEnter
			PanicExit_OnEnter();
		FSM_OnUpdate
			return PanicExit_OnUpdate();
		FSM_OnExit
			PanicExit_OnExit();

	FSM_State(State_NMExit)
		FSM_OnEnter
			NMExit_OnEnter();
		FSM_OnUpdate
			return NMExit_OnUpdate();

	FSM_State(State_PanicNMExit)
		FSM_OnEnter
			PanicNMExit_OnEnter();
		FSM_OnUpdate
			return PanicNMExit_OnUpdate();

	FSM_State(State_ScenarioTransition)
		FSM_OnEnter
			ScenarioTransition_OnEnter();
		FSM_OnUpdate
			return ScenarioTransition_OnUpdate();

	FSM_State(State_Finish)
		FSM_OnUpdate
			return FSM_Quit;

	FSM_End
}

bool CTaskUseScenario::IsInProcessPostMovementAttachState()
{
	if ((GetState() == State_Enter || GetState() == State_Exit || GetState() == State_PanicExit || 
		GetState() == State_AggroOrCowardIntro || GetState() == State_AggroOrCowardOutro) )
	{
		return true;
	}

	return false;
}

bool CTaskUseScenario::ProcessPostMovement()
{
	if (IsInProcessPostMovementAttachState() 
		&& GetClipHelper())
	{
		const crClip* pClip = GetClipHelper()->GetClip();
		if (pClip)
		{
			// The target situation only needs to be updated if the scenario is attached to a dynamic entity.
			// Skip this logic if exiting to a fall.
			if(IsDynamic() && !m_bExitToLowFall && !m_bExitToHighFall)
			{
				// Don't adjust the fall status.
				UpdateTargetSituation(false);
			}
			
			const float fClipPhase = GetClipHelper()->GetPhase();

			Vector3 vNewPedPosition(Vector3::ZeroType);
			Quaternion qNewPedOrientation(0.0f,0.0f,0.0f,1.0f);

			// Calculate the new ped position absolutely relative to our target situation, begin by calculating the initial situation
			// we do this every frame in case we want to move the target
			m_PlayAttachedClipHelper.ComputeInitialSituation(vNewPedPosition, qNewPedOrientation);

			// Add the mover changes up to the current phase to the situation 
			m_PlayAttachedClipHelper.ComputeSituationFromClip(pClip, 0.0f, fClipPhase, vNewPedPosition, qNewPedOrientation);

			// Some asserts were reported where this quaternion wasn't considered a pure rotation,
			// as it wasn't quite unit length. Looked like it was close enough that it's probably
			// due to round-off errors, so it should be safe to just normalize it here. Similar things
			// are done in other places where ComputeSituationFromClip() is used (CTaskEnterVehicle, etc),
			// and the performance cost should be fairly small compared to everything else we do here.
			qNewPedOrientation.Normalize();

			Vector3 vOffset(Vector3::ZeroType);
			Quaternion qOffset(0.0f,0.0f,0.0f,1.0f);

			if (!m_bExitToHighFall)
			{
				// Compute a situational offset which defines how far away from the target situation we are
				m_PlayAttachedClipHelper.ComputeOffsets(pClip, fClipPhase, 1.0f, vNewPedPosition, qNewPedOrientation, vOffset, qOffset);

				// Make up the offset during certain parts of the clip to nail the target
				m_PlayAttachedClipHelper.ApplyOffsets(vNewPedPosition, qNewPedOrientation, pClip, vOffset, qOffset, fClipPhase);
			}



			//Attach the ped.
			Attach(vNewPedPosition, qNewPedOrientation);
		}
		else
		{
#if __DEV
			taskWarningf("NULL clip pointer from set '%s' while trying to get clip '%s' {%d}. Current state: %s, previous state: %s. Repeat due to blackboard = ???", GetClipHelper()->GetClipSet().TryGetCStr(), GetClipHelper()->GetClipId().TryGetCStr(), GetClipHelper()->GetClipId().GetHash(), GetStateName(GetState()), GetStateName(GetPreviousState()));
#endif
		}
	}

	return true;
}

CTask::FSM_Return CTaskUseScenario::Start_OnEnter()
{
	// When we enter the task we need to grab some information from our scenario
	// This included the idle time to be at the scenario for and the entry and exit clip sets
	const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(m_iScenarioIndex);

	if(NetworkInterface::IsGameInProgress() && !GetPed()->IsNetworkClone())
	{
		const CTask* pParentTask = GetParent();
		if(pParentTask && pParentTask->GetTaskType() == CTaskTypes::TASK_AMBULANCE_PATROL)
		{
			m_bMedicTypeScenario = true;
		}
	}

	if ( (sm_bDetachScenarioPeds || sm_bAggressiveDetachScenarioPeds) && GetPed() && GetPed()->GetIsAttached() && GetTaskAmbientMigrateHelper())
	{
		m_bHasTaskAmbientMigrateHelper = true;
	}

	if (taskVerifyf(pScenarioInfo, "Couldn't find scenario info for scenario index %i", m_iScenarioIndex))
	{
		// Only if the user didn't specify a time by calling SetTimeToLeave() do we get the time from
		// the scenario info.
		if(m_fIdleTime < (sm_fUninitializedIdleTime + 0.5f)) // Taking into account serialization quantisation errors 
		{
			float timeTillPedLeaves = pScenarioInfo->GetTimeTillPedLeaves();

			// Use the override from the scenario point itself, if there is one.
			CScenarioPoint* pScenarioPoint = GetScenarioPoint();
			if(pScenarioPoint)
			{
				// Quit if the scenario point obstructed
				if (pScenarioPoint->IsObstructedAndReset())
				{
					return FSM_Quit;
				}

				if(pScenarioPoint->HasTimeTillPedLeaves())
				{
					timeTillPedLeaves = pScenarioPoint->GetTimeTillPedLeaves();
				}
			}

#if SCENARIO_DEBUG
			if(m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData))
			{
				// Ped is never ready to leave ... until queue says they are ... 
				timeTillPedLeaves = -1;
			}
#endif	// SCENARIO_DEBUG

			// If the scenario time is set to >= 0.0, make sure that when randomised it doesn't go negative, leaving the ped stationary indefinately.
			if( timeTillPedLeaves >= 0.0f )
			{
				// If the PreciseUseTime is set, we don't do any randomization.
				if(pScenarioPoint && pScenarioPoint->IsFlagSet(CScenarioPointFlags::PreciseUseTime))
				{
					m_fIdleTime = timeTillPedLeaves;
				}
				else
				{
					// Compute how much randomness we should have, which is either a fixed amount, or a fraction of the total use time.
					float fRandom = Max(sm_Tunables.m_TimeToLeaveRandomAmount, timeTillPedLeaves*sm_Tunables.m_TimeToLeaveRandomFraction);

					m_fIdleTime = Max(timeTillPedLeaves + fwRandom::GetRandomNumberInRange(-fRandom, fRandom), 0.0f);

					if(m_iFlags.IsFlagSet(SF_AdvanceUseTimeRandomly))
					{
						float advanceProportion = fwRandom::GetRandomNumberInRange(0.0f, sm_Tunables.m_AdvanceUseTimeRandomMaxProportion);
						m_fIdleTime = m_fIdleTime*(1.0f - advanceProportion);
					}
				}
			}
			else
			{
				m_fIdleTime = timeTillPedLeaves;
			}
		}

		if (m_iFlags.IsFlagSet(SF_IdleForever))
		{
			m_fIdleTime = -1;
		}

		taskAssertf(m_fIdleTime <= fMAX_IDLE_TIME, "This value should be sync with CClonedTaskUseScenarioInfo::SIZEOF_IDLE_TIME");

		m_pConditionalAnimsGroup = pScenarioInfo->GetClipData();


		if(m_pConditionalAnimsGroup)
		{
			s32 iPriority;

			aiAssert(m_ConditionalAnimsChosen==-1);

			CScenarioCondition::sScenarioConditionData conditionData;
			InitConditions(conditionData);
			if (GetScenarioPoint())
			{
				// ChooseConditionalAnimationsConsiderNearbyPeds() needs the position of the scenario to do a spatial query on.
				// Rather than have a new member or parameter, reuse the existing vector parameter on the sScenarioConditionData we'll pass to that function. [4/30/2013 mdawe]
				conditionData.vAmbientEventPosition = GetScenarioPoint()->GetPosition();
			} 
			else
			{
				// If we don't have a scenario point, we'll look around the ped's current position. 
				conditionData.vAmbientEventPosition = GetPed()->GetTransform().GetPosition();
			}

			// if we have not restarted
			if (!m_bDefaultTransitionRequested)
			{
				// Allow all anims in group, because we can create a prop if necessary.
				conditionData.iScenarioFlags |= SF_IgnoreHasPropCheck;
			}

			conditionData.m_bNeedsValidEnterClip = !m_iFlags.IsFlagSet(SF_SkipEnterClip);

			if (   CAmbientAnimationManager::ChooseConditionalAnimationsConsiderNearbyPeds(conditionData,iPriority,m_pConditionalAnimsGroup,m_ConditionalAnimsChosen,CConditionalAnims::AT_BASE)
					SCENARIO_DEBUG_ONLY( || m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData))
					)
			{
				m_baseClipSetId  = ChooseConditionalClipSet(CConditionalAnims::AT_BASE, conditionData);

				const CConditionalClipSet* pEnterClipSet = NULL;

				 // The prop set for the entire anim group, if any
				u32 uAnimGroupPropSetHash = 0;

				// Figure out how the ped should enter the scenario.
				fwMvClipSetId enterSet = DetermineEnterAnimations(conditionData, &pEnterClipSet, &uAnimGroupPropSetHash);
				SetEnterClipSetId(enterSet, GetPed(), this);
				SetChosenClipId(CLIP_ID_INVALID, GetPed(), this);
				if (pEnterClipSet)
				{
					m_bConsiderOrientation = pEnterClipSet->GetConsiderOrientation();
				}
				if (uAnimGroupPropSetHash)
				{
					const CAmbientPropModelSet* pPropSet = CAmbientAnimationManager::GetPropSetFromHash(uAnimGroupPropSetHash);
					if (Verifyf(pPropSet, "Could not get a propset from conditional anim hash in TaskUseScenario"))
					{
						strLocalIndex propModelIndex = pPropSet->GetRandomModelIndex();
						if (Verifyf(CModelInfo::IsValidModelInfo(propModelIndex.Get()), "Prop set [%s] with model index [%d] not a valid model index", pPropSet->GetName(), propModelIndex.Get() ))
						{
							const CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(propModelIndex));
							Assert(pModelInfo);
							if (!m_pPropHelper)
							{
								// See if there's an existing PropManagmentHelper we should be using. [5/20/2013 mdawe]
								m_pPropHelper = GetPed()->GetPedIntelligence()->GetActivePropManagementHelper();
								if (!m_pPropHelper)
								{
									// If there's not an existing PropManagementHelper, create one. [5/20/2013 mdawe]
									m_pPropHelper = rage_new CPropManagementHelper();
								}
							}

							gnetDebug3("%s %s CTaskUseScenario::Start_OnEnter: this [%p]. m_pPropHelper %s, IsPropLoaded %s, IsHoldingProp %s, IsPropSharedAmongTasks %s. GetTaskAmbientMigrateHelper [%p], m_enterClipSetId %s.  m_iFlags 0x%x FC: %d\n",
								GetPed()->GetDebugName(),
								GetPed()->IsNetworkClone()?"C":"L",
								this,
								m_pPropHelper->GetPropNameHash().TryGetCStr(),
								m_pPropHelper->IsPropLoaded()?"T":"F",
								m_pPropHelper->IsHoldingProp()?"T":"F",
								m_pPropHelper->IsPropSharedAmongTasks()?"T":"F",
								GetTaskAmbientMigrateHelper(),
								m_enterClipSetId.TryGetCStr(),
								m_iFlags.GetAllFlags(),
								fwTimer::GetFrameCount());

							//Check if we need to force load and equip prop if we've skipped enter anim stage that was expected to load prop
							if( GetPed()->IsNetworkClone() && 
								(m_pPropHelper->GetPropNameHash().GetHash()==0 || GetTaskAmbientMigrateHelper()==NULL)  &&
								GetStateFromNetwork()==State_PlayAmbients) 
							{
								CTaskInfo* pTaskInfo =GetPed()->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_USE_SCENARIO, PED_TASK_PRIORITY_MAX, false);

								if(pTaskInfo)
								{
									CClonedTaskUseScenarioInfo* pTUSInfo = static_cast<CClonedTaskUseScenarioInfo*>(pTaskInfo);

									bool bSyncDataNotExpectedToSkipEnter = (pTUSInfo->GetFlags() & SF_SkipEnterClip) != SF_SkipEnterClip;  

									if(bSyncDataNotExpectedToSkipEnter && m_iFlags.IsFlagSet(SF_SkipEnterClip) ) //check if we have overridden skip to snap ped at start
									{

										if( m_pPropHelper->GetPropNameHash().GetHash() != 0 &&
											taskVerifyf(!m_pPropHelper->IsHoldingProp(),"%s Dont expect to be holding prop %s",GetPed()->GetDebugName(),m_pPropHelper->GetPropNameHash().TryGetCStr()))										
										{
											taskAssertf(!m_pPropHelper->IsPropSharedAmongTasks(),"%s Dont expect to be shared %s",GetPed()->GetDebugName(),m_pPropHelper->GetPropNameHash().TryGetCStr());
											m_pPropHelper->ReleasePropRefIfUnshared(GetPed());
										}

										// request the prop
										m_pPropHelper->RequestProp(pModelInfo->GetModelNameHash());

										// set flags on the prop helper
										const CConditionalAnims* pAnims = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimsChosen);
										m_pPropHelper->SetCreatePropInLeftHand(pAnims->GetCreatePropInLeftHand());
										m_pPropHelper->SetDestroyProp(pAnims->ShouldDestroyPropInsteadOfDropping());
										m_pPropHelper->SetForcedLoading();
									}
								}
							}
							
							// If we're a ped spawning into this UseScenarioTask, the prop will be requested in CTaskAmbientClips. Don't request it here. [10/2/2012 mdawe]
							if (!m_iFlags.IsFlagSet(SF_SkipEnterClip) || (!m_iFlags.IsFlagSet(SF_StartInCurrentPosition) && !m_iFlags.IsFlagSet(SF_Warp)))
							{
								m_pPropHelper->RequestProp(pModelInfo->GetModelNameHash());
							}

							const CConditionalAnims* pAnims = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimsChosen);
							m_pPropHelper->SetCreatePropInLeftHand(pAnims->GetCreatePropInLeftHand());
							m_pPropHelper->SetDestroyProp(pAnims->ShouldDestroyPropInsteadOfDropping());

							if( NetworkInterface::IsGameInProgress() && 
								!GetPed()->IsNetworkClone() && 
								!GetTaskAmbientMigrateHelper() &&
								m_pPropHelper->GetPropNameHash().GetHash()!=0 && 
								!m_pPropHelper->IsPropLoaded() &&
								GetEnterClipSetId(GetPed(),this)==CLIP_SET_ID_INVALID &&
								m_iFlags.IsFlagSet(SF_StartInCurrentPosition) && 
								pAnims->GetChanceOfSpawningWithAnything()==1.0f ) 
							{
								gnetDebug3("%s this [%p] CTaskUseScenario::Start_OnEnter m_pPropHelper->SetForcedLoading. m_pPropHelper 0x%x, %s. m_iFlags 0x%x  FC: %d\n",
									GetPed()->GetDebugName(),
									this,
									m_pPropHelper->GetPropNameHash().GetHash(),
									m_pPropHelper->GetPropNameHash().TryGetCStr(),
									m_iFlags.GetAllFlags(),
									fwTimer::GetFrameCount());

								//Going straight into an ambient task without enter anim and definitely needs prop immediately 
								m_pPropHelper->SetForcedLoading();
							}

							// Request move signal calls
							RequestProcessMoveSignalCalls();
						}
					}
				}
			}
		}

		// Look for a prop in the environment to use.
		if(pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::UsePropFromEnvironment))
		{
			taskAssert(m_pPropHelper);
			m_pPropHelper->SetUsePropFromEnvironment(true);

			// Look for a prop in the environment, but only if one hasn't been specified already
			// (the user may have passed one in).
			if (!m_pPropHelper->GetPropInEnvironment())
			{
				m_pPropHelper->SetPropInEnvironment(FindPropInEnvironment());
			}

			// If we didn't find a prop, quit right away as we won't be able to use
			// this scenario properly.
			if(!m_pPropHelper->GetPropInEnvironment())
			{
				return FSM_Quit;
			}

			// If the UsePropFromEnvironment flag is set, we probably don't want any other prop to be created.
			SetCreateProp(false);
		}

		if (pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::DontActivatePhysicsOnRelease))
		{
			taskAssert(m_pPropHelper);
			m_pPropHelper->SetDontActivatePhysicsOnRelease(true);
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskUseScenario::Start_OnUpdate()
{
	if(mbNewTaskMigratedWhileCowering  &&  taskVerifyf(NetworkInterface::IsGameInProgress(),"Only expect mbNewTaskMigratedWhileCowering used in network games"))
	{
		m_bNeedsCleanup = true; //always check cleanup when migrating

		HandleOnEnterAmbientScenarioInfoFlags();
		HandlePanicSetup();

		//Handle re-attaching if needed if ped has been left
		//unattached when last task quit, or when warping to clone and creating anew
		NetworkSetUpPedPositionAndAttachments();

		return FSM_Continue;
	}

	if (HasTriggeredExit())
	{
		// This can happen if someone has requested an exit but we're still trying to stream our Enter clips.
		SetState(State_Finish);
		return FSM_Continue;
	}

	// If we get to this point, we may be about to attach and do other things,
	// so CleanUp() needs to do some work if it gets called.
	m_bNeedsCleanup = true;

	//Get the scenario position and heading.
	Vector3 vWorldPosition;
	float fWorldHeading;
	GetScenarioPositionAndHeading(vWorldPosition, fWorldHeading);

	// B* 2038626 - It's a bit too risky to reuse IgnoreScenarioPointHeading so instead let's use a new info flag here.
	// Also restricting it to world effects only, as I don't think this would work out well with entity attachments since we 
	// don't cache off what the randomly selected heading was.
	// Don't migrate this code!
	if (GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::RandomizeSpawnedPedHeading) && m_eScenarioMode == SM_WorldEffect)
	{
		fWorldHeading = fwRandom::GetRandomNumberInRange(-PI, PI);
	}
	// End don't migrate block
	
	// Set the target for the attach clip helper immediately.
	Quaternion qTargetRotation;
	qTargetRotation.FromEulers(Vector3(0.0f,0.0f,fWorldHeading), "xyz");
	m_PlayAttachedClipHelper.SetTarget(vWorldPosition, qTargetRotation);

	CPed* pPed = GetPed();

	// If this scenario point has the AllowInvestigation flag, set some flags on the ped [1/15/2013 mdawe]
	if (pPed && GetScenarioPoint() && GetScenarioPoint()->DoesAllowInvestigation())
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ListensToSoundEvents, true);
		CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();
		if (pPedIntelligence)
		{
			pPedIntelligence->GetCombatBehaviour().SetFlag(CCombatData::BF_CanInvestigate);

			// Any ped spawned at this kind of point will automatically be set to hate the player. [1/15/2013 mdawe]
			pPedIntelligence->SetRelationshipGroup(CRelationshipManager::s_pAggressiveInvestigateGroup);
		}
	}

	// If starting in the current position
	if (m_iFlags.IsFlagSet(SF_StartInCurrentPosition))
	{
		if (m_iFlags.IsFlagSet(SF_SkipEnterClip) || GetEnterClipSetId(GetPed(), this) ==CLIP_SET_ID_INVALID)
		{
			SetState(State_PlayAmbients);
		}
		else if(AreEnterClipsStreamed())
		{
			SetState(State_Enter);
		}
		else if (m_pPropHelper)
		{
			m_pPropHelper->UpdateExistingOrMissingProp(*GetPed());
			CPropManagementHelper::Context context(*GetPed());
			context.m_ClipHelper = GetClipHelper();
			context.m_PropClipId = GetChosenClipId(GetPed(), this);
			context.m_PropClipSetId = GetEnterClipSetId(GetPed(), this);
			m_pPropHelper->Process(context);
		}
		return FSM_Continue;
	}
	else if (m_iFlags.IsFlagSet(SF_Warp))
	{
		bool bAllowSetPosition = true;

		if (pPed->GetIsAttached())
		{
			bAllowSetPosition = false;
		}

		if (NetworkInterface::IsGameInProgress())
		{
			if (pPed->IsNetworkClone() && pPed->GetUsingRagdoll())
			{
				bAllowSetPosition = false;
			}
		}

		if(bAllowSetPosition)
		{
			// We used to always pass in true for this parameter, but the call to ScanUntilProbeTrue()
			// is potentially dangerous since it can use up a lot of CPU time if we fail to find ground
			// (due to a misplaced scenario point, for example). To be safe, for mission peds we still do
			// do the extra scan. Ambient peds that spawn in place should hopefully already know what interior
			// they belong to when they got added to the world.
			const bool triggerPortalRescan = pPed->PopTypeIsMission();

			pPed->Teleport(vWorldPosition, fWorldHeading, true, triggerPortalRescan);
		}

		if(ShouldBeAttachedWhileUsing())
		{
			//Calculate the orientation from the heading.
			Quaternion qWorldOrientation;
			qWorldOrientation.FromEulers(Vector3(0.0f, 0.0f, fWorldHeading), "xyz");
			
			//Attach the ped.
			Attach(vWorldPosition, qWorldOrientation);
		}
		else
		{
			// We are warping, and should not be attached. By setting this flag, we will ensure
			// that the next time we switch from low LOD physics to regular LOD, we get activated,
			// meaning that the physics system can take care of positioning us properly against
			// the surrounding geometry. This should only happen once, if successful (i.e. not switching
			// back to low LOD again before settling). It may also not happen at all if we are already
			// in regular LOD and switch to low LOD while not active, or if the task ends before any of
			// this becomes relevant.
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ActivateOnSwitchFromLowPhysicsLod, true);

			if (m_eScenarioMode == SM_WorldEffect && bAllowSetPosition)
			{
				aiAssert(pPed->GetCapsuleInfo());
				vWorldPosition.z += pPed->GetCapsuleInfo()->GetGroundToRootOffset(); 
				pPed->SetPosition(vWorldPosition, true, true, true);			
			}

			// If we teleported earlier, it appears that CPED_CONFIG_FLAG_IsStanding was cleared,
			// which causes the ped to try to activate physics on every frame when in low LOD physics.
			// To avoid this, we set the flag again here.
			if(bAllowSetPosition)
			{
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsStanding, true);
			}
		}

#if SCENARIO_DEBUG
		if (m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData))
		{
			switch(m_pDebugInfo->m_AnimType)
			{
			case CConditionalAnims::AT_ENTER:
				if(GetEnterClipSetId(GetPed(), this)!=CLIP_SET_ID_INVALID && AreEnterClipsStreamed())
				{
					SetState(State_Enter);
				}
				break;
			case CConditionalAnims::AT_EXIT:
				LeaveScenario();
				break;
			case CConditionalAnims::AT_BASE:
			case CConditionalAnims::AT_VARIATION:
			case CConditionalAnims::AT_REACTION:
				SetState(State_PlayAmbients);
				break;
			case CConditionalAnims::AT_PANIC_INTRO:
				m_iScenarioReactionFlags.SetFlag(SREF_RespondingToEvent);
				SetState(State_PlayAmbients);
				break;
			case CConditionalAnims::AT_PANIC_EXIT:
				m_iScenarioReactionFlags.SetFlag(SREF_RespondingToEvent);
				m_iScenarioReactionFlags.SetFlag(SREF_ExitingToFlee);
				SetState(State_PlayAmbients);
				break;
			case CConditionalAnims::AT_PANIC_BASE:
				m_iDebugFlags.SetFlag(SFD_SkipPanicIntro);
				m_iScenarioReactionFlags.SetFlag(SREF_RespondingToEvent);
				SetState(State_PlayAmbients);
				break;
			case CConditionalAnims::AT_PANIC_OUTRO:
				m_iDebugFlags.SetFlag(SFD_SkipPanicIntro);
				m_iDebugFlags.SetFlag(SFD_SkipPanicIdle);
				m_iScenarioReactionFlags.SetFlag(SREF_RespondingToEvent);
				SetState(State_PlayAmbients);
				break;
			case CConditionalAnims::AT_COUNT:
				//exit case ... 
				SetState(State_Finish);
				break;
			default:
				{
					aiAssertf(0, "Trying to debug unsupported anim type '%s'", CConditionalAnims::GetTypeAsStr(m_pDebugInfo->m_AnimType));
					SetState(State_Finish);
				}
				break;
			};   
			
			return FSM_Continue;
		}
#endif // SCENARIO_DEBUG
		if (m_iFlags.IsFlagSet(SF_SkipEnterClip))
		{
			SetState(State_PlayAmbients);
		}
		else if(AreEnterClipsStreamed())
		{
			SetState(State_Enter);
		}
		else if (m_pPropHelper)
		{
			m_pPropHelper->UpdateExistingOrMissingProp(*GetPed());
		}

		return FSM_Continue;
	}

	// Wait for the enter clips to be streamed in for some scenario types.
	// This prevents the ped being tasked to go to the scenario point directly instead of the entry location.
	// This is important when navigating perfectly is worth more than continuous movement.
	if (GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::WaitOnStreamingBeforeNavigatingToStartPosition))
	{
		// There is a timeout on this behaviour of 5 seconds in case we get into a terrible streaming situation.
		// In practice it only takes a few frames at most to get these clips.
		if (GetTimeInState() < 5.0f && ((!AreEnterClipsStreamed() && m_enterClipSetId != CLIP_SET_ID_INVALID) || (!AreBaseClipsStreamed() && m_baseClipSetId != CLIP_SET_ID_INVALID)))
		{
			OverrideStreaming();
			return FSM_Continue;
		}
	}

	SetState(State_GoToStartPosition);

	return FSM_Continue;
}


void CTaskUseScenario::Start_OnExit()
{
}


void CTaskUseScenario::GoToStartPosition_OnEnter()
{
	CPed* pPed = GetPed();

	m_iFlags.ClearFlag(SF_GotoPositionTaskSet);

	// Check if the destination scenario point is flagged to open doors
	CScenarioPoint* pScenarioPoint = GetScenarioPoint();
	if (pScenarioPoint && pScenarioPoint->GetOpenDoor())
	{
		// enable open doors arm IK
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_OpenDoorArmIK, true);
	}

	if(!pPed->IsNetworkClone()) //B* 1280282 Don't create this subtask on clones as it uses complex movement task
	{  
		Vector3 vWorldPosition;
		float fWorldHeading;
		GetScenarioPositionAndHeading(vWorldPosition, fWorldHeading);

		Vector3 vDiff = vWorldPosition-VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		if( vDiff.XYMag2() > rage::square(GetTunables().m_SkipGotoXYDist) || ABS(vDiff.z) > GetTunables().m_SkipGotoZDist || 
			fabs(SubtractAngleShorter(pPed->GetCurrentHeading(), fWorldHeading) * RtoD) > GetTunables().m_SkipGotoHeadingDeltaDegrees )
		{
			StartInitialGotoSubtask(vWorldPosition, fWorldHeading);
		}
	}

	// if we have a prop helper
	if (m_pPropHelper)
	{
		// Request move signal calls
		RequestProcessMoveSignalCalls();
	}
}

void CTaskUseScenario::StartInitialGotoSubtask(const Vector3& vWorldPosition, float fWorldHeading)
{
	CTaskGoToScenario::TaskSettings settings( vWorldPosition, fWorldHeading );

	const CScenarioInfo *pInfo = CScenarioManager::GetScenarioInfo( GetScenarioType() );

	if (pInfo->GetIsFlagSet( CScenarioInfoFlags::IgnoreScenarioPointHeading)) 
	{
		m_PlayAttachedClipHelper.SetRotationalOffsetSetEnabled(false);
		settings.Set(CTaskGoToScenario::TaskSettings::IgnoreStopHeading, true);
	}

	if(m_OverriddenMoveBlendRatio >= 0.0f)
	{
		settings.SetMoveBlendRatio(m_OverriddenMoveBlendRatio);
	}
	else
	{
		if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < pInfo->GetChanceOfRunningTowards())
		{
			settings.SetMoveBlendRatio(MOVEBLENDRATIO_RUN);
		}
	}

	settings.Set( CTaskGoToScenario::TaskSettings::IgnoreSlideToPosition, pInfo->GetIsFlagSet( CScenarioInfoFlags::IgnoreSlideToCoordOnArrival ) );

	CTaskGoToScenario* pGoToTask = rage_new CTaskGoToScenario(settings);
	if(m_pAdditionalTaskDuringApproach)
	{
		const aiTask* pAdditionalTask = m_pAdditionalTaskDuringApproach->Copy();
		taskAssert(dynamic_cast<const CTask*>(pAdditionalTask));
		pGoToTask->SetAdditionalTaskDuringApproach(static_cast<const CTask*>(pAdditionalTask));
	}

	SetNewTask(pGoToTask);
}


void CTaskUseScenario::CalculateStartPosition( Vector3 & pedPosition, float & pedRotation ) const
{
	const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(GetEnterClipSetId(GetPed(), this), GetChosenClipId(GetPed(), this));
	if (pClip)
	{
		// Compute the world space matrix for where we want to be when the intro animation finishes.
		const Vec3V introAnimEndPosV = RCC_VEC3V(pedPosition);
		Mat34V introAnimEndMtrx;
		Mat34VFromZAxisAngle(introAnimEndMtrx, ScalarVFromF32(pedRotation), introAnimEndPosV);

		// Compute the transformation matrix that will be applied over the course
		// of the whole intro animation.
		Mat34V introAnimTransformation;
		fwAnimHelpers::GetMoverTrackMatrixDelta(*pClip, 0.f, 1.f, RC_MATRIX34(introAnimTransformation));

		// Compute the inverse of the transformation matrix of the intro animation.
		// This is the transformation we need to apply on the intro anim end matrix
		// to find the intro anim start matrix.
		Mat34V invIntroAnimTransformation;
		InvertTransformOrtho(invIntroAnimTransformation, introAnimTransformation);

		// Compute the position and orientation where we need to start the intro animation
		// in order to finish it at the desired point.
		Mat34V introAnimStartMtrx;
		Transform(introAnimStartMtrx, introAnimEndMtrx, invIntroAnimTransformation);

		// Store the position to start the animation at.
		pedPosition = VEC3V_TO_VECTOR3(introAnimStartMtrx.GetCol3());

		// Compute and store the rotation around the Z axis from the matrix,
		// in the same way as fwMatrixTransform::GetHeading().
		const float dirX = introAnimStartMtrx.GetM11f();
		const float dirY = introAnimStartMtrx.GetM10f();
		pedRotation = Atan2f(dirY, dirX);

		// Adjust the target position based on the height of the ped's capsule.
		const CPed* pPed = GetPed();
		Assert(pPed->GetCapsuleInfo());
		if (ShouldBeAttachedWhileUsing())
		{
			pedPosition.z -= pPed->GetCapsuleInfo()->GetGroundToRootOffset();
		}
	}
	else
	{
		aiAssertf(false,"Clip not found %s\n",PrintClipInfo(GetEnterClipSetId(GetPed(), this),GetChosenClipId(GetPed(), this)));
	}
}

float CTaskUseScenario::CalculateBaseHeadingOffset() const
{
	if (m_baseClipSetId == CLIP_SET_ID_INVALID)
		return 0.f;

	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(m_baseClipSetId);
	if (!pClipSet)
		return 0.f;

	if (pClipSet->GetClipItemCount() <= 0)
		return 0.f;

	float ClipOrientation = GetClipOrientationProperty(pClipSet->GetClip(GetChosenClipId(GetPed(), this)));
	return ClipOrientation != FLT_MAX ? ClipOrientation : 0.f;

	//return PI * 0.5f;
}

bool CTaskUseScenario::ConsiderOrientation(const CPed* pPed, const Vector3& vWorldPosition, float fScenarioHeading, fwMvClipSetId clipSetId, fwMvClipId& chosenClipId) const
{
	if (m_bConsiderOrientation && clipSetId != CLIP_SET_ID_INVALID)
	{
		fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
		if (!pClipSet)
			return false;

		Vector3 DirToScenario = vWorldPosition - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		float fHeading = fwAngle::GetRadianAngleBetweenPoints(DirToScenario.x, DirToScenario.y, 0.0f, 0.0f);
		float AngleDiff = SubtractAngleShorter(fScenarioHeading, fHeading);

		int iBestClip = -1;
		float fBestOrientation = FLT_MAX;

		const int nClipItems = pClipSet->GetClipItemCount();
		taskAssertf(nClipItems > 0, "No clips in ConsiderOrientation clipset %s\n", clipSetId.GetCStr());
		for (int i = 0; i < nClipItems; ++i)
		{
			const crClip* pClip = pClipSet->GetClip(pClipSet->GetClipItemIdByIndex(i));
			float ClipOrientation = GetClipOrientationProperty(pClip);
			if (iBestClip < 0 || IsCloserToAngle(AngleDiff, ClipOrientation, fBestOrientation))
			{
				iBestClip = i;
				fBestOrientation = ClipOrientation;
			}
			
			taskAssertf(ClipOrientation != FLT_MAX, "ConsiderOrientation set but clip lack valid properties %s\n",PrintClipInfo(clipSetId, pClipSet->GetClipItemIdByIndex(i)));
		}

		taskAssertf(iBestClip != -1, "ConsiderOrientation couldn't find any suitable enter clip! %s\n", clipSetId.GetCStr());

		// So this should be the best one yes!
		chosenClipId = pClipSet->GetClipItemIdByIndex(iBestClip);
		return true;
	}

	return false;
}

float CTaskUseScenario::GetClipOrientationProperty(const crClip* pClip) const
{
	if (pClip)
	{
		Vector3 OutPos(VEC3_ZERO);
		Quaternion OutRot = fwAnimHelpers::GetMoverTrackRotationDelta(*pClip, 0.0f, 1.0f);

		Vector3 EulerAngles(VEC3_ZERO);
		OutRot.ToEulers(EulerAngles);

		return EulerAngles.z;
	}

	return FLT_MAX;
}

// PURPOSE:  Modify vWorldPosition and fWorldHeading to account for the entrypoint of the closest matching entryclip.
// The best matching clip is the one with the closest start orientation to that of the ped.
// Returns true if the target is valid.
bool CTaskUseScenario::AdjustTargetForClips(Vector3& vWorldPosition, float& fWorldHeading)
{
	if (GetEnterClipSetId(GetPed(), this) != CLIP_SET_ID_INVALID)
	{
		if (AreEnterClipsStreamed() &&
			(m_baseClipSetId == CLIP_SET_ID_INVALID || AreBaseClipsStreamed()))
		{
			fwMvClipId chosenClipId = CLIP_ID_INVALID;
			if(ConsiderOrientation(GetPed(), vWorldPosition, fWorldHeading, GetEnterClipSetId(GetPed(), this), chosenClipId))
			{
				SetChosenClipId(chosenClipId, GetPed(), this);
			}

			m_fBaseHeadingOffset = CalculateBaseHeadingOffset();
			fWorldHeading += m_fBaseHeadingOffset;
			CalculateStartPosition(vWorldPosition, fWorldHeading);
			return true;
		}
		else
		{
			// Invalid target adjustment if the clips are not streamed in yet.
			return false;
		}
	}
	else
	{
		// Valid target adjustment if there are no entry clips.
		return true;
	}
}

void CTaskUseScenario::AdjustGotoPositionForPedHeight(Vector3& vWorldPosition)
{
	float fWorldHeading;

	const Tunables& tune = GetTunables();

	GetScenarioPositionAndHeading(vWorldPosition, fWorldHeading);

	CPed* pPed = GetPed();
	Vec3V vPed = pPed->GetTransform().GetPosition();

	Assert(pPed->GetCapsuleInfo());

	if (fabs(vWorldPosition.z - vPed.GetZf()) <= tune.m_ZThresholdForApproachOffset)
	{

		//Ensure the task is valid.
		const CTaskMoveFollowNavMesh* pTask = static_cast<CTaskMoveFollowNavMesh *>(pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
		if(pTask && pTask->IsFollowingNavMeshRoute())
		{
			// Check the route length.
			if (pTask->IsTotalRouteLongerThan(GetTunables().m_RouteLengthThresholdForApproachOffset))
			{
				vWorldPosition.z = vPed.GetZf() - pPed->GetCapsuleInfo()->GetGroundToRootOffset();
			}
		}
	}
}

CTask::FSM_Return CTaskUseScenario::GoToStartPosition_OnUpdate()
{
	CPed* pPed = GetPed();

	CScenarioPoint* pScenarioPoint = GetScenarioPoint();
	if (pScenarioPoint)
	{
		// If we haven't checked for obstructions and we are close enough to the point
		if (!m_bCheckedScenarioPointForObstructions && CloseEnoughForFinalApproach())
		{
			// check if the point is obstructed
			if (IsScenarioPointObstructed(pScenarioPoint))
			{
				// if we are not obstructed, mark the point as obstructed unless this is a player ped.
				if (!pScenarioPoint->IsObstructedAndReset() && !pPed->IsAPlayerPed()) 
				{
					pScenarioPoint->SetObstructed();
				}
				// quit
				return FSM_Quit;
			}
			m_bCheckedScenarioPointForObstructions = true;
		}

		// enable searching for doors while moving to the scenario point
		if (pScenarioPoint->GetOpenDoor())
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_SearchingForDoors, true);
		}
	}

	if (pPed->IsNetworkClone())
	{
		if(GetStateFromNetwork()>State_GoToStartPosition)
		{
			// This is normally done from ProcessPreFSM(), but I suspect that we might need to do it here too.
			// The situation where it may be needed is this:
			// 1. On the clone side (here), the entry animation set is already resident.
			// 2. The clone task starts to run and hits Start_OnEnter(), which sets m_enterClipSetId, and on the
			//    same frame, goes to GoToStartPosition.
			// 3. In the remote owner task, the task may already be in State_Enter, so we
			//    proceed to State_Enter here as well - all in the same frame. This would not be the normal
			//    case but could perhaps happen if network messages are delayed.
			// 4. In Intro_OnEnter(), asserts fail because m_enterClipSetId has been set to a valid set
			//    which is streamed in, but m_ChosenClipId is invalid because ProcessPreFSM() didn't run after
			//    m_enterClipSetId was set.
			// This is my current theory for the cause of B* 384004: "Assert: clipsets.cpp(202): Error: clipId != CLIP_ID_INVALID: clipId is invalid!".
			// Note: with the AreEnterClipsStremed() condition below, is probably not strictly necessary, we would
			// just have to wait for another frame for ProcessPreFSM() to run otherwise.
			PickEntryClipIfNeeded();

			// Probably shouldn't enter State_Enter unless we have streamed in the intro animations
			// (or they don't exist). Otherwise, we could probably end up with misalignment, if the intro
			// animations don't play before the base.
			if(GetEnterClipSetId(GetPed(), this) == CLIP_SET_ID_INVALID || AreEnterClipsStreamed())
			{
				SetState(State_Enter);
			}
		}
		return FSM_Continue;
	}

	Vector3 vWorldPosition;
	float fWorldHeading;
	GetScenarioPositionAndHeading(vWorldPosition, fWorldHeading);

	// If the goto has not started, or it has started but the position isn't final (because the clip hasn't streamed in)
	// then try and stream in the clips
	if (!m_iFlags.IsFlagSet(SF_GotoPositionTaskSet))
	{
		//Vector3 ScenarioDir(0.f, 0.f, 0.f);
		//ScenarioDir.x = ANGLE_TO_VECTOR_X( ( RtoD *  fWorldHeading ) );
		//ScenarioDir.y = ANGLE_TO_VECTOR_Y( ( RtoD *  fWorldHeading ) );

		//grcDebugDraw::Line(vWorldPosition, vWorldPosition + ZAXIS, Color_blue, Color_blue, 500);
		//grcDebugDraw::Line(vWorldPosition + ZAXIS, vWorldPosition + ScenarioDir + ZAXIS, Color_green, Color_green, 500);

		bool bCanAdjustTargetPoint = AdjustTargetForClips(vWorldPosition, fWorldHeading);

		if (bCanAdjustTargetPoint)
		{
			if (IsUsingAPropScenarioWithNoEntryAnimations())
			{
				// Set no collision on the scenario entity.  If we are moving to a prop scenario but have no entrypoints to head for,
				// the ped is going to navigate straight into the prop.  We will need to turn off collision for that case.
				CEntity* pEntity = GetScenarioEntity();
				if (pEntity)
				{
					GetPed()->SetNoCollision(pEntity, NO_COLLISION_NEEDS_RESET);
				}

				// Adjust the goto position.
				if (IsUsingAPropScenarioWithNoEntryAnimations()) 
				{
					AdjustGotoPositionForPedHeight(vWorldPosition);
				}
			}

			if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_GO_TO_SCENARIO)
			{
				CTaskGoToScenario* pGoToScenarioTask = static_cast<CTaskGoToScenario*>(GetSubTask());
				pGoToScenarioTask->SetNewTarget(vWorldPosition, fWorldHeading);
			}

			m_iFlags.SetFlag(SF_GotoPositionTaskSet);

			return FSM_Continue;
		}
		else if (m_pPropHelper)
		{
			m_pPropHelper->UpdateExistingOrMissingProp(*pPed);
			
			CPropManagementHelper::Context context(*pPed);
			context.m_ClipHelper = GetClipHelper();
			context.m_PropClipId = GetChosenClipId(GetPed(), this);
			context.m_PropClipSetId = GetEnterClipSetId(GetPed(), this);
			m_pPropHelper->Process(context);
		}
	} 
	else 
	{
		//Keep requesting enter animations so they don't stream out
		AreEnterClipsStreamed();

		// If our go to subtask failed then we need to quit
		aiTask* pSubTask = GetSubTask();
		if (pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_GO_TO_SCENARIO &&
			static_cast<CTaskGoToScenario*>(pSubTask)->GetHasFailed())
		{
			// if we are not a scenario that ignores obstructions and not the player mark this point as obstructed for future use.
			if (!GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::IgnoreObstructions) && !pPed->IsAPlayerPed())
			{
				// if our scenario point is valid
				CScenarioPoint* pPoint = GetScenarioPoint();
				if (pPoint)
				{
					// if we are not obstructed...
					if (!pPoint->IsObstructedAndReset()) 
					{
						pPoint->SetObstructed();
					}
				}
			}
			return FSM_Quit;
		}

		// If our subtask finished then move on to the intro clip
		// Or if we can start blend already to make it look more seemless, if the blendout kicked in maybe we should blend for a longer time
		if (GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !pSubTask ||
			(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_GO_TO_SCENARIO &&
			static_cast<CTaskGoToScenario*>(pSubTask)->CanBlendOut()))
		{
			// make sure our dictionary is loaded before moving on the the enter clip
			if ( GetEnterClipSetId(GetPed(), this) == CLIP_SET_ID_INVALID || (GetEnterClipSetId(GetPed(), this) != CLIP_SET_ID_INVALID && AreEnterClipsStreamed()) || (GetChosenClipId(GetPed(), this) == CLIP_ID_INVALID) )
			{
				// If there was no enter clip just warp to the point if using a prop scenario.
				// In this case there isn't much chance of nailing the target point because often there are navmesh issues/objects in the way
				// of the scenariopoint itself (see B* 666790).
				if (IsUsingAPropScenarioWithNoEntryAnimations())
				{
					CPed* pPed = GetPed();
					AI_LOG_WITH_ARGS("GoToStartPosition_OnUpdate - Ped is [%s], Attached to P: [%s] \n", AILogging::GetDynamicEntityNameSafe(pPed), pPed->GetAttachParent() ? pPed->GetAttachParent()->GetDebugName() : "(None)");
					pPed->SetPosition(vWorldPosition);
					pPed->SetHeading(fWorldHeading);
					pPed->SetDesiredHeading(fWorldHeading);
				}

				// The TaskAmbientClips could have changed these properties of the propHelper based on what it was playing during the GoToScenario. We need to reset them for
				//  whatever clip we're about to play. [6/22/2013 mdawe]
				if (m_pPropHelper && m_pConditionalAnimsGroup && m_ConditionalAnimsChosen >= 0 && m_ConditionalAnimsChosen < m_pConditionalAnimsGroup->GetNumAnims())
				{
					const CConditionalAnims* pAnims = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimsChosen);
					if (pAnims)
					{
						m_pPropHelper->SetCreatePropInLeftHand(pAnims->GetCreatePropInLeftHand());
						m_pPropHelper->SetDestroyProp(pAnims->ShouldDestroyPropInsteadOfDropping());
					} 
				}

				SetState(State_Enter);
				return FSM_Continue;
			}
		}

		if (!m_bFinalApproachSet && CloseEnoughForFinalApproach())
		{
			CTask* pSubtask = GetSubTask();
			if (pSubtask && pSubtask->GetTaskType() == CTaskTypes::TASK_GO_TO_SCENARIO)
			{
				// Now that the ped is closer, adjust the final target position to match the entrypoint with orientation.
				AdjustTargetForClips(vWorldPosition, fWorldHeading);

				if (IsUsingAPropScenarioWithNoEntryAnimations()) 
				{
					AdjustGotoPositionForPedHeight(vWorldPosition);
				}

				CTaskGoToScenario* pTaskGoto = static_cast<CTaskGoToScenario*>(pSubtask);
				pTaskGoto->SetNewTarget(vWorldPosition, fWorldHeading);
				m_bFinalApproachSet = true;
			}
		}
	}
	return FSM_Continue;
}

bool CTaskUseScenario::IsScenarioPointObstructed(const CScenarioPoint* pScenarioPoint) const
{
	taskAssert(pScenarioPoint);

	// if we are a scenario that ignores obstructions
	if (GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::IgnoreObstructions))
	{
		return false;
	}

	// Has the scenario point been flagged as obstructed?
	if (pScenarioPoint->IsObstructed())
	{
		return true;
	}

	// Make sure that a ped can use the scenario point
	const Vec3V		vRootPos				= pScenarioPoint->GetPosition();// Use the scenario point's position
	const bool		useBoundingBox			= true;							// Use bounding box for collision check
	const float		pedRadius				= CPedModelInfo::ms_fPedRadius;	// Use the PedRadius
	const bool		doVehicleCollisionTest	= true;							// check for nearby vehicle collision
	const bool		doPedCollisionTest		= false;						// check for nearby ped collision
	const bool		doObjectCollisionTest	= true;							// check for nearby object collision
	const CEntity*	pExcludedEntity1		= pScenarioPoint->GetEntity();	// Exclude the entity the point is attached to, if any
	const CEntity*	pExcludedEntity2		= GetPropInEnvironmentToUse();	// Exclude our prop in environment, if any

	// Check if this position collides with any other objects or cars.
	static const int maxObjects = 4;
	CEntity* pCollidingObjects[maxObjects];
	for(s32 i =0; i < maxObjects; ++i)
	{
		pCollidingObjects[i] = NULL;
	}
	if(!CPedPlacement::IsPositionClearForPed(VEC3V_TO_VECTOR3(vRootPos), pedRadius, useBoundingBox, maxObjects, pCollidingObjects, doVehicleCollisionTest, doPedCollisionTest, doObjectCollisionTest))
	{
		// Determine if we have collided with anything
		for(s32 i =0; i < maxObjects; ++i)
		{
			// Make sure the collision exists.
			if(!pCollidingObjects[i])
			{
				continue;
			}

			// Make sure the collision is with something other than the than the excluded entities.
			if(pCollidingObjects[i] == pExcludedEntity1 || pCollidingObjects[i] == pExcludedEntity2)
			{
				continue;
			}

			// Make sure the collision is with an uprooted object.
			if(pCollidingObjects[i]->GetIsTypeObject())
			{
				CObject* pObject = static_cast<CObject*>(pCollidingObjects[i]);
				if(pObject)
				{
					// Check if the object has been uprooted.
					if(!pObject->m_nObjectFlags.bHasBeenUprooted)
					{
						continue;
					}
					
					// Check if the object is an ambient prop.
					if(pObject->m_nObjectFlags.bAmbientProp)
					{
						continue;
					}
				}
			}
			// Make sure the collision is not with a parked car.
			else if(pCollidingObjects[i]->GetIsTypeVehicle())
			{
				CVehicle* pVehicle = static_cast<CVehicle*>((pCollidingObjects[i]));
				if(pVehicle && pVehicle->PopTypeGet() == POPTYPE_RANDOM_PARKED)
				{
					continue;
				}
			}

			// A collision has occurred.
			return true;
		}
	}
	// no collision detected
	return false;
}

// Return true if the ped is close enough to the scenario point to make a final approach towards an entrypoint.
bool CTaskUseScenario::CloseEnoughForFinalApproach() const
{
	//Ensure the task is valid.
	const CTaskMoveFollowNavMesh* pTask = static_cast<CTaskMoveFollowNavMesh *>(GetPed()->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
	if(!pTask)
	{
		return false;
	}

	//Ensure we are following the route.
	if(!pTask->IsFollowingNavMeshRoute())
	{
		return false;
	}

	//Check if the route is short enough.
	if(pTask->IsTotalRouteLongerThan(GetTunables().m_RouteLengthThresholdForFinalApproach))
	{
		//Route is too long, cannot repath yet.
		return false;
	}
	else
	{
		//Route is small enough to repath towards the best entry animation.
		return true;
	}
}

// Helper function to determine if the ped is using a prop scenario with no entry animations (a bad situation to be in).
bool CTaskUseScenario::IsUsingAPropScenarioWithNoEntryAnimations() const
{
	return GetEnterClipSetId(GetPed(), this) == CLIP_SET_ID_INVALID && ShouldBeAttachedWhileUsing();
}


void CTaskUseScenario::GoToStartPosition_OnExit()
{
	// Turn off our arm IK flag
	GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_OpenDoorArmIK, false);
}


void CTaskUseScenario::Enter_OnEnter()
{
	//Set the last used scenario point.
	SetLastUsedScenarioPoint();

	DisposeOfPropIfInappropriate();

	// If we are not going to be attached, we need to make sure we end up at a good position
	// relative to the ground and other obstructions, so we don't float or get into leg IK issues.
	// The ped may have moved up to this scenario in low LOD mode, and if there is no movement
	// in the enter animation, probably wouldn't otherwise activate if switching to high physics LOD.
	if(!ShouldBeAttachedWhileUsing())
	{
		GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_ActivateOnSwitchFromLowPhysicsLod, true);
	}

	// if we have a prop helper
	if (m_pPropHelper)
	{
		// Request move signal calls
		RequestProcessMoveSignalCalls();
	}
}

CTask::FSM_Return	CTaskUseScenario::Enter_OnUpdate()
{

	CPed* pPed = GetPed();
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostMovementTimeSliced, true);

	if (!AttemptToPlayEnterClip())
	{
		// If we still haven't played our Enter yet, return so we can try again next frame. [3/18/2013 mdawe]
		return FSM_Continue;
	}

	const CEntity* pScenarioEntity = GetScenarioEntity();
	if (pScenarioEntity && pScenarioEntity->GetTransform().GetC().GetZf() < GetTunables().m_BreakAttachmentOrientationThreshold)
	{
		// We usually would ragdoll here, but it probably looks better just to blend out of the enter at this point,
		// since presumably any chair tipping would occur early on in the animation's progress.
		return FSM_Quit;
	}

	if (m_pPropHelper)
	{
		m_pPropHelper->UpdateExistingOrMissingProp(*pPed);
	}

	// if our clip is finished play our idles
	if (!GetClipHelper() || (GetClipHelper() && GetClipHelper()->IsHeldAtEnd()))
	{
		// ProcessPostMovement() wants to be attached when playing intro animations, but we don't necessarily
		// want to be attached after the intro animation is over (i.e. now), so we may have to explicitly
		// detach. Going through ShouldBeAttachedWhileUsing() ensures that we are attached in the same cases
		// as we would have been attached if we had warped in.
		if(!ShouldBeAttachedWhileUsing())
		{
			GetPed()->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
		}

		
		if (m_fIdleTime >= 0.0f)
		{
			// Subtract how long the ped has been in the intro state from the idle timer.
			// This is done to count the time spent playing the intro animation. 
			m_fIdleTime = Max(0.0f, m_fIdleTime - GetTimeInState());
		}
		
#if SCENARIO_DEBUG
		if (m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData))
		{
			//Based on what we want to do next we choose the proper action ... 
			// use of the flag SFD_WantNextDebugAnimData is what lets us set 
			// the next thing we want to debug with ... 
			switch(m_pDebugInfo->m_AnimType)
			{
			case CConditionalAnims::AT_BASE:
			case CConditionalAnims::AT_VARIATION:
			case CConditionalAnims::AT_REACTION:
				SetState(State_PlayAmbients);
				break;
			case CConditionalAnims::AT_COUNT:
				SetState(State_Finish);//no next data so exit ... 
				break;
			default:
				SetState(State_Start);//go to start 
				break;
			};
		}
		else
#endif // SCENARIO_DEBUG
		{
			SetState(State_PlayAmbients);
		}
	}

	if (m_pPropHelper)
	{
		CPropManagementHelper::Context context(*pPed);
		context.m_ClipHelper = GetClipHelper();
		context.m_PropClipId = GetChosenClipId(GetPed(), this);
		context.m_PropClipSetId = GetEnterClipSetId(GetPed(), this);
		m_pPropHelper->Process(context);
	}

	return FSM_Continue;
}

void CTaskUseScenario::PlayAmbients_OnEnter()
{
	// CPED_CONFIG_FLAG_ActivateOnSwitchFromLowPhysicsLod is already set when warping, and when
	// entering the Enter state. However, they could probably have moved during the Enter state,
	// switching back to low LOD, and then back to high LOD once in the PlayAmbients state, so
	// we set it again to make sure we don't end up misaligned.
	if(!m_iFlags.IsFlagSet(SF_Warp))
	{
		if(!ShouldBeAttachedWhileUsing())
		{
			GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_ActivateOnSwitchFromLowPhysicsLod, true);
		}
	}

	//Set the last used scenario point.
	SetLastUsedScenarioPoint();

	DisposeOfPropIfInappropriate();

	u32 flags = GetAmbientFlags();

	// if we played an enter animation let the subtask know
	if (HasPlayedEnterAnimation())
	{
		flags |= CTaskAmbientClips::Flag_DontRandomizeBaseOnStartup;
	}

#if SCENARIO_DEBUG
		if (m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData))
	{
		flags |= CTaskAmbientClips::Flag_UseDebugAnimData;
	}
#endif // SCENARIO_DEBUG

	taskAssertf(m_pConditionalAnimsGroup!=NULL,
				"m_pConditionalAnimsGroup==NULL, m_eScenarioMode %d, m_iFlags %d, m_ConditionalAnimsChosen %d, m_iScenarioIndex %d",
				m_eScenarioMode,m_iFlags.GetAllFlags(),m_ConditionalAnimsChosen,m_iScenarioIndex);

	CTaskAmbientClips* pTask = rage_new CTaskAmbientClips(flags, m_pConditionalAnimsGroup, m_ConditionalAnimsChosen, m_pPropHelper);

#if SCENARIO_DEBUG
	if (m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData))
	{
		if (m_pDebugInfo->m_AnimType == CConditionalAnims::AT_BASE || 
			m_pDebugInfo->m_AnimType == CConditionalAnims::AT_VARIATION ||
			m_pDebugInfo->m_AnimType == CConditionalAnims::AT_REACTION
			)
		{
			pTask->SetOverrideAnimData(*m_pDebugInfo);

			//Ready for my next set of data ... 
			m_iDebugFlags.SetFlag(SFD_WantNextDebugAnimData);
		}
	}
#endif // SCENARIO_DEBUG

	pTask->SetSynchronizedPartner(m_SynchronizedPartner);

	// Create a synchronized scene for CTaskAmbientClips to use (for the base clip).
	// The main reason for why this is done here and not inside CTaskAmbientClips is
	// that CTaskAmbientClips wouldn't know where to put the origin (i.e. the scenario point).
	if(CreateSynchronizedSceneForScenario())
	{
		fwAnimDirectorComponentSyncedScene::SetSyncedSceneLooped(m_SyncedSceneId, true);
		pTask->SetExternalSyncedSceneId(m_SyncedSceneId);
	}

	//if we have overriden the base clipset and clip ... 
	if (m_OverriddenBaseClipSet.GetHash() && m_OverriddenBaseClip.GetHash())
	{
		pTask->OverrideBaseClip(m_OverriddenBaseClipSet, m_OverriddenBaseClip);
	}

	SetNewTask(pTask);

	HandleOnEnterAmbientScenarioInfoFlags();

	SetUsingScenario(true);

	// Check if sitting in an object that needs fixed physics.
	ToggleFixedPhysicsForScenarioEntity(true);

	// Start ignoring the walked into event.
	GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_DisablePotentialToBeWalkedIntoResponse, true);

	// Start the countdown to offset the bounds for the ped.
	// Seems to be the case that if the ped warps to the scenario it requires a delay before shifting the bounds.
	m_iShiftedBoundsCountdown = m_iFlags.IsFlagSet(SF_Warp) ? GetTunables().m_UpdatesBeforeShiftingBounds : 0;


	m_fLastPavementCheckTimer = 0.f;
	m_PavementHelper.CleanupPavementFloodFillRequest();

	// start streaming our transition animations, if applicable set whether or not we need to play a transition animation
	const CScenarioTransitionInfo* pTransitionInfo = GetScenarioTransitionInfoFromConditionalAnims();
	if (pTransitionInfo)
	{
		if (UpdateConditionalAnimsGroupAndChosenFromTransitionInfo(pTransitionInfo))
		{
			SetupForScenarioTransition(pTransitionInfo);
		}
	}

	// reset our transition requested flag
	m_bDefaultTransitionRequested = false;

	// Figure out if this is a special scenario that allows the ped to quit in MP if their scenario entity is destroyed.
	if (NetworkInterface::IsGameInProgress() && !GetPed()->IsNetworkClone())
	{
		CEntity* pEntity = GetScenarioEntity();
		if (pEntity)
		{
			s32 iIplIndex = pEntity->GetIplIndex();
			if (iIplIndex > 0)
			{
				fwMapDataDef* pDef = fwMapDataStore::GetStore().GetSlot(strLocalIndex(iIplIndex));
				if (pDef && pDef->GetIsCodeManaged())
				{
					// B*1965200 - Code managed Ipls can be destroyed suddenly due to changing weather/time conditions,
					// so we must permit the scenario ped to be destroyed when this happens in this special case.
					// There's a chance of something like B*1627729 coming back, but that's the lesser of two evils here.
					// I don't think it could happen in this particular case, as hopefully by the time the ped gets to state PlayAmbients
					// the entity has successfully transferred to the locally owned ped.
					m_bAllowDestructionEvenInMPForMissingEntity = true;
				}
			}
		}
	}
}

// PURPOSE:  Check and process ScenarioInfoFlags that set properties of the ped when PlayAmbients_OnEnter() runs.
void CTaskUseScenario::HandleOnEnterAmbientScenarioInfoFlags()
{
	const CScenarioInfo& rInfo = GetScenarioInfo();
	CPed* pPed = GetPed();

	// Make sure we ignore low priority shocking events if the scenario info says we should.
	// This may not have been done yet if we skipped the intro animation.
	SetIgnoreLowPriShockingEventsFromScenarioInfo(rInfo);

	// Similar thing with stationary reactions:
	SetStationaryReactionsFromScenarioPoint(rInfo);

	// Check if this is a seated scenario.
	if (rInfo.GetIsFlagSet(CScenarioInfoFlags::SeatedScenario))
	{
		pPed->SetIsSitting(true);
	}

	// Set the combat behaviour flag for radios if necessary.
	if (rInfo.GetIsFlagSet(CScenarioInfoFlags::ScenarioGivesCombatRadio))
	{
		pPed->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_HasRadio);
	}

	// Check if this scenario blocks paths.
	if ( rInfo.GetIsFlagSet(CScenarioInfoFlags::NavmeshBlocking) )
	{
		Vector3 vPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		Vector3 vSize = (pPed->GetBoundingBoxMax() - pPed->GetBoundingBoxMin())/2;
		vSize.x = Max(0.0f, vSize.x - 0.5f);	// MAGIC! Reduce bounds slightly to reduce instances of sidewalks blocked for wandering peds
		vSize.y = Max(0.0f, vSize.y - 0.5f);
		float heading = pPed->GetTransform().GetHeading();
		m_TDynamicObjectIndex = CPathServerGta::AddBlockingObject(vPos, vSize, heading, TDynamicObject::BLOCKINGOBJECT_ALL_PATH_TYPES);
	}

	// Allow the ped to activate their ragdoll more easily than normal.
	if (rInfo.GetIsFlagSet(CScenarioInfoFlags::RagdollEasilyFromPlayerContact))
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ActivateRagdollFromMinorPlayerContact, true);
	}
}


CTask::FSM_Return CTaskUseScenario::PlayAmbients_OnUpdate()
{

	if(GetPed() && GetPed()->IsNetworkClone())
	{
		if(IsDynamic() && GetPed()->GetAttachParent()==NULL)
		{
			//If task has dynamic object but clone is not attached then restart task to correct this B* 848122
			m_ConditionalAnimsChosen = -1;
			m_iFlags.SetFlag(SF_Warp | SF_SkipEnterClip); //make sure ped goes straight to position
			SetState(State_Start);
			return FSM_Continue;
		}
	}

	// Update the bounds shift countdown.  Don't bother counting down unless the ped is in high lod physics mode because the offset won't catch.
	if (m_iShiftedBoundsCountdown >= 0 && !GetPed()->IsUsingLowLodPhysics())
	{
		// Shift the bounds.
		if (m_iShiftedBoundsCountdown == 0)
		{
			ManagePedBoundForScenario(true);
		}
		m_iShiftedBoundsCountdown--;
	}

#if SCENARIO_DEBUG
	static const float sf_EnterBlendTimeout = 10.0f;

	if (!m_bCheckedEnterAnim)
	{
		if (GetTimeInState() > sf_EnterBlendTimeout)
		{
			m_bCheckedEnterAnim = true;
			if (HasPlayedEnterAnimation() && GetClipHelper() && GetClipHelper()->GetPhase() == 1.0f && GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_AMBIENT_CLIPS)
			{
#if __ASSERT
				CTaskAmbientClips* pTaskAmbientClips = static_cast<CTaskAmbientClips*>(GetSubTask());
				Assertf(pTaskAmbientClips->GetConditionalAnimChosen() >= 0, "The enter clip for scenario %s did not blend out.  This likely means the enter clips were not tagged to create props.  Conditional anims group is %s .", 
					GetScenarioInfo().GetName(),  m_pConditionalAnimsGroup && m_ConditionalAnimsChosen >= 0 ? m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimsChosen)->GetName() : "NULL");
#endif
			}
		}
	}


	if (m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData))
	{
		//Ambient clips is gone so finished playing it stuff ... 
		CTaskAmbientClips* pAmbientTask = static_cast<CTaskAmbientClips*>(FindSubTaskOfType(CTaskTypes::TASK_AMBIENT_CLIPS));
		if(	!pAmbientTask || 
			(
				(pAmbientTask && !pAmbientTask->GetIsPlayingAmbientClip()) &&
				(pAmbientTask && pAmbientTask->IsIdleFinished()) &&
				(pAmbientTask && pAmbientTask->GetBaseClip() && !pAmbientTask->GetBaseClip()->IsPlayingClip())
			)
		  )
		{
			//let us move on... 
			if (m_pDebugInfo->m_AnimType == CConditionalAnims::AT_EXIT)
			{
				m_iFlags.SetFlag(SF_ShouldAbort);

				//NOTE: setting the clipset here because this data is set in Start_onenter, but 
				// may be different coming from the debug anim queue so override it here with the data
				// we want to use ... the clip should be set fine through use of LeaveScenario()
				m_exitClipSetId = m_pDebugInfo->m_ClipSetId;
				m_iExitClipsFlags.ChangeFlag(eExitClipsDetermined, true);
			}
			else 
			{
				//Else if we have no valid anims to play we should just exit ... 
				// NOTE: debug type AT_ENTER is evaluated as a new task ... so if that is what we are attempting to do from here
				// then exit anyway cause the task will be created from our parent ... 
				SetState(State_Finish);
			}
		}
		else if (pAmbientTask->GetFlags().IsFlagSet(CTaskAmbientClips::Flag_WantNextDebugAnimData))
		{
			if (m_pDebugInfo->m_AnimType == CConditionalAnims::AT_BASE || 
				m_pDebugInfo->m_AnimType == CConditionalAnims::AT_VARIATION ||
				m_pDebugInfo->m_AnimType == CConditionalAnims::AT_REACTION
			   )
			{
				pAmbientTask->SetOverrideAnimData(*m_pDebugInfo);
				//Ready for my next set of data ... 
				m_iDebugFlags.SetFlag(SFD_WantNextDebugAnimData);
			}
			else
			{
				//reset the data if we dont have any next anim data.
				pAmbientTask->SetOverrideAnimData(ScenarioAnimDebugInfo());
			}
		}
	}
#endif // SCENARIO_DEBUG
	if(GetPed() && GetPed()->IsNetworkClone() && GetStateFromNetwork()==State_Exit)
	{
		LeaveScenario(true);
		return FSM_Continue;
	}

	if ( CheckAttachStatus() )
	{
		if (RagdollOutOfScenario())
		{
			return FSM_Quit;
		}
	}

	if (m_iScenarioReactionFlags.IsFlagSet(SREF_RespondingToEvent) || m_iScenarioReactionFlags.IsFlagSet(SREF_AggroOrCowardAgitationReaction) || 
		m_iScenarioReactionFlags.IsFlagSet(SREF_ScriptedCowerInPlace))
	{
		HandlePanicSetup();
		return FSM_Continue;
	}

	if (m_iFlags.IsFlagSet(SF_ExitingHangoutConversation))
	{
		LeaveScenario();
	}

	// If we are currently playing an clip (our enter)
	if(GetClipHelper())
	{
		// Check if the base clip of our ambient task exists yet and stop our local clip
		CTaskAmbientClips* pAmbientTask = static_cast<CTaskAmbientClips*>(FindSubTaskOfType(CTaskTypes::TASK_AMBIENT_CLIPS));
		if(pAmbientTask && pAmbientTask->IsBaseClipPlaying())
		{
			StopClip();
		}
	}

	bool bConditionsMet = true;
	if(m_iFlags.IsFlagSet(SF_CheckConditions))
	{
		// Look at the point and determine if the scenario ped should never leave.
		CScenarioPoint* pPoint = GetScenarioPoint();
		bool bNeverLeave = (pPoint && pPoint->GetTimeTillPedLeaves() < 0.0f);

		CScenarioCondition::sScenarioConditionData conditionData;
		InitConditions(conditionData);

		//Check other conditions here that are on the point itself and not known about 
		// by the scenario info or are overridden from the scenario info default.
		// If the ped is set to NeverLeave, don't check the time.
		if (pPoint && pPoint->HasTimeOverride())
		{
			int hour = pPoint->GetTimeEndOverride();

			//Add a bit of randomness to time check ... 
			if (GetPed())
			{
				// In this case, we also need the minutes and seconds from CClock.
				int minute = CClock::GetMinute();
				int seconds = CClock::GetSecond();

				// Make sure the input hour is in the 0-24 range that we expect.
				Assert(hour >= 0 && hour <= 24);

				// Pick a threshold based on how far the time is into the next hour.
				float fFraction = (1.0f/60.0f)*(float)minute + (1.0f/3600.0f)*(float)seconds;

				// Use a ped-specific random number generator to figure out how late this ped is willing to stay.
				mthRandom rnd(GetPed()->GetRandomSeed() + 123 /* MAGIC! */);
				const float change = rnd.GetFloat()*CTaskUseScenario::GetTunables().m_TimeOfDayRandomnessHours;

				// Check to see if the time should be extended.
				if (fFraction < change)
				{
					// Stay within [0, 24].
					if (hour < 24)
						hour++;

				}

				Assert(hour >= 0 && hour <= 24);
			}
			
			if(!CClock::IsTimeInRange(pPoint->GetTimeStartOverride(), hour) ) 
			{ 
				if(bNeverLeave) 
				{ 
					GetPed()->SetRemoveAsSoonAsPossible(true); 
				} 
				else 
				{ 
					bConditionsMet = false; 
				} 
			}
			
			if (!bNeverLeave)
			{
				conditionData.iScenarioFlags |= ::SF_IgnoreTime;
			}	
		}

		if(bConditionsMet)
		{
			const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(m_iScenarioIndex);
			if (pScenarioInfo)
			{
				const CScenarioConditionWorld* pCondition = pScenarioInfo->GetCondition();
				if (pCondition)
				{
					// Apply ped-specific randomness on time of day conditions.
					conditionData.iScenarioFlags |= ::SF_AddPedRandomTimeOfDayDelay;

					if (pPoint && pPoint->IsFlagSet(CScenarioPointFlags::IgnoreWeatherRestrictions))
					{
						conditionData.iScenarioFlags |= ::SF_IgnoreWeather;
					}

					ASSERT_ONLY(conditionData.m_ScenarioName = pScenarioInfo->GetName();)

					conditionData.m_bCheckUntilFailedWeatherCondition = true;
					bConditionsMet = pCondition->Check(conditionData);

					if (!bConditionsMet && conditionData.GetFailedWeatherCondition() && pCondition->IsWeatherCondition())
					{
						Vector3 vPedPosition = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
						CPopZone * pPopZone = CPopZones::FindSmallestForPosition(&vPedPosition, ZONECAT_ANY, ZONETYPE_ANY);
						if (pPopZone)
						{
							if (pPopZone->m_bAllowScenarioWeatherExits)
							{
								// Allow this ped to exit the scenario even if there's no pavement around if it's started raining or something.
								m_iFlags.SetFlag(SF_CanExitForNearbyPavement);
							}
							else
							{
								// mark this ped to be removed as soon as possible.
								GetPed()->SetRemoveAsSoonAsPossible(true);
							}
						}
						else
						{
							// If we fail to find a popzone, allow this by default.
							// Allow this ped to exit the scenario even if there's no pavement around if it's started raining or something.
							m_iFlags.SetFlag(SF_CanExitForNearbyPavement);
						}
					}
					else if (!bConditionsMet && bNeverLeave)
					{
						// Weather conditions are the only ones we'll let a ped leave the point for. Any other, respect the NeverLeave flag.
						bConditionsMet = true;
					}
				}
			}
		}
		// If the previously checked conditions are all fulfilled, check if any IMAP that
		// we need are still loaded.
		if(bConditionsMet && pPoint)
		{
			if(!CScenarioManager::CheckScenarioPointMapData(*pPoint))
			{
				bConditionsMet = false;
			}
		}
	}

	// If we are a slave, there are some restrictions on when we are allowed to leave.
	if(m_iFlags.IsFlagSet(SF_SynchronizedSlave))
	{
		bool leave = true;
		bool stream = false;

		// Look up the master's task. If we can't find it, I guess we are free to go.
		const CTaskUseScenario* pMasterTask = FindSynchronizedMasterTask();
		if(pMasterTask)
		{
			// If the master has ever requested the exit clips to be streamed in, we will
			// start streaming them too.
			if(pMasterTask->m_iExitClipsFlags.IsFlagSet(eExitClipsRequested))
			{
				stream = true;
			}

			// Look at the master task's state to see if he/she decided to leave. If not, we can't leave either.
			const int masterState = pMasterTask->GetState();
			if(masterState == State_Exit || masterState == State_Finish)
			{
				leave = true;

				DetermineExitAnimations();

				// If the animations are to be synchronized, we may also need to make sure that
				// the leader has created the synchronized scene.
				if(m_iExitClipsFlags.IsFlagSet(eExitClipsAreSynchronized))
				{
					if(pMasterTask->m_SyncedSceneId == INVALID_SYNCED_SCENE_ID)
					{
						leave = false;
					}
				}

			}
			else
			{
				leave = false;
			}
		}

		if(leave)
		{
			LeaveScenario();
		}
		else if(stream)
		{
			DetermineExitAnimations();

			// Not ready to leave, but try to stream in the exit animations.
			if(m_exitClipSetId != CLIP_SET_ID_INVALID)
			{
				m_ClipSetRequestHelper.RequestClipSetIfExists(m_exitClipSetId);
				if (m_ClipSetRequestHelper.IsClipSetStreamedIn())
				{
					// The exit animations have been streamed in, tell the master that we are ready.
					m_bSlaveReadyForSyncedExit = true;
				}
			}
		}
		return FSM_Continue;
	}

	// if we have been here for our allotted time or we have been told to abort
	if(m_iFlags.IsFlagSet(SF_ShouldAbort))
	{
		LeaveScenario();
	}
	else
	{
		bool timedOut = (m_fIdleTime >= 0.0f && GetTimeInState() > m_fIdleTime);
		if(!bConditionsMet)
		{
			// If the conditions aren't met, we should probably still not leave if somebody
			// else left very recently.
			if(ReadyToLeaveScenario(timedOut))
			{
				LeaveScenario();
			}
		}
		else if (timedOut)
		{
			// Check if the user has told us to not actually leave yet.
			if(m_iFlags.IsFlagSet(SF_DontLeaveThisFrame))
			{
				// Set this flag indicating to the user that we would have left
				// if SF_DontLeaveThisFrame had not been set.
				m_iFlags.SetFlag(SF_WouldHaveLeftLastFrame);
			}
			else
			{
				if (ReadyToLeaveScenario(true))
				{
					// If we leave due to the timer expiring (as opposed to being aborted),
					// disable the stationary reactions again, if they were set.
					if (!GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::DontAdjustStationaryReactionStatus))
					{
						SetStationaryReactionsEnabledForPed(*GetPed(), false);
					}
					LeaveScenario();
				}
			}
		}
	}

	CTaskAmbientClips* pAmbientTask = static_cast<CTaskAmbientClips*>(FindSubTaskOfType(CTaskTypes::TASK_AMBIENT_CLIPS));
	if (!pAmbientTask || pAmbientTask->GetIsFlagSet(aiTaskFlags::TerminationRequested))
	{
		// if we have transition info
		if (GetScenarioTransitionInfoFromConditionalAnims())
		{
			// make sure that the anims are streamed in
			if (AreEnterClipsStreamed())
			{
				SetState(State_ScenarioTransition);
			}
			return FSM_Continue;
		}	
		// we do not have transition info, but we do have an ambient task
		else if (pAmbientTask)
		{
			// setup transition anim info if needed
			if (!m_bDefaultTransitionRequested)
			{
				// cache our ambient task's animation info
				m_pConditionalAnimsGroup = pAmbientTask->GetConditionalAnimsGroup();
				m_ConditionalAnimsChosen = pAmbientTask->GetConditionalAnimChosen();

				// If we successfully request streaming anims
				if( SetupForScenarioTransition(NULL) )
				{
					// set our transition request flag
					m_bDefaultTransitionRequested = true;
				}
			}
		}

		// if default transition has been requested
		if (m_bDefaultTransitionRequested)
		{
			// make sure that our anims are streamed in
			if (AreEnterClipsStreamed())
			{
				SetState(State_ScenarioTransition);
			}

			// Wait for anims or make state transition
			return FSM_Continue;
		}

 		// At this point, we don't have transition info, and we don't have the needed info from the AmbientClips task
		// to go through a transition, so we should leave TaskUseScenario
		SetState(State_Finish);
		return FSM_Continue;
	}

	if(CPedAILodManager::IsTimesliceScenariosAggressivelyCurrentlyActive())
	{
		CPed& ped = *GetPed();
		if(GetState() == State_PlayAmbients	// A bit ugly, but in some code paths above we may have switched.
				&& ped.PopTypeIsRandom())
		{
			bool canUseTimeslicing = false;

			// At least for now, we only do this extra timeslicing if we are a human in a regular
			// standing still locomotion task. Known cases that we don't include here include animals
			// and humans in swimming scenarios. No known problems with them, just feels safer to
			// restrict it.
			const CTask* pMotionTask = ped.GetCurrentMotionTask(false);
			if(pMotionTask && pMotionTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION)
			{
				if(pMotionTask->GetState() == CTaskHumanLocomotion::State_Idle)
				{
					canUseTimeslicing = true;
				}
			}

			if(canUseTimeslicing)
			{
				CPedAILod& lod = ped.GetPedAiLod();
				lod.ClearBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
				lod.SetUnconditionalTimesliceIntelligenceUpdate(true);
			}
		}
	}
	return FSM_Continue;
}

// PURPOSE: Check to see if normal scenario exits have been globally disabled.
bool CTaskUseScenario::CanAnyScenarioPedsLeave() const
{
	return !CScenarioManager::AreScenarioExitsSuppressed();
}

bool CTaskUseScenario::IsTooSoonToLeaveAfterSomebodyElseLeft() const
{
#if SCENARIO_DEBUG
	//If this is in debug mode then never too soon to leave.
	if (m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData))
	{
		return false;
	}
#endif // SCENARIO_DEBUG
	return fwTimer::GetTimeInMilliseconds() < sm_NextTimeAnybodyCanGetUpMS;
}


// PURPOSE: Return true if the ped may exit due to timers - possibly they may want to finish their idle anim first.
bool CTaskUseScenario::ReadyToLeaveScenario(bool timedExit)
{
	bool preciseTiming = false;
	if(timedExit)
	{
		const CScenarioPoint* pt = GetScenarioPoint();
		preciseTiming = pt && pt->IsFlagSet(CScenarioPointFlags::PreciseUseTime);
	}

	// Respect scenario info overrides on precise use time.
	if (GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::PreciseUseTime))
	{
		preciseTiming = true;
	}

	if(!preciseTiming)
	{
		const CScenarioInfo *pInfo = CScenarioManager::GetScenarioInfo(GetScenarioType());
		// If no info or the flag was not set, then return true as there is nothing to stop an exit.
		if (pInfo && pInfo->GetIsFlagSet(CScenarioInfoFlags::DontTimeoutUntilIdleFinishes))
		{
			// Grab the ambient subtask (if one exists) - check if the idle is done and if so allow the ped to exit.
			CTaskAmbientClips* pAmbientTask = static_cast<CTaskAmbientClips*>(FindSubTaskOfType(CTaskTypes::TASK_AMBIENT_CLIPS));
			if (pAmbientTask && !pAmbientTask->IsIdleFinished())
			{
				return false;
			}
		}

		// If no info or the flag was not set, then return true as there is nothing to stop an exit.
		if (pInfo && pInfo->GetIsFlagSet(CScenarioInfoFlags::DontTimeoutUntilBaseFinishes))
		{
			// If the ambient subtask's base hasn't finished yet, don't allow it to exit.
			CTaskAmbientClips* pAmbientTask = static_cast<CTaskAmbientClips*>(FindSubTaskOfType(CTaskTypes::TASK_AMBIENT_CLIPS));
			if (pAmbientTask && pAmbientTask->GetBaseClip() && !(pAmbientTask->GetBaseClip()->GetPhase() + pAmbientTask->GetBaseClip()->GetPhaseUpdateAmount() * 2.0f > 1.0f))
			{
				return false;
			}
		}

		if (IsTooSoonToLeaveAfterSomebodyElseLeft())
		{
			return false;
		}
	}

	// Script can disable all scenario exits except those to points in the ped's chain.
	if (!m_iFlags.IsFlagSet(SF_CanExitToChain) && !CanAnyScenarioPedsLeave())
	{
		return false;
	}

	// Pavement checks.
	// Some peds (like animals) don't care about doing a pavement check when leaving scenarios.
	bool bCaresAboutPavement = BANK_ONLY(!m_bIgnorePavementCheckWhenLeavingScenario && )
		!GetPed()->GetTaskData().GetIsFlagSet(CTaskFlags::IgnorePavementCheckWhenLeavingScenarios) && !m_iFlags.IsFlagSet(SF_CanExitToChain);

	// Respect scenario info overrides.
	if (GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::IgnorePavementChecks))
	{
		bCaresAboutPavement = false;
	}

	if (bCaresAboutPavement && !HasNearbyPavement())
	{
		return false;
	}

	return true;
}


void CTaskUseScenario::SetTimeUntilNextLeave()
{
	// Compute the next time anybody is allowed to leave, which should be some short time into the future.
	// Note: we could add randomness here, but this is really just as a backup to other randomized mechanisms,
	// to prevent perfect synchronization of animations, so we may not need it.
	sm_NextTimeAnybodyCanGetUpMS = Max(sm_NextTimeAnybodyCanGetUpMS, fwTimer::GetTimeInMilliseconds() + (u32)(1000.0f*sm_Tunables.m_TimeToLeaveMinBetweenAnybody));
}


void CTaskUseScenario::PlayAmbients_OnExit()
{
	// Restore the ped capsule.
	if (m_iFlags.IsFlagSet(SF_CapsuleNeedsToBeRestored))
	{
		ManagePedBoundForScenario(false);
	}

	if ( m_TDynamicObjectIndex != DYNAMIC_OBJECT_INDEX_NONE )
	{
		CPathServerGta::RemoveBlockingObject(m_TDynamicObjectIndex);
		m_TDynamicObjectIndex = DYNAMIC_OBJECT_INDEX_NONE;
	}
	
	ReleaseSynchronizedScene();

	bool bClearFlagWarp = true;

	if(GetPed()->IsNetworkClone())
	{
		if(GetState()==State_Start)
		{
			bClearFlagWarp = false; //clone has been sent to State_Start again which requires warp flag to be kept
		}
	}

	if(bClearFlagWarp)
	{
		m_iFlags.ClearFlag(SF_Warp);
	}
	
	m_PavementHelper.CleanupPavementFloodFillRequest();
	m_PavementHelper.ResetSearchResults();
	m_iFlags.ClearFlag(SF_PerformedPavementFloodFill);
}

void CTaskUseScenario::ScenarioTransition_OnEnter()
{
	PlayScenarioTransitionClip();

	// if we do not have Transition Info, make sure that we are not looping our transition anim.
	if (m_bDefaultTransitionRequested)
	{
		if (GetClipHelper())
		{
			GetClipHelper()->UnsetFlag(APF_ISLOOPED);
		}
	}

#if __ASSERT

	if (!m_bDefaultTransitionRequested && GetClipHelper())
	{
		taskAssertf(!GetClipHelper()->IsFlagSet(APF_ISLOOPED), 
			"Transition info should never LOOP!\nScenarioType: %s\nGroupName: %s\n"
			, GetScenarioTransitionInfoFromConditionalAnims()->GetTransitionToScenarioConditionalAnims().GetCStr()
			, GetScenarioTransitionInfoFromConditionalAnims()->GetTransitionToScenarioType().GetCStr()
			);
	}

#endif // __ASSERT


	// if we have a prop helper
	if (m_pPropHelper)
	{
		// Request move signal calls
		RequestProcessMoveSignalCalls();
	}
}

CTask::FSM_Return CTaskUseScenario::ScenarioTransition_OnUpdate()
{
	// Cache ped handle
	CPed* pPed = GetPed();
	
	// Update prop helper if appropriate
	if (m_pPropHelper)
	{
		m_pPropHelper->UpdateExistingOrMissingProp(*pPed);

		CPropManagementHelper::Context context(*pPed);
		context.m_ClipHelper = GetClipHelper();
		context.m_PropClipId = GetChosenClipId(pPed, this);
		context.m_PropClipSetId = GetEnterClipSetId(pPed, this);
		m_pPropHelper->Process(context);
	}

	// if we finished playing our transition clip
	if ((!GetClipHelper() || (GetClipHelper() && GetClipHelper()->IsHeldAtEnd())))
	{
		if (m_fIdleTime >= 0.0f)
		{
			// Subtract how long the ped has been in the transition state from the idle timer.
			// This is done to count the time spent playing the transition animation. 
			m_fIdleTime = Max(0.0f, m_fIdleTime - GetTimeInState());
		}

		// Check for chain info on the transition scenario
		const CScenarioTransitionInfo* pTransitionInfo = GetScenarioTransitionInfoFromConditionalAnims();
		if (pTransitionInfo)
		{
			// Setup the next scenario from the chain info
			if (UpdateConditionalAnimsGroupAndChosenFromTransitionInfo(pTransitionInfo))
			{
				SetState(State_PlayAmbients);
			}
		}
		// if we do not have transition info and we have requested to transition into a new scenario
		else if (m_bDefaultTransitionRequested)
		{
			m_ConditionalAnimsChosen = -1;
			SetState(State_Start);
		}
		else
		{
			// we don't have a TransitionInfo when we should, and we have not been asked to transition. Bail out.
			Assertf(pTransitionInfo, "TransitionInfo for scenario does not lead to a follow up scenario and we were not asked to transition to a new scenario!");
			return FSM_Quit;
		}
	}

	return FSM_Continue;
}

const CScenarioTransitionInfo* CTaskUseScenario::GetScenarioTransitionInfoFromConditionalAnims() const
{
	// make sure we have valid anim info
	if (m_pConditionalAnimsGroup &&
		m_ConditionalAnimsChosen != -1 &&
		taskVerifyf(m_ConditionalAnimsChosen < m_pConditionalAnimsGroup->GetNumAnims(),
		"%s: m_ConditionalAnimsChosen %d exceeds Num anims %d in m_pConditionalAnimsGroup %s",
		GetPed()->GetDebugName(),m_ConditionalAnimsChosen,m_pConditionalAnimsGroup->GetNumAnims(), m_pConditionalAnimsGroup->GetName()))
	{
		// grab the currently used conditional anims
		const CConditionalAnims* pAnims = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimsChosen);
		if ( pAnims )
		{
			// grab the chain info
			const CScenarioTransitionInfo* pTransitionInfo = pAnims->GetScenarioTransitionInfo();
			
			// make sure the chain info is valid
			if (pTransitionInfo && pTransitionInfo->GetTransitionToScenarioType().IsNotNull() && pTransitionInfo->GetTransitionToScenarioConditionalAnims().IsNotNull())
			{
				return pTransitionInfo;
			}
		}
	}
	return NULL;
}

bool CTaskUseScenario::SetupForScenarioTransition(const CScenarioTransitionInfo* pTransitionInfo)
{
	// make sure that our m_pConditionalAnimsGroup and m_ConditionalAnimsChosen are valid
	if (m_pConditionalAnimsGroup && m_ConditionalAnimsChosen != -1)
	{
		// make sure that we have the right conditional anims
		const CConditionalAnims* pAnims = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimsChosen);
		if (pAnims)
		{
			// if we have transition info, make sure the anims' hash matches the one in the transition info
			if (!pTransitionInfo || 
				taskVerifyf(pAnims->GetNameHash() == pTransitionInfo->GetTransitionToScenarioConditionalAnims().GetHash(), "ConditionalAnims:%s does not match ChainInfo:%s", pAnims->GetName(), pTransitionInfo->GetTransitionToScenarioConditionalAnims().GetCStr())
				)
			{
				// find the clipset inside that matches our conditions
				CScenarioCondition::sScenarioConditionData conditionData;
				InitConditions(conditionData);
				const CConditionalClipSet* pChosenClipSet = CConditionalAnims::ChooseClipSet(*pAnims->GetClipSetArray(CConditionalAnims::AT_BASE), conditionData);
				if (pChosenClipSet)
				{
					// cache the ped
					CPed* pPed = GetPed();

					// set the EnterId for later use.
					fwMvClipSetId chosenClipSetId = fwMvClipSetId(pChosenClipSet->GetClipSetHash());
					SetEnterClipSetId(chosenClipSetId, pPed, this);

					// begin the animation request
					m_ClipSetRequestHelper.RequestClipSet(chosenClipSetId);
					m_ClipSetRequestHelper.SetStreamingPriorityOverride(SP_High);

					// find and set the related clip for later use
					fwMvClipId chosenClipId = CLIP_ID_INVALID;
					if (ChooseClipInClipSet(CConditionalAnims::AT_BASE, chosenClipSetId, chosenClipId) == CScenarioClipHelper::ClipCheck_Success)
					{
						SetChosenClipId(chosenClipId, pPed, this);
					}

					// clear warp flag if it exists and set the start in place flag
					m_iFlags.ClearFlag(SF_Warp);
					m_iFlags.SetFlag(SF_StartInCurrentPosition);

					// report streaming request initiated
					return true;
				}
			}
		}
	}

	// report no streaming requested
	return false;
}

void CTaskUseScenario::PlayScenarioTransitionClip()
{
	// cache the ped
	CPed* pPed = GetPed();
	// make sure the clips exist
	fwMvClipSetId enterClipSetId = GetEnterClipSetId(pPed, this);
	fwMvClipId chosenClipId = GetChosenClipId(pPed, this);
	if(fwAnimManager::GetClipIfExistsBySetId(enterClipSetId, chosenClipId))
	{
		// start clip playback
		StartClipBySetAndClip(pPed, enterClipSetId, chosenClipId, INSTANT_BLEND_IN_DELTA, CClipHelper::TerminationType_OnDelete);
	}
}

bool CTaskUseScenario::UpdateConditionalAnimsGroupAndChosenFromTransitionInfo(const CScenarioTransitionInfo* pTransitionInfo)
{
	// make sure we have chain info
	taskAssertf(pTransitionInfo, "UpdateConditionalAnimsGroupAndChosenFromTransitionInfo called with a NULL pTransitionInfo");
	if (!pTransitionInfo)
	{
		return false;
	}

	// grab the scenario info from the chain info
	s32 scenarioType = CScenarioManager::GetScenarioTypeFromHashKey(pTransitionInfo->GetTransitionToScenarioType().GetHash());
	const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(scenarioType);
	if (pScenarioInfo)
	{
		// set the conditional anims group according to the chain info's scenario info
		m_pConditionalAnimsGroup = pScenarioInfo->GetClipData();

		// search through our animsgroup to find the conditionalanim that matches the one specified in the CScenarioChainInfo.
		m_ConditionalAnimsChosen = -1;
		u32 numAnims = m_pConditionalAnimsGroup->GetNumAnims();
		for (u32 index = 0; index < numAnims; ++index)
		{
			const CConditionalAnims* pAnims = m_pConditionalAnimsGroup->GetAnims(index);
			if (pAnims->GetNameHash() == pTransitionInfo->GetTransitionToScenarioConditionalAnims().GetHash())
			{
				// Set the conditional anim according to the chain info
				m_ConditionalAnimsChosen = index;

				// everything succeeded.
				return true;
			}
		}
	}

	// report failure
	return false;
}

void CTaskUseScenario::Exit_OnEnter()
{
	DetermineExitAnimations();

	SetTimeUntilNextLeave();

	if (m_exitClipSetId != CLIP_SET_ID_INVALID)
	{
		m_ClipSetRequestHelper.RequestClipSetIfExists(m_exitClipSetId);
		if (m_ClipSetRequestHelper.IsClipSetStreamedIn())
		{
			CPed* pPed = GetPed();
			pPed->SetNoCollision(GetScenarioEntity(), NO_COLLISION_NEEDS_RESET);

			// If the Ped was using a scenario that grants extra money
			if (GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::ScenarioGrantsExtraCash))
			{
				// give the Ped extra money
				taskAssert(GetTunables().m_MinExtraMoney < GetTunables().m_MaxExtraMoney);
				pPed->SetMoneyCarried(pPed->GetMoneyCarried() + fwRandom::GetRandomNumberInRange(GetTunables().m_MinExtraMoney, GetTunables().m_MaxExtraMoney));
			}

			//Update the target situation.
			UpdateTargetSituation();

			//Set the initial offsets.
			m_PlayAttachedClipHelper.SetInitialOffsets(*pPed);

			if(fwAnimManager::GetClipIfExistsBySetId(m_exitClipSetId, GetChosenClipId(GetPed(), this)))
			{
				if(m_iExitClipsFlags.IsFlagSet(eExitClipsAreSynchronized))
				{
					// Probably best to blend instantly for now, otherwise, it seems like there may not
					// always be anything else to blend with.
					float blendInDelta = INSTANT_BLEND_IN_DELTA;

					// Tag-sync if we're going to end in a walk.
					bool tagSyncOut = m_iExitClipsFlags.IsFlagSet(eExitClipsEndsInWalk);

					StartPlayingSynchronized(m_exitClipSetId, GetChosenClipId(GetPed(), this), blendInDelta, tagSyncOut);
				}
				else
				{
					float fScenarioBlendDuration = fabs(GetScenarioInfo().GetOutroBlendInDuration());
					// Metadata stores it in seconds, StartClipBySetAndClip expects things to be 1 / seconds.
					fScenarioBlendDuration = fScenarioBlendDuration == 0.0f ? INSTANT_BLEND_IN_DELTA : 1.0f / fScenarioBlendDuration;

					int iClipPriority = GetExitClipPriority();
					StartClipBySetAndClip(pPed, m_exitClipSetId, GetChosenClipId(GetPed(), this), APF_ISBLENDAUTOREMOVE, 0, iClipPriority, fScenarioBlendDuration, CClipHelper::TerminationType_OnDelete);

					if (taskVerifyf(GetClipHelper(), "Failed to start scenario exit clip!"))
					{
						if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee) && m_iScenarioReactionFlags.IsFlagSet(SREF_RespondingToEvent))
						{
							RandomizeCowerPanicRateAndUpdateLastRate();
						}
						else
						{
							RandomizeRateAndUpdateLastRate();
							if (m_iScenarioReactionFlags.IsFlagSet(SREF_ExitingToCombat))
							{
								// When exiting to go into combat, speed up the rate some.
								static const float s_fCombatExitExtraRate = 0.15f;
								GetClipHelper()->SetRate(GetClipHelper()->GetRate() + s_fCombatExitExtraRate);
							}
						}
					}
				}
			}
			else
			{
				taskAssertf(false,"Scenario outro clip not found %s\n",PrintClipInfo(m_exitClipSetId,GetChosenClipId(GetPed(), this)));
			}
		}
	}

	// Let the ped push other peds around.
	TogglePedCapsuleCollidesWithInactives(true);

	// if we have a prop helper
	if (m_pPropHelper)
	{
		// Request move signal calls
		RequestProcessMoveSignalCalls();
	}
}

CTask::FSM_Return CTaskUseScenario::Exit_OnUpdate()
{
	CPed* pPed = GetPed();
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostMovementTimeSliced, true);

	// Do head IK if responding to an event.
	LookAtTarget(1.0f);

	if ( CheckAttachStatus() )
	{
		if (RagdollOutOfScenario())
		{
			return FSM_Quit;
		}
	}

	if (m_pPropHelper)
	{
		m_pPropHelper->UpdateExistingOrMissingProp(*pPed);
	}

	if (IsRespondingToEvent() || m_iScenarioReactionFlags.IsFlagSet(SREF_AggroOrCowardAgitationReaction))
	{
		KeepNavMeshTaskAlive();
	}

	bool done = false;

	taskAssertf(m_iExitClipsFlags.IsFlagSet(eExitClipsDetermined), "Exit clip flags not set.  Clone: %d, Scenario type:  %s", GetPed()->IsNetworkClone(), GetScenarioInfo().GetName());

	bool bFastExit = IsRespondingToEvent();
	bool bExitToWalk = !GetPed()->IsPlayer() && m_iExitClipsFlags.IsFlagSet(eExitClipsEndsInWalk);

	bool bFinishExitAnimationBeforeLeaving = GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::FinishExitAnimationBeforeLeaving);

	if(m_SyncedSceneId == INVALID_SYNCED_SCENE_ID)
	{
		if (!GetClipHelper() || !GetClipHelper()->GetClip())
		{
			done = true;
		}
		else if(!bFinishExitAnimationBeforeLeaving && bExitToWalk && GetClipHelper()->IsEvent(CClipEventTags::MoveEndsInWalk))
		{
			//Check if the 'ends in walk' event has been hit.
			done = true;
		}
		
		if (!done)
		{
			CClipHelper* pClipHelper = GetClipHelper();
			const crClip* pClip = pClipHelper->GetClip();

			// Grab the phase.
			float fNextPhase = ComputeNextPhase(*pClipHelper, *pClip);

			if (bFinishExitAnimationBeforeLeaving)
			{
				if (fNextPhase >= 0.99f && !pClipHelper->IsBlendingOut())
				{
					StopClip(SLOW_BLEND_OUT_DELTA, 0);
				}
			}
			else
			{
				// Check for an exit to a high fall.
				if (EndBecauseOfFallTagging())
				{
					done = true;
				}

				if (pClipHelper->IsHeldAtEnd() || fNextPhase > GetTunables().m_RegularExitDefaultPhaseThreshold)
				{
					done = true;
				}

				if (bFastExit)
				{
					if (CheckForInterruptibleTag(fNextPhase, *pClip) || fNextPhase > GetTunables().m_FastExitDefaultPhaseThreshold)
					{
						done = true;
					}
				}
			}
		}
	}
	else
	{
		const CTask* pSubTask = GetSubTask();
		if(!pSubTask || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
		{
			done = true;
		}
		else
		{
			// Check if we have passed the blend-out phase.
			const float phase = fwAnimDirectorComponentSyncedScene::GetSyncedScenePhase(m_SyncedSceneId);
			if(phase >= m_SyncedSceneBlendOutPhase)
			{
				done = true;
			}
		}

	}

	if (m_pPropHelper)
	{
		CPropManagementHelper::Context context(*pPed);
		context.m_ClipHelper = GetClipHelper();
		context.m_PropClipId = GetChosenClipId(GetPed(), this);
		context.m_PropClipSetId = m_exitClipSetId;
		m_pPropHelper->Process(context);

		if (done)
		{
			m_pPropHelper->ProcessClipEnding(context);
		}
	}

	if (done)
	{
		pPed->SetNoCollision(NULL, 0);

		// Try to exit the task by letting the event kill the task, but if his doesn't happen for one frame
		// then move to finish anyway.
		if (IsRespondingToEvent())
		{
			m_iScenarioReactionFlags.SetFlag(SREF_AllowEventsToKillThisTask);
		}
		else
		{
			//Force the motion state.
			if(bExitToWalk)
			{
				pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Walk);
			}
			else
			{
				pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle);
			}
			SetState(State_Finish);
		}
	}
	return FSM_Continue;
}


void CTaskUseScenario::Exit_OnExit()
{
	if(m_iExitClipsFlags.IsFlagSet(eExitClipsAreSynchronized))
	{
		ReleaseSynchronizedScene();
	}
	if (m_iFlags.IsFlagSet(SF_ExitingHangoutConversation))
	{
		StartNewScenarioAfterHangout();
		m_iFlags.ClearFlag(SF_ExitingHangoutConversation);
	}
	
	// clean up the anim flags
	ClearPlayedEnterAnimation();
}


void CTaskUseScenario::StreamWaitClip_OnEnter()
{
	DetermineConditionalAnims();

	CPed* pPed = GetPed();
	CScenarioCondition::sScenarioConditionData conditionData;
	conditionData.pPed = pPed;

	// Check for scripted cowering.
	if (m_iScenarioReactionFlags.IsFlagSet(SREF_ScriptedCowerInPlace))
	{
		conditionData.eAmbientEventType = AET_In_Place;
	}
	else
	{
		conditionData.eAmbientEventType = CTaskCowerScenario::ShouldPlayDirectedCowerAnimation(m_iEventTriggeringPanic) ? AET_Directed_In_Place : AET_In_Place;
	}

	m_panicClipSetId = ChooseConditionalClipSet(CConditionalAnims::AT_PANIC_INTRO, conditionData);

	Assertf(m_panicClipSetId != CLIP_SET_ID_INVALID, "Invalid panic clip set id.  Scenario:  %s, Conditional Anims:  %s, Previous State:  %d",
		GetScenarioInfo().GetName(), m_pConditionalAnimsGroup && m_ConditionalAnimsChosen > 0 ? m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimsChosen)->GetName() : "NULL", GetPreviousState());
}

CTask::FSM_Return CTaskUseScenario::StreamWaitClip_OnUpdate()
{
	if (CheckAttachStatus())
	{
		if (RagdollOutOfScenario())
		{
			return FSM_Quit;
		}
	}

	bool bKeepCowering = NetworkInterface::IsGameInProgress()?mbNewTaskMigratedWhileCowering:false;

	if (!IsRespondingToEvent() && !m_iScenarioReactionFlags.IsFlagSet(SREF_AggroOrCowardAgitationReaction) 
		&& !m_iScenarioReactionFlags.IsFlagSet(SREF_ScriptedCowerInPlace) && !bKeepCowering)
	{
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
		SetState(State_PlayAmbients);
		return FSM_Continue;
	}

	if (m_panicClipSetId != CLIP_SET_ID_INVALID)
	{
		m_ClipSetRequestHelper.RequestClipSetIfExists(m_panicClipSetId);
		OverrideStreaming();
		if (m_ClipSetRequestHelper.IsClipSetStreamedIn())
		{
			SetState(State_WaitForProbes);
		}
	}
	else
	{
		Assertf(false, "Somehow got into the wait clip state without having a wait clip.  Scenario type:  %s Mission ped:  %d", GetScenarioInfo().GetName(), GetPed()->PopTypeIsMission());
	}
	return FSM_Continue;
}

void CTaskUseScenario::StreamWaitClip_OnExit()
{
	// Blend slowly into the cower clip.
	if (GetState() == State_WaitForProbes)
	{
		CTaskAmbientClips* pSubTask = static_cast<CTaskAmbientClips*>(GetSubTask());
		if (pSubTask)
		{
			pSubTask->SetCleanupBlendOutDelta(SLOW_BLEND_OUT_DELTA);
		}
	}
}

bool CTaskUseScenario::ShouldStreamWaitClip()
{
	// If responding to disputes, never use a cowering clip.
	if (m_iScenarioReactionFlags.IsFlagSet(SREF_AggroOrCowardAgitationReaction))
	{
		return false;
	}

	// If responding to directed script exit, never use a cowering clip.
	if (m_iScenarioReactionFlags.IsFlagSet(SREF_DirectedScriptExit))
	{
		return false;
	}

	// Build a conditional anims clip request.
	DetermineConditionalAnims();
	CPed* pPed = GetPed();
	CScenarioCondition::sScenarioConditionData conditionData;
	conditionData.pPed = pPed;

	// Check for scripted cowering in place.
	if (m_iScenarioReactionFlags.IsFlagSet(SREF_ScriptedCowerInPlace))
	{
		conditionData.eAmbientEventType = AET_In_Place;
	}
	else
	{
		conditionData.eAmbientEventType = CTaskCowerScenario::ShouldPlayDirectedCowerAnimation(m_iEventTriggeringPanic) ? AET_Directed_In_Place : AET_In_Place;
	}

	// Check to make sure there actually is a cowering clip to play.
	if (ChooseConditionalClipSet(CConditionalAnims::AT_PANIC_INTRO, conditionData) == CLIP_SET_ID_INVALID)
	{
		return false;
	}

	return true;
}

// While waiting on probes, decide if a cowering subtask should be created or if one already exists.
bool CTaskUseScenario::ShouldCreateCoweringSubtaskInWaitForProbes() const
{
	if (GetPreviousState() == State_StreamWaitClip)
	{
		return true;
	}

	if (GetPreviousState() == State_Enter || GetPreviousState() == State_PlayAmbients)
	{
		if (GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee))
		{
			return true;
		}
	}

	// Need to create the cowering tasks if the ped just migrated over.
	if (NetworkInterface::IsGameInProgress() && mbNewTaskMigratedWhileCowering && GetPreviousState() == State_Start)
	{
		if (GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee))
		{
			return true;
		}
	}

	return false;
}

void CTaskUseScenario::CheckKeepCloneCowerSubTaskAtStateTransition()
{
	if( taskVerifyf(GetPed(),"Null ped") && taskVerifyf(GetPed()->IsNetworkClone(),"%s is not a clone",GetPed()->GetDebugName()) )
	{
		if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_COWER_SCENARIO &&
			GetPed()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COWER_SCENARIO))
		{
			SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition); //keeps the cower task if running on clone
		}
	}
}


void CTaskUseScenario::CreateCoweringSubtask()
{
	CPed* pPed = GetPed();

	if (pPed->GetTaskData().GetIsFlagSet(CTaskFlags::DisableCower))
	{
		// Respect task flags - if you can't cower then do nothing.
		SetNewTask(rage_new CTaskDoNothing(-1));
	}
	else
	{
		if (m_panicClipSetId != CLIP_SET_ID_INVALID && m_pConditionalAnimsGroup && m_ConditionalAnimsChosen >= 0 && m_ConditionalAnimsChosen < m_pConditionalAnimsGroup->GetNumAnims())
		{
			// We have in place cowers - use them.
			CTaskCowerScenario* pTaskCower = rage_new CTaskCowerScenario(GetScenarioType(), GetScenarioPoint(), m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimsChosen), m_Target, m_iEventTriggeringPanic, m_iPriorityOfEventTriggeringPanic);
			pTaskCower->SetCowerForever(true);
			SetNewTask(pTaskCower);
		}
		else
		{
			// We don't have in place cowers.  Just use the default cower task.
			CTaskCower* pTaskCower = rage_new CTaskCower(-1);
			SetNewTask(pTaskCower);
		}
	}
}

// Look at the reaction flags and decide how to exit the point.
s32 CTaskUseScenario::DecideOnStateForExit() const
{
	if (m_iScenarioReactionFlags.IsFlagSet(SREF_ExitingToFlee))
	{
		return State_PanicExit;
	}
	else if (m_iScenarioReactionFlags.IsFlagSet(SREF_CowardReaction))
	{
		return State_AggroOrCowardIntro;
	}
	else if (m_iScenarioReactionFlags.IsFlagSet(SREF_AggroAgitation) && !m_iScenarioReactionFlags.IsFlagSet(SREF_AgitationWithNormalExit))
	{
		return State_AggroOrCowardIntro;
	}
	else
	{
		return State_Exit;
	}
}

void CTaskUseScenario::WaitForProbes_OnEnter()
{
	CPed* pPed = GetPed();

	// Potentially create a subtask to play while waiting.
	if (ShouldCreateCoweringSubtaskInWaitForProbes())
	{
		CreateCoweringSubtask();
	}
	
	// Figure out what clipset to try and exit with.  No need to do this if we are going to cower forever, though.
	if (!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee))
	{
		// Reset this after picking a panic clipset for the cower intro.
		m_panicClipSetId = CLIP_SET_ID_INVALID;

		CScenarioCondition::sScenarioConditionData conditionData;
		InitConditions(conditionData);

		// Decide based on the reaction flags what kind of clip to select.
		if (m_iScenarioReactionFlags.IsFlagSet(SREF_ExitingToFlee))
		{
			m_panicClipSetId = ChooseConditionalClipSet(CConditionalAnims::AT_PANIC_EXIT, conditionData);
		}
		else if (m_iScenarioReactionFlags.IsFlagSet(SREF_CowardReaction))
		{
			m_panicClipSetId = ChooseConditionalClipSet(CConditionalAnims::AT_PANIC_INTRO, conditionData);
		}
		else if (m_iScenarioReactionFlags.IsFlagSet(SREF_AggroAgitation))
		{
			// Check for aggro agitation (uses panic intro).
			m_panicClipSetId = ChooseConditionalClipSet(CConditionalAnims::AT_PANIC_INTRO, conditionData);

			if (m_panicClipSetId == CLIP_SET_ID_INVALID)
			{
				m_iScenarioReactionFlags.SetFlag(SREF_AgitationWithNormalExit);
				OnLeavingScenarioDueToAgitated();
			}
		}

		if (m_panicClipSetId == CLIP_SET_ID_INVALID)
		{
			// Reaction will use the <Exit> conditional anims group section.
			DetermineExitAnimations(true);
			m_panicClipSetId = m_exitClipSetId;
		}

		Assertf(m_panicClipSetId != CLIP_SET_ID_INVALID, 
			"Scenario ped is trying to play an exit clip as a response to some event but there were no exit clips.  Bad scenarios.meta %s?  Disputes? %d Event? %d Scripted cower in place %d Migrated while cowering? %d. %s, StationaryReactions flag %s. m_ConditionalAnimChosen %d, m_pConditionalAnimsGroup %s, eExitClipsDetermined? %d, m_iScenarioReactionFlags 0x%x", 
			GetScenarioInfo().GetName(), m_iScenarioReactionFlags.IsFlagSet(SREF_AggroOrCowardAgitationReaction), m_iEventTriggeringPanic, m_iScenarioReactionFlags.IsFlagSet(SREF_ScriptedCowerInPlace), mbNewTaskMigratedWhileCowering,
			pPed->GetDebugName(),
			GetScenarioPoint()?(GetScenarioPoint()->IsFlagSet(CScenarioPointFlags::StationaryReactions)?"T":"F"):"null SP",
			m_ConditionalAnimsChosen,
			m_pConditionalAnimsGroup?m_pConditionalAnimsGroup->GetName():"Null m_pConditionalAnimsGroup",
			m_iExitClipsFlags.IsFlagSet(eExitClipsDetermined),
			m_iScenarioReactionFlags.GetAllFlags());
	}
}

CTask::FSM_Return CTaskUseScenario::WaitForProbes_OnUpdate()
{
	if (CheckAttachStatus())
	{
		if (RagdollOutOfScenario())
		{
			return FSM_Quit;
		}
	}

	bool bKeepCowering = NetworkInterface::IsGameInProgress()?mbNewTaskMigratedWhileCowering:false;

	if (!IsRespondingToEvent() && !m_iScenarioReactionFlags.IsFlagSet(SREF_AggroOrCowardAgitationReaction) 
		&& !m_iScenarioReactionFlags.IsFlagSet(SREF_ScriptedCowerInPlace) && !bKeepCowering)
	{
		// If the event disappeared go back to playing the ambient animations.
		m_ExitHelper.Reset();
		m_ExitHelper.ResetPreviousWinners();
		SetState(State_PlayAmbients);
		return FSM_Continue;
	}

	CPed* pPed = GetPed();

	// If at any time the ped has decided to cower, move to the cowering state.
	bool bDoStationaryReactions = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee);
	if (bDoStationaryReactions)
	{
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
		SetState(State_WaitingOnExit);
		return FSM_Continue;
	}

	// Stream in the event response clip.
	m_ClipSetRequestHelper.RequestClipSetIfExists(m_panicClipSetId);
	OverrideStreaming();
	
	if (m_ClipSetRequestHelper.IsClipSetStreamedIn())
	{
		fwMvClipId chosenClipId = CLIP_ID_INVALID;

		// Decide how to select a clip from the clipset.
		CConditionalAnims::eAnimType animType = CConditionalAnims::AT_EXIT;
		s32 iStateForReaction = DecideOnStateForExit();
		switch(iStateForReaction)
		{
			case State_PanicExit:
			{
				animType = CConditionalAnims::AT_PANIC_EXIT;
				break;
			}
			case State_AggroOrCowardIntro:
			{
				animType = CConditionalAnims::AT_PANIC_INTRO;
				break;
			}
			default:
			{
				animType = CConditionalAnims::AT_EXIT;
				break;
			}
		}

		// Select a clip.
		// Either pick from the clippicker helper or use the stored clip from an earlier check if the reason for the previous failure was blackboard.
		CScenarioClipHelper::ScenarioClipCheckType iType = m_iScenarioReactionFlags.IsFlagSet(SREF_WaitingOnBlackboard) 
			? CScenarioClipHelper::ClipCheck_Success : ChooseClipInClipSet(animType, m_panicClipSetId, chosenClipId);
		if (iType == CScenarioClipHelper::ClipCheck_Success)
		{
			// Only set the chosen clip if we didn't store it earlier.
			if (!m_iScenarioReactionFlags.IsFlagSet(SREF_WaitingOnBlackboard))
			{
				SetChosenClipId(chosenClipId, GetPed(), this);
			}

			// Check with the blackboard to see if we're allowed to play this clip right now
			CAnimBlackboard::AnimBlackboardItem blackboardItem(GetPed()->GetTransform().GetPosition(), CAnimBlackboard::PANIC_EXIT, CAnimBlackboard::REACTION_ANIM_TIME_DELAY);
			if (sm_bIgnoreBlackboardForScenarioReactions || ANIMBLACKBOARD.AddItemToBlackboardAndPedIntelligenceIfAllowedToPlay(blackboardItem, pPed->GetPedIntelligence()))
			{
				SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
				m_iScenarioReactionFlags.ClearFlag(SREF_WaitingOnBlackboard);
				SetState(State_WaitForPath);
			}
			else
			{
				// Blackboard prevented us from playing the clip. Remember what the winning clip was and try to play it next frame.
				m_iScenarioReactionFlags.SetFlag(SREF_WaitingOnBlackboard);
			}
		}
		else if (iType == CScenarioClipHelper::ClipCheck_Pending)
		{
			// Still working on the probe checks.  Try again later.
		}
		else
		{
			SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
			if (!m_iScenarioReactionFlags.IsFlagSet(SREF_ExitingToFlee) && m_Target.GetIsValid())
			{
				// The ped was trying to play a non-flee exit.  Try again, this time playing the flee exit.
				m_iScenarioReactionFlags.SetFlag(SREF_ExitingToFlee);

				// Reset the clip picker as the clipset has changed.
				m_ExitHelper.ResetPreviousWinners();
				SetFlag(aiTaskFlags::RestartCurrentState);
			}
			else
			{
				// Tried and failed to play a flee exit clip.  Cowering is all we can do at this point.
				m_panicClipSetId = CLIP_SET_ID_INVALID;

				// Now that all clips have been exhausted, we don't need to keep track of the successful clips anymore.
				m_ExitHelper.ResetPreviousWinners();
				SetState(State_WaitingOnExit);
			}
		}	
	}
	
	return FSM_Continue;
}

void CTaskUseScenario::WaitForPath_OnEnter()
{
	CPed* pPed = GetPed();

	// Need to reset these values here - otherwise we won't apply the right translation offset in UpdateTargetSituation().
	// We can go through WaitForPath_OnEnter() multiple times...i.e. when a ped tries to exit, cowers, and tries to exit again.
	m_bExitToLowFall = false;
	m_bExitToHighFall = false;

	// Update the target situation. Do this before the pathfind request since we will use this position there
	UpdateTargetSituation();

	//Set the initial offsets.
	m_PlayAttachedClipHelper.SetInitialOffsets(*pPed);

	if (!pPed->IsNetworkClone())
	{
		StartNavMeshTask();
	}
}

CTask::FSM_Return CTaskUseScenario::WaitForPath_OnUpdate()
{
	KeepNavMeshTaskAlive();
	if (CheckAttachStatus())
	{
		if (RagdollOutOfScenario())
		{
			return FSM_Quit;
		}
	}

	CPed* pPed = GetPed();

	bool bWaitForPath = GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::ValidateExitsWithProbeChecks) && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_NeverDoScenarioExitProbeChecks) && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_NeverDoScenarioNavChecks);

	// Script and data can override the need to do a path check.
	if (!bWaitForPath)
	{
		s32 iNextState = DecideOnStateForExit();
		SetState(iNextState);
		return FSM_Continue;
	}

	if (!IsRespondingToEvent() && !m_iScenarioReactionFlags.IsFlagSet(SREF_AggroOrCowardAgitationReaction))
	{
		// If the event disappeared go back to playing the ambient animations.
		m_ExitHelper.Reset();
		m_ExitHelper.ResetPreviousWinners();
		SetState(State_PlayAmbients);
		return FSM_Continue;
	}

	// Stream in the panic exit clip.
	m_ClipSetRequestHelper.RequestClipSetIfExists(m_panicClipSetId);
	OverrideStreaming();

	if (m_ClipSetRequestHelper.IsClipSetStreamedIn())
	{
		if(GetPed()->IsNetworkClone())
		{
			bool bCloneStateChanging = false;
			if(GetStateFromNetwork()==State_WaitingOnExit)
			{
				// If we ran out of space in the successful clips, reset this here and go to cowering to try again.
				m_ExitHelper.ResetPreviousWinners();
				bCloneStateChanging = true;
				SetState(State_WaitingOnExit);
			}
			else if (GetStateFromNetwork()==State_WaitForProbes)
			{
				m_ExitHelper.ResetPreviousWinners();
				bCloneStateChanging = true;
				SetState(State_WaitForProbes);
			}
			if( GetStateFromNetwork() == State_PanicExit || 
				GetStateFromNetwork() == State_AggroOrCowardIntro || 
				GetStateFromNetwork() == State_AggroOrCowardIntro )
			{
				// We got a good path, we can forget about the previous winners at this point.
				m_ExitHelper.ResetPreviousWinners();
				s32 iNextState = DecideOnStateForExit();
				bCloneStateChanging = true;
				SetState(iNextState);
			}
			if(bCloneStateChanging)
			{
				CheckKeepCloneCowerSubTaskAtStateTransition();
			}
			return FSM_Continue;
		}

		CTask* pMoveTask = pPed->GetPedIntelligence()->GetGeneralMovementTask();
		if (pMoveTask && pMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
		{
			CTaskMoveFollowNavMesh* pNavmeshTask = static_cast<CTaskMoveFollowNavMesh*>(pMoveTask);
			if (pNavmeshTask->IsFollowingNavMeshRoute())
			{
				float fLength = pNavmeshTask->GetTotalRouteLength();

				if (fLength > GetTunables().m_MinPathLengthForValidExit)
				{
					// Restore default properties that we only wanted to change for checking the route.
					pNavmeshTask->SetQuitTaskIfRouteNotFound(false);

					// We got a good path, we can forget about the previous winners at this point.
					m_ExitHelper.ResetPreviousWinners();
					s32 iNextState = DecideOnStateForExit();
					SetState(iNextState);
				}
				else
				{
					SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
					if (!m_ExitHelper.CanStoreAnyMoreWinningIndices())
					{
						// If we ran out of space in the successful clips, reset this here and go to cowering to try again.
						m_ExitHelper.ResetPreviousWinners();
						SetState(State_WaitingOnExit);
					}
					else
					{
						// Go back to the wait on probes state, in the hopes that there is another clip that works out and is more fortunate as far as pathing goes.
						SetState(State_WaitForProbes);
						return FSM_Continue;
					}
				}
			}
		}
		else
		{
			// The navmesh task was destroyed because a path was not found.
			SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
			if (!m_ExitHelper.CanStoreAnyMoreWinningIndices())
			{
				// If we ran out of space in the successful clips, reset this here and go to cowering to try again.
				m_ExitHelper.ResetPreviousWinners();
				SetState(State_WaitingOnExit);
			}
			else
			{
				// Go back to the wait on probes state, in the hopes that there is another clip that works out and is more fortunate as far as pathing goes.
				SetState(State_WaitForProbes);
				return FSM_Continue;
			}
		}
	}

	return FSM_Continue;
}

void CTaskUseScenario::PanicExit_OnEnter()
{
	CPed* pPed = GetPed();
	if (Verifyf(m_panicClipSetId != CLIP_SET_ID_INVALID, "Panic Exit started without a valid clipsetID!"))
	{
		if (Verifyf(m_ClipSetRequestHelper.IsClipSetStreamedIn(), "Panic Exit clip should already be streamed in by HandlePanicSetup()!"))
		{
			pPed->SetNoCollision(GetScenarioEntity(), NO_COLLISION_NEEDS_RESET);

			if ( fwAnimManager::GetClipIfExistsBySetId(m_panicClipSetId, GetChosenClipId(GetPed(), this)) )
			{
				int iClipPriority = GetExitClipPriority();
				
				float fBlendDuration = fabs(GetScenarioInfo().GetPanicExitBlendInDuration());
				// Metadata stores it in seconds, StartClipBySetAndClip expects things to be 1 / seconds.
				fBlendDuration = fBlendDuration == 0.0f ? INSTANT_BLEND_IN_DELTA : 1.0f / fBlendDuration;

				StartClipBySetAndClip(pPed, m_panicClipSetId, GetChosenClipId(GetPed(), this), APF_ISBLENDAUTOREMOVE, 0, iClipPriority, fBlendDuration, CClipHelper::TerminationType_OnDelete);
				RandomizeCowerPanicRateAndUpdateLastRate();
			}
			else
			{
				taskAssertf(false,"Scenario panic outro clip not found %s\n",PrintClipInfo(m_panicClipSetId,GetChosenClipId(GetPed(), this)));
			}
		}
	}

	// Let the ped push other peds around.
	TogglePedCapsuleCollidesWithInactives(true);
}

// PURPOSE:  Start a navmesh task on the ped during the panic animation, so that when the ped is done playing the anim they can move onto it for SmartFlee.
void CTaskUseScenario::StartNavMeshTask()
{
	Vector3 vPosition;
	if (m_Target.GetIsValid() && m_Target.GetPosition(vPosition))
	{
		//Grab the ped.
		CPed* pPed = GetPed();

		Assertf(!pPed->IsNetworkClone(),"Don't expect clone to use this function");

		//Create the task.
		CTaskMoveFollowNavMesh* pNavTask = CTaskSmartFlee::CreateMoveTaskForFleeOnFoot(VECTOR3_TO_VEC3V(vPosition));

		//Set the spheres on influence.
		pNavTask->SetInfluenceSphereBuilderFn(SpheresOfInfluenceBuilder);

		//Set the move/blend ratio.  Keep it at 0 to block any runstart animations from happening.
		//This will be set to something more appropriate when the ped leaves the PanicExit() state.
		pNavTask->SetMoveBlendRatio(0.0f);

		pNavTask->SetPolySearchForward(true);

		// Flee settings roughly equivalent to those used in CTaskSmartFlee.

		// Keep to pavements if that is the ped's preference.
		if (pPed && pPed->GetPedIntelligence() && pPed->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().IsFlagSet(CFleeBehaviour::BF_PreferPavements))
		{	
			pNavTask->SetKeepToPavements(true);
		}

		// Clamp the safe flee distance to be some reasonable value - too large of a value and the target point will be skewed
		// to possibly run towards the threat.
		float fFleeSafeDist = Dist(pPed->GetTransform().GetPosition(), VECTOR3_TO_VEC3V(vPosition)).Getf() + sm_Tunables.m_ExtraFleeDistance;
		pNavTask->SetFleeSafeDist(fFleeSafeDist);

		//Only let the task end in water if the ped is currently in water.
		pNavTask->SetFleeNeverEndInWater( !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsDrowning) );

		//Since the path length is pretty short, it makes sense to have the ped keep moving once reaching the end of it.
		pNavTask->SetDontStandStillAtEnd(true);

		// If attached to something that isn't fixed, we need the path to not snap quite so much.
		// That way if the path started somewhere where there isn't navmesh it will fail as expected.
		CEntity* pScenarioEntity = GetScenarioEntity();
		if (!pScenarioEntity || (pScenarioEntity->GetBaseModelInfo() && (pScenarioEntity->GetBaseModelInfo()->GetIsFixed() || pScenarioEntity->GetBaseModelInfo()->GetIsFixedForNavigation())))
		{
			// Respect overrides.
			float fMaxDistanceMayAdjustPath = GetScenarioInfo().GetMaxDistanceMayAdjustPathSearchOnExit();

			if (fMaxDistanceMayAdjustPath < 0.0f)
			{
				fMaxDistanceMayAdjustPath = sm_Tunables.m_MaxDistanceNavmeshMayAdjustPath;
			}
			
			pNavTask->SetMaxDistanceMayAdjustPathStartPosition(fMaxDistanceMayAdjustPath);

			// We want the navmesh task to fail completely.
			pNavTask->SetQuitTaskIfRouteNotFound(true);
		}

		// Add the move task for the ped so it can be picked back up by TaskSmartFlee later.
		pPed->GetPedIntelligence()->AddTaskMovement(pNavTask);

		// Since CTaskMoveFollowNavMesh is not running under CTaskComplexControlMovement, set this task as the owner.
		CTask* pMoveTask = pPed->GetPedIntelligence()->GetGeneralMovementTask();
		if (pMoveTask)
		{
			CTaskMoveInterface* pMoveInterface = pMoveTask->GetMoveInterface();
			if (pMoveInterface)
			{
				NOTFINAL_ONLY(pMoveInterface->SetOwner(this));
			}
		}
	}
}

CTask::FSM_Return CTaskUseScenario::PanicExit_OnUpdate()
{
	if (CheckAttachStatus())
	{
		if (RagdollOutOfScenario())
		{
			return FSM_Quit;
		}
	}

	CPed* pPed = GetPed();
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostMovementTimeSliced, true);

	// Start streaming in the reaction to flee clips now so that they are ready once the ReactAndFlee task starts.
	m_ClipSetRequestHelper.Request(CLIP_SET_REACTION_GUNFIRE_RUNS);
	OverrideStreaming();

	// if we have a prop helper
	if (m_pPropHelper)
	{
		// update prop status
		m_pPropHelper->UpdateExistingOrMissingProp(*pPed);
	}

#if SCENARIO_DEBUG
	if (m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData))
	{
		if (!GetClipHelper())
		{
			m_panicClipSetId = CLIP_SET_ID_INVALID;
			SetState(State_Finish); //we are done playing so quit ... 
		}		
		return FSM_Continue;
	}
#endif // SCENARIO_DEBUG

	// Do head IK if responding to an event.
	LookAtTarget(1.0f);

	CClipHelper* pClipHelper = GetClipHelper();

	bool shouldEnd = false;
	if (!pClipHelper || !pClipHelper->GetClip())
	{
		shouldEnd = true;
	}
	else
	{
		const crClip* pClip = pClipHelper->GetClip();

		// The EventHandling logic occurs before the task update, so to actually get this right we need to guess what the next phase will be.
		float fNextPhase = ComputeNextPhase(*pClipHelper, *pClip);

		bool bFinishExitAnimationBeforeLeaving = GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::FinishExitAnimationBeforeLeaving);

		if (bFinishExitAnimationBeforeLeaving)
		{
			if (fNextPhase >= 0.99f && !pClipHelper->IsBlendingOut())
			{
				StopClip(SLOW_BLEND_OUT_DELTA, 0);
			}
		}
		else
		{
			if (EndBecauseOfFallTagging())
			{
				shouldEnd = true;
			}

			if (CheckForInterruptibleTag(fNextPhase, *pClip))
			{
				shouldEnd = true;
			}

			// In case the clip is not tagged.
			if (fNextPhase >= GetTunables().m_ReactAndFleeBlendOutPhase && !pPed->IsNetworkClone())
			{
				shouldEnd = true;
			}
		}
	}

	if (shouldEnd)
	{
		pPed->SetNoCollision(NULL, 0);
		m_iScenarioReactionFlags.SetFlag(SREF_AllowEventsToKillThisTask);

		// B* 426456 - disable handsup after panic exiting a scenario
		pPed->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().SetFlag(CFleeBehaviour::BF_DisableHandsUp);
	}

	// Maintain the movement task started on the ped earlier.
	KeepNavMeshTaskAlive();

	// if we have a prop helper
	if (m_pPropHelper)
	{
		// Process clip events
		CPropManagementHelper::Context context(*pPed);
		context.m_ClipHelper = GetClipHelper();
		context.m_PropClipId = GetChosenClipId(GetPed(), this);
		context.m_PropClipSetId = m_panicClipSetId;
		m_pPropHelper->Process(context);
	}

	return FSM_Continue;
}

void CTaskUseScenario::PanicExit_OnExit()
{
	CPed* pPed = GetPed();
	CTask* pMoveTask = pPed->GetPedIntelligence()->GetGeneralMovementTask();
	if (pMoveTask && pMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
	{
		if (m_iScenarioReactionFlags.IsFlagSet(SREF_ExitingToFlee) && IsRespondingToEvent())
		{
			// Now that the ped is about to leave the scenario, set the MBR to something appropriate.
			CTaskMoveFollowNavMesh* pNavmesh = static_cast<CTaskMoveFollowNavMesh*>(pMoveTask);
			pNavmesh->SetMoveBlendRatio(fwRandom::GetRandomNumberInRange(sm_Tunables.m_FleeMBRMin, sm_Tunables.m_FleeMBRMax));
		}
	}
}

void CTaskUseScenario::NMExit_OnEnter()
{
	CPed* pPed = GetPed();
	if (pPed->GetIsAttached())
	{

		pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
	}

	CTaskGetUp* pTask = rage_new CTaskGetUp();
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskUseScenario::NMExit_OnUpdate()
{
	// Do head IK if responding to an event.
	LookAtTarget(1.0f);

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		m_iScenarioReactionFlags.SetFlag(SREF_AllowEventsToKillThisTask);
	}
	return FSM_Continue;
}

void CTaskUseScenario::PanicNMExit_OnEnter()
{
	CPed* pPed = GetPed();
	if (pPed->GetIsAttached())
	{
		pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
	}

	Vector3 vTarget;

	if (m_Target.GetIsValid())
	{
		m_Target.GetPosition(vTarget);
	}
	else
	{
		// Invalid target (maybe the event we are responding to didn't have a position).
		// So just use the forward vector as our "direction" for the response.
		vTarget = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition() + pPed->GetTransform().GetForward());
	}

	// From a quarter to a half second of NM.
	CTaskNMFlinch* pNMTask = rage_new CTaskNMFlinch(250, 500, vTarget, NULL);

	// Set up the NM task to blend out when finished.
	CTaskNMControl* pNMControlTask = rage_new CTaskNMControl(65535, 65535, pNMTask, CTaskNMControl::DO_BLEND_FROM_NM);
	SetNewTask(pNMControlTask);
}

CTask::FSM_Return CTaskUseScenario::PanicNMExit_OnUpdate()
{
	// Do head IK if responding to an event.
	LookAtTarget(1.0f);

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		m_iScenarioReactionFlags.SetFlag(SREF_AllowEventsToKillThisTask);
	}
	return FSM_Continue;
}

void CTaskUseScenario::KeepNavMeshTaskAlive()
{
	CPed* pPed = GetPed();
	// Playing the animation while attached and heading to a flee, so keep the navmesh task active.	
	CTask* pMoveTask = pPed->GetPedIntelligence()->GetGeneralMovementTask();
	if (pMoveTask)
	{
		// Since CTaskMoveFollowNavMesh is not running under CTaskComplexControlMovement, set this task as the owner.
		CTaskMoveInterface* pMoveInterface = pMoveTask->GetMoveInterface();
		if (pMoveInterface)
		{
			pMoveInterface->SetCheckedThisFrame(true);

			if (m_iScenarioReactionFlags.IsFlagSet(SREF_ExitingToFlee))
			{
				// Notify react and flee that we have already played the reaction part of ReactAndFlee so the ped doesn't do a double take.
				pPed->SetPedResetFlag(CPED_RESET_FLAG_SkipReactInReactAndFlee, true);

				// Force a forward facing transition clip to improve blending.
				pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceForwardTransitionInReactAndFlee, true);

				// Disable timeslicing, since the navmesh task needs to be checked every frame.
				pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
			}
		}
	}
}

void CTaskUseScenario::CalculateTargetSituation(Vector3& vTargetPosition, Quaternion& qTargetOrientation, fwMvClipSetId clipSetId, fwMvClipId clipId, bool bAdjustFallStatus)
{
	//Get the scenario position and heading.
	Vector3 vWorldPosition;
	float fWorldHeading;
	GetScenarioPositionAndHeading(vWorldPosition, fWorldHeading);
	fWorldHeading += m_fBaseHeadingOffset;

	s32 iState = GetState();

	bool bAdjustedForPedCapusle = false;
	
	if (iState == State_Enter || iState == State_AggroOrCowardOutro)
	{
		//Assign the target position.
		vTargetPosition = vWorldPosition;

		//Assign the target orientation.
		qTargetOrientation.FromEulers(Vector3(0.0f, 0.0f, fWorldHeading), "xyz");
	}
	else if (iState == State_Exit || iState == State_PanicExit || iState == State_AggroOrCowardIntro || iState == State_WaitForPath)
	{
		Assertf(clipSetId != CLIP_SET_ID_INVALID, "Scenario outro clipsetid was invalid.  State:  %d, Scenario:  %s, Conditional Anim:  %s", 
			iState, GetScenarioInfo().GetName(),  m_pConditionalAnimsGroup && m_ConditionalAnimsChosen >= 0 ? m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimsChosen)->GetName() : "NULL");
		const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(clipSetId, clipId);
		if (pClip)
		{
			Vector3 vEvent;
			Quaternion qWorldOrientation;
			qWorldOrientation.FromEulers(Vector3(0.0f, 0.0f, fWorldHeading), "xyz");
			
			//Compute the end situation.
			float fZShift = 0.0f;

			// HACK GTA5 - B*1941739 - Don't check vehicles to compute the ground location for the seat steps scenario.
			// It's easier to do this check directly in code than it is to patch scenarios.meta at this point.
			static const atHashWithStringNotFinal stepsHackHash("WORLD_HUMAN_SEAT_STEPS", 0xe8107aad);
			bool bCheckVehicles = GetScenarioInfo().GetHash() != stepsHackHash.GetHash();
			bAdjustedForPedCapusle = m_PlayAttachedClipHelper.ComputeIdealEndSituation(GetPed(), vWorldPosition, qWorldOrientation, pClip, vTargetPosition, qTargetOrientation, fZShift, true, bCheckVehicles);

			float fSmallFallTolerance = GetScenarioInfo().GetReassessGroundExitThreshold();
			float fHighFallTolerance = GetScenarioInfo().GetFallExitThreshold();

			// Try to deal with exiting to a fall.
			if (bAdjustFallStatus && !(m_bExitToLowFall || m_bExitToHighFall) && fSmallFallTolerance > 0.0f && fabs(fZShift) > fSmallFallTolerance)
			{
				// Unshift the target position so it no longer tries to hit the ground.
				vTargetPosition.z -= fZShift;

				// Check if this is a high fall or a low fall.
				if (fHighFallTolerance > 0.0f && fZShift < -fHighFallTolerance)
				{
					m_bExitToHighFall = true;
				}
				else
				{
					m_bExitToLowFall = true;
				}
			}
		}
		else
		{
			taskAssertf(false,"Scenario outro clip not found %s\n",PrintClipInfo(clipSetId,clipId));
		}
	}
	else
	{
		taskAssert(0);
	}

	if (m_eScenarioMode == SM_WorldEffect || m_eScenarioMode == SM_EntityEffect)
	{
		const CScenarioInfo *pInfo = CScenarioManager::GetScenarioInfo( GetScenarioType() );
		if ( !bAdjustedForPedCapusle && !pInfo->GetIsFlagSet( CScenarioInfoFlags::AttachPed ) )
		{
			aiAssert(GetPed()->GetCapsuleInfo());
			vTargetPosition.z += GetPed()->GetCapsuleInfo()->GetGroundToRootOffset();
		}
	}
}

void CTaskUseScenario::GetScenarioPositionAndHeading(Vector3& vWorldPosition, float& fWorldHeading) const
{
	const CPed& ped = *GetPed();

	//Check the mode.
	switch(m_eScenarioMode)
	{
	case SM_EntityEffect:
	case SM_WorldEffect:
		{
			CScenarioPoint* pPoint = GetScenarioPoint();
			if(pPoint)
			{
				vWorldPosition = VEC3V_TO_VECTOR3(pPoint->GetPosition());
				fWorldHeading  = pPoint->GetDirectionf();
				if(GetScenarioEntity())
				{
					fWorldHeading = fwAngle::LimitRadianAngle(GetScenarioEntity()->GetTransform().GetHeading() + fWorldHeading);
				}
			}
			else
			{
				// Ideally we wouldn't hit this case at all, but we can when the player teleports away.  The following prevents
				// follow-up crashes. See B* 443456:
				// "Fatal Assert - TaskUseScenario.cpp(1077): [ai_task] Error: Verifyf(GetScenarioPoint()) FAILED: ScenarioPoint is NULL"
				vWorldPosition = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
				fWorldHeading = ped.GetTransform().GetHeading();
			}
		}
		break;
	case SM_WorldLocation:
		{
			//Either use the ped's parameters, or the values passed in specifically by the caller.
			if(m_iFlags.IsFlagSet(SF_StartInCurrentPosition))
			{
				vWorldPosition = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
				fWorldHeading = ped.GetTransform().GetHeading();
			}
			else
			{
				vWorldPosition = m_vPosition;
				fWorldHeading  = m_fHeading;
			}
		}
		break;
	default:
		taskAssertf(0, "Unknown scenario mode: %d", (int)m_eScenarioMode);
		break;
	}
}

void CTaskUseScenario::UpdateTargetSituation(bool bAdjustFallStatus)
{
	//Calculate the target situation.
	Vector3 vTargetPosition(Vector3::ZeroType);
	Quaternion qTargetOrientation(0.0f,0.0f,0.0f,1.0f);


	switch (GetState())
	{
		case State_Enter:
		{
			if (m_enterClipSetId == CLIP_SET_ID_INVALID)
			{
				return;
			}
			m_PlayAttachedClipHelper.SetTranslationalOffsetSetEnabled(!GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::IgnoreSlideToCoordOnArrival));
			CalculateTargetSituation(vTargetPosition, qTargetOrientation,GetEnterClipSetId(GetPed(), this),GetChosenClipId(GetPed(), this), bAdjustFallStatus);
			break;
		}
		case State_Exit:
		{
			if (m_exitClipSetId == CLIP_SET_ID_INVALID)
			{
				return;
			}
			taskAssert(m_iExitClipsFlags.IsFlagSet(eExitClipsDetermined));
			m_PlayAttachedClipHelper.SetTranslationalOffsetSetEnabled(!GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::IgnoreSlideToCoordOnExit));
			m_PlayAttachedClipHelper.SetRotationalOffsetSetEnabled(!GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::IgnoreScenarioPointHeadingOnExit));
			CalculateTargetSituation(vTargetPosition, qTargetOrientation,m_exitClipSetId,GetChosenClipId(GetPed(), this), bAdjustFallStatus);
			m_bHasExitStartPos = true;
			break;
		}
		case State_WaitForPath:
		case State_PanicExit:
		{
			if (m_panicClipSetId == CLIP_SET_ID_INVALID)
			{
				return;
			}
			m_PlayAttachedClipHelper.SetTranslationalOffsetSetEnabled(true);
			CalculateTargetSituation(vTargetPosition, qTargetOrientation,m_panicClipSetId,GetChosenClipId(GetPed(), this), bAdjustFallStatus);
			m_bHasExitStartPos = true;
			break;
		}
		case State_AggroOrCowardIntro:
		{
			// It is possible that the target of the reaction was destroyed during the reaction, which means that the panic clipset gets reset to invalid.
			// In that case return, as we can't update the target situation anymore without a valid target.  This is my theory on B*1342743.
			if (m_panicClipSetId == CLIP_SET_ID_INVALID)
			{
				return;
			}
			m_PlayAttachedClipHelper.SetTranslationalOffsetSetEnabled(false);
			CalculateTargetSituation(vTargetPosition, qTargetOrientation, m_panicClipSetId,GetChosenClipId(GetPed(), this), bAdjustFallStatus);
			break;
		}
		case State_AggroOrCowardOutro:
		{
			if (m_panicClipSetId == CLIP_SET_ID_INVALID)
			{
				return;
			}
			m_PlayAttachedClipHelper.SetTranslationalOffsetSetEnabled(true);
			CalculateTargetSituation(vTargetPosition, qTargetOrientation, m_panicClipSetId,GetChosenClipId(GetPed(), this), bAdjustFallStatus);
			break;
		}
		default:
		{
			aiAssert(false);
			break;
		}
	}

	//Set the target
	m_PlayAttachedClipHelper.SetTarget(vTargetPosition, qTargetOrientation);
}

// Guess at the next phase for the currently playing clip based on the timestep.
float CTaskUseScenario::ComputeNextPhase(const CClipHelper& clipHelper, const crClip& clip) const
{
	float fCurrentPhase = clipHelper.GetPhase();
	float fNextPhase = MIN(fCurrentPhase + (clipHelper.GetRate() * clip.ConvertTimeToPhase(GetTimeStep())), 1.0f);
	return fNextPhase;
}

// Search the clip for Interruptible tags, and see if the phase supplied exceeds them.
bool CTaskUseScenario::CheckForInterruptibleTag(float fPhase, const crClip& clip) const
{
	crTag::Key uKey;
	crTagIterator it(*(clip.GetTags()));
	while(*it)
	{
		uKey = (*it)->GetKey();
		if(uKey == CClipEventTags::Interruptible.GetHash())
		{
			if (fPhase >= (*it)->GetStart())
			{
				return true;
			}
		}
		++it;
	}

	return false;
}

void CTaskUseScenario::Attach(const Vector3& vWorldPosition, const Quaternion& qWorldOrientation)
{
	u32 commonAttachFlags = ATTACH_FLAG_DONT_ASSERT_ON_NULL_ENTITY|ATTACH_FLAG_AUTODETACH_ON_RAGDOLL|ATTACH_FLAG_COL_ON;

	CPed* pPed = GetPed();

	//Check if the scenario is attached to a dynamic entity.
	if(IsDynamic())
	{
		//Get the position relative to the entity.
		Vector3 vRelativePosition = VEC3V_TO_VECTOR3(GetScenarioEntity()->GetTransform().UnTransform(VECTOR3_TO_VEC3V(vWorldPosition)));

		//Get the scenario heading relative to the entity.
		Vector3 vRelativeOrientation;
		qWorldOrientation.ToEulers(vRelativeOrientation, "xyz");
		float fRelativeHeading = fwAngle::LimitRadianAngle(vRelativeOrientation.z - GetScenarioEntity()->GetTransform().GetHeading());

		//Attach the ped to the entity.
		pPed->AttachPedToPhysical(static_cast<CPhysical *>(GetScenarioEntity()), -1, (u16) (ATTACH_STATE_PED|commonAttachFlags), &vRelativePosition, NULL, fRelativeHeading, 0.0f);
	}
	else
	{
		//Attach the ped to the world.
		pPed->AttachToWorldBasic((u32) (ATTACH_STATE_WORLD|commonAttachFlags), vWorldPosition, &qWorldOrientation);
	}
}

bool CTaskUseScenario::IsDynamic() const
{
	const CScenarioInfo& scenarioInfo = GetScenarioInfo();
	if (scenarioInfo.GetIsFlagSet(CScenarioInfoFlags::AlwaysAttachToWorld))
		return false;

	return (m_eScenarioMode == SM_EntityEffect && GetScenarioEntity() && GetScenarioEntity()->GetIsClass<CPhysical>());
}

void CTaskUseScenario::SetGoToAnimationTaskSettings(CTaskGoToScenario::TaskSettings& settings)
{
	//Ensure the enter clip set is valid.
	if(GetEnterClipSetId(GetPed(), this) == CLIP_SET_ID_INVALID)
	{
		return;
		
	}
	//Ensure the chosen clip is valid.
	if(GetChosenClipId(GetPed(), this) == CLIP_ID_INVALID)
	{
		return;
	}

	settings.SetClipDict(fwClipSetManager::GetClipDictionaryIndex(GetEnterClipSetId(GetPed(), this)));
	settings.SetClipHash(GetChosenClipId(GetPed(), this).GetHash());
	settings.Set(CTaskGoToScenario::TaskSettings::UsesEntryClip, true);
}

void CTaskUseScenario::DetermineConditionalAnims()
{
	if(m_ConditionalAnimsChosen < 0)
	{
		const CTask* pSubTask = GetSubTask();
		if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_AMBIENT_CLIPS)
		{
			m_ConditionalAnimsChosen = static_cast<const CTaskAmbientClips*>(pSubTask)->GetConditionalAnimChosen();
		}
	}
}

fwMvClipSetId CTaskUseScenario::DetermineEnterAnimations(CScenarioCondition::sScenarioConditionData& conditionData, const CConditionalClipSet** ppOutConClipSet, u32* const pOutPropSetHash)
{
	// Try to pick a coward entry animation.
	if (ShouldUseCowardEnter())
	{
		// Clear the flag so it isn't used again.
		m_iFlags.ClearFlag(SF_UseCowardEnter);

		// Force the condition data to be threatened.
		conditionData.eAmbientEventType = AET_Threatened;

		fwMvClipSetId retSet = ChooseConditionalClipSet(CConditionalAnims::AT_PANIC_OUTRO, conditionData, ppOutConClipSet, pOutPropSetHash);
		if (retSet != CLIP_SET_ID_INVALID)
		{
			// Success, return the select coward enter.
			return retSet;
		}
	}

	// Fall back to returning the normal enter.
	return ChooseConditionalClipSet(CConditionalAnims::AT_ENTER, conditionData, ppOutConClipSet, pOutPropSetHash);
}

void CTaskUseScenario::DetermineExitAnimations(bool bIsEventReaction)
{
	if(m_iExitClipsFlags.IsFlagSet(eExitClipsDetermined))
	{
		return;
	}

	DetermineConditionalAnims();

	if(SCENARIO_DEBUG_ONLY((m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData) && m_pDebugInfo->m_AnimType == CConditionalAnims::AT_EXIT) || ) (m_ConditionalAnimsChosen >= 0 && m_pConditionalAnimsGroup))
	{
		CScenarioCondition::sScenarioConditionData conditionData;
		InitConditions(conditionData);

		conditionData.m_bIsReaction = bIsEventReaction;

		const CConditionalClipSet* pExitClipSet = NULL;
		m_exitClipSetId = ChooseConditionalClipSet(CConditionalAnims::AT_EXIT, conditionData, &pExitClipSet);
		if (pExitClipSet)
		{
			m_iExitClipsFlags.ChangeFlag(eExitClipsAreSynchronized,pExitClipSet->GetIsSynchronized());
			m_iExitClipsFlags.ChangeFlag(eExitClipsEndsInWalk,pExitClipSet->GetEndsInWalk());
		}
	}
	m_iExitClipsFlags.ChangeFlag(eExitClipsDetermined,true);

}


void CTaskUseScenario::ResetExitAnimations()
{
	m_iExitClipsFlags.ChangeFlag(eExitClipsDetermined,false);
	m_iExitClipsFlags.ChangeFlag(eExitClipsAreSynchronized,false);
	m_iExitClipsFlags.ChangeFlag(eExitClipsEndsInWalk,false);

	m_exitClipSetId = CLIP_SET_ID_INVALID;
}

#if !__FINAL
void CTaskUseScenario::Debug() const
{
#if DEBUG_DRAW

	if((m_eScenarioMode == SM_EntityEffect || m_eScenarioMode == SM_WorldEffect)
			&& !GetScenarioPoint())
	{
		// In this case, GetScenarioPositionAndHeading() would fail (an assert),
		// so return to avoid that. It's a little unclear on what's actually causing
		// this, but perhaps it's from a call to Debug() on the same frame as
		// CleanUp() cleared the effect pointer.
		return;
	}

	//Get the scenario position and heading.
	Vector3 vWorldPosition;
	float fWorldHeading;
	GetScenarioPositionAndHeading(vWorldPosition, fWorldHeading);
	fWorldHeading += m_fBaseHeadingOffset;
	
	grcDebugDraw::Sphere(vWorldPosition, 0.025f, Color_red);

	Vector3 vDir(0.0f,1.0f,0.0f);
	vDir.RotateZ(fWorldHeading);
	grcDebugDraw::Line(vWorldPosition, vWorldPosition + vDir, Color_red, Color_blue);
	grcDebugDraw::Sphere(vWorldPosition + vDir, 0.025f, Color_blue);

	if (m_iScenarioReactionFlags.IsFlagSet(SREF_RespondingToEvent))
	{
		char tmp[256];
		formatf(tmp, "Handling response for event type %s"
			, m_iScenarioReactionFlags.IsFlagSet(SREF_AggroOrCowardAgitationReaction) ? "Disputes" : CEventNames::GetEventName(static_cast<eEventType>(m_iEventTriggeringPanic)));
		grcDebugDraw::Text(vWorldPosition, Color_red, 0, -grcDebugDraw::GetScreenSpaceTextHeight(), tmp);
	}

#endif
}

const char * CTaskUseScenario::GetStaticStateName( s32 iState  )
{
	taskAssert( iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:					return "State_Start";
	case State_GoToStartPosition: 		return "State_GoToStartPosition";
	case State_Enter:					return "State_Enter";
	case State_PlayAmbients:			return "State_PlayAmbients";
	case State_Exit:					return "State_Exit";
	case State_Finish:					return "State_Finish";
	case State_AggroOrCowardIntro:		return "State_AggroOrCowardIntro";
	case State_AggroOrCowardOutro:		return "State_AggroOrCowardOutro";
	case State_WaitingOnExit:			return "State_WaitingOnExit";
	case State_StreamWaitClip:			return "State_StreamWaitClip";
	case State_WaitForProbes:			return "State_WaitForProbes";
	case State_WaitForPath:				return "State_WaitForPath";
	case State_PanicExit:				return "State_PanicExit";
	case State_NMExit:					return "State_NMExit";
	case State_PanicNMExit:				return "State_PanicNMExit";
	case State_ScenarioTransition:		return "State_ScenarioTransition";
	default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif


u32 CTaskUseScenario::GetAmbientFlags()
{
	u32 ambientFlags = (CTaskAmbientClips::Flag_PlayBaseClip | CTaskAmbientClips::Flag_PlayIdleClips | CTaskScenario::GetAmbientFlags() );

	if (m_iFlags.IsFlagSet(SF_Warp) )
	{
		ambientFlags |= CTaskAmbientClips::Flag_InstantlyBlendInBaseClip;
	}
	if(m_iFlags.IsFlagSet(SF_SynchronizedMaster))
	{
		ambientFlags |= CTaskAmbientClips::Flag_SynchronizedMaster;

		// Not sure, maybe should find a better way to achieve this:
		ambientFlags |= CTaskAmbientClips::Flag_InstantlyBlendInBaseClip;
	}
	if(m_iFlags.IsFlagSet(SF_SynchronizedSlave))
	{
		ambientFlags |= CTaskAmbientClips::Flag_SynchronizedSlave;

		// Not sure, maybe should find a better way to achieve this:
		ambientFlags |= CTaskAmbientClips::Flag_InstantlyBlendInBaseClip;
	}

	return ambientFlags;
}


void CTaskUseScenario::GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const
{
	const CScenarioInfo& scenarioInfo = GetScenarioInfo();
	const u32 cameraHash = scenarioInfo.GetCameraNameHash();
	if(cameraHash > 0)
	{
		settings.m_CameraHash = cameraHash;
	}
}

void CTaskUseScenario::OverrideBaseClip(fwMvClipSetId clipSetId, fwMvClipId clipId)
{
	m_OverriddenBaseClipSet = clipSetId;
	m_OverriddenBaseClip = clipId;
}

void CTaskUseScenario::SetCanExitToChainedScenario()
{ 
	m_iFlags.SetFlag(SF_CanExitToChain); 
	m_PavementHelper.CleanupPavementFloodFillRequest();
}

void CTaskUseScenario::SetStationaryReactionsEnabledForPed(CPed& ped, bool enabled, const atHashWithStringNotFinal sReactHash)
{
	if(ped.IsPlayer())
	{
		return;
	}

	if(enabled)
	{
		// Use CPED_CONFIG_FLAG_CowerInsteadOfFlee to check if we've already enabled
		// the stationary reactions. If so, we intentionally don't touch the combat movement,
		// so other users have a chance to change it if they really want.
		if(!ped.GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee))
		{
			ped.SetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee, true);
			ped.GetPedIntelligence()->GetFleeBehaviour().SetStationaryReactHash(sReactHash);
			ped.GetPedIntelligence()->GetCombatBehaviour().SetCombatMovement(CCombatData::CM_Stationary);
		}
	}
	else
	{
		// Like in the enabled case above, check CPED_CONFIG_FLAG_CowerInsteadOfFlee first
		// and don't touch anything else if it's not already set. This prevents this function
		// from stomping on the combat movement set by other users, if not using a StationaryReactions
		// scenario.
		if(ped.GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee))
		{
			ped.SetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee, false);
			ped.GetPedIntelligence()->GetFleeBehaviour().SetStationaryReactHash(sReactHash);
			// Try to determine the default combat movement, i.e. setting it back to what
			// CCombatBehaviour::InitFromCombatData() would have set it to.
			CCombatData::Movement move = CCombatData::CM_WillAdvance;
			const CPedModelInfo* modelInfo = ped.GetPedModelInfo();
			if(modelInfo)
			{
				u32 iCombatInfoHash = modelInfo->GetCombatInfoHash();
				const CCombatInfo* pCombatInfo = CCombatInfoMgr::GetCombatInfoByHash(iCombatInfoHash);
				if(pCombatInfo)
				{
					move = pCombatInfo->GetCombatMovement();
				}
			}
			ped.GetPedIntelligence()->GetCombatBehaviour().SetCombatMovement(move);
		}
	}
}

bool CTaskUseScenario::SpheresOfInfluenceBuilder(TInfluenceSphere* apSpheres, int& iNumSpheres, const CPed* pPed)
{
	//Ensure the task is valid.
	CTaskUseScenario* pTask = static_cast<CTaskUseScenario *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
	if(!pTask)
	{
		return false;
	}

	return CTaskSmartFlee::SpheresOfInfluenceBuilderForTarget(apSpheres, iNumSpheres, pPed, pTask->m_Target);
}

void CTaskUseScenario::SetAdditionalTaskDuringApproach(const CTask* task)
{
	if(m_pAdditionalTaskDuringApproach)
	{
		delete m_pAdditionalTaskDuringApproach;
		m_pAdditionalTaskDuringApproach = NULL;
	}

	m_pAdditionalTaskDuringApproach = task;
}


void CTaskUseScenario::AggroOrCowardIntro_OnEnter()
{
#if SCENARIO_DEBUG
	if (m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData))
	{
		//we need to set this up as the code has been reorganized a bit to expect this to be to chosen and 
		// requested in the PanicIntro_OnUpdate function.
		//  Doing this allows the debug functionality to run again.
		CScenarioCondition::sScenarioConditionData conditionData;
		InitConditions(conditionData);
		m_panicBaseClipSetId = ChooseConditionalClipSet(CConditionalAnims::AT_PANIC_BASE, conditionData);
		
		if (m_iDebugFlags.IsFlagSet(SFD_SkipPanicIntro))
		{
			return;
		}
	}
#endif // SCENARIO_DEBUG

	CPed* pPed = GetPed();

	if(fwAnimManager::GetClipIfExistsBySetId(m_panicClipSetId, GetChosenClipId(GetPed(), this)))
	{

		if (!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee) && m_iScenarioReactionFlags.IsFlagSet(SREF_AggroOrCowardReaction))
		{
			//Update the target situation for the attached clip helper.
			UpdateTargetSituation();
			//Set the initial offsets.
			m_PlayAttachedClipHelper.SetInitialOffsets(*pPed);
		}

		int iClipPriority = GetExitClipPriority();
		StartClipBySetAndClip(pPed, m_panicClipSetId, GetChosenClipId(GetPed(), this), APF_ISBLENDAUTOREMOVE, 0, iClipPriority, NORMAL_BLEND_IN_DELTA, CClipHelper::TerminationType_OnDelete);
		RandomizeRateAndUpdateLastRate();

		CScenarioCondition::sScenarioConditionData conditionData;
		InitConditions(conditionData);
		m_panicBaseClipSetId = ChooseConditionalClipSet(CConditionalAnims::AT_PANIC_BASE, conditionData);
		Assertf(m_panicBaseClipSetId != CLIP_SET_ID_INVALID, "Panic base clipset was invalid.  Possible problem with scenarios.meta.  Scenario:  %s, Male: %d, AET:  %d, CowardAgitation: %d, AggroAgitation: %d, CowardReaction:  %d, Responding to event type:  %d, Conditional anims:  %s", 
			GetScenarioInfo().GetName(), pPed->IsMale(), conditionData.eAmbientEventType, m_iScenarioReactionFlags.IsFlagSet(SREF_CowardAgitation), m_iScenarioReactionFlags.IsFlagSet(SREF_AggroAgitation), 
			m_iScenarioReactionFlags.IsFlagSet(SREF_CowardReaction), m_iEventTriggeringPanic, m_pConditionalAnimsGroup && m_ConditionalAnimsChosen >= 0 ? m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimsChosen)->GetName() : "NULL");

#if SCENARIO_DEBUG
		if (m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData) && m_pDebugInfo->m_AnimType == CConditionalAnims::AT_PANIC_INTRO)
		{
			m_iDebugFlags.SetFlag(SFD_WantNextDebugAnimData);
		}
#endif // SCENARIO_DEBUG
	}
	else
	{
		taskAssertf(false,"Scenario panic clip not found %s\n",PrintClipInfo(m_panicClipSetId,GetChosenClipId(GetPed(), this)));
	}

	// Let the ped push other peds around.
	TogglePedCapsuleCollidesWithInactives(true);
}

// Tell the ped using the scenario to look at the target entity/position for a specified amount of time.
void CTaskUseScenario::LookAtTarget(float fTime)
{
	//Look at the target.
	// Tell the ped to look at the position the ambient event occurred.
	CPed* pPed = GetPed();
	if (m_Target.GetIsValid())
	{
		u32 uTime = rage::round(fTime) * 1000;
		u32 uFlags = LF_WHILE_NOT_IN_FOV;
		s32 iBlendInTime = 500;
		s32 iBlendOutTime = 500;
		CIkManager::eLookAtPriority nLookAtPriority = CIkManager::IK_LOOKAT_HIGH;
		const CEntity* pEntity = m_Target.GetEntity();
		if (pEntity && pEntity->GetIsTypePed())
		{
			pPed->GetIkManager().LookAt(0, pEntity,
				uTime, BONETAG_HEAD, NULL, uFlags, iBlendInTime, iBlendOutTime, nLookAtPriority);
		}
		else if (pEntity && pEntity->GetIsTypeVehicle())
		{
			pPed->GetIkManager().LookAt(0, pEntity,
				uTime, BONETAG_INVALID, NULL, uFlags, iBlendInTime, iBlendOutTime, nLookAtPriority);	
		}
		else
		{
			Vector3 vLookPos;
			m_Target.GetPosition(vLookPos);
			pPed->GetIkManager().LookAt(0, NULL,
				uTime, BONETAG_INVALID, &vLookPos, uFlags, iBlendInTime, iBlendOutTime, nLookAtPriority);
		}
	}
}

void CTaskUseScenario::HandleAnimatedTurns()
{
	const s32 iState = GetState();
	CPed* pPed = GetPed();
	if (iState == State_Exit || iState == State_PanicExit || iState == State_Enter || iState == State_AggroOrCowardIntro || iState == State_AggroOrCowardOutro)
	{
		// Block animated turns.
		pPed->SetDesiredHeading(pPed->GetCurrentHeading());
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableMoveTaskHeadingAdjustments, true);

		if (iState == State_Exit || iState == State_PanicExit || iState == State_AggroOrCowardIntro || iState == State_WaitForPath)
		{
			// Force the idle state.
			pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle);
		}
	}
}

// Toggle fixed physics on the scenario entity (if it exists).
void CTaskUseScenario::ToggleFixedPhysicsForScenarioEntity(bool bFixed)
{
	CEntity* pEntity = GetScenarioEntity();
	if (pEntity)
	{
		// A scenario entity exists.
		if (pEntity->GetIsTypeObject())
		{
			// An object type scenario entity exists.
			CObject* pObject = static_cast<CObject*>(pEntity);
			bool bObjectIsCurrentlyFixed = pObject->GetIsAnyFixedFlagSet();
			if ((bObjectIsCurrentlyFixed && pObject->m_nObjectFlags.bFixedForSmallCollisions) || (bFixed && !bObjectIsCurrentlyFixed))
			{
				// Do not turn off fixed physics unless the fixed for small collisions flag is set.
				// Do not turn on fixed for small collisions flag unless fixed physics is off.
				pObject->m_nObjectFlags.bFixedForSmallCollisions = bFixed;
				pObject->SetFixedPhysics(bFixed);
			}

			pObject->SetFixedByScriptOrCode(bFixed);
		}
	}
}

// Bound offset helper function.
void CTaskUseScenario::ManagePedBoundForScenario(bool bInitial)
{
	// Cache off the ped.
	CPed* pPed = GetPed();

	Vec3V vOffset = VECTOR3_TO_VEC3V(GetScenarioInfo().GetCapsuleOffset());

	// Make sure that the scenario offset is substantial enough to bother with.
	if (IsLessThanAll(Dot(vOffset, vOffset), ScalarV(VERY_SMALL_FLOAT)))
	{
		return;
	}

	// Make sure that the ped has a composite bound.
	if (!Verifyf(pPed->GetAnimatedInst() && pPed->GetAnimatedInst()->GetArchetype() && pPed->GetAnimatedInst()->GetArchetype()->GetBound() && 
		pPed->GetAnimatedInst()->GetArchetype()->GetBound()->GetType() == phBound::COMPOSITE, "Ped did not have a valid composite bound!"))
	{
		return;
	}


	phBoundComposite* pComposite = static_cast<phBoundComposite*>(pPed->GetAnimatedInst()->GetArchetype()->GetBound());

	// Make sure that slot CBaseCapsuleInfo::BOUND_MAIN_CAPSULE on the composite bound is indeed the main capsule.
	if (!Verifyf(pComposite->GetBound(CBaseCapsuleInfo::BOUND_MAIN_CAPSULE) 
		&& pComposite->GetBound(CBaseCapsuleInfo::BOUND_MAIN_CAPSULE)->GetType() == phBound::CAPSULE, "Ped did not have a valid capsule bound in slot 0!"))
	{
		return;
	}

	// If altering the ped's capsule in some way, note this so it gets restored.
	m_iFlags.ChangeFlag(SF_CapsuleNeedsToBeRestored, bInitial);

	// Move forward or backwards based on the input parameter.
	if (bInitial)
	{
		//Tell the ped what offset to use
		pPed->SetDesiredBoundOffset(RCC_VECTOR3(vOffset));
		pPed->ResetProcessBoundsCountdown();

		if (Verifyf(m_uCachedBoundsFlags == 0, "About to cache off the ped bounds include flags, but there was a previous value already stored there."))
		{
			// Remember what the bound capsule include flags were prior to changing them below.
			m_uCachedBoundsFlags = pComposite->GetIncludeFlags(CBaseCapsuleInfo::BOUND_MAIN_CAPSULE);

			Assertf(m_uCachedBoundsFlags != 0, "A ped used a scenario with a BOUND_MAIN_CAPSULE type include mask of 0, does this make sense?");

			// If pushing the bound away, have it only collide against peds.
			pComposite->SetIncludeFlags(CBaseCapsuleInfo::BOUND_MAIN_CAPSULE, ArchetypeFlags::GTA_PED_TYPE);
		}
	}
	else
	{
		if (Verifyf(m_uCachedBoundsFlags != 0, "About to restore the cached off ped bounds include flags, but the cached flags were 0.  Does this make sense?"))
		{
			// Restore the bound capsule include flags to what they were previously.
			pComposite->SetIncludeFlags(CBaseCapsuleInfo::BOUND_MAIN_CAPSULE, m_uCachedBoundsFlags);

			// Reset the bounds flag cache.
			m_uCachedBoundsFlags = 0;
		}

		//Remove the offset we had.
		Vector3 vZero;
		vZero.Zero();
		pPed->SetDesiredBoundOffset(vZero);
		pPed->ResetProcessBoundsCountdown();
	}
}

#if !HACK_RDR3

// DO NOT INTEGRATE THIS CODE INTO PROJECTS OTHER THAN GTAV!

// Should we let the ped exit to the event, even though stationary reactions are enabled.
// If they respond with combat, this will trigger stationary mode.
// See B* 1745645
bool CTaskUseScenario::StationaryReactionsHack()
{
	// Possible that this doesn't exist yet as we only validate it after ProcessPreFSM() is called.
	const CScenarioInfo* pInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(GetScenarioType());
	if (!pInfo)
	{
		return false;
	}

	// Only the binoculars type.
	if (pInfo->GetHash() != ATSTRINGHASH("WORLD_HUMAN_BINOCULARS", 0x9663f747))
	{
		return false;
	}

	// Military base
	// Coords taken from sp_manifest.meta.
	const spdAABB militaryBaseAABB(Vec3V(-3533.733154f, 2766.466309f, -93.703957f), Vec3V(-1591.284180f, 3853.811279f, 149.070206f));

	// Only on the military base for now.
	if (militaryBaseAABB.ContainsPoint(GetPed()->GetTransform().GetPosition()))
	{
		return true;
	}

	// Otherwise act normally.
	return false;
}

#endif

// Mark this ped for destruction if applicable
void CTaskUseScenario::FlagToDestroyPedIfApplicable()
{
	CPed* pPed = GetPed();
	if (pPed)
	{
		// If a network object, the ped must be deletable
		if (pPed->GetNetworkObject() && !pPed->GetNetworkObject()->CanDelete())
		{
			return;
		}

		// We shouldn't be calling this on a player ped
		if (!pPed->IsAPlayerPed())
		{
			// 1) If this ped isn't owned by us, don't mark it for deletion
			// 2) If this ped was populated by a mission, let it handle its own cleanup. [11/26/2012 mdawe]
			// 3) FlagToDestroyWhenNextProcessed will assert if the ped is a NetworkClone
			if (pPed->IsControlledByLocalAi() && !pPed->PopTypeIsMission() && !pPed->IsNetworkClone())
			{
				pPed->FlagToDestroyWhenNextProcessed();
			}
		}
	}
}


CTask::FSM_Return CTaskUseScenario::AggroOrCowardIntro_OnUpdate()
{
	if (CheckAttachStatus())
	{
		if (RagdollOutOfScenario())
		{
			return FSM_Quit;
		}
	}

	CPed* pPed = GetPed();
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostMovementTimeSliced, true);

	// Do head IK if responding to an event.
	LookAtTarget(1.0f);

	// Continue to stream in the panic base clip.
	if (m_panicBaseClipSetId != CLIP_SET_ID_INVALID)
	{
		m_ClipSetRequestHelper.RequestClipSetIfExists(m_panicBaseClipSetId);
		OverrideStreaming();
	}

	bool bClipDonePlaying = !GetClipHelper() || GetClipHelper()->IsHeldAtEnd() || !GetClipHelper()->GetClip();
	
	if (!bClipDonePlaying)
	{
		CClipHelper* pClipHelper = GetClipHelper();
		const crClip* pClip = pClipHelper->GetClip();


		if (EndBecauseOfFallTagging())
		{
			bClipDonePlaying = true;
		}

		// Grab the phase.
		float fNextPhase = ComputeNextPhase(*pClipHelper, *pClip);

		// Check against the threshold for exiting and the interruptible tag.
		if (CheckForInterruptibleTag(fNextPhase, *pClip) || fNextPhase >= GetTunables().m_RegularExitDefaultPhaseThreshold)
		{
			bClipDonePlaying = true;
		}
	}

	SCENARIO_DEBUG_ONLY(bool bPanicBaseStreamed = m_panicBaseClipSetId == CLIP_SET_ID_INVALID || m_ClipSetRequestHelper.IsClipSetStreamedIn());
	if (bClipDonePlaying SCENARIO_DEBUG_ONLY(&& (!m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData) || bPanicBaseStreamed)))
	{
		// Restore the collision state.
		TogglePedCapsuleCollidesWithInactives(false);
#if SCENARIO_DEBUG
		if (m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData))
		{
			if (m_iDebugFlags.IsFlagSet(SFD_SkipPanicIdle))
			{
				//This may look incorrect but what we are doing is skipping the clip playing not the state
				// the state AT_PANIC_OUTRO is the next valid item in the chain so need to setup the clips
				//	properly to let this debug clip play as expected.
				aiAssert(m_pDebugInfo->m_AnimType == CConditionalAnims::AT_PANIC_OUTRO);
				SetState(State_AggroOrCowardOutro);
				return FSM_Continue;
			}
			else
			{
				fwMvClipId chosenClipId = CLIP_ID_INVALID;
				switch (m_pDebugInfo->m_AnimType)
				{
				case CConditionalAnims::AT_PANIC_BASE:
					// proceed as that is the next thing to play ... 
					m_panicClipSetId = m_panicBaseClipSetId;
					if (ChooseClipInClipSet(CConditionalAnims::AT_PANIC_BASE, m_panicClipSetId, chosenClipId) == CScenarioClipHelper::ClipCheck_Success)
					{
						SetChosenClipId(chosenClipId, GetPed(), this);
					}
					SetState(State_WaitingOnExit);
					return FSM_Continue;
				case CConditionalAnims::AT_COUNT:
					//exit case ... 
					SetState(State_Finish);
					return FSM_Continue;
				default:
					//go back to start
					SetState(State_Start);
					return FSM_Continue;
				};
			}
		}
#endif // SCENARIO_DEBUG

		if (IsRespondingToEvent())
		{
			// If an event has been specified, exit to respond to it.
			m_iScenarioReactionFlags.SetFlag(SREF_AllowEventsToKillThisTask);
		}
		else
		{
			// Otherwise finish out.
			SetState(State_Finish);
		}
	}
	return FSM_Continue;
}

void CTaskUseScenario::AggroOrCowardIntro_OnExit()
{
	if (false SCENARIO_DEBUG_ONLY(|| m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData)))
	{
		CPed* pPed = GetPed();

		CTaskMotionBase* pMotionTask = pPed->GetPrimaryMotionTaskIfExists();

		// Set the motion state based on the clipset stored in the panic idle slot.
		// Won't actually be playing it, just using it as the on foot clip set.
		if (pMotionTask && (m_panicBaseClipSetId != CLIP_SET_ID_INVALID) && m_ClipSetRequestHelper.IsClipSetStreamedIn())
		{
			pMotionTask->SetOnFootClipSet(m_panicBaseClipSetId);
		}
	}
}

void CTaskUseScenario::WaitingOnExit_OnEnter()
{
	if (false SCENARIO_DEBUG_ONLY(|| m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData)))
	{
#if SCENARIO_DEBUG
		CPed* pPed = GetPed();
		if (pPed)
		{
			if (m_iDebugFlags.IsFlagSet(SFD_SkipPanicIdle))
			{
				// dont run any clips ... just get to the onUpdate where the proper clips are streamed and executed.
				return;
			}
			if(fwAnimManager::GetClipIfExistsBySetId(m_panicClipSetId, GetChosenClipId(pPed, this)))
			{
				float fBlend = m_iScenarioReactionFlags.IsFlagSet(SREF_AggroOrCowardReaction) ? SLOW_BLEND_IN_DELTA : NORMAL_BLEND_IN_DELTA;
				StartClipBySetAndClip(pPed, m_panicClipSetId, GetChosenClipId(pPed, this), fBlend, CClipHelper::TerminationType_OnDelete);
				RandomizeCowerPanicRateAndUpdateLastRate(); 
			}

			if (m_pDebugInfo->m_AnimType == CConditionalAnims::AT_PANIC_BASE)
			{
				m_iDebugFlags.SetFlag(SFD_WantNextDebugAnimData);
			}
		}
#endif // SCENARIO_DEBUG
	}

	if (m_panicBaseClipSetId != CLIP_SET_ID_INVALID)
	{
		// Adjust the ped bounds only if using in place cowers.
		ManagePedBoundForScenario(true);
	}

	// Once all the existing exits for this ped have failed, disable disputes on this ped, provided they aren't cowering for some other reason besides 
	// disputes.
	if (!IsRespondingToEvent() && m_iScenarioReactionFlags.IsFlagSet(SREF_AggroOrCowardAgitationReaction))
	{
		ForgetAboutAgitation();
	}
}

CTask::FSM_Return	CTaskUseScenario::WaitingOnExit_OnUpdate()
{
	if (CheckAttachStatus())
	{
		if (RagdollOutOfScenario())
		{
			return FSM_Quit;
		}
	}

	if (false SCENARIO_DEBUG_ONLY(|| m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData)))
	{
#if SCENARIO_DEBUG
		if (!GetClipHelper() || (GetTimeInState() > GetClipHelper()->GetDuration() && m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData)))
		{
			switch (m_pDebugInfo->m_AnimType)
			{
			case CConditionalAnims::AT_PANIC_OUTRO:
				// proceed as that is the next thing to play ... 
				break;
			case CConditionalAnims::AT_COUNT:
				//exit case ... 
				m_iScenarioReactionFlags.ClearFlag(SREF_RespondingToEvent);
				m_iScenarioReactionFlags.ClearFlag(SREF_ExitingToFlee);
				m_iDebugFlags.ClearFlag(SFD_SkipPanicIntro);
				m_iDebugFlags.ClearFlag(SFD_SkipPanicIdle);
				SetState(State_Finish);
				return FSM_Continue;
				break;
			default:
				//go back to start
				m_iScenarioReactionFlags.ClearFlag(SREF_RespondingToEvent);
				m_iScenarioReactionFlags.ClearFlag(SREF_ExitingToFlee);
				m_iDebugFlags.ClearFlag(SFD_SkipPanicIntro);
				m_iDebugFlags.ClearFlag(SFD_SkipPanicIdle);
				SetState(State_Start);
				return FSM_Continue;
			};
			CScenarioCondition::sScenarioConditionData conditionData;
			InitConditions(conditionData);
			m_panicClipSetId = ChooseConditionalClipSet(CConditionalAnims::AT_PANIC_OUTRO, conditionData);
			if (m_panicClipSetId != CLIP_SET_ID_INVALID)
			{
				fwMvClipId chosenClipId = CLIP_ID_INVALID;
				if (ChooseClipInClipSet(CConditionalAnims::AT_PANIC_OUTRO, m_panicClipSetId, chosenClipId) == CScenarioClipHelper::ClipCheck_Success)
				{
					SetChosenClipId(chosenClipId, GetPed(), this);
				}
				else
				{
					aiAssertf(false,"Could not pick animation for panic outro ClipSetId:  %s, Conditional Anims Group: %s", m_panicClipSetId.GetCStr(), GetScenarioInfo().GetConditionalAnimsGroup().GetName());
				}				
			}
			if ( m_panicClipSetId != CLIP_SET_ID_INVALID )
			{
				m_ClipSetRequestHelper.RequestClipSetIfExists(m_panicClipSetId);
				if (m_ClipSetRequestHelper.IsClipSetStreamedIn())
				{
					// Check with the blackboard to see if we can play this clip right now
					CAnimBlackboard::AnimBlackboardItem blackboardItem(m_panicClipSetId, GetChosenClipId(GetPed(), this), CAnimBlackboard::REACTION_ANIM_TIME_DELAY);
					if (sm_bIgnoreBlackboardForScenarioReactions || ANIMBLACKBOARD.AddItemToBlackboardAndPedIntelligenceIfAllowedToPlay(blackboardItem, GetPed()->GetPedIntelligence()))
					{
						SetState(State_AggroOrCowardOutro);
					}
				}
			}
		}

#endif // SCENARIO_DEBUG
	}
	else
	{

		CScenarioPoint* pScenarioPoint = GetScenarioPoint();
		bool bDoStationaryReactions = (pScenarioPoint && pScenarioPoint->IsFlagSet(CScenarioPointFlags::StationaryReactions))
			|| m_iScenarioReactionFlags.IsFlagSet(SREF_ScriptedCowerInPlace);

		if(GetPed()->IsNetworkClone())
		{
			bDoStationaryReactions |=  mbNewTaskMigratedWhileCowering;
		}

		if (!bDoStationaryReactions)
		{
			if (!IsRespondingToEvent() && !m_iScenarioReactionFlags.IsFlagSet(SREF_AggroOrCowardAgitationReaction))
			{
				// Not responding to an event.  So, the reason this ped wants to stop cowering is because a scripter said to stop through the trigger function.
				// So attempt to cause the subtask to exit.
				if (!GetSubTask() || GetSubTask()->MakeAbortable(aiTask::ABORT_PRIORITY_URGENT, NULL))
				{
					// The task is stopped, resume playing ambients.
					m_ExitHelper.Reset();
					m_ExitHelper.ResetPreviousWinners();
					SetState(State_PlayAmbients);
				}
			}
			else
			{
				// Either:
				// We are a mission ped.
				// OR
				// Enough time has passed to retry the exit.  Maybe the ped got lucky and it can get up now.
				bool bCanLeaveToOtherState = GetPed()->PopTypeIsMission() || GetTimeInState() >= GetTunables().m_TimeBetweenChecksToLeaveCowering;
				if (bCanLeaveToOtherState)
				{	
					if (GetSubTask())
					{
						SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
					}
					SetState(State_WaitForProbes);
				}
			}
		}
	}

	return FSM_Continue;
}

void CTaskUseScenario::WaitingOnExit_OnExit()
{
	mbNewTaskMigratedWhileCowering = false;

	// Restore the ped capsule.
	if (m_iFlags.IsFlagSet(SF_CapsuleNeedsToBeRestored))
	{
		ManagePedBoundForScenario(false);
	}
}

void CTaskUseScenario::AggroOrCowardOutro_OnEnter()
{
	if(fwAnimManager::GetClipIfExistsBySetId(m_panicClipSetId, GetChosenClipId(GetPed(), this)))
	{
		CPed* pPed = GetPed();
		
		if (!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee) && m_iScenarioReactionFlags.IsFlagSet(SREF_AggroOrCowardReaction))
		{
			//Update the target situation for the attached clip helper.
			UpdateTargetSituation();
			//Set the initial offsets.
			m_PlayAttachedClipHelper.SetInitialOffsets(*pPed);
		}

		StartClipBySetAndClip(pPed, m_panicClipSetId, GetChosenClipId(GetPed(), this), NORMAL_BLEND_IN_DELTA, CClipHelper::TerminationType_OnDelete);
		RandomizeCowerPanicRateAndUpdateLastRate(); 
		m_panicClipSetId = CLIP_SET_ID_INVALID;
	
#if SCENARIO_DEBUG
		if (m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData) && m_pDebugInfo->m_AnimType == CConditionalAnims::AT_PANIC_OUTRO)
		{
			m_iDebugFlags.SetFlag(SFD_WantNextDebugAnimData);
		}
#endif // SCENARIO_DEBUG
	}
}

CTask::FSM_Return CTaskUseScenario::AggroOrCowardOutro_OnUpdate()
{
	CPed* pPed = GetPed();
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostMovementTimeSliced, true);

	if (IsRespondingToEvent() || m_iScenarioReactionFlags.IsFlagSet(SREF_AggroOrCowardAgitationReaction))
	{
		KeepNavMeshTaskAlive();
	}

	if (!GetClipHelper() || (GetClipHelper() && GetClipHelper()->IsHeldAtEnd()))
	{
		m_iFlags.ClearFlag(SF_ShouldAbort);
		if (IsRespondingToEvent())
		{
			EndEventReact();
		}

#if SCENARIO_DEBUG
		if (m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData))
		{
			switch (m_pDebugInfo->m_AnimType)
			{
			case CConditionalAnims::AT_COUNT:
				//exit case ... 
				m_iScenarioReactionFlags.ClearFlag(SREF_RespondingToEvent);
				m_iScenarioReactionFlags.ClearFlag(SREF_ExitingToFlee);
				m_iDebugFlags.ClearFlag(SFD_SkipPanicIntro);
				m_iDebugFlags.ClearFlag(SFD_SkipPanicIdle);
				SetState(State_Finish);
				return FSM_Continue;
				break;
			default:
				//go back to start
				m_iScenarioReactionFlags.ClearFlag(SREF_RespondingToEvent);
				m_iScenarioReactionFlags.ClearFlag(SREF_ExitingToFlee);
				m_iDebugFlags.ClearFlag(SFD_SkipPanicIntro);
				m_iDebugFlags.ClearFlag(SFD_SkipPanicIdle);
				SetState(State_Start);
				return FSM_Continue;
			};
		}
#endif // SCENARIO_DEBUG

		SetState(State_PlayAmbients);
	}
	return FSM_Continue;
}

void CTaskUseScenario::AggroOrCowardOutro_OnExit()
{
	// Desired heading can be changed by AggroOrCowardIdle, reset it here after playing the animation.
	CPed* pPed = GetPed();
	pPed->SetDesiredHeading(pPed->GetCurrentHeading());
}

CScenarioClipHelper::ScenarioClipCheckType CTaskUseScenario::PickClipFromSet(const CPed* pPed, fwMvClipSetId clipSetId, fwMvClipId& chosenClipId, 
																			 bool bRotateToTarget, bool bDoProbeChecks, bool bForceOneFrameCheck)
{
	CScenarioClipHelper::ScenarioClipCheckType iRetval = m_ExitHelper.PickClipFromSet(pPed, GetScenarioEntity(), m_Target, clipSetId, 
		chosenClipId, bRotateToTarget, bDoProbeChecks, bForceOneFrameCheck, false, GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::IgnoreWorldGeometryDuringExitProbeCheck),
		GetScenarioInfo().GetProbeHelperFinalZOverride(), GetScenarioInfo().GetProbeHelperPedCapsuleRadiusOverride());
	
	// Once we have finished searching, reset the clip finder so it is ready for the next batch of tests on a new clipset.
	if (iRetval != CScenarioClipHelper::ClipCheck_Pending)
	{
		m_ExitHelper.Reset();

		// If the ped doesn't care about doing exit probing, then reset the previous winners here.
		// Otherwise, the ped may eventually exhaust possible clips from repeated calls to this function (which will happen due to the CAnimBlackboard system).
		if (!bDoProbeChecks)
		{
			m_ExitHelper.ResetPreviousWinners();
		}
	}

	return iRetval;
}

CScenarioClipHelper::ScenarioClipCheckType CTaskUseScenario::PickReEnterClipFromSet(const CPed* pPed, fwMvClipSetId clipSetId, fwMvClipId& chosenClipId)
{
	CAITarget aTarget;
	Vector3 vPosition;
	float fHeading;
	GetScenarioPositionAndHeading(vPosition, fHeading);
	aTarget.SetPosition(vPosition);

	const bool bValidate = true;
	const bool bRotate = true;
	const bool bCheckFullLengthOfAnimation = true;

	// Try to pick a clip that doesn't collide with anything.
	CScenarioClipHelper::ScenarioClipCheckType iRetval = m_ExitHelper.PickClipFromSet(pPed, GetScenarioEntity(), aTarget, clipSetId, chosenClipId, bRotate, bValidate, true, bCheckFullLengthOfAnimation);
	m_ExitHelper.ResetPreviousWinners();
	m_ExitHelper.Reset();

	if (iRetval != CScenarioClipHelper::ClipCheck_Success)
	{
		// If all the clips failed, default to picking the forward clip.
		m_ExitHelper.PickFrontFacingClip(clipSetId, chosenClipId);
		
		if (chosenClipId != CLIP_ID_INVALID)
		{
			iRetval = CScenarioClipHelper::ClipCheck_Success;
		}
	}

	return iRetval;
}


void CTaskUseScenario::PickEntryClipIfNeeded()
{
	// If we've yet to choose an clip index from our entry set then we should attempt to do so.
	if(GetChosenClipId(GetPed(), this) == CLIP_ID_INVALID && GetEnterClipSetId(GetPed(), this) != CLIP_SET_ID_INVALID && GetState() <= State_Enter)
	{
		if ( GetEnterClipSetId(GetPed(), this) != CLIP_SET_ID_INVALID )
		{
			m_EnterClipSetRequestHelper.Request(GetEnterClipSetId(GetPed(), this));

			if ( m_EnterClipSetRequestHelper.IsLoaded())
			{
				fwMvClipId chosenClipId = CLIP_ID_INVALID;
				if (ChooseClipInClipSet(CConditionalAnims::AT_ENTER, GetEnterClipSetId(GetPed(), this), chosenClipId) == CScenarioClipHelper::ClipCheck_Success)
				{
					SetChosenClipId(chosenClipId, GetPed(), this);
				}
				else
				{
					aiAssertf(false,"Unable to choose clip");
				}
			}
		}
	}
}

namespace
{
	struct sFindPropInEnvironmentCBData
	{
		sFindPropInEnvironmentCBData()
				: m_ConditionalAnimsGroup(NULL)
				, m_BestObjectFound(NULL)
				, m_BestObjectFoundDistSq(FLT_MAX)
				, m_AllowUprooted(false)
		{}

		Vec3V							m_Center;
		const CConditionalAnimsGroup*	m_ConditionalAnimsGroup;
		CObject*						m_BestObjectFound;
		ScalarV							m_BestObjectFoundDistSq;
		bool							m_AllowUprooted;
	};
}

CObject* CTaskUseScenario::FindPropInEnvironment() const
{
	// If we are going to warp, it will probably be fine to use a prop that has been
	// uprooted, since we can just warp the prop back to a good position. If we are not
	// going to work, we would have to be much more careful about props that have been
	// moved.
	// Note: while this will allow us to find uprooted props, there may still be
	// a possibility that these props will be considered to be in the way when checking
	// for collisions at the scenario point. May need to add some sort of exception
	// elsewhere to deal with that case, or just add code for resetting these props
	// fairly aggressively.
	bool allowUprooted = m_iFlags.IsFlagSet(SF_Warp);

	// Look for a prop we can use, either near the ped's position, if that's where
	// we are going to do the scenario, or near the scenario point.
	if(m_iFlags.IsFlagSet(CTaskUseScenario::SF_StartInCurrentPosition))
	{
		const CPed* pPed = GetPed();
		const Vec3V scanPosV = pPed->GetTransform().GetPosition();
		return FindPropInEnvironmentNearPosition(scanPosV, m_iScenarioIndex, allowUprooted);
	}
	else
	{
		const CScenarioPoint* pt = GetScenarioPoint();
		if(pt)
		{
			return FindPropInEnvironmentNearPoint(*pt, m_iScenarioIndex, allowUprooted);
		}
	}
	return NULL;
}


bool CTaskUseScenario::FindPropInEnvironmentCB(CEntity* pEntity, void* pData)
{
	sFindPropInEnvironmentCBData& callbackData = *reinterpret_cast<sFindPropInEnvironmentCBData*>(pData);

	// Note: maybe we should use the center of gravity or something here, the local origin
	// of objects may actually frequently be on a corner of the object, so we might find an object
	// that's not really the closest.
	const Vec3V entityPosV = pEntity->GetTransform().GetPosition();

	const ScalarV distSqV = DistSquared(entityPosV, callbackData.m_Center);

	if(IsGreaterThanAll(distSqV, callbackData.m_BestObjectFoundDistSq))
	{
		return true;
	}

	if(!pEntity->GetIsTypeObject())
	{
		return true;
	}

	CObject* pObj = static_cast<CObject*>(pEntity);

	if(pObj->GetIsAttached())
	{
		return true;
	}

	if(!callbackData.m_AllowUprooted && pObj->m_nObjectFlags.bHasBeenUprooted)
	{
		return true;
	}

	// TODO: Maybe check other conditions here:
	// - owner

	const CConditionalAnimsGroup* pGrp = callbackData.m_ConditionalAnimsGroup;
	taskAssert(pGrp);	// If we don't have one of these, we probably shouldn't have done any search to begin with.

	const int numAnims = pGrp->GetNumAnims();

	const u32 propModelHash = pEntity->GetBaseModelInfo()->GetModelNameHash();

	bool found = false;
	for(int i = 0; i < numAnims; i++)
	{
		// Note: may be worth caching off the CAmbientModelSet pointers, or even a list of
		// usable prop model hashes, inside sFindPropInEnvironmentCBData. Then again, we probably
		// won't find all that many CObjects within the radius we are looking.
		const u32 propSetHash = pGrp->GetAnims(i)->GetPropSetHash();
		if(!propSetHash)
		{
			continue;
		}
		const CAmbientModelSet* pPropSet = CAmbientAnimationManager::GetPropSetFromHash(propSetHash);
		if(!pPropSet)
		{
			continue;
		}
		if(pPropSet->GetContainsModel(propModelHash))
		{
			found = true;
			break;
		}
	}
	if(!found)
	{
		return true;
	}

	callbackData.m_BestObjectFound = pObj;
	callbackData.m_BestObjectFoundDistSq = distSqV;

	return true;
}


void CTaskUseScenario::DisposeOfPropIfInappropriate()
{
#if SCENARIO_DEBUG
	if(m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData))
	{
		// Probably best to not drop anything if in debug mode.
		return;
	}
#endif // SCENARIO_DEBUG

	// Drop or destroy the prop.
	CPropManagementHelper::DisposeOfPropIfInappropriate(*GetPed(), m_iScenarioIndex, m_pPropHelper);
}

// Chooses and streams in clip sets for panic to start
void CTaskUseScenario::HandlePanicSetup()
{
	DetermineConditionalAnims();

	CPed* pPed = GetPed();
	if ( m_panicClipSetId == CLIP_SET_ID_INVALID )
	{
		CScenarioCondition::sScenarioConditionData conditionData;
		InitConditions(conditionData);

		// Scenario animation debug takes highest priority.
#if SCENARIO_DEBUG
		if (m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData) && !m_iScenarioReactionFlags.IsFlagSet(SREF_ExitingToFlee))
		{
			m_panicClipSetId = ChooseConditionalClipSet(CConditionalAnims::AT_PANIC_INTRO, conditionData);
	
			fwMvClipId chosenClipId = CLIP_ID_INVALID;
			if(ChooseClipInClipSet(CConditionalAnims::AT_PANIC_INTRO, m_panicClipSetId, chosenClipId) == CScenarioClipHelper::ClipCheck_Success)
			{
				SetChosenClipId(chosenClipId, GetPed(), this);
			}

			if (m_panicClipSetId == CLIP_SET_ID_INVALID && m_iDebugFlags.IsFlagSet(SFD_SkipPanicIntro))
			{
				//This may look incorrect but what we are doing is skipping the clip playing not the state
				// the state AT_PANIC_BASE or AT_PANIC_OUTRO is a valid item in the panic chain so need to setup the clips
				//	properly to let this debug clip play as expected.
				aiAssert(m_pDebugInfo->m_AnimType == CConditionalAnims::AT_PANIC_OUTRO || m_pDebugInfo->m_AnimType == CConditionalAnims::AT_PANIC_BASE);
			}
			SetState(State_AggroOrCowardIntro);
			return;
		}
#endif // SCENARIO_DEBUG

		// Check for the use of NM panic exits.
		if (GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::RagdollPanicExit))
		{
			SetState(State_PanicNMExit);
			return;
		}

		// Check for the use of NM exits.
		if (GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::UseNMGetupWhenExiting))
		{
			SetState(State_NMExit);
			return;
		}

		if ((m_iScenarioReactionFlags.IsFlagSet(SREF_AggroOrCowardAgitationReaction)) && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee))
		{
			// Never do an agitation reaction if this is a stationary point.
			ForgetAboutAgitation();
			return;
		}
		else
		{
			if (m_iFlags.IsFlagSet(SF_ExitToScriptCommand) && !m_iScenarioReactionFlags.IsFlagSet(SREF_DirectedScriptExit))
			{
				// For script command exits just leave normally.
				LeaveScenario();
			}
			else
			{
				// Keep the base animations going during the exit.
				SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);

				// Any other kind of reaction, move to the wait state.
				if (ShouldStreamWaitClip())
				{
					SetState(State_StreamWaitClip);
				}
				else
				{
					SetState(State_WaitForProbes);
				}
			}
		}
	}
}

void CTaskUseScenario::ForgetAboutAgitation()
{
	CPed* pPed = GetPed();

	// Ped was trying to leave for disputes, but the probe test didn't work out.  Clear the abort flag.
	m_iFlags.ClearFlag(SF_ShouldAbort);

	// Cant get up to do agitation, so disable it so we don't keep getting the events.
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CanBeAgitated, false);

	// Need to at least have an audio response here as we will be disabling agitation.
	pPed->NewSay("GENERIC_WHATEVER");

	// Clear all event flags if responding to an event.
	if (IsRespondingToEvent())
	{
		EndEventReact();
	}

	// Clear reaction flags.
	m_iScenarioReactionFlags.ClearAllFlags();
	// Set the panic clipset back to invalid.
	m_panicClipSetId = CLIP_SET_ID_INVALID;
}

// Override the streaming priority to be high so the ped is guaranteed to be able to stream in their animation - otherwise they will appear not to react in some high-demand situations.
void CTaskUseScenario::OverrideStreaming()
{
	m_ClipSetRequestHelper.SetStreamingPriorityOverride(SP_High);
}


CObject* CTaskUseScenario::FindPropInEnvironmentNearPoint(const CScenarioPoint& point, int realScenarioType, bool allowUprooted)
{
	return FindPropInEnvironmentNearPosition(point.GetPosition(), realScenarioType, allowUprooted);
}


CObject* CTaskUseScenario::FindPropInEnvironmentNearPosition(Vec3V_In scanPos, int realScenarioType, bool allowUprooted)
{
	const Vec3V scanPosV = scanPos;

	float scanDist = sm_Tunables.m_FindPropInEnvironmentDist;
	const ScalarV scanDistV = ScalarV(scanDist);
	const spdSphere scanningSphere(scanPosV, scanDistV);

	const CScenarioInfo* pInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(realScenarioType);
	if(!taskVerifyf(pInfo, "Expected scenario info for type %d", realScenarioType))
	{
		return NULL;
	}
	const CConditionalAnimsGroup* pAnims = pInfo->GetClipData();
	if(!pAnims)
	{
		return NULL;
	}

	sFindPropInEnvironmentCBData callbackData;
	callbackData.m_Center = scanPosV;
	callbackData.m_BestObjectFoundDistSq = Scale(scanDistV, scanDistV);
	callbackData.m_ConditionalAnimsGroup = pAnims;
	callbackData.m_AllowUprooted = allowUprooted;

	fwIsSphereIntersecting intersection(scanningSphere);
	CGameWorld::ForAllEntitiesIntersecting(&intersection, CTaskUseScenario::FindPropInEnvironmentCB, (void*)&callbackData, ENTITY_TYPE_MASK_OBJECT, SEARCH_LOCATION_INTERIORS|SEARCH_LOCATION_EXTERIORS, SEARCH_LODTYPE_HIGHDETAIL, SEARCH_OPTION_FORCE_PPU_CODEPATH, WORLDREP_SEARCHMODULE_PEDS);

	return callbackData.m_BestObjectFound;
}


// Returns true if something has happened to move or disrupt the ped's attached prop
// enough to make them leave
bool CTaskUseScenario::CheckAttachStatus()
{
	CPed * pPed = GetPed();
	if ( IsDynamic() )
	{
		fwAttachmentEntityExtension *pedAttachExt = pPed->GetAttachmentExtension();

		const Tunables& tunables = GetTunables();

		// If the ped is attached to an object that becomes active, stand up straight away.
		if( pedAttachExt && pedAttachExt->GetIsAttached() && !(pedAttachExt->GetAttachFlags() & ATTACH_FLAG_ATTACHED_TO_WORLD_BASIC) )
		{
			CEntity* pAttachEntity = static_cast<CEntity*>(pedAttachExt->GetAttachParent());
			if( !pAttachEntity )
			{
				pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
				pPed->SetFixedPhysics(false);

				return true;
			}
			// If the object has been broken up in any way
			else if( pAttachEntity && pAttachEntity->GetFragInst() && pAttachEntity->GetFragInst()->GetPartBroken() )
			{
				return true;
			}
			// Ragdoll due to the orientation of the object not being world up.
			else if( pAttachEntity->GetTransform().GetC().GetZf() < tunables.m_BreakAttachmentOrientationThreshold )
			{
				return true;
			}
			else if(pAttachEntity->GetIsTypeObject() && !pAttachEntity->GetIsStatic() && static_cast<CObject*>(pAttachEntity)->GetVelocity().Mag2() > square(tunables.m_BreakAttachmentMoveSpeedThreshold))
			{
				// The prop we are attached to is moving, probably shouldn't stay attached.
				return true;
			}
		}
	} 
	else if (GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::LeaveScenarioIfGivenGroundPhysical) && pPed->GetGroundPhysical())
	{
		// If this ped type gets a ground physical and wasn't expecting one, then the ped should leave their scenario point.
		// If the player was close by, then we assume the player was bumping them somehow with a prop.
		// Note this so the ped gets an agitation event.
		CPed* pPlayer = CGameWorld::FindLocalPlayer();
		if (pPlayer)
		{
			static const float sf_ClosePlayerBumpDistanceSquared = 4.0f * 4.0f;
			Vec3V vPos = pPed->GetTransform().GetPosition();
			Vec3V vPlayerPos = pPlayer->GetTransform().GetPosition();
			if (IsLessThanAll(DistSquared(vPos, vPlayerPos), ScalarV(sf_ClosePlayerBumpDistanceSquared)))
			{
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_BumpedByPlayer, true);
			}
			LeaveScenario();
		}
	}

	return false;
}

// Have the ped calculate the target position again, this time aiming to snap to the ground.
void CTaskUseScenario::ReassessExitTargetForSmallFalls()
{
	CPed* pPed = GetPed();

	// Now the ped will interpolate towards the ground while playing the exit animation.

	// Cache off the old target.
	Vector3 vTarget;
	Quaternion qTarget;
	m_PlayAttachedClipHelper.GetTarget(vTarget, qTarget);
	
	UpdateTargetSituation();

	// Determine what the new target will be.
	vTarget.z = m_PlayAttachedClipHelper.GetTargetPositionZ();

	// Set the new target with the updated Z.
	// We don't want to readjust the X or Y target offsets.
	m_PlayAttachedClipHelper.SetTarget(vTarget, qTarget);

	// Set the initial offsets.
	m_PlayAttachedClipHelper.SetInitialOffsets(*pPed);

	// Set this to false so we don't continuously update the target's position - it's an expensive function.
	m_bExitToLowFall = false;
}

// When exiting to a fall, return if the ped can stop playing an exit animation.
bool CTaskUseScenario::EndBecauseOfFallTagging()
{
	// If no clip helper, then you can exit.
	if (!GetClipHelper())
	{
		return true;
	}

	// Grab the phase.
	float fPhase = GetClipHelper()->GetPhase();

	if (m_bExitToLowFall || m_bExitToHighFall)
	{
		// Check for phase event to determine how to best exit this scenario. 
		// Marked as "FallExit" in the clip, this is usually when the ped just pushes off the wall.
		if (GetClipHelper()->IsEvent(CClipEventTags::FallExit) || fPhase > GetTunables().m_DetachExitDefaultPhaseThreshold)
		{
			// For low falls just recalculate the target position so the animation hits the ground when done.
			if (m_bExitToLowFall)
			{
				ReassessExitTargetForSmallFalls();
			}
			// For high falls quit immediately so the fall task can take over.
			else if(m_bExitToHighFall)
			{
				return true;
			}
		}
	}

	return false;
}

// Give the ped a ragdoll event to terminate the scenario and start NM.
bool CTaskUseScenario::RagdollOutOfScenario()
{
	CPed* pPed = GetPed();

	if (CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_FALL))
	{
		CTaskNMRelax* pTask = rage_new CTaskNMRelax(200, 300);
		CEventSwitch2NM event(300, pTask, true);

		pPed->SwitchToRagdoll(event);

		return true;
	}
	else
	{
		nmDebugf2("[%u] RagdollOutOfScenario:  %s (%p) - FAILED. CanUseRagdoll() returned false.", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed);
	}

	return false;
}


bool CTaskUseScenario::ShouldBeAttachedWhileUsing() const
{
	bool attach = false;

	if (m_eScenarioMode == SM_EntityEffect)
	{
		attach = true;
	}
	else
	{
		const CScenarioInfo *pInfo = CScenarioManager::GetScenarioInfo(GetScenarioType());
		// Peds only get attached to world (ie: non-prop) scenario points if this flag is set
		// Used for any non-standing peds that need to have their mover clamped to the scenario point
		if(pInfo->GetIsFlagSet(CScenarioInfoFlags::AttachPed))
		{
			attach = true;
		}			
	}

	return attach;
}


void CTaskUseScenario::LeaveScenario(bool bAllowClones)
{
	if (GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::UseNMGetupWhenExiting))
	{
		SetState(State_NMExit);
		return;
	}

	CPed* pPed = GetPed();
	DetermineExitAnimations();

	if(pPed->IsNetworkClone() && !bAllowClones)
	{
		return;
	}

	// then we should try to load the exit dictionary and play the exit clip
	if (m_exitClipSetId != CLIP_SET_ID_INVALID && !m_iFlags.IsFlagSet(SF_NeverUseRegularExit))
	{
		m_iExitClipsFlags.ChangeFlag(eExitClipsRequested,true);
		m_ClipSetRequestHelper.RequestClipSetIfExists(m_exitClipSetId);
		if (m_iFlags.IsFlagSet(SF_ForceHighPriorityStreamingForNormalExit))
		{
			OverrideStreaming();
		}

		if (m_ClipSetRequestHelper.IsClipSetStreamedIn())
		{
			// If we are the master, and the slaves are not ready, don't leave just yet.
			if(m_iFlags.IsFlagSet(SF_SynchronizedMaster) && m_iFlags.IsFlagSet(SF_SlaveNotReadyForSync))
			{
				return;
			}

			fwMvClipId chosenClipId = CLIP_ID_INVALID;

			// If stationary flag is set, then never allow exits.
			CScenarioClipHelper::ScenarioClipCheckType iType = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee) ? 
				CScenarioClipHelper::ClipCheck_Failed : ChooseClipInClipSet(CConditionalAnims::AT_EXIT, m_exitClipSetId, chosenClipId);
			if (iType == CScenarioClipHelper::ClipCheck_Success)
			{
				SetChosenClipId(chosenClipId, GetPed(), this);
#if __ASSERT
				// Do some error checking to perhaps catch some problems with the setup of the scenario.
				if(m_iFlags.IsFlagSet(SF_SynchronizedSlave))
				{
					const CTaskUseScenario* pMasterTask = FindSynchronizedMasterTask();
					if(pMasterTask)
					{
						if(m_iExitClipsFlags.IsFlagSet(eExitClipsAreSynchronized))
						{
							taskAssertf(pMasterTask->m_iExitClipsFlags.IsFlagSet(eExitClipsAreSynchronized), "Slave's exit clips are synchronized, but the master's are not.");
							taskAssertf(pMasterTask->m_SyncedSceneId != INVALID_SYNCED_SCENE_ID, "Master doesn't have a synchronized scene set up.");						
						}
						else
						{
							taskAssertf(!pMasterTask->m_iExitClipsFlags.IsFlagSet(eExitClipsAreSynchronized), "Master's exit clips are synchronized, but the slave's are not.");
						}
					}
				}
#endif	// __ASSERT

				SetState(State_Exit);
			}
			else if (iType == CScenarioClipHelper::ClipCheck_Pending)
			{
				// Try again later.
				return;
			}
			else // ELSE: Stuck...
			{
				// Never try to leave normally again.
				m_iFlags.SetFlag(SF_IdleForever);
				m_fIdleTime = -1;
				m_iFlags.ClearFlag(SF_CheckConditions);
				m_iFlags.ClearFlag(SF_ShouldAbort);
			}
		}
	}
	else
	{
		// It's feasible that we don't have an Outro (a Stationary Scenario for example). So just quit!
		SetState(State_Finish);

		// This is normally done by the Exit state, which we are about to bypass.
		SetTimeUntilNextLeave();

		//Likewise, we'd do this in Exit if we were going there
		if (m_iFlags.IsFlagSet(SF_ExitingHangoutConversation))
		{
			StartNewScenarioAfterHangout();
			m_iFlags.ClearFlag(SF_ExitingHangoutConversation);
		}
	}
}


void CTaskUseScenario::SetIgnoreLowPriShockingEventsFromScenarioInfo(const CScenarioInfo& rInfo)
{
	SetIgnoreLowPriShockingEvents(rInfo.GetIsFlagSet(CScenarioInfoFlags::IgnoreLowShockingEvents));
}


void CTaskUseScenario::SetIgnoreLowPriShockingEvents(bool b)
{
	// Only do something if we get a new value.
	if(m_bIgnoreLowPriShockingEvents != b)
	{
		CPed* pPed = GetPed();
		if(pPed)
		{
			// Call StartIgnoreLowPriShockingEvents() or EndIgnoreLowPriShockingEvents()
			// to increase or decrease the low priority shocking ignore count. By going
			// through SetIgnoreLowPriShockingEvents(), this task can only ever hold one
			// count, and can't leak it (assuming we go through the regular cleanup and
			// don't bypass this function).
			CPedIntelligence& intel = *pPed->GetPedIntelligence();
			if(b)
			{
				intel.StartIgnoreLowPriShockingEvents();
			}
			else
			{
				intel.EndIgnoreLowPriShockingEvents();
			}
		}

		// Set the new value.
		m_bIgnoreLowPriShockingEvents = b;
	}
}


void CTaskUseScenario::SetStationaryReactionsFromScenarioPoint(const CScenarioInfo& rInfo)
{
	if (rInfo.GetIsFlagSet(CScenarioInfoFlags::DontAdjustStationaryReactionStatus))
	{
		return;
	}
	const CScenarioPoint* pScenarioPoint = GetScenarioPoint();
	atHashWithStringNotFinal sReactHash("CODE_HUMAN_STAND_COWER",0x161530CA);
	bool stationaryReactions = false;
	if(pScenarioPoint && pScenarioPoint->IsFlagSet(CScenarioPointFlags::StationaryReactions))
	{
		stationaryReactions = true;
		sReactHash = rInfo.GetStationaryReactHash();
	}

	if( NetworkInterface::IsGameInProgress() && 
		(pScenarioPoint==NULL || GetPed()->IsNetworkClone()) )
	{
#if __BANK
		if(pScenarioPoint==NULL && mbNewTaskMigratedWhileCowering)
		{
			gnetDebug3("%s SetStationaryReactionsFromScenarioPoint pScenarioPoint NULL while mbNewTaskMigratedWhileCowering",GetPed()->GetDebugName());
		}
#endif
		stationaryReactions |=  mbNewTaskMigratedWhileCowering;
	}

	SetStationaryReactionsEnabledForPed(*GetPed(), stationaryReactions, sReactHash);
}

void CTaskUseScenario::SetUsingScenario(bool b)
{
	CPed& ped = *GetPed();
	bool oldVal = ped.GetPedConfigFlag(CPED_CONFIG_FLAG_UsingScenario);
	if(oldVal == b)
	{
		return;
	}
	ped.SetPedConfigFlag(CPED_CONFIG_FLAG_UsingScenario, b);
	ped.UpdateSpatialArrayTypeFlags();

	if(b)
	{
		ped.GetPedAudioEntity()->OnScenarioStarted(GetScenarioType(), &GetScenarioInfo());
	}
	else
	{
		ped.GetPedAudioEntity()->OnScenarioStopped(GetScenarioType());
	}
}

bool CTaskUseScenario::IsEventConsideredAThreat(const CEvent& rEvent) const
{
	// Pseudohack for B*2050647 - allow player dangerous animal events through the filter as it looks worse for peds
	// to ignore this situation just because they didn't have LoS.
	const CPed* pSourcePed = rEvent.GetSourcePed();
	if (rEvent.GetEventType() == EVENT_SHOCKING_DANGEROUS_ANIMAL && pSourcePed && pSourcePed->IsAPlayerPed())
	{
		return false;
	}

	//Check the response task type.
	CTaskTypes::eTaskType nTaskType = rEvent.GetResponseTaskType();
	switch(nTaskType)
	{
		case CTaskTypes::TASK_COMBAT:
		case CTaskTypes::TASK_POLICE_WANTED_RESPONSE:
		case CTaskTypes::TASK_SWAT_WANTED_RESPONSE:
		case CTaskTypes::TASK_THREAT_RESPONSE:
		{
			return true;
		}
		default:
		{
			return false;
		}
	}
}

bool CTaskUseScenario::IsLosClearToThreatEvent(const CEvent& rEvent) const
{
	//Keep track of the target ped.
	const CPed* pTargetPed = NULL;
	
	//Check the response task type.
	CTaskTypes::eTaskType nTaskType = rEvent.GetResponseTaskType();
	switch(nTaskType)
	{
		case CTaskTypes::TASK_COMBAT:
		case CTaskTypes::TASK_POLICE_WANTED_RESPONSE:
		case CTaskTypes::TASK_SWAT_WANTED_RESPONSE:
		{
			pTargetPed = rEvent.GetTargetPed();
			break;
		}
		case CTaskTypes::TASK_THREAT_RESPONSE:
		{
			pTargetPed = rEvent.GetSourcePed();
			break;
		}
		default:
		{
			taskAssertf(false, "The task type is invalid: %d.", nTaskType);
			break;
		}
	}
	
	//Ensure the target ped is valid.
	if(!pTargetPed)
	{
		return false;
	}
	
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Ensure the targeting is valid.
	CPedTargetting* pTargeting = pPed->GetPedIntelligence()->GetTargetting(true);
	if(!taskVerifyf(pTargeting, "The targeting is invalid."))
	{
		return false;
	}

	//Check if we haven't registered the target.
	if(!pTargeting->FindTarget(pTargetPed))
	{
		//Ensure the ped is not friendly with the target.
		if(pPed->GetPedIntelligence()->IsFriendlyWith(*pTargetPed))
		{
			return false;
		}

		//Register the threat.
		pPed->GetPedIntelligence()->RegisterThreat(pTargetPed, false);
	}

	//Note that the targeting is in use.
	pTargeting->SetInUse();

	return (pTargeting->GetLosStatus(pTargetPed, false) == Los_clear);
}

void CTaskUseScenario::StartNewScenarioAfterHangout()
{
	// we are allowed to wander if pavement is nearby AND
	// if our scenario point is not set to NeverLeave.
	bool bAllowedToWander =	(SCENARIO_DEBUG_ONLY(sm_bDoPavementCheck &&) m_iFlags.IsFlagSet(SF_CanExitForNearbyPavement)) &&
							(GetScenarioPoint() && GetScenarioPoint()->GetTimeTillPedLeaves() >= 0.0f);

	int random = fwRandom::GetRandomNumberInRange(0, 2);
	if (random == 0 &&  bAllowedToWander)
	{
		// 33% chance of leaving entirely.

		// say goodbye if there are other peds near [8/9/2012 mdawe]
		CPed* pPed = GetPed();
		CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
		if (entityList.GetCount() != 0)
		{
			CPed* pPlayer = CGameWorld::FindLocalPlayer();
			if (naVerifyf(pPlayer,"Can't find local player."))
			{	
				CEntityScannerIterator entityList = pPlayer->GetPedIntelligence()->GetNearbyPeds();
				for( CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext() )
				{
					CPed* pNearbyPed = static_cast<CPed*>(pEnt);
					if (pNearbyPed)
					{
						if( pNearbyPed != pPlayer )
						{
							pPed->NewSay("GENERIC_BYE");
							break;
						}
					}
				}
			}
		}
	}
	else 
	{
		// 66% chance of moving to a new CTaskUseScenario [8/9/2012 mdawe]
		GetPed()->GetPedIntelligence()->SetStartNewHangoutScenario(true);
	}
}

bool CTaskUseScenario::HasNearbyPavement()
{
	if ( SCENARIO_DEBUG_ONLY(sm_bDoPavementCheck &&) true)
	{
		if (m_iFlags.IsFlagSet(SF_CanExitForNearbyPavement) || m_iFlags.IsFlagSet(SF_CanExitToChain))
		{
			return true;
		}
		else
		{
			if (m_PavementHelper.IsSearchNotStarted() && m_fLastPavementCheckTimer > sm_Tunables.m_DelayBetweenPavementFloodFillSearches)
			{
				// We haven't done a pavement check yet, so start one. [9/21/2012 mdawe]
				m_PavementHelper.StartPavementFloodFillRequest(GetPed());
				m_iFlags.ClearFlag(SF_CanExitForNearbyPavement);
				m_iFlags.SetFlag(SF_PerformedPavementFloodFill);
			} 
			else if (m_PavementHelper.IsSearchActive())
			{
				// We have a pavement check in flight. Check to see if it's finished. [9/21/2012 mdawe]
				m_PavementHelper.UpdatePavementFloodFillRequest(GetPed());
			}
			else if (m_PavementHelper.HasPavement())
			{
				m_iFlags.SetFlag(SF_CanExitForNearbyPavement);
				m_iFlags.ClearFlag(SF_PerformedPavementFloodFill);
				return true;
			}
			else if (m_PavementHelper.HasNoPavement())
			{
				m_iFlags.ClearFlag(SF_CanExitForNearbyPavement);
				m_iFlags.ClearFlag(SF_PerformedPavementFloodFill);
			}
		}

		// Either we just started the request, it isn't done yet, or it has failed.
		return false;
	}
	else
	{
		//Pavement check is disabled.
		return true;
	}
}

float CTaskUseScenario::RandomizeRate(float fMinRate, float fMaxRate, float fMinDifferenceFromLastRate, float& fLastRate) const
{
	float fRate = fwRandom::GetRandomNumberInRange(fMinRate, fMaxRate);

	// Ensure that the random cower rate we've chosen isn't too close to the last one picked. [10/16/2012 mdawe]
	if (rage::Abs(fRate - fLastRate) < fMinDifferenceFromLastRate)
	{
		if (fRate < fLastRate)
		{
			fRate -= fMinDifferenceFromLastRate;
		}
		else 
		{
			fRate += fMinDifferenceFromLastRate;
		}
	}

	fLastRate = fRate;

	return fRate;
}

void CTaskUseScenario::RandomizeRateAndUpdateLastRate()
{
	static dev_float s_fMinRate = 0.85f;
	static dev_float s_fMaxRate = 1.15f;
	static dev_float s_fMinDifferenceFromLastRate = 0.05f;
	float fRate = RandomizeRate(s_fMinRate, s_fMaxRate, s_fMinDifferenceFromLastRate, sm_fLastRate);

	GetClipHelper()->SetRate(fRate);
}

void CTaskUseScenario::RandomizeCowerPanicRateAndUpdateLastRate()
{
	float fCowerPanicRate = RandomizeRate(sm_Tunables.m_MinRateToPlayCowerReaction, sm_Tunables.m_MaxRateToPlayCowerReaction,
		sm_Tunables.m_MinDifferenceBetweenCowerReactionRates, sm_fLastCowerPanicRate);

	GetClipHelper()->SetRate(fCowerPanicRate);
}

// Returns the priority slot that the exit clip should use when playing (which is determined by matching whatever slot the ambient clips task is using).
s32 CTaskUseScenario::GetExitClipPriority()
{
	CTask* pSubtask = GetSubTask();
	if (pSubtask && pSubtask->GetTaskType() == CTaskTypes::TASK_AMBIENT_CLIPS)
	{
		// Ask the subtask what its currently active clip priority is.
		// We want these to match so the blend works out properly.
		CTaskAmbientClips* pAmbient = static_cast<CTaskAmbientClips*>(pSubtask);
		return pAmbient->GetActiveClipPriority();
	}
	return AP_LOW;
}



//-------------------------------------------------------------------------
// Clone task implementation
//-------------------------------------------------------------------------
// helper for network migrating Clone to local
void CTaskUseScenario::SetUpMigratingCloneToLocalCowerTask(CClonedFSMTaskInfo* pTaskInfo)
{
	if( taskVerifyf(pTaskInfo,"Null pTaskInfo") )
	{
		mbNewTaskMigratedWhileCowering = true;
		ReadQueriableState(pTaskInfo);
	}
}

void CTaskUseScenario::NetworkSetUpPedPositionAndAttachments()
{
	CPed* pPed = GetPed();

	Assertf(NetworkInterface::IsGameInProgress(),"%s Only expect this to be used in network games",pPed?pPed->GetDebugName():"Null ped");

	if (taskVerifyf(pPed,"Expect valid ped") && !pPed->GetIsAttached())
	{
		bool bShouldBeAttached = ShouldBeAttachedWhileUsing();

		//Get the scenario position and heading.
		Vector3 vWorldPosition;
		float fWorldHeading;
		GetScenarioPositionAndHeading(vWorldPosition, fWorldHeading);

		// If not attached, we want to teleport the ped so that their feet are on the scenario point, not their root.
		if (!bShouldBeAttached)
		{
			aiAssert(pPed->GetCapsuleInfo());
			vWorldPosition.z += pPed->GetCapsuleInfo()->GetGroundToRootOffset();
		}

		const bool triggerPortalRescan = pPed->PopTypeIsMission();
		pPed->Teleport(vWorldPosition, fWorldHeading, true, triggerPortalRescan);

		if (bShouldBeAttached)
		{
			//Calculate the orientation from the heading.
			Quaternion qWorldOrientation;
			qWorldOrientation.FromEulers(Vector3(0.0f, 0.0f, fWorldHeading), "xyz");

			//Attach the ped.
			Attach(vWorldPosition, qWorldOrientation);
			ToggleFixedPhysicsForScenarioEntity(true);
		}
	}
}

CTaskInfo* CTaskUseScenario::CreateQueriableState() const
{
	CTaskInfo* pNewTaskInfo = NULL;

	CAITarget target(m_Target); 

	if (NetworkInterface::IsGameInProgress() && m_Target.GetIsValid())
	{	
		s32 nTargetMode = m_Target.GetMode();

		// Check the network status of the target entity before syncing
		if(nTargetMode == CAITarget::Mode_Entity || nTargetMode == CAITarget::Mode_EntityAndOffset)
		{
			PlayerFlags targetClonedState=0, targetCreateState=0, targetRemoveState=0;
			PlayerFlags pedClonedState;

			const CEntity* pTargetEntity = m_Target.GetEntity();

			if( taskVerifyf(pTargetEntity,"Null target entity") )
			{
				if(pTargetEntity->GetIsDynamic())
				{
					const netObject* pTargetNetObj = pTargetEntity->GetIsDynamic() ? static_cast<const CDynamicEntity*>(pTargetEntity)->GetNetworkObject(): NULL;

					if(pTargetNetObj)
					{
						pTargetNetObj->GetClonedState(targetClonedState, targetCreateState, targetRemoveState);
					}
				}

				CNetObjPed*	netObjPed	= static_cast<CNetObjPed *>(GetPed()->GetNetworkObject());

				if(taskVerifyf(netObjPed,"Expect valid net object for task ped"))
				{
					pedClonedState = netObjPed->GetClonedState();
					if (pedClonedState != targetClonedState || targetCreateState!=0 || targetRemoveState != 0 )
					{	
						// If the target is not in the same state as this tasks ped then only sync the target as a position type
						Vector3 vTargetPosition;
						m_Target.GetPosition(vTargetPosition);
						target.SetPosition(vTargetPosition);
					}
				}		
			}
			else
			{
				target.Clear(); //reg pointer to entity has been cleared so clear target
			}
		}
	}

	bool bCreateProp = false;

	const CTask* pSubTask = GetSubTask();
	if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_AMBIENT_CLIPS)
	{
		const CTaskAmbientClips* pTaskAmbientClips = static_cast<const CTaskAmbientClips*>(pSubTask);

		if(pTaskAmbientClips->GetFlags().IsFlagSet(CTaskAmbientClips::Flag_PickPermanentProp))
		{
			bCreateProp = true;
		}
	}

	bCreateProp |= GetCreateProp();

	u32 syncedFlagsMask		= (SF_LastSyncedFlag<<1)-1;
	u32 syncedSREFFlagsMask = (SREF_LastSyncedFlag<<1)-1;

	u32 flags = m_iFlags & syncedFlagsMask;
	u32 SREFflags = m_iScenarioReactionFlags & syncedSREFFlagsMask;

	float fHeading = m_fHeading;

	if(m_bMedicTypeScenario && GetScenarioPoint())
	{
		fHeading = GetScenarioPoint()->GetDirectionf();
	}

	pNewTaskInfo =  rage_new CClonedTaskUseScenarioInfo(GetState(),
														static_cast<s16>(GetScenarioType()),
														GetScenarioPoint(),
														GetIsWorldScenario(),
														GetWorldScenarioPointIndex(),
														flags,
														static_cast<u16>(SREFflags),
														m_eScenarioMode,
														fHeading,
														m_vPosition,
														target,
														bCreateProp,
														m_bMedicTypeScenario,
														m_fIdleTime);

	return pNewTaskInfo;
}

void CTaskUseScenario::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_USE_SCENARIO );

	CClonedTaskUseScenarioInfo *CScenarioInfo = static_cast<CClonedTaskUseScenarioInfo*>(pTaskInfo);

	u32 flags		= (u32)m_iFlags.GetAllFlags();
	u32 SREFflags	= (u32)m_iScenarioReactionFlags.GetAllFlags();

	u32 syncedFlagsMask		= (SF_LastSyncedFlag<<1)-1;
	u32 syncedSREFFlagsMask = (SREF_LastSyncedFlag<<1)-1;

	flags &= ~syncedFlagsMask;
	flags |= CScenarioInfo->GetFlags();

	SREFflags &= ~syncedSREFFlagsMask;
	SREFflags |= CScenarioInfo->GetSREFFlags();

	m_iFlags.SetAllFlags((s32)flags);
	m_iScenarioReactionFlags.SetAllFlags((u16)SREFflags);

	if(CScenarioInfo->GetState()==State_PlayAmbients)
	{
		m_iFlags.SetFlag(CTaskUseScenario::SF_Warp | CTaskUseScenario::SF_SkipEnterClip); //make sure ped goes straight to same position and does not play enter clip now it is in State_PlayAmbients
	}

	CScenarioInfo->GetWeaponTargetHelper().UpdateTarget(m_Target);

	m_bMedicTypeScenario = CScenarioInfo->GetMedicType();

	if(m_bMedicTypeScenario && GetScenarioPoint()==NULL)
	{
		//Create the custom paramedic scenario point from the synced details
		float		fHeading		= CScenarioInfo->GetHeading();
		int			scenarioType	= CScenarioInfo->GetScenarioType();
		Vector3		vPos			= CScenarioInfo->GetScenarioPosition();

		CreateMedicScenarioPoint(vPos, fHeading, scenarioType);
	}

	m_fIdleTime = CScenarioInfo->GetIdleTime();

	//Will read the 
	CTaskScenario::ReadQueriableState(pTaskInfo);
}

CTask::FSM_Return CTaskUseScenario::UpdateClonedFSM( const s32 iState, const FSM_Event iEvent )
{ 
	if(GetStateFromNetwork() == State_Finish)
	{
		return FSM_Quit;
	}

	if( (iEvent == OnUpdate) && GetIsWaitingForEntity() )
	{
		return FSM_Continue;	
	}

	return UpdateFSM(  iState,  iEvent );
} 

void CTaskUseScenario::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

bool CTaskUseScenario::IsInScope(const CPed* pPed)
{
	bool inScope = CTaskScenario::IsInScope(pPed);

	if(inScope && m_eScenarioMode!=SM_WorldLocation)
	{
		bool bPedIsPlayer = pPed && pPed->IsPlayer();

		if( bPedIsPlayer && pPed->GetNetworkObject())
		{
			CNetObjPlayer* netObjPlayer = SafeCast(CNetObjPlayer, pPed->GetNetworkObject());

			if(netObjPlayer)
			{
				if(netObjPlayer->IsConcealed())
				{
					//Concealed player so early out and don't try to get it a 
					//scenario point via CloneFindScenarioPoint
					if(GetScenarioPoint())
					{
						//If this concealed player has a scenario point for
						//this task then free it up so local player can use it
						//e.g. seats in apartments
						SetScenarioPoint(NULL);
						ClearCachedWorldScenarioPointIndex();
					}
					inScope = false;
				}
			}
		}

		if(inScope)
		{
			if(GetScenarioPoint() == 0 && !GetIsWaitingForEntity())
			{
				CTaskScenario::CloneFindScenarioPoint();

				if(GetScenarioPoint() == 0)
				{
					inScope = false;
				}
			}

			if(GetScenarioPoint() && !CScenarioManager::CheckScenarioPointEnabledByGroupAndAvailability(*GetScenarioPoint()))
			{
				inScope = false;
			}
		}
	}

	if(GetState() > State_Start)
	{
		// just left scope
		if(m_WasInScope && !inScope)
		{
			CPed* ped = (CPed*)pPed;
			if(ped->IsPlayer())
			{
				ped->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
			}
		}
		// just came back into scope
		else if(!m_WasInScope && inScope)
		{
			if(pPed->IsPlayer())
			{
				if(ShouldBeAttachedWhileUsing())
				{
					//Get the scenario position and heading.
					Vector3 vWorldPosition;
					float fWorldHeading;
					GetScenarioPositionAndHeading(vWorldPosition, fWorldHeading);
					//Calculate the orientation from the heading.
					Quaternion qWorldOrientation;
					qWorldOrientation.FromEulers(Vector3(0.0f, 0.0f, fWorldHeading), "xyz");

					//Attach the ped.
					Attach(vWorldPosition, qWorldOrientation);
				}
			}
		}
	}
	m_WasInScope = inScope;

	return inScope;
}

bool CTaskUseScenario::IsMedicScenarioPoint() const
{
	static const atHashWithStringNotFinal timeofDeathName("CODE_HUMAN_MEDIC_TIME_OF_DEATH",0x3869617A);
	static const atHashWithStringNotFinal tendToPedName("CODE_HUMAN_MEDIC_TEND_TO_DEAD",0x3F2E396F);
	static const atHashWithStringNotFinal kneelName("CODE_HUMAN_MEDIC_KNEEL",0xA6059A2A);
	static const atHashWithStringNotFinal standImpatient("WORLD_HUMAN_STAND_IMPATIENT",0xD7D92E66);
	s32 iTOD_ScenarioIndex = SCENARIOINFOMGR.GetScenarioIndex(timeofDeathName);
	s32 iTTP_ScenarioIndex = SCENARIOINFOMGR.GetScenarioIndex(tendToPedName);
	s32 iK_ScenarioIndex = SCENARIOINFOMGR.GetScenarioIndex(kneelName);
	s32 iSI_ScenarioIndex = SCENARIOINFOMGR.GetScenarioIndex(standImpatient);

	if( GetScenarioType() == iTOD_ScenarioIndex ||
		GetScenarioType() == iTTP_ScenarioIndex ||
		GetScenarioType() == iK_ScenarioIndex	|| 
		GetScenarioType() == iSI_ScenarioIndex)
	{
		return true;
	}

	return false;
}

bool CTaskUseScenario::CreateMedicScenarioPoint(Vector3 &vPosition, float &fDirection, int iType)
{
	if(taskVerifyf(IsMedicScenarioPoint(), "Expect a medic scenario point type here %d", GetScenarioType()))
	{
		bool bSetNew = false;
		if(m_pClonesParamedicScenarioPoint == NULL)
		{
			bSetNew = true;
			m_pClonesParamedicScenarioPoint = rage_new CScenarioPoint;
		}
		else if(GetScenarioType()!= iType)
		{
			bSetNew = true;
		}

		if(bSetNew && taskVerifyf(m_pClonesParamedicScenarioPoint,"Expect valid m_pClonesParamedicScenarioPoint!") )
		{
			m_pClonesParamedicScenarioPoint->Reset();
			m_pClonesParamedicScenarioPoint->SetPosition(VECTOR3_TO_VEC3V(vPosition));
			m_pClonesParamedicScenarioPoint->SetDirection(fDirection);
			m_pClonesParamedicScenarioPoint->SetScenarioType((unsigned)iType);
			RemoveScenarioPoint();
			SetScenarioPoint(m_pClonesParamedicScenarioPoint);
		}
		return true;
	}

	return false;
}


CTaskFSMClone *CTaskUseScenario::CreateTaskForClonePed(CPed *)
{
	CTaskUseScenario* pTask = NULL;

	pTask = rage_new CTaskUseScenario(*this);

	if(GetCachedWorldScenarioPointIndex()!=-1)
	{
		pTask->SetCachedWorldScenarioPointIndex(GetCachedWorldScenarioPointIndex());
	}

	if(GetScenarioPoint() && m_eScenarioMode!=SM_WorldLocation &&  pTask->m_vPosition.IsClose(VEC3_ZERO,0.1f) )
	{	//We have a scenario point and in a net game and we are not a SM_WorldLocation we copied m_vPosition zero above so use m_vPosition to keep a cache of position 
		pTask->m_vPosition = VEC3V_TO_VECTOR3(GetScenarioPoint()->GetPosition());  
	}

	if (GetState()==State_WaitingOnExit)
	{
		mbOldTaskMigratedWhileCowering = true;
		pTask->mbNewTaskMigratedWhileCowering = true;
	}

	if(GetState()==State_PlayAmbients)
	{
		pTask->m_iFlags.SetFlag(SF_Warp | SF_SkipEnterClip); //make sure ped goes straight to same position and state as before migrate
	}

	//Copy over the synced scenario reaction flags at migrate
	u32 SREFflags	= (u32)m_iScenarioReactionFlags.GetAllFlags();
	u32 syncedSREFFlagsMask = (SREF_LastSyncedFlag<<1)-1;
	SREFflags &= syncedSREFFlagsMask;
	pTask->m_iScenarioReactionFlags.SetAllFlags((u16)SREFflags);

	pTask->SetTimeToLeave(m_fIdleTime); // Copy time to leave

	pTask->ResetConditionalAnimsChosen();	//Reset this when copied task starts from initial state
	pTask->ClearChosenClipId();	//Reset this when copied task starts from initial state

	//Since TS_CreateProp is cleared when task creates the prop on CTaskAmbientClips sub task
	//ensure we reset it on copied task here so it can keep prop on its CTaskAmbientClips sub task
	const CTask* pSubTask = GetSubTask();
	if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_AMBIENT_CLIPS)
	{
		const CTaskAmbientClips* pTaskAmbientClips = static_cast<const CTaskAmbientClips*>(pSubTask);

		if(pTaskAmbientClips->GetFlags().IsFlagSet(CTaskAmbientClips::Flag_PickPermanentProp))
		{
			pTask->SetCreateProp(true);
		}
	}

	if(m_bMedicTypeScenario && m_pClonesParamedicScenarioPoint)
	{
		//Create the next task custom paramedic scenario point from the previous task paramedic scenario point details
		float		fHeading		= m_pClonesParamedicScenarioPoint->GetDirectionf();
		int			scenarioType	= m_pClonesParamedicScenarioPoint->GetScenarioTypeVirtualOrReal();
		Vector3		vPos			= VEC3V_TO_VECTOR3(m_pClonesParamedicScenarioPoint->GetPosition());
		pTask->CreateMedicScenarioPoint(vPos, fHeading, scenarioType);
	}

	//Copy the target if it is valid
	if(m_Target.GetIsValid())
	{
		s32 nTargetMode = m_Target.GetMode();
		Vector3 vTargetPosition;
		m_Target.GetPosition(vTargetPosition);

		switch(nTargetMode)
		{
		case	CAITarget::Mode_Entity:
			pTask->m_Target.SetEntity(m_Target.GetEntity());
			break;
		case	CAITarget::Mode_EntityAndOffset:
			pTask->m_Target.SetEntityAndOffset(m_Target.GetEntity(),vTargetPosition);
			break;
		case	CAITarget::Mode_WorldSpacePosition:
			pTask->m_Target.SetPosition(vTargetPosition);
			break;

		default:
			Assertf(0,"Unexpected target mode! %d",nTargetMode);
			break;
		}
	}

	return pTask;
}

CTaskFSMClone *CTaskUseScenario::CreateTaskForLocalPed(CPed *pPed)
{
	// do the same as clone ped
	return CreateTaskForClonePed(pPed);
}

bool CTaskUseScenario::ControlPassingAllowed(CPed *UNUSED_PARAM(pPed), const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType))
{
	if( IsInProcessPostMovementAttachState() ||
		GetState()==State_WaitForProbes ||
		GetState()==State_WaitForPath ||
		GetState()==State_StreamWaitClip )
	{
		return false;
	}

	return true;
}

bool CTaskUseScenario::AddGetUpSets(CNmBlendOutSetList& sets, CPed* UNUSED_PARAM(pPed))
{
	if (m_iScenarioReactionFlags.IsFlagSet(SREF_ExitingToFlee))
	{
		if (m_Target.GetIsValid())
		{
			Vector3 vTarget;
			m_Target.GetPosition(vTarget);
			CTaskSmartFlee::SetupFleeingGetup(sets, vTarget, GetPed());
			return true;
		}
	}

	return false;
}

bool CTaskUseScenario::AttemptToPlayEnterClip()
{
	if (HasPlayedEnterAnimation())
	{
		return true;
	}

	if ( GetEnterClipSetId(GetPed(), this) != CLIP_SET_ID_INVALID )
	{
		m_EnterClipSetRequestHelper.Request(GetEnterClipSetId(GetPed(), this));
		if (m_EnterClipSetRequestHelper.IsLoaded())
		{
			CPed* pPed = GetPed();
			
			if(fwAnimManager::GetClipIfExistsBySetId(GetEnterClipSetId(GetPed(), this), GetChosenClipId(GetPed(), this)))
			{
				// Check with the blackboard to see if we can play this clip right now
				CAnimBlackboard::AnimBlackboardItem blackboardItem(GetEnterClipSetId(GetPed(), this), GetChosenClipId(GetPed(), this), CAnimBlackboard::IDLE_ANIM_TIME_DELAY);
				if (!ANIMBLACKBOARD.AddItemToBlackboardAndPedIntelligenceIfAllowedToPlay(blackboardItem, GetPed()->GetPedIntelligence()))
				{
					return false;
				}

				pPed->SetNoCollision(GetScenarioEntity(), NO_COLLISION_NEEDS_RESET);

				// Set the enter animation flag to be caught via TaskAmbient to determine random Base anim time
				SetPlayedEnterAnimation();

				//Update the target situation.
				UpdateTargetSituation();

				//Set the initial offsets.
				m_PlayAttachedClipHelper.SetInitialOffsets(*pPed);

				// Metadata stores it in seconds, StartClipBySetAndClip expects things to be 1 / seconds.
				float fScenarioBlendDuration = fabs(GetScenarioInfo().GetIntroBlendInDuration());
				fScenarioBlendDuration = fScenarioBlendDuration == 0.0f ? INSTANT_BLEND_IN_DELTA : 1.0f / fScenarioBlendDuration;
				StartClipBySetAndClip(pPed, GetEnterClipSetId(GetPed(), this), GetChosenClipId(GetPed(), this), fScenarioBlendDuration, CClipHelper::TerminationType_OnDelete);
				
				// Check for the scenario index set by CTaskCower::StateRunning_OnEnter [10/18/2012 mdawe]
				if (m_iScenarioIndex == SCENARIOINFOMGR.GetScenarioIndex(GetPed()->GetPedIntelligence()->GetFleeBehaviour().GetStationaryReactHash()))
				{
					RandomizeCowerPanicRateAndUpdateLastRate();
				}

#if SCENARIO_DEBUG
				if (m_iDebugFlags.IsFlagSet(SFD_UseDebugAnimData))
				{
					//Ready for my next set of data ... 
					m_iDebugFlags.SetFlag(SFD_WantNextDebugAnimData);
				}
#endif // SCENARIO_DEBUG
			}
			else
			{

#if __ASSERT

				CNetObjPed*					netObjPed					= static_cast<CNetObjPed *>(pPed->GetNetworkObject());
				const CScenarioInfo*		pTaskScenarioInfo			= SCENARIOINFOMGR.GetScenarioInfoByIndex(GetScenarioType());
				const CScenarioInfo*		pTaskInfoScenarioInfo		= NULL;
				CTaskInfo*					pTaskInfo					= NULL;
				CClonedScenarioFSMInfoBase* pScenarioInfo				= NULL;

				if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(GetTaskType()))
				{
					pTaskInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(GetTaskType(), PED_TASK_PRIORITY_MAX);
					pScenarioInfo = dynamic_cast<CClonedScenarioFSMInfoBase*>(pTaskInfo);
					if (pScenarioInfo)
					{
						pTaskInfoScenarioInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(pScenarioInfo->GetScenarioType());
					}
				}

				Vector3 vWorldPosition;
				float fWorldHeading;
				GetScenarioPositionAndHeading(vWorldPosition, fWorldHeading);

				taskAssertf(false,"Scenario intro clip not found %s\n %s %s: scenario type index %d task scenario type name %s, task info scenario type %s pos %f %f %f : Heading %f\nm_enterClipSetId %d %s : m_ChosenClipId %d %s\n",
					PrintClipInfo(GetEnterClipSetId(GetPed(), this),GetChosenClipId(GetPed(), this)), 
					netObjPed ? netObjPed->GetLogName():"NULL CNetObjPed",
					netObjPed ? netObjPed->GetPlayerOwner()->GetLogName() : "NULL NetGamePlayer owner",
					GetScenarioType(),
					pTaskScenarioInfo ? pTaskScenarioInfo->GetName(): "Unknown task scenario",
					pTaskInfoScenarioInfo ? pTaskInfoScenarioInfo->GetName(): "Unknown task info scenario",
					vWorldPosition.x, vWorldPosition.y, vWorldPosition.z,
					fWorldHeading,
					GetEnterClipSetId(GetPed(), this).GetHash(),
					GetEnterClipSetId(GetPed(), this).GetCStr(),
					GetChosenClipId(GetPed(), this).GetHash(),
					GetChosenClipId(GetPed(), this).GetCStr()
					);

#endif /* __ASSERT */
			}
		}
	}

	const CScenarioInfo& rInfo = GetScenarioInfo();

	// From this point on, ignore low priority shocking events, but only
	// if dictated by the scenario data.
	SetIgnoreLowPriShockingEventsFromScenarioInfo(rInfo);

	// Similar thing with stationary reactions:
	SetStationaryReactionsFromScenarioPoint(rInfo);

	SetUsingScenario(true);

	return true;
}

void CTaskUseScenario::NotifyNewObjectFromDummyObject(CObject& obj)
{
	taskAssert(obj.GetRelatedDummy() == m_DummyObjectForAttachedEntity);

	// If we already have a point for some reason, we probably shouldn't mess with it.
	if(GetScenarioPoint())
	{
		return;
	}

	// We don't keep track of the scenario points associated with an entity, unfortunately, so we
	// have to look at all the entity points.
	int numEntityPts = SCENARIOPOINTMGR.GetNumEntityPoints();
	for(int i = 0; i < numEntityPts; i++)
	{
		CScenarioPoint& pt = SCENARIOPOINTMGR.GetEntityPoint(i);
		if(pt.GetEntity() != &obj)
		{
			// Doesn't belong to the right entity.
			continue;
		}

		if((int)pt.GetScenarioTypeVirtualOrReal() != m_DummyObjectScenarioPtOffsetAndType.GetWi())
		{
			// Not the right type.
			continue;
		}

		const ScalarV distSqV = DistSquared(pt.GetLocalPosition(), m_DummyObjectScenarioPtOffsetAndType.GetXYZ());
		if(IsGreaterThanAll(distSqV, ScalarV(V_FLT_SMALL_2)))
		{
			// Not the right position (maybe a different seat on the same bench).
			continue;
		}

		SetScenarioPoint(&pt);

		CPed* pPed = GetPed();
		if(pPed)
		{
			if(!GetIsFlagSet(aiTaskFlags::IsAborted) && GetState() == State_PlayAmbients)
			{
				if(!pPed->GetIsAttached() && ShouldBeAttachedWhileUsing())
				{
					//Get the scenario position and heading.
					Vector3 vWorldPosition;
					float fWorldHeading;
					GetScenarioPositionAndHeading(vWorldPosition, fWorldHeading);

					//Calculate the orientation from the heading.
					Quaternion qWorldOrientation;
					qWorldOrientation.FromEulers(Vector3(0.0f, 0.0f, fWorldHeading), "xyz");

					//Attach the ped.
					Attach(vWorldPosition, qWorldOrientation);
				}

				ToggleFixedPhysicsForScenarioEntity(true);
			}
		}

		return;
	}
}

void CTaskUseScenario::SetDummyObjectForAttachedEntity(CDummyObject* pDummyObject)
{
	if(pDummyObject == m_DummyObjectForAttachedEntity)
	{
		return;
	}

	CDummyObject* pOldObj = m_DummyObjectForAttachedEntity;
	if(pOldObj)
	{
		CTaskUseScenarioEntityExtension* pOldExt = pOldObj->GetExtension<CTaskUseScenarioEntityExtension>();
		if(pOldExt)
		{
			pOldExt->RemoveTask(*this);
		
			if(pOldExt->IsEmpty())
			{
				taskVerifyf(pOldObj->GetExtensionList().Destroy(pOldExt), "Call to remove CTaskUseScenarioEntityExtension failed.");
			}	
		}
	}

	m_DummyObjectForAttachedEntity = pDummyObject;
	if(pDummyObject)
	{
		CTaskUseScenarioEntityExtension* pExt = pDummyObject->GetExtension<CTaskUseScenarioEntityExtension>();
		if(!pExt)
		{
			pExt = rage_new CTaskUseScenarioEntityExtension;
			if(pExt)
			{
				pDummyObject->GetExtensionList().Add(*pExt);
			}
		}
		if(pExt)
		{
			pExt->AddTask(*this);
		}
	}
}

CTaskAmbientMigrateHelper*	CTaskUseScenario::GetTaskAmbientMigrateHelper()
{
	if(NetworkInterface::IsGameInProgress())
	{
		CPed* pPed = GetPed();

		CNetObjPed     *netObjPed  = pPed?static_cast<CNetObjPed *>(pPed->GetNetworkObject()):NULL;

		if(netObjPed)
		{
			CTaskAmbientMigrateHelper* pMigrateTaskHelper = netObjPed->GetMigrateTaskHelper();

			if(pMigrateTaskHelper)
			{
				return pMigrateTaskHelper;
			}
		}
	}

	return NULL;
}

struct ScenarioPedStreamingCheckInOut {
	spdSphere m_posAndRadius;
	int		  m_count;
};

static bool UseScenarioStateIsNotReady(const s32 iState)
{
	switch (iState)
	{
	case CTaskUseScenario::State_Start:
	case CTaskUseScenario::State_StreamWaitClip:
	case CTaskUseScenario::State_WaitForProbes:
	case CTaskUseScenario::State_WaitForPath:
		{
			return true;
		}
	default:
		{
			return false;
		}
	}
}

static bool IsScenarioTaskReady(const CTaskUseScenario * pScenarioTask, const CTaskAmbientClips * pClipsTask, CAmbientClipRequestHelper * pClipRequestHelper)
{
	if (pScenarioTask && UseScenarioStateIsNotReady(pScenarioTask->GetState()))
	{
		return false;
	}
	if (pClipsTask && (pClipsTask->IsBaseClipPlaying() || (pClipsTask->GetConditionalAnimChosen() < 0) || (pClipRequestHelper && pClipRequestHelper->CheckClipsLoaded())))
	{
		return true;
	}
	return false;
}

static bool CheckScenarioPedIsStreamingInAnim(void * pPedVoid, void * streamingCheckDataVoid)
{
	Assert(streamingCheckDataVoid);
	CPed * pPed = static_cast<CPed *>(pPedVoid);
	ScenarioPedStreamingCheckInOut * pStreamingCheckData = static_cast<ScenarioPedStreamingCheckInOut *>(streamingCheckDataVoid);
	if (pPed && pStreamingCheckData->m_posAndRadius.ContainsPoint(pPed->GetTransform().GetPosition()))
	{
		CTaskUseScenario * pTaskScenario = smart_cast<CTaskUseScenario *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
		CTaskAmbientClips * pTaskAmbientClips = smart_cast<CTaskAmbientClips *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));
		CAmbientClipRequestHelper * pClipRequestHelper = (pTaskAmbientClips) ? &(pTaskAmbientClips->GetBaseClipRequestHelper()) : NULL; 
		if (!IsScenarioTaskReady(pTaskScenario, pTaskAmbientClips, pClipRequestHelper))
		{
			pStreamingCheckData->m_count++;
			return false;
		}
	}
	return true;
}

bool CTaskUseScenario::AreAnyPedsInAreaStreamingInScenarioAnims( Vec3V_In pos, ScalarV_In r )
{
	CPed::Pool * pPool = CPed::GetPool();
	ScenarioPedStreamingCheckInOut sc;
	sc.m_posAndRadius.Set(pos, r);
	sc.m_count = 0;
	Assert(pPool);
	pPool->ForAll(CheckScenarioPedIsStreamingInAnim, &sc);
	return (sc.m_count > 0);
}

void CTaskUseScenario::InitTunables()
{
	sm_bDetachScenarioPeds = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("DETACH_SCENARIO_PEDS", 0x81cd3ac7), sm_bDetachScenarioPeds);
	sm_bAggressiveDetachScenarioPeds = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("DETACH_SCENARIO_PEDS_AGGRESSIVE", 0x17a20639), sm_bAggressiveDetachScenarioPeds);
}

//-------------------------------------------------------------------------
// Task info for Task Use Scenario Info
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------

CClonedTaskUseScenarioInfo::CClonedTaskUseScenarioInfo(s32 state,
													   s16 scenarioType,
													   CScenarioPoint* pScenarioPoint,
													   bool bIsWorldScenario,
													   int worldScenarioPointIndex,
													   u32 flags,
													   u16 SREFflags,
													   u32 scenarioMode,
													   float fHeading,
													   const Vector3& worldLocationPos,
													   const CAITarget& target,
													   bool bCreateProp,
													   bool bMedicType,
													   float fIdleTime) 
: CClonedScenarioFSMInfo( state, scenarioType, pScenarioPoint, bIsWorldScenario, worldScenarioPointIndex)
, m_Flags(flags)
, m_SREFFlags(SREFflags)
, m_ScenarioMode(scenarioMode)
, m_fHeading(fHeading)
, m_WorldLocationPos(worldLocationPos)
, m_weaponTargetHelper(&target)
, m_bCreateProp(bCreateProp)
, m_bMedicType(bMedicType)
, m_fIdleTime(fIdleTime)
{
	if(m_ScenarioMode != CTaskUseScenario::SM_WorldLocation && m_ScenarioPos.IsClose(VEC3_ZERO,0.1f) )
	{
		m_ScenarioPos = worldLocationPos;
	}

	SetStatusFromMainTaskState(state);
}

CClonedTaskUseScenarioInfo::CClonedTaskUseScenarioInfo()
: m_Flags(0) 
, m_SREFFlags(0)
, m_ScenarioMode(CTaskUseScenario::SM_EntityEffect)
, m_fHeading(0.0f)
, m_WorldLocationPos(VEC3_ZERO)
, m_bCreateProp(false)
, m_bMedicType(false)
, m_fIdleTime(CTaskUseScenario::sm_fUninitializedIdleTime)
{
}

CTaskFSMClone *CClonedTaskUseScenarioInfo::CreateCloneFSMTask()
{
	CTaskUseScenario* pTask=NULL;

	u32 flags = GetFlags();

	if(m_ScenarioMode == CTaskUseScenario::SM_WorldLocation)
	{
		if(m_Flags & CTaskUseScenario::SF_StartInCurrentPosition)
		{
			pTask =  rage_new CTaskUseScenario(GetScenarioType(), flags);
		}
		else
		{
			if(m_WorldLocationPos.IsClose(VEC3_ZERO, 0.1f))
			{
#if __ASSERT
				const CScenarioInfo* pInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(GetScenarioType());
				Assertf(0, "SM_WorldLocation - m_WorldLocationPos is at the origin! Type %s, m_bIsWorldScenario %s, m_ScenarioId %d", 
					pInfo ? pInfo->GetName(): "Unknown scenario", m_bIsWorldScenario ? "true" : "false", m_ScenarioId);
#endif
			}
			pTask =  rage_new CTaskUseScenario(GetScenarioType(), m_WorldLocationPos, m_fHeading, flags);
		}
	}
	else if(m_ScenarioMode == CTaskUseScenario::SM_WorldEffect)
	{
		if(!m_bMedicType && m_ScenarioId==-1)
		{	//remote is sending null scenario point must have aborted and is dormant
			return NULL;
		}

		CScenarioPoint* pScenarioPoint = GetScenarioPoint();
		pTask = rage_new CTaskUseScenario(GetScenarioType(), pScenarioPoint, flags);

		if (m_ScenarioId != -1)
		{
			pTask->SetCachedWorldScenarioPointIndex(m_ScenarioId);
		}
		else
		{
			pTask->CreateMedicScenarioPoint(m_ScenarioPos, m_fHeading, m_ScenarioMode);
		}

	}
	else if(m_ScenarioMode == CTaskUseScenario::SM_EntityEffect )
	{
		float FIND_SCENARIO_RANGE	=	0.5f;

		if(m_ScenarioPos.IsClose(VEC3_ZERO, 0.1f))
		{	//remote is sending null scenario point must have aborted and is dormant
			return NULL;
		}

		CScenarioPoint* pScenarioPoint = GetScenarioPoint();

		if( pScenarioPoint==NULL )
		{
			CPedPopulation::GetScenarioPointInAreaArgs args(RCC_VEC3V(m_ScenarioPos), FIND_SCENARIO_RANGE);
			args.m_NumTypes = 1;
			s32 type = m_scenarioType;
			args.m_Types = &type;
			args.m_MustBeFree = false;
			args.m_PedOrDummyToUsePoint = NULL;
			args.m_Random = false;
			args.m_EntitySPsOnly = true;
			args.m_CheckPopulation = false;

			CPedPopulation::GetScenarioPointInArea(	args, &pScenarioPoint);
		}

		//If this doesn't find a CScenarioPoint streamed in the clone ProcessPreFSM update will find it later
		pTask =  rage_new CTaskUseScenario(GetScenarioType(), pScenarioPoint, flags);

	}

	if (taskVerifyf(pTask, "NULL task pointer"))
	{
#if __ASSERT
		if(m_ScenarioMode == CTaskUseScenario::SM_WorldEffect)
		{
			Assertf(pTask->GetScenarioPoint()!=NULL || GetState()==CTaskUseScenario::State_Start || m_ScenarioId!=-1, 
				"Only expect NULL SM_WorldEffect scenario point in State_Start or if scenario ID is valid pending loading in. State %d", GetState() );
		}
		else if(m_ScenarioMode == CTaskUseScenario::SM_EntityEffect )
		{
			Assertf(pTask->GetScenarioPoint()!=NULL || GetState()==CTaskUseScenario::State_Start || !m_ScenarioPos.IsClose(VEC3_ZERO, 0.1f), 
				"Only expect a NULL SM_EntityEffect scenario point if in State_Start or position is valid pending stream-in. State %d", GetState() );
		}
#endif
		pTask->SetIsWorldScenario(m_bIsWorldScenario);
		pTask->m_eScenarioMode = (CTaskUseScenario::eScenarioMode)m_ScenarioMode;

		if(GetCreateProp())
		{
			pTask->SetCreateProp(true);
		}

		pTask->m_fIdleTime = m_fIdleTime;

		//Copy over the synced scenario reaction flags
		pTask->m_iScenarioReactionFlags.SetAllFlags((u16)GetSREFFlags());
	}

	if(pTask && m_ScenarioMode!=CTaskUseScenario::SM_WorldLocation && !m_ScenarioPos.IsClose(VEC3_ZERO,0.1f) )
	{	//Cache this straight away
		pTask->m_vPosition = m_ScenarioPos;  
	}

	return  pTask;
}

CTask *CClonedTaskUseScenarioInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
	CTaskUseScenario * pTask = (CTaskUseScenario*) CreateCloneFSMTask();

	return pTask;
}
