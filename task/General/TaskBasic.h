#ifndef TASK_BASIC_H
#define TASK_BASIC_H

// Rage headers
#include "ai/task/taskchannel.h"

// Game headers
#include "Event/Events.h"
#include "IK/IkManager.h" // Required for default IK priority argument 
#include "Network/Objects/Synchronisation/SyncNodes/PedSyncNodes.h"
#include "Peds/DefensiveArea.h"
#include "Peds/QueriableInterface.h"
#include "Physics/Constrainthandle.h"
#include "script/Handlers/GameScriptHandler.h"
#include "script/Handlers/GameScriptResources.h"
#include "Task/Default/TaskAmbient.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/TaskMove.h"
#include "Task/System/TaskTypes.h"
#include "Task/System/TaskComplex.h"

class CEntity;
class CPed;
class CVehicle;
class CClipPlayer;
class C2dEffect;

///////////////
//Uninterruptable
///////////////

class CTaskUninterruptable : public CTask
{
public:
	CTaskUninterruptable() {SetInternalTaskType(CTaskTypes::TASK_UNINTERRUPTABLE);}
	~CTaskUninterruptable() {}

	virtual aiTask* Copy() const {return rage_new CTaskUninterruptable();}

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_UNINTERRUPTABLE;}
	virtual s32 GetDefaultStateAfterAbort() const { return GetState(); }
	virtual bool ShouldAbort(const AbortPriority iPriority, const aiEvent* ) 
	{
		if(CTask::ABORT_PRIORITY_IMMEDIATE==iPriority)
			return true;
		return false;
	}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32    ) {  return "No state";  };
#endif
	virtual FSM_Return UpdateFSM(const s32 UNUSED_PARAM(iState), const FSM_Event UNUSED_PARAM(iEvent))
	{
		return FSM_Continue;
	}
};


///////////////
//Network clone
///////////////

class CTaskNetworkClone : public CTask
{
public:

	CTaskNetworkClone()
	{
		SetInternalTaskType(CTaskTypes::TASK_NETWORK_CLONE);
	}

	~CTaskNetworkClone()
	{
	}

	virtual aiTask* Copy() const {return rage_new CTaskNetworkClone();}

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NETWORK_CLONE;}

    virtual s32 GetDefaultStateAfterAbort() const { return GetState(); }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32    ) {  return "No state";  };
#endif

private:

    virtual FSM_Return UpdateFSM(const s32 UNUSED_PARAM(iState), const FSM_Event UNUSED_PARAM(iEvent))
    {
        return FSM_Continue;
    }
};

//-------------------------------------------------------------------------
// Task that creates and manages movement task
// This complex version can also be given a normal subtask to run alongside
//-------------------------------------------------------------------------
class CTaskComplexControlMovement : public CTaskComplex
{
public:
	friend class CTaskCounter;
	enum
	{
		TerminateOnMovementTask = 0,
		TerminateOnSubtask,
		TerminateOnEither,
		TerminateOnBoth
	};
	enum
	{
		Flag_AllowClimbLadderSubTask = 0x1
	};

	CTaskComplexControlMovement( CTask* pMoveTask, CTask* pSubTask = NULL, s32 iTerminationType = TerminateOnMovementTask, float fWarpTimer = 0.0f, bool bMoveTaskAlreadyRunning = false, u32 iFlags = 0 );
	~CTaskComplexControlMovement();

	virtual aiTask* Copy() const {return rage_new CTaskComplexControlMovement(m_pBackupMovementSubtask?(CTask*)m_pBackupMovementSubtask->Copy():NULL, m_pMainSubTask?(CTask*)m_pMainSubTask->Copy():NULL, m_iTerminationType, m_fWarpTimer, false, m_iFlags);}

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT;}

	// Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	s32 GetDefaultStateAfterAbort() const { return CTaskComplex::State_ControlSubTask; }

	// Standard complex task interface
	virtual aiTask* CreateNextSubTask(CPed* pPed);
	virtual aiTask* CreateFirstSubTask(CPed* pPed);
	virtual aiTask* ControlSubTask(CPed* pPed);

	// Generate default task
	virtual aiTask* GenerateDefaultTask(CPed* pPed);

	// Interface for changing the move tasks
	void SetNewMoveTask( CTask* pSubTask );
	// Interface for changing the main tasks
	void SetNewMainSubtask( CTask* pSubTask );
	// Returns the type of the movement task requested
	s32	GetMoveTaskType( void ) const {return  !m_pBackupMovementSubtask ? CTaskTypes::TASK_NONE: m_pBackupMovementSubtask->GetTaskType(); }
	// Returns the type of the main subtask
	s32	GetMainSubTaskType( void ) const {return !m_pMainSubTask ? CTaskTypes::TASK_NONE: m_pMainSubTask->GetTaskType(); }

	u32 GetFlags() { return m_iFlags; }
	float GetWarpTimer() const { return m_fWarpTimer; }

	// returns the stored copy of the movement task this task is in charge of, this
	// version is ONLY used to restart the task if removed and cannot be used to check status
	CTask*	GetBackupCopyOfMovementSubtask( void ) {return m_pBackupMovementSubtask;}

	// Gets and sets the target of the current movement task
	void	SetTarget			( const CPed* pPed, const Vector3& vTarget, const bool bForce = false );
	void	SetTargetRadius		( const CPed* pPed, const float fTargetRadius );
	void	SetSlowDownDistance	( const CPed* pPed, const float fSlowDownDistance );
	Vector3 GetTarget			(void) const;
	Vector3 GetLookAheadTarget() const { return GetTarget(); }

	// Returns true if the movement task this task is interested in is up and running
	bool	IsMovementTaskRunning();
	// Returns the Running movement task associated with this control task, returns NULL if this tasks movement task is not running
	CTask*	GetRunningMovementTask( const CPed* pPed = NULL );
	// Creates the movement and any subtask
	aiTask* BeginMovementTasks(CPed* pPed);

	void SetTerminationType(s32  iTerminationType) { m_iTerminationType = iTerminationType; }
	void SetMoveBlendRatio( const CPed* pPed, const float fMoveBlend );
	float GetMoveBlendRatio() const;

	virtual bool IsValidForMotionTask(CTaskMotionBase& task) const;
	
	virtual bool HandlesRagdoll(const CPed* pPed) const;

    // Function to create cloned task information
	virtual CTaskInfo* CreateQueriableState() const;

	// Checks if we are riding on a mount that has its own task
	static bool ShouldYieldToMount(const CPed& ped);

	bool MayClimbLadderAsSubtask()
	{
		return (ms_bCanClimbLadderAsSubtask && (((m_iFlags & Flag_AllowClimbLadderSubTask)!=0) || ms_bForceAllowClimbLadderAsSubtask));
	}

	void SetAllowClimbLadderSubtask(const bool b)
	{
		if(b) { m_iFlags |= Flag_AllowClimbLadderSubTask; } else { m_iFlags &= ~Flag_AllowClimbLadderSubTask; }
	}

	void SetIsClimbingLadder(const Vector3 & vLadderTarget, const float fLadderHeading, const float fLadderMBR)
	{
		Assert(MayClimbLadderAsSubtask());
		Assert(!m_bClimbingLadder);
		Assert(!m_bLeavingVehicle);

		if(MayClimbLadderAsSubtask())
		{
			m_vLadderTarget = vLadderTarget;
			m_fLadderHeading = fLadderHeading;
			m_fLadderMBR = fLadderMBR;

			m_bClimbingLadder = true;
			m_bLastClimbingLadderFailed = false;
		}
	}
	bool GetIsClimbingLadder	 () const { return m_bClimbingLadder; }
	bool HasLastClimbingLadderFailed () const { return m_bLastClimbingLadderFailed; }
	void OnClimbingLadderFailed	 ()	{ m_bLastClimbingLadderFailed = true; }

	static const float ms_fMaxWalkRoundEntityDuration;
	static bank_bool ms_bCanClimbLadderAsSubtask;
	static bank_bool ms_bForceAllowClimbLadderAsSubtask;

