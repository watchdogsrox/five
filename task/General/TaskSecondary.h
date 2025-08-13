#ifndef TASK_SECONDARY_H
#define TASK_SECONDARY_H

// Rage headers
#include "ATL/String.h"

// Game headers
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/TaskTypes.h"

class CEntity;

//REMOVE ME WHEN DECISION MAKERS ARE REPLACED
class CTaskSimpleFacial
{
public:

	enum 
	{
		FACIAL_SURPRISEPANIC = 0,
		FACIAL_SURPRISEMILD,
		FACIAL_CURIOSITY,
		FACIAL_ANGER,
		FACIAL_HAPPINESS,
		FACIAL_SADNESS,
		MAX_NUM_EMOTIONS,
		FACIAL_TALKING,
		FACIAL_CHEWING
	};
};
class CTaskAffectSecondaryBehaviour : public CTask
{
public:

	friend class CTaskCounter;

    CTaskAffectSecondaryBehaviour(const bool bAdd, const int iType, aiTask* pTask);
    ~CTaskAffectSecondaryBehaviour();

    virtual aiTask* Copy() const 
    {
    	aiTask* pCopy=0;
    	if(m_pTask)
    	{
	    	pCopy=m_pTask->Copy();
			Assert( pCopy->GetTaskType() == m_pTask->GetTaskType() );

		}
    	return rage_new CTaskAffectSecondaryBehaviour(m_bAdd,m_iType,pCopy);
    } 

    virtual int			GetTaskTypeInternal() const {return CTaskTypes::TASK_AFFECT_SECONDARY_BEHAVIOUR;}

#if !__FINAL
    virtual void		Debug() const {}
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual s32		GetDefaultStateAfterAbort() const { return State_start; }

private:

	FSM_Return	Start_OnUpdate(CPed* pPed);
	FSM_Return	AddBehaviour_OnUpdate(CPed* pPed);
	FSM_Return	DeleteBehaviour_OnUpdate(CPed* pPed);

	typedef enum {State_start, State_addBehaviour, State_deleteBehaviour, State_finish} State;

	bool m_bAdd;
	int m_iType;
	RegdaiTask m_pTask;
};

class CTaskCrouch;

class CClonedCrouchInfo : public CSerialisedFSMTaskInfo
{
public:

	CClonedCrouchInfo(s32 iDuration = 0) : m_iDuration(iDuration)
	{
	}

	virtual s32 GetTaskInfoType() const { return INFO_TYPE_CROUCH; }
	virtual int GetScriptedTaskType() const { return SCRIPT_TASK_DUCK; }

	virtual CTask* CreateLocalTask(fwEntity* pEntity);

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		bool bSyncDuration = (m_iDuration > 0);
		SERIALISE_BOOL(serialiser, bSyncDuration);
		if(bSyncDuration)
		{
			SERIALISE_UNSIGNED(serialiser, m_iDuration, SIZEOF_DURATION, "Duration");
		}
	}

private:

	static const unsigned int SIZEOF_DURATION = 14; 
	s32 m_iDuration;
};

class CTaskCrouch : public CTask
{
	public:

	enum
	{
		State_Initial,
		State_Crouching
	};


	static const u16 ms_nLengthOfCrouch;

	CTaskCrouch(s32 nLengthOfCrouch=0, s32 nUseShotsWhizzingEvents=-1);
	virtual ~CTaskCrouch();

	virtual aiTask* Copy() const { return rage_new CTaskCrouch(m_nLengthOfCrouch, m_nShotWhizzingCounter); }

	virtual void DoAbort(const AbortPriority iPriority, const aiEvent * pEvent);

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_CROUCH;}
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

#if !__FINAL
	virtual void Debug() const {}
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case State_Initial : return "Initial";
			case State_Crouching : return "Crouching";
			default: Assert(0); return NULL;
		}
	}
#endif

	void SetCrouchTimer(u16 nLengthOfCrouch);

	// Function to create cloned task information
	virtual CTaskInfo* CreateQueriableState() const
	{
		return rage_new CClonedCrouchInfo(m_nLengthOfCrouch);
	}

private:

	u32 m_nStartTime;
	u32 m_nLengthOfCrouch;
	u32 m_nShotWhizzingCounter;

	bool ShouldQuit(CPed * pPed);

	FSM_Return StateInitial_OnEnter(CPed * pPed);
	FSM_Return StateInitial_OnUpdate(CPed * pPed);
	FSM_Return StateCrouching_OnEnter(CPed * pPed);
	FSM_Return StateCrouching_OnUpdate(CPed * pPed);
};

