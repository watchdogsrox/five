#ifndef TASK_VEHICLE_POLICE_BEHAVIOUR_H
#define TASK_VEHICLE_POLICE_BEHAVIOUR_H

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
class CTaskVehiclePoliceBehaviour : public CTaskVehicleMissionBase
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Start = 0,
		State_Cruise,
		State_Ram,
		State_Follow,
		State_Block,
		State_Helicopter,
		State_Boat,
		State_Stop,
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehiclePoliceBehaviour			( const sVehicleMissionParams& params );
	~CTaskVehiclePoliceBehaviour		();

	int				GetTaskTypeInternal			() const { return CTaskTypes::TASK_VEHICLE_POLICE_BEHAVIOUR; }
	CTask*			Copy				() const {return rage_new CTaskVehiclePoliceBehaviour(m_Params);}

	// FSM implementations
	FSM_Return		UpdateFSM			(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const	{return State_Start;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char*  GetStaticStateName			( s32 iState );

#endif //!__FINAL

	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehiclePoliceBehaviour>; }

private:

	// FSM function implementations
	FSM_Return	Start_OnUpdate			(CVehicle* pVehicle);

	void		Cruise_OnEnter			(CVehicle* pVehicle);
	FSM_Return	Cruise_OnUpdate			(CVehicle* pVehicle);

	void		Ram_OnEnter				(CVehicle* pVehicle);
	FSM_Return	Ram_OnUpdate			(CVehicle* pVehicle);

	void		Follow_OnEnter			(CVehicle* pVehicle);
	FSM_Return	Follow_OnUpdate			(CVehicle* pVehicle);

	void		Block_OnEnter			(CVehicle* pVehicle);
	FSM_Return	Block_OnUpdate			(CVehicle* pVehicle);

	void		Helicopter_OnEnter		(CVehicle* pVehicle);
	FSM_Return	Helicopter_OnUpdate		(CVehicle* pVehicle);

	void		Boat_OnEnter			(CVehicle* pVehicle);
	FSM_Return	Boat_OnUpdate			(CVehicle* pVehicle);

	void		Stop_OnEnter			(CVehicle* pVehicle);
	FSM_Return	Stop_OnUpdate			(CVehicle* pVehicle);

	void		DeterminePoliceBehaviour(CVehicle* pVeh);

	float		m_fStraightLineDist;
};


//
//
//
class CTaskVehiclePoliceBehaviourHelicopter : public CTaskVehicleMissionBase
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Start = 0,
		State_Cruise,
		State_Circle,
		State_Follow,
		State_MaintainAltitude,
		State_Stop,
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehiclePoliceBehaviourHelicopter( const sVehicleMissionParams& params );
	~CTaskVehiclePoliceBehaviourHelicopter();

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_POLICE_BEHAVIOUR_HELICOPTER; }
	CTask*			Copy() const {return rage_new CTaskVehiclePoliceBehaviourHelicopter(m_Params);}

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const	{return State_Start;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehiclePoliceBehaviourHelicopter>; }

private:

	// FSM function implementations
	FSM_Return	Start_OnUpdate					(CVehicle* pVehicle);

	void		Follow_OnEnter					(CVehicle* pVehicle);
	FSM_Return	Follow_OnUpdate					(CVehicle* pVehicle);

	void		MaintainAltitude_OnEnter		(CVehicle* pVehicle);
	FSM_Return	MaintainAltitude_OnUpdate		(CVehicle* pVehicle);

	void		DeterminePoliceHeliBehaviour	(CVehicle* pVeh);

	float	m_fStraightLineDist;
	u16		m_iFlightHeight;
	u16		m_iMinHeightAboveTerrain;
};


//
//
//
class CTaskVehiclePoliceBehaviourBoat : public CTaskVehicleMissionBase
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Start = 0,
		State_Cruise,
		State_Circle,
		State_Ram,
		State_Stop,
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehiclePoliceBehaviourBoat( const sVehicleMissionParams& params);
	~CTaskVehiclePoliceBehaviourBoat(){;}

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_POLICE_BEHAVIOUR_BOAT; }
	CTask*			Copy() const {return rage_new CTaskVehiclePoliceBehaviourBoat(m_Params);}

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const	{return State_Start;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehiclePoliceBehaviourBoat>; }

private:

	// FSM function implementations
	FSM_Return	Start_OnUpdate					(CVehicle* pVehicle);

	void		Cruise_OnEnter					(CVehicle* pVehicle);
	FSM_Return	Cruise_OnUpdate					(CVehicle* pVehicle);

	void		Circle_OnEnter					(CVehicle* pVehicle);
	FSM_Return	Circle_OnUpdate					(CVehicle* pVehicle);

	void		Ram_OnEnter						(CVehicle* pVehicle);
	FSM_Return	Ram_OnUpdate					(CVehicle* pVehicle);

	void		Stop_OnEnter					(CVehicle* pVehicle);
	FSM_Return	Stop_OnUpdate					(CVehicle* pVehicle);

	void		DeterminePoliceBoatBehaviour	(CVehicle* pVehicle);

	float		m_fStraightLineDist;
};



#endif
