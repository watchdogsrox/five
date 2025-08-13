#ifndef TASK_DETONATOR_H
#define TASK_DETONATOR_H

// Game headers
#include "Task/System/Task.h"
#include "Task/Weapons/WeaponController.h"
#include "Task/Weapons/WeaponTarget.h"

//////////////////////////////////////////////////////////////////////////
// CTaskDetonator
//////////////////////////////////////////////////////////////////////////

class CTaskDetonator : public CTask
{
public:

	CTaskDetonator(const CWeaponController::WeaponControllerType weaponControllerType, float fDuration = -1);
	virtual ~CTaskDetonator();

	// Clone used to produce an exact copy of task
	virtual aiTask* Copy() const { return rage_new CTaskDetonator(m_weaponControllerType, m_fDuration); }

	// Returns the task Id. Each task class has an Id for quick identification
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_DETONATOR; }

	bool GetHasFired() const { return m_bHasFired; }

protected:

	// The task state
	enum State
	{
		State_Init = 0,
		State_Detonate,
		State_DuckAndDetonate,
		State_Swap,
		State_Quit,
	};

	virtual FSM_Return ProcessPreFSM();

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual s32 GetDefaultStateAfterAbort() const { return State_Quit; }

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

private:

	//
	// State functions
	//
	FSM_Return StateInit_OnUpdate(CPed* pPed);
	FSM_Return StateDetonate_OnEnter(CPed* pPed);
	FSM_Return StateDetonate_OnUpdate(CPed* pPed);
	FSM_Return StateDuckAndDetonate_OnEnter(CPed* pPed);
	FSM_Return StateDuckAndDetonate_OnUpdate(CPed* pPed);
	FSM_Return StateSwap_OnEnter(CPed* pPed);
	FSM_Return StateSwap_OnUpdate(CPed* pPed);

	//
	// Members
	//

	// Weapon controller
	const CWeaponController::WeaponControllerType m_weaponControllerType;

	// Time (in milliseconds) before ending the task (-1 for infinite)
	float m_fDuration;

	bool m_bHasFired;

	static fwMvNetworkId ms_DetonateNetworkId;
	static fwMvClipId	ms_CrouchFireId;
	static fwMvClipId	ms_FireId;

	CGenericClipHelper			m_GenericClipHelper;
	float						m_fDetonateTime;
	bool						InsertDetonatorNetworkCB(fwMoveNetworkPlayer* pPlayer, float fBlendDuration, u32);

};

#endif // TASK_DETONATOR_H
