#ifndef TASK_VEHICLE_THREE_POINT_TURN_H
#define TASK_VEHICLE_THREE_POINT_TURN_H

// Gta headers.
#include "Renderer/HierarchyIds.h"
#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"
#include "Task\System\TaskTypes.h"
#include "VehicleAi\racingline.h"
#include "VehicleAi\VehMission.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"
#include "VehicleAi\task\TaskVehicleGoToAutomobile.h"

class CVehicle;
class CVehControls;

//Rage headers.
#include "Vector/Vector3.h"

//
//
//
class CTaskVehicleThreePointTurn : public CTaskVehicleMissionBase
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_ThreePointTurn = 0,
		State_Exit
	} VehicleControlState;


	typedef enum
	{
		TURN_NONE,
		TURN_CLOCKWISE_GOING_FORWARD,
		TURN_CLOCKWISE_GOING_BACKWARD,
		TURN_COUNTERCLOCKWISE_GOING_FORWARD,
		TURN_COUNTERCLOCKWISE_GOING_BACKWARD,
		TURN_STRAIGHT_GOING_FORWARD,
		TURN_STRAIGHT_GOING_BACKWARD,
	} ThreePointTurnState;

	// Constructor/destructor
	CTaskVehicleThreePointTurn( const sVehicleMissionParams& params, const bool bStartedBecauseUTurn);
	~CTaskVehicleThreePointTurn();

	int					GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_THREE_POINT_TURN; }
	aiTask*				Copy() const; 

	// FSM implementations
	FSM_Return		ProcessPreFSM						();
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32				GetDefaultStateAfterAbort()	const {return State_ThreePointTurn;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

	static	bool	ShouldDoThreePointTurn(CVehicle* pVehicle, float* pDirToTargetOrientation, float* pVehDriveOrientation, bool b_StopAtLights, bool &isStuck, float &desiredAngleDiff, float &absDesiredAngleDiff, u32 &impatienceTimer, const bool bOnSmallRoad, const bool bHasUTurn );

	static	bool	RejectNoneCB(CPathNode * pPathNode, void * data);
	static	bool	IsPointOnRoad(const Vector3 &pos, const CVehicle* pVehicle, const bool bReturnTrueIfInJuntionAABB=false);
	float GetMaxThrottleFromTraction() const { return m_fMaxThrottleFromTraction; }

private:
	void				AdjustControls(const CVehicle* const pVeh, CVehControls* pVehControls, const float desiredSteerAngle, const float desiredSpeed);

	bool				ThreePointTurn(CVehicle* pVeh, float* pDirToTargetOrientation, float* pVehDriveOrientation, float *pDesiredSteerAngle, float *pDesiredSpeed, const bool bOnSmallRoad);

	void UpdateDesiredSpeedAndSteerAngle(const float fForwardMoveSpeed, float* pDesiredSpeed, float* pDesiredSteerAngle, const bool bIgnoreSpeed) const;


	// FSM function implementations
	// State_Start
	void				ThreePointTurn_OnEnter(CVehicle *pVehicle);
	FSM_Return			ThreePointTurn_OnUpdate(CVehicle* pVehicle);

	//this is meant to be the root pos, not steering pos. saves us having to worry about which bumper to use
	Vector3				m_vPositionWhenlastChangedDirection;

	float	m_fMaxThrottleFromTraction;
	ThreePointTurnState m_eThreePointTurnState;					//the current state the three point turn is in.
	float				m_fPreviousForwardSpeed;				// Used for hitting stuff detection
	int					m_relaxPavementCheckCount;				// to relax the "Don't go on pavements" check
	int					m_iChangeDirectionsCount;
	int					m_wasOnRoadIndex;						// Which one we're using
	u32					m_LastTime3PointTurnChangedDirection;
	u32					m_LastTimeRelaxedPavementCheckCount;
	bool				m_wasOnRoad[2];							// One for front, one for back
	u8					m_iBlind3PointTurnIterations;			// If we're doing a 3point turn because we're stuck this value will be non zero.
	bool				m_bStartedBecauseOfUTurn;

	static dev_float s_fMinFacingAngleRadians;
	static dev_float s_fMinFacingAngleRadiansSmallRoad;
};


#endif