protected:

	// PURPOSE: Determine whether or not the task should abort
	// RETURNS: True if should abort
	virtual bool ShouldAbort(const AbortPriority priority, const aiEvent* pEvent);

	// PURPOSE: Get the ped that should be running the movement task.
	// RETURNS: Pointer to this task's ped, or, if riding, its mount.
	CPed* GetMovementTaskPed();

private:

	Vector3 m_vLadderTarget;

	RegdTask  m_pBackupMovementSubtask;
	RegdTask  m_pMainSubTask;
	s32 m_iTerminationType;
	u32 m_iFlags;

	float  m_fWarpTimer;
	float  m_fTimeBeenWalkingRoundEntity; // This is here to prevent CTaskComplexWalkRoundEntity event response from locking out the main movement task indefinately
	float m_fLadderHeading;
	float m_fLadderMBR;

	unsigned int m_bNewMainTask :1;
	unsigned int m_bNewMoveTask :1;
	unsigned int m_bWaitingForSubtaskToAbort :1;
	unsigned int m_bLeavingVehicle :1;
	unsigned int m_bMovementTaskCompleted :1;
	unsigned int m_bMoveTaskAlreadyRunning :1;
	unsigned int m_bClimbingLadder :1;
	unsigned int m_bLastClimbingLadderFailed :1;
};


/////////////
//Pause
/////////////


//-------------------------------------------------------------------------
// Task info for task pause
//-------------------------------------------------------------------------
class CClonedPauseInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedPauseInfo() : m_iPauseTime(0), m_iTimerFlags(0) {}
	CClonedPauseInfo(u32 iPauseTime, u32 iTimerFlags) : m_iPauseTime(iPauseTime), m_iTimerFlags(iTimerFlags) {}
	~CClonedPauseInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_PAUSE;}

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		u32 iPauseTime = m_iPauseTime;
		SERIALISE_UNSIGNED(serialiser, iPauseTime, SIZEOF_PAUSETIME, "Pause Time");
		m_iPauseTime = iPauseTime;

		u32 iTimerFlags = m_iTimerFlags;
		SERIALISE_UNSIGNED(serialiser, iTimerFlags, SIZEOF_TIMERFLAGS, "Timer Flags");
		m_iTimerFlags = iTimerFlags;
	}

private:

	static const unsigned int SIZEOF_PAUSETIME = 16;
	static const unsigned int SIZEOF_TIMERFLAGS = 2;

	u32 m_iPauseTime : SIZEOF_PAUSETIME;
	u32 m_iTimerFlags : SIZEOF_TIMERFLAGS;
};

//FSM Version
class CTaskPause : public CTaskFSMClone
{
public:

	typedef enum
	{
		State_Start = 0,
		State_GameTimerPause,
		State_SystemTimerPause,
		State_Finish
	} PauseState;

	enum TimerFlags
	{
		TF_GameTimer		= BIT0,
		TF_SystemTimer		= BIT1
	};

	CTaskPause(const int iPauseTime, const s32 iTimerFlags = TF_GameTimer);
	~CTaskPause();

	//Gets/sets flags 
	fwFlags32& GetFlags() { return m_TimerFlags; }

	//Creates a clone of our pause task. 
	virtual aiTask* Copy() const {return rage_new CTaskPause(m_iPauseTime, m_TimerFlags);}

	//Gets the type of task 
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_PAUSE;}

	//Updates the FSM of the pause task.
	FSM_Return		UpdateFSM			(const s32 iState, const FSM_Event iEvent);

	//Tells the pause task to finish if its aborted
	virtual s32   GetDefaultStateAfterAbort() const {return State_Finish;}

	// Clone task implementation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

private:
	//Time set for pause duration
	int m_iPauseTime;

	//Flags to define which timer the task will use
	fwFlags32 m_TimerFlags; 

	// Start state
	FSM_Return Start_OnUpdate(CPed* UNUSED_PARAM(pPed));

	// Game timer Pause state
	void		GameTimerPause_OnEnter();
	FSM_Return	GameTimerPause_OnUpdate();

	CTaskGameTimer m_GameTimer;

	// System timer
	void		SystemTimerPause_OnEnter();
	FSM_Return	SystemTimerPause_OnUpdate();

	CTaskSystemTimer m_SystemTimer;
};


/////////////
//CTaskSayAudio
/////////////

class CTaskSayAudio : public CTask
{
public:
	typedef enum
	{
		State_Start = 0,
		State_WaitingToSayAudio,
		State_SayAudio,
		State_SpeakingAudio,
		State_WaitingToFinish,
		State_Finish
	} SayAudioState;

	CTaskSayAudio(const char* szContext, float fDelayBeforeAudio, float fDelayAfterAudio, CEntity* m_pFaceTowardEntity,u32 szContextReply = 0, float fHeadingRateMultiplier=2.0f);
	~CTaskSayAudio();

	virtual aiTask* Copy() const {return rage_new CTaskSayAudio(m_szAudioName, m_fDelayAfterAudio, m_fDelayAfterAudio, m_pFaceTowardEntity, m_AudioReplyHash, m_fHeadingRateMultiplier );}
	virtual s32 GetTaskTypeInternal() const {return CTaskTypes::TASK_SAY_AUDIO;}

	// Main FSM implementation:
	FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual s32 GetDefaultStateAfterAbort() const {return State_Finish;}

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

protected:

	// Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

private:
	// Helper functions for the FSM implementation:
	FSM_Return Start_OnUpdate(CPed* UNUSED_PARAM(pPed));
	FSM_Return WaitingToSayAudio_OnUpdate(CPed* UNUSED_PARAM(pPed));
	FSM_Return SayAudio_OnUpdate(CPed* pPed);
	void       SpeakingAudio_OnEnter(CPed* UNUSED_PARAM(pPed));
	FSM_Return SpeakingAudio_OnUpdate(CPed* pPed);
	void       SpeakingAudio_OnExit(CPed* UNUSED_PARAM(pPed));
	FSM_Return WaitingToFinish_OnUpdate(CPed* UNUSED_PARAM(pPed));
	FSM_Return Finish_OnUpdate(CPed* pPed);

	void MakePedFaceTowardEntity(CPed* pPed);

	// Private member variables:
	enum { AUDIO_NAME_LENGTH = 32 };
	bool          m_bIsSpeaking;
	u32			  m_AudioReplyHash;
	char		  m_szAudioName[AUDIO_NAME_LENGTH];
	float		  m_fDelayBeforeAudio;
	float		  m_fDelayAfterAudio;
	RegdEnt       m_pFaceTowardEntity;
	float		  m_fHeadingRateMultiplier;
};


//////////////////
// toggle crouch:
// use: this is purely used to toggle on/off a secondary crouch task from the script.
//////////////////

class CTaskCrouchToggle : public CTask
{
public:

	enum{ TOGGLE_CROUCH_AUTO = -1, TOGGLE_CROUCH_OFF, TOGGLE_CROUCH_ON };
	typedef enum
	{
		State_Start = 0,
		State_CrouchOff,
		State_CrouchOn,
		State_Finish
	} CrouchState;

	CTaskCrouchToggle(int nSwitchOn = -1);
	~CTaskCrouchToggle();

	virtual aiTask* Copy() const {return rage_new CTaskCrouchToggle(m_nToggleType);}

	virtual s32 GetTaskTypeInternal() const {return CTaskTypes::TASK_CROUCH_TOGGLE;}

