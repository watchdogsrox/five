// FILE :    TaskFirePatrol.cpp
// PURPOSE : Basic patrolling task of a fireman, patrol in vehicle or on foot until given specific orders by dispatch
// CREATED : 25-05-2009

#ifndef INC_TASK_FIRE_DEFAULT_H
#define INC_TASK_FIRE_DEFAULT_H

//Game headers
#include "Scene/RegdRefTypes.h"
#include "Task/Default/TaskUnalerted.h"
#include "Task/Scenario/ScenarioChaining.h"
#include "Task/Scenario/ScenarioPoint.h"
#include "Task/System/Task.h"

class CFire;
class CFireOrder;

// Class declaration
class CTaskFirePatrol : public CTask
{
public:
	// FSM states
	enum 
	{ 
		State_Start = 0, 
		State_Unalerted,
		State_ChoosePatrol, 
		State_RespondToOrder, 
		State_PatrolInVehicle,
		State_PatrolAsPassenger,
		State_PatrolOnFoot,
		State_GoToIncidentLocationInVehicle,
		State_GoToIncidentLocationOnFoot,
		State_GoToIncidentLocationAsPassenger,
		State_DriverArriveAtIncident,
		State_StopVehicle,
		State_SprayFireWithFireExtinguisher,
		State_SprayFireWithMountedWaterHose,
		State_ExitVehicle,
		State_ReturnToTruck,
		State_WaitInTruck,
		State_MakeDecisionHowToEnd,
		State_HandleExpiredFire,
		State_Finish
	};

public:

	CTaskFirePatrol (CTaskUnalerted* pUnalertedTask = NULL);
	virtual ~CTaskFirePatrol();

	// Task required implementations
	virtual aiTask* Copy() const { return rage_new CTaskFirePatrol(); }
	virtual int		GetTaskTypeInternal() const { return CTaskTypes::TASK_FIRE_PATROL; }
	
	// FSM required implementations

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32	GetDefaultStateAfterAbort() const { return State_Start; }
	virtual bool ProcessMoveSignals();

	virtual	void CleanUp();

	virtual	CScenarioPoint* GetScenarioPoint() const;
	virtual int GetScenarioPointType() const;

	bool GetCantPathOnFoot() const { return m_bCantPathOnFoot; }

	CTaskUnalerted* GetTaskUnalerted() { return m_pUnalertedTask && m_pUnalertedTask->GetTaskType() == CTaskTypes::TASK_UNALERTED ? static_cast<CTaskUnalerted*>(m_pUnalertedTask.Get()) : NULL; }

// debug:
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL
private:
	// Local state function calls
	// Start
	FSM_Return Start_OnUpdate				();
	// Unalerted
	void Unalerted_OnEnter					();
	FSM_Return Unalerted_OnUpdate			();
	// Choose Patrol
	FSM_Return ChoosePatrol_OnUpdate		();
	// Respond to order
	FSM_Return RespondToOrder_OnUpdate		();
	// PatrolInVehicle
	void PatrolInVehicle_OnEnter			();
	FSM_Return PatrolInVehicle_OnUpdate		();
	// PatrolAsPassenger
	void PatrolAsPassenger_OnEnter			();
	FSM_Return PatrolAsPassenger_OnUpdate	();
	// PatrolOnFoot
	void PatrolOnFoot_OnEnter				();
	FSM_Return PatrolOnFoot_OnUpdate		();
	// GoToIncidentLocationInVehicle
	void GoToIncidentLocationInVehicle_OnEnter	();
	FSM_Return GoToIncidentLocationInVehicle_OnUpdate();
	void GoToIncidentLocationInVehicle_OnExit();
	// GoToIncidentLocationAsPassenger
	void GoToIncidentLocationAsPassenger_OnEnter	();
	FSM_Return GoToIncidentLocationAsPassenger_OnUpdate();
	// GoToIncidentLocationOnFoot
	void GoToIncidentLocationOnFoot_OnEnter		();
	FSM_Return GoToIncidentLocationOnFoot_OnUpdate();
	// DriverArriveAtIncident
	void DriverArriveAtIncident_OnEnter();
	FSM_Return DriverArriveAtIncident_OnUpdate();
	// DriverArriveAtIncident
	void StopVehicle_OnEnter();
	FSM_Return StopVehicle_OnUpdate();
	// SprayFireWithFireExtinguisher
	void SprayFireWithFireExtinguisher_OnEnter();
	FSM_Return SprayFireWithFireExtinguisher_OnUpdate();
	// SprayFireWithMountedWaterHose
	void SprayFireWithMountedWaterHose_OnEnter();
	FSM_Return SprayFireWithMountedWaterHose_OnUpdate();
	// ReturnToTruck
	void ReturnToTruck_OnEnter();
	FSM_Return ReturnToTruck_OnUpdate();
	// ExitVehicle
	void ExitVehicle_OnEnter();
	FSM_Return ExitVehicle_OnUpdate();
	//	WaitInTruck
	void WaitInTruck_OnEnter		();
	FSM_Return WaitInTruck_OnUpdate	();
	// MakeDecisionHowToEnd
	void MakeDecisionHowToEnd_OnEnter		();
	FSM_Return MakeDecisionHowToEnd_OnUpdate	();
	// HandleExpiredFire
	void HandleExpiredFire_OnEnter	();
	FSM_Return HandleExpiredFire_OnUpdate	();

