//
// task/task.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef TASK_H
#define TASK_H

// Framework headers
#include "ai/task/task.h"
#include "fwtl/pool.h"

// Game headers
#include "Debug/DebugDrawStore.h"
#include "Peds/PedMoveBlend/PedMoveBlendTypes.h"
#include "Task/Physics/BlendFromNmData.h"
#include "Task/System/TaskClassInfo.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/TaskTypes.h"
#include "Event/system/EventData.h"
#include "system/poolallocator.h"

// Forward declarations
class  CDamageInfo;
class  CDummyTask;
class  CTaskInfo;
class  CTaskMoveInterface;
class  CTaskMove;
class  CTaskTreePed;
class  CVehMission;
class  CPedDebugVisualiser;
class  CTaskMotionBase;
struct tGameplayCameraSettings;
class  CScenarioPoint;
class CPropManagementHelper;

#define DEFINE_TASK_ENUM(S) S
#define DEFINE_TASK_STRING(S) #S
#define DEFINE_STATIC_STATE_NAME_STRINGS(FIRST_STATE, LAST_STATE, STATE_LIST)   static const char *GetStaticStateName(s32 iState)																\
																				{																												\
																					if (taskVerifyf(iState >= FIRST_STATE && iState <= LAST_STATE, "Invalid State (%i) Passed In", iState))		\
																					{																											\
																						static const char* StateNames[] = {	STATE_LIST(DEFINE_TASK_STRING) };									\
																						return StateNames[iState];																				\
																					}																											\
																					return "State_Invalid";																						\
																				};																												\

enum ConvertPriority
{
	CONVERT_DIST_NEAR		= 15,
	CONVERT_DIST_AVERAGE	= 25,
	CONVERT_DIST_FAR		= 35,
	CONVERT_DIST_NEVER		= -1
};

#define TASK_DEBUG_ENABLED (!__NO_OUTPUT || !__FINAL)

#define TASK_USE_THREADSAFE_POOL_ALLOCATOR 1

// PURPOSE: Base class for all tasks
class CTask : public aiTask
{
	friend CTaskTreePed;
	friend CPedDebugVisualiser;
public:

	REGISTER_POOL_ALLOCATOR(CTask, TASK_USE_THREADSAFE_POOL_ALLOCATOR);

#if DEBUG_DRAW
		static CDebugDrawStore ms_debugDraw;
#endif // DEBUG_DRAW

	CTask();
	virtual ~CTask();

#if !__NO_OUTPUT
	virtual const char* GetTaskName() const { return TASKCLASSINFOMGR.GetTaskName(GetTaskType()); }

	// PURPOSE: Display debug information specific to this task
	virtual void Debug() const;

	virtual const char *GetStateName(s32 iState) const { return TASKCLASSINFOMGR.GetTaskStateName(GetTaskType(),iState); }

	inline const char *GetCurrentStateName() const { return GetStateName( GetState() ); }

	// PURPOSE: Get the name of this task as a string
	virtual atString GetName() const;
#endif // !__NO_OUTPUT

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void StaticUpdate();
	static void ResetStatics();

	// Function for returning if pointer is a valid task
	static bool IsTaskPtr(void* pData);

	// Return the parent task
	const CTask* GetParent() const { return static_cast<const CTask*>(aiTask::GetParent()); }
	CTask* GetParent() { return static_cast<CTask*>(aiTask::GetParent()); }

	// Return the current child task
	const CTask* GetSubTask() const { return static_cast<const CTask*>(aiTask::GetSubTask()); }
	CTask* GetSubTask() { return static_cast<CTask*>(aiTask::GetSubTask()); }

	//get a ped pointer from the m_pEntity
	const CPed* GetPed() const;
	CPed* GetPed();

	const CVehicle* GetVehicle() const;
	CVehicle* GetVehicle();

	const CObject* GetObject() const;
	CObject* GetObject();

	const CPhysical* GetPhysical() const;
	CPhysical* GetPhysical();

	// This function will be called during each time slice of the physics update, so
	// that a continuous force may be applied to the ped by a simple task.
	// return true if the task needs to ped to be kept awake, false if it's done applying forces or whatever
	virtual bool ProcessPhysics(float UNUSED_PARAM(fTimeStep), int UNUSED_PARAM(nTimeSlice)) { return false; }