	// Main FSM implementation:
	FSM_Return		UpdateFSM			(const s32 iState, const FSM_Event iEvent);
	virtual s32   GetDefaultStateAfterAbort() const {return State_Finish;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

private:
	// Helper functions for the FSM implementation:
	FSM_Return Start_OnUpdate(CPed* pPed);
	void CrouchOn_OnEnter(CPed* pPed);
	void CrouchOff_OnEnter(CPed* pPed);

	// Private member variables:
	int m_nToggleType;
};

class CClonedDoNothingInfo : public CSerialisedFSMTaskInfo
{
public:

	CClonedDoNothingInfo() : m_Duration(0) { }
	CClonedDoNothingInfo(s32 iDuration) : m_Duration(iDuration < 0 ? 0 : static_cast<u32>(iDuration))
	{

	}

    virtual CTask  *CreateLocalTask(fwEntity *pEntity);
	virtual s32		GetTaskInfoType() const { return CTaskInfo::INFO_TYPE_STAND_STILL; }
    virtual int GetScriptedTaskType() const { return SCRIPT_TASK_STAND_STILL; }

	s32 GetDuration() { return (m_Duration == 0) ? -1 : static_cast<s32>(m_Duration); }

    void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		bool bSyncDuration = (m_Duration > 0);
		SERIALISE_BOOL(serialiser, bSyncDuration, "Has Duration");
		if(bSyncDuration || serialiser.GetIsMaximumSizeSerialiser())
		{
			u32 nDuration = m_Duration;
			SERIALISE_UNSIGNED(serialiser, nDuration, SIZEOF_DURATION, "Duration");
			m_Duration = nDuration;
		}
		else
			m_Duration = 0;
    }

private:

	static const unsigned int SIZEOF_DURATION = 16; 
	u32 m_Duration : SIZEOF_DURATION;
};

class CTaskDoNothing : public CTask
{
public:

	static const int ms_iDefaultDuration;

	enum
	{
		State_Initial,
		State_StandStillForever,
		State_StandStillTimed
	};

	CTaskDoNothing(const int iDurationMs, const int iNumFramesToRun=0, bool makeMountStandStill = true, bool bEnableTimeslicing = false);
	virtual ~CTaskDoNothing();

	virtual CTask * Copy() const { return rage_new CTaskDoNothing(m_iDurationMs, m_iNumFramesToRun, m_bMakeMountStandStill, m_bEnableTimeslicing); }

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_DO_NOTHING; }
	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

	virtual bool ProcessMoveSignals();

    virtual CTaskInfo *CreateQueriableState() const;

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
		case State_Initial : return "Initial";
		case State_StandStillForever : return "StandStillForever";
		case State_StandStillTimed : return "StandStillTimed";
		default: Assert(0); return NULL;
		}
	}
#endif

protected:

	FSM_Return StateInitial_OnEnter(CPed * pPed);
	FSM_Return StateInitial_OnUpdate(CPed * pPed);
	FSM_Return StateStandStillForever_OnEnter(CPed * pPed);
	FSM_Return StateStandStillForever_OnUpdate(CPed * pPed);
	FSM_Return StateStandStillTimed_OnEnter(CPed * pPed);
	FSM_Return StateStandStillTimed_OnUpdate(CPed * pPed);
	bool StandStillTimed_OnProcessMoveSignals();

	// PURPOSE:	Helper function to make the ped stand still, shared between states.
	void DoStandStill(CPed* pPed);

	// PURPOSE: Helper function to enable timeslicing of intelligence update on the ped.
	void ActivateTimeslicing(CPed* pPed);

	int m_iDurationMs;
	int m_iNumFramesToRun;
	int m_iNumFramesRunSoFar;
	CTaskGameTimer m_Timer;

	// PURPOSE:	True if we should run a CTaskMoveStandStill on the mount
	//			if we're riding. This is normally the case, except for when
	//			running CTaskDoNothing on the rider while CTaskComplexControlMovement
	//			gives another movement task to the horse.
	bool m_bMakeMountStandStill;

	// Whether to apply timeslicing when running this task
	bool m_bEnableTimeslicing;
};




class CTaskComplexUseMobilePhoneAndMovement : public CTaskComplex
{

public:

	CTaskComplexUseMobilePhoneAndMovement(s32 iTime = -1) { m_iTime = iTime; SetInternalTaskType(CTaskTypes::TASK_COMPLEX_USE_MOBILE_PHONE_AND_MOVEMENT);}
	~CTaskComplexUseMobilePhoneAndMovement() {};

	virtual aiTask* Copy() const {return rage_new CTaskComplexUseMobilePhoneAndMovement(m_iTime);}

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_COMPLEX_USE_MOBILE_PHONE_AND_MOVEMENT;}   

	s32 GetDefaultStateAfterAbort() const { return CTaskComplex::State_ControlSubTask; }

	virtual aiTask* CreateNextSubTask	(CPed* UNUSED_PARAM(pPed)) {return NULL;}
	virtual aiTask* CreateFirstSubTask	(CPed* pPed);
	virtual aiTask* ControlSubTask		(CPed* UNUSED_PARAM(pPed)) {return GetSubTask();}

protected:

    // Generic cleanup function, called when the task quits itself or is aborted
    virtual	void CleanUp();

private:
	s32 m_iTime;
};


//-------------------------------------------------------------------------
// Helper class used to store and query a list of tasks
//-------------------------------------------------------------------------
class CTaskList
{
public:
	enum
	{
		MAX_LIST_SIZE=16
	};
	friend class CTaskCounter;

	CTaskList();
	virtual ~CTaskList();
	void Flush();
	bool AddTask(aiTask* pTask);
	void AddTask(const int index, aiTask* pTask);
	bool Contains(aiTask* p);
	aiTask* Contains(const int iTaskType);

	bool IsEmpty() const 
	{
		return (m_tasks[0] ? false : true);
	}

	s32 GetFirstEmptySlotIndex() const
	{
		for (s32 i=0; i<MAX_LIST_SIZE; ++i)
		{
			if (!m_tasks[i])
			{
				return i;
			}
		}
		return -1;
	}

	u32 GetNumTasks() const
	{
		u32 count = 0;

		for (s32 i=0; i<MAX_LIST_SIZE; ++i)
		{
			if (m_tasks[i])
			{
				count++;
			}
		}

		return count;
	}

	const aiTask* GetTask(const int i)  const {return m_tasks[i];}
	void From( const CTaskList& );
	aiTask* GetLastAddedTask() const;
private:
	RegdaiTask m_tasks[MAX_LIST_SIZE];
};

///////////////////////////////////////////////////////////////////////////////////
// Wrapper class which maintains a list of tasks to be run sequentially
// and provides functions to obtain tasks from the list and increment the progress
// this is used by CTaskUseSequence, which provides a means for actors to 
// follow sequences of tasks defined by either code or script
// CTaskUseSequence handles deletion of the list in its constructor
///////////////////////////////////////////////////////////////////////////////////

class CTaskSequenceList
{
public:

	FW_REGISTER_CLASS_POOL(CTaskSequenceList);

	// Statics
	static const s32 MAX_STORAGE = 31;

	enum
	{
		REPEAT_NOT = 0,
		REPEAT_FOREVER,
		NUM_REPEAT_MODES
	};

	// Construct an empty task sequence list
	CTaskSequenceList();

	// Copy constructor
	CTaskSequenceList(const CTaskSequenceList& other);

	// Destructor
	~CTaskSequenceList();

	// Create an new copy of the list (Make sure the copy is passed into a new TaskUseSequence otherwise the pointer won't get cleaned up)
	CTaskSequenceList* Copy() const;

	///////////////////////////////////////////////////////////

