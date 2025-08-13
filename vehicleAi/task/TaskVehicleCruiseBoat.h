//
// boat cruise
// April 24, 2013
//

#pragma once


#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"


class CTaskVehicleCruiseBoat : public CTaskVehicleMissionBase
{
public:
	enum State
	{
		State_Init,
		State_Cruise,
	};

	struct Tunables : CTuning
	{
		Tunables();

		float m_fTimeToPickNewPoint;
		float m_fDistToPickNewPoint;
		float m_fDistSearch;

		PAR_PARSABLE;
	};

	enum ConfigFlags
	{
		CF_SpeedFromPopulation			= BIT0,
		CF_NumConfigFlags				= 1
	};


	// Constructor/destructor
	CTaskVehicleCruiseBoat(const sVehicleMissionParams& params, const fwFlags8 uConfigFlags = 0);
	CTaskVehicleCruiseBoat(const CTaskVehicleCruiseBoat& in_rhs);
	~CTaskVehicleCruiseBoat();


	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_CRUISE_BOAT; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Init; }
	virtual aiTask* Copy() const {return rage_new CTaskVehicleCruiseBoat(*this);}
	virtual void SetTargetPosition(const Vector3 *pTargetPosition);
	virtual void SetParams(const sVehicleMissionParams& params);

	FSM_Return ProcessPreFSM();
	FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// network:
	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleCruiseBoat>; }

	void Serialise(CSyncDataBase& serialiser)
	{
		static const unsigned SIZEOF_CONFIG_FLAGS = CF_NumConfigFlags;

		CTaskVehicleMissionBase::Serialise(serialiser);

		u32 configFlags = (u32)m_uConfigFlags.GetAllFlags();

		SERIALISE_UNSIGNED(serialiser, configFlags, SIZEOF_CONFIG_FLAGS, "Config flags");

		m_uConfigFlags.SetAllFlags((u8)configFlags);
	}

protected:

	void PickCruisePoint(CVehicle& in_Vehicle);

	FSM_Return Init_OnUpdate(CVehicle& in_Vehicle);
	FSM_Return Cruise_OnEnter(CVehicle& in_Vehicle);
	FSM_Return Cruise_OnUpdate(CVehicle& in_Vehicle);

	fwFlags8 m_uConfigFlags;
	float m_fPickedCruiseSpeedWithVehModel;
	float m_fPickPointTimer;

	static Tunables sm_Tunables;

public:

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char*  GetStaticStateName( s32 iState );
	void Debug() const;
#endif //!__FINAL


};
