//
// boat cruise
// April 17, 2013
//

#pragma once


#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"


class CTaskVehicleGoToBoat : public CTaskVehicleMissionBase
{
public:

	typedef fwFlags16 ConfigFlags;
	typedef fwFlags16 RunningFlags;

	enum eState
	{
		State_Init,
		State_GotoStraightLine,
		State_GotoNavmesh,
		State_ThreePointTurn,
		State_PauseForRoadRoute,
		State_Stop,

	} ;

	enum eConfigFlags
	{
		CF_StopAtEnd					= BIT0,
		CF_StopAtShore					= BIT1,
		CF_AvoidShore					= BIT2,
		CF_PreferForward				= BIT3,
		CF_NeverStop					= BIT4,
		CF_NeverNavMesh					= BIT5,
		CF_NeverRoute					= BIT6,
		CF_ForceBeached					= BIT7,
		CF_UseWanderRoute				= BIT8,
		CF_UseFleeRoute					= BIT9,
		CF_NeverPause					= BIT10,
		CF_NumFlags						= 11
	};

	enum eRunningFlags
	{
		RF_HasValidRoute					= BIT0,
		RF_HasUpdatedRouteThisFrame			= BIT1,		
		RF_HasUpdatedSegmentThisFrame		= BIT2,	
		RF_IsRunningRouteSearch				= BIT3,		
		RF_IsUnableToFindNavPath			= BIT4,
		RF_RouteIsPartial					= BIT5,
	};

	struct Tunables : CTuning
	{
		Tunables();

		float m_SlowdownDistance;
		float m_RouteArrivalDistance;
		float m_RouteLookAheadDistance;
		PAR_PARSABLE;
	};


	// Constructor/destructor
	CTaskVehicleGoToBoat(const sVehicleMissionParams& params, const ConfigFlags uConfigFlags = CF_StopAtEnd | CF_StopAtShore | CF_AvoidShore);
	CTaskVehicleGoToBoat(const CTaskVehicleGoToBoat& in_rhs);
	~CTaskVehicleGoToBoat();


	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_GOTO_BOAT; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Init; }
	virtual aiTask* Copy() const {return rage_new CTaskVehicleGoToBoat(*this);}
	virtual void SetTargetPosition(const Vector3 *pTargetPosition);

	FSM_Return ProcessPreFSM();
	FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	bool HasValidRoute() const { return m_uRunningFlags.IsFlagSet(RF_HasValidRoute); }
	bool IsNearEndOfRoute() const { return m_RouteFollowHelper.IsLastSegment(); }

	// network:
	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleGoToBoat>; }

	void Serialise(CSyncDataBase& serialiser)
	{
		static const unsigned SIZEOF_CONFIG_FLAGS = CF_NumFlags;

		CTaskVehicleMissionBase::Serialise(serialiser);

		u32 configFlags = (u32)m_uConfigFlags.GetAllFlags();

		SERIALISE_UNSIGNED(serialiser, configFlags, SIZEOF_CONFIG_FLAGS, "Config flags");

		m_uConfigFlags.SetAllFlags((u16)configFlags);
	}

protected:

	bool CanUseNavmeshToReachPoint(const Vector3& in_vStartCoords, const Vector3& in_vTargetCoords ) const;
	bool ShouldUseThreePointTurn(CVehicle& in_Vehicle, const Vector3& in_Target ) const;
	bool HasReachedDestination(CVehicle& in_Vehicle) const;

	float ComputeDesiredSpeed() const;
	void UpdateSearchRoute(CVehicle& in_Vehicle);
	void UpdateFollowRoute(CVehicle& in_Vehicle);

	FSM_Return Init_OnUpdate(CVehicle& in_Vehicle);
	
	void PauseForRoadRoute_OnEnter(CVehicle& in_Vehicle);
	FSM_Return PauseForRoadRoute_OnUpdate(CVehicle& in_Vehicle);

	void GotoStraightLine_OnEnter(CVehicle& in_Vehicle);
	FSM_Return GotoStraightLine_OnUpdate(CVehicle& in_Vehicle);

	void GotoNavmesh_OnEnter(CVehicle& in_Vehicle);
	FSM_Return GotoNavmesh_OnUpdate(CVehicle& in_Vehicle);
	void GotoNavmesh_OnExit(CVehicle& in_Vehicle);	

	void ThreePointTurn_OnEnter(CVehicle& in_Vehicle);
	FSM_Return ThreePointTurn_OnUpdate(CVehicle& in_Vehicle);
	void ThreePointTurn_OnExit(CVehicle& in_Vehicle);	
	
	void Stop_OnEnter();
	FSM_Return Stop_OnUpdate();

	CPathNodeRouteSearchHelper m_RouteSearchHelper;
	CBoatFollowRouteHelper m_RouteFollowHelper;

	float m_fTimeBlocked;
	float m_fTimeBlockThreePointTurn;
	float m_fTimeRanRouteSearch;
	bool	m_bWasBlockedForThreePointTurn;

	ConfigFlags m_uConfigFlags;
	RunningFlags m_uRunningFlags;

	static Tunables sm_Tunables;


public:

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char*  GetStaticStateName( s32 iState );
	void Debug() const;
#endif //!__FINAL


};