	// Functions to keep track of number of references to static sequence lists in CTaskSequences::ms_TaskSequenceLists

	// Increment ref count
	void Register() { ++m_iRefCount; }	

	// Delete all tasks from list
	void Flush();						

	// If list has no references try to delete all tasks
	void TryToEmpty()				
	{
		if (m_iRefCount == 0)
		{
			Flush();
		}
	}

	// Decrement ref count and try to empty task list
	void DeRegister()				
	{		
		--m_iRefCount;
		taskAssert(m_iRefCount >= 0);
		TryToEmpty();
	}

	// Checks if the task list is empty
	bool IsEmpty() const { return m_taskList.IsEmpty(); }

	///////////////////////////////////////////////////////////

	// Main interface functions

	// Adds a new task to the list, deletes the task and returns false if it can't add 
	bool AddTask(aiTask* pTask);

	const aiTask* GetTask(const int i)  const { return m_taskList.GetTask(i); }
	aiTask* GetLastAddedTask() const { return m_taskList.GetLastAddedTask();}

	// Sets whether or not to repeat the sequence task list upon reaching the end task
	void SetRepeatMode(const s32 iRepeatMode)	{ m_iRepeatMode = iRepeatMode; }	
	s32  GetRepeatMode() const { return m_iRepeatMode; }

	const CTaskList& GetTaskList() const { return m_taskList; }

	void PreventMigration()				{ m_bPreventMigration = true; }
	bool IsMigrationPrevented() const   { return m_bPreventMigration; }

private:

	CTaskList	m_taskList;					// Stores the tasks to be run in the sequence task
	s32			m_iRepeatMode : 2;			// Stores whether or not to repeat the sequence when it's finished
	u32			m_bPreventMigration : 1;	// Stores whether the sequence is prevented from migrating to non-participants of the script that created it.
	s32			m_iRefCount;				// Stores the number of references to this task list
};

/////////////////////////////////
//Perform a sequence of movement tasks
/////////////////////////////////

class CTaskMoveSequence : public CTaskMove
{
public:

	enum
	{
		REPEAT_NOT=0,
		REPEAT_FOREVER
	};
	enum
	{
		State_Initial,
		State_PlayingSequence
	};

	friend class CTaskCounter;
	friend class CEventCounter;

	CTaskMoveSequence();
	~CTaskMoveSequence();

	void Flush();

	void SetRepeatMode(const int iRepeatMode) 
	{
		m_iRepeatMode=iRepeatMode;
	}
	void SetProgress(const int iProgress)
	{
		m_iProgress=iProgress;
	}

	virtual aiTask* Copy() const 
	{
		CTaskMoveSequence* p=rage_new CTaskMoveSequence();
		p->m_taskList.From(m_taskList);
		p->m_iRepeatMode=m_iRepeatMode;
		p->m_iProgress=m_iProgress;
		return p;
	}

	virtual int GetTaskTypeInternal() const; 
#if !__FINAL
	virtual void Debug() const;
	virtual atString GetName() const;
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
		case State_Initial: return "Initial";
		case State_PlayingSequence: return "PlayingSequence";
		default : return NULL;
		}
	}
#endif   

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

	FSM_Return Initial_OnEnter();
	FSM_Return Initial_OnUpdate();
	FSM_Return PlayingSequence_OnEnter();
	FSM_Return PlayingSequence_OnUpdate();

	aiTask* CreateFirstSubTaskInSequence(int & iProgress);
	aiTask* CreateNextSubTaskInSequence(int & iProgress, int & iRepeatProgress);

	bool AddTask(aiTask* pTask);
	void AddTask(const int index, aiTask* pTask);

	int GetProgress() const {return m_iProgress;}

	bool Contains(aiTask* p) {return m_taskList.Contains(p);}
	aiTask* Contains(const int iTaskType){return m_taskList.Contains(iTaskType);}

	int IsEmpty() const {return m_taskList.IsEmpty();}

	const aiTask* GetTask(const int i) const {return m_taskList.GetTask(i);}

	void SetCanBeEmptied(const bool b) 
	{
		if ( (0==m_iRefCount) && (true == b) )
		{
			m_bCanBeEmptied=false;
			Flush();
		}
		else
		{
			m_bCanBeEmptied=b;
		}
	}
	void Register() 
	{
		m_iRefCount++;
	}
	void DeRegister() 
	{
		m_iRefCount--;
		if((0==m_iRefCount) && (m_bCanBeEmptied))
		{
			m_bCanBeEmptied=false;
			Flush();
		}
	}

	virtual bool HasTarget() const { return false; }
	virtual Vector3 GetTarget(void) const 
	{
		Assertf(0, "CTaskMoveSequence : Target requested from Movement task sequence!" );
		return Vector3( 0.0f, 0.0f, 0.0f );
	}

	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }

	virtual float GetTargetRadius(void) const 
	{
		Assertf(0, "CTaskMoveSequence : Target radius requested from Movement task sequence!" );
		return 0.0f;
	}

private:
	CTaskList m_taskList;
	int m_iProgress;
	int m_iRepeatMode;
	int m_iRepeatProgress;
	int m_iRefCount;
	bool m_bCanBeEmptied;
};

class CTaskSequences
{
public:

	enum
	{
		MAX_NUM_SEQUENCE_TASKS=128
	};

	CTaskSequences(){}
	~CTaskSequences(){}

	static void Shutdown(void);

	static int ms_iActiveSequence;
	static bool ms_bIsOpened[MAX_NUM_SEQUENCE_TASKS];
	static CTaskSequenceList ms_TaskSequenceLists[MAX_NUM_SEQUENCE_TASKS];
	static void Init();
	static s32 GetAvailableSlot(bool bSequenceForMission);
	static bool IsAMissionSequence(u32 sequenceId) { return sequenceId >= MAX_NUM_SEQUENCE_TASKS / 2; }

private:

};


///////////////////
// TriggerLookAt //
///////////////////

class CTaskTriggerLookAt 
	: public CTask
{
public:

	CTaskTriggerLookAt(const CEntity* pEntity, s32 time, eAnimBoneTag offsetBoneTag, const Vector3& offsetPos, s32 flags, float speed=0.25f, s32 blendTime=1000, CIkManager::eLookAtPriority priority=CIkManager::IK_LOOKAT_MEDIUM);
	~CTaskTriggerLookAt();

	virtual aiTask*	Copy() const;

	virtual int		GetTaskTypeInternal() const {return CTaskTypes::TASK_TRIGGER_LOOK_AT;}

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	FSM_Return		UpdateFSM( const s32 iState, const FSM_Event iEvent );
	virtual s32	GetDefaultStateAfterAbort() const { return State_finish; }

	// Function to create cloned task information
	virtual CTaskInfo* CreateQueriableState() const;

private:

	FSM_Return		Start_OnUpdate ( );
	FSM_Return		TriggerLook_OnUpdate ( CPed* pPed );

	typedef enum {State_start = 0, State_triggerLook, State_finish} State;

	Vector3 m_offsetPos;
	RegdConstEnt m_pEntity;
	s32 m_time;
	eAnimBoneTag m_offsetBoneTag;
	CIkManager::eLookAtPriority	m_priority;
	s32 m_blendTime;
	float m_speed;
	s32 m_flags;
	bool m_nonNullEntity;

};

//-------------------------------------------------------------------------
// Task info for look at
//-------------------------------------------------------------------------
class CClonedLookAtInfo : public CSerialisedFSMTaskInfo
{
public:

	CClonedLookAtInfo(ObjectId lookAtEntityID, const s32 time, u32 flags)
		: m_LookAtEntity(true)
		, m_EntityID(lookAtEntityID)
		, m_Position(Vector3(VEC3_ZERO))
		, m_Time(time < 0 ? 0 : (u32)time)
		, m_Flags(flags)
	{
	}

