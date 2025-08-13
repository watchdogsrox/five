//
// Task/Default/TaskCuffed.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_TASK_CUFFED_H
#define INC_TASK_CUFFED_H

// Game headers
#include "Peds/QueriableInterface.h"
#include "Task/System/Task.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/Tuning.h"

#define ENABLE_TASKS_ARREST_CUFFED 0

#if ENABLE_TASKS_ARREST_CUFFED
# define ENABLE_TASKS_ARREST_CUFFED_ONLY(x) x
#else 
# define ENABLE_TASKS_ARREST_CUFFED_ONLY(x) 
#endif

#if ENABLE_TASKS_ARREST_CUFFED

class CTaskArrest;

//////////////////////////////////////////////////////////////////////////
// CTaskCuffed
//////////////////////////////////////////////////////////////////////////

enum CuffedFlags
{
	CF_UnCuff						= BIT0,				//Indicates arrest ped is trying to uncuff ped.
	CF_ArrestPedFinishedHandshake	= BIT1,				//Indicates arrest ped in finish handshake.
	CF_ArrestPedQuit				= BIT2,				//Indicates arrest ped finished.
	CF_TakeCustody					= BIT3,				//Indicates custodian is trying to take custody
	CF_CantInterruptSyncScene		= BIT4,				//Indicates we have gone beyond the point of interrupt.
};

class CTaskCuffed : public CTaskFSMClone
{
public:
	struct Tunables : public CTuning
	{
		Tunables();

		fwMvClipSetId m_HandcuffedClipSetId;
		fwMvNetworkDefId m_ActionsHandcuffedNetworkId;

		PAR_PARSABLE;
	};

	CTaskCuffed(CPed* pArrestPed = NULL, fwFlags8 iFlags = 0);

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_CUFFED; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const { return rage_new CTaskCuffed(m_pArrestPed, m_iFlags); }

	// FSM states
	enum State
	{
		State_Start = 0,

		// Waiting to decide if using synchronized scene or fallback
		State_WaitOnArrester,

		// Getting cuffed/uncuffed using synchronised scene
		State_SyncedScene_Prepare,
		State_SyncedScene_Playing,

		// Full cuffing/uncuffing state.
		State_Full_Prepare,
		State_Full_Phase1,
		State_Full_Phase2,
		State_Full_Phase3,

		// Getting cuffed/uncuffed using fallback anims
		State_Fallback_Prepare,
		State_Fallback_WaitForShove,
		State_Fallback_Shoved,

		// Cuffed
		State_Cuffed,

		// uncuffed
		State_Uncuffed,

		// restarted after an abort (used to skip animation).
		State_Restarted,

		State_Finish,
	};

	// PURPOSE: Returns if currently being cuffed/uncuffed using a synchronized scene
	bool IsReceivingSyncedScene() const;

	// PURPOSE: Returns if currently being cuffed using the full arrest sequence.
	bool IsReceivingFullArrest() const;

	// PURPOSE: Returns if currently being cuffed using the procedural fallback method
	bool IsReceivingFallbackArrest() const;

	// PURPOSE: Returns if currently being cuffed
	bool IsBeingCuffed() const;

	// PURPOSE: Returns if currently being uncuffed
	bool IsBeingUncuffed() const;

	// PURPOSE: Returns if currently cuffed
	bool IsCuffed() const;

	// PURPOSE: Notified when arrestee finishes sequence.
	void OnArrestPedFinishedHandshake();

	// PURPOSE: Notified when arrestee quits.
	void OnArrestPedQuit();

	// PURPOSE: Check network state is the same as local state. This lets us know if we are simulating ahead/behind clone ped.
	bool IsInSyncWithNetworkState() const;

	// PURPOSE: Get arresting Ped.
	CPed* GetArrestPed() const { return m_pArrestPed; }

	// PURPOSE: Get cuffed phase.
	float GetPhase() const;

	// PURPOSE: Can we still cancel sync scene?
	bool CanInterruptSyncScene() const { return !m_iFlags.IsFlagSet(CF_CantInterruptSyncScene); }

	// PURPOSE: Allow secondary cuffed task to start running?
	bool CanStartSecondaryCuffedAnims() const;

