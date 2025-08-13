//
// filename:	TaskClonedFSM.h
// description:	Base state machine class, provides support for writing/reading network sync data.
//				Also provides an interface to be run on a clone ped on another machine as a clone task
//				visually simulating the peds AI to make it look as if it were running locally.
//
#ifndef TASK_CLONED_FSM_H
#define TASK_CLONED_FSM_H

#include "Task/System/Task.h"
#include "Scene/RegdRefTypes.h"

class CPed;
class CTaskInfo;
class CClonedFSMTaskInfo;

class CTaskFSMClone : public CTask
{
public:
	CTaskFSMClone();

	~CTaskFSMClone();

    // NetSequenceID accessors - the network sequence ID is used to distinguish between
    // instances of the same task when synchronised over the network - this is required
    // in order to prevent the same task being executed more than once on a network clone
    u32 GetNetSequenceID() const { return m_netSequenceID; }
    void SetNetSequenceID(u32 netSequenceID);

	u32 GetPriority() const { return m_iTaskPriority; }
	void SetPriority(const u32 taskPriority) { m_iTaskPriority = taskPriority; }

	// set on clone tasks that are given locally to a clone ped, rather than generated from the queriable state
	void SetRunningLocally(bool bLocal)		{ m_bRunningLocally = bLocal; if (m_bRunningLocally) m_bWasLocallyStarted = true; }
	bool IsRunningLocally() const			{ return m_bRunningLocally; }
	bool WasLocallyStarted() const			{ return m_bWasLocallyStarted; }

	virtual bool				IsClonedFSMTask() const		{ return true; }

   // If this returns true the network blender will not blend the peds position and allow
    // the clone task control of positioning the ped directly. This is used to prevent the
    // clone task update fighting with the network blender causing positional glitches
    virtual bool                OverridesNetworkBlender(CPed *UNUSED_PARAM(pPed)) { return false; }

	// If this returns true the network blender will not blend the peds heading and allow
	// the clone task control of changing the heading directly. 
	virtual bool                OverridesNetworkHeadingBlender(CPed *UNUSED_PARAM(pPed)) { return false; }

	// If this returns true the network blender will not process the peds pending attachments
	// This is used to prevent the clone task attachment update fighting with the network ProcessPendingAttachment
	virtual bool                OverridesNetworkAttachment(CPed *UNUSED_PARAM(pPed)) { return false; }

	// If this returns true the clone task will be allowed to alter the ped's collision irrespective of what it should
	// be, as dictated by the sync updates. 
	virtual bool                OverridesCollision()		{ return false; }

	// If this returns true clone ped will ignore any vehicle state from the network updates (ie the vehicle the ped is supposed to be in)
	virtual bool				OverridesVehicleState()	const	{ return false; }

   // If this returns false the clone task prevents local hit reactions from playing on a clone ped - this is
    // necessary in some circumstances where we don't want a clone task to be interrupted at critical times
    virtual bool                AllowsLocalHitReactions() const { return true; }

    // If this returns true the clone task wants to use in-air blending for the ped. This is typically for
    // cases where the ped is expected to be falling through the air while the task is running
    virtual bool                UseInAirBlender() const { return false; }