	CClonedLookAtInfo(const Vector3& position, const s32 time, u32 flags)
		: m_LookAtEntity(false)
		, m_EntityID(NETWORK_INVALID_OBJECT_ID)
		, m_Position(position)
		, m_Time(time < 0 ? 0 : (u32)time)
		, m_Flags(flags)
	{
	}

	CClonedLookAtInfo()
		: m_LookAtEntity(false)
		, m_EntityID(NETWORK_INVALID_OBJECT_ID)
		, m_Position(Vector3(VEC3_ZERO))
		, m_Time(0)
		, m_Flags(0)
	{
	}

	virtual CTask  *CreateLocalTask(fwEntity *pEntity);

	virtual s32		GetTaskInfoType() const { return CTaskInfo::INFO_TYPE_LOOK_AT; }

	void Serialise(CSyncDataBase& serialiser)
	{
		bool bHasTime = m_Time > 0;

		SERIALISE_BOOL(serialiser, m_LookAtEntity);

		if (m_LookAtEntity && !serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_OBJECTID(serialiser, m_EntityID, "Entity");
		}
		else
		{
			SERIALISE_POSITION(serialiser, m_Position);
		}

		SERIALISE_BOOL(serialiser, bHasTime);

		if (bHasTime || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_Time, SIZEOF_TIME, "Time");
		}
		else
		{
			m_Time = 0;
		}

		SERIALISE_UNSIGNED(serialiser, m_Flags, SIZEOF_FLAGS, "Flags");
	}

private:

	static const unsigned SIZEOF_TIME = 12;
	static const unsigned SIZEOF_FLAGS = LF_NUM_FLAGS;

	bool		  m_LookAtEntity;
	ObjectId	  m_EntityID; 
	Vector3		  m_Position;
	u32           m_Time;    
	u32           m_Flags; 
};

/////////////////
// ClearLookAt //
/////////////////

class CTaskClearLookAt : public CTask
{
public:

	CTaskClearLookAt();
	~CTaskClearLookAt();

	virtual CTask*	Copy() const {return rage_new CTaskClearLookAt();}

	virtual int		GetTaskTypeInternal() const {return CTaskTypes::TASK_CLEAR_LOOK_AT;}

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	FSM_Return		UpdateFSM( const s32 iState, const FSM_Event iEvent );
	virtual s32	GetDefaultStateAfterAbort() const { return State_finish; }

private:

	FSM_Return		AbortLook_OnUpdate(CPed* pPed);

	typedef enum {State_abortLook = 0, State_finish} State; 

};

class CClonedClearLookAtInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedClearLookAtInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_CLEAR_LOOK_AT;}

	virtual CTask *CreateLocalTask(fwEntity*) { return rage_new CTaskClearLookAt(); }
};

///////////////////////
//Use a mobile phone
///////////////////////

class CTaskComplexUseMobilePhone : public CTaskComplex
{
public:

	CTaskComplexUseMobilePhone(const int iDuration=-1);
	~CTaskComplexUseMobilePhone();

	virtual aiTask* Copy() const {
		return rage_new CTaskComplexUseMobilePhone(m_iDuration);}

	void CleanUp();

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_COMPLEX_USE_MOBILE_PHONE;} 

	s32 GetDefaultStateAfterAbort() const { return CTaskComplex::State_ControlSubTask; }

	virtual aiTask* CreateNextSubTask(CPed* pPed);
	virtual aiTask* CreateFirstSubTask(CPed* pPed);
	virtual aiTask* ControlSubTask(CPed* pPed);

	static fwMvClipSetId GetPhoneClipGroup(CPed* pPed);

	void Stop(CPed* pPed);

	virtual FSM_Return ProcessPreFSM();

protected:

	// PURPOSE: Handle aborting the task - this will be called in addition to CleanUp when we are aborting
	virtual void DoAbort(const AbortPriority priority, const aiEvent* pEvent);

private:
	void RemovePhoneModel(CPed* pPed);

	int m_iDuration;
	CTaskGameTimer m_timer;
	u32 m_iModelIndex;
	bool m_bIsAborting;
	bool m_bQuit;
	bool m_bModelReferenced;
	bool m_bFromTexting;
	bool m_bIsQuitting;
	bool m_bGivenPhone;
	s32	m_iPreviousSelectedWeapon;
};

////////////////////////////
//Generic set var task
////////////////////////////
class CTaskSetCharState : public CTaskFSMClone
{
public:
	typedef enum {
		State_start, 
		State_finish
	} State;

	CTaskSetCharState(){}

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32    ) { return "N/A"; };
#endif

	FSM_Return UpdateFSM( const s32 UNUSED_PARAM(iState), const FSM_Event UNUSED_PARAM(iEvent) )
	{
		SetCharState();
		return FSM_Quit;
	}

	virtual s32		GetDefaultStateAfterAbort() const { return State_finish; }

	// Clone task implementation
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent) { return UpdateFSM(iState, iEvent); }

private:

	virtual void SetCharState() = 0;
};

////////////////////////////
//Set blocking of non-temporary events
////////////////////////////
class CClonedSetBlockingOfNonTemporaryEventsInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedSetBlockingOfNonTemporaryEventsInfo() : m_bBlock(false) {}
	CClonedSetBlockingOfNonTemporaryEventsInfo(bool block) : m_bBlock(block) {}
	~CClonedSetBlockingOfNonTemporaryEventsInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_SET_BLOCKING_OF_NON_TEMPORARY_EVENTS;}

	virtual CTaskFSMClone *CreateCloneFSMTask();
	virtual CTask         *CreateLocalTask(fwEntity *pEntity);

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_BOOL(serialiser, m_bBlock, "Block");
	}

private:

	bool m_bBlock;
};

class CTaskSetBlockingOfNonTemporaryEvents : public CTaskSetCharState
{
public:
	CTaskSetBlockingOfNonTemporaryEvents(bool bBlock)
		:m_bBlock(bBlock)
	{
		SetInternalTaskType(CTaskTypes::TASK_SET_BLOCKING_OF_NON_TEMPORARY_EVENTS);
	}
	virtual ~CTaskSetBlockingOfNonTemporaryEvents() {}

	virtual aiTask* Copy() const {return rage_new CTaskSetBlockingOfNonTemporaryEvents(m_bBlock);}

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_SET_BLOCKING_OF_NON_TEMPORARY_EVENTS;}

	virtual CTaskInfo*			CreateQueriableState() const;
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed) { return CreateTaskForClonePed(pPed); }

private:
	void SetCharState();
	bool m_bBlock;
};

////////////////////////////
//Force motion state
////////////////////////////
class CClonedForceMotionStateInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedForceMotionStateInfo() : m_State(0), m_bForceRestart(false) {}
	CClonedForceMotionStateInfo(u32 state, bool forceRestart) : m_State(state), m_bForceRestart(forceRestart) {}
	~CClonedForceMotionStateInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_FORCE_MOTION_STATE;}

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_UNSIGNED(serialiser, m_State, 32, "State");
		SERIALISE_BOOL(serialiser, m_bForceRestart, "Force restart");
	}

private:

	u32  m_State;
	bool m_bForceRestart;
};

class CTaskForceMotionState : public CTaskSetCharState
{
public:
	CTaskForceMotionState(CPedMotionStates::eMotionState state, bool forceRestart = false)
		: m_State(state)
		, m_bForceRestart(forceRestart)
	{
		SetInternalTaskType(CTaskTypes::TASK_FORCE_MOTION_STATE);
	}
	virtual ~CTaskForceMotionState() {}

