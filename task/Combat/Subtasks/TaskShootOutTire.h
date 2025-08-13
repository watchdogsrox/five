// FILE :    TaskShootOutTire.h
// PURPOSE : Combat subtask in control of shooting out a tire
// CREATED : 3/1/2012

#ifndef TASK_SHOOT_OUT_TIRE_H
#define TASK_SHOOT_OUT_TIRE_H

// Game headers
#include "AI/AITarget.h"
#include "renderer/HierarchyIds.h"
#include "Scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

// Forward declarations
class CVehicle;
class CWheel;

//=========================================================================
// CTaskShootOutTire
//=========================================================================

class CTaskShootOutTire : public CTask
{

public:

	struct Tunables : public CTuning
	{
		Tunables();

		float	m_MinTimeoutToAcquireLineOfSight;
		float	m_MaxTimeoutToAcquireLineOfSight;
		float	m_TimeBetweenLineOfSightChecks;
		float	m_MinTimeToWaitForShot;
		float	m_MaxTimeToWaitForShot;
		int		m_MaxWaitForShotFailures;
		float	m_MinSpeedToApplyReaction;

		PAR_PARSABLE;
	};
	
	enum ShootOutTireConfigFlags
	{
		SOTCF_ApplyReactionToVehicle	= BIT0,
	};
	
	enum ShootOutTireRunningFlags
	{
		SOTRF_HasLineOfSight	= BIT0,
	};

	enum ShootOutTireState
	{
		State_Start = 0,
		State_DecideWhichTireToShoot,
		State_AcquireLineOfSight,
		State_WaitForShot,
		State_Shoot,
		State_GiveUp,
		State_Finish
	};

	CTaskShootOutTire(const CAITarget& target, const fwFlags8 uConfigFlags = 0);
	virtual ~CTaskShootOutTire();

	virtual aiTask*	Copy() const;
	virtual s32	GetTaskTypeInternal() const {return CTaskTypes::TASK_SHOOT_OUT_TIRE;}
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif

	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	
private:

	void		Start_OnEnter();
	FSM_Return	Start_OnUpdate();
	void		DecideWhichTireToShoot_OnEnter();
	FSM_Return	DecideWhichTireToShoot_OnUpdate();
	void		AcquireLineOfSight_OnEnter();
	FSM_Return	AcquireLineOfSight_OnUpdate();
	void		WaitForShot_OnEnter();
	FSM_Return	WaitForShot_OnUpdate();
	void		Shoot_OnEnter();
	FSM_Return	Shoot_OnUpdate();
	void		GiveUp_OnEnter();
	FSM_Return	GiveUp_OnUpdate();
	
			void		ApplyDamageToTire();
			void		ApplyReactionToVehicle();
			Vec3V_Out	GetAimPosition() const;
	const	CPed*		GetTargetPed() const;
			CVehicle*	GetTargetVehicle() const;
			CWheel*		GetWheel() const;
			void		ProcessAimAtWheel();
			void		ProcessLineOfSight();
			void		SetStateAndKeepSubTask(s32 iState);
			void		ShootWeapon();
			bool		ShouldApplyReactionToVehicle() const;
			bool		ShouldProcessLineOfSight() const;
			void		UpdateLineOfSight();
	
private:

	CAITarget		m_Target;
	eHierarchyId	m_nTire;
	float			m_fTimeoutToAcquireLineOfSight;
	float			m_fTimeSinceLastLineOfSightCheck;
	float			m_fTimeToWaitForShot;
	u32				m_uWaitForShotFailures;
	fwFlags8		m_uConfigFlags;
	fwFlags8		m_uRunningFlags;
	
private:

	static Tunables	ms_Tunables;
	
};

#endif //TASK_SHOOT_OUT_TIRE_H