	static void CreateTask(CPed* pTargetPed, CPed* pInstigatorPed, fwFlags8 iFlags = 0);

protected:

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// PURPOSE: Determine whether or not the task should abort
	// RETURNS: True if should abort
	virtual bool ShouldAbort(const AbortPriority priority, const aiEvent* pEvent);

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const;

	// PURPOSE: Allows NM Control tasks to abort if being replaced by this task
	virtual bool HandlesRagdoll(const CPed* pPed) const;

	// PURPOSE: Called by the nm getup task on the primary task to determine which get up sets to use when
	//			blending from nm. Allows the task to request specific get up types for different a.i. behaviors
	virtual bool UseCustomGetup() { return true; }
	virtual bool AddGetUpSets(CNmBlendOutSetList& sets, CPed* pPed);

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

    // Clone task implementation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual bool                ControlPassingAllowed(CPed *pPed, const netPlayer& player, eMigrationType migrationType);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual bool                OverridesNetworkBlender(CPed *pPed);

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return Start_OnEnter();
	FSM_Return Start_OnUpdate();

	FSM_Return WaitOnArrester_OnUpdate();

	FSM_Return GetUpFromRagdoll_OnEnter();
	FSM_Return GetUpFromRagdoll_OnUpdate();

	FSM_Return SyncedScenePrepare_OnEnter();
	FSM_Return SyncedScenePrepare_OnUpdate();
	FSM_Return SyncedScenePlaying_OnEnter();
	FSM_Return SyncedScenePlaying_OnUpdate();

	FSM_Return FullPrepare_OnEnter();
	FSM_Return FullPrepare_OnUpdate();
	FSM_Return FullPhase1_OnEnter();
	FSM_Return FullPhase1_OnUpdate();
	FSM_Return FullPhase2_OnEnter();
	FSM_Return FullPhase2_OnUpdate();
	FSM_Return FullPhase3_OnEnter();
	FSM_Return FullPhase3_OnUpdate();

	FSM_Return FallbackPrepare_OnEnter();
	FSM_Return FallbackPrepare_OnUpdate();
	FSM_Return FallbackWaitForShove_OnUpdate();
	FSM_Return FallbackShoved_OnEnter();
	FSM_Return FallbackShoved_OnUpdate();
	FSM_Return FallbackShoved_OnExit();

	FSM_Return Cuffed_OnEnter();
	FSM_Return Cuffed_OnUpdate();

	FSM_Return Uncuffed_OnEnter();
	FSM_Return Uncuffed_OnUpdate();

	FSM_Return Restarted_OnEnter();
	FSM_Return Restarted_OnUpdate();

	FSM_Return Finish_OnEnter();

	////////////////////////////////////////////////////////////////////////////////

private:

	////////////////////////////////////////////////////////////////////////////////

	void SetCuffedFlags(bool enabled);
	s32 CheckAndSetCuffedState();

	bool IsArrestPedRunningArrestTask();
	CTaskArrest* GetArrestPedTask();
	s32 GetArrestPedState();

	void SelectCuffingState();

	//
	// Members
	//

	// PURPOSE: The ped arresting.
	RegdPed m_pArrestPed;

	// PURPOSE: Move network helper, contains the interface to the move network
	CMoveNetworkHelper m_moveNetworkHelper;

	// PURPOSE: Helper for streaming in anim dictionary.
	CClipRequestHelper m_ClipRequestHelper;
	CClipRequestHelper m_ClipRequestHelperFull;
	
	static const int ms_nMaxGetUpSets = 4;
	CClipRequestHelper m_GetUpHelpers[ms_nMaxGetUpSets];

	// PURPOSE: Bit "state" flags.
	fwFlags8 m_iFlags;

	// PURPOSE: Have we posted finish event.
	bool m_bPostedEvent : 1;

	//
	// Static members
	//

public:

	static Tunables	sm_Tunables;

	static const strStreamingObjectName ms_GetUpDicts[ms_nMaxGetUpSets];