	virtual aiTask* Copy() const {return rage_new CTaskForceMotionState(m_State, m_bForceRestart);}

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_FORCE_MOTION_STATE;}

	virtual CTaskInfo*			CreateQueriableState() const;
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed) { return CreateTaskForClonePed(pPed); }

private:
	void SetCharState();
	CPedMotionStates::eMotionState m_State;
	bool m_bForceRestart;
};

////////////////////////////
//Set the decision maker
////////////////////////////

class CTaskSetCharDecisionMaker : public CTask
{
public:

	CTaskSetCharDecisionMaker(const u32 uDM)
		: m_uDM(uDM)
	{
		SetInternalTaskType(CTaskTypes::TASK_SET_CHAR_DECISION_MAKER);
	}

	virtual aiTask* Copy() const {return rage_new CTaskSetCharDecisionMaker(m_uDM);}

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_SET_CHAR_DECISION_MAKER;}

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual s32		GetDefaultStateAfterAbort() const { return State_finish; }
	//virtual bool ProcessPed(CPed* pPed);

private:

	FSM_Return	Start_OnUpdate(CPed* pPed);

		typedef enum {
			State_start, 
			State_finish
		} State;

	u32 m_uDM;    
};

////////////////////////////
//Set the decision maker
////////////////////////////

//-------------------------------------------------------------------------
// Task info for setting a peds defensive area
//-------------------------------------------------------------------------
class CClonedSetPedDefensiveAreaInfo : public CSerialisedFSMTaskInfo
{
public:
    CClonedSetPedDefensiveAreaInfo(bool clearDefensiveArea, const Vector3 &centrePoint, const float radius);
    CClonedSetPedDefensiveAreaInfo() {}
    ~CClonedSetPedDefensiveAreaInfo() {}

	virtual s32 GetTaskInfoType() const {return INFO_TYPE_SET_DEFENSIVE_AREA;}
    virtual int GetScriptedTaskType() const { return SCRIPT_TASK_SET_PED_DEFENSIVE_AREA; }
	
    bool    IsClearingDefensiveArea() const { return m_ClearDefensiveArea; }
    Vector3 GetCentrePoint() const { return m_Centre; }
    float   GetRadius() const { return m_Radius; }

	virtual CTask *CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity));

    void Serialise(CSyncDataBase& serialiser)
    {
        const unsigned int SIZEOF_RADIUS = 8;
        const float        MAX_RADIUS    = 1000.0f;

        CSerialisedFSMTaskInfo::Serialise(serialiser);

        SERIALISE_BOOL(serialiser, m_ClearDefensiveArea, "Clear Defensive Area");

        if(!m_ClearDefensiveArea || serialiser.GetIsMaximumSizeSerialiser())
        {
            SERIALISE_POSITION(serialiser, m_Centre, "Centre Point");
            SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_Radius, MAX_RADIUS, SIZEOF_RADIUS, "Radius");
        }
    }

private:

    CClonedSetPedDefensiveAreaInfo(const CClonedSetPedDefensiveAreaInfo &);

    CClonedSetPedDefensiveAreaInfo &operator=(const CClonedSetPedDefensiveAreaInfo &);

    bool    m_ClearDefensiveArea; // whether we are clearing or setting the defensive area
    Vector3 m_Centre;             // The centre of the defensive area (if we are setting it)
    float   m_Radius;             // The radius of the defensive area (if we are setting it)
};

class CTaskSetPedDefensiveArea : public CTaskFSMClone
{
public:

	CTaskSetPedDefensiveArea(const Vector3 &vCenter, const float fRadius)
		: m_vCenter(vCenter)
		, m_fRadius(fRadius)
		, m_bClearDefensiveArea(false)
	{
		taskAssertf(fRadius >= CDefensiveArea::GetMinRadius(), "The minimum radius for a defensive area is %.2f, CTaskSetPedDefensiveArea constructor is setting %.2f", CDefensiveArea::GetMinRadius(), fRadius);
		SetInternalTaskType(CTaskTypes::TASK_SET_PED_DEFENSIVE_AREA);
	}

	// An empty constructor implies clearing of the defensive area (otherwise there would be parameters needed to define to area)
	CTaskSetPedDefensiveArea()
		: m_bClearDefensiveArea(true)
		, m_fRadius(0.0f)
	{
		m_vCenter.Zero();
		SetInternalTaskType(CTaskTypes::TASK_SET_PED_DEFENSIVE_AREA);
	}

	virtual aiTask* Copy() const 
	{
		if(!m_bClearDefensiveArea)
		{
			return rage_new CTaskSetPedDefensiveArea(m_vCenter, m_fRadius);
		}
		else
		{
			return rage_new CTaskSetPedDefensiveArea();
		}
	}

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_SET_PED_DEFENSIVE_AREA;}

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual s32		GetDefaultStateAfterAbort() const { return State_finish; }

    virtual CTaskInfo*	CreateQueriableState() const;
	virtual void        ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return  UpdateClonedFSM (const s32 /*iState*/, const FSM_Event /*iEvent*/) { return FSM_Quit; }

private:

	FSM_Return	Start_OnUpdate();

	typedef enum {
		State_start, 
		State_finish
	} State;

	Vector3 m_vCenter;
	float	m_fRadius;

	bool	m_bClearDefensiveArea;
};

///////////////////////////////////////////////////////////////////////////////////
// Task used to run a sequence of tasks 
///////////////////////////////////////////////////////////////////////////////////

// This class is responsible for the synchronization 
//  of the task sequences data.
class CClonedTaskSequenceInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedTaskSequenceInfo();
	CClonedTaskSequenceInfo(u32 sequenceId, u32 resourceId, u32 progress1, s32 progress2, bool exitAfterInterrupted);
	~CClonedTaskSequenceInfo();

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_SEQUENCE;}
    virtual int GetScriptedTaskType() const { return SCRIPT_TASK_PERFORM_SEQUENCE; }

	u32   GetSequenceResourceId()		 const;
	u8    GetPrimarySequenceProgress()   const  { return m_Progress1; }
	s8    GetSubtaskSequenceProgress()   const  { return m_Progress2; }
	bool  GetExitAfterInterrupted()      const  { return m_ExitAfterInterrupted; }

	virtual CTask*	CreateLocalTask(fwEntity *pEntity);

	void Serialise(CSyncDataBase& serialiser)
	{
		bool bIsScriptSequence = (m_ResourceId != 0);
		bool bHasSubtaskSequence = (m_Progress2 > -1);

		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_BOOL(serialiser, bIsScriptSequence);

		if (bIsScriptSequence)
		{
			SERIALISE_UNSIGNED(serialiser, m_ResourceId, CScriptResource_SequenceTask::SIZEOF_SEQUENCE_ID, "Sequence resource id");
		}
		else
		{
			m_ResourceId = 0;
		}

		SERIALISE_UNSIGNED(serialiser, m_Progress1,  SIZEOF_PROGRESS1,  "Primary Sequence Progress");

		SERIALISE_BOOL(serialiser, bHasSubtaskSequence);

		if (bHasSubtaskSequence || serialiser.GetIsMaximumSizeSerialiser())
		{
			u32 progress2 = (u32)m_Progress2;
			SERIALISE_UNSIGNED(serialiser, progress2,  SIZEOF_PROGRESS2,  "Subtask Sequence Progress");
			m_Progress2 = (s8)progress2;
		}
		else
		{
			m_Progress2 = -1;
		}

		SERIALISE_BOOL(serialiser, m_ExitAfterInterrupted, "Exit After Interrupt");
	}

public:
	static const u32 SIZEOF_SEQUENCE_ARRAY_INDEX = 8;
	static const u32 SIZEOF_PROGRESS1 = datBitsNeeded<IPedNodeDataAccessor::MAX_SYNCED_SEQUENCE_TASKS>::COUNT;
	static const u32 SIZEOF_PROGRESS2 = 3;

