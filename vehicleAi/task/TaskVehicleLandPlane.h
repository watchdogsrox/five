//
// TaskVehicleLandPlane
//

#ifndef TASK_VEHICLE_LAND_PLANE_H
#define TASK_VEHICLE_LAND_PLANE_H

#pragma once

#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"
#include "Task\System\TaskTypes.h"
#include "Task/System/Tuning.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"



//Rage headers.
#include "Vector/Vector3.h"


class CVehicle;
class CVehControls;
class CPlane;

//
//
//
class CTaskVehicleLandPlane : public CTaskVehicleMissionBase
{
public:

	struct Tunables : CTuning
	{
		Tunables();

		float m_SlowDownDistance;
		float m_TimeOnGroundToDrive;
		float m_HeightToStartLanding;
		float m_LandSlopeNoseUpMin;
		float m_LandSlopeNoseUpMax;

		PAR_PARSABLE;
	};


#define TASKVEHICLELANDPLANE_STATES	\
			TASKVEHICLELANDPLANE_STATE(kState_Init), \
			TASKVEHICLELANDPLANE_STATE(kState_CircleRunway), \
			TASKVEHICLELANDPLANE_STATE(kState_Descend), \
			TASKVEHICLELANDPLANE_STATE(kState_PerformLanding), \
			TASKVEHICLELANDPLANE_STATE(kState_TaxiToTargetLocation), \
			TASKVEHICLELANDPLANE_STATE(kState_Exit)
		
#define TASKVEHICLELANDPLANE_STATE(x) x		
	enum eStates
	{
		TASKVEHICLELANDPLANE_STATES
	};
#undef TASKVEHICLELANDPLANE_STATE

	// Constructor/destructor
	CTaskVehicleLandPlane(Vec3V_In vRunWayStart, Vec3V_In vRunWayEnd, bool in_bAllowRetry = true);
	CTaskVehicleLandPlane();
	~CTaskVehicleLandPlane();

	int GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_LAND_PLANE; }
	aiTask*	Copy() const {return rage_new CTaskVehicleLandPlane(m_vRunwayStart, m_vRunwayEnd, m_bAllowRetry);}

	FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	FSM_Return ProcessPreFSM();

	s32	GetDefaultStateAfterAbort()	const {return kState_Exit;}

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );

	virtual void Debug() const;

#endif //!__FINAL

	// network:
	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleLandPlane>; }

	void Serialise(CSyncDataBase& serialiser)
	{
		Vector3 runwayStart = VEC3V_TO_VECTOR3(m_vRunwayStart);
		Vector3 runwayEnd = VEC3V_TO_VECTOR3(m_vRunwayEnd);

		SERIALISE_POSITION(serialiser, runwayStart, "Runway start");
		SERIALISE_POSITION(serialiser, runwayEnd, "Runway end");
		SERIALISE_BOOL(serialiser, m_bAllowRetry, "Allow retry");

		m_vRunwayStart = VECTOR3_TO_VEC3V(runwayStart);
		m_vRunwayEnd = VECTOR3_TO_VEC3V(runwayEnd);
	}

private:

	FSM_Return Init_OnUpdate();

	FSM_Return CircleRunway_OnEnter();
	FSM_Return CircleRunway_OnUpdate();

	FSM_Return Descend_OnEnter();
	FSM_Return Descend_OnUpdate();

	FSM_Return PerformLanding_OnEnter();
	FSM_Return PerformLanding_OnUpdate();

	FSM_Return TaxiToTargetLocation_OnEnter();
	FSM_Return TaxiToTargetLocation_OnUpdate();

	FSM_Return Exit_OnUpdate();

	Vec3V m_vRunwayStart;
	Vec3V m_vRunwayEnd;


	Vec2V m_ApproachCircleCenter;
	ScalarV m_ApproachCircleRadius;
	float m_TimeOnTheGround;
	
	bool m_bClockwise;
	bool m_bAllowRetry;


	static Tunables sm_Tunables;
	
};

#endif // TASK_VEHICLE_LAND_PLANE_H
