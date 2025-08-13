#ifndef TASK_TOHURTTRANSIT_H
#define TASK_TOHURTTRANSIT_H

#include "Scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/TaskHelpers.h"
#include "Task/Weapons/WeaponTarget.h"
#include "Peds/ped.h"
#include "peds/QueriableInterface.h"

class CTaskToHurtTransit : public CTask
{
public:

	CTaskToHurtTransit(CEntity* pTarget);
	~CTaskToHurtTransit();

	virtual aiTask* Copy() const 
	{
		return rage_new CTaskToHurtTransit(m_pTarget);
	}
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_TO_HURT_TRANSIT; }
	virtual s32 GetDefaultStateAfterAbort() const { return State_Finish; }

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName(s32 );
#endif // !__FINAL

	FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	enum
	{
		State_ToHurtAnim,
		State_Finish,
		State_Invalid,
	};

protected:

	void ToHurtAnim_OnEnter();
	FSM_Return ToHurtAnim_OnUpdate();

private:

	RegdEnt m_pTarget;

	static const fwMvClipId ms_HurtClipId;

	static const fwMvClipSetId m_ClipSetIdPistolToHurt;
	static const fwMvClipSetId m_ClipSetIdRifleToHurt;
};


#endif