	// If the peds position needs changing then it should be done in this function.
	// In general the ped position shouldn't be changed by tasks as this screws
	// up the collision resolving but in some cases like getting in and out of cars
	// this is necessary. Because this function is called after everything has been
	// processed it should get the correct position if it is relative to another
	// object.
	// This is called after physics and script.
	//Returns true if it did change the peds position	
	virtual bool ProcessPostMovement() { return false; }

	// A task can query a fully updated and resolved camera frame within this function, as the camera system has already been updated.
	virtual bool ProcessPostCamera() { return false; }

	// Called after the skeleton update but before the Ik
	virtual void ProcessPreRender2() {}

	// Called after clip and Ik
	virtual bool ProcessPostPreRender() { return false; }

	// Called after attachments
	virtual bool ProcessPostPreRenderAfterAttachments() { return false; }

	// Optionally called to give tasks a chance to communicate with Move on every frame during AI timeslicing, should return true if the calls need to continue
	virtual bool ProcessMoveSignals() { return false; }

	// Call this when ProcessMoveSignals() needs to be called.
	void RequestProcessMoveSignalCalls();

	// Returns true if this task is classified as a movement task and lives in the 
	// movement task list
	virtual bool IsMoveTask() const { return false; }
	
	// Returns true if this task is trying to move the ped (i.e. generate a move blend ratio of higher than MOVEBLENDRATIO_STILL)
	virtual bool IsTaskMoving() const { return false; }

	// Tasks which inherit from CVehicleMissionBase may own a CVehicleNodeList.  We may need to cast up to the base class to set the nodes.
	virtual bool IsVehicleMissionTask() const { return false; }

	// Tasks which inherit from CTaskNMBehaviour.
	virtual bool IsNMBehaviourTask() const { return false; }

	// Tasks which inherit from CTaskBlendfromNM.
	virtual bool IsBlendFromNMTask() const { return false; }

	virtual const CTaskMoveInterface* GetMoveInterface() const { return NULL; }
	virtual CTaskMoveInterface* GetMoveInterface() { return NULL; }

	// this determines whether the clip a task is running is synced across the network
	virtual class CClipHelper* GetClipToBeNetworked() { return NULL;}

	// Basic clip interface
	// Start clip
	bool StartClipBySetAndClip(CDynamicEntity* pEntity, const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, float fBlendDelta, CClipHelper::eTerminationType terminationType = CClipHelper::TerminationType_OnFinish, u32 appendFlags = 0);
	bool StartClipBySetAndClip(CDynamicEntity* pEntity, const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, int flags, u32 boneMask, int priority, float fBlendDelta, CClipHelper::eTerminationType terminationType);
	bool StartClipByDictAndHash(CDynamicEntity* pEntity, s32 iDictionaryIndex, u32 clipHash, int flags, u32 boneMask, int priority, float fBlendDelta, CClipHelper::eTerminationType terminationType);

	// Stop clip
	void StopClip(float fBlendDelta = NORMAL_BLEND_OUT_DELTA, u32 uFlags = 0);

	// Get clip
	const CClipHelper* GetClipHelper() const  { return m_clipHelper.GetClipHelper(); }
	CClipHelper* GetClipHelper() { return m_clipHelper.GetClipHelper(); }

	// Get the status of the clip helper
	s32 GetClipHelperStatus() { return m_clipHelper.GetStatus(); }

	// Reset the status to unassigned, its only valid to call this from an cliphelper that has previously terminated
	void ResetClipHelperStatus() { m_clipHelper.ResetStatus(); }

	// Change the clip helpers current callback
	//void SetClipCallBack(CClipHelper::eTerminationType terminationType) { m_clipHelper.SetCallBack(terminationType); }

	// Needs to be moved -CC
	void ProcessAchieveTargetHeading(CPed* pPed, const float fTargetHeading) { m_clipHelper.ProcessAchieveTargetHeading(pPed, fTargetHeading);}

	// Only to be called on a movement task.  Goes through subtasks setting SetMoveBlendRatio().
	// This has to be in CTask because CTaskMoveInterface isn't aware of its owner task in __FINAL builds.
	void PropagateNewMoveSpeed(const float fMBR);

	virtual bool IsValidForMotionTask(CTaskMotionBase& task) const;

	// Virtual interface, does this class internally deal with damage events, returns true if it does and it will
	// generate a ragdoll or animated response as appropriate
	virtual bool RecordDamage(CDamageInfo& damageInfo);

