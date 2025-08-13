#ifndef TASK_PLAYER_WEAPON_H
#define TASK_PLAYER_WEAPON_H

// Game headers
#include "Scene/RegdRefTypes.h"
#include "Task/System/Task.h"

//////////////////////////////////////////////////////////////////////////
// CTaskPlayerWeapon
//////////////////////////////////////////////////////////////////////////

class CTaskPlayerWeapon : public CTask
{
public:

	enum State
	{
		State_Weapon = 0,
		State_Quit,
	};

	CTaskPlayerWeapon(s32 iGunFlags = 0);

	// Clone used to produce an exact copy of task
	virtual aiTask* Copy() const { return rage_new CTaskPlayerWeapon(m_iGunFlags); }

	// Returns the task Id. Each task class has an Id for quick identification
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_PLAYER_WEAPON; }

protected:

	virtual	void CleanUp();
	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Quit; }

	void CheckForMeleeAction();

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

private:

	FSM_Return Weapon_OnEnter();
	FSM_Return Weapon_OnUpdate(CPed* pPed);
	
	// Update the movement task
	void UpdateMovementTask(CPed* pPed);

	u32   GetFiringPatternHash() const;
	s32 GetGunFlags() const;

	//
	// Members
	//

	// Helper to stream in ambient clip group
	CAmbientClipRequestHelper m_ambientClipRequestHelper;

	// Flags
	bool m_bDoingRunAndGun;
	bool m_bSkipIdleTransition;
	s32	 m_iGunFlags;

	//
	// Static members
	//

	static u32 ms_uiFrameLastSwitchedCrouchedState;
};

#endif // TASK_PLAYER_WEAPON_H
