#ifndef INC_TASK_PARKED_VEHICLE_SCENARIO_H
#define INC_TASK_PARKED_VEHICLE_SCENARIO_H

#include "Task/Scenario/TaskScenario.h"

//-------------------------------------------------------------------------
// Parked Car Scenario
//-------------------------------------------------------------------------
class CTaskParkedVehicleScenario : public CTaskScenario
{
public:

	// FSM states
	enum
	{
		State_Start = 0,
		State_PlayAmbients,
		State_ReturnToPosition,
		State_Finish
	};

	static int					PickScenario(CVehicle* pVehicle, ParkedType parkedType);
	static CTask*				CreateScenarioTask(int scenarioTypeIndex, Vector3& vPedPos, float& fHeading, CVehicle* pVehicle);
	static bool					CanVehicleBeUsedForBrokenDownScenario(const CVehicle& veh);

public:

	CTaskParkedVehicleScenario(s32 scenarioType, CVehicle* pScenarioVehicle, bool bWarpPed = true);
	~CTaskParkedVehicleScenario();

	// Task required implementations
	virtual aiTask* Copy() const { return rage_new CTaskParkedVehicleScenario(m_iScenarioIndex, m_pScenarioVehicle.Get(), m_bWarpPed); }
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_PARKED_VEHICLE_SCENARIO;}
	virtual s32 GetDefaultStateAfterAbort() const	{ return State_Start; }

protected:

	// Determine whether or not the task should abort
	virtual bool ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);
	// 
	// 	// Handle aborting the task - this will be called in addition to CleanUp when we are aborting
	// 	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);

private:

	// State Machine Update Functions
	virtual FSM_Return			ProcessPreFSM();
	virtual FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// Local state function calls
	FSM_Return					Start_OnUpdate				();
	void						PlayAmbients_OnEnter		();
	FSM_Return					PlayAmbients_OnUpdate		();
	void						ReturnToPosition_OnEnter	();
	FSM_Return					ReturnToPosition_OnUpdate	();

	// Helper Functions
	bool						SetupVehicleForScenario(const CScenarioParkedVehicleInfo* pPVInfo, CVehicle* pVehicle);
	void						CalculateScenarioInitialPositionAndHeading();

private:

	Vector3						m_vOriginalPedPos;
	Vector3						m_vOriginalVehiclePos;
	float						m_fOriginalPedHeading;
	float						m_fOriginalVehicleHeading;
	RegdVeh						m_pScenarioVehicle;
	bool						m_bSetUpVehicle;
	bool						m_bWarpPed;

#if !__FINAL
public:
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

};

#endif//!INC_TASK_PARKED_VEHICLE_SCENARIO_H