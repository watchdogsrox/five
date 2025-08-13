//
// boat flee
// April 24, 2013
//

#pragma once


#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"


class CTaskVehicleFleeBoat : public CTaskVehicleMissionBase
{
public:
	enum State
	{
		State_Init,
		State_Flee,
	};

	struct Tunables : CTuning
	{
		Tunables();

		float m_FleeDistance;
		PAR_PARSABLE;
	};


	// Constructor/destructor
	CTaskVehicleFleeBoat(const sVehicleMissionParams& params);
	CTaskVehicleFleeBoat(const CTaskVehicleFleeBoat& in_rhs);
	~CTaskVehicleFleeBoat();


	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_FLEE_BOAT; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Init; }
	virtual aiTask* Copy() const {return rage_new CTaskVehicleFleeBoat(*this);}

	FSM_Return ProcessPreFSM();
	FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleFleeBoat>; }

protected:

	Vector3::Return CalculateTargetCoords(const CVehicle& pVehicle) const;

	FSM_Return Init_OnUpdate(CVehicle& in_Vehicle);

	void Flee_OnEnter(CVehicle& in_Vehicle);
	FSM_Return Flee_OnUpdate(CVehicle& in_Vehicle);

	fwFlags8 m_uRunningFlags;
	static Tunables sm_Tunables;

public:

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char*  GetStaticStateName( s32 iState );
	void Debug() const;
#endif //!__FINAL


};
