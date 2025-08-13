// FILE :    TaskAmbulancePatrol.cpp
// PURPOSE : Basic patrolling task of a medic, patrol in vehicle or on foot until given specific orders by dispatch
// CREATED : 25-05-2009

#ifndef INC_TASK_AMBULANCE_PATROL_H
#define INC_TASK_AMBULANCE_PATROL_H

//Game headers
#include "peds/QueriableInterface.h"
#include "Scene/RegdRefTypes.h"
#include "Task/Default/TaskUnalerted.h"
#include "Task/System/Task.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/TaskHelperFSM.h"
#include "Task/Scenario/ScenarioChaining.h"
#include "Task/Scenario/ScenarioPoint.h"

class CAmbulanceOrder;

// Class declaration
class CTaskAmbulancePatrol : public CTaskFSMClone
{
public:
	// FSM states
	enum 
	{ 
		State_Start = 0,
		State_Resume,
		State_Unalerted,
		State_ChoosePatrol, 
		State_RespondToOrder, 
		State_PatrolInVehicle,
		State_PatrolAsPassenger,
		State_PatrolOnFoot,
		State_GoToIncidentLocationInVehicle,
		State_StopVehicle,
		State_GoToIncidentLocationOnFoot,
		State_GoToIncidentLocationAsPassenger,
		State_PullFromVehicle,
		State_ExitVehicleAndChooseResponse,		
		State_PlayRecoverySequence,		
		State_HealVictim,
		State_ReturnToAmbulance,		
		State_SuperviseRecovery,
		State_TransportVictim,
		State_TransportVictimAsPassenger,
		State_ReviveFailed,
		State_WaitForBothMedicsToFinish,
		State_WaitInAmbulance,
		State_Finish
	};

public:

	CTaskAmbulancePatrol (CTaskUnalerted* pUnalertedTask = NULL);
	virtual ~CTaskAmbulancePatrol();

	// Task required implementations
	virtual aiTask* Copy() const { return rage_new CTaskAmbulancePatrol(); }
	virtual int		GetTaskTypeInternal() const { return CTaskTypes::TASK_AMBULANCE_PATROL; }
	
	// FSM required implementations
	virtual FSM_Return ProcessPreFSM();
	virtual bool ProcessMoveSignals();
	virtual FSM_Return UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32	GetDefaultStateAfterAbort() const { return State_Resume; }

	virtual	void CleanUp();

	virtual	CScenarioPoint* GetScenarioPoint() const;
	virtual int GetScenarioPointType() const;

	CTaskUnalerted* GetTaskUnalerted() { return m_pUnalertedTask && m_pUnalertedTask->GetTaskType() == CTaskTypes::TASK_UNALERTED ? static_cast<CTaskUnalerted*>(m_pUnalertedTask.Get()) : NULL; }

// debug:
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL
private:
	// Local state function calls
	// Start
	FSM_Return Start_OnUpdate();
	FSM_Return Resume_OnUpdate();
	// Unalerted
	void Unalerted_OnEnter();
	FSM_Return Unalerted_OnUpdate();
	// Choose Patrol
	FSM_Return ChoosePatrol_OnUpdate();
	// Respond to order
	FSM_Return RespondToOrder_OnUpdate();
	// PatrolInVehicle
	void PatrolInVehicle_OnEnter();
	FSM_Return PatrolInVehicle_OnUpdate();
	// PatrolAsPassenger
	void PatrolAsPassenger_OnEnter();
	FSM_Return PatrolAsPassenger_OnUpdate();
	// PatrolOnFoot
	void PatrolOnFoot_OnEnter();
	FSM_Return PatrolOnFoot_OnUpdate();
	// GoToIncidentLocationInVehicle
	void GoToIncidentLocationInVehicle_OnEnter();
	FSM_Return GoToIncidentLocationInVehicle_OnUpdate();
	void GoToIncidentLocationInVehicle_OnExit();
	// StopVehicle
	void StopVehicle_OnEnter();
	FSM_Return StopVehicle_OnUpdate();
	// GoToIncidentLocationAsPassenger
	void GoToIncidentLocationAsPassenger_OnEnter();
	FSM_Return GoToIncidentLocationAsPassenger_OnUpdate();
	// GoToIncidentLocationOnFoot
	void GoToIncidentLocationOnFoot_OnEnter	();
	FSM_Return GoToIncidentLocationOnFoot_OnUpdate();
	//	ExitVehicleAndChooseResponse
	void ExitVehicleAndChooseResponse_OnEnter();
	FSM_Return ExitVehicleAndChooseResponse_OnUpdate();
	//	PlayRecoverySequence	
	void PlayRecoverySequence_OnEnter();
	FSM_Return PlayRecoverySequence_OnUpdate();
	void PlayRecoverySequence_OnExit();
	//	ReturnToAmbulance	
	void ReturnToAmbulance_OnEnter();
	FSM_Return ReturnToAmbulance_OnUpdate();
	//	SuperviseRecovery
	void SuperviseRecovery_OnEnter();
	FSM_Return SuperviseRecovery_OnUpdate();
	void SuperviseRecovery_OnExit();
	//	TransportVictim
	void TransportVictim_OnEnter();
	FSM_Return TransportVictim_OnUpdate();
	//	TransportVictimAsPassenger
	void TransportVictimAsPassenger_OnEnter();
	FSM_Return TransportVictimAsPassenger_OnUpdate();
	//	ReviveFailed
	void ReviveFailed_OnEnter();
	FSM_Return ReviveFailed_OnUpdate();
	//	WaitForBothMedicsToFinish
	void WaitForBothMedicsToFinish_OnEnter();
	FSM_Return WaitForBothMedicsToFinish_OnUpdate();
	//	WaitInAmbulance
	void WaitInAmbulance_OnEnter();
	FSM_Return WaitInAmbulance_OnUpdate();
	//	PullFromVehicle
	void PullFromVehicle_OnEnter();
	FSM_Return PullFromVehicle_OnUpdate();
	//	HealVictim
	void HealVictim_OnEnter();
	FSM_Return HealVictim_OnUpdate();


