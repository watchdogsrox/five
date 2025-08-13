#ifndef TASK_CAR_ANIMS_H
#define TASK_CAR_ANIMS_H

//Game headers.
#include "Control/Route.h"
#include "Game/ModelIndices.h"
#include "Scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Task/System/TaskTypes.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/Vehicle/TaskGoToVehicleDoor.h"

class CPed;
class CVehicle;
class CTaskMoveGoToPoint;
class CTaskMoveFollowPointRoute;

//Std headers.
#include <vector>

///////////////////////////
//Set ped in as passenger
///////////////////////////
class CTaskSetPedInVehicle : public CTask
{
public:
	enum state
	{
		State_Start = 0,
		State_SetPedIn,
		State_Finish
	};

	CTaskSetPedInVehicle(CVehicle* pTargetVehicle, const s32 iTargetSeat, u32 iFlags);
	~CTaskSetPedInVehicle();

	virtual aiTask* Copy() const 
	{
		return rage_new CTaskSetPedInVehicle(m_pTargetVehicle,m_targetSeat,m_iFlags);
	}

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_SET_PED_IN_VEHICLE;}

	virtual s32   GetDefaultStateAfterAbort() const {return State_Finish;}
	FSM_Return	UpdateFSM (const s32 iState, const FSM_Event iEvent);

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif 

private:    
	// Start
	FSM_Return	Start_OnUpdate(CPed* pPed);
	// Play Clips
	void		SetPedIn_OnEnter(CPed* pPed);
	FSM_Return	SetPedIn_OnUpdate(CPed* pPed);


	RegdVeh		m_pTargetVehicle;
	s32		m_targetSeat;
	u32		m_iFlags;
};

///////////////////////////
//Set ped in as passenger
///////////////////////////
class CTaskSetPedOutOfVehicle : public CTask
{
public:
	enum state
	{
		State_Start = 0,
		State_SetPedOut,
		State_Finish
	};

	CTaskSetPedOutOfVehicle(u32 iFlags);
	~CTaskSetPedOutOfVehicle();

	virtual aiTask* Copy() const 
	{
		return rage_new CTaskSetPedOutOfVehicle(m_iFlags);
	}

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_SET_PED_OUT_OF_VEHICLE;}

	virtual s32   GetDefaultStateAfterAbort() const {return State_Finish;}
	FSM_Return	UpdateFSM (const s32 iState, const FSM_Event iEvent);

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif 

private:    
	// Start
	FSM_Return	Start_OnUpdate(CPed* pPed);
	// Play Clips
	void		SetPedOut_OnEnter(CPed* pPed);
	FSM_Return	SetPedOut_OnUpdate(CPed* pPed);

	u32		m_iFlags;
};

/////////////////////////////
// Smash a car window
/////////////////////////////

class CTaskSmashCarWindow : public CTask
{
	enum eState
	{
		State_Init,
		State_StreamingClips,
		State_PlaySmashClip,
		State_Finish
	};

public:
	CTaskSmashCarWindow(bool bSmashWindscreen);

	virtual aiTask* Copy() const {return rage_new CTaskSmashCarWindow(m_bSmashWindscreen);}

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_SMASH_CAR_WINDOW;}

	// FSM interface
	virtual s32 GetDefaultStateAfterAbort() const { return State_Init; }


#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif 


protected:

	FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	FSM_Return Init_OnUpdate(CPed* pPed);
	FSM_Return StreamingClips_OnUpdate(CPed* pPed);
	void PlaySmashClip_OnEnter(CPed* pPed);
	FSM_Return PlaySmashClip_OnUpdate(CPed* pPed);
	FSM_Return Finish_OnUpdate(CPed*) { return FSM_Quit; }
	
private:

	bool m_bSmashWindscreen;
	eHierarchyId m_windowToSmash;

	s32 m_iClipDictIndex;
	u32 m_iClipHash;
	float m_fRandomAIDelayTimer;

	CClipRequestHelper m_clipStreamer;

	//// Static utility functions ////
public:
	static bool GetWindowToSmash( const CPed* pPed, bool bWindScreen, u32& iClipHash, s32& iDictIndex, eHierarchyId& window, bool bIgnoreCracks = false );
};

//-------------------------------------------------------------------------
// Wait for the given seat to be free
//-------------------------------------------------------------------------
class CTaskWaitForCondition : public CTask
{
public:
	enum WaitState
	{
		State_start,
		State_checkCondition,
		State_exit
	};
	CTaskWaitForCondition() {}
	~CTaskWaitForCondition() {}
	virtual aiTask* Copy() const = 0;

	virtual int GetTaskTypeInternal() const = 0;
	// FSM interface
	virtual s32 GetDefaultStateAfterAbort() const { return State_start; }

	FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual bool CheckCondition (CPed* pPed) = 0;

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif
private:

	// Start
	FSM_Return Start_OnUpdate(CPed* pPed);
	//Check condition
	void CheckCondition_OnEnter(CPed* pPed);
	FSM_Return CheckCondition_OnUpdate(CPed* pPed);

};

//-------------------------------------------------------------------------
// Wait for the given seat to be free
//-------------------------------------------------------------------------
class CTaskWaitForSteppingOut : public CTaskWaitForCondition
{
public:
	CTaskWaitForSteppingOut(float fWaitTime = 0.0f) :m_fWaitTime(fWaitTime){SetInternalTaskType(CTaskTypes::TASK_WAIT_FOR_STEPPING_OUT);}
	~CTaskWaitForSteppingOut() {}
	virtual aiTask* Copy() const {return rage_new CTaskWaitForSteppingOut( m_fWaitTime );}

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_WAIT_FOR_STEPPING_OUT;}

	virtual bool CheckCondition (CPed* pPed);

private:
	float m_fWaitTime;
};

//-------------------------------------------------------------------------
// Wait for the car to stop
//-------------------------------------------------------------------------
class CTaskWaitForMyCarToStop : public CTaskWaitForCondition
{
public:
	CTaskWaitForMyCarToStop( ) { SetInternalTaskType(CTaskTypes::TASK_WAIT_FOR_MY_CAR_TO_STOP); }
	~CTaskWaitForMyCarToStop() {}
	virtual aiTask* Copy() const {return rage_new CTaskWaitForMyCarToStop();}

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_WAIT_FOR_MY_CAR_TO_STOP;}
	virtual bool CheckCondition (CPed* pPed);

private:
};

#endif
