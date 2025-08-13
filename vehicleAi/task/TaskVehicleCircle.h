#ifndef TASK_VEHICLE_CIRCLE_H
#define TASK_VEHICLE_CIRCLE_H

// Gta headers.
#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"
#include "Task\System\TaskTypes.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"

class CVehicle;
class CVehControls;
class CHeli;

//Rage headers.
#include "Vector/Vector3.h"

//
//
//
class CTaskVehicleCircle : public CTaskVehicleMissionBase
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Start = 0,
		State_Circle,
		State_CircleHeli,
		State_Stop,
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehicleCircle(const sVehicleMissionParams& params);
	~CTaskVehicleCircle();

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_CIRCLE; }
	aiTask*			Copy() const;

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_Circle;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleCircle>; }
	
	void		SetHelicopterSpecificParams(float fHeliRequestedOrientation, int iFlightHeight, int iMinHeightAboveTerrain, s32 iHeliFlags);

private:

	// FSM function implementations
	// State_Start
	FSM_Return		Circle_OnUpdate					(CVehicle* pVehicle);

	// State_Circle
	FSM_Return		Start_OnUpdate					(CVehicle* pVehicle);

	// State_CircleHeli
	void			CircleHeli_OnEnter				(CVehicle* pVehicle);
	FSM_Return		CircleHeli_OnUpdate				(CVehicle* pVehicle);

	//State_Stop
	void			Stop_OnEnter					(CVehicle* pVehicle);
	FSM_Return		Stop_OnUpdate					(CVehicle* pVehicle);

	//vehicle steering.
	void			Helicopter_SteerCirclingTarget	(CHeli *pHeli, const CEntity *pTarget);
	void			Boat_SteerCirclingTarget		(CVehicle *pVehicle, CVehControls* pVehControls, const CEntity *pTarget, float fCruiseSpeed);

	// Optional helicopter params
	float	m_fHeliRequestedOrientation;
	int		m_iFlightHeight; 
	int		m_iMinHeightAboveTerrain;
	s32		m_iHeliFlags;
};

#endif
