// FILE :    TaskBoatCombat.h
// PURPOSE : Subtask of combat used for boat combat

#ifndef TASK_BOAT_COMBAT_H
#define TASK_BOAT_COMBAT_H

// Game headers
#include "ai/AITarget.h"
#include "physics/WorldProbe/shapetestresults.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

// Forward declarations
class CBoat;

//=========================================================================
// CTaskBoatCombat
//=========================================================================

class CTaskBoatCombat : public CTask
{

public:

	enum State
	{
		State_Start = 0,
		State_Chase,
		State_Strafe,
		State_Wait,
		State_Finish,
	};
	
	enum RunningFlags
	{
		RF_IsCollisionImminent	= BIT0,
		RF_IsLandImminent		= BIT1,
		RF_IsTargetUnreachable	= BIT2,
	};

public:

	struct Tunables : public CTuning
	{
		struct Rendering
		{
			Rendering()
			{}

			bool m_Enabled;
			bool m_CollisionProbe;
			bool m_LandProbe;

			PAR_SIMPLE_PARSABLE;
		};

		Tunables();

		Rendering m_Rendering;
		
		float m_MinSpeedForChase;
		float m_TimeToLookAheadForCollision;
		float m_DepthForLandProbe;
		float m_TimeToWait;

		PAR_PARSABLE;
	};

	CTaskBoatCombat(const CAITarget& rTarget);
	virtual ~CTaskBoatCombat();

public:

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

private:

	CBoat*	GetBoat() const;
	void	ProcessProbes();
	void	ProcessTargetUnreachable();
	bool	ShouldChase() const;
	bool	ShouldStrafe() const;
	bool	ShouldWait() const;

private:

	virtual aiTask* Copy() const { return rage_new CTaskBoatCombat(m_Target); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_BOAT_COMBAT; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	void		Chase_OnEnter();
	FSM_Return	Chase_OnUpdate();
	void		Strafe_OnEnter();
	FSM_Return	Strafe_OnUpdate();
	void		Wait_OnEnter();
	FSM_Return	Wait_OnUpdate();
	
private:

	WorldProbe::CShapeTestSingleResult	m_CollisionProbeResults;
	WorldProbe::CShapeTestSingleResult	m_LandProbeResults;
	CNavmeshRouteSearchHelper			m_NavMeshRouteSearchHelper;
	CAITarget							m_Target;
	fwFlags8							m_uRunningFlags;

private:
	
	static Tunables	sm_Tunables;

};

#endif // TASK_BOAT_COMBAT_H