	// This function allows an active task to override the game play camera and associated settings.
	virtual void GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& UNUSED_PARAM(settings)) const {}

	// Accessor for the vehicle AI to find a car mission, by default returns the subtasks
	virtual CVehMission* GetCarMission() { return( GetSubTask() ? GetSubTask()->GetCarMission() : NULL); }

	// Gets the chat helper
	virtual CChatHelper* GetChatHelper() { taskAssertf(0,"Tasks using the chat helper should implement this function"); return NULL; }

	// Gets the prop management helper
	virtual CPropManagementHelper* GetPropHelper() { return NULL; }
	virtual const CPropManagementHelper* GetPropHelper() const { return NULL; }


	// IS this s clone FSM task, this isn't, it needs to be overridden to return true if it is.
	virtual bool IsClonedFSMTask() const { return false; }
	
	// Function to create cloned task information
	virtual CTaskInfo* CreateQueriableState() const;

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();


	// PURPOSE: Adjusts the cover search bounding areas
	virtual void AdjustCoverSearchPosition(Vector3& vCoverSearchPosition) 
	{ 
		if(GetSubTask())
		{
			GetSubTask()->AdjustCoverSearchPosition(vCoverSearchPosition);
		}
	}

	// PURPOSE: Called by ped event scanner when checking dead peds tasks.
	// If this returns true a damage event is not generated if a dead ped is not running the dying task but is running this task 
	virtual bool HandlesDeadPed() 
	{ 
		if(GetSubTask())
		{
			return GetSubTask()->HandlesDeadPed();
		}

		return false;
	}
	
	// PURPOSE: Called by NM control task when deciding whether to abort.
	// If this returns true the NM control task will be aborted.
	virtual bool HandlesRagdoll(const CPed* pPed) const
	{ 
		if(GetSubTask())
		{
			return GetSubTask()->HandlesRagdoll(pPed);
		}

		return false;
	}

	// PURPOSE: Get target entity that we consider as target in the first task that got a target
	// Using this for pathfinder to know which entity to get a position from when pathfinding
	virtual const CEntity* GetTargetPositionEntity() const
	{
		if(GetSubTask())
		{
			return GetSubTask()->GetTargetPositionEntity();
		}

		return NULL;
	}

	// PURPOSE: Gets the tasks move network helper
	virtual const CMoveNetworkHelper*	GetMoveNetworkHelper() const { return NULL; }
	virtual CMoveNetworkHelper*	GetMoveNetworkHelper() { return NULL; }

	inline bool IsClipConsideredFinished() const
	{ 
		static dev_float FINISH_CLIP_PHASE = 0.99f;
		const CClipHelper* pHelper = m_clipHelper.GetClipHelper();
		if (pHelper)
		{
			if (!pHelper->IsFlagSet(APF_ISLOOPED))
			{
				if ((pHelper->GetPhase() + GetTimeStep()) >= FINISH_CLIP_PHASE)
				{
					return true;
				}
			}
		}
		return false;
	}

	// PURPOSE: Is there a valid move network transition to the next ai state from our current state?
	virtual bool IsMoveTransitionAvailable(s32 UNUSED_PARAM(iNextState)) const { return true; }

	// PURPOSE: Reset clip status flags
	virtual void FSM_SetAnimationFlags();

	// PURPOSE: Game level implementation called to abort tasks as they switch
	virtual void FSM_AbortPreviousTask();

	// PURPOSE: This is called for every event received once per frame, allows tasks to intercept
	// the events and carry out state changes and request the event is deleted.
	//enum HandleEvent_result {HandleEvent_Delete = 0, HandleEvent_Continue};
	//virtual HandleEvent_result HandleEvent(CEvent* UNUSED_PARAM(pEvent)) {return HandleEvent_Continue;}

	virtual	CScenarioPoint*		GetScenarioPoint() const { return NULL; }

	// PURPOSE:	If GetScenarioPoint() returned a point, this function should be overridden to
	//			return the associated type, except that it must be a real scenario type, not a virtual
	//			one, so we couldn't always just get it from the point.
	virtual int GetScenarioPointType() const { taskAssertf(false, "GetScenarioPointType() called for %s, which doesn't support it.", GetTaskName()); return -1; }

	//////////////////////////////////////////////////////////////////////////
	// AI task interface for nm getup sets
	//////////////////////////////////////////////////////////////////////////

	virtual bool UseCustomGetup() { return false; }

	// PURPOSE: Called by the nm getup task on the primary task to determine which get up sets to use when
	//			blending from nm. Allows the task to request specific get up types for different a.i. behaviors
	// PARAMS:	sets - use the Add and AddWithFallback methods to specify the blend out sets you want to choose from
	// RETURNS: true if one or more getup sets were specified by the task. False otherwise (this will cause the blend from nm task to use a
	//			default set).
	virtual bool AddGetUpSets(CNmBlendOutSetList& sets, CPed* pPed) 
	{ 
		if (GetSubTask())
		{
			return GetSubTask()->AddGetUpSets(sets, pPed);
		}

		return false; 
	}

	// PURPOSE: Called on the primary task immediately before a blend from nm task ends to inform the 
	//			task which blend out set was actually used.
	//			The task can then make a decision about how to resume correctly.
	// PARAMS:	eNmBlendOutPoseSet - the matched blend out set
	//			CNmBlendOutPoseItem* - The final pose item from the blend out set. This contains information
	//			about the last set of animations played by the blend out set, and how it blended out.
	//			CTaskMove - A pointer to the move task that was run by the nm blend out task (these can be specified in AddGetUpSets()
	//			This allows pre-calculation of navmesh paths, etc whilst the getup task is running.
	virtual void GetUpComplete(eNmBlendOutSet setMatched, CNmBlendOutItem* lastItem, CTaskMove* pMoveTask, CPed* pPed) 
	{
		if (GetSubTask())
		{
			GetSubTask()->GetUpComplete(setMatched, lastItem, pMoveTask, pPed);
		}
	}

	// PURPOSE:	Will be called every frame on the primary task whilst the character is in nm. Allows the task to stream any necessary
	//			anim assets for getups whilst the nm performance is ongoing.
	virtual void RequestGetupAssets( CPed* pPed ) 
	{
		if (GetSubTask())
		{
			GetSubTask()->RequestGetupAssets(pPed);
		}	
	}

	// I want to see if ped was running in "combat" before when NMGetup was running
	virtual bool IsConsideredInCombat() 
	{
		if (GetSubTask())
		{
			return GetSubTask()->IsConsideredInCombat();
		}

		return false; 
	}

	// PURPOSE: If the task was created to respond to an event, returns the event type. If not, it returns EVENT_NONE
	// NOTE[egarcia]: IsResponseForEventType is synchronized in online games for some types of tasks and prevents a task from being interrupted 
	// even if we have lost the current event info (after the ped ownership has been transferred, for example).
	// If we ever synchronize peds events, this would not be necessary.
	virtual eEventType GetIsResponseForEventType() const { return EVENT_NONE; }

	// PURPOSE:	Begin ProcessPhysics() calls on tasks, specifying how many update steps
	//			the tasks that are about to update should expect.
	static void BeginProcessPhysicsUpdates(int numSteps)
	{
		Assert(!sm_ProcessPhysicsNumSteps);
		Assert(numSteps > 0 && numSteps <= 0xff);
		sm_ProcessPhysicsNumSteps = (u8)numSteps;
	}

	// PURPOSE:	End the update phase initiated by a BeginProcessPhysicsUpdates().
	//			Calls to BeginProcessPhysicsUpdates() and EndProcessPhysicsUpdate()
	//			should always be matched.
	static void EndProcessPhysicsUpdate()
	{
		Assert(sm_ProcessPhysicsNumSteps > 0);
		sm_ProcessPhysicsNumSteps = 0;
	}

	// PURPOSE:	Meant to be called from within a task's ProcessPhysics() function,
	//			get how many physics steps are to be expected. An assert will fail
	//			if it's called outside of this context.
	static int GetNumProcessPhysicsSteps()
	{
		Assert(sm_ProcessPhysicsNumSteps > 0);
		return sm_ProcessPhysicsNumSteps;
	}

