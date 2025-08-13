#ifndef TASK_VEHICLE_SPIN_OUT_H
#define TASK_VEHICLE_SPIN_OUT_H

// Rage headers
#include "fwutil/Flags.h"

// Game headers
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"
#include "VehicleAi/task/TaskVehicleMissionBase.h"

//=========================================================================
// CTaskVehicleSpinOut
//=========================================================================

class CTaskVehicleSpinOut : public CTaskVehicleMissionBase
{
private:

	enum RunningFlags
	{
		RF_HasMadeContact = BIT0,
	};

public:

	struct Tunables : public CTuning
	{
		Tunables();
		
		float	m_TimeToLookAhead;
		float	m_MinDistanceToLookAhead;
		float	m_BumperOverlapForMaxSpeed;
		float	m_BumperOverlapForMinSpeed;
		float	m_CatchUpSpeed;
		float	m_BumperOverlapToBeInPosition;
		float	m_MaxSidePaddingForTurn;
		float	m_TurnTime;
		float	m_InvMassScale;

		PAR_PARSABLE;
	};

	enum VehicleSpinOutState
	{
		State_Start = 0,
		State_GetInPosition,
		State_Turn,
		State_Finish,
	};

	CTaskVehicleSpinOut(const sVehicleMissionParams& rParams, float fStraightLineDistance = sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST);
	virtual ~CTaskVehicleSpinOut();

public:

	bool HasMadeContact() const
	{
		return (m_uRunningFlags.IsFlagSet(RF_HasMadeContact));
	}

public:

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

	// network:
	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleSpinOut>; }

private:

	virtual aiTask* Copy() const;
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_SPIN_OUT; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual void		CleanUp();
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	void		GetInPosition_OnEnter();
	FSM_Return	GetInPosition_OnUpdate();
	void		Turn_OnEnter();
	FSM_Return	Turn_OnUpdate();

private:

	void			AnalyzePositions(float& fBumperOverlap, float& fSidePadding) const;
	void			CheckForContact();
	float			GetBumperOverlap() const;
	const CVehicle*	GetTargetVehicle() const;
	bool			IsInPosition(bool& bCloseEnoughToTurn) const;
	void			UpdateCruiseSpeedForGetInPosition(CTaskVehicleMissionBase& rTask);
	void			UpdateTargetPositionForGetInPosition(CTaskVehicleMissionBase& rTask);
	void			UpdateVehicleMissionForGetInPosition();
	
private:

	float		m_fStraightLineDistance;
	fwFlags8	m_uRunningFlags;
	
private:

	static Tunables	sm_Tunables;

};

#endif