private:

	u32   m_ResourceId;					// The script handler sequence id (the unique sequence resource index assigned by the script handler)
	u8    m_Progress1;					// Primary Sequence Progress
	s8    m_Progress2;					// Subtask Sequence Progress
	bool  m_ExitAfterInterrupted;		// Exit After Interrupt
};

class CTaskUseSequence : public CTaskFSMClone
{
public:

	// Task States
	enum
	{
		State_Start = 0,
		State_RunningTask,
		State_Finish
	};

	// This constructor takes an id which refers to a static task sequence list in CTaskSequences::ms_TaskSequenceLists
	// used for script sequences
	CTaskUseSequence(const s32 sequenceId, const u32 resourceId = 0);

	// This constructor allows code to use a task sequence list created on the fly
	// used in code as an easy way to string together tasks easily
	CTaskUseSequence(CTaskSequenceList& TaskSequenceList);

	// Copy Constructor (Calls Register if a valid id is passed in)
	CTaskUseSequence(const CTaskUseSequence& other);

	// Destructor
	~CTaskUseSequence();

	// Task required functions
	virtual aiTask* Copy() const			{ return rage_new CTaskUseSequence(*this); }
	virtual int GetTaskTypeInternal() const			{ return CTaskTypes::TASK_USE_SEQUENCE; } 
	s32 GetDefaultStateAfterAbort() const;

	u32 GetPrimarySequenceProgress() const	{ return m_iPrimarySequenceProgress; }
	s32 GetSubtaskSequenceProgress() const	{ return m_iSubtaskSequenceProgress; }	

	// This is used to set the progress of the sequence task (and a sequence subtask)
	void SetProgress(const u32 iPrimarySequenceProgress, const s32 iSubtaskSequenceProgress) { m_iPrimarySequenceProgress = iPrimarySequenceProgress; m_iSubtaskSequenceProgress = iSubtaskSequenceProgress; }

	// Calculate any subtask sequence progress, returns -1 if no progress
	s32 GetSubtaskProgress() const;

	bool GetInitialiseSubtaskSequenceProgress() const { return m_bInitialiseSubtaskSequenceProgress; }

	bool GetExitAfterInterrupted() const { return m_bExitAfterInterrupted; }
	void SetExitAfterInterrupted(bool bExitAfterInterrupted) { m_bExitAfterInterrupted = bExitAfterInterrupted; }

	// Try to create a new task from the task list at the index at iProgress, 
	// it will increment the iProgress and iRepeatProgress if necessary
	aiTask* CreateTask();
	
	// Get the index into the static array of sequence task lists
	s32 GetSequenceID() const { return m_StaticSequenceHelper.GetID(); }
	s32 GetSequenceResourceID() const { return m_sequenceResourceId; }

	// Get the tasks sequence list (either static or dynamic)
	const CTaskSequenceList* GetTaskSequenceList() const;

	const CTaskSequenceList* GetStaticTaskSequenceList() const;
	const CTaskSequenceList* GetDynamicTaskSequenceList() const;

	virtual CTaskInfo*	CreateQueriableState() const;
	virtual void        ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return  UpdateClonedFSM (const s32 /*iState*/, const FSM_Event /*iEvent*/) { return FSM_Quit; }

private:

	// State Machine Update Functions
	virtual FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// Local state function calls
	FSM_Return					Start_OnUpdate();
	void						RunningTask_OnEnter();
	FSM_Return					RunningTask_OnUpdate();
	void						Finish_OnEnter();

	void						InitialiseSequenceSubtask(aiTask* pCloneTask) const;

private:

	// Internal helper class to ensure register and deregister get called at the appropriate places
	class CStaticSequenceHelper
	{
		public:

			// Constructor
			CStaticSequenceHelper(const s32 sequenceId);

			// Copy Constructor
			CStaticSequenceHelper(const CStaticSequenceHelper& other);

			// Destructor
			~CStaticSequenceHelper();

			s32 GetID() const { return m_id; }

		private:

			s32  m_id;          // Stores the index to the static sequence list
	};

	u32							m_iPrimarySequenceProgress;	
	s32							m_iSubtaskSequenceProgress;	
	s32							m_iRepeatProgress;
	bool						m_bRecreateOverNetwork;
	bool						m_bInitialiseSubtaskSequenceProgress;
	mutable bool				m_bExitAfterInterrupted;
	CTaskSequenceList*			m_pTaskSequenceList;
	CStaticSequenceHelper		m_StaticSequenceHelper;
	u32							m_sequenceResourceId;	// used in MP. This is the script resource id of the sequence, that is sent in the queriable state and will be consistent across the network
	bool						m_bPedCanMigrate;		// used in MP. This is the cached value of the GLOBALFLAG_SCRIPT_MIGRATION flag which this task alters
	bool						m_bEarlyOutFromStopExactly;	// To make all stops used in a sequence to not have a 3 frame delay before next task can start
#if !__FINAL
public:

	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	virtual const char * GetStateName( s32 ) const;
#endif // !__FINAL
};


class CTaskOpenDoor : public CTask
{
public:

	CTaskOpenDoor( CDoor* pDoor );
	~CTaskOpenDoor() {}

	virtual CTask*		Copy() const {return rage_new CTaskOpenDoor( m_pDoor );}
	virtual int			GetTaskTypeInternal() const {return CTaskTypes::TASK_OPEN_DOOR;}
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM( const s32 iState, const FSM_Event iEvent );
	virtual s32			GetDefaultStateAfterAbort() const { return State_Initial; }
	CClipHelper*		GetClipToBeNetworked() { return GetClipHelper(); }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

	static bool			IsPedInFrontOfDoor(const CPed* pPed, CDoor* pDoor);
	static bool			ShouldAllowTask(const CPed* pPed, CDoor* pDoor);

	// Arm ik interface
	Vec3V				GetClosestDoorPosition(void) const { return m_vClosestDoorPosition; }
	bool				GetIsInFrontOfDoor(void) const { return m_bInFrontOfDoor; } 
	bool				GetIsInsideDoorWidth(void) const { return m_bInsideDoorWidth; }
	bool				GetShouldUseLeftArm(void) const { return m_bUseLeftArm; }

protected:

	static bool			ShouldUseLeftHand(const CPed* pPed, CDoor* pDoor, Vec3V_In vClosestDoorPosition, bool bFront);
	static float		CalculatePedDotToDoor(const CPed* pPed, CDoor* pDoor, bool bInFrontOfDoor);
	static Vec3V_Out	CalculateConstraintNormal(const CPed* pPed, CDoor* pDoor, bool bFront);
	static Vec3V_Out	CalculateClosestDoorPosition(const CPed* pPed, CDoor* pDoor, Vec3V_In vTestPosition, bool bFront, bool &bInsideDoorWidth, bool &bInsideDoorHeight);
	static bool			NavigatingPedMustAbort(const CPed* pPed, CDoor* pDoor);

	// Generic cleanup function, called when the task quits itself or is aborted
	virtual	void		CleanUp();
	void				ProcessClipEvents(const CPed* pPed);

private:

	enum
	{
		State_Initial,
		State_OpenDoor,
		State_ConstraintOnly,
		State_Barge,
		State_Finish,
	};

	FSM_Return			StateInitial_OnUpdate( CPed* pPed );

	FSM_Return			StateOpenDoor_OnEnter( CPed* pPed );
	FSM_Return			StateOpenDoor_OnUpdate( CPed* pPed );

	FSM_Return			StateConstraintOnly_OnUpdate( CPed* pPed );