#if !__FINAL
	// PURPOSE
	//  Verifies that we don't have any orphaned tasks
	static void VerifyTaskCounts();
	static bool VerifyTaskCountCallback(void* pItem, void* data);
#else
	inline static void VerifyTaskCounts() {}
#endif
	
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_NONE; }

#if TASK_DEBUG_ENABLED
	static void PoolFullCallback(void* item);
#endif

private:

	CClipHelper m_clipHelper;

	// PURPOSE:	During the ProcessPhysics() calls on the tasks, this static variable
	//			stores how many calls to ProcessPhysics() the tasks should expect.
	//			Outside of the ProcessPhysics() calls, this should be zero.
	static u8 sm_ProcessPhysicsNumSteps;
};

////////////////////////////////////////////////////////////////////////////////

inline const CPed* CTask::GetPed() const
{
	taskAssert(GetEntity());
	taskAssert(GetEntity() && GetEntity()->GetType() == ENTITY_TYPE_PED);
	return ((const CPed*)GetEntity());
} 

////////////////////////////////////////////////////////////////////////////////

inline CPed* CTask::GetPed()
{
	taskAssert(GetEntity());
	taskAssert(GetEntity() && GetEntity()->GetType() == ENTITY_TYPE_PED);
	return ((CPed*)GetEntity());
}

