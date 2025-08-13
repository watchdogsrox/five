#ifndef TASK_CAR_COLLISION_RESPONSE
#define TASK_CAR_COLLISION_RESPONSE

#include "Task/System/Task.h"
#include "Task/System/TaskTypes.h"
#include "Task/System/Tuning.h"
#if !__FINAL
#include "atl/string.h"
#endif

class CVehicle;

//////////////////////////////////////////////////////////////////////////
// CTaskComplexCarReactToVehicleCollision
//////////////////////////////////////////////////////////////////////////

class CTaskCarReactToVehicleCollision : public CTask
{

public:

	struct Tunables : CTuning
	{
		struct SlowDown
		{
			SlowDown()
			{}

			float m_MinTimeToReact;
			float m_MaxTimeToReact;
			float m_MaxCruiseSpeed;
			float m_ChancesToHonk;
			float m_ChancesToHonkHeldDown;
			float m_ChancesToFlipOff;
			float m_MinTime;
			float m_MaxTime;

			PAR_SIMPLE_PARSABLE;
		};

		Tunables();

		SlowDown m_SlowDown;

		float m_MaxDamageToIgnore;

		PAR_PARSABLE;
	};

public:

	CTaskCarReactToVehicleCollision(CVehicle* pMyVehicle, CVehicle* pOtherVehicle, float fDamageDone, const Vector3& vDamagePos, const Vector3& vDamageDir);
	~CTaskCarReactToVehicleCollision();

public:

	void SetIsContinuouslyColliding(bool bValue, float fSpeedOfContinuousCollider) { m_bIsContinuouslyColliding = bValue; m_fSpeedOfContinuousCollider = fSpeedOfContinuousCollider; }

public:

	virtual aiTask* Copy() const
	{
		CTaskCarReactToVehicleCollision* pTask = rage_new CTaskCarReactToVehicleCollision(m_pMyVehicle, m_pOtherVehicle, m_fDamageDone, m_vDamagePos, m_vDamageDir);
		pTask->SetIsContinuouslyColliding(m_bIsContinuouslyColliding, m_fSpeedOfContinuousCollider);
		return pTask;
	}

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_CAR_REACT_TO_VEHICLE_COLLISION; }
	
	virtual s32 GetDefaultStateAfterAbort() const;

#if !__FINAL

	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );

#endif

private:

	bool AreAbnormalExitsDisabledForSeat() const;
	bool AreBothVehiclesStationary() const;
	bool CanUseThreatResponse() const;
	bool IsBusDriver() const;
	bool IsCollisionSignificant() const;
	bool IsFlippingOff() const;
	bool IsTalking() const;
	bool IsVehicleStationary(const CVehicle& rVehicle) const;
	void ProcessGestures();
	void ProcessThreatResponse();

private:

	FSM_Return ProcessPreFSM();
	virtual bool ProcessMoveSignals();
	FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return Start_OnUpdate(CPed* pPed);

	FSM_Return Fight_OnEnter();
	FSM_Return Fight_OnUpdate(CPed* pPed);

	FSM_Return GetOut_OnEnter(CPed* pPed);
	FSM_Return GetOut_OnUpdate();

	FSM_Return TauntInVehicle_OnEnter();
	FSM_Return TauntInVehicle_OnUpdate();

	FSM_Return Gesture_OnUpdate();

	FSM_Return Flee_OnEnter(CPed* pPed);
	FSM_Return Flee_OnUpdate();

	FSM_Return GetBackInVehicle_OnEnter(CPed* pPed);
	FSM_Return GetBackInVehicle_OnUpdate();

	FSM_Return	ThreatResponseInVehicle_OnEnter(CPed* pPed);
	FSM_Return	ThreatResponseInVehicle_OnUpdate();
	void		ThreatResponseInVehicle_OnExit();

	FSM_Return	SlowDown_OnUpdate();
	void		SlowDown_OnExit();

	typedef enum {
		State_start,
		State_getOut,
		State_flee,
		State_taunt,
		State_gesture,
		State_fight,
		State_getBackInVehicle,
		State_threatResponseInVehicle,
		State_slowDown,
		State_finish,
	} State;

	enum eActionToTake
	{
		ACTION_INVALID = 0,
		ACTION_STAY_IN_CAR,
		ACTION_TAUNT_IN_CAR,
		ACTION_STAY_IN_CAR_ANGRY,
		ACTION_GET_OUT,
		ACTION_GET_OUT_ON_FIRE,
		ACTION_GET_OUT_STUMBLE,
		ACTION_GET_OUT_ANGRY,
		ACTION_GET_OUT_INSPECT_DMG,
		ACTION_GET_OUT_INSPECT_DMG_ANGRY,
		ACTION_FLEE,
		ACTION_THREAT_RESPONSE_IN_VEHICLE,
		ACTION_SLOW_DOWN,
		ACTION_DIE_IN_VEHICLE
	};

	aiTask* CreateSubTask(const int iTaskType, CPed* pPed);

	eActionToTake GetNewAction(CPed* pPed);

	//
	// Members
	//

	Vector3 m_vDamagePos;
	Vector3 m_vDamageDir;
	eActionToTake m_nAction;
	RegdVeh m_pMyVehicle;
	RegdVeh m_pOtherVehicle;
	RegdPed m_pOtherDriver;
	float m_fDamageDone;
	float m_fCachedMaxCruiseSpeed;
	float m_fSpeedOfContinuousCollider;
	bool m_bHasReacted;
	bool m_bIsContinuouslyColliding;
	bool m_bGotOutOfVehicle;
	bool m_bUseThreatResponse;
	bool m_bPreviousDisablePretendOccupantsCached;