	FSM_Return			StateBarge_OnEnter( CPed* pPed );
	FSM_Return			StateBarge_OnUpdate( CPed* pPed );
	FSM_Return			StateBarge_OnExit( CPed* pPed );

	void				ApplyCloseDelay( CPed* pPed, bool bFront );
	const CWeaponInfo*	GetPedWeaponInfo(const CPed* pPed) const;
	bool				PrepareMoveNetwork( CPed* pPed );
	void				UpdateConstraint( CPed* pPed, bool bFront, Vec3V_In vPedPosition, Vec3V_In vClosestPosition, float fConstraintOffset );
	void				SendParams( CPed* pPed );

	void				AdjustHeight( const CPed* pPed, Vec3V& vPedPosition, Vec3V& vDoorPosition );

	CMoveNetworkHelper	m_moveNetworkHelper;
	RegdDoor			m_pDoor;
	phConstraintHandle	m_PhysicsConstraint;
	crFrameFilterMultiWeight* m_pLeftArmFilter;
	crFrameFilterMultiWeight* m_pRightArmFilter;
	Vec3V				m_vClosestDoorPosition;
	float				m_fActorDirection;
	float				m_fConstraintDistance;
	float				m_fCriticalFramePhase;
	float				m_fTargetAnimRate;
	bool				m_bPastCriticalFrame;
	bool				m_bSetOpenDoorArm;
	bool				m_bSetActorDirection;
	bool				m_bInFrontOfDoor;
	bool				m_bInsideDoorWidth;
	bool				m_bInsideDoorHeight;
	bool				m_bUseLeftArm;
	bool				m_bAppliedBargeImpulse;

	static const fwMvClipSetId ms_UnarmedClipSetId;
	static const fwMvClipSetId ms_1HandedClipSetId;
	static const fwMvClipSetId ms_2HandedClipSetId;
	static const fwMvClipSetId ms_2HandedMeleeClipSetId;
	static const fwMvClipId	ms_AnimClipId;
	static const fwMvFloatId ms_ActorDirectionId;
	static const fwMvFloatId ms_AnimClipRateId;
	static const fwMvFloatId ms_AnimClipPhaseId;
	static const fwMvFlagId ms_ActorUseLeftArmId;
	static const fwMvBooleanId ms_BargeAnimFinishedId;
	static const fwMvFilterId ms_RightArmFilterId;
	static const fwMvFilterId ms_LeftArmFilterId;
	static dev_float ms_fDirectionBlendRate;
	static dev_float ms_fMaxActorToDoorReachingDistance;
	static dev_float ms_fMinTimeBeforeIkUse;
	static dev_float ms_fMaxActorToDoorDistanceRunning;
	static dev_float ms_fMaxActorToDoorDistance;
	static dev_float ms_fMinActorToDoorDistance;
	static dev_float ms_fWalkingDoorConstraintOffset;
	static dev_float ms_fRunningDoorConstraintOffset;
	static dev_float ms_fConstraintGrowthRate;
	static dev_float ms_fPedDotToDoorThresholdRunning;
	static dev_float ms_fPedDotToDoorThreshold;
	static dev_float ms_fMaxBargeAnimRate;
	static dev_float ms_fMBRWalkBoundary;
	static dev_u32 ms_nBargeDoorDelay;
};

class CTaskShovePed : public CTask
{
public:

	CTaskShovePed( CPed* pTargetPed );
	~CTaskShovePed() {}

	virtual CTask*		Copy() const {return rage_new CTaskShovePed( m_pTargetPed );}
	virtual int			GetTaskTypeInternal() const {return CTaskTypes::TASK_SHOVE_PED;}
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM( const s32 iState, const FSM_Event iEvent );
	virtual s32			GetDefaultStateAfterAbort() const { return State_Initial; }
	CClipHelper*		GetClipToBeNetworked() { return GetClipHelper(); }
	float				GetBlendOutDuration() const { return m_fBlendOutDuration; }
	void				SetBlendOutDuration( float fBlendOutDuration ) { m_fBlendOutDuration = fBlendOutDuration; }

#if !__FINAL
	virtual void		Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

	static float		CalculatePedDotToTargetPed(const CPed* pPed, CPed* pTargetPed);
	static Vec3V_Out	CalculateConstraintNormal(const CPed* pPed, CPed* pTargetPed);
	static bool			ShouldAllowTask(const CPed* pPed, CPed* pTargetPed);
	static Vector3		CalculateClosestTargetPedPosition(const CPed* pPed, CPed* pTargetPed);

	static bool			ScanForShoves();

#if __BANK
	static bool			IsTesting();
#endif //!__FINAL

protected:

	// Generic cleanup function, called when the task quits itself or is aborted
	virtual	void		CleanUp();
	void				CalculateTargetAnimRate( CPed* pPed, const Vector3& vClosestPosition, float fTargetDistance );

private:

	enum
	{
		State_Initial,
		State_Shove,
		State_Push,
		State_Barge,
		State_Finish,
	};

	FSM_Return		StateInitial_OnUpdate( CPed* pPed );

	FSM_Return		StatePush_OnEnter( CPed* pPed );
	FSM_Return		StatePush_OnUpdate( CPed* pPed );

	FSM_Return		StateShove_OnEnter( CPed* pPed );
	FSM_Return		StateShove_OnUpdate( CPed* pPed );

	FSM_Return		StateBarge_OnEnter( CPed* pPed );
	FSM_Return		StateBarge_OnUpdate( CPed* pPed );

	bool			PrepareMoveNetwork( CPed* pPed );
	bool			SendParams( CPed* pPed, bool bEnteringState, bool bForceDirection = false );

	CMoveNetworkHelper	m_moveNetworkHelper;
	RegdPed				m_pTargetPed;
	crFrameFilterMultiWeight* m_pLeftArmFilter;
	crFrameFilterMultiWeight* m_pRightArmFilter;
	float				m_fActorDirection;
	float				m_fInterruptPhase;
	float				m_fTargetAnimRate;
	float				m_fBlendOutDuration;
	bool				m_bComputeTargetAnimRate;
	bool				m_bUseLeftArm;
	bool				m_bDoTargetReaction;

	static const fwMvClipSetId ms_UnarmedShoveClipSetId;
	static const fwMvClipSetId ms_UnarmedClipSetId;
	static const fwMvClipSetId ms_1HandedClipSetId;
	static const fwMvClipSetId ms_2HandedClipSetId;
	static const fwMvClipId	ms_AnimClipId;
	static const fwMvFloatId ms_ActorDirectionId;
	static const fwMvFloatId ms_AnimClipRateId;
	static const fwMvFloatId ms_AnimClipPhaseId;
	static const fwMvFlagId ms_ActorUseLeftArmId;
	static const fwMvBooleanId ms_BargeAnimFinishedId;
	static const fwMvFilterId ms_RightArmFilterId;
	static const fwMvFilterId ms_LeftArmFilterId;
	static dev_float ms_fDirectionBlendRate;
	static dev_float ms_fMinTimeBeforeIkUse;
	static dev_float ms_fMaxActorToTargetDistanceRunning;
	static dev_float ms_fActorToTargetHeightThreshold;
	static dev_float ms_fPedTestHeightOffset;
	static dev_float ms_fMaxActorToTargetDistance;
	static dev_float ms_fMinActorToTargetDistance;
	static dev_float ms_fRunningTargetOffset;
	static dev_float ms_fPedDotToTargetThresholdRunning;
	static dev_float ms_fPedDotToTargetThreshold;
	static dev_float ms_fMaxBargeAnimRate;
	static dev_float ms_fMinPedVelocityToBarge;

	// Debug draw variables
#if !__FINAL
	Vector3 m_vClosestPoint;
#endif //!__FINAL
};

#endif
