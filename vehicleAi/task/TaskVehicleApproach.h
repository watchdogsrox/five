#ifndef TASK_VEHICLE_APPROACH_H
#define TASK_VEHICLE_APPROACH_H

// Rage headers
#include "fwutil/Flags.h"
#include "fwvehicleai/pathfindtypes.h"
#include "system/bit.h"

// Game headers
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"
#include "VehicleAi/task/TaskVehicleMissionBase.h"

//=========================================================================
// CTaskVehicleApproach
//=========================================================================

class CTaskVehicleApproach : public CTaskVehicleMissionBase
{

public:

	enum State
	{
		State_Start = 0,
		State_ApproachTarget,
		State_ApproachClosestRoadNode,
		State_Wait,
		State_Finish,
	};

private:

	enum RunningFlags
	{
		RF_ClosestRoadNodeChanged	= BIT0,
	};

public:

	struct Tunables : public CTuning
	{
		Tunables();
		
		float m_MaxDistanceAroundClosestRoadNode;

		PAR_PARSABLE;
	};
	
public:

	CTaskVehicleApproach(const sVehicleMissionParams& rParams);
	virtual ~CTaskVehicleApproach();

public:

	bool GetTargetPosition(Vec3V_InOut vPosition) const;

public:

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

	// network:
	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleApproach>; }

private:

	virtual aiTask* Copy() const;
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_APPROACH; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	void		ApproachTarget_OnEnter();
	FSM_Return	ApproachTarget_OnUpdate();
	void		ApproachClosestRoadNode_OnEnter();
	FSM_Return	ApproachClosestRoadNode_OnUpdate();
	void		Wait_OnEnter();
	FSM_Return	Wait_OnUpdate();
	
private:

	bool FindClosestRoadNodeFromDispatch(CNodeAddress& rClosestRoadNode) const;
	bool FindClosestRoadNodeFromHelper(CNodeAddress& rClosestRoadNode);
	bool GeneratePositionNearClosestRoadNode(Vec3V_InOut vPosition) const;
	bool GenerateRandomPositionNearClosestRoadNode(Vec3V_InOut vPosition) const;
	void ProcessClosestRoadNode();
	void UpdateCruiseSpeed();

private:

	CFindNearestCarNodeHelper	m_FindNearestCarNodeHelper;
	CNodeAddress				m_ClosestRoadNode;
	fwFlags8					m_uRunningFlags;
	
private:

	static Tunables	sm_Tunables;

};

#endif