	//Driver target location - updated everyframe
	Vector3 m_IncidentTargetLocation;

	//like the name says, this should only be used
	//for a lookup, it's not guaranteed to be valid
	//or it could be valid and point to a different fire
	CFire* m_pCurrentFireDontDereferenceMe;

	bool m_bShouldInvestigateExpiredFire;

	CFireOrder* HelperGetFireOrder();
	const CFireOrder* HelperGetFireOrder() const;

	void UpdateKeepFiresAlive();

	void RegisterFightingFire(CFire* pFire);
	void DeregisterFightingFire();

	void SetFiremanEventResponse();
	void ResetDefaultEventResponse();

	bool EvaluateUsingMountedWaterHose(CFireOrder* pOrder);

	void SetScenarioPoint(CVehicle* pVehicle, CScenarioPoint& pScenarioPoint);

	Vector3 GetBestTruckTargetLocation(CPed * pPed, CFireOrder* pOrder);
	SearchStatus CheckPedsCanReachFire(CPed* pPed, const Vector3& target, float fCompletionRadius);

	bool PassengerArrivedAtMobileCall(CPed* pPed);

	static const float s_fWalkToFireDistance;
	static const float s_fVehicleStopAwayFromIncident;

	//Timer used to keep the fire engine at an invalid incident
	int m_iTimeToIncident; 

	u32 m_PreviousDecisionMarkerId;

	//Scenario point used for the fire men to hang out at 
	CScenarioPoint m_pScenarioPoint;

	//Time to hang around in scenario 
	CTaskGameTimer m_iHangAroundTruckTimer;

	//Cache the nearest road node to the fire
	CNodeAddress m_NearestRoadNodeLocation;

	//Used to make only one ped take over from a dead/null driver
	bool m_bWantsToBeDriver : 1;

	//If spawned on a turned off road node then we're allowed to keep using turned off nodes
	bool m_bAllowToUseTurnedOffNodes : 1;

	//If the peds aren't going to be able to path to the fire then don't get off the truck.
	bool m_bCantPathOnFoot : 1;

	//Used to find out if it's possible to path on foot from the fire truck to the fire.
	CNavmeshRouteSearchHelper	m_RouteSearchHelper;

	//Handle being at a scenario as a fireman
	RegdTask m_pUnalertedTask;

	RegdOrder m_pIgnoreOrder;

private:

	// Scenario point resuming helper functions
	void	CacheScenarioPointToReturnTo();
	void	AttemptToCreateResumableUnalertedTask();

	// Scenario point resuming member variables
	RegdScenarioPnt					m_pPointToReturnTo;
	CScenarioPointChainUseInfoPtr	m_SPC_UseInfo; //NOTE: this class does not own this pointer do not delete it.
	int								m_iPointToReturnToRealTypeIndex;
};

#endif // INC_TASK_REACT_TO_DEAD_PED_H