	// PURPOSE: Move network ids corresponding to control parameters in the move network
	static const fwMvFloatId ms_HeadingDelta;
	static const fwMvFloatId ms_PairedAnimAngle;
	static const fwMvFlagId	ms_ShoveRight;
	static const fwMvFlagId ms_UseFallback;
	static const fwMvFlagId ms_UseUncuff;
	static const fwMvFlagId ms_UseNewAnims;
	static const fwMvBooleanId ms_Phase1Complete;
	static const fwMvBooleanId ms_Phase2Complete;
	static const fwMvBooleanId ms_SetCuffedState;
	static const fwMvRequestId	ms_EnterP2Id;

	////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
public:
	// PURPOSE: Get the name of the specified state
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////
// CClonedTaskCuffedInfo
//////////////////////////////////////////////////////////////////////////

class CClonedTaskCuffedInfo : public CSerialisedFSMTaskInfo
{
public:

	static const int SIZEOF_FLAGS = 8;

	CClonedTaskCuffedInfo(s32 iState, CPed* pArrestPed, u8 iFlags);	
	CClonedTaskCuffedInfo();
	~CClonedTaskCuffedInfo();

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_CUFFED;}	
	virtual CTaskFSMClone *CreateCloneFSMTask();

	virtual bool HasState() const { return true; }
	virtual u32 GetSizeOfState() const { return datBitsNeeded< CTaskCuffed::State_Finish >::COUNT; }

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_OBJECTID(serialiser, m_ArrestPedId, "Arrest Ped");
		SERIALISE_UNSIGNED(serialiser, m_iFlags, SIZEOF_FLAGS, "Flags");
	}

	CPed* GetArrestPed();
	u8 GetFlags() const { return m_iFlags; }

private:

	CClonedTaskCuffedInfo(const CClonedTaskCuffedInfo &);
	CClonedTaskCuffedInfo &operator=(const CClonedTaskCuffedInfo &);

	ObjectId m_ArrestPedId;
	u8 m_iFlags;

	////////////////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////
// CTaskInCustody
//////////////////////////////////////////////////////////////////////////

class CTaskInCustody : public CTaskFSMClone
{
public:
	struct Tunables : public CTuning
	{
		Tunables();

		float m_AbandonDistance;
		float m_FollowRadius;
		Vector3 m_FollowOffset;

		PAR_PARSABLE;
	};

	CTaskInCustody(CPed* pCustodianPed = NULL, bool bSkipToInCustody = false);

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_IN_CUSTODY; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const { return rage_new CTaskInCustody(m_pCustodianPed, GetState() == State_InCustody); }

	// FSM states
	enum State
	{
		State_Start = 0,
		State_BeingPutInCustody,
		State_InCustody,
		State_Finish,
	};

	bool IsBeingTakenIntoCustody() const;

	// PURPOSE: Returns if currently cuffed with a custodian
	bool IsInCustody() const { return GetState() == State_InCustody; }

	// PURPOSE: Get current custodian.
	CPed* GetCustodian() const { return m_pCustodianPed; }

	CTaskArrest *GetCustodianTask() const;

	// PURPOSE: Get phase of custody task.
	float GetPhase() const;

	// PURPOSE: Custodian has quit.
	void OnCustodianPedQuit() { m_bCustodianPedQuit = true; }

protected:

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// PURPOSE: Determine whether or not the task should abort
	// RETURNS: True if should abort
	virtual bool ShouldAbort(const AbortPriority priority, const aiEvent* pEvent);

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const;

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// Clone task implementation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual bool                ControlPassingAllowed(CPed *pPed, const netPlayer& player, eMigrationType migrationType);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual void				UpdateClonedSubTasks(CPed* pPed, int const iState);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return Start_OnEnter();
	FSM_Return Start_OnUpdate();

	FSM_Return BeingPutInCustody_OnUpdate();

	FSM_Return InCustody_OnEnter();
	FSM_Return InCustody_OnUpdate();

	FSM_Return Finish_OnEnter();

	////////////////////////////////////////////////////////////////////////////////

private:

	// PURPOSE: The ped arresting or taking custody of us
	RegdPed m_pCustodianPed;

	// PURPOSE: Indicates custodian ped has finished early (i.e. before this task started properly).
	bool m_bCustodianPedQuit : 1;
	// PURPOSE: Skip straight to in custody, don't go through put in custody state. useful for migration.
	bool m_bSkipToInCustody : 1;

public:

