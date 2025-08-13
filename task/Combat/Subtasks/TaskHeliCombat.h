// FILE :    TaskHeliCombat.h
// PURPOSE : Subtask of combat used for heli combat

#ifndef TASK_HELI_COMBAT_H
#define TASK_HELI_COMBAT_H

// Game headers
#include "ai/AITarget.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

// Forward declarations
class CHeli;

//=========================================================================
// CTaskHeliCombat
//=========================================================================

class CTaskHeliCombat : public CTask
{

public:

	enum State
	{
		State_Start = 0,
		State_Chase,
		State_Strafe,
		State_Refuel,
		State_Finish,
	};

public:

	struct Tunables : public CTuning
	{
		struct Chase
		{
			Chase()
			{}
			
			float m_MinSpeed;
			float m_MinTargetOffsetX;
			float m_MaxTargetOffsetX;
			float m_MinTargetOffsetY;
			float m_MaxTargetOffsetY;
			float m_MinTargetOffsetZ;
			float m_MaxTargetOffsetZ;
			float m_MinTargetOffsetZ_TargetInAir;
			float m_MaxTargetOffsetZ_TargetInAir;
			
			PAR_SIMPLE_PARSABLE;
		};
		
		Tunables();
		
		Chase m_Chase;

		PAR_PARSABLE;
	};

	CTaskHeliCombat(const CAITarget& rTarget);
	virtual ~CTaskHeliCombat();

public:

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

private:

	Vec3V_Out		CalculateTargetOffsetForChase() const;
	Vec3V_Out		CalculateTargetVelocity() const;
	CHeli*			GetHeli() const;
	const CEntity*	GetTargetEntity() const;
	const CPed*		GetTargetPed() const;
	CVehicle*		GetTargetVehicle() const;
	bool			ShouldChase() const;
	bool			ShouldStrafe() const;
	void			UpdateTargetOffsetForChase();

private:

	virtual aiTask* Copy() const { return rage_new CTaskHeliCombat(m_Target); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_HELI_COMBAT; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	void		Chase_OnEnter();
	FSM_Return	Chase_OnUpdate();
	void		Strafe_OnEnter();
	FSM_Return	Strafe_OnUpdate();
	void		Refuel_OnEnter();
	FSM_Return	Refuel_OnUpdate();
	void		Refuel_OnExit();
	
private:

	CAITarget m_Target;
	CTaskTimer m_ChangeStateTimer;

private:
	
	static Tunables	sm_Tunables;

};

#endif // TASK_HELI_COMBAT_H