	void MarkPedAsFinishedWithOrder();
	bool UpdateCheckForNewReviver();
	CAmbulanceOrder* HelperGetAmbulanceOrder();
	const CAmbulanceOrder* HelperGetAmbulanceOrder() const;

	void HelperUpdateWaitForVictimToBeHealed(CPed* pPed);
	bool HasReviverFinished();
	
	bool CheckBodyCanBeReached(CPed* pPed);
	bool GetBestPointToDriveTo(CPed* pPed, Vector3& vLocation);
	//bool TestNodeForArrivial(const Vector3& vPedPos, const Vector3& vIncidentPos, const Vector3& vNodePos, float &fCurrentBestDistance);
	
	void LookAtTarget(CPed* pPed, CEntity* pTarget);
	void SetBodyPosition(Vector3 & vPos) { m_vBodyPosition = vPos; }
	bool HasBodyMoved(CPed* pPed);

	// Clone task implementation
	virtual void			OnCloneTaskNoLongerRunningOnOwner();
	virtual CTaskInfo*		CreateQueriableState() const;
	virtual void			UpdateClonedSubTasks(CPed* pPed, int const iState);
	virtual void            ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return		UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual CTaskFSMClone*	CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*	CreateTaskForLocalPed(CPed* pPed);

	static const int s_TimeBetweenReviverSearches = 1000;
	static const float s_fDriverWalkDistance;
	static const float s_fHealerWalkDistance;

	RegdTask m_pUnalertedTask;
	CScenarioPoint m_ScenarioPoint;
	CClipRequestHelper  m_clipHelper;
	int m_iNextTimeToCheckForNewReviver;
	int m_iTimeToLeaveBody; 
	bool m_bShouldApproachInjuredPed;
	bool m_bClipStarted;
	bool m_bAllowToUseTurnedOffNodes;
	bool m_bArriveAudioTriggered;
	Vector3 m_vBodyPosition;

	TDynamicObjectIndex m_iNavMeshBlockingBounds[2];

private:

	void DebugAmbulancePosition();

	// Scenario point resuming helper functions
	void	CacheScenarioPointToReturnTo();
	void	AttemptToCreateResumableUnalertedTask();

	// Scenario point resuming member variables
	RegdScenarioPnt					m_pPointToReturnTo;
	CScenarioPointChainUseInfoPtr	m_SPC_UseInfo; //NOTE: this class does not own this pointer do not delete it.
	int								m_iPointToReturnToRealTypeIndex;
};

//-------------------------------------------------------------------------
// Task info for CTaskAmbulancePatrol
//-------------------------------------------------------------------------
class CClonedAmbulancePatrolInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedAmbulancePatrolInfo(u32 const state, bool bEntityFinishedWithIncident, bool bIncidentHasBeenAddressed);
	CClonedAmbulancePatrolInfo();
	~CClonedAmbulancePatrolInfo() {}

	virtual bool HasState() const				{ return true; }
	virtual u32	 GetSizeOfState() const			{ return datBitsNeeded<CTaskAmbulancePatrol::State_Finish>::COUNT; }

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_AMBULANCE_PATROL;}

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser);

	bool m_bEntityFinishedWithIncident;
	bool m_bIncidentHasBeenAddressed;
};

#endif // INC_TASK_AMBULANCE_PATROL_H
