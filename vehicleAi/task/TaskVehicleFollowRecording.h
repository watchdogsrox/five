#ifndef TASK_VEHICLE_FOLLOW_RECORDING_H
#define TASK_VEHICLE_FOLLOW_RECORDING_H

// Gta headers.
#include "Renderer/HierarchyIds.h"
#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"
#include "Task\System\TaskTypes.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"

class CVehicle;
class CVehControls;

//Rage headers.
#include "Vector/Vector3.h"

//
//
//
class CTaskVehicleFollowRecording : public CTaskVehicleMissionBase
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_FollowRecording = 0,
		State_Stop
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehicleFollowRecording(const sVehicleMissionParams& params);
	~CTaskVehicleFollowRecording();

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_FOLLOW_RECORDING; }
	aiTask*			Copy() const {return rage_new CTaskVehicleFollowRecording(m_Params);}

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_FollowRecording;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleFollowRecording>; }
    virtual void CloneUpdate(CVehicle *pVehicle);
private:

	// FSM function implementations
	// State_FollowRecording
	void			FollowRecording_OnEnter				(CVehicle* pVehicle);
	FSM_Return		FollowRecording_OnUpdate			(CVehicle* pVehicle);

	//State_Stop
	void			Stop_OnEnter						(CVehicle* pVehicle);
	FSM_Return		Stop_OnUpdate						(CVehicle* pVehicle);

	void			ControlAIVeh_FollowPreRecordedPath	(CVehicle* pVeh);

	void HelperSetupParamsForSubtask(sVehicleMissionParams& params);
};

class CTaskVehicleFollowWaypointRecording : public CTaskVehicleMissionBase
{
public:
	typedef enum 
	{
		State_Invalid = -1,
		State_FollowRecording,
		State_Paused,
		State_Finished,
	} VehicleControlState;

	CTaskVehicleFollowWaypointRecording(::CWaypointRecordingRoute * pRoute, const sVehicleMissionParams& params, const int iInitialProgress=0, const u32 iFlags=0, const int iTargetProgress=-1);
	~CTaskVehicleFollowWaypointRecording();

	int			GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_FOLLOW_WAYPOINT_RECORDING; }
	aiTask*		Copy() const 
	{
		CTaskVehicleFollowWaypointRecording* pNewTask = rage_new CTaskVehicleFollowWaypointRecording(m_waypoints.GetRoute(), m_Params, m_waypoints.GetProgress(), m_waypoints.GetFlags(), m_waypoints.GetTargetProgress());
		if (pNewTask)
		{
			pNewTask->SetLoopPlayback(m_waypoints.GetIsLoopPlayback());
		}
		return pNewTask;
	}

	// FSM implementations
	FSM_Return	ProcessPreFSM						();
	FSM_Return	UpdateFSM							(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort			()	const {return State_FollowRecording;}

	int GetProgress() const { return m_waypoints.GetProgress(); }
	void SetLoopPlayback(const bool b) { m_waypoints.SetLoopPlayback(b); }
	bool GetIsPaused();
	void SetPause();
	void SetResume();
	void SetOverrideSpeed(const float fMBR);
	void UseDefaultSpeed();
	CWaypointRecordingRoute * GetRoute() { return m_waypoints.GetRoute(); }
	void ClearRoute() { m_waypoints.ClearRoute(); }
	void SetQuitImmediately() { m_bWantToQuitImmediately = true; }

	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);
	virtual const CVehicleFollowRouteHelper* GetFollowRouteHelper() const { return (const CVehicleFollowRouteHelper*)&m_followRoute; }

	static float GetLookaheadDistanceForFollowRecording(const CVehicle* pVehicle, const CTaskVehicleMissionBase* pSubtask, const float fOurCruiseSpeed);

	virtual bool IsIgnoredByNetwork() const { return true; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	virtual void		Debug() const;
#endif //!__FINAL
private:
	// State_FollowRecording
	void			FollowRecording_OnEnter				(CVehicle* pVehicle);
	FSM_Return		FollowRecording_OnUpdate			(CVehicle* pVehicle);

	//State_Paused
	void			Paused_OnEnter						(CVehicle* pVehicle);
	FSM_Return		Paused_OnUpdate						(CVehicle* pVehicle);

	//State_Finished
	void			Finished_OnEnter					(CVehicle* pVehicle);
	FSM_Return		Finished_OnUpdate					(CVehicle* pVehicle);

	// Calculates the position the vehicle should be driving towards and the speed.
	// Returns the distance to the end of the route
	float FindTargetPositionAndCapSpeed( CVehicle* pVehicle, const Vector3& vStartCoords, Vector3& vTargetPosition, float& fOutSpeed );
	void ProcessSuperDummyHelper(CVehicle* pVehicle, const float fBackupSpeed) const; 

	CVehicleWaypointRecordingHelper		m_waypoints;
	CVehicleFollowRouteHelper			m_followRoute;
	float								m_fPathLength;
	bool								m_bWantToQuitImmediately;
};

#endif // TASK_VEHICLE_FOLLOW_RECORDING_H
