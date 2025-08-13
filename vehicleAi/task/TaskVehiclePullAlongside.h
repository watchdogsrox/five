#ifndef TASK_VEHICLE_PULL_ALONGSIDE_H
#define TASK_VEHICLE_PULL_ALONGSIDE_H

// Rage headers
#include "fwutil/Flags.h"

// Game headers
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"
#include "VehicleAi/task/TaskVehicleMissionBase.h"

//=========================================================================
// CTaskVehiclePullAlongside
//=========================================================================

class CTaskVehiclePullAlongside : public CTaskVehicleMissionBase
{

private:

	enum RunningFlags
	{
		RF_HasBeenAlongside = BIT0,
		RF_FromInfront = BIT1,
	};

	enum State
	{
		State_Start = 0,
		State_GetInPosition,
		State_Finish,
	};

public:

	struct Tunables : public CTuning
	{
		Tunables();
		
		float	m_TimeToLookAhead;
		float	m_MinDistanceToLookAhead;
		float	m_OverlapSpeedMultiplier;
		float	m_MaxSpeedDifference;
		float	m_AlongsideBuffer;

		PAR_PARSABLE;
	};

	CTaskVehiclePullAlongside(	const sVehicleMissionParams& rParams, 
								float fStraightLineDistance = sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST, 
								bool bFromInfront = false);
	virtual ~CTaskVehiclePullAlongside();

public:

	bool HasBeenAlongside() const
	{
		return (m_uRunningFlags.IsFlagSet(RF_HasBeenAlongside));
	}

public:

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

	// network:
	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehiclePullAlongside>; }

private:

	virtual aiTask* Copy() const;
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_PULL_ALONGSIDE; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual void		CleanUp();
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	void		GetInPosition_OnEnter();
	FSM_Return	GetInPosition_OnUpdate();

private:

	const CPed*		GetTargetPed() const;
	const CVehicle*	GetTargetVehicle() const;
	const CVehicle*	GetVehicleToPullAlongside() const;

	void			UpdateVehicleMissionForGetInPosition();
	void			UpdateCruiseSpeedForGetInPosition(CTaskVehicleMissionBase& rTask);
	void			UpdateTargetPositionForGetInPosition(CTaskVehicleMissionBase& rTask);
	bool			UpdateTargetPositionForGetInPositionInfront(CTaskVehicleMissionBase& rTask);
	void			UpdateCruiseSpeedForGetInPositionInfront(CTaskVehicleMissionBase& rTask, bool bCruiseSpeedSet);
	bool			CanBrakeToAvoidCollision() const;
	float			GetRequiredAlongsideBuffer() const;
	float			GetRequiredForwardBuffer() const;
private:

	float		m_fStraightLineDistance;
	fwFlags8	m_uRunningFlags;
	
private:

	static Tunables	sm_Tunables;

};

#endif