//////////////////////////////////////////////////////////////////////////
// Handle putting on a helmet when getting into a vehicle
//////////////////////////////////////////////////////////////////////////

class CTaskPutOnHelmet : public CTask
{
public:
	CTaskPutOnHelmet();
	virtual ~CTaskPutOnHelmet();

	virtual aiTask*		Copy() const {return rage_new CTaskPutOnHelmet();}

	virtual int			GetTaskTypeInternal() const {return CTaskTypes::TASK_PUT_ON_HELMET;}

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif 

	virtual FSM_Return	ProcessPreFSM();

	FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32		GetDefaultStateAfterAbort() const { return State_finish; }

protected:

	// Handle aborting the task - this will be called in addition to CleanUp when we are aborting
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);

private:

	FSM_Return Start_OnUpdate(CPed* pPed);
	FSM_Return PickUpHelmet_OnEnter(CPed* pPed);
	FSM_Return PickUpHelmet_OnUpdate(CPed* pPed);
	FSM_Return PutOnHelmet_OnUpdate(CPed* pPed);
	FSM_Return WaitForClip_OnUpdate();

	void ProcessFinished(CPed* pPed);

	typedef enum {
		State_start=0, 
		State_pickUpHelmet,
		State_putOnHelmet,
		State_waitForClip,
		State_finish
	} PutOnHelmetState;
};

//////////////////////////////////////////////////////////////////////////
// Handle taking off a helmet
//////////////////////////////////////////////////////////////////////////

class CTaskTakeOffHelmet : public CTask
{
public:
	CTaskTakeOffHelmet();
	virtual ~CTaskTakeOffHelmet();

	virtual aiTask* Copy() const {return rage_new CTaskTakeOffHelmet();}

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_TAKE_OFF_HELMET;}

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	virtual FSM_Return	ProcessPreFSM	();

	FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32		GetDefaultStateAfterAbort() const { return State_finish; }

	static fwMvClipId GetVisorClipId(CPed& rPed, bool bVisorIsUpNow, bool bOnFoot);

	void CleanUp();

	void SetIsVisorSwitching( bool bValue) { m_bVisorSwitch = bValue; }
	bool GetIsVisorSwitching() const { return m_bVisorSwitch; }
	void SetVisorTargetState( int nValue) { m_nVisorTargetState = nValue; }
	int GetVisorTargetState() const { return m_nVisorTargetState; }

protected:

	// Handle aborting the task - this will be called in addition to CleanUp when we are aborting
	virtual bool ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	// PURPOSE: Called after camera is updated
	virtual bool ProcessPostCamera();

private:

	FSM_Return Start_OnUpdate(CPed* pPed);
	FSM_Return WaitForStream_OnUpdate(CPed* pPed);
	FSM_Return WaitForVisorStream_OnUpdate(CPed* pPed);
	FSM_Return TakeOffHelmet_OnEnter(CPed* pPed);
	FSM_Return TakeOffHelmet_OnUpdate(CPed* pPed);
	FSM_Return SwitchHelmetVisor_OnEnter(CPed* pPed);
	FSM_Return SwitchHelmetVisor_OnUpdate(CPed* pPed);
	FSM_Return PutDownHelmet_OnUpdate(CPed* pPed);
	FSM_Return WaitForClip_OnUpdate(CPed* pPed);
	
	void ProcessFinished(CPed* pPed);

	void EnableHelmet();

	typedef enum {
		State_start=0, 
		State_waitForStream,
		State_waitForVisorStream,
		State_takeOffHelmet,
		State_switchHelmetVisor,
		State_putDownHelmet,
		State_waitForClip,
		State_finish
	} PutOnHelmetState;

	bool m_bLoadedRestoreProp;
	bool m_bRecreateWeapon;
	bool m_bEnabledHelmet;
	bool m_bEnabledHelmetFx;
	bool m_bDeferEnableHelmet;
	bool m_bVisorSwitch;
	int m_nVisorTargetState;
#if FPS_MODE_SUPPORTED
	bool m_bEnableTakeOffAnimsFPS;
	CFirstPersonIkHelper* m_pFirstPersonIkHelper;
#endif	//FPS_MODE_SUPPORTED
};

#endif