    // If this returns false the network code will not allow the ped to be passed control to another machine.
    // This is useful for delaying ownership changes on ped performing tasks at a critical stage of the task,
    // or for some short duration tasks, delaying it until the task has finished processing
    virtual bool                ControlPassingAllowed(CPed *UNUSED_PARAM(pPed), const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType)) { return true; }

	// Functions to create cloned task information and run the task as a 
	// cloned version of a task running on a ped under control of another client.
	// Read/write queriable state - by default a queariable state is created storing just the state
	// and is of type CTaskClonedFSMInfo.
	virtual CTaskInfo*			CreateQueriableState() const = 0;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);

    // This function is called when the task on the remote machine this clone task is replicating is no longer running
    virtual void                OnCloneTaskNoLongerRunningOnOwner();

	// Process FSM, decisdes whether UpdateFSM, or UpdateClonedFSM needs to be called.
	FSM_Return					ProcessFSM( const s32 iState, const FSM_Event iEvent );

	// main FSM update function to be implemented by inheriting classes
	virtual FSM_Return			UpdateClonedFSM		 (const s32 iState, const FSM_Event iEvent)			= 0;

	// Automatically create a subtask (called by the UpdateClonedSubTasks in the derived class
	virtual void				UpdateClonedSubTasks(CPed* pPed, int const iState);
	bool						CreateSubTaskFromClone(CPed* pPed, CTaskTypes::eTaskType const expectedTaskType, bool bAssertIfTaskInfoNotFound = true);

	// Sets whether this task is being run locally or as a clone
	void						SetRunAsAClone(bool bRunAsAClone) { m_bRunAsAClone = bRunAsAClone; }
	bool						GetRunAsAClone()				  { return m_bRunAsAClone; }

	bool GetCachedIsInScope() const { return m_bCachedIsInScope; }

	bool UpdateCachedIsInScope()
	{
		bool b = IsInScope(GetPed());
		m_bCachedIsInScope = b;
		return b;
	}

    // Management functions for handling a clone task going in/out of scope.
    // A clone task is typically out of scope if it is running on a ped far away from the camera,
    // on a ped that remains cloned on the local machine.
    virtual bool IsInScope(const CPed* pPed);
    virtual void EnterScope(const CPed* pPed);
    virtual void LeaveScope(const CPed* pPed);
	virtual float GetScopeDistance() const;

    // Management functions for replacing this task with a clone/local equivalent.
    // This is required to ensure task consistency when a peds ownership is taken/given
    // to another machine
    virtual CTaskFSMClone *CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed)){ return 0; }// = 0;
    virtual CTaskFSMClone *CreateTaskForLocalPed(CPed* UNUSED_PARAM(pPed)){ return 0; }// = 0;

    // Functions for performs any additional functionality required before adding
    // a newly created task (from the CreateTaskForClonePed/CreateTaskForLocalPed functions).
    // An example of this is handling any undesired changes caused when the old task is
    // aborted prior to the new task being added.
    virtual void HandleLocalToCloneSwitch(CPed* pPed)
	{
		CTask* pSubTask = GetSubTask();
		if(pSubTask && pSubTask->IsClonedFSMTask())
		{
			static_cast<CTaskFSMClone *>(pSubTask)->HandleLocalToCloneSwitch(pPed);
		}
	}

    virtual void HandleCloneToLocalSwitch(CPed* pPed)
	{
		CTask* pSubTask = GetSubTask();
		if(pSubTask && pSubTask->IsClonedFSMTask())
		{
			static_cast<CTaskFSMClone *>(pSubTask)->HandleCloneToLocalSwitch(pPed);
		}
	}

	// called when a locally given clone task switches to being remotely updated. If false is returned the task is dumped.
	virtual bool HandleLocalToRemoteSwitch(CPed* UNUSED_PARAM(pPed), CClonedFSMTaskInfo* UNUSED_PARAM(pTaskInfo)) { return true; }

	// called to determine if we should retain task if we fall to switch to remote.
	virtual bool CanRetainLocalTaskOnFailedSwitch() { return false; }

    //PURPOSE
    // This function allows a task to prevent itself from being replaced by a clone task with a higher 
    // priority of the specified task type. This is useful in cases where one task needs to reach a certain state
    // before another task carries on from where it left off (such as animation->ragdoll transitions)
    virtual bool CanBeReplacedByCloneTask(u32 UNUSED_PARAM(taskType)) const { return true; }

    virtual bool IgnoresCloneTaskPriorities() const { return false; }

    virtual bool RequiresTaskInfoToMigrate() const { return true; }

    //PURPOSE
    // Allows a synced clone task to specify a velocity for the network blender to use
    virtual bool HasSyncedVelocity() const { return false; }
    virtual Vector3 GetSyncedVelocity() const { return VEC3_ZERO; }

	// PURPOSE: Force a task to clean itself up and thereby get into a state where it could be safely deleted.
	// PARAMS
	//	priority - The required abort urgency level
	//	pEvent - The event that caused the abort, could be NULL
	// RETURNS: If the task aborted
	virtual bool MakeAbortable(const AbortPriority priority, const aiEvent* pEvent);
	
	virtual bool ProcessPhysics(float UNUSED_PARAM(fTimeStep), int UNUSED_PARAM(nTimeSlice)) { return false; }
	
	virtual float GetScopeDistanceMultiplierOverride() { return 1.0f; }

	s32 GetStateFromNetwork() const { return m_iStateFromNetwork; }

#if ENABLE_NETWORK_LOGGING
	void ResetLastTaskMigrateFailReason()
	{
		formatf(m_LastTaskMigrateFailReason, "");
	}

	const char* GetLastTaskMigrateFailReason()
	{
		return m_LastTaskMigrateFailReason;
	}
#endif // ENABLE_NETWORK_LOGGING
protected:

    // set on clone tasks to indicate they are migrating, for both the task that will be destroyed
    // and the new task that will be created to replace it. This allows the task to leave things running
    // that will be picked up by the replacing task
    void SetIsMigrating(bool bMigrating) { m_bIsMigrating = bMigrating; }
    bool IsMigrating() const { return m_bIsMigrating; }

    void SetStateFromNetwork(const s32 state)  { m_iStateFromNetwork = state; }

	//A helper function that will handle requesting the dictionary or dictionaries for a derived clone task.
	//It will check if the passed array of hashes is loaded and AddRef for each valid one if it is, 
	//and request it if not. It will set the m_bCloneDictRefAdded flag if all numHashes are ready.
	//The derived class is expected to handle RemoveRefWithoutDelete - in the destructor usually.
	//The function can be called in IsInScope to see when the dictionary/ies are valid and ready.
	bool CheckAndRequestSyncDictionary(const u32 *arrayOfHashes, u32 numHashes);

	//This return false if dictionary not streamed ready after requesting it is streamed
	//or will return true with the passed outClipDictIndex if the dictionary is already streamed or 
	bool IsDictStreamedReady(u32 dictHash, int& outClipDictIndex);

protected:
	float   m_scopeModifier;
#if ENABLE_NETWORK_LOGGING
	static const int CONTROL_PASSING_FAIL_REASON_SIZE = 128;
	char m_LastTaskMigrateFailReason[CONTROL_PASSING_FAIL_REASON_SIZE];
#endif // ENABLE_NETWORK_LOGGING


private:
    u32		m_netSequenceID;
    u32     m_iTaskPriority;
	s32		m_iStateFromNetwork;
	bool	m_bCachedIsInScope		: 1;
    bool	m_bRunAsAClone          : 1;
    bool	m_bRunningLocally       : 1;
    bool	m_bWasLocallyStarted    : 1;
    bool    m_bIsMigrating          : 1;

protected:
	bool	m_cloneTaskInScope		: 1;
	bool	m_bCloneDictRefAdded	: 1; //If task needed to stream in a dictionary that wasn't ready this records the reference.
};

#endif //TASK_CLONED_FSM_H