	static Tunables	sm_Tunables;

	////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
public:
	// PURPOSE: Get the name of the specified state
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////
// CClonedTaskInCustodyInfo
//////////////////////////////////////////////////////////////////////////

class CClonedTaskInCustodyInfo : public CSerialisedFSMTaskInfo
{
public:

	CClonedTaskInCustodyInfo(s32 iState, CPed* pCustodianPed);	
	CClonedTaskInCustodyInfo();
	~CClonedTaskInCustodyInfo();

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_IN_CUSTODY;}	
	virtual CTaskFSMClone *CreateCloneFSMTask();

	virtual bool HasState() const { return true; }
	virtual u32 GetSizeOfState() const { return datBitsNeeded< CTaskCuffed::State_Finish >::COUNT; }

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);
		SERIALISE_OBJECTID(serialiser, m_CustodianPedId, "Custodian Ped");
	}

	CPed* GetCustodianPed();

private:

	CClonedTaskInCustodyInfo(const CClonedTaskInCustodyInfo &);
	CClonedTaskInCustodyInfo &operator=(const CClonedTaskInCustodyInfo &);

	ObjectId m_CustodianPedId;
};

//////////////////////////////////////////////////////////////////////////
// CTaskPlayCuffedSecondaryAnims
//////////////////////////////////////////////////////////////////////////

class CTaskPlayCuffedSecondaryAnims : public CTask
{
public:

	// PURPOSE: Macro to allow us to define the state enum and string lists once
	#define PLAY_CUFFED_SECONDARY_ANIMS_STATES(X)		X(State_Start),		\
														X(State_Running),	\
														X(State_Finish)

	// PURPOSE: FSM states
	enum ePlayCuffedSecondaryAnimsState
	{
		PLAY_CUFFED_SECONDARY_ANIMS_STATES(DEFINE_TASK_ENUM)
	};

	CTaskPlayCuffedSecondaryAnims(const fwMvNetworkDefId& networkDefId, 
		const fwMvClipSetId& clipsetId,
		float fBlendInDuration = NORMAL_BLEND_DURATION, 
		float fBlendOutDuration = NORMAL_BLEND_DURATION);

	~CTaskPlayCuffedSecondaryAnims();

	virtual	void CleanUp();
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_PLAY_CUFFED_SECONDARY_ANIMS; }
	virtual aiTask* Copy() const { return rage_new CTaskPlayCuffedSecondaryAnims(m_NetworkDefId, 
		m_ClipSetId, 
		m_fBlendInDuration,
		m_fBlendOutDuration); }

	virtual CTask::FSM_Return ProcessPreFSM();
	virtual CTask::FSM_Return ProcessPostFSM();

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	static void ProcessHandCuffs(CPed &ped);

protected:

	// PURPOSE: Determine whether or not the task should abort
	// RETURNS: True if should abort
	virtual bool ShouldAbort(const AbortPriority priority, const aiEvent* pEvent);

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const;

private:

	FSM_Return	Start_OnEnter();
	FSM_Return	Start_OnUpdate();
	FSM_Return	Running_OnUpdate();

	void UpdateMoVE();

	////////////////////////////////////////////////////////////////////////////////

private:

	CMoveNetworkHelper	m_MoveNetworkHelper;
	fwMvNetworkDefId	m_NetworkDefId;
	fwMvClipSetId		m_ClipSetId;
	float				m_fBlendInDuration;
	float				m_fBlendOutDuration;

	// PURPOSE: Helper for streaming in clipsets
	fwClipSetRequestHelper m_ClipSetRequestHelper;

public:

#if !__FINAL
	// PURPOSE: Get the name of the specified state
	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, PLAY_CUFFED_SECONDARY_ANIMS_STATES)
#endif // !__FINAL

	static const fwMvFloatId	ms_MoveSpeedId;
	static const fwMvFlagId		ms_InVehicleId;

	////////////////////////////////////////////////////////////////////////////////
};

#endif // ENABLE_TASKS_ARREST_CUFFED
	
#endif // INC_TASK_CUFFED_H