private:

	static Tunables sm_Tunables;

};

//////////////////////////////////////////////////////////////////////////
// CTaskComplexCarReactToVehicleCollisionGetOut
//////////////////////////////////////////////////////////////////////////

class CTaskCarReactToVehicleCollisionGetOut : public CTask
{
public:

	enum eFlags
	{
		// Is ped driving car?
		FLAG_IS_DRIVER   = BIT0,

		// Will trigger the on fire response
		FLAG_ON_FIRE     = BIT1,

		// Will trigger the stumble/collapse response
		FLAG_COLLAPSE    = BIT2,

		// Angry reaction?
		FLAG_IS_ANGRY    = BIT3,

		// Should inspect the damage?
		FLAG_INSPECT_DMG = BIT4,
	};

	CTaskCarReactToVehicleCollisionGetOut(CVehicle* pMyVehicle, CVehicle* pOtherVehicle, const Vector3& vDamagePos,float fDamageDone, u8 nFlags = 0);
	~CTaskCarReactToVehicleCollisionGetOut();

	virtual aiTask* Copy() const { return rage_new CTaskCarReactToVehicleCollisionGetOut(m_pMyVehicle, m_pOtherVehicle, m_vDamagePos, m_fDamageDone, m_nFlags); }

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_CAR_REACT_TO_VEHICLE_COLLISION_GET_OUT; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual s32 GetDefaultStateAfterAbort() const { return State_finish; }

protected:

	// PURPOSE: Determine whether or not the task should abort
	// RETURNS: True if should abort
	virtual bool ShouldAbort(const AbortPriority priority, const aiEvent* pEvent);

private:

	bool ShouldWaitBeforeExitingVehicle() const;

private:

	//
	// Members
	//
	FSM_Return Start_OnUpdate(CPed* pPed);

	FSM_Return WaitBeforeExitingVehicle_OnUpdate(CPed* pPed);

	FSM_Return ExitVehicle_OnEnter(CPed* pPed);
	FSM_Return ExitVehicle_OnUpdate(CPed* pPed);

	FSM_Return OnFire_OnEnter(CPed* pPed);
	FSM_Return OnFire_OnUpdate();

	FSM_Return Collapse_OnEnter(CPed* pPed);
	FSM_Return Collapse_OnUpdate();
	
	FSM_Return Agitated_OnEnter();
	FSM_Return Agitated_OnUpdate();

	FSM_Return MoveToImpactPoint_OnEnter();
	FSM_Return MoveToImpactPoint_OnUpdate(CPed* pPed);

	FSM_Return LookAtImpactPoint_OnEnter(CPed* pPed);
	FSM_Return LookAtImpactPoint_OnUpdate();

	FSM_Return EnterVehicle_OnEnter();
	FSM_Return EnterVehicle_OnUpdate();

	FSM_Return Fight_OnEnter();
	FSM_Return Fight_OnUpdate(CPed* pPed);

	typedef enum {
		State_start,
		State_waitBeforeExitingVehicle,
		State_exitVehicle,
		State_onFire,
		State_collapse,
		State_agitated,
		State_moveToImpactPoint,
		State_lookAtImpactPoint,
		State_fight,
		State_enterVehicle,
		State_finish
	}State;

	/*
	enum eState
	{
		STATE_EXIT_VEHICLE = 0,
		STATE_ON_FIRE,
		STATE_COLLAPSE,
		STATE_ORIENTATE,
		STATE_MOVE_TO_IMPACT_POINT,
		STATE_LOOK_AT_IMPACT_POINT,
		STATE_SAY_AUDIO,
		STATE_COMBAT,
		STATE_ENTER_VEHICLE,
		STATE_FINISHED,
	};

	// Task state
	eState m_state;
*/

	// My vehicle
	RegdVeh m_pMyVehicle;

	// Vehicle that has crashed into me
	RegdVeh m_pOtherVehicle;

	// Driver of other vehicle
	RegdPed m_pOtherDriver;

	// Point on my vehicle where other vehicle collided - point local to my vehicle space
	Vector3 m_vDamagePos;

	// The amount of damage done to the vehicle
	float m_fDamageDone;

	// Flags
	u8 m_nFlags;
};

#endif // TASK_CAR_COLLISION_RESPONSE
