#ifndef TASK_VEHICLE_GO_TO_H
#define TASK_VEHICLE_GO_TO_H

// Gta headers.
#include "Renderer/HierarchyIds.h"
#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"
#include "Task\System\TaskTypes.h"
#include "VehicleAi\racingline.h"
#include "VehicleAi\VehMission.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"
#include "modelinfo\PedModelInfo.h"

class CVehicle;
class CVehControls;

//Rage headers.
#include "Vector/Vector3.h"
#include "vector/vector2.h"

//
//
//
class CTaskVehicleGoTo : public CTaskVehicleMissionBase
{
public:
	// Constructor/destructor
	CTaskVehicleGoTo(const sVehicleMissionParams& params);

	virtual ~CTaskVehicleGoTo();

	// network:
	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleGoTo>; }

#if !__FINAL
	static const char*  GetStaticStateName			( s32  )
	{
		return "<none>";
	}
#endif

};


//
//
// Makes the vehicle follow the target
class CTaskVehicleFollow : public CTaskVehicleMissionBase
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Follow,
		State_FollowAutomobile,
		State_FollowHelicopter,
		State_FollowSubmarine,
		State_Stop,
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehicleFollow(const sVehicleMissionParams& params,	const int FollowDistance = 20);
	CTaskVehicleFollow(const CTaskVehicleFollow& in_rhs);

	~CTaskVehicleFollow(){;}

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_FOLLOW; }
	aiTask*			Copy() const {return rage_new CTaskVehicleFollow(*this);}

	// FSM implementations
	virtual void		CleanUp();
	FSM_Return	UpdateFSM					(const s32 iState, const FSM_Event iEvent);
	s32		GetDefaultStateAfterAbort	()	const {return State_Follow;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char*  GetStaticStateName			( s32 iState );
#endif //!__FINAL

	// network:
	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleFollow>; }


	void SetHelicopterSpecificParams(float fHeliRequestedOrientation, int iFlightHeight, int iMinHeightAboveTerrain, s32 iHeliFlags);

	void Serialise(CSyncDataBase& serialiser)
	{
		CTaskVehicleMissionBase::Serialise(serialiser);

		SERIALISE_UNSIGNED(serialiser, m_iFollowCarDistance, SIZEOF_FOLLOW_DISTANCE, "Follow distance");
	}

protected:

	void ModifyTargetForArrivalTolerance(Vector3 &io_targetPos, const CVehicle& in_Vehicle ) const;

	static const unsigned SIZEOF_FOLLOW_DISTANCE = 7;

	// FSM function implementations
	// State_Follow
	FSM_Return	Follow_OnUpdate				(CVehicle* pVehicle);
	// State_FollowAutomobile
	void		FollowAutomobile_OnEnter	(CVehicle* pVehicle);
	FSM_Return	FollowAutomobile_OnUpdate	(CVehicle* pVehicle);
	// State_FollowHelicopter
	void		FollowHelicopter_OnEnter	(CVehicle* pVehicle);
	FSM_Return	FollowHelicopter_OnUpdate	(CVehicle* pVehicle);
	// State_FollowSubmarine
	void		FollowSubmarine_OnEnter		(CVehicle* pVehicle);
	FSM_Return	FollowSubmarine_OnUpdate	(CVehicle* pVehicle);
	// State_Stop
	void		Stop_OnEnter				(CVehicle* pVehicle);
	FSM_Return	Stop_OnUpdate				(CVehicle* pVehicle);

	//update the subtasks target
	void		UpdateTarget				();
	void		UpdateAvoidanceCache		();

	u32			m_iFollowCarDistance;			// The distance at which we follow another car for MISSION_FOLLOWCAR_CLOSE


	// Optional helicopter params
	float	m_fHeliRequestedOrientation;
	int		m_iFlightHeight; 
	int		m_iMinHeightAboveTerrain;
	s32		m_iHeliFlags;


};


#endif