////////////////////////////////////////////////////////////////////////////////

inline const CVehicle* CTask::GetVehicle() const
{
	taskAssert(GetEntity());
	taskAssert(GetEntity() && GetEntity()->GetType() == ENTITY_TYPE_VEHICLE);
	return ((CVehicle*)GetEntity());
}

////////////////////////////////////////////////////////////////////////////////

inline CVehicle* CTask::GetVehicle()
{
	taskAssert(GetEntity());
	taskAssert(GetEntity() && GetEntity()->GetType() == ENTITY_TYPE_VEHICLE);
	return ((CVehicle*)GetEntity());
}

////////////////////////////////////////////////////////////////////////////////

inline const CObject* CTask::GetObject() const
{
	taskAssert(GetEntity());
	taskAssert(GetEntity()->GetType() == ENTITY_TYPE_OBJECT);
	return ((CObject*)GetEntity());
}

////////////////////////////////////////////////////////////////////////////////

inline CObject* CTask::GetObject()
{
	taskAssert(GetEntity());
	taskAssert(GetEntity()->GetType() == ENTITY_TYPE_OBJECT);
	return ((CObject*)GetEntity());
}

////////////////////////////////////////////////////////////////////////////////

inline const CPhysical* CTask::GetPhysical() const
{
	taskAssert(GetEntity());
	taskAssert(((CEntity*)GetEntity())->GetIsPhysical());
	return ((CPhysical*)GetEntity());
}

////////////////////////////////////////////////////////////////////////////////

inline CPhysical* CTask::GetPhysical()
{
	taskAssert(GetEntity());
	taskAssert(((CEntity*)GetEntity())->GetIsPhysical());
	return ((CPhysical*)GetEntity());
}

////////////////////////////////////////////////////////////////////////////////

inline bool CTask::StartClipBySetAndClip(CDynamicEntity* pEntity, const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, float fBlendDelta, CClipHelper::eTerminationType terminationType, u32 appendFlags)
{
	bool success = m_clipHelper.StartClipBySetAndClip(pEntity, clipSetId, clipId, fBlendDelta, terminationType, appendFlags);
	if (success)
	{
		m_Flags.ClearFlag(aiTaskFlags::AnimFinished);
	}
	return success;
}

////////////////////////////////////////////////////////////////////////////////

inline bool CTask::StartClipBySetAndClip(CDynamicEntity* pEntity, const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, int flags, u32 boneMask, int priority, float fBlendDelta, CClipHelper::eTerminationType terminationType)
{
	bool success = m_clipHelper.StartClipBySetAndClip(pEntity, clipSetId, clipId, flags, boneMask, priority, fBlendDelta, terminationType);
	if (success)
	{
		m_Flags.ClearFlag(aiTaskFlags::AnimFinished);
	}
	return success;
}

////////////////////////////////////////////////////////////////////////////////

inline bool CTask::StartClipByDictAndHash(CDynamicEntity* pEntity, s32 iDictionaryIndex, u32 clipHash, int flags, u32 boneMask, int priority, float fBlendDelta, CClipHelper::eTerminationType terminationType)
{
	bool success = m_clipHelper.StartClipByDictAndHash(pEntity, iDictionaryIndex, clipHash, flags, boneMask, priority, fBlendDelta, terminationType);
	if (success)
	{
		m_Flags.ClearFlag(aiTaskFlags::AnimFinished);
	}
	return success;
}

////////////////////////////////////////////////////////////////////////////////

inline void CTask::StopClip(float fBlendDelta, u32 uFlags)
{
	m_clipHelper.StopClip(fBlendDelta, uFlags);
}

////////////////////////////////////////////////////////////////////////////////

#endif // TASK_H
