#ifndef TASK_WEAPON_H
#define TASK_WEAPON_H

// Game headers
#include "Task/System/Task.h"
#include "Task/Weapons/WeaponController.h"
#include "Task/Weapons/WeaponTarget.h"

//////////////////////////////////////////////////////////////////////////
// CTaskWeapon
//////////////////////////////////////////////////////////////////////////

class CTaskWeapon : public CTask
{
public:

	CTaskWeapon(const CWeaponController::WeaponControllerType weaponControllerType, CTaskTypes::eTaskType aimTaskType, s32 iFlags, u32 uFiringPatternHash = 0, const CWeaponTarget& target = CWeaponTarget(), float fDuration = -1);
	CTaskWeapon(const CTaskWeapon& other);

	// Clone used to produce an exact copy of task
	virtual aiTask* Copy() const { return rage_new CTaskWeapon(*this); }

	// Returns the task Id. Each task class has an Id for quick identification
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_WEAPON; }

	virtual void GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const;

	// The task state
	enum State
	{
		State_Init = 0,
		State_Gun,
		State_Projectile,
		State_Bomb,
		State_Detonator,
		State_Quit,
	};

	// Is the task able to fire?
	bool GetIsReadyToFire() const;

	// Get whether we are reloading
	bool GetIsReloading() const;

	// Check if we're assisted aiming
	bool GetIsAssistedAiming() const { return m_bShouldUseAssistedAimingCamera; }

protected:

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

	FSM_Return StateInitOnUpdate(CPed* pPed);
	FSM_Return StateGunOnEnter(CPed* pPed);
	FSM_Return StateGunOnUpdate(CPed* pPed);
	FSM_Return StateProjectileOnEnter(CPed* pPed);
	FSM_Return StateProjectileOnUpdate(CPed* pPed);
	FSM_Return StateBombOnEnter(CPed* pPed);
	FSM_Return StateBombOnUpdate(CPed* pPed);
	FSM_Return StateDetonatorOnEnter(CPed* pPed);
	FSM_Return StateDetonatorOnUpdate(CPed* pPed);

	//
	// Members
	//

	// Weapon controller
	const CWeaponController::WeaponControllerType m_weaponControllerType;

	// Further control of what the task does
	fwFlags32 m_iFlags;

	// Which aim task to use
	CTaskTypes::eTaskType m_aimTaskType;

	// Targeting
	CWeaponTarget m_target;

	// Used to look up firing pattern from CFiringPatternManager
	u32 m_uFiringPatternHash;

	// Time (in milliseconds) before ending the task (-1 for infinite)
	float m_fDuration;

	// We need to keep track that we used aim assist because otherwise we get a frame when request an aim
	// cam when we stop aiming 
	u32 m_bShouldUseAssistedAimingCamera : 1;
	u32 m_bShouldUseRunAndGunAimingCamera : 1;
};

#endif // TASK_WEAPON_H
